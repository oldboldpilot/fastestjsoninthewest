// Comprehensive correctness tests for fastjson_turbo.
// Run: ./test_turbo_correctness (exits 0 on pass, non-zero on first failure)

import fastjson_turbo;

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal test framework
// ---------------------------------------------------------------------------

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define CHECK(expr) do { \
    g_tests_run++; \
    if (!(expr)) { \
        g_tests_failed++; \
        std::fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    } else { \
        g_tests_passed++; \
    } \
} while(0)

#define CHECK_EQ(a, b) do { \
    g_tests_run++; \
    if (!((a) == (b))) { \
        g_tests_failed++; \
        std::fprintf(stderr, "FAIL  %s:%d  expected equal: %s == %s\n", __FILE__, __LINE__, #a, #b); \
    } else { \
        g_tests_passed++; \
    } \
} while(0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static fastjson::turbo::turbo_parser g_parser;

static fastjson::turbo::result<fastjson::turbo::turbo_document>
parse(const char* json) {
    return fastjson::turbo::parse(std::string_view{json, std::strlen(json)}, g_parser);
}

// ---------------------------------------------------------------------------
// 1. Primitive types
// ---------------------------------------------------------------------------

static void test_string() {
    auto doc = parse(R"("hello world")");
    CHECK(doc.has_value());
    auto val = doc->root();
    CHECK(val.is_string());
    CHECK(!val.is_number());
    CHECK(!val.is_object());
    CHECK(!val.is_array());
    CHECK_EQ(val.as_string(), std::string_view("hello world"));
    CHECK_EQ(val.as_std_string(), std::string("hello world"));
}

static void test_integer() {
    auto doc = parse("42");
    CHECK(doc.has_value());
    auto val = doc->root();
    CHECK(val.is_number());
    auto r = val.get_int64();
    CHECK(r.has_value());
    CHECK_EQ(*r, (int64_t)42);
}

static void test_negative_integer() {
    auto doc = parse("-7");
    CHECK(doc.has_value());
    auto r = doc->root().get_int64();
    CHECK(r.has_value());
    CHECK_EQ(*r, (int64_t)-7);
}

static void test_float() {
    auto doc = parse("3.14");
    CHECK(doc.has_value());
    auto r = doc->root().get_double();
    CHECK(r.has_value());
    double diff = *r - 3.14;
    CHECK(diff > -1e-9 && diff < 1e-9);
}

static void test_bool_true() {
    auto doc = parse("true");
    CHECK(doc.has_value());
    auto r = doc->root().get_bool();
    CHECK(r.has_value());
    CHECK(*r == true);
}

static void test_bool_false() {
    auto doc = parse("false");
    CHECK(doc.has_value());
    auto r = doc->root().get_bool();
    CHECK(r.has_value());
    CHECK(*r == false);
}

// ---------------------------------------------------------------------------
// 2. Simple flat object
// ---------------------------------------------------------------------------

static void test_flat_object() {
    auto doc = parse(R"({"name":"Alice","age":30,"active":true})");
    CHECK(doc.has_value());
    auto root = doc->root();
    CHECK(root.is_object());
    auto obj = root.get_object();
    CHECK(obj.has_value());

    auto name = obj->find_field("name");
    CHECK(name.has_value());
    CHECK(name->is_string());
    CHECK_EQ(name->as_string(), std::string_view("Alice"));

    auto age = obj->find_field("age");
    CHECK(age.has_value());
    CHECK(age->is_number());
    auto age_v = age->get_int64();
    CHECK(age_v.has_value());
    CHECK_EQ(*age_v, (int64_t)30);

    auto active = obj->find_field("active");
    CHECK(active.has_value());
    auto active_v = active->get_bool();
    CHECK(active_v.has_value());
    CHECK(*active_v == true);

    // Missing key
    auto missing = obj->find_field("missing_key");
    CHECK(!missing.has_value());
}

// ---------------------------------------------------------------------------
// 3. Flat array iteration
// ---------------------------------------------------------------------------

static void test_flat_array() {
    auto doc = parse(R"([1, 2, 3, 4, 5])");
    CHECK(doc.has_value());
    auto root = doc->root();
    CHECK(root.is_array());
    auto arr = root.get_array();
    CHECK(arr.has_value());

    std::vector<int64_t> got;
    for (auto elem : *arr) {
        auto v = elem.get_int64();
        CHECK(v.has_value());
        got.push_back(*v);
    }
    std::vector<int64_t> expected = {1, 2, 3, 4, 5};
    CHECK_EQ(got.size(), expected.size());
    for (size_t i = 0; i < std::min(got.size(), expected.size()); i++) {
        CHECK_EQ(got[i], expected[i]);
    }
}

static void test_string_array() {
    auto doc = parse(R"(["foo","bar","baz"])");
    CHECK(doc.has_value());
    auto arr = doc->root().get_array();
    CHECK(arr.has_value());

    std::vector<std::string> got;
    for (auto elem : *arr) {
        CHECK(elem.is_string());
        got.push_back(elem.as_std_string());
    }
    CHECK_EQ(got.size(), (size_t)3);
    if (got.size() == 3) {
        CHECK_EQ(got[0], std::string("foo"));
        CHECK_EQ(got[1], std::string("bar"));
        CHECK_EQ(got[2], std::string("baz"));
    }
}

// ---------------------------------------------------------------------------
// 4. Nested objects
// ---------------------------------------------------------------------------

static void test_nested_object() {
    auto doc = parse(R"({"user":{"name":"Bob","score":99}})");
    CHECK(doc.has_value());
    auto root_obj = doc->root().get_object();
    CHECK(root_obj.has_value());

    auto user = root_obj->find_field("user");
    CHECK(user.has_value());
    CHECK(user->is_object());

    auto inner = user->get_object();
    CHECK(inner.has_value());

    auto name = inner->find_field("name");
    CHECK(name.has_value());
    CHECK_EQ(name->as_string(), std::string_view("Bob"));

    auto score = inner->find_field("score");
    CHECK(score.has_value());
    auto sv = score->get_int64();
    CHECK(sv.has_value());
    CHECK_EQ(*sv, (int64_t)99);
}

// ---------------------------------------------------------------------------
// 5. Array of objects (the hot benchmark pattern)
// ---------------------------------------------------------------------------

static void test_array_of_objects() {
    auto doc = parse(R"([{"id":1,"val":"a"},{"id":2,"val":"b"},{"id":3,"val":"c"}])");
    CHECK(doc.has_value());
    auto arr = doc->root().get_array();
    CHECK(arr.has_value());

    int64_t expected_id = 1;
    const char* expected_vals[] = {"a", "b", "c"};
    int idx = 0;
    for (auto elem : *arr) {
        CHECK(elem.is_object());
        auto obj = elem.get_object();
        CHECK(obj.has_value());

        auto id = obj->find_field("id");
        CHECK(id.has_value());
        auto id_v = id->get_int64();
        CHECK(id_v.has_value());
        CHECK_EQ(*id_v, expected_id++);

        auto val = obj->find_field("val");
        CHECK(val.has_value());
        CHECK(val->is_string());
        if (idx < 3) CHECK_EQ(val->as_string(), std::string_view(expected_vals[idx]));
        idx++;
    }
    CHECK_EQ(idx, 3);
}

// ---------------------------------------------------------------------------
// 6. Deeply nested structure (tests O(1) skip_value with matching_)
// ---------------------------------------------------------------------------

static void test_deep_nesting() {
    auto doc = parse(R"({"a":{"b":{"c":{"d":{"e":42}}}}})");
    CHECK(doc.has_value());

    auto r0 = doc->root().get_object(); CHECK(r0.has_value());
    auto r1 = r0->find_field("a");     CHECK(r1.has_value());
    auto r2 = r1->get_object();        CHECK(r2.has_value());
    auto r3 = r2->find_field("b");     CHECK(r3.has_value());
    auto r4 = r3->get_object();        CHECK(r4.has_value());
    auto r5 = r4->find_field("c");     CHECK(r5.has_value());
    auto r6 = r5->get_object();        CHECK(r6.has_value());
    auto r7 = r6->find_field("d");     CHECK(r7.has_value());
    auto r8 = r7->get_object();        CHECK(r8.has_value());
    auto r9 = r8->find_field("e");     CHECK(r9.has_value());
    auto rv = r9->get_int64();         CHECK(rv.has_value());
    CHECK_EQ(*rv, (int64_t)42);
}

// ---------------------------------------------------------------------------
// 7. Key ordering: last key is found after skipping earlier values
// ---------------------------------------------------------------------------

static void test_skip_over_values() {
    // The key "z" comes after large nested values -- tests that skip_value
    // (using O(1) matching_ lookup) correctly jumps over them.
    auto doc = parse(R"({"nested":{"x":1,"y":2},"arr":[1,2,3,4,5],"z":"found"})");
    CHECK(doc.has_value());
    auto obj = doc->root().get_object();
    CHECK(obj.has_value());

    auto z = obj->find_field("z");
    CHECK(z.has_value());
    CHECK(z->is_string());
    CHECK_EQ(z->as_string(), std::string_view("found"));
}

// ---------------------------------------------------------------------------
// 8. Empty containers
// ---------------------------------------------------------------------------

static void test_empty_object() {
    auto doc = parse("{}");
    CHECK(doc.has_value());
    auto obj = doc->root().get_object();
    CHECK(obj.has_value());
    auto r = obj->find_field("anything");
    CHECK(!r.has_value());
}

static void test_empty_array() {
    auto doc = parse("[]");
    CHECK(doc.has_value());
    auto arr = doc->root().get_array();
    CHECK(arr.has_value());
    int count = 0;
    for ([[maybe_unused]] auto _ : *arr) count++;
    CHECK_EQ(count, 0);
}

// ---------------------------------------------------------------------------
// 9. Mixed-type array
// ---------------------------------------------------------------------------

static void test_mixed_array() {
    auto doc = parse(R"([1, "two", true, false, 3.14])");
    CHECK(doc.has_value());
    auto arr = doc->root().get_array();
    CHECK(arr.has_value());

    int count = 0;
    for ([[maybe_unused]] auto _ : *arr) count++;
    CHECK_EQ(count, 5);
}

// ---------------------------------------------------------------------------
// 10. as_string() zero-copy – view points into original buffer
// ---------------------------------------------------------------------------

static void test_as_string_zero_copy() {
    const char* json = R"({"key":"hello"})";
    auto doc = parse(json);
    CHECK(doc.has_value());
    auto obj = doc->root().get_object();
    CHECK(obj.has_value());
    auto key = obj->find_field("key");
    CHECK(key.has_value());
    auto sv = key->as_string();
    // The string_view should point inside the original JSON buffer
    CHECK(sv.data() >= json && sv.data() < json + std::strlen(json));
    CHECK_EQ(sv, std::string_view("hello"));
}

// ---------------------------------------------------------------------------
// 11. as_std_string() produces an owned copy
// ---------------------------------------------------------------------------

static void test_as_std_string_owned() {
    auto doc = parse(R"({"msg":"world"})");
    CHECK(doc.has_value());
    auto obj = doc->root().get_object();
    CHECK(obj.has_value());
    auto msg = obj->find_field("msg");
    CHECK(msg.has_value());
    std::string owned = msg->as_std_string();
    CHECK_EQ(owned, std::string("world"));
}

// ---------------------------------------------------------------------------
// Structural index sanity – verify no interior characters get indexed
// ---------------------------------------------------------------------------

static void test_structural_index_n10() {
    std::string json = "[";
    for (int i = 0; i < 10; i++) {
        if (i > 0) json += ",";
        json += R"({"id":)" + std::to_string(i) + R"(,"name":"User )" + std::to_string(i) + R"("})";
    }
    json += "]";

    auto doc = fastjson::turbo::parse(std::string_view{json}, g_parser);
    CHECK(doc.has_value());
    if (!doc.has_value()) return;

    size_t sc = doc->structurals_count();
    // Each element contributes exactly: [ { " " : val , " " : " " } ]
    // but we just assert none of the structural chars are plain letters/digits from inside strings
    const char* valid = R"({}[],:0123456789.eE+-"trufalsn)";  // structural + scalar starts
    bool all_valid = true;
    for (size_t i = 0; i < sc; i++) {
        char c = doc->get_structural_char(i);
        bool ok = false;
        for (const char* p = valid; *p; ++p) { if (c == *p) { ok = true; break; } }
        if (!ok) {
            std::fprintf(stderr, "[DIAG] unexpected structural char '%c' (0x%02x) at index %zu pos %u\n",
                (c >= 32 && c < 127) ? c : '?', (unsigned char)c, i, doc->get_structural(i));
            all_valid = false;
        }
    }
    CHECK(all_valid);
}



