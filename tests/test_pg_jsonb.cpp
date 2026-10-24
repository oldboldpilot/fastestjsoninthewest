// Tests for fastjson::parse_pg_jsonb -- the PostgreSQL jsonb binary wire format adapter.
// Run: ./test_pg_jsonb (exits 0 on pass, non-zero on any failure)

import std;
import fastjson;

static int g_failed = 0;

#define CHECK(expr) do { \
    if (!(expr)) { std::println(std::cerr, "FAIL {}:{}: {}", __FILE__, __LINE__, #expr); ++g_failed; } \
} while (0)

// A binary-mode jsonb value as libpq hands it over: version byte, then JSON text.
static auto wire(std::string_view json, char version = '\x01') -> std::string {
    std::string w(1, version);
    w += json;
    return w;
}

int main() {
    // Object with nested values parses identically to the text it carries.
    {
        auto r = fastjson::parse_pg_jsonb(wire(R"({"a": 1, "b": [true, null, "x"]})"));
        CHECK(r.has_value());
        CHECK(r && r->is_object());
        CHECK(r && r->as_object().at("a").as_number() == 1.0);
        CHECK(r && r->as_object().at("b").as_array().size() == 3);
    }
    // Scalar roots are valid jsonb.
    {
        auto r = fastjson::parse_pg_jsonb(wire("\"hello\""));
        CHECK(r && r->is_string() && r->as_string() == "hello");
        auto n = fastjson::parse_pg_jsonb(wire("null"));
        CHECK(n && n->is_null());
    }
    // Empty wire value: no version byte at all.
    {
        auto r = fastjson::parse_pg_jsonb("");
        CHECK(!r && r.error().code == fastjson::json_error_code::empty_input);
    }
    // Version byte with nothing after it is empty JSON text.
    {
        auto r = fastjson::parse_pg_jsonb(wire(""));
        CHECK(!r && r.error().code == fastjson::json_error_code::empty_input);
    }
    // Unknown version bytes are rejected, including a text-mode value passed by mistake.
    {
        auto r = fastjson::parse_pg_jsonb(wire("{}", '\x02'));
        CHECK(!r && r.error().code == fastjson::json_error_code::unsupported_jsonb_version);
        auto t = fastjson::parse_pg_jsonb(R"({"a":1})");
        CHECK(!t && t.error().code == fastjson::json_error_code::unsupported_jsonb_version);
        auto z = fastjson::parse_pg_jsonb(wire("{}", '\x00'));
        CHECK(!z && z.error().code == fastjson::json_error_code::unsupported_jsonb_version);
    }
    // Malformed JSON after a valid version byte surfaces the parser's own error.
    {
        auto r = fastjson::parse_pg_jsonb(wire("{\"a\":"));
        CHECK(!r);
    }

    if (g_failed != 0) {
        std::println(std::cerr, "{} check(s) failed", g_failed);
        return 1;
    }
    std::println("test_pg_jsonb: all checks passed");
    return 0;
}
