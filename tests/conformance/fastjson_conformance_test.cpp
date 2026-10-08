/**
 * fastjson_conformance_test -- malformed JSON is an ERROR, never a partial result or a crash; well-formed JSON
 * round-trips; every SIMD tier agrees with the scalar one.
 *
 * Run once per string-scan tier: the binary takes the tier name as argv[1] only for its report; the tier itself is
 * chosen by the FASTJSON_SIMD environment variable (scalar | avx2 | unset = best), which the registering build sets.
 *
 * Sections:
 *   strings    -- \u escapes (surrogate pairs decode to 4-byte UTF-8; a lone surrogate is an error), raw invalid
 *                 UTF-8 is an error, control bytes and embedded NULs are errors, every escape decodes.
 *   numbers    -- RFC 8259 grammar; NaN/Infinity literals are errors; a value no representation holds is an
 *                 error, never a silent NaN/inf; one past double range keeps its digits; is_number() is true for
 *                 every JSON number; integer accessors refuse an out-of-range value.
 *   structure  -- truncation at every byte of a valid document is an error; trailing commas, missing colons,
 *                 trailing garbage and empty input are errors; nesting past the depth limit is an error, and a
 *                 very deep input neither crashes nor overflows the stack.
 *   serialize  -- to_string() output is valid JSON that parses back to an equal value (control bytes, quotes,
 *                 backslashes, non-ASCII, non-finite numbers).
 *   ondemand   -- the lazy API refuses a malformed document instead of indexing it.
 *   writer     -- the streaming writer (fastjson::writer), escape_string and to_json emit valid JSON in call
 *                 order and refuse an unbalanced or keyless sequence, a non-finite number and non-UTF-8 text.
 *
 * Author: Olumuyiwa Oluwasanmi
 */
import std;
import fastjson;

namespace {

int g_failures = 0;
int g_checks = 0;

auto check(bool ok, std::string_view what) -> void {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::println("[FAIL] {}", what);
    }
}

// Printable form of an input for failure messages: bytes outside printable ASCII become \xNN.
[[nodiscard]] auto shown(std::string_view s) -> std::string {
    std::string out;
    for (const char c : s.substr(0, 80)) {
        const auto b = static_cast<unsigned char>(c);
        if (b >= 0x20 && b < 0x7F) {
            out += c;
        } else {
            out += std::format("\\x{:02X}", b);
        }
    }
    if (s.size() > 80) { out += "..."; }
    return out;
}

auto expectError(std::string_view input, std::string_view why) -> void {
    const auto r = fastjson::parse(input);
    check(!r.has_value(), std::format("{}: parse(\"{}\") must be an error", why, shown(input)));
}

auto expectString(std::string_view input, std::string_view want, std::string_view why) -> void {
    const auto r = fastjson::parse(input);
    if (!r.has_value()) {
        check(false, std::format("{}: parse(\"{}\") failed: {}", why, shown(input), r.error().message));
        return;
    }
    const bool is_str = r.value().is_string();
    check(is_str && r.value().as_string() == want,
          std::format("{}: parse(\"{}\") -> \"{}\", want \"{}\"", why, shown(input),
                      is_str ? shown(r.value().as_string()) : std::string{"<not a string>"}, shown(want)));
}

// ---- strings ------------------------------------------------------------------------------------------------