static void test_large_array_n(int N) {
    std::string json = "[";
    for (int i = 0; i < N; i++) {
        if (i > 0) json += ",";
        json += R"({"id":)" + std::to_string(i) + R"(,"name":"User )" + std::to_string(i) + R"("})";
    }
    json += "]";

    auto doc = fastjson::turbo::parse(std::string_view{json}, g_parser);
    CHECK(doc.has_value());
    if (!doc.has_value()) { std::fprintf(stderr, "  [N=%d] parse failed\n", N); return; }

    auto arr = doc->root().get_array();
    CHECK(arr.has_value());
    if (!arr.has_value()) return;

    int count = 0;
    bool all_ok = true;
    for (auto elem : *arr) {
        bool is_obj = elem.is_object();
        if (!is_obj) {
            std::fprintf(stderr, "  [N=%d count=%d] elem is not object\n", N, count);
            all_ok = false; break;
        }
        auto obj = elem.get_object();
        if (!obj.has_value()) {
            std::fprintf(stderr, "  [N=%d count=%d] get_object() failed\n", N, count);
            all_ok = false; break;
        }
        auto id = obj->find_field("id");
        if (!id.has_value()) {
            std::fprintf(stderr, "  [N=%d count=%d] find_field(id) failed\n", N, count);
            all_ok = false; break;
        }
        auto id_v = id->get_int64();
        if (!id_v.has_value()) {
            std::fprintf(stderr, "  [N=%d count=%d] get_int64() failed\n", N, count);
            all_ok = false; break;
        }
        if (*id_v != (int64_t)count) {
            std::fprintf(stderr, "  [N=%d count=%d] id=%lld != expected %d\n", N, count, (long long)*id_v, count);
            all_ok = false; break;
        }
        count++;
    }
    if (count != N || !all_ok) {
        std::fprintf(stderr, "  [N=%d] iterated %d/%d, all_ok=%d\n", N, count, N, (int)all_ok);
    }
    CHECK(all_ok);
    CHECK_EQ(count, N);
}

