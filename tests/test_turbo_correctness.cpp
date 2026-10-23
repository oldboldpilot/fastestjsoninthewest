// Comprehensive correctness tests for fastjson_turbo.
// Run: ./test_turbo_correctness (exits 0 on pass, non-zero on first failure)

#include <sys/mman.h>   // guard-page over-read tests (section 24)
#include <unistd.h>

import std;
import fastjson_turbo;
import fastjson_multiregister_parser;

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
        std::println(std::cerr, "FAIL  {}:{}  {}", __FILE__, __LINE__, #expr); \
    } else { \
        g_tests_passed++; \
    } \
} while(0)

#define CHECK_EQ(a, b) do { \
    g_tests_run++; \
    if (!((a) == (b))) { \
        g_tests_failed++; \
        std::println(std::cerr, "FAIL  {}:{}  expected equal: {} == {}", __FILE__, __LINE__, #a, #b); \
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

static fastjson::turbo::result<fastjson::turbo::turbo_document>
iterate(const char* json) {
    return fastjson::turbo::iterate(std::string_view{json, std::strlen(json)}, g_parser);
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
    CHECK_EQ(*r, static_cast<std::int64_t>(42));
}

static void test_negative_integer() {
    auto doc = parse("-7");
    CHECK(doc.has_value());
    auto r = doc->root().get_int64();
    CHECK(r.has_value());
    CHECK_EQ(*r, static_cast<std::int64_t>(-7));
}

static void test_float() {
    auto doc = parse("3.14");
    CHECK(doc.has_value());
    auto r = doc->root().get_double();
    CHECK(r.has_value());
    CHECK(r.has_value() && *r == 3.14);  // exact: correctly rounded
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
    CHECK_EQ(*age_v, static_cast<std::int64_t>(30));

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

    std::vector<std::int64_t> got;
    for (auto elem : *arr) {
        auto v = elem.get_int64();
        CHECK(v.has_value());
        got.push_back(*v);
    }
    std::vector<std::int64_t> expected = {1, 2, 3, 4, 5};
    CHECK_EQ(got.size(), expected.size());
    for (std::size_t i = 0; i < std::min(got.size(), expected.size()); i++) {
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
    CHECK_EQ(got.size(), static_cast<std::size_t>(3));
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
    CHECK_EQ(*sv, static_cast<std::int64_t>(99));
}

// ---------------------------------------------------------------------------
// 5. Array of objects (the hot benchmark pattern)
// ---------------------------------------------------------------------------

static void test_array_of_objects() {
    auto doc = parse(R"([{"id":1,"val":"a"},{"id":2,"val":"b"},{"id":3,"val":"c"}])");
    CHECK(doc.has_value());
    auto arr = doc->root().get_array();
    CHECK(arr.has_value());

    std::int64_t expected_id = 1;
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
    CHECK_EQ(*rv, static_cast<std::int64_t>(42));
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

    std::size_t sc = doc->structurals_count();
    // Each element contributes exactly: [ { " " : val , " " : " " } ]
    // but we just assert none of the structural chars are plain letters/digits from inside strings
    const char* valid = R"({}[],:0123456789.eE+-"trufalsn)";  // structural + scalar starts
    bool all_valid = true;
    for (std::size_t i = 0; i < sc; i++) {
        char c = doc->get_structural_char(i);
        bool ok = false;
        for (const char* p = valid; *p; ++p) { if (c == *p) { ok = true; break; } }
        if (!ok) {
            std::println(std::cerr, "[DIAG] unexpected structural char '{}' ({:#04x}) at index {} pos {}",
                (c >= 32 && c < 127) ? c : '?', static_cast<unsigned char>(c), i, doc->get_structural(i));
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
    if (!doc.has_value()) { std::println(std::cerr, "  [N={}] parse failed", N); return; }

    auto arr = doc->root().get_array();
    CHECK(arr.has_value());
    if (!arr.has_value()) return;

    int count = 0;
    bool all_ok = true;
    for (auto elem : *arr) {
        bool is_obj = elem.is_object();
        if (!is_obj) {
            std::println(std::cerr, "  [N={} count={}] elem is not object", N, count);
            all_ok = false; break;
        }
        auto obj = elem.get_object();
        if (!obj.has_value()) {
            std::println(std::cerr, "  [N={} count={}] get_object() failed", N, count);
            all_ok = false; break;
        }
        auto id = obj->find_field("id");
        if (!id.has_value()) {
            std::println(std::cerr, "  [N={} count={}] find_field(id) failed", N, count);
            all_ok = false; break;
        }
        auto id_v = id->get_int64();
        if (!id_v.has_value()) {
            std::println(std::cerr, "  [N={} count={}] get_int64() failed", N, count);
            all_ok = false; break;
        }
        if (*id_v != static_cast<std::int64_t>(count)) {
            std::println(std::cerr, "  [N={} count={}] id={} != expected {}", N, count, *id_v, count);
            all_ok = false; break;
        }
        count++;
    }
    if (count != N || !all_ok) {
        std::println(std::cerr, "  [N={}] iterated {}/{}, all_ok={}", N, count, N, all_ok);
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
    CHECK_EQ(*r, static_cast<std::int64_t>(0));
}

static void test_negative_float() {
    auto doc = parse("-2.5");
    CHECK(doc.has_value());
    auto r = doc->root().get_double();
    CHECK(r.has_value());
    CHECK(r.has_value() && *r == -2.5);  // exact
}

// ---------------------------------------------------------------------------
// 14. Multiple keys – find_field returns the correct one
// ---------------------------------------------------------------------------

static void test_many_keys() {
    auto doc = parse(R"({"a":1,"b":2,"c":3,"d":4,"e":5,"f":6,"g":7,"h":8})");
    CHECK(doc.has_value());
    auto obj = doc->root().get_object();
    CHECK(obj.has_value());

    auto check = [&](const char* key, std::int64_t expected) {
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
// 16. OnDemand Streaming Iteration tests
// ---------------------------------------------------------------------------

static void test_iterate_cases() {
    // 1. Primitive
    {
        auto doc = iterate(R"("hello world")");
        CHECK(doc.has_value());
        auto val = doc->root();
        CHECK(val.is_string());
        CHECK_EQ(val.as_string(), std::string_view("hello world"));
    }
    // 2. Object with multiple fields
    {
        auto doc = iterate(R"({"symbol":"QQQ","price":530.12,"vol":45230100,"active":true})");
        CHECK(doc.has_value());
        auto obj = doc->root().get_object();
        CHECK(obj.has_value());
        auto sym = obj->find_field("symbol");
        CHECK(sym.has_value());
        CHECK_EQ(sym->as_string(), std::string_view("QQQ"));
        auto price = obj->find_field("price");
        CHECK(price.has_value());
        auto p_res = price->get_double();
        CHECK(p_res.has_value());
        if (p_res.has_value()) CHECK_EQ(*p_res, 530.12);
        auto vol = obj->find_field("vol");
        CHECK(vol.has_value());
        auto v_res = vol->get_int64();
        CHECK(v_res.has_value());
        if (v_res.has_value()) CHECK_EQ(*v_res, 45230100);
        auto active = obj->find_field("active");
        CHECK(active.has_value());
        auto a_res = active->get_bool();
        CHECK(a_res.has_value());
        if (a_res.has_value()) CHECK_EQ(*a_res, true);
    }
    // 3. Array of objects with lazy skipping
    {
        auto doc = iterate(R"({"records":[{"id":1,"close":100.5},{"id":2,"close":101.5}]})");
        CHECK(doc.has_value());
        auto obj = doc->root().get_object();
        CHECK(obj.has_value());
        auto recs = obj->find_field("records");
        CHECK(recs.has_value());
        auto arr = recs->get_array();
        CHECK(arr.has_value());
        double sum = 0.0;
        int count = 0;
        for (auto item : *arr) {
            auto r = item.get_object();
            CHECK(r.has_value());
            auto c = r->find_field("close");
            CHECK(c.has_value());
            sum += *c->get_double();
            count++;
        }
        CHECK_EQ(count, 2);
        CHECK(std::abs(sum - 202.0) < 0.001);
    }
}

static void test_iterate_large_record() {
    std::string json = R"({"header":{"format":"safetensors","num_tensors":128,"total_bytes":524288},"records":[{"id":0,"symbol":"SYM0","open":100.0,"high":101.0,"low":99.0,"close":100.5,"volume":1000000,"features":[0.0,0.1,0.2,0.3,0.4,0.5,0.6,0.7]}]})";
    auto doc = iterate(json.c_str());
    CHECK(doc.has_value());
    auto root = doc->root();
    CHECK(root.is_object());
    auto obj = root.get_object();
    CHECK(obj.has_value());
    auto arr = (*obj)["records"].get_array();
    CHECK(arr.has_value());
    int count = 0;
    for (auto item : *arr) {
        auto rec = item.get_object();
        CHECK(rec.has_value());
        auto close_val = (*rec)["close"].get_double();
        CHECK(close_val.has_value());
        if (close_val) {
            CHECK_EQ(*close_val, 100.5);
        }
        count++;
    }
    CHECK_EQ(count, 1);
}

// ---------------------------------------------------------------------------
// 17. NASA Spaceflight-Grade Boundary & Escape Regression Tests
//     (64B / 128B / 256B / 512B SIMD block boundaries + backslash chains)
// ---------------------------------------------------------------------------

static void test_simd_64b_boundary_quotes_and_escapes() {
    // Test strings and escaped quotes positioned across every byte offset 0..140
    // covering 64-byte and 128-byte SIMD register boundaries.
    for (std::size_t pad = 0; pad <= 140; ++pad) {
        std::string spaces(pad, ' ');
        // Case A: Escaped quote inside string straddling boundary, with structural chars inside string
        std::string json = spaces + R"({"key":"val\"ue{}:,[]end","num":12345,"ok":true})";

        auto doc_dom = fastjson::turbo::parse(json, g_parser);
        CHECK(doc_dom.has_value());
        if (doc_dom.has_value()) {
            auto obj = doc_dom->root().get_object();
            CHECK(obj.has_value());
            if (obj.has_value()) {
                auto k = obj->find_field("key");
                CHECK(k.has_value());
                if (k.has_value()) {
                    CHECK_EQ(k->as_string(), std::string_view(R"(val\"ue{}:,[]end)"));
                }
                auto n = obj->find_field("num");
                CHECK(n.has_value());
                if (n.has_value()) {
                    auto nv = n->get_int64();
                    CHECK(nv.has_value());
                    if (nv.has_value()) CHECK_EQ(*nv, static_cast<std::int64_t>(12345));
                }
                auto ok = obj->find_field("ok");
                CHECK(ok.has_value());
                if (ok.has_value()) {
                    auto okv = ok->get_bool();
                    CHECK(okv.has_value());
                    if (okv.has_value()) CHECK_EQ(*okv, true);
                }
            }
        }

        auto doc_iter = fastjson::turbo::iterate(json, g_parser);
        CHECK(doc_iter.has_value());
        if (doc_iter.has_value()) {
            auto obj = doc_iter->root().get_object();
            CHECK(obj.has_value());
            if (obj.has_value()) {
                auto k = obj->find_field("key");
                CHECK(k.has_value());
                if (k.has_value()) {
                    CHECK_EQ(k->as_string(), std::string_view(R"(val\"ue{}:,[]end)"));
                }
                auto n = obj->find_field("num");
                CHECK(n.has_value());
                if (n.has_value()) {
                    auto nv = n->get_int64();
                    CHECK(nv.has_value());
                    if (nv.has_value()) CHECK_EQ(*nv, static_cast<std::int64_t>(12345));
                }
                auto ok = obj->find_field("ok");
                CHECK(ok.has_value());
                if (ok.has_value()) {
                    auto okv = ok->get_bool();
                    CHECK(okv.has_value());
                    if (okv.has_value()) CHECK_EQ(*okv, true);
                }
            }
        }
    }

    // Case B: Chains of 1..16 consecutive backslashes before a quote across 64B boundaries
    for (int bs_count = 1; bs_count <= 16; ++bs_count) {
        for (std::size_t pad = 48; pad <= 72; ++pad) {
            std::string spaces(pad, ' ');
            std::string slashes(static_cast<std::size_t>(bs_count), '\\');
            std::string json;
            std::string expected_s;
            if (bs_count % 2 == 1) {
                // Odd number of backslashes: the quote after slashes is ESCAPED, so we add a closing quote
                expected_s = "abc" + slashes + "\"def";
                json = spaces + R"({"s":")" + expected_s + R"(","after":77})";
            } else {
                // Even number of backslashes: all backslashes are escaped, so the quote CLOSES the string
                expected_s = "abc" + slashes;
                json = spaces + R"({"s":")" + expected_s + R"(","after":77})";
            }

            auto doc_iter = fastjson::turbo::iterate(json, g_parser);
            CHECK(doc_iter.has_value());
            if (doc_iter.has_value()) {
                auto obj = doc_iter->root().get_object();
                CHECK(obj.has_value());
                if (obj.has_value()) {
                    auto s = obj->find_field("s");
                    CHECK(s.has_value());
                    if (s.has_value()) CHECK_EQ(s->as_string(), std::string_view(expected_s));
                    auto after = obj->find_field("after");
                    CHECK(after.has_value());
                    if (after.has_value()) {
                        auto av = after->get_int64();
                        CHECK(av.has_value());
                        if (av.has_value()) CHECK_EQ(*av, static_cast<std::int64_t>(77));
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 18. Key Prefix Collision, Empty String, and Nested Container Cursor Safety
// ---------------------------------------------------------------------------

static void test_key_prefix_and_nested_cursor_safety() {
    // 1. Key prefix collision safety: "id_long", "id_2", "", "id"
    const char* prefix_json = R"({"id_long":111,"id_2":222,"":333,"id":444})";
    for (int mode = 0; mode < 2; ++mode) {
        auto doc = (mode == 0) ? parse(prefix_json) : iterate(prefix_json);
        CHECK(doc.has_value());
        auto obj = doc->root().get_object();
        CHECK(obj.has_value());
        auto id_val = obj->find_field("id");
        CHECK(id_val.has_value());
        if (id_val.has_value()) {
            auto v = id_val->get_int64();
            CHECK(v.has_value());
            if (v.has_value()) CHECK_EQ(*v, static_cast<std::int64_t>(444));
        }
    }

    // 2. Empty strings as keys and values
    const char* empty_str_json = R"({"a":"","b":"nonempty","c":""})";
    for (int mode = 0; mode < 2; ++mode) {
        auto doc = (mode == 0) ? parse(empty_str_json) : iterate(empty_str_json);
        CHECK(doc.has_value());
        auto obj = doc->root().get_object();
        CHECK(obj.has_value());
        auto a = obj->find_field("a");
        CHECK(a.has_value());
        if (a.has_value()) CHECK_EQ(a->as_string(), std::string_view(""));
        auto b = obj->find_field("b");
        CHECK(b.has_value());
        if (b.has_value()) CHECK_EQ(b->as_string(), std::string_view("nonempty"));
        auto c = obj->find_field("c");
        CHECK(c.has_value());
        if (c.has_value()) CHECK_EQ(c->as_string(), std::string_view(""));
    }

    // 3. Accessing a nested container field inside an array of objects, then advancing
    //    both to a sibling field in the same object AND to the next element in the outer array.
    const char* nested_arr_json = R"({
        "records": [
            {"id": 1, "features": [1.0, 2.0, 3.0], "meta": {"k": "v1"}, "tail": 10},
            {"id": 2, "features": [4.0, 5.0, 6.0], "meta": {"k": "v2"}, "tail": 20},
            {"id": 3, "features": [7.0, 8.0, 9.0], "meta": {"k": "v3"}, "tail": 30}
        ]
    })";
    for (int mode = 0; mode < 2; ++mode) {
        auto doc = (mode == 0) ? parse(nested_arr_json) : iterate(nested_arr_json);
        CHECK(doc.has_value());
        auto root_obj = doc->root().get_object();
        CHECK(root_obj.has_value());
        auto arr = (*root_obj)["records"].get_array();
        CHECK(arr.has_value());

        int idx = 0;
        double feat_sum = 0.0;
        std::int64_t tail_sum = 0;
        for (auto elem : *arr) {
            auto rec = elem.get_object();
            CHECK(rec.has_value());
            // Access nested array "features" without exhausting it, then advance to next record
            auto feats = (*rec)["features"].get_array();
            CHECK(feats.has_value());
            if (feats.has_value()) {
                for (auto f : *feats) {
                    auto fv = f.get_double();
                    CHECK(fv.has_value());
                    if (fv.has_value()) feat_sum += *fv;
                }
            }
            auto tail = (*rec)["tail"].get_int64();
            CHECK(tail.has_value());
            if (tail.has_value()) tail_sum += *tail;
            idx++;
        }
        CHECK_EQ(idx, 3);
        CHECK(std::abs(feat_sum - 45.0) < 1e-9);
        CHECK_EQ(tail_sum, static_cast<std::int64_t>(60));
    }
}

// ---------------------------------------------------------------------------
// 19. Numeric Boundary & Error Handling Regression Tests
// ---------------------------------------------------------------------------

static void test_numeric_boundaries_and_errors() {
    // INT64_MAX and large negative integers
    {
        auto doc = iterate("9223372036854775807");
        CHECK(doc.has_value());
        auto v = doc->root().get_int64();
        CHECK(v.has_value());
        if (v.has_value()) CHECK_EQ(*v, std::numeric_limits<std::int64_t>::max());
    }
    {
        auto doc = iterate("-9223372036854775807");
        CHECK(doc.has_value());
        auto v = doc->root().get_int64();
        CHECK(v.has_value());
        if (v.has_value()) CHECK_EQ(*v, -std::numeric_limits<std::int64_t>::max());
    }
    // Scientific notation and high-precision floats
    {
        auto doc = iterate(R"([0.0, -0.0, 1e5, 1.5e-3, -2.5E+4, 3.141592653589793])");
        CHECK(doc.has_value());
        auto arr = doc->root().get_array();
        CHECK(arr.has_value());
        std::vector<double> vals;
        for (auto item : *arr) {
            auto d = item.get_double();
            CHECK(d.has_value());
            if (d.has_value()) vals.push_back(*d);
        }
        CHECK_EQ(vals.size(), static_cast<std::size_t>(6));
        if (vals.size() == 6) {
            // Exact (bit-level) equality: get_double is correctly rounded.
            CHECK_EQ(std::bit_cast<std::uint64_t>(vals[0]), std::bit_cast<std::uint64_t>(0.0));
            CHECK_EQ(std::bit_cast<std::uint64_t>(vals[1]), std::bit_cast<std::uint64_t>(-0.0));
            CHECK_EQ(vals[2], 1e5);
            CHECK_EQ(vals[3], 1.5e-3);
            CHECK_EQ(vals[4], -25000.0);
            CHECK_EQ(vals[5], 3.141592653589793);
        }
    }
    // Malformed scalar rejection
    {
        auto doc = iterate("123.");
        CHECK(doc.has_value());
        auto d = doc->root().get_double();
        CHECK(!d.has_value());
    }
    {
        auto doc = iterate("tru");
        CHECK(doc.has_value());
        auto b = doc->root().get_bool();
        CHECK(!b.has_value());
    }
}

// ---------------------------------------------------------------------------
// 19b. DOM parser (fastjson::mr::parse) 128-bit integer range edges
//      Regression: the old `result < prev` overflow test missed wraps, negative
//      magnitudes above |INT128_MIN| were negated into wrong positive values, and
//      the double fallback returned an uninitialized value when from_chars failed.
// ---------------------------------------------------------------------------

static bool rel_close(double got, double want) {
    return std::abs(got - want) <= std::abs(want) * 1e-12;
}

static void test_mr_integer_range_edges() {
    const __int128 i128_max = static_cast<__int128>((static_cast<unsigned __int128>(1) << 127) - 1);
    const __int128 i128_min = -i128_max - 1;
    const unsigned __int128 u128_max = ~static_cast<unsigned __int128>(0);

    {   // INT128_MAX stays a signed integer
        auto v = fastjson::mr::parse("170141183460469231731687303715884105727");
        CHECK(v.has_value());
        if (v.has_value()) {
            CHECK(v->is_int());
            auto i = v->as_int();
            CHECK(i.has_value() && *i == i128_max);
        }
    }
    {   // INT128_MIN is exact (not UB from negating INT128_MAX + 1)
        auto v = fastjson::mr::parse("-170141183460469231731687303715884105728");
        CHECK(v.has_value());
        if (v.has_value()) {
            CHECK(v->is_int());
            auto i = v->as_int();
            CHECK(i.has_value() && *i == i128_min);
        }
    }
    {   // One below INT128_MIN: must be a negative double, never a wrapped integer
        auto v = fastjson::mr::parse("-170141183460469231731687303715884105729");
        CHECK(v.has_value());
        if (v.has_value()) {
            CHECK(v->is_float());
            auto d = v->as_float();
            CHECK(d.has_value() && rel_close(*d, -1.7014118346046923e38));
        }
    }
    {   // Previously negated into a wrong positive integer
        auto v = fastjson::mr::parse("-200000000000000000000000000000000000000");
        CHECK(v.has_value());
        if (v.has_value()) {
            CHECK(v->is_float());
            auto d = v->as_float();
            CHECK(d.has_value() && *d < 0.0 && rel_close(*d, -2e38));
        }
    }
    {   // UINT128_MAX stays an unsigned integer
        auto v = fastjson::mr::parse("340282366920938463463374607431768211455");
        CHECK(v.has_value());
        if (v.has_value()) {
            CHECK(v->is_uint());
            auto u = v->as_uint();
            CHECK(u.has_value() && *u == u128_max);
        }
    }
    {   // UINT128_MAX + 1 overflows to double
        auto v = fastjson::mr::parse("340282366920938463463374607431768211456");
        CHECK(v.has_value());
        if (v.has_value()) {
            CHECK(v->is_float());
            auto d = v->as_float();
            CHECK(d.has_value() && rel_close(*d, 3.402823669209385e38));
        }
    }
    {   // Previously wrapped silently past the `result < prev` check
        auto v = fastjson::mr::parse("400000000000000000000000000000000000000");
        CHECK(v.has_value());
        if (v.has_value()) {
            CHECK(v->is_float());
            auto d = v->as_float();
            CHECK(d.has_value() && rel_close(*d, 4e38));
        }
    }
    {   // Overflowing value inside a container must not derail the parse
        auto v = fastjson::mr::parse("[400000000000000000000000000000000000000, 7]");
        CHECK(v.has_value());
        if (v.has_value()) {
            auto* arr = v->as_array();
            CHECK(arr != nullptr && arr->size() == 2);
            if (arr != nullptr && arr->size() == 2) {
                CHECK((*arr)[0].is_float());
                auto seven = (*arr)[1].as_int();
                CHECK(seven.has_value() && *seven == 7);
            }
        }
    }
    {   // Beyond double range: deterministic error, never an uninitialized value
        const std::string huge_pos = "1" + std::string(400, '0');
        const std::string huge_neg = "-" + huge_pos;
        CHECK(!fastjson::mr::parse(huge_pos).has_value());
        CHECK(!fastjson::mr::parse(huge_neg).has_value());
    }
    {   // Zero and negative zero remain integers
        auto z = fastjson::mr::parse("0");
        CHECK(z.has_value() && z->is_int() && z->as_int().value_or(1) == 0);
        auto nz = fastjson::mr::parse("-0");
        CHECK(nz.has_value() && nz->is_int() && nz->as_int().value_or(1) == 0);
    }
}

// ---------------------------------------------------------------------------
// 20. Lazy LINQ Transducer Pipeline Verification (Zero Intermediate Allocations)
// ---------------------------------------------------------------------------

static void test_lazy_linq_transducer_pipeline() {
    std::vector<int> data(10000);
    for (int i = 0; i < 10000; ++i) data[static_cast<std::size_t>(i)] = i + 1;

    // 1. Verify lazy composition does ZERO work before terminal operation
    int where_calls = 0;
    int select_calls = 0;
    fastjson::mr::query_result<int> q(&data);
    auto pipeline = q.where([&](int x) {
                         ++where_calls;
                         return x % 2 == 0;
                     })
                     .select([&](int x) {
                         ++select_calls;
                         return x * 10;
                     })
                     .take(3);

    CHECK(pipeline.is_lazy());
    CHECK_EQ(where_calls, 0);
    CHECK_EQ(select_calls, 0);

    // 2. Consuming take(3) evaluates ONLY the first 6 elements (2, 4, 6) and stops!
    auto top3 = pipeline.to_vector();
    CHECK_EQ(top3.size(), static_cast<std::size_t>(3));
    CHECK_EQ(top3[0], 20);
    CHECK_EQ(top3[1], 40);
    CHECK_EQ(top3[2], 60);
    CHECK_EQ(where_calls, 6);
    CHECK_EQ(select_calls, 3);

    // 3. Early termination on first(), any(), all(), find_index(), single()
    int pred_calls = 0;
    auto first_match = q.where([&](int x) {
                            ++pred_calls;
                            return x == 5;
                        }).first();
    CHECK(first_match.has_value());
    CHECK_EQ(*first_match, 5);
    CHECK_EQ(pred_calls, 5);

    int any_calls = 0;
    bool has_ten = q.any([&](int x) {
        ++any_calls;
        return x == 10;
    });
    CHECK(has_ten);
    CHECK_EQ(any_calls, 10);

    int all_calls = 0;
    bool all_lt_5 = q.all([&](int x) {
        ++all_calls;
        return x < 5;
    });
    CHECK(!all_lt_5);
    CHECK_EQ(all_calls, 5);

    // 4. Lazy flat_map + take early termination
    int outer_calls = 0;
    auto flattened = q.flat_map([&](int x) {
                          ++outer_calls;
                          return std::vector<int>{x, x * 100};
                      })
                      .take(5)
                      .to_vector();
    CHECK_EQ(flattened.size(), static_cast<std::size_t>(5));
    CHECK_EQ(outer_calls, 3); // 3 outer items produce 6 inner items; stops on 5th!
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 21. Number exactness (correct rounding) and strict number grammar.
//     Regression: get_double used `val * 10^-k` with a rounded reciprocal,
//     which mis-rounds ~30% of decimals by 1 ulp ("0.3" -> 0.30000000000000004).
//     Every double is now compared BIT-EXACT against glibc strtod (correctly
//     rounded), through all four access paths: root/in-array × iterate/parse.
// ---------------------------------------------------------------------------

static std::uint64_t dbits(double d) { return std::bit_cast<std::uint64_t>(d); }

// Expected result for `s` (strtod; overflow to inf => must be rejected).
struct expected_num { bool ok; double v; };
static expected_num expect_strtod(const std::string& s) {
    char* endp = nullptr;
    const double v = std::strtod(s.c_str(), &endp);
    if (std::isinf(v)) return {false, 0.0};
    return {true, v};
}

// Returns number of mismatching paths (0 == pass) for one numeric literal.
static int check_double_all_paths(const std::string& lit, const expected_num& want, bool verbose = true) {
    int bad = 0;
    auto judge = [&](const fastjson::turbo::result<double>& got, const char* path) {
        const bool ok = want.ok ? (got.has_value() && dbits(*got) == dbits(want.v))
                                : (!got.has_value() &&
                                   got.error().code == fastjson::turbo::error_code::number_out_of_range);
        if (!ok) {
            ++bad;
            if (verbose)
                std::println(std::cerr, "  number mismatch [{}] '{}': got {} want {}", path, lit,
                             got.has_value() ? std::format("{:.17g}", *got) : std::string("error"),
                             want.ok ? std::format("{:.17g}", want.v) : std::string("out_of_range"));
        }
    };
    {
        fastjson::turbo::turbo_parser p;
        auto d = fastjson::turbo::iterate(lit, p);
        if (d) judge(d->root().get_double(), "iterate/root"); else ++bad;
    }
    {
        const std::string arr = "[" + lit + "]";
        fastjson::turbo::turbo_parser p;
        auto d = fastjson::turbo::iterate(arr, p);
        if (d) { auto a = d->root().get_array(); if (a) judge((*a->begin()).get_double(), "iterate/array"); else ++bad; }
        else ++bad;
    }
    {
        fastjson::turbo::turbo_parser p;
        auto d = fastjson::turbo::parse(lit, p);
        if (d) judge(d->root().get_double(), "parse/root"); else ++bad;
    }
    {
        const std::string arr = "[" + lit + " ]";
        fastjson::turbo::turbo_parser p;
        auto d = fastjson::turbo::parse(arr, p);
        if (d) { auto a = d->root().get_array(); if (a) judge((*a->begin()).get_double(), "parse/array"); else ++bad; }
        else ++bad;
    }
    {
        // Tape DOM: numbers are converted during parse; out-of-range fails the parse.
        fastjson::turbo::dom_parser p;
        for (const std::string& doc : {lit, "{\"k\":" + lit + "}"}) {
            auto d = p.parse(doc);
            fastjson::turbo::result<double> got = d ? (doc[0] == '{' ? (*d)["k"]->get_double() : d->get_double())
                                                    : fastjson::turbo::result<double>(std::unexpected(d.error()));
            judge(got, doc[0] == '{' ? "dom/object" : "dom/root");
        }
    }
    {
        // Padded context: >= 24 bytes follow the literal, so the loop-free
        // scan_number_fast path (needs p + 24 <= end) is exercised on every
        // front end, not only the bounded end-of-input scanner.
        const std::string pad = "[" + lit + ",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]";
        fastjson::turbo::turbo_parser p;
        if (auto d = fastjson::turbo::iterate(pad, p)) {
            if (auto a = d->root().get_array()) judge((*a->begin()).get_double(), "iterate/padded"); else ++bad;
        } else ++bad;
        if (auto d = fastjson::turbo::parse(pad, p)) {
            if (auto a = d->root().get_array()) judge((*a->begin()).get_double(), "parse/padded"); else ++bad;
        } else ++bad;
        fastjson::turbo::dom_parser dp;
        auto d = dp.parse(pad);
        fastjson::turbo::result<double> got = d ? d->at(0)->get_double()
                                                : fastjson::turbo::result<double>(std::unexpected(d.error()));
        judge(got, "dom/padded");
    }
    return bad;
}

static void test_double_exact_known_cases() {
    const char* cases[] = {
        "0.3", "0.7", "0.1", "0.2", "1.1", "2.675", "123.456", "530.12", "100.000000",
        "0.30000000000000004", "3.141592653589793", "45230100", "-2.5E+4", "1.5e-3",
        "9007199254740992", "9007199254740993", "9007199254740995", "18446744073709551615",
        "1e22", "1e23", "8.533e+68", "4.1006e-184", "9.998e+307", "9.9538452227e-280",
        "6.47660115e-260", "7.4e+47", "5.92e+48", "7.35e+66", "8.32116e+55",
        "2.2250738585072011e-308", "2.2250738585072012e-308", "2.2250738585072014e-308",
        "4.9406564584124654e-324", "5e-324", "2.4703282292062328e-324", "2.4703282292062327e-324",
        "1.7976931348623157e308", "1.7976931348623158e308", "-1.7976931348623157e308",
        "1.00000000000000011102230246251565404236316680908203125",
        "1.00000000000000011102230246251565404236316680908203126",
        "7.2057594037927933e16", "3.14159265358979323846264338327950288419716939937510",
        "123456789012345678901234567890e-10", "0.000000000000000000000000000000000000000000001e45",
        "0.0000000000000000000000000000001234", "-0", "-0.0", "0e0", "0E-0", "-0e+10", "0.0e-999",
        "1e-400", "-1e-400", "1e-99999999999", "123e-330",
        // scan_number_fast boundaries (1..8 digit runs; 9+ falls back):
        "1", "12345678", "-12345678", "99999999", "123456789", "-123456789",
        "1.12345678", "12345678.12345678", "-99999999.99999999", "1.123456789",
        "12345678.123456789", "0.00000000", "0.00000001", "-0.00000001", "10000000.5",
        // Out of range (must be rejected with number_out_of_range):
        "1e400", "-1e400", "1.7976931348623159e308", "1e99999999999", "123e310",
    };
    int bad = 0;
    for (const char* c : cases) bad += check_double_all_paths(c, expect_strtod(c));
    CHECK_EQ(bad, 0);

    // >19-digit slow path, both directions of from_chars ERANGE.
    const std::string big = "1" + std::string(400, '0');                   // 1e400 -> overflow
    const std::string tiny = "0." + std::string(400, '0') + "1";           // 1e-401 -> +0
    const std::string ntiny = "-0." + std::string(400, '0') + "123";       // -> -0
    const std::string long_exact = "0." + std::string(30, '0') + "12345678901234567890123"; // 23 sig digits
    int bad2 = 0;
    for (const std::string& s : {big, tiny, ntiny, long_exact})
        bad2 += check_double_all_paths(s, expect_strtod(s));
    CHECK_EQ(bad2, 0);
}

static void test_double_exact_randomized() {
    std::mt19937_64 rng(0xFA57'5EED'2026ULL);
    // (a) random decimal literals: sign, int part, fraction, exponent.
    int bad_dec = 0, n_dec = 0;
    std::string big_array = "[";
    std::vector<expected_num> big_expect;
    for (int t = 0; t < 60000; ++t) {
        std::string s;
        if (rng() & 1) s += '-';
        if (rng() % 10 < 3) s += '0';
        else {
            s += static_cast<char>('1' + rng() % 9);
            const int k = static_cast<int>(rng() % 21);
            for (int j = 0; j < k; ++j) s += static_cast<char>('0' + rng() % 10);
        }
        if (rng() % 10 < 6) {
            s += '.';
            const int k = 1 + static_cast<int>(rng() % 25);
            for (int j = 0; j < k; ++j) s += static_cast<char>('0' + rng() % 10);
        }
        if (rng() & 1) {
            s += (rng() & 1) ? 'e' : 'E';
            const auto r = rng() % 3;
            if (r == 1) s += '-'; else if (r == 2) s += '+';
            s += std::to_string(rng() % 350);
        }
        const expected_num want = expect_strtod(s);
        if (t < 4000) bad_dec += check_double_all_paths(s, want, bad_dec < 10);
        if (want.ok) {                    // in-range values also go into one big array
            if (big_expect.size()) big_array += ',';
            big_array += s;
            big_expect.push_back(want);
        }
        ++n_dec;
    }
    big_array += ']';
    {
        fastjson::turbo::turbo_parser p;
        auto d = fastjson::turbo::iterate(big_array, p);
        CHECK(d.has_value());
        if (d) {
            auto a = d->root().get_array();
            CHECK(a.has_value());
            std::size_t i = 0;
            if (a) for (auto v : *a) {
                auto g = v.get_double();
                if (i >= big_expect.size() || !g || dbits(*g) != dbits(big_expect[i].v)) {
                    if (bad_dec < 10) std::println(std::cerr, "  big-array mismatch at {}", i);
                    ++bad_dec;
                }
                ++i;
            }
            CHECK_EQ(i, big_expect.size());
        }
    }
    {
        fastjson::turbo::dom_parser p;
        auto d = p.parse(big_array);
        CHECK(d.has_value());
        if (d) {
            std::size_t i = 0;
            for (auto v : *d->get_array()) {
                auto g = v.get_double();
                if (i >= big_expect.size() || !g || dbits(*g) != dbits(big_expect[i].v)) {
                    if (bad_dec < 10) std::println(std::cerr, "  dom big-array mismatch at {}", i);
                    ++bad_dec;
                }
                ++i;
            }
            CHECK_EQ(i, big_expect.size());
        }
    }
    CHECK_EQ(bad_dec, 0);

    // (b) random finite doubles: shortest round-trip form and 17-digit form.
    int bad_rt = 0;
    for (int t = 0; t < 60000; ++t) {
        double d;
        do { d = std::bit_cast<double>(rng()); } while (!std::isfinite(d));
        char buf[64];
        auto r1 = std::to_chars(buf, buf + sizeof buf, d);
        const std::string shortest(buf, r1.ptr);
        auto r2 = std::to_chars(buf, buf + sizeof buf, d, std::chars_format::scientific, 16);
        const std::string sci17(buf, r2.ptr);
        const expected_num w1{true, d};                    // shortest must round-trip exactly
        const expected_num w2 = expect_strtod(sci17);
        if (t < 3000) {
            bad_rt += check_double_all_paths(shortest, w1, bad_rt < 10);
            bad_rt += check_double_all_paths(sci17, w2, bad_rt < 10);
        } else {
            // Fast single-path check for the bulk.
            for (const auto& [lit, w] : {std::pair{shortest, w1}, std::pair{sci17, w2}}) {
                const std::string arr = "[" + lit + "]";
                fastjson::turbo::turbo_parser p;
                auto doc = fastjson::turbo::iterate(arr, p);
                auto g = doc ? (*doc->root().get_array()->begin()).get_double()
                             : fastjson::turbo::result<double>(std::unexpected(fastjson::turbo::parse_error{}));
                if (!g || dbits(*g) != dbits(w.v)) {
                    if (bad_rt < 10) std::println(std::cerr, "  round-trip mismatch '{}'", lit);
                    ++bad_rt;
                }
            }
        }
    }
    CHECK_EQ(bad_rt, 0);
    std::println("  number exactness: {} random decimals, 120000 random-double literals checked bit-exact", n_dec);
}

static void test_number_grammar_and_int64_strictness() {
    using fastjson::turbo::error_code;
    // Invalid JSON numbers: get_double must reject every one (no partial parse).
    const char* bad_nums[] = {
        "01", "-01", "00", "1.", "1.e5", ".5", "-.5", "1e", "1e+", "1E-", "-", "--1", "+1",
        "1.2.3", "1e5e5", "0x10", "1-", "1+", "12a", "1_000", "-0x1", "1ee5", "0.e1",
        // fast-path-sized shapes (<= 8-digit runs) with a bad tail / leading zero:
        "012345678", "-01234567", "00000000", "12345678a", "1234567.", "1234567.a",
        "12345678.1234567x", "-", "-a", "1.12345678.1", "12345678-", "1.5-", "0.00000000x",
    };
    for (const char* s : bad_nums) {
        for (const std::string& lit : {std::string(s), "[" + std::string(s) + "]",
                                       "[" + std::string(s) + ",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]"}) {
            fastjson::turbo::turbo_parser p;
            auto d = fastjson::turbo::iterate(lit, p);
            CHECK(d.has_value());
            if (!d) continue;
            auto v = (lit[0] == '[') ? (*d->root().get_array()->begin()) : d->root();
            auto g = v.get_double();
            if (g.has_value()) std::println(std::cerr, "  accepted invalid number '{}'", lit);
            CHECK(!g.has_value());
            CHECK(!v.get_int64().has_value());
        }
    }
    // Non-numbers are type errors.
    for (const char* s : {"\"1\"", "true", "null", "Infinity", "NaN"}) {
        fastjson::turbo::turbo_parser p;
        auto d = fastjson::turbo::iterate(std::string_view(s), p);
        CHECK(d.has_value());
        if (d) CHECK(!d->root().get_double().has_value());
    }
    // get_int64: exact range, rejects non-integers instead of truncating.
    struct icase { const char* lit; bool ok; std::int64_t v; error_code err; };
    const icase ic[] = {
        {"0", true, 0, {}}, {"-0", true, 0, {}}, {"42", true, 42, {}},
        {"9223372036854775807", true, std::numeric_limits<std::int64_t>::max(), {}},
        {"-9223372036854775808", true, std::numeric_limits<std::int64_t>::min(), {}},
        {"9223372036854775808", false, 0, error_code::number_out_of_range},
        {"-9223372036854775809", false, 0, error_code::number_out_of_range},
        {"18446744073709551615", false, 0, error_code::number_out_of_range},
        {"123456789012345678901", false, 0, error_code::number_out_of_range},
        {"1.5", false, 0, error_code::type_error},     // was silently truncated to 1
        {"1e2", false, 0, error_code::type_error},
        {"-1.0", false, 0, error_code::type_error},
    };
    for (const auto& c : ic) {
        for (const std::string& lit : {std::string(c.lit), "[" + std::string(c.lit) + "]"}) {
            fastjson::turbo::turbo_parser p;
            auto d = fastjson::turbo::iterate(lit, p);
            CHECK(d.has_value());
            if (!d) continue;
            auto v = (lit[0] == '[') ? (*d->root().get_array()->begin()) : d->root();
            auto g = v.get_int64();
            if (c.ok) { CHECK(g.has_value()); if (g) CHECK_EQ(*g, c.v); }
            else      { CHECK(!g.has_value()); if (!g) CHECK(g.error().code == c.err); }
        }
    }
    // Booleans: exact literal + terminator.
    for (const char* s : {"truex", "tru", "falsey", "fals", "True", "t"}) {
        fastjson::turbo::turbo_parser p;
        auto d = fastjson::turbo::iterate(std::string_view(s), p);
        CHECK(d.has_value());
        if (d) CHECK(!d->root().get_bool().has_value());
    }
    {
        fastjson::turbo::turbo_parser p;
        auto d = fastjson::turbo::iterate(std::string_view("[true ,false]"), p);
        CHECK(d.has_value());
        if (d) {
            auto a = d->root().get_array();
            CHECK(a.has_value());
            std::vector<bool> got;
            if (a) for (auto v : *a) { auto b = v.get_bool(); CHECK(b.has_value()); if (b) got.push_back(*b); }
            CHECK(got == std::vector<bool>({true, false}));
        }
    }
}

// ---------------------------------------------------------------------------
// 22. Stage-1 validation: UTF-8 (differential vs scalar reference), raw
//     control characters in strings, unterminated strings, empty input, and
//     bracket structure for the tape parse(). Every rejection is checked for
//     the exact error code (deterministic errors).
// ---------------------------------------------------------------------------

namespace ft = fastjson::turbo;

static bool utf8_simd(const std::string& s) { return ft::detail::validate_utf8(s.data(), s.size()); }
static bool utf8_ref(const std::string& s)  { return ft::detail::validate_utf8_scalar(s.data(), s.size()); }

static void test_utf8_validator_differential() {
    long long mism = 0, checked = 0;
    auto cmp = [&](const std::string& s) {
        ++checked;
        if (utf8_simd(s) != utf8_ref(s)) {
            if (mism < 5) {
                std::string hex;
                for (unsigned char ch : s) if (ch >= 0x80) hex += std::format("{:02X} ", ch);
                std::println(std::cerr, "  utf8 mismatch len={} simd={} ref={} bytes: {}", s.size(), utf8_simd(s), utf8_ref(s), hex);
            }
            ++mism;
        }
    };
    // Exhaustive 1- and 2-byte sequences, and all 3-byte sequences with a
    // non-ASCII lead, at offsets that straddle the 16-byte lane and 32-byte
    // vector boundaries.
    for (int off : {0, 15, 30, 31}) {
        std::string buf(64, 'a');
        for (int b0 = 0; b0 < 256; ++b0) {
            buf.assign(64, 'a'); buf[off] = static_cast<char>(b0); cmp(buf);
            for (int b1 = 0; b1 < 256; ++b1) {
                buf.assign(64, 'a'); buf[off] = static_cast<char>(b0); buf[off + 1] = static_cast<char>(b1); cmp(buf);
            }
        }
    }
    for (int off : {13, 30}) {
        std::string buf(64, 'a');
        for (int b0 = 0x80; b0 < 256; ++b0)
            for (int b1 = 0; b1 < 256; ++b1)
                for (int b2 = 0; b2 < 256; ++b2) {
                    buf[off] = static_cast<char>(b0); buf[off + 1] = static_cast<char>(b1); buf[off + 2] = static_cast<char>(b2);
                    cmp(buf);
                }
    }
    // 4-byte leads F0..FF × all second bytes × boundary third/fourth bytes.
    const int edge[] = {0x00, 0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC2, 0xE0, 0xF0, 0xFF};
    for (int off : {12, 29, 31}) {
        for (int b0 = 0xF0; b0 < 256; ++b0)
            for (int b1 = 0; b1 < 256; ++b1)
                for (int b2 : edge) for (int b3 : edge) {
                    std::string buf(64, 'a');
                    buf[off] = static_cast<char>(b0); buf[off + 1] = static_cast<char>(b1);
                    buf[off + 2] = static_cast<char>(b2); buf[off + 3] = static_cast<char>(b3);
                    cmp(buf);
                }
    }
    // Truncated sequences at the very end of input (no padding after).
    for (const char* t : {"\xC3", "\xE2\x82", "\xF0\x9F\x98", "\xED\xA0", "\xF4\x8F\xBF"}) {
        for (int pre : {0, 1, 31, 32, 63, 64}) cmp(std::string(pre, 'x') + t);
    }
    // Random fuzz: mostly-valid UTF-8 with random corruption, random lengths.
    std::mt19937_64 rng(0x0'7F8'BEEFULL);
    for (int it = 0; it < 200000; ++it) {
        std::string s;
        const int len = 1 + static_cast<int>(rng() % 200);
        while (static_cast<int>(s.size()) < len) {
            const auto k = rng() % 10;
            std::uint32_t cp;
            if (k < 4) cp = static_cast<std::uint32_t>(rng() % 0x80);
            else if (k < 6) cp = 0x80 + static_cast<std::uint32_t>(rng() % 0x780);
            else if (k < 8) cp = 0x800 + static_cast<std::uint32_t>(rng() % 0xF800);
            else cp = 0x10000 + static_cast<std::uint32_t>(rng() % 0x100000);
            if (cp < 0x80) s += static_cast<char>(cp);
            else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
            else if (cp < 0x10000) { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
            else { s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
        }
        if (rng() % 3 == 0) s[rng() % s.size()] = static_cast<char>(rng());
        if (rng() % 7 == 0) s.resize(rng() % s.size() + 1);
        cmp(s);
    }
    CHECK_EQ(mism, 0LL);
    std::println("  utf8 differential: {} buffers, SIMD == scalar reference", checked);
}

// Runs `json` through iterate(), parse() and the DOM; all three must fail
// with `want` (or all succeed when want == success).
static void expect_all_paths(std::string_view json, ft::error_code want, int line) {
    ft::turbo_parser tp1, tp2;
    ft::dom_parser dp;
    auto a = ft::iterate(json, tp1);
    auto b = ft::parse(json, tp2);
    auto c = dp.parse(json);
    auto code = [](const auto& r) { return r.has_value() ? ft::error_code::success : r.error().code; };
    const bool ok = code(a) == want && code(b) == want && code(c) == want;
    g_tests_run++;
    if (ok) { g_tests_passed++; return; }
    g_tests_failed++;
    std::println(std::cerr, "FAIL  line {}: expected code {} got iterate={} parse={} dom={} for {:?}", line,
                 static_cast<int>(want), static_cast<int>(code(a)), static_cast<int>(code(b)),
                 static_cast<int>(code(c)), std::string(json.substr(0, 80)));
}
#define EXPECT_ALL(json, code) expect_all_paths((json), (code), __LINE__)

static void test_stage1_validation_errors() {
    using ec = ft::error_code;
    // UTF-8: valid multibyte content is accepted everywhere.
    EXPECT_ALL("[\"caf\xC3\xA9\", \"\xE2\x82\xAC\", \"\xF0\x9D\x84\x9E\", \"\xEF\xBF\xBF\"]", ec::success);
    // Invalid UTF-8 inside strings, at many offsets (crossing 32/64B blocks).
    const char* bad_utf8[] = {"\xC0\xAF", "\xC1\xBF", "\xE0\x80\xAF", "\xE0\x9F\xBF", "\xED\xA0\x80", "\xED\xBF\xBF",
                              "\xF0\x80\x80\xAF", "\xF0\x8F\xBF\xBF", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80",
                              "\x80", "\xBF", "\xFE", "\xFF", "\xC3", "\xE2\x82", "\xF0\x9F\x98", "\xC3\x28"};
    for (const char* bseq : bad_utf8) {
        for (int off : {0, 1, 26, 27, 28, 29, 30, 31, 32, 58, 59, 60, 61, 62, 63, 64, 125}) {
            std::string j = "{\"k\":\"" + std::string(static_cast<std::size_t>(off), 'a') + bseq + "\"}";
            EXPECT_ALL(j, ec::utf8_error);
        }
    }
    // Truncated sequence as the very last bytes of the document.
    EXPECT_ALL(std::string("\"ab\xE2\x82"), ec::utf8_error);
    // Raw control characters inside strings (0x00-0x1F) are rejected; 0x7F is legal.
    for (int ch = 0; ch < 0x20; ++ch) {
        for (int off : {0, 30, 62, 63}) {
            std::string j = "[\"" + std::string(static_cast<std::size_t>(off), 'x') + static_cast<char>(ch) + "y\"]";
            EXPECT_ALL(j, ec::unescaped_control);
        }
    }
    EXPECT_ALL("[\"a\x7F" "b\"]", ec::success);
    EXPECT_ALL("[\t1,\n2,\r3 ]", ec::success);                       // whitespace controls outside strings
    // Unterminated strings.
    EXPECT_ALL("\"abc", ec::unclosed_string);
    EXPECT_ALL("{\"a\":\"b}", ec::unclosed_string);
    EXPECT_ALL("[\"abc\\\"]", ec::unclosed_string);                  // escaped final quote
    EXPECT_ALL(std::string(100, ' ') + "[\"" + std::string(200, 'z'), ec::unclosed_string);
    // Empty documents.
    EXPECT_ALL("", ec::empty_document);
    EXPECT_ALL("  \n\t\r ", ec::empty_document);

    // parse() (structural tape): bracket balance / pairing / depth.
    auto tape_code = [](std::string_view j) {
        ft::turbo_parser p;
        auto r = ft::parse(j, p);
        return r.has_value() ? ec::success : r.error().code;
    };
    for (const char* s : {"[1,2", "[1,2}", "{\"a\":1]", "]", "}", "[]]", "[[]", "{]", "[}", "[[[]]", "{\"a\":[1}"})
        CHECK(tape_code(s) == ec::invalid_json);
    CHECK(tape_code("[[[1]],{\"a\":{\"b\":[]}}]") == ec::success);
    {
        std::string ok4096 = std::string(4096, '[') + std::string(4096, ']');
        std::string deep = std::string(5000, '[') + std::string(5000, ']');
        CHECK(tape_code(ok4096) == ec::success);
        CHECK(tape_code(deep) == ec::depth_exceeded);               // was a stack-buffer overflow
        // Legacy single-pass builder: no overflow, correct partners at depth > 4096.
        std::vector<ft::tape_entry> tape(deep.size() + 64);
        const std::size_t cnt = ft::detail::build_structural_index(deep, tape.data());
        CHECK_EQ(cnt, deep.size());
        CHECK_EQ(tape[0].partner, static_cast<std::uint32_t>(deep.size() - 1));
        CHECK_EQ(tape[4095].partner, static_cast<std::uint32_t>(deep.size() - 1 - 4095));
    }
}

// ---------------------------------------------------------------------------
// 23. Tape DOM (dom_parser): API, zero-copy / inline strings, escapes,
//     128-bit integers, grammar (accept + reject corpus), depth limits,
//     randomized round-trip against a reference model, mutation robustness.
// ---------------------------------------------------------------------------

static void test_dom_basic_api() {
    ft::dom_parser p;
    const std::string j = R"({"id":7,"name":"widget-long-name","ok":true,"no":false,"nil":null,
        "pi":3.14159,"neg":-12,"big":18446744073709551615,"arr":[1,[2,3],{"x":4},"s"],"empty_o":{},"empty_a":[]})";
    auto r = p.parse(j);
    CHECK(r.has_value());
    if (!r) return;
    auto root = *r;
    CHECK(root.is_object());
    CHECK(root.type() == ft::dom_type::object);
    CHECK_EQ(root.get_object()->size(), static_cast<std::size_t>(11));
    CHECK_EQ(*root["id"]->get_int64(), static_cast<std::int64_t>(7));
    CHECK(root["id"]->type() == ft::dom_type::int64);
    CHECK_EQ(*root["name"]->get_string(), std::string_view("widget-long-name"));
    // Zero-copy: a long unescaped string points into the input buffer.
    {
        auto sv = *root["name"]->get_string();
        CHECK(sv.data() >= j.data() && sv.data() + sv.size() <= j.data() + j.size());
    }
    CHECK_EQ(*root["ok"]->get_bool(), true);
    CHECK_EQ(*root["no"]->get_bool(), false);
    CHECK(root["nil"]->is_null());
    CHECK_EQ(*root["pi"]->get_double(), 3.14159);
    CHECK_EQ(*root["neg"]->get_int64(), static_cast<std::int64_t>(-12));
    CHECK_EQ(*root["neg"]->get_double(), -12.0);
    CHECK(root["big"]->type() == ft::dom_type::uint64);
    CHECK_EQ(*root["big"]->get_uint64(), std::numeric_limits<std::uint64_t>::max());
    CHECK(root["big"]->get_int64().error().code == ft::error_code::number_out_of_range);
    CHECK(root["neg"]->get_uint64().error().code == ft::error_code::number_out_of_range);
    CHECK(root["name"]->get_int64().error().code == ft::error_code::type_error);
    CHECK(root["pi"]->get_int64().error().code == ft::error_code::type_error);
    CHECK(root["missing"].error().code == ft::error_code::key_not_found);
    CHECK(root["id"]->operator[]("x").error().code == ft::error_code::type_error);
    auto arr = root["arr"]->get_array();
    CHECK(arr.has_value());
    CHECK_EQ(arr->size(), static_cast<std::size_t>(4));
    CHECK_EQ(*arr->at(0)->get_int64(), static_cast<std::int64_t>(1));
    CHECK_EQ(arr->at(1)->get_array()->size(), static_cast<std::size_t>(2));
    CHECK((*arr->at(1))["k"].error().code == ft::error_code::type_error);   // [] on an array
    CHECK_EQ(*arr->at(1)->at(1)->get_int64(), static_cast<std::int64_t>(3));
    CHECK_EQ(*(*arr->at(2))["x"]->get_int64(), static_cast<std::int64_t>(4));
    CHECK_EQ(*arr->at(3)->get_string(), std::string_view("s"));
    CHECK(arr->at(4).error().code == ft::error_code::out_of_bounds);
    // Iteration visits every element exactly once, in order (O(1) container skip).
    {
        std::vector<int> kinds;
        for (auto e : *arr) kinds.push_back(static_cast<int>(e.type()));
        CHECK(kinds == std::vector<int>({static_cast<int>(ft::dom_type::int64), static_cast<int>(ft::dom_type::array),
                                         static_cast<int>(ft::dom_type::object), static_cast<int>(ft::dom_type::string)}));
    }
    {
        std::vector<std::string> keys;
        for (auto f : *root.get_object()) keys.emplace_back(f.key);
        CHECK(keys == std::vector<std::string>({"id", "name", "ok", "no", "nil", "pi", "neg", "big", "arr", "empty_o", "empty_a"}));
    }
    CHECK_EQ(root["empty_o"]->get_object()->size(), static_cast<std::size_t>(0));
    CHECK(root["empty_o"]->get_object()->begin() == root["empty_o"]->get_object()->end());
    CHECK_EQ(root["empty_a"]->get_array()->size(), static_cast<std::size_t>(0));
    CHECK(root["empty_a"]->get_array()->begin() == root["empty_a"]->get_array()->end());
    // Root scalars.
    for (auto [js, ty] : {std::pair{"42", ft::dom_type::int64}, std::pair{"\"s\"", ft::dom_type::string},
                          std::pair{"true", ft::dom_type::boolean}, std::pair{"null", ft::dom_type::null_value},
                          std::pair{" -1.5e3 ", ft::dom_type::double_value}}) {
        auto rr = p.parse(js);
        CHECK(rr.has_value());
        if (rr) CHECK(rr->type() == ty);
    }
    // Parser reuse: a smaller document after a bigger one, then a bigger one again.
    CHECK(p.parse(j).has_value());
    CHECK(p.parse("[1]").has_value());
    CHECK_EQ(*p.parse(j)->operator[]("id")->get_int64(), static_cast<std::int64_t>(7));
}

static void test_dom_strings_and_escapes() {
    ft::dom_parser p;
    struct sc { const char* json; std::string want; };
    const sc cases[] = {
        {R"(["plain"])", "plain"},
        {R"([""])", ""},
        {R"(["12345678"])", "12345678"},                       // exactly 8 bytes: inline
        {R"(["123456789"])", "123456789"},                     // 9 bytes: zero-copy view
        {R"(["a\"b"])", "a\"b"},
        {R"(["\\\/\b\f\n\r\t"])", "\\/\b\f\n\r\t"},
        {R"(["\u0041\u00e9\u20AC"])", "A\xC3\xA9\xE2\x82\xAC"},
        {R"(["\ud83d\ude00"])", "\xF0\x9F\x98\x80"},           // surrogate pair -> U+1F600
        {R"(["\u0000"])", std::string(1, '\0')},
        {R"(["x\u0022y"])", "x\"y"},
        {R"(["long string with an escape \n in the middle of it"])", "long string with an escape \n in the middle of it"},
    };
    for (const auto& c : cases) {
        auto r = p.parse(c.json);
        CHECK(r.has_value());
        if (!r) continue;
        auto s = r->at(0)->get_string();
        CHECK(s.has_value());
        if (s) {
            if (*s != c.want) std::println(std::cerr, "  string mismatch for {}", c.json);
            CHECK(*s == c.want);
        }
    }
    // Invalid escapes / lone surrogates are rejected.
    for (const char* bad : {R"(["\x"])", R"(["\u12"])", R"(["\u12G4"])", R"(["\ud800"])", R"(["\udc00"])",
                            R"(["\ud800\u0041"])", R"(["\ud800\\"])", R"(["\U0041"])", R"(["\'"])", R"(["\a"])"}) {
        auto r = p.parse(bad);
        if (r.has_value()) std::println(std::cerr, "  accepted bad escape {}", bad);
        CHECK(!r.has_value());
        if (!r) CHECK(r.error().code == ft::error_code::invalid_json);
    }
    // Escaped keys are found by their unescaped spelling; short and long.
    {
        auto r = p.parse(R"({"a\"b":1,"tab\tkey":2,"long\u0020escaped\u0020key":3,"plain":4})");
        CHECK(r.has_value());
        if (r) {
            CHECK_EQ(*(*r)["a\"b"]->get_int64(), static_cast<std::int64_t>(1));
            CHECK_EQ(*(*r)["tab\tkey"]->get_int64(), static_cast<std::int64_t>(2));
            CHECK_EQ(*(*r)["long escaped key"]->get_int64(), static_cast<std::int64_t>(3));
            CHECK_EQ(*(*r)["plain"]->get_int64(), static_cast<std::int64_t>(4));
            CHECK(!(*r)["a\\\"b"].has_value());
        }
    }
    // Escapes positioned around every 64-byte block boundary, plus unescaped
    // strings sharing a block with an escaped one (must stay zero-copy & exact).
    int bad = 0;
    for (int pad = 0; pad < 140; ++pad) {
        std::string j = "[\"" + std::string(static_cast<std::size_t>(pad), 'p') + "\\n\\u00e9q\",\"plain_neighbour_string\",\"" +
                        std::string(static_cast<std::size_t>(pad % 9), 'k') + "\"]";
        auto r = p.parse(j);
        if (!r) { ++bad; continue; }
        const std::string want0 = std::string(static_cast<std::size_t>(pad), 'p') + "\n\xC3\xA9q";
        if (*r->at(0)->get_string() != want0) ++bad;
        if (*r->at(1)->get_string() != "plain_neighbour_string") ++bad;
        auto nb = *r->at(1)->get_string();
        if (!(nb.data() >= j.data() && nb.data() < j.data() + j.size())) ++bad;   // zero-copy kept
        if (*r->at(2)->get_string() != std::string(static_cast<std::size_t>(pad % 9), 'k')) ++bad;
    }
    CHECK_EQ(bad, 0);
}

static void test_dom_integers_128() {
    ft::dom_parser p;
    auto r = p.parse("[9223372036854775807,-9223372036854775808,9223372036854775808,18446744073709551615,"
                     "18446744073709551616,-9223372036854775809,170141183460469231731687303715884105727,"
                     "-170141183460469231731687303715884105728,340282366920938463463374607431768211455,"
                     "340282366920938463463374607431768211456,-170141183460469231731687303715884105729,-0,0]");
    CHECK(r.has_value());
    if (!r) return;
    auto a = *r->get_array();
    using T = ft::dom_type;
    const T want_t[] = {T::int64, T::int64, T::uint64, T::uint64, T::uint128, T::int128, T::uint128,
                        T::int128, T::uint128, T::double_value, T::double_value, T::int64, T::int64};
    std::size_t k = 0;
    for (auto e : a) { CHECK(k < 13 && e.type() == want_t[k]); ++k; }
    CHECK_EQ(k, static_cast<std::size_t>(13));
    constexpr __int128 i128max = static_cast<__int128>((static_cast<unsigned __int128>(1) << 127) - 1);
    constexpr __int128 i128min = -i128max - 1;
    CHECK(*a.at(0)->get_int64() == std::numeric_limits<std::int64_t>::max());
    CHECK(*a.at(1)->get_int64() == std::numeric_limits<std::int64_t>::min());
    CHECK(*a.at(2)->get_uint64() == 9223372036854775808ULL);
    CHECK(*a.at(4)->get_uint128() == (static_cast<unsigned __int128>(1) << 64));
    CHECK(*a.at(5)->get_int128() == static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) - 1);
    CHECK(*a.at(6)->get_int128() == i128max);
    CHECK(*a.at(7)->get_int128() == i128min);
    CHECK(*a.at(8)->get_uint128() == ~static_cast<unsigned __int128>(0));
    CHECK(a.at(8)->get_int128().error().code == ft::error_code::number_out_of_range);
    CHECK(a.at(7)->get_uint128().error().code == ft::error_code::number_out_of_range);
    CHECK(*a.at(9)->get_double() == 340282366920938463463374607431768211456.0);
    CHECK(*a.at(10)->get_double() == -170141183460469231731687303715884105729.0);
    CHECK(*a.at(6)->get_double() == 170141183460469231731687303715884105727.0);
    CHECK(a.at(4)->get_int64().error().code == ft::error_code::number_out_of_range);
    // "-0" keeps its sign as a double but is still the integer 0.
    CHECK(*a.at(11)->get_int64() == 0);
    CHECK(std::bit_cast<std::uint64_t>(*a.at(11)->get_double()) == std::bit_cast<std::uint64_t>(-0.0));
    CHECK(std::bit_cast<std::uint64_t>(*a.at(12)->get_double()) == std::bit_cast<std::uint64_t>(0.0));
}

static void test_dom_grammar_corpus() {
    ft::dom_parser p;
    auto code = [&](std::string_view j) {
        auto r = p.parse(j);
        return r.has_value() ? ft::error_code::success : r.error().code;
    };
    // Must be accepted (RFC 8259).
    const char* accept[] = {
        "[]", "{}", "[[]]", "[{}]", "{\"\":0}", "{\"a\":{}}", " [ 1 , 2 ] ", "\n{\"a\" : [ true , false , null ] }\r\n",
        "[1,-1,0,-0,0.5,1e5,1E+5,1e-5,-0.0e0,123456789]", "[\"\\u0000\"]", "[\"\\u2028\\u2029\"]",
        "{\"a\":1,\"a\":2}", "[[[[[[[[[[1]]]]]]]]]]", "0", "-0", "\"\"", "[1e308]", "[-1e-320]",
    };
    for (const char* j : accept) {
        const auto c = code(j);
        if (c != ft::error_code::success) std::println(std::cerr, "  rejected valid {:?} code {}", j, static_cast<int>(c));
        CHECK(c == ft::error_code::success);
    }
    // Must be rejected, with the documented error code.
    struct rj { std::string json; ft::error_code code; };
    using ec = ft::error_code;
    const rj reject[] = {
        {"[1,]", ec::invalid_json}, {"[,1]", ec::invalid_json}, {"[,]", ec::invalid_json}, {"[1,,2]", ec::invalid_json},
        {"{\"a\":1,}", ec::invalid_json}, {"{,}", ec::invalid_json}, {"{\"a\"}", ec::invalid_json},
        {"{\"a\":}", ec::invalid_json}, {"{:1}", ec::invalid_json}, {"{1:1}", ec::invalid_json},
        {"{\"a\" 1}", ec::invalid_json}, {"{\"a\"::1}", ec::invalid_json}, {"{\"a\":1:2}", ec::invalid_json},
        {"[1 2]", ec::invalid_json}, {"{\"a\":1 \"b\":2}", ec::invalid_json}, {"[\"a\" \"b\"]", ec::invalid_json},
        {"[", ec::invalid_json}, {"]", ec::invalid_json}, {"{", ec::invalid_json}, {"}", ec::invalid_json},
        {"[[]", ec::invalid_json}, {"{]", ec::invalid_json}, {"[}", ec::invalid_json}, {"{\"a\":1]", ec::invalid_json},
        {"[1}", ec::invalid_json}, {"{\"a\":[}", ec::invalid_json}, {"[:]", ec::invalid_json}, {"[1:2]", ec::invalid_json},
        {"tru", ec::invalid_json}, {"nul", ec::invalid_json}, {"[True]", ec::invalid_json}, {"[nulll]", ec::invalid_json},
        {"[truefalse]", ec::invalid_json}, {"[NaN]", ec::invalid_json}, {"[Infinity]", ec::invalid_json},
        {"[-]", ec::invalid_json}, {"[01]", ec::invalid_json}, {"[1.]", ec::invalid_json}, {"[.1]", ec::invalid_json},
        {"[1e]", ec::invalid_json}, {"[+1]", ec::invalid_json}, {"[0x1]", ec::invalid_json}, {"[1.2.3]", ec::invalid_json},
        {"['a']", ec::invalid_json}, {"[1;2]", ec::invalid_json}, {"// c\n[1]", ec::invalid_json},
        {"/* c */ [1]", ec::invalid_json}, {"[1] // c", ec::trailing_content}, {"[\"a\"x]", ec::invalid_json},
        {"[1]x", ec::trailing_content}, {"[1] [2]", ec::trailing_content}, {"{} {}", ec::trailing_content},
        {"1 2", ec::trailing_content}, {"\"a\" \"b\"", ec::trailing_content}, {"[1]]", ec::trailing_content},
        {std::string("[1\x0C" "2]"), ec::invalid_json}, {std::string("[1\x1A" "2]"), ec::invalid_json},
        {std::string("[\x0C]"), ec::invalid_json}, {std::string("[1,\0 2]", 7), ec::invalid_json},
        {"[1e400]", ec::number_out_of_range}, {"[-1e400]", ec::number_out_of_range},
        {"{\"a\":\"\\x\"}", ec::invalid_json}, {"\xEF\xBB\xBF[]", ec::invalid_json},
    };
    for (const auto& r : reject) {
        const auto c = code(r.json);
        if (c != r.code) std::println(std::cerr, "  {:?}: expected code {} got {}", r.json, static_cast<int>(r.code), static_cast<int>(c));
        CHECK(c == r.code);
    }
    // Depth: default limit 1024 (same as simdjson); configurable.
    {
        const std::string d1024 = std::string(1024, '[') + std::string(1024, ']');
        const std::string d1025 = std::string(1025, '[') + std::string(1025, ']');
        CHECK(code(d1024) == ec::success);
        CHECK(code(d1025) == ec::depth_exceeded);
        ft::dom_parser deep(100000);
        const std::string d50k = std::string(50000, '[') + std::string(50000, ']');
        CHECK(deep.parse(d50k).has_value());
        ft::dom_parser shallow(2);
        CHECK(shallow.parse("[[1]]").has_value());
        CHECK(shallow.parse("[[[1]]]").error().code == ec::depth_exceeded);
        CHECK(shallow.parse("{\"a\":{\"b\":{}}}").error().code == ec::depth_exceeded);
    }
}

// Reference model for randomized round-trip tests.
struct jmodel {
    enum kind_t { k_null, k_bool, k_int, k_dbl, k_str, k_arr, k_obj } kind = k_null;
    bool b = false;
    std::int64_t i = 0;
    double d = 0;
    std::string s;
    std::vector<jmodel> arr;
    std::vector<std::pair<std::string, jmodel>> obj;
};

static std::string random_string(std::mt19937_64& rng, std::string& encoded) {
    std::string raw;
    encoded = "\"";
    const int len = static_cast<int>(rng() % 24);
    for (int k = 0; k < len; ++k) {
        const auto r = rng() % 20;
        if (r < 12) { char ch = static_cast<char>('a' + rng() % 26); raw += ch; encoded += ch; }
        else if (r == 12) { raw += '"'; encoded += "\\\""; }
        else if (r == 13) { raw += '\\'; encoded += "\\\\"; }
        else if (r == 14) { raw += '\n'; encoded += "\\n"; }
        else if (r == 15) { raw += "\xC3\xA9"; encoded += (rng() & 1) ? "\\u00e9" : "\xC3\xA9"; }
        else if (r == 16) { raw += "\xF0\x9F\x98\x80"; encoded += (rng() & 1) ? "\\uD83D\\uDE00" : "\xF0\x9F\x98\x80"; }
        else if (r == 17) { raw += '/'; encoded += (rng() & 1) ? "\\/" : "/"; }
        else if (r == 18) { raw += '\t'; encoded += "\\t"; }
        else { raw += ' '; encoded += ' '; }
    }
    encoded += '"';
    return raw;
}

static std::string ws(std::mt19937_64& rng) {
    static const char* w[] = {"", "", "", " ", "\n", "  ", "\t", "\r\n"};
    return w[rng() % 8];
}

static jmodel random_value(std::mt19937_64& rng, int depth, std::string& out) {
    jmodel m;
    const auto r = rng() % (depth > 6 ? 5 : 8);
    out += ws(rng);
    switch (r) {
        case 0: m.kind = jmodel::k_null; out += "null"; break;
        case 1: m.kind = jmodel::k_bool; m.b = rng() & 1; out += m.b ? "true" : "false"; break;
        case 2: {
            m.kind = jmodel::k_int;
            m.i = static_cast<std::int64_t>(rng()) >> (rng() % 63);
            out += std::to_string(m.i);
            break;
        }
        case 3: {
            m.kind = jmodel::k_dbl;
            do { m.d = std::bit_cast<double>(rng()); } while (!std::isfinite(m.d));
            if (rng() & 1) m.d = static_cast<double>(static_cast<std::int64_t>(rng() % 2000000) - 1000000) / 1000.0;
            char buf[64];
            auto res = std::to_chars(buf, buf + sizeof buf, m.d, std::chars_format::scientific, 17);
            out.append(buf, res.ptr);
            break;
        }
        case 4: { m.kind = jmodel::k_str; std::string enc; m.s = random_string(rng, enc); out += enc; break; }
        case 5: case 6: {
            m.kind = jmodel::k_arr; out += '[';
            const int n = static_cast<int>(rng() % 6);
            for (int k = 0; k < n; ++k) { if (k) out += ','; m.arr.push_back(random_value(rng, depth + 1, out)); }
            out += ws(rng); out += ']';
            break;
        }
        default: {
            m.kind = jmodel::k_obj; out += '{';
            const int n = static_cast<int>(rng() % 6);
            for (int k = 0; k < n; ++k) {
                if (k) out += ',';
                std::string enc;
                std::string key = random_string(rng, enc) + std::to_string(k);   // unique keys
                enc.insert(enc.size() - 1, std::to_string(k));
                out += ws(rng); out += enc; out += ws(rng); out += ':';
                m.obj.emplace_back(key, random_value(rng, depth + 1, out));
            }
            out += ws(rng); out += '}';
            break;
        }
    }
    out += ws(rng);
    return m;
}

static bool dom_matches(const ft::dom_element& e, const jmodel& m) {
    switch (m.kind) {
        case jmodel::k_null: return e.is_null();
        case jmodel::k_bool: return e.get_bool() && *e.get_bool() == m.b;
        case jmodel::k_int:  return e.get_int64() && *e.get_int64() == m.i;
        case jmodel::k_dbl: {
            auto d = e.get_double();
            return d && std::bit_cast<std::uint64_t>(*d) == std::bit_cast<std::uint64_t>(m.d);
        }
        case jmodel::k_str: return e.get_string() && *e.get_string() == m.s;
        case jmodel::k_arr: {
            auto a = e.get_array();
            if (!a || a->size() != m.arr.size()) return false;
            std::size_t k = 0;
            for (auto x : *a) { if (k >= m.arr.size() || !dom_matches(x, m.arr[k])) return false; ++k; }
            return k == m.arr.size();
        }
        case jmodel::k_obj: {
            auto o = e.get_object();
            if (!o || o->size() != m.obj.size()) return false;
            std::size_t k = 0;
            for (auto f : *o) {
                if (k >= m.obj.size() || f.key != m.obj[k].first || !dom_matches(f.value, m.obj[k].second)) return false;
                ++k;
            }
            for (const auto& [key, val] : m.obj) {      // keyed lookup agrees with iteration
                auto v = (*o)[key];
                if (!v || !dom_matches(*v, val)) return false;
            }
            return k == m.obj.size();
        }
    }
    return false;
}

static void test_dom_randomized_roundtrip_and_mutation() {
    std::mt19937_64 rng(0xD0D0'2026'0042ULL);
    ft::dom_parser p;
    int bad = 0, docs = 0;
    std::vector<std::string> corpus;
    for (int it = 0; it < 3000; ++it) {
        std::string json;
        const jmodel m = random_value(rng, 0, json);
        auto r = p.parse(json);
        ++docs;
        if (!r || !dom_matches(*r, m)) {
            if (bad < 5) std::println(std::cerr, "  roundtrip mismatch ({}): {}", r ? "value" : "parse error", json.substr(0, 200));
            ++bad;
        }
        if (it < 400) corpus.push_back(std::move(json));
    }
    CHECK_EQ(bad, 0);
    // Mutation robustness: random single-byte edits must never crash, and the
    // three front ends must stay deterministic (same input -> same verdict).
    int nondet = 0;
    long long mutants = 0, accepted = 0;
    for (const auto& base : corpus) {
        for (int k = 0; k < 40; ++k) {
            std::string m = base;
            const auto op = rng() % 3;
            const std::size_t pos = m.empty() ? 0 : rng() % m.size();
            static const char pool[] = "{}[]:,\"\\ 0123456789.eE+-tfnul\x01\x7F\x80\xC3\xFF";
            const char ch = pool[rng() % (sizeof pool - 1)];
            if (op == 0 && !m.empty()) m[pos] = ch;
            else if (op == 1) m.insert(m.begin() + static_cast<std::ptrdiff_t>(pos), ch);
            else if (!m.empty()) m.erase(pos, 1);
            auto r1 = p.parse(m);
            auto r2 = p.parse(m);
            ++mutants;
            if (r1.has_value() != r2.has_value() ||
                (!r1 && r1.error().code != r2.error().code)) ++nondet;
            if (r1) ++accepted;
        }
    }
    CHECK_EQ(nondet, 0);
    std::println("  dom: {} random documents round-tripped exactly; {} mutants parsed safely ({} still valid)",
                 docs, mutants, accepted);
}

// ---------------------------------------------------------------------------
// 24. Guard-page over-read safety
// ---------------------------------------------------------------------------
// Every document is copied so its LAST byte sits immediately before a
// PROT_NONE page: any read past the end of the input (stage 1, number
// scanning, key comparison, string unescaping, container skipping) faults
// instead of silently reading neighbouring memory. Regression for an
// OnDemand find_field bug that read in[ks + key_len] before bounds-checking.

namespace guard {
struct page_buf {
    std::size_t page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
    std::size_t data_pages = 4;   // up to 16 KiB documents on 4 KiB pages
    char* base = nullptr;
    page_buf() {
        void* m = ::mmap(nullptr, (data_pages + 1) * page, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (m == MAP_FAILED) return;
        base = static_cast<char*>(m);
        if (::mprotect(base + data_pages * page, page, PROT_NONE) != 0) { ::munmap(m, (data_pages + 1) * page); base = nullptr; }
    }
    ~page_buf() { if (base) ::munmap(base, (data_pages + 1) * page); }
    page_buf(const page_buf&) = delete;
    page_buf& operator=(const page_buf&) = delete;
    // Returns a view whose last byte is adjacent to the guard page.
    std::string_view place(std::string_view s) const {
        char* end = base + data_pages * page;
        char* dst = end - s.size();
        std::memcpy(dst, s.data(), s.size());
        return {dst, s.size()};
    }
    std::size_t capacity() const { return data_pages * page; }
};

// Touch every value reachable through the OnDemand API (bounded).
static void walk_od(fastjson::turbo::turbo_value v, int depth, int& budget) {
    if (depth > 64 || --budget <= 0) return;
    (void)v.get_double(); (void)v.get_int64(); (void)v.get_bool(); (void)v.get_string();
    if (auto a = v.get_array()) {
        int n = 0;
        for (auto e : *a) { walk_od(e, depth + 1, budget); if (++n > 4096 || budget <= 0) break; }
    }
    if (auto o = v.get_object()) {
        static const std::string_view keys[] = {
            "", "a", "id", "abc", "close", "k", "\\u0041",
            "a_much_longer_key_than_anything_in_the_document_0123456789"};
        for (auto k : keys)
            if (auto f = o->find_field(k)) walk_od(*f, depth + 1, budget);
    }
}

static void walk_dom(fastjson::turbo::dom_element e, int depth) {
    if (depth > 1100) return;
    (void)e.get_double(); (void)e.get_int64(); (void)e.get_uint64(); (void)e.get_bool();
    (void)e.get_string(); (void)e.get_int128(); (void)e.get_uint128();
    if (auto a = e.get_array()) for (auto x : *a) walk_dom(x, depth + 1);
    if (auto o = e.get_object()) {
        for (auto f : *o) { (void)f.key.size(); walk_dom(f.value, depth + 1); }
        (void)o->find_field("a_much_longer_key_than_anything_in_the_document_0123456789");
        (void)o->find_field("a");
    }
}
} // namespace guard

static void test_guard_page_overread() {
    guard::page_buf buf;
    CHECK(buf.base != nullptr);
    if (!buf.base) return;
    fastjson::turbo::turbo_parser tp;
    fastjson::turbo::dom_parser   dp;
    std::vector<std::string> docs = {
        "1", "-", "-0", "0.", "1e", "1e+", "-1.5e300", "123456789012345678901234567890",
        "0.1234567", "12345678", "-12345678.12345678", "1.7976931348623157e308", "4.9e-324",
        "true", "fals", "null", "nul", "\"\"", "\"abc\"", "\"unterminated", "\"esc\\", "\"\\u00",
        "\"\\uD83D\\uDE00\"", "\"\\uD83D\"", "[", "[1", "[1,", "[1,2]", "{", "{\"a\"", "{\"a\":",
        "{\"a\":1", "{\"a\":1}", "{\"abc\":\"x\"}", "{\"a\":[1,2,{\"b\":null}]}",
        "{\"id\":1,\"close\":2.5,\"k\":\"v\"}", "{\"a_much_longer_key\":1}",
        "[\"caf\xC3\xA9\"]", "\"\xE2\x82", "\"\xF0\x9F\x98\x80\"", "  \t\n 7  \n",
    };
    // Every length 1..200 of a number-and-string-dense document, so each
    // truncation point (mid-number, mid-key, mid-escape) lands on the guard.
    const std::string full =
        R"({"id":12345,"symbol":"SYM\"7\\","open":100.125,"close":-2.5e-3,"k":[1,2.0,true,null,"x\u00e9y"],"a":{"abc":"0123456789abcdef"}})";
    for (std::size_t n = 1; n <= full.size(); ++n) docs.push_back(full.substr(0, n));
    // Long documents crossing many 64-byte blocks, ending at the guard.
    std::string big = "[";
    for (int i = 0; i < 700; ++i) { if (i) big += ','; big += std::to_string(i * 0.37) + ",\"s" + std::to_string(i) + "\""; }
    for (std::size_t cut : {big.size(), big.size() - 1, big.size() - 3, big.size() - 7}) docs.push_back(big.substr(0, cut));
    big += ']';
    docs.push_back(big);

    std::mt19937_64 rng(0xC0FFEEu);
    const std::size_t base_count = docs.size();
    for (std::size_t i = 0; i < base_count; ++i) {
        for (int k = 0; k < 6; ++k) {
            std::string m = docs[i];
            if (m.empty()) break;
            static const char pool[] = "{}[]:,\"\\ 0123456789.eE+-tfnul\x01\x80\xC3";
            m[rng() % m.size()] = pool[rng() % (sizeof pool - 1)];
            docs.push_back(std::move(m));
        }
    }

    long long checked = 0;
    for (const auto& d : docs) {
        if (d.size() > buf.capacity()) continue;
        const std::string_view sv = buf.place(d);
        int budget = 20000;
        if (auto r = fastjson::turbo::iterate(sv, tp)) guard::walk_od(r->root(), 0, budget);
        budget = 20000;
        if (auto r = fastjson::turbo::parse(sv, tp)) guard::walk_od(r->root(), 0, budget);
        if (auto r = dp.parse(sv)) guard::walk_dom(*r, 0);
        ++checked;
    }
    CHECK(checked == static_cast<long long>(docs.size()));
    // Unclosed final strings whose closing-quote search would reach the
    // opening quote (lone '"', '"' + whitespace) — exact code on every path.
    for (std::string_view d : {"\"", "\" ", "\"\t\n ", "[1,\"", "[1,\"   ", "{\"k\":\"", "{\"k\":\" \n",
                               "{\"", "{\"  ", "[\"a\",\"", "\"\\\"", "\"\\\" "}) {
        const std::string_view sv = buf.place(d);
        auto r = dp.parse(sv);
        CHECK(!r.has_value());
        if (!r) CHECK(r.error().code == fastjson::turbo::error_code::unclosed_string);
        auto ri = fastjson::turbo::iterate(sv, tp);
        CHECK(!ri.has_value());
        if (!ri) CHECK(ri.error().code == fastjson::turbo::error_code::unclosed_string);
        auto rp = fastjson::turbo::parse(sv, tp);
        CHECK(!rp.has_value());
        if (!rp) CHECK(rp.error().code == fastjson::turbo::error_code::unclosed_string);
    }
    std::println("  guard-page: {} documents (incl. every prefix + mutants) parsed and walked on 3 front ends without over-read",
                 checked);
}

// ---------------------------------------------------------------------------
// 25. Explicit regression tests for failed / fixed code paths
// ---------------------------------------------------------------------------

static void test_regressions_found_during_audit() {
    using fastjson::turbo::error_code;
    guard::page_buf buf;
    CHECK(buf.base != nullptr);
    if (!buf.base) return;

    fastjson::turbo::turbo_parser tp;
    fastjson::turbo::dom_parser   dp;

    // Regression 1: find_field with a lookup key longer than the remaining
    // document bytes placed flush against a PROT_NONE guard page. Previously
    // turbo_object::find_field read in[ks + key_len] before checking
    // ks + key_len < idxs[curr + 1] (SIGSEGV on the guard page).
    {
        const std::string_view sv = buf.place(R"({"a":1})");
        const std::string_view long_key = "a_lookup_key_much_longer_than_the_entire_document_0123456789";
        auto ri = fastjson::turbo::iterate(sv, tp);
        CHECK(ri.has_value());
        if (ri) {
            auto o = ri->root().get_object();
            CHECK(o.has_value());
            auto f = o->find_field(long_key);
            CHECK(!f.has_value() && f.error().code == error_code::key_not_found);
            auto fa = o->find_field("a");
            CHECK(fa.has_value() && fa->get_int64() && *fa->get_int64() == 1);
        }
        auto rp = fastjson::turbo::parse(sv, tp);
        CHECK(rp.has_value());
        if (rp) {
            auto o = rp->root().get_object();
            CHECK(o.has_value());
            auto f = o->find_field(long_key);
            CHECK(!f.has_value() && f.error().code == error_code::key_not_found);
            auto fa = o->find_field("a");
            CHECK(fa.has_value() && fa->get_int64() && *fa->get_int64() == 1);
        }
        auto rd = dp.parse(sv);
        CHECK(rd.has_value());
        if (rd) {
            auto f = (*rd)[long_key];
            CHECK(!f.has_value() && f.error().code == error_code::key_not_found);
            auto fa = (*rd)["a"];
            CHECK(fa.has_value() && fa->get_int64() && *fa->get_int64() == 1);
        }
    }

    // Regression 2a: find_field must not falsely match an escaped quote inside
    // a key (e.g. key "a\"b" when searching for raw key "a\") on either
    // iterate() or parse(), with or without whitespace before ':'.
    for (std::string_view j : {std::string_view{R"({"a\"b":111,"a\\":222})"},
                               std::string_view{R"({"a\"b"  :  111 , "a\\"  :  222})"}}) {
        const std::string_view sv = buf.place(j);
        for (int mode = 0; mode < 2; ++mode) {
            auto doc = (mode == 0) ? fastjson::turbo::iterate(sv, tp)
                                   : fastjson::turbo::parse(sv, tp);
            CHECK(doc.has_value());
            if (!doc) continue;
            auto obj = doc->root().get_object();
            CHECK(obj.has_value());
            // Raw prefix "a\" before the escaped quote in "a\"b" must NOT match:
            auto v_bad = obj->find_field(R"(a\)");
            CHECK(!v_bad.has_value() && v_bad.error().code == error_code::key_not_found);
            // Raw key "a\\" (second field) must match 222:
            auto v = obj->find_field(R"(a\\)");
            CHECK(v.has_value());
            if (v) {
                auto iv = v->get_int64();
                CHECK(iv.has_value() && *iv == 222);
            }
        }
        // dom_parser unescapes keys: "a\"b" -> a"b (111) and "a\\" -> a\ (222).
        auto rd = dp.parse(sv);
        CHECK(rd.has_value());
        if (rd) {
            auto v1 = (*rd)[R"(a"b)"];
            CHECK(v1.has_value() && v1->get_int64() && *v1->get_int64() == 111);
            auto v2 = (*rd)[R"(a\)"];
            CHECK(v2.has_value() && v2->get_int64() && *v2->get_int64() == 222);
        }
    }
    for (std::string_view j : {std::string_view{R"({"a\"b":111})"},
                               std::string_view{R"({"a\"b"   :   111})"}}) {
        const std::string_view sv = buf.place(j);
        for (int mode = 0; mode < 2; ++mode) {
            auto doc = (mode == 0) ? fastjson::turbo::iterate(sv, tp)
                                   : fastjson::turbo::parse(sv, tp);
            CHECK(doc.has_value());
            if (!doc) continue;
            auto obj = doc->root().get_object();
            CHECK(obj.has_value());
            auto v = obj->find_field("a\\");
            CHECK(!v.has_value() && v.error().code == error_code::key_not_found);
        }
    }

    // Regression 2b: find_field on the parse() tape path must not span across
    // keys when the lookup string embeds '":111,"'.
    {
        const std::string_view sv = buf.place(R"({"a":111,"b":222})");
        for (int mode = 0; mode < 2; ++mode) {
            auto doc = (mode == 0) ? fastjson::turbo::iterate(sv, tp)
                                   : fastjson::turbo::parse(sv, tp);
            CHECK(doc.has_value());
            if (!doc) continue;
            auto obj = doc->root().get_object();
            CHECK(obj.has_value());
            auto v = obj->find_field("a\":111,\"b");
            CHECK(!v.has_value() && v.error().code == error_code::key_not_found);
        }
        auto rd = dp.parse(sv);
        CHECK(rd.has_value());
        if (rd) {
            auto v = (*rd)["a\":111,\"b"];
            CHECK(!v.has_value() && v.error().code == error_code::key_not_found);
        }
    }

    // Regression 3: dom_emit_string / s1_stream::refill length underflow on an
    // unclosed final string at end of input (previously e = ip[1] - 1 walked
    // back to the opening quote, underflowing slen = e - start to 0xFFFFFFFF
    // and reading past the input into the guard page).
    for (std::string_view u : {std::string_view{"\""}, std::string_view{"\"   "},
                               std::string_view{"\"\\\""}, std::string_view{"[1,\""},
                               std::string_view{"[1,\" \t\r\n"}, std::string_view{"{\"k\":\""},
                               std::string_view{"{\"k\":\"   "}}) {
        const std::string_view sv = buf.place(u);
        auto rd = dp.parse(sv);
        CHECK(!rd.has_value() && rd.error().code == error_code::unclosed_string);
        auto ri = fastjson::turbo::iterate(sv, tp);
        CHECK(!ri.has_value() && ri.error().code == error_code::unclosed_string);
        auto rp = fastjson::turbo::parse(sv, tp);
        CHECK(!rp.has_value() && rp.error().code == error_code::unclosed_string);
    }

    // Regression 4: bkt_stk[4096] stack overflow when nesting depth exceeds
    // 4096 in turbo::parse (and configurable max_depth in dom_parser).
    {
        const std::string ok_4096 = std::string(4096, '[') + "1" + std::string(4096, ']');
        const std::string too_deep_4097 = std::string(4097, '[') + "1" + std::string(4097, ']');
        const std::string too_deep_5000 = std::string(5000, '[') + "1" + std::string(5000, ']');
        auto r_ok = fastjson::turbo::parse(ok_4096, tp);
        CHECK(r_ok.has_value());
        auto r_4097 = fastjson::turbo::parse(too_deep_4097, tp);
        CHECK(!r_4097.has_value() && r_4097.error().code == error_code::depth_exceeded);
        auto r_5000 = fastjson::turbo::parse(too_deep_5000, tp);
        CHECK(!r_5000.has_value() && r_5000.error().code == error_code::depth_exceeded);
        auto d_5000 = dp.parse(too_deep_5000);
        CHECK(!d_5000.has_value() && d_5000.error().code == error_code::depth_exceeded);
    }

    // Regression 5: negative zero ("-0" and "-0.0") sign preservation,
    // get_int64 non-integer rejection ("1.5", "-0.0"), and scan_number_fast
    // 8-digit integer / 7-digit fraction boundary in both EOF and padded contexts.
    for (bool padded : {false, true}) {
        auto wrap = [&](std::string_view tok) -> std::string {
            return padded ? ("[" + std::string(tok) + ",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]")
                          : ("[" + std::string(tok) + "]");
        };
        // "-0": integer 0 for get_int64, -0.0 (signbit set) for get_double.
        {
            const std::string doc = wrap("-0");
            const std::string_view sv = buf.place(doc);
            auto ri = fastjson::turbo::iterate(sv, tp);
            auto rp = fastjson::turbo::parse(sv, tp);
            auto rd = dp.parse(sv);
            CHECK(ri.has_value() && rp.has_value() && rd.has_value());
            if (ri && rp && rd) {
                auto vi = *ri->root().get_array()->begin();
                auto vp = *rp->root().get_array()->begin();
                auto vd = *rd->at(0);
                CHECK(vi.get_int64() && *vi.get_int64() == 0);
                CHECK(vp.get_int64() && *vp.get_int64() == 0);
                CHECK(vd.get_int64() && *vd.get_int64() == 0);
                CHECK(vi.get_double() && *vi.get_double() == 0.0 && std::signbit(*vi.get_double()));
                CHECK(vp.get_double() && *vp.get_double() == 0.0 && std::signbit(*vp.get_double()));
                CHECK(vd.get_double() && *vd.get_double() == 0.0 && std::signbit(*vd.get_double()));
            }
        }
        // "-0.0": rejected by get_int64 (type_error), -0.0 (signbit set) for get_double.
        {
            const std::string doc = wrap("-0.0");
            const std::string_view sv = buf.place(doc);
            auto ri = fastjson::turbo::iterate(sv, tp);
            auto rp = fastjson::turbo::parse(sv, tp);
            auto rd = dp.parse(sv);
            CHECK(ri.has_value() && rp.has_value() && rd.has_value());
            if (ri && rp && rd) {
                auto vi = *ri->root().get_array()->begin();
                auto vp = *rp->root().get_array()->begin();
                auto vd = *rd->at(0);
                CHECK(!vi.get_int64() && vi.get_int64().error().code == error_code::type_error);
                CHECK(!vp.get_int64() && vp.get_int64().error().code == error_code::type_error);
                CHECK(!vd.get_int64() && vd.get_int64().error().code == error_code::type_error);
                CHECK(vi.get_double() && *vi.get_double() == 0.0 && std::signbit(*vi.get_double()));
                CHECK(vp.get_double() && *vp.get_double() == 0.0 && std::signbit(*vp.get_double()));
                CHECK(vd.get_double() && *vd.get_double() == 0.0 && std::signbit(*vd.get_double()));
            }
        }
        // 7-, 8-, and 9-digit integers (straddling scan_number_fast's 8-digit limit):
        for (std::int64_t expected : {9999999LL, -9999999LL, 12345678LL, -12345678LL,
                                      99999999LL, -99999999LL, 123456789LL, -123456789LL}) {
            const std::string doc = wrap(std::to_string(expected));
            const std::string_view sv = buf.place(doc);
            auto ri = fastjson::turbo::iterate(sv, tp);
            auto rp = fastjson::turbo::parse(sv, tp);
            auto rd = dp.parse(sv);
            CHECK(ri.has_value() && rp.has_value() && rd.has_value());
            if (ri && rp && rd) {
                auto vi = *ri->root().get_array()->begin();
                auto vp = *rp->root().get_array()->begin();
                auto vd = *rd->at(0);
                CHECK(vi.get_int64() && *vi.get_int64() == expected);
                CHECK(vp.get_int64() && *vp.get_int64() == expected);
                CHECK(vd.get_int64() && *vd.get_int64() == expected);
                const double ed = static_cast<double>(expected);
                CHECK(vi.get_double() && dbits(*vi.get_double()) == dbits(ed));
                CHECK(vp.get_double() && dbits(*vp.get_double()) == dbits(ed));
                CHECK(vd.get_double() && dbits(*vd.get_double()) == dbits(ed));
            }
        }
    }
    std::println("  audit-regressions: all 5 regression suites passed");
}

// ---------------------------------------------------------------------------
// 26. Multi-threaded concurrent parsing & traversal (ThreadSanitizer verification)
// ---------------------------------------------------------------------------
// Exercises 8 concurrent worker threads sharing read-only input buffers while
// running independent turbo_parser and dom_parser instances across valid,
// multi-64KB, and adversarial malformed inputs. Verifies zero data races on
// CPUID dispatch, power-of-5 / power-of-10 tables, and parser scratch buffers.

static void test_multithreaded_concurrent_parsing() {
    std::string large_doc = R"({"meta":{"version":3,"active":true},"items":[)";
    for (int i = 0; i < 1200; ++i) {
        if (i > 0) large_doc += ',';
        large_doc += std::format(
            R"({{"id":{},"symbol":"SYM_{}","price":{:.6f},"qty":{},"tags":["alpha","beta\n{}"]}})",
            i, i, 100.125 + i * 0.03125, i * 10 - 500, i);
    }
    large_doc += "]}";

    const std::vector<std::string> invalid_docs = {
        "{\"a\":1,}", "[1,2", "\"unclosed", "{\"k\":\"\xC0\xAF\"}",
        "[\"bad\\xescape\"]", "[1e400]", "{\"a\":1} trailing"
    };

    constexpr int kThreads = 8;
    constexpr int kIters = 40;
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            fastjson::turbo::turbo_parser tp_iter;
            fastjson::turbo::turbo_parser tp_tape;
            fastjson::turbo::dom_parser   dp;

            for (int it = 0; it < kIters; ++it) {
                auto ri = fastjson::turbo::iterate(large_doc, tp_iter);
                auto rp = fastjson::turbo::parse(large_doc, tp_tape);
                auto rd = dp.parse(large_doc);
                if (!ri || !rp || !rd) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                auto obj_i = ri->root().get_object();
                auto obj_p = rp->root().get_object();
                if (!obj_i || !obj_p) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                auto items_i_val = obj_i->find_field("items");
                auto items_p_val = obj_p->find_field("items");
                if (!items_i_val || !items_p_val) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                auto items_i = items_i_val->get_array();
                auto items_p = items_p_val->get_array();
                auto items_d = (*rd)["items"]->get_array();
                if (!items_i || !items_p || !items_d || items_d->size() != 1200) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                const std::size_t probe = static_cast<std::size_t>((t * 131 + it * 17) % 1200);
                auto elem_d = items_d->at(probe);
                if (!elem_d || !(*elem_d)["id"]->get_int64() ||
                    *(*elem_d)["id"]->get_int64() != static_cast<std::int64_t>(probe)) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                const auto& bad = invalid_docs[static_cast<std::size_t>((t + it) % invalid_docs.size())];
                if (dp.parse(bad).has_value()) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    CHECK_EQ(failures.load(), 0);
    std::println("  multithreaded: {} threads x {} iterations verified race-free", kThreads, kIters);
}

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
    test_iterate_cases();
    test_iterate_large_record();
    test_simd_64b_boundary_quotes_and_escapes();
    test_key_prefix_and_nested_cursor_safety();
    test_numeric_boundaries_and_errors();
    test_mr_integer_range_edges();
    test_lazy_linq_transducer_pipeline();
    test_double_exact_known_cases();
    test_double_exact_randomized();
    test_number_grammar_and_int64_strictness();
    test_utf8_validator_differential();
    test_stage1_validation_errors();
    test_dom_basic_api();
    test_dom_strings_and_escapes();
    test_dom_integers_128();
    test_dom_grammar_corpus();
    test_dom_randomized_roundtrip_and_mutation();
    test_guard_page_overread();
    test_regressions_found_during_audit();
    test_multithreaded_concurrent_parsing();

    std::println("\n========================================");
    std::println("Tests run:    {}", g_tests_run);
    std::println("Tests passed: {}", g_tests_passed);
    std::println("Tests failed: {}", g_tests_failed);
    std::println("========================================");

    if (g_tests_failed > 0) {
        std::println("RESULT: FAIL");
        return 1;
    }
    std::println("RESULT: PASS");
    return 0;
}
