// Tests for how a double is written: a whole value below 2^53 as its integer digits ("300000", not the shorter
// scientific "3e+05" std::to_chars picks), every other finite double in its shortest round-trip form, and a
// NaN or infinity unchanged (null from to_string(), an error from to_json() and the writer).
// Run: ./test_number_format (exits 0 on pass, non-zero on any failure)

import std;
import fastjson;

static int g_failed = 0;

#define CHECK(expr) do { \
    if (!(expr)) { std::println(std::cerr, "FAIL {}:{}: {}", __FILE__, __LINE__, #expr); ++g_failed; } \
} while (0)

#define CHECK_TEXT(got, want) do { \
    const std::string got_text = (got); \
    if (got_text != (want)) { \
        std::println(std::cerr, "FAIL {}:{}: got {} want {}", __FILE__, __LINE__, got_text, want); ++g_failed; } \
} while (0)

static auto text_of(double d) -> std::string { return fastjson::json_value(d).to_string(); }

static auto writer_text(double d) -> std::string {
    auto r = fastjson::writer{}.value(d).finish();
    return r ? *r : std::string("<error>");
}

int main() {
    // The reported defect and its neighbours.
    CHECK_TEXT(text_of(300000.0), "300000");
    CHECK_TEXT(text_of(-300000.0), "-300000");
    CHECK_TEXT(text_of(1000000.0), "1000000");
    CHECK_TEXT(text_of(900000.0), "900000");
    CHECK_TEXT(text_of(1.0), "1");
    CHECK_TEXT(text_of(42.0), "42");
    CHECK_TEXT(text_of(1e15), "1000000000000000");
    CHECK_TEXT(text_of(1e21), "1e+21");  // past 2^53: the shortest round-trip form, as before
    CHECK_TEXT(text_of(0.1), "0.1");
    CHECK_TEXT(text_of(-0.0), "-0");
    CHECK_TEXT(text_of(0.0), "0");
    CHECK_TEXT(text_of(1e-7), "1e-07");
    CHECK_TEXT(text_of(1.5), "1.5");
    CHECK_TEXT(text_of(300000.5), "300000.5");
    CHECK_TEXT(text_of(0.30000000000000004), "0.30000000000000004");
    // The edge of the exactly representable integers.
    CHECK_TEXT(text_of(9007199254740991.0), "9007199254740991");   // 2^53 - 1
    CHECK_TEXT(text_of(-9007199254740991.0), "-9007199254740991");
    CHECK_TEXT(text_of(9007199254740992.0), "9007199254740992");   // 2^53, the shortest form of it
    CHECK_TEXT(text_of(1.7976931348623157e308), "1.7976931348623157e+308");
    // Non-finite values are unchanged: null from to_string(), an error from to_json() and the writer.
    CHECK_TEXT(text_of(std::numeric_limits<double>::quiet_NaN()), "null");
    CHECK_TEXT(text_of(std::numeric_limits<double>::infinity()), "null");
    CHECK_TEXT(text_of(-std::numeric_limits<double>::infinity()), "null");
    CHECK(!fastjson::to_json(fastjson::json_value(std::numeric_limits<double>::quiet_NaN())).has_value());
    CHECK_TEXT(writer_text(std::numeric_limits<double>::infinity()), "<error>");
    // The streaming writer uses the same form.
    CHECK_TEXT(writer_text(300000.0), "300000");
    CHECK_TEXT(writer_text(0.1), "0.1");
    CHECK_TEXT(writer_text(1e21), "1e+21");
    CHECK_TEXT(writer_text(-0.0), "-0");
    // Parsed text round-trips: 300000 and 3e5 parse to the same value and write as the integer.
    for (const char* in : {"300000", "3e5", "3E+5", "300000.0"}) {
        auto p = fastjson::parse(in);
        CHECK(p.has_value());
        if (p) { CHECK_TEXT(p->to_string(), "300000"); }
    }
    // Round trip, exactly: every written text parses back to the same double, over whole values, powers of ten
    // and a deterministic spread of arbitrary doubles.
    std::mt19937_64 rng(0x5EEDF00D);
    std::vector<double> samples;
    for (int e = -30; e <= 30; ++e) {
        samples.push_back(std::pow(10.0, e));
        samples.push_back(3.0 * std::pow(10.0, e));
    }
    for (int i = 0; i < 20000; ++i) {
        samples.push_back(static_cast<double>(static_cast<std::int64_t>(rng() >> 11)));  // whole, up to 2^53
        samples.push_back(std::bit_cast<double>(rng()));                                   // any bit pattern
        samples.push_back(static_cast<double>(rng() % 2000000) / 1000.0);                  // short fractions
    }
    std::size_t checked = 0;
    for (const double d : samples) {
        if (!std::isfinite(d)) { continue; }
        const std::string t = text_of(d);
        double back = 0;
        const auto [end, ec] = std::from_chars(t.data(), t.data() + t.size(), back);
        CHECK(ec == std::errc{} && end == t.data() + t.size());
        CHECK(std::bit_cast<std::uint64_t>(back) == std::bit_cast<std::uint64_t>(d));
        // The text is a valid JSON number, and carries no exponent for a whole value below 2^53.
        CHECK(!t.empty() && (t[0] == '-' || (t[0] >= '0' && t[0] <= '9')));
        if (d == std::trunc(d) && std::fabs(d) < 9007199254740992.0) {
            CHECK(t.find_first_of(".eE") == std::string::npos);
        }
        ++checked;
    }
    CHECK(checked > 50000);

    if (g_failed != 0) {
        std::println(std::cerr, "test_number_format: {} check(s) failed", g_failed);
        return 1;
    }
    std::println("test_number_format: all checks passed ({} round trips)", checked);
    return 0;
}