static void test_large_array() {
    for (int n : {2, 5, 10, 50, 100, 500, 999, 1000, 1001}) {
        test_large_array_n(n);
    }
}

// ---------------------------------------------------------------------------
// 13. Number edge cases
// ---------------------------------------------------------------------------

static void test_zero() {
    auto doc = parse("0");
    CHECK(doc.has_value());
    auto r = doc->root().get_int64();
    CHECK(r.has_value());
    CHECK_EQ(*r, (int64_t)0);
}

static void test_negative_float() {
    auto doc = parse("-2.5");
    CHECK(doc.has_value());
    auto r = doc->root().get_double();
    CHECK(r.has_value());
    double diff = *r - (-2.5);
    CHECK(diff > -1e-9 && diff < 1e-9);
}

// ---------------------------------------------------------------------------
// 14. Multiple keys – find_field returns the correct one
// ---------------------------------------------------------------------------

static void test_many_keys() {
    auto doc = parse(R"({"a":1,"b":2,"c":3,"d":4,"e":5,"f":6,"g":7,"h":8})");
    CHECK(doc.has_value());
    auto obj = doc->root().get_object();
    CHECK(obj.has_value());

    auto check = [&](const char* key, int64_t expected) {
        auto f = obj->find_field(key);
        CHECK(f.has_value());
        if (f.has_value()) {
            auto v = f->get_int64();
            CHECK(v.has_value());
            if (v.has_value()) CHECK_EQ(*v, expected);
        }
    };
    check("a", 1); check("b", 2); check("c", 3); check("d", 4);
    check("e", 5); check("f", 6); check("g", 7); check("h", 8);
}