auto runStrings() -> void {
    expectString(R"("\uD83D\uDE00")", "\xF0\x9F\x98\x80", "surrogate pair decodes to one 4-byte code point");
    expectString(R"("\ud83d\ude00")", "\xF0\x9F\x98\x80", "lower-case surrogate pair");
    expectString(R"("a\uD834\uDD1Eb")", "a\xF0\x9D\x84\x9E" "b", "surrogate pair between ASCII");
    expectString(R"("\u00e9\u20AC\u0041")", "\xC3\xA9\xE2\x82\xAC" "A", "BMP escapes");
    expectString(R"("\u0000")", std::string_view{"\0", 1}, "escaped NUL is data");
    expectString(R"("\"\\\/\b\f\n\r\t")", "\"\\/\b\f\n\r\t", "every simple escape");
    expectString("\"\xC3\xA9\xF0\x9F\x98\x80\"", "\xC3\xA9\xF0\x9F\x98\x80", "raw valid UTF-8");
    expectString("\"x\xE2\x82\xACy\\n\"", "x\xE2\x82\xACy\n", "raw UTF-8 beside an escape (slow path)");

    expectError(R"("\uD83D")", "lone high surrogate");
    expectError(R"("\uDE00")", "lone low surrogate");
    expectError(R"("\uD83Dx")", "high surrogate followed by a plain byte");
    expectError(R"("\uD83D\u0041")", "high surrogate followed by a non-surrogate escape");
    expectError(R"("\uD83D\uD83D")", "two high surrogates");
    expectError(R"("\u12")", "truncated \\u escape");
    expectError(R"("\u12G4")", "non-hex digit in \\u escape");
    expectError(R"("\x41")", "unknown escape");
    expectError("\"\\", "escape at end of input");
    expectError("\"abc", "unterminated string");
    expectError(std::string_view{"\"a\0b\"", 5}, "raw NUL inside a string");
    expectError("\"a\x1F" "b\"", "raw control byte inside a string");
    expectError("\"a\nb\"", "raw newline inside a string");

    // Raw invalid UTF-8 (RFC 8259 section 8.1: JSON text MUST be UTF-8), on the fast path (no escape) and the
    // slow path (an escape elsewhere in the same string).
    constexpr std::array<std::string_view, 9> bad_utf8{
        "\xC3\x28",          // bad continuation
        "\xFF",              // never valid
        "\xC0\xAF",          // overlong '/'
        "\xE0\x80\xAF",      // overlong 3-byte
        "\xED\xA0\x80",      // UTF-8-encoded surrogate
        "\xF4\x90\x80\x80",  // above U+10FFFF
        "\xE2\x82",          // truncated 3-byte
        "\x80",              // lone continuation
        "\xF8\x88\x80\x80\x80",  // 5-byte form
    };
    for (const auto b : bad_utf8) {
        expectError(std::format("\"a{}b\"", b), "raw invalid UTF-8 (fast path)");
        expectError(std::format("\"a{}b\\n\"", b), "raw invalid UTF-8 (escape path)");
        expectError(std::format("{{\"k{}\":1}}", b), "raw invalid UTF-8 in an object key");
    }
}

// ---- numbers ------------------------------------------------------------------------------------------------

[[nodiscard]] auto parsed(std::string_view text) -> std::optional<fastjson::json_value> {
    auto r = fastjson::parse(text);
    if (!r) { return std::nullopt; }
    return std::move(r).value();
}

