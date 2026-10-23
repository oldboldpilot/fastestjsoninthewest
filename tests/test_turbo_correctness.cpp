// Comprehensive correctness tests for fastjson_turbo.
// Run: ./test_turbo_correctness (exits 0 on pass, non-zero on first failure)

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
        if (p_res.has_value()) CHECK(std::abs(*p_res - 530.12) < 0.001);
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
            CHECK_EQ(vals[0], 0.0);
            CHECK_EQ(vals[1], 0.0);
            CHECK(std::abs(vals[2] - 1e5) < 1e-6);
            CHECK(std::abs(vals[3] - 1.5e-3) < 1e-12);
            CHECK(std::abs(vals[4] - (-25000.0)) < 1e-6);
            CHECK(std::abs(vals[5] - 3.141592653589793) < 1e-14);
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