// ---------------------------------------------------------------------------
// 15. Whitespace resilience
// ---------------------------------------------------------------------------

static void test_whitespace() {
    auto doc = parse("  {  \"k\"  :  \"v\"  }  ");
    CHECK(doc.has_value());
    auto obj = doc->root().get_object();
    CHECK(obj.has_value());
    auto k = obj->find_field("k");
    CHECK(k.has_value());
    if (k.has_value()) CHECK_EQ(k->as_string(), std::string_view("v"));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    test_string();
    test_integer();
    test_negative_integer();
    test_float();
    test_bool_true();
    test_bool_false();
    test_flat_object();
    test_flat_array();
    test_string_array();
    test_nested_object();
    test_array_of_objects();
    test_deep_nesting();
    test_skip_over_values();
    test_empty_object();
    test_empty_array();
    test_mixed_array();
    test_as_string_zero_copy();
    test_as_std_string_owned();
    test_large_array();
    test_zero();
    test_negative_float();
    test_many_keys();
    test_whitespace();
    test_structural_index_n10();

    std::printf("\n========================================\n");
    std::printf("Tests run:    %d\n", g_tests_run);
    std::printf("Tests passed: %d\n", g_tests_passed);
    std::printf("Tests failed: %d\n", g_tests_failed);
    std::printf("========================================\n");

    if (g_tests_failed > 0) {
        std::printf("RESULT: FAIL\n");
        return 1;
    }
    std::printf("RESULT: PASS\n");
    return 0;
}