auto runNumbers() -> void {
    for (const std::string_view bad : {"NaN", "nan", "Infinity", "-Infinity", "inf", "+1", "01", "-", "1.", ".5",
                                       "1e", "1e+", "--1", "0x10", "1.2.3", "1e5e5", "-01"}) {
        expectError(bad, "number grammar");
    }
    for (const std::string_view huge : {"1e5000", "-1e5000", "[1e99999]", "{\"a\":-2e5000}",
                                        "123456789012345678901234567890e5000"}) {
        expectError(huge, "a number outside every representation is an error, never NaN or infinity");
    }
    // Past double range but inside the 128-bit reader's: a finite value that prints back as digits, never "inf".
    for (const std::string_view wide : {"1e999", "-2e308", "1.5e400", "1e-400", "123456789012345678901234567890e300"}) {
        const auto v = parsed(wide);
        check(v.has_value() && v.value().is_number(), std::format("{} parses as a number", wide));
        if (!v.has_value()) { continue; }
        const std::string text = v.value().to_string();
        const auto again = parsed(text);
        check(text.find("inf") == std::string::npos && text.find("nan") == std::string::npos && again.has_value() &&
                  again.value().is_number() && again.value().to_string() == text,
              std::format("{} prints as a number that reads back to itself (got {})", wide, text));
    }
    // is_number() means "a JSON number", whatever representation the parser picked for it.
    for (const std::string_view num : {"0.30000000000000004", "-1.2345678901234567", "12345678901234567890",
                                       "9007199254740993", "3.14159265358979323846264338327950288",
                                       "340282366920938463463374607431768211455", "-170141183460469231731687303715884105728"}) {
        const auto v = parsed(num);
        check(v.has_value() && v.value().is_number(), std::format("is_number() is true for {}", num));
    }
    {
        const auto v = parsed("0.30000000000000004");
        check(v.has_value() && v.value().to_string() == "0.30000000000000004" &&
                  v.value().as_number() == 0.30000000000000004,
              "a 17-digit double keeps its value and prints back unchanged");
        check(v.has_value() && !v.value().is_number_128() && !v.value().is_int_128(),
              "a 17-digit float is held as a double (17 digits identify a double; only more need 128 bits)");
        const auto eighteen = parsed("0.123456789012345678");
        check(eighteen.has_value() && eighteen.value().is_number_128(), "an 18-digit float is held in 128 bits");
        const auto pi = parsed("3.14159265358979323846264338327950288");
        check(pi.has_value() && pi.value().to_string().starts_with("3.141592653589793238"),
              std::format("a 36-digit float keeps more than double precision ({})",
                          pi.has_value() ? pi.value().to_string() : std::string{"error"}));
    }
    {
        const auto r = fastjson::parse("[0, -0, 1.5, -2.25e3, 9007199254740993]");
        check(r.has_value() && r.value().is_array() && r.value().size() == 5, "well-formed numbers parse");
        if (r.has_value() && r.value().size() == 5) {
            const auto& a = r.value().as_array();
            check(a[2].as_number() == 1.5 && a[3].as_number() == -2250.0, "fractions and exponents");
            // 2^53 + 1 is not a double: the value must keep every digit (a 128-bit integer), not round.
            check(a[4].to_string() == "9007199254740993", std::format("2^53+1 keeps its digits ({})", a[4].to_string()));
        }
    }
    {
        const auto r = fastjson::parse("340282366920938463463374607431768211455");  // 2^128 - 1
        check(r.has_value() && r.value().to_string() == "340282366920938463463374607431768211455",
              "2^128-1 parses exactly");
        expectError("340282366920938463463374607431768211456", "an integer past 128 bits is an error");
    }
    // Floating -> integer accessors: in range truncates; out of range is refused (it was undefined behaviour).
    {
        const auto ok = parsed("42.9");
        check(ok.has_value() && ok.value().as_int64() == 42 && ok.value().as_uint64() == 42, "42.9 -> 42");
        const auto refused = [](std::string_view text, auto&& get) {
            const auto v = parsed(text);
            if (!v.has_value()) { return false; }
            try {
                (void)get(v.value());
            } catch (const std::out_of_range&) {
                return true;
            }
            return false;
        };
        check(refused("1e30", [](const fastjson::json_value& v) { return v.as_int64(); }), "as_int64(1e30) is refused");
        check(refused("-1", [](const fastjson::json_value& v) { return v.as_uint64(); }), "as_uint64(-1.0) is refused");
        check(refused("1e999", [](const fastjson::json_value& v) { return v.as_int64(); }),
              "as_int64 of a 128-bit float past int64 is refused");
        check(refused("1e40", [](const fastjson::json_value& v) { return v.as_int128(); }), "as_int128(1e40) is refused");
    }
}

// ---- structure ----------------------------------------------------------------------------------------------

auto runStructure() -> void {
    for (const std::string_view bad : {"", "   ", "[", "[1", "[1,", "[1,]", "[,1]", "{", "{\"a\"", "{\"a\":",
                                       "{\"a\":}", "{\"a\" 1}", "{\"a\":1,}", "{,}", "{1:2}", "{\"a\":1 \"b\":2}",
                                       "tru", "nul", "fals", "truex", "[]]", "{}}", "{}x", "1 2", "[1] [2]",
                                       "\"a\" \"b\"", "]", "}", ":", ","}) {
        expectError(bad, "structure");
    }
    expectError(std::string_view{"[1]\0", 4}, "NUL after the value");
    expectError(std::string_view{"\0[1]", 4}, "NUL before the value");

    // Truncating a valid document at EVERY byte must give an error (never a partial value) -- except where the
    // prefix is itself a complete document ("1" from "12", or the whole input).
    const std::string doc =
        R"({"name":"x\u00e9\uD83D\uDE00","n":[1,-2.5e3,true,false,null,{"k":"v\n"}],"big":12345678901234567890})";
    check(fastjson::parse(doc).has_value(), "the truncation fixture itself parses");
    for (std::size_t len = 0; len < doc.size(); ++len) {
        expectError(std::string_view{doc}.substr(0, len), std::format("truncated at byte {}", len));
    }

    // Depth: up to the limit parses; past it is an error; very deep input neither crashes nor exhausts the stack.
    auto nested = [](std::size_t depth) { return std::string(depth, '[') + std::string(depth, ']'); };
    check(fastjson::parse(nested(500)).has_value(), "500 nested arrays parse");
    expectError(nested(5000), "5000 nested arrays exceed the depth limit");
    expectError(std::string(200000, '['), "200000 unclosed '[' is an error, not a stack overflow");
    std::string deep_obj;
    for (int i = 0; i < 100000; ++i) { deep_obj += "{\"a\":"; }
    expectError(deep_obj, "100000 unclosed objects is an error, not a stack overflow");

    // Duplicate keys: one deterministic rule (the last value wins), not an unspecified one.
    const auto dup = fastjson::parse(R"({"a":1,"a":2})");
    check(dup.has_value() && dup.value().is_object() && dup.value().size() == 1 && dup.value()["a"].as_number() == 2.0,
          "duplicate key: the last value wins");
}

// ---- serialize ----------------------------------------------------------------------------------------------

// Value equality. Member ORDER is not compared: a json_object is a hash map, so the order to_string() writes is
// unspecified (fastjson::writer is the call-order tool), while every member and value must survive.
[[nodiscard]] auto sameValue(const fastjson::json_value& a, const fastjson::json_value& b) -> bool {
    if (a.is_object() || b.is_object()) {
        if (!a.is_object() || !b.is_object() || a.size() != b.size()) { return false; }
        for (const auto& [key, value] : a.as_object()) {
            if (!b.contains(key) || !sameValue(value, b[key])) { return false; }
        }
        return true;
    }
    if (a.is_array() || b.is_array()) {
        if (!a.is_array() || !b.is_array() || a.size() != b.size()) { return false; }
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (!sameValue(a[i], b[i])) { return false; }
        }
        return true;
    }
    return a.to_string() == b.to_string();  // scalars: their JSON text identifies them
}

[[nodiscard]] auto roundTrips(const fastjson::json_value& v) -> bool {
    const auto back = fastjson::parse(v.to_string());
    return back.has_value() && sameValue(back.value(), v);
}

auto runSerialize() -> void {
    std::string all_bytes;
    for (int b = 1; b < 0x80; ++b) { all_bytes += static_cast<char>(b); }
    all_bytes += "\xC3\xA9\xF0\x9F\x98\x80";
    all_bytes += std::string_view{"\0tail", 5};
    const fastjson::json_value s{all_bytes};
    const std::string text = s.to_string();
    const auto back = fastjson::parse(text);
    check(back.has_value() && back.value().is_string() && back.value().as_string() == all_bytes,
          std::format("every ASCII byte, NUL and non-ASCII round-trip through to_string ({})", shown(text)));

    for (const double nonfinite : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                                   -std::numeric_limits<double>::infinity()}) {
        fastjson::json_value arr{fastjson::json_array{}};
        arr.push_back(fastjson::json_value{nonfinite});
        const std::string out = arr.to_string();
        check(fastjson::parse(out).has_value(),
              std::format("a non-finite double serialises to valid JSON (got {})", out));
        check(!fastjson::to_json(arr).has_value(), "to_json refuses a non-finite number (it has no JSON form)");
    }
    const auto parsed = fastjson::parse(
        R"({"a":[1,2.5,"s\"q",{"b":null}],"c":-0,"u":"\u2028\u2029","big":170141183460469231731687303715884105727})");
    check(parsed.has_value() && roundTrips(parsed.value()), "a mixed document round-trips");
    check(parsed.has_value() && fastjson::parse(parsed.value().to_pretty_string(2)).has_value(),
          "pretty output parses");
    check(parsed.has_value() && fastjson::to_json(parsed.value()).has_value() &&
              fastjson::to_json(parsed.value()).value() == parsed.value().to_string(),
          "to_json of a writable value is to_string()");
    check(!fastjson::to_json(fastjson::json_value{std::string{"bad \xFF"}}).has_value(),
          "to_json refuses a string that is not UTF-8");
}

// ---- ondemand -----------------------------------------------------------------------------------------------

auto runOndemand() -> void {
    for (const std::string_view bad : {"[1,", "{\"a\":", "[1]]", "{\"a\" 1}", "\"abc", "[\"\xFF\"]", "tru", "[1 2]"}) {
        const auto d = fastjson::ondemand_document::parse(std::string{bad});
        check(!d.has_value(), std::format("ondemand: parse(\"{}\") must be an error", shown(bad)));
    }
    auto ok = fastjson::ondemand_document::parse(std::string{R"({"k":[1,2,3],"s":"v"})"});
    check(ok.has_value(), "ondemand: a well-formed document parses");
}

// ---- writer -------------------------------------------------------------------------------------------------

auto runWriter() -> void {
    // escape_string: the same escaping to_string uses, available to code that writes JSON text itself.
    const auto esc = fastjson::escape_string("a\"b\\c\n\x01\xC3\xA9");
    check(esc.has_value() && esc.value() == "\"a\\\"b\\\\c\\n\\u0001\xC3\xA9\"",
          std::format("escape_string ({})", esc.has_value() ? shown(esc.value()) : esc.error().message));
    check(!fastjson::escape_string("bad \xFF").has_value(), "escape_string refuses text that is not UTF-8");

    // The streaming writer keeps CALL ORDER (a json_object is a hash map and does not).
    auto out = fastjson::writer{}
                   .begin_object()
                   .key("z").value(1)
                   .key("a").value("x\"y")
                   .key("list").begin_array().value(true).value(nullptr).value(2.5).value(std::int64_t{-7}).end_array()
                   .key("obj").begin_object().key("k").value(std::string_view{"v"}).end_object()
                   .end_object()
                   .finish();
    check(out.has_value() && out.value() == R"({"z":1,"a":"x\"y","list":[true,null,2.5,-7],"obj":{"k":"v"}})",
          std::format("writer output in call order ({})", out.has_value() ? out.value() : out.error().message));
    check(out.has_value() && fastjson::parse(out.value()).has_value(), "writer output parses");

    check(!fastjson::writer{}.begin_object().value(1).end_object().finish().has_value(),
          "writer: a value inside an object without a key is refused");
    check(!fastjson::writer{}.begin_array().finish().has_value(), "writer: an unclosed array is refused");
    check(!fastjson::writer{}.begin_array().end_object().finish().has_value(), "writer: a mismatched close is refused");
    check(!fastjson::writer{}.value(1).value(2).finish().has_value(), "writer: two top-level values are refused");
    check(!fastjson::writer{}.begin_array().value(std::numeric_limits<double>::quiet_NaN()).end_array().finish()
              .has_value(),
          "writer: a non-finite number is refused");
    check(!fastjson::writer{}.begin_object().key("a").key("b").value(1).end_object().finish().has_value(),
          "writer: two keys in a row are refused");
    check(!fastjson::writer{}.finish().has_value(), "writer: an empty document is refused");
    check(!fastjson::writer{}.begin_array().value(std::string_view{"\xFF"}).end_array().finish().has_value(),
          "writer: invalid UTF-8 text is refused");
    // A pre-encoded fragment from to_string() embeds verbatim.
    const auto inner = fastjson::parse(R"({"q":[1,2]})");
    auto raw = fastjson::writer{}.begin_array().raw(inner.value().to_string()).end_array().finish();
    check(raw.has_value() && raw.value() == R"([{"q":[1,2]}])", "writer: raw() embeds a serialised value");
    check(!fastjson::writer{}.begin_array().raw("{").end_array().finish().has_value(),
          "writer: raw() refuses text that is not one JSON value");
}

}  // namespace

auto main(int argc, char** argv) -> int {
    const std::string_view tier = argc > 1 ? std::string_view{argv[1]} : std::string_view{"default"};
    runStrings();
    runNumbers();
    runStructure();
    runSerialize();
    runOndemand();
    runWriter();
    std::println("fastjson_conformance [{} / scan tier {}]: {} checks, {} failed", tier, fastjson::string_scan_tier(),
                 g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
