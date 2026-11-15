/**
 * test_prefix_recognizer -- fastjson::prefix_recognizer against fastjson::parse itself.
 *
 * Run once per string-scan tier, like fastjson_conformance_test: FASTJSON_SIMD (scalar | avx2 | unset = best) picks
 * the tier, argv[1] names it for the report. argv[2], when given, is tests/proofs/prefix_structural: the committed
 * SMT scripts, which must equal the ones the CURRENT table generates (`--emit-smt <dir>` rewrites them).
 *
 * Sections:
 *   agreement   -- complete() is true iff parse() accepts: a hand corpus of every grammar corner (escapes,
 *                  surrogates, raw UTF-8, number grammar and range, depth, structure), every prefix of every
 *                  valid document, and 20,000 random documents with 5 random mutations each.
 *   chunking    -- one push, byte-by-byte steps and every two-chunk split reach the same state.
 *   viability   -- every prefix the recognizer accepts can still be finished: a completion built from expects()
 *                  alone parses. With agreement this is the "valid prefix" half of the contract.
 *   events      -- step() reports what each byte did (the API a schema layer is built on).
 *   smt         -- the structural table, as SMT-LIB obligations (one .smt2 per property under tests/proofs/prefix_structural).
 *   concurrency -- eight threads try candidates from one shared committed state.
 *   speed       -- state copy, candidate feed and bulk push, measured (printed, not gated).
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
        if (g_failures <= 40) { std::println("[FAIL] {}", what); }
    }
}

[[nodiscard]] auto shown(std::string_view s) -> std::string {
    std::string out;
    for (const char c : s.substr(0, 100)) {
        const auto b = static_cast<unsigned char>(c);
        out += (b >= 0x20 && b < 0x7F) ? std::string(1, c) : std::format("\\x{:02X}", b);
    }
    if (s.size() > 100) { out += std::format("...({} bytes)", s.size()); }
    return out;
}

using fastjson::prefix_recognizer;

[[nodiscard]] auto recognizes(std::string_view text) -> bool {
    prefix_recognizer r;
    return r.push(text).has_value() && r.complete();
}

[[nodiscard]] auto parses(std::string_view text) -> bool { return fastjson::parse(text).has_value(); }

/// The core claim, for one input: complete() iff parse() accepts.
auto agree(std::string_view text, std::string_view why) -> void {
    const bool p = parses(text);
    const bool r = recognizes(text);
    check(p == r, std::format("{}: parse {} but recognizer {} on \"{}\"", why, p ? "accepts" : "refuses",
                              r ? "accepts" : "refuses", shown(text)));
}

// ---- viability: finish an accepted prefix using expects() alone ---------------------------------------------

/// Appends bytes chosen only from expects() until the recognizer is complete. False if it gets stuck.
[[nodiscard]] auto finish(prefix_recognizer r, std::string& out) -> bool {
    const auto try_bytes = [&](std::string_view candidates) {
        for (const char c : candidates) {
            if (auto next = r.feed(std::string_view{&c, 1})) {
                r = *next;
                out.push_back(c);
                return true;
            }
        }
        return false;
    };
    for (int guard = 0; guard < 5000; ++guard) {
        if (r.complete()) { return true; }
        const auto x = r.expects();
        const std::string close(1, x.close);
        bool ok = false;
        switch (x.what) {
            case fastjson::prefix_expect::value: ok = try_bytes("0"); break;
            case fastjson::prefix_expect::value_or_close:
            case fastjson::prefix_expect::comma_or_close:
            case fastjson::prefix_expect::key_or_close: ok = try_bytes(close); break;
            case fastjson::prefix_expect::key:
            case fastjson::prefix_expect::string_body: ok = try_bytes("\""); break;
            case fastjson::prefix_expect::colon: ok = try_bytes(":"); break;
            case fastjson::prefix_expect::escape: ok = try_bytes("n"); break;
            case fastjson::prefix_expect::unicode_escape: ok = try_bytes("0DC"); break;
            case fastjson::prefix_expect::surrogate_pair: ok = try_bytes("\\u"); break;
            case fastjson::prefix_expect::utf8_continuation: ok = try_bytes("\x80\x90\xA0"); break;
            case fastjson::prefix_expect::number: ok = try_bytes(close + ".0"); break;
            case fastjson::prefix_expect::literal: ok = try_bytes("truefalsn"); break;
            case fastjson::prefix_expect::end: return r.complete();
        }
        if (!ok) { return false; }
    }
    return false;
}

auto viable(std::string_view prefix, std::string_view why) -> void {
    prefix_recognizer r;
    if (!r.push(prefix)) { return; }
    std::string completion;
    const bool finished = finish(r, completion);
    const std::string whole = std::string{prefix} + completion;
    check(finished && parses(whole), std::format("{}: accepted prefix \"{}\" could not be finished (\"{}\")", why,
                                                 shown(prefix), shown(completion)));
}

/// One push, byte-by-byte steps, and every two-chunk split must leave the same state (or refuse the same byte).
auto chunking(std::string_view text, std::string_view why) -> void {
    prefix_recognizer whole;
    const auto w = whole.push(text);
    prefix_recognizer bytes;
    std::optional<std::size_t> refused;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (!bytes.step(text[i])) {
            refused = i;
            break;
        }
    }
    check(w.has_value() == !refused.has_value() && (w.has_value() || w.error().offset == *refused),
          std::format("{}: push and step disagree on \"{}\"", why, shown(text)));
    if (w.has_value()) {
        check(whole == bytes && whole.complete() == bytes.complete(),
              std::format("{}: push and step reach different states on \"{}\"", why, shown(text)));
    }
    if (text.size() > 400) { return; }
    for (std::size_t cut = 0; cut <= text.size(); ++cut) {
        prefix_recognizer split;
        const auto a = split.push(text.substr(0, cut));
        if (!a) {
            check(!w.has_value() && a.error().offset == w.error().offset,
                  std::format("{}: split at {} refuses where one push does not", why, cut));
            continue;
        }
        const auto b = split.push(text.substr(cut));
        check(b.has_value() == w.has_value() && (b.has_value() ? split == whole : b.error().offset + cut == w.error().offset),
              std::format("{}: split at {} disagrees with one push on \"{}\"", why, cut, shown(text)));
    }
}

auto full(std::string_view text, std::string_view why) -> void {
    agree(text, why);
    chunking(text, why);
    viable(text, why);
}

// ---- agreement: the hand corpus -----------------------------------------------------------------------------

auto runCorpus() -> void {
    const std::vector<std::string> corpus = {
        // scalars and structure
        "0", "-0", "1", "-1", "12", "true", "false", "null", "\"\"", "\"a\"", "[]", "{}", " [ ] ", "\t{\n}\r\n",
        "[1,2,3]", "[[[]]]", R"({"a":1})", R"({"a":{"b":[1,{"c":null}]}})", R"({"a":1,"a":2})", R"( {"k" : "v" } )",
        "", "   ", "[", "[1", "[1,", "[1,]", "[,1]", "{", "{\"a\"", "{\"a\":", "{\"a\":}", "{\"a\" 1}", "{\"a\":1,}",
        "{,}", "{1:2}", "{\"a\":1 \"b\":2}", "tru", "nul", "fals", "truex", "nulll", "[]]", "{}}", "{}x", "1 2",
        "[1] [2]", "\"a\" \"b\"", "]", "}", ":", ",", "[}", "{]", "[:]", "{\"a\",1}", "[\"a\":1]", "{\"a\"::1}",
        std::string{"[1]\0", 4}, std::string{"\0[1]", 4}, "[true false]", "[1 2]", "{\"a\":1}{}", "TRUE", "Null",
        // strings
        R"("\uD83D\uDE00")", R"("\ud83d\ude00")", R"("a\uD834\uDD1Eb")", R"("\u00e9\u20AC\u0041")", R"("\u0000")",
        R"("\"\\\/\b\f\n\r\t")", "\"\xC3\xA9\xF0\x9F\x98\x80\"", "\"x\xE2\x82\xACy\\n\"", R"("\uD83D")",
        R"("\uDE00")", R"("\uD83Dx")", R"("\uD83D\u0041")", R"("\uD83D\uD83D")", R"("\u12")", R"("\u12G4")",
        R"("\x41")", "\"\\", "\"abc", std::string{"\"a\0b\"", 5}, "\"a\x1F" "b\"", "\"a\nb\"", "\"a\tb\"",
        R"("\uDBFF\uDFFF")", R"("\uD800\uDC00")", R"("\uDC00\uD800")", R"("\uFFFF")", R"("\uD7FF")", R"("\uE000")",
        R"("\uD83D\")", R"("\uD83D\\")", R"("\uD83D\u")", R"("\uD83D\uDE0")", R"("\uD83D\uDE0G")",
        "\"\xC3\x28\"", "\"\xFF\"", "\"\xC0\xAF\"", "\"\xE0\x80\xAF\"", "\"\xED\xA0\x80\"", "\"\xF4\x90\x80\x80\"",
        "\"\xE2\x82\"", "\"\x80\"", "\"\xF0\x9F\x98\"", "\"\xEF\xBF\xBF\"", "\"\xF4\x8F\xBF\xBF\"", "\"\xC2\x80\"",
        "\"\xDF\xBF\"", "\"\xE0\xA0\x80\"", "\"\xED\x9F\xBF\"", "\"\xF0\x90\x80\x80\"", "\"\xF5\x80\x80\x80\"",
        "\"\xC1\xBF\"", "{\"\xC3\xA9\":1}", "{\"\\u00e9\":1}", "{\"\xFF\":1}", "{\"a\\qb\":1}",
        "\"" + std::string(40, 'x') + "\xC3\xA9" + std::string(40, 'y') + "\"",
        "\"" + std::string(40, 'x') + "\xC3" + std::string(40, 'y') + "\"",
        "\"" + std::string(70, 'x') + "\\n" + std::string(70, 'y') + "\"",
        // numbers: grammar
        "NaN", "nan", "Infinity", "-Infinity", "inf", "+1", "01", "-", "1.", ".5", "1e", "1e+", "--1", "0x10", "1.2.3",
        "1e5e5", "-01", "1.5", "-2.25e3", "1E5", "1e-5", "1E+5", "0.0", "-0.0e-0", "0e0", "00", "-a", "1.e5", "1e.5",
        "[01]", "[-]", "[1.]", "[1e]", "[1e+]", "{\"a\":01}", "[0.5e+10,-1]",
        // numbers: range (number_from_text decides for both)
        "1e5000", "-1e5000", "[1e99999]", "{\"a\":-2e5000}", "123456789012345678901234567890e5000", "1e308",
        "1.7976931348623157e308", "1.8e308", "1e4932", "1e4933", "1.18973e4932", "1.19e4932", "1e-5000", "1e-99999",
        "0e99999", "-0e99999", "9007199254740993", "340282366920938463463374607431768211455",
        "340282366920938463463374607431768211456", "-170141183460469231731687303715884105728",
        "-170141183460469231731687303715884105729", "1427247692705959881058285969449495136382746624",
        "1427247692705959881058285969449495136382746625", "100000000000000000000000000000000000000000000000000",
        "[100000000000000000000000000000000000000000000000000]",
        "[100000000000000000000000000000000000000000000000000.0]", "0.1234567890123456789",
        std::string(99, '1'), std::string(100, '1'), std::string(101, '1'), std::string(98, '1') + ".5",
        std::string(98, '1') + ".", "0." + std::string(98, '1'), "0." + std::string(99, '1'),
        "1e" + std::string(98, '0'), "1e" + std::string(99, '0'), "1." + std::string(95, '0') + "e1",
        std::string(97, '1') + "e-1", std::string(98, '1') + "e-", std::string(63, '1') + ".0",
        std::string(64, '1') + ".0", std::string(62, '1'), "-" + std::string(62, '1'),
        // depth
        std::string(1000, '[') + std::string(1000, ']'), std::string(1001, '[') + std::string(1001, ']'),
        std::string(999, '[') + "1" + std::string(999, ']'), std::string(1000, '[') + "1" + std::string(1000, ']'),
        std::string(999, '[') + "{}" + std::string(999, ']'), std::string(999, '[') + "{\"a\":1}" + std::string(999, ']'),
    };
    for (const auto& text : corpus) { full(text, "corpus"); }

    // Every prefix of every valid document.
    for (const std::string doc : {std::string{R"({"name":"x\u00e9\uD83D\uDE00","n":[1,-2.5e3,true,false,null,{"k":"v\n"}],"big":12345678901234567890})"},
                                  std::string{"[\"\xC3\xA9\xF0\x9F\x98\x80\", 1e-7, {\"\": []}, \"\\u0041\\/\"]"},
                                  std::string{" { \"a\" : [ 0 , -0.5 , 1E+2 ] , \"b\" : { } } "}}) {
        check(parses(doc), "the prefix fixture parses");
        for (std::size_t len = 0; len <= doc.size(); ++len) { full(std::string_view{doc}.substr(0, len), "prefix"); }
    }
}

// ---- agreement: random documents and mutations --------------------------------------------------------------

class Generator {
public:
    explicit Generator(std::uint64_t seed) : rng_(seed) {}

    auto document() -> std::string {
        std::string out = ws();
        value(0, out);
        out += ws();
        return out;
    }

    auto mutate(std::string s) -> std::string {
        // sizeof, not strlen: the set carries a NUL byte.
        static constexpr char kRaw[] = "{}[]:,\"\\ \t\n0123456789.eE+-truefalsn/bu\x80\xBF\xC3\xA9\xE2\xF0\x9F\xED\xA0\xFF\x1F\x00x";
        static constexpr std::string_view kBytes{kRaw, sizeof kRaw - 1};
        const int edits = pick(1, 3);
        for (int e = 0; e < edits; ++e) {
            const std::size_t at = s.empty() ? 0 : static_cast<std::size_t>(pick(0, static_cast<int>(s.size()) - 1));
            const char b = kBytes[static_cast<std::size_t>(pick(0, static_cast<int>(kBytes.size()) - 1))];
            switch (pick(0, 3)) {
                case 0: if (!s.empty()) { s.erase(at, 1); } break;
                case 1: s.insert(at, 1, b); break;
                case 2: if (!s.empty()) { s[at] = b; } break;
                default: s.resize(at); break;
            }
        }
        return s;
    }

    auto pick(int lo, int hi) -> int { return std::uniform_int_distribution<int>{lo, hi}(rng_); }

private:
    auto ws() -> std::string {
        static constexpr std::string_view kWs = " \t\n\r";
        std::string out;
        for (int n = pick(-2, 2); n > 0; --n) { out += kWs[static_cast<std::size_t>(pick(0, 3))]; }
        return out;
    }

    auto digits(int n, bool first_nonzero) -> std::string {
        std::string out;
        for (int i = 0; i < n; ++i) { out += static_cast<char>('0' + pick(i == 0 && first_nonzero ? 1 : 0, 9)); }
        return out;
    }

    auto number() -> std::string {
        std::string out = pick(0, 3) == 0 ? "-" : "";
        const int long_mode = pick(0, 9);
        const int int_len = long_mode == 0 ? pick(30, 102) : pick(1, 18);
        out += pick(0, 5) == 0 ? "0" : digits(int_len, true);
        if (pick(0, 2) == 0) { out += "." + digits(long_mode == 1 ? pick(30, 100) : pick(1, 20), false); }
        if (pick(0, 3) == 0) {
            out += pick(0, 1) == 0 ? "e" : "E";
            const int sign = pick(0, 2);
            out += sign == 0 ? "" : (sign == 1 ? "+" : "-");
            static constexpr std::array<std::string_view, 6> kExps{"308", "309", "4931", "4932", "4933", "99999"};
            out += pick(0, 3) == 0 ? std::string{kExps[static_cast<std::size_t>(pick(0, 5))]} : digits(pick(1, 3), false);
        }
        return out;
    }

    auto string() -> std::string {
        static constexpr std::array<std::string_view, 16> kPieces{
            "a",  "Z",          " ",           "\\n",          "\\\"",         "\\\\",     "\\/",   "\\u00e9",
            "\\uD83D\\uDE00", "\xC3\xA9", "\xE4\xB8\xAD", "\xF0\x9F\x98\x80", "\\u0000", "\\t",  "key", "\\uFFFF"};
        std::string out = "\"";
        const int n = pick(0, 3) == 0 ? pick(10, 60) : pick(0, 6);
        for (int i = 0; i < n; ++i) { out += kPieces[static_cast<std::size_t>(pick(0, 15))]; }
        return out + "\"";
    }

    auto value(int depth, std::string& out) -> void {
        const int kind = depth > 6 ? pick(0, 3) : pick(0, 6);
        switch (kind) {
            case 0: out += number(); break;
            case 1: out += string(); break;
            case 2: {
                static constexpr std::array<std::string_view, 3> kLit{"true", "false", "null"};
                out += kLit[static_cast<std::size_t>(pick(0, 2))];
                break;
            }
            case 3: out += string(); break;
            case 4:
            case 5: {
                out += "[" + ws();
                for (int i = 0, n = pick(0, 4); i < n; ++i) {
                    if (i > 0) { out += ws() + "," + ws(); }
                    value(depth + 1, out);
                }
                out += ws() + "]";
                break;
            }
            default: {
                out += "{" + ws();
                for (int i = 0, n = pick(0, 4); i < n; ++i) {
                    if (i > 0) { out += ws() + "," + ws(); }
                    out += string() + ws() + ":" + ws();
                    value(depth + 1, out);
                }
                out += ws() + "}";
                break;
            }
        }
    }

    std::mt19937_64 rng_;
};

auto runRandom() -> void {
    Generator g{0x5EED2026ULL};
    int valid = 0;
    int mutants = 0;
    int mutant_valid = 0;
    for (int i = 0; i < 20000; ++i) {
        const std::string doc = g.document();
        valid += parses(doc) ? 1 : 0;
        agree(doc, "random document");
        if (i % 10 == 0) { chunking(doc, "random document"); }
        viable(doc.substr(0, static_cast<std::size_t>(g.pick(0, static_cast<int>(doc.size())))), "random prefix");
        for (int m = 0; m < 5; ++m) {
            const std::string bad = g.mutate(doc);
            ++mutants;
            mutant_valid += parses(bad) ? 1 : 0;
            agree(bad, "random mutation");
            viable(bad, "random mutation");
            if (m == 0 && i % 10 == 0) { chunking(bad, "random mutation"); }
        }
    }
    std::println("random: 20000 documents ({} parse), {} mutations ({} parse)", valid, mutants, mutant_valid);
}

// ---- events -------------------------------------------------------------------------------------------------

auto runEvents() -> void {
    using E = fastjson::prefix_event;
    prefix_recognizer r;
    const std::string_view doc = R"({"a":[1,"x",true],"b":null})";
    std::vector<E> got;
    for (const char c : doc) {
        const auto ev = r.step(c);
        check(ev.has_value(), "events fixture steps");
        got.push_back(ev.value_or(E::none));
    }
    check(r.complete(), "events fixture completes");
    const auto has = [&](std::size_t i, E e) { return i < got.size() && fastjson::has_event(got[i], e); };
    check(has(0, E::object_begin), "'{' begins an object");
    check(has(1, E::key_begin) && has(2, E::content) && has(3, E::key_end), "a member name: begin, content, end");
    check(has(4, E::name_separator), "':'");
    check(has(5, E::array_begin), "'['");
    check(has(6, E::number_begin) && has(6, E::content), "a number's first byte");
    check(has(7, E::number_end) && has(7, E::value_separator), "',' ends the number and separates");
    check(has(8, E::string_begin) && has(9, E::content) && has(10, E::string_end), "a string value");
    check(has(12, E::true_begin) && has(15, E::literal_end), "true");
    check(has(16, E::array_end) && !has(16, E::number_end), "']' after a literal closes only the array");
    check(has(22, E::null_begin) && has(26, E::object_end), "null, then '}'");

    // A refused byte changes nothing.
    prefix_recognizer s;
    check(s.push("[1,").has_value(), "[1, accepted");
    const auto before = s;
    const auto bad = s.push("]");
    check(!bad.has_value() && bad.error().code == fastjson::json_error_code::invalid_syntax && s == before,
          "a trailing comma's ']' is refused and the state is unchanged");
    check(s.expects().what == fastjson::prefix_expect::value, "after ',' in an array a value is due");

    prefix_recognizer n;
    check(n.push("[12.5e").has_value() && n.number().text == "12.5e" && n.number().int_digits == 2 &&
              n.number().frac_digits == 1 && !n.expects().may_end,
          "number(): text and digit counts mid-exponent");
    check(n.push("3").has_value() && n.expects().may_end && n.number().exp_digits == 1, "an exponent digit ends it");

    prefix_recognizer obj{fastjson::prefix_root::object};
    check(!obj.feed("[").has_value() && !obj.feed("1").has_value() && obj.feed(" {").has_value(),
          "prefix_root::object admits only an object at the top");
    check(!obj.started() && obj.feed(" ").has_value() && !obj.feed(" ")->started() && obj.feed("{")->started(),
          "started() turns true with the first non-whitespace byte");

    // state_key: the same continuations from two routes into one state.
    prefix_recognizer k1;
    prefix_recognizer k2;
    check(k1.push(R"({"a":"\u00e9xyz)").has_value() && k2.push(R"({"bb":"q)").has_value() && k1 == k2,
          "two string bodies at the same nesting are one state");
}

// ---- smt: the structural table as obligations ---------------------------------------------------------------

[[nodiscard]] auto kindOf(fastjson::prefix_structural s) -> int {
    using S = fastjson::prefix_structural;
    switch (s) {
        case S::top_value:
        case S::top_end: return 0;
        case S::array_first:
        case S::array_value:
        case S::array_next: return 1;
        default: return 2;
    }
}

/// The table as a RELATION over bounded integers, one disjunct per cell:
/// (or (and (= sv s) (= cv c) (= av action) (= nv next)) ...). Every variable is bounded by its own rows, so the
/// script is finite-domain and `sen reason smt` decides it by SAT with a checked certificate. `fixed_class`
/// restricts the relation to one byte class (cv is then not mentioned).
[[nodiscard]] auto tableRel(std::string_view sv, std::string_view cv, std::string_view av, std::string_view nv,
                            std::optional<std::size_t> fixed_class = std::nullopt) -> std::string {
    const auto& t = fastjson::prefix_structural_table;
    std::string out = "(or";
    for (std::size_t s = 0; s < fastjson::prefix_structural_count; ++s) {
        for (std::size_t c = 0; c < fastjson::prefix_class_count; ++c) {
            if (fixed_class && *fixed_class != c) { continue; }
            const std::string class_eq = fixed_class ? std::string{} : std::format(" (= {} {})", cv, c);
            out += std::format("\n  (and (= {} {}){} (= {} {}) (= {} {}))", sv, s, class_eq, av,
                               static_cast<int>(t[s][c].action), nv, static_cast<int>(t[s][c].next));
        }
    }
    return out + ")";
}

/// The container kind of each state (0 top level, 1 array, 2 object), as a relation.
[[nodiscard]] auto kindRel(std::string_view sv, std::string_view kv) -> std::string {
    std::string out = "(or";
    for (std::size_t s = 0; s < fastjson::prefix_structural_count; ++s) {
        out += std::format(" (and (= {} {}) (= {} {}))", sv, s, kv, kindOf(static_cast<fastjson::prefix_structural>(s)));
    }
    return out + ")";
}

[[nodiscard]] auto declareBounded(std::string_view v, std::size_t hi) -> std::string {
    return std::format("(declare-fun {} () Int)\n(assert (and (>= {} 0) (<= {} {})))\n", v, v, v, hi);
}

/// One script per property. Each asserts the property's NEGATION over the real table; UNSAT proves it.
[[nodiscard]] auto smtScripts() -> std::vector<std::pair<std::string, std::string>> {
    using A = fastjson::prefix_action;
    using S = fastjson::prefix_structural;
    using C = fastjson::prefix_class;
    const auto num = [](auto e) { return static_cast<int>(e); };
    constexpr std::size_t kStates = fastjson::prefix_structural_count - 1;
    constexpr std::size_t kClasses = fastjson::prefix_class_count - 1;
    const std::size_t kActions = static_cast<std::size_t>(A::value_separator);
    const std::size_t kNext = static_cast<std::size_t>(S::after_value);
    const std::string head = std::format(
        "; GENERATED by test_prefix_recognizer --emit-smt from fastjson::prefix_structural_table. Do not edit.\n"
        "; s: structural state 0..{}, c: byte class 0..{}, act: prefix_action, nxt: next state ({} = after_value),\n"
        "; k: the container kind of s (0 top level, 1 array, 2 object). UNSAT proves the PROPERTY below.\n"
        "{}{}{}{}{}(assert {})\n(assert {})\n",
        kStates, kClasses, kNext, declareBounded("s", kStates), declareBounded("c", kClasses),
        declareBounded("act", kActions), declareBounded("nxt", kNext), declareBounded("k", 2),
        tableRel("s", "c", "act", "nxt"), kindRel("s", "k"));
    std::vector<std::pair<std::string, std::string>> out;
    const auto add = [&](std::string name, std::string what, std::string body) {
        out.emplace_back(std::move(name), head + "; PROPERTY: " + what + "\n" + body + "(check-sat)\n");
    };
    add("close_array_needs_array", "']' is accepted only in a state of an array",
        std::format("(assert (= act {}))\n(assert (not (= k 1)))\n", num(A::close_array)));
    add("close_object_needs_object", "'}' is accepted only in a state of an object",
        std::format("(assert (= act {}))\n(assert (not (= k 2)))\n", num(A::close_object)));
    add("container_kind_invariant",
        "every transition keeps 'the state's container kind is the kind on top of the stack': a push enters the "
        "pushed kind, a pop or a token's end resolves after_value against the stack, everything else stays in kind",
        declareBounded("nk", 2) +
            std::format("(assert (not (= act {})))\n(assert (not (= nxt {})))\n(assert {})\n"
                        "(assert (or (and (= act {}) (not (= nk 2))) (and (= act {}) (not (= nk 1)))\n"
                        "            (and (not (= act {})) (not (= act {})) (not (= nk k)))))\n",
                        num(A::reject), num(S::after_value), kindRel("nxt", "nk"), num(A::open_object),
                        num(A::open_array), num(A::open_object), num(A::open_array)));
    add("pops_resolve_against_stack", "a close and a scalar token always go to after_value (the stack decides)",
        std::format("(assert (or (= act {}) (= act {}) (= act {}) (= act {}) (= act {})))\n(assert (not (= nxt {})))\n",
                    num(A::close_object), num(A::close_array), num(A::begin_string), num(A::begin_number),
                    num(A::begin_literal), num(S::after_value)));
    add("no_trailing_comma", "the state a ',' enters accepts no close",
        declareBounded("s0", kStates) + declareBounded("c0", kClasses) + declareBounded("act0", kActions) +
            declareBounded("nxt0", kNext) +
            std::format("(assert {})\n(assert (= act0 {}))\n(assert (= s nxt0))\n(assert (or (= act {}) (= act {})))\n",
                        tableRel("s0", "c0", "act0", "nxt0"), num(A::value_separator), num(A::close_object),
                        num(A::close_array)));
    add("colon_only_after_member_name", "':' is accepted only right after a member name",
        std::format("(assert (= c {}))\n(assert (not (= act {})))\n(assert (not (= s {})))\n", num(C::colon),
                    num(A::reject), num(S::object_colon)));
    add("comma_only_after_value", "',' is accepted only after an element or a member value",
        std::format("(assert (= c {}))\n(assert (not (= act {})))\n(assert (not (or (= s {}) (= s {}))))\n",
                    num(C::comma), num(A::reject), num(S::array_next), num(S::object_next)));
    add("only_whitespace_after_document", "after the top-level value only whitespace is accepted",
        std::format("(assert (= s {}))\n(assert (not (= act {})))\n(assert (not (= c {})))\n", num(S::top_end),
                    num(A::reject), num(C::whitespace)));
    add("values_only_where_due", "a value begins only at the top, after '[', after ',' in an array or after ':'",
        std::format("(assert (or (= act {}) (= act {}) (= act {}) (= act {}) (= act {})))\n"
                    "(assert (not (or (= s {}) (= s {}) (= s {}) (= s {}))))\n",
                    num(A::open_object), num(A::open_array), num(A::begin_string), num(A::begin_number),
                    num(A::begin_literal), num(S::top_value), num(S::array_first), num(S::array_value),
                    num(S::object_value)));
    add("keys_only_in_objects", "a member name begins only after '{' or after ',' in an object",
        std::format("(assert (= act {}))\n(assert (not (or (= s {}) (= s {}))))\n", num(A::begin_key),
                    num(S::object_first), num(S::object_key)));
    {
        // No dead state: every state other than top_end has a byte that does more than skip whitespace. One
        // action variable per byte class, all read from state s; all of them reject-or-skip is the negation.
        std::string body = std::format("(assert (not (= s {})))\n", num(S::top_end));
        for (std::size_t cc = 0; cc < fastjson::prefix_class_count; ++cc) {
            const std::string a = std::format("a{}", cc);
            const std::string n = std::format("n{}", cc);
            body += declareBounded(a, kActions) + declareBounded(n, kNext) +
                    std::format("(assert {})\n(assert (or (= {} {}) (= {} {})))\n", tableRel("s", "", a, n, cc), a,
                                num(A::reject), a, num(A::skip));
        }
        add("no_dead_state", "every state but top_end accepts a byte other than whitespace", std::move(body));
    }
    return out;
}

auto runSmt(std::optional<std::filesystem::path> committed) -> void {
    const auto scripts = smtScripts();
    check(scripts.size() == 11, "eleven structural obligations");
    if (!committed) { return; }
    for (const auto& [name, text] : scripts) {
        std::ifstream in{*committed / (name + ".smt2"), std::ios::binary};
        const std::string on_disk{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
        check(on_disk == text, std::format("tests/proofs/prefix_structural/{}.smt2 is what the table generates "
                                           "(rerun with --emit-smt and re-prove)", name));
    }
}

auto emitSmt(const std::filesystem::path& dir) -> int {
    std::filesystem::create_directories(dir);
    for (const auto& [name, text] : smtScripts()) {
        std::ofstream out{dir / (name + ".smt2"), std::ios::binary};
        out << text;
    }
    std::println("wrote {} scripts to {}", smtScripts().size(), dir.string());
    return 0;
}

// ---- concurrency --------------------------------------------------------------------------------------------

auto runConcurrent() -> void {
    prefix_recognizer committed;
    check(committed.push(R"({"items":[{"id":1,"tags":["a")").has_value(), "concurrency fixture");
    const std::array<std::string_view, 8> candidates{",", "]", "}", "\"b\"", ",\"c\"]", "1", ":", "]}]}"};
    std::array<int, 8> want{};
    for (std::size_t i = 0; i < candidates.size(); ++i) { want[i] = committed.feed(candidates[i]).has_value() ? 1 : 0; }
    std::atomic<int> mismatches{0};
    {
        std::vector<std::jthread> threads;
        for (int t = 0; t < 8; ++t) {
            threads.emplace_back([&] {
                for (int iter = 0; iter < 2000; ++iter) {
                    for (std::size_t i = 0; i < candidates.size(); ++i) {
                        if ((committed.feed(candidates[i]).has_value() ? 1 : 0) != want[i]) { ++mismatches; }
                    }
                }
            });
        }
    }
    check(mismatches.load() == 0, "8 threads trying candidates from one shared state agree with one thread");
}

// ---- speed --------------------------------------------------------------------------------------------------

template <typename T>
auto keep(T& v) -> void {
    asm volatile("" : : "r"(&v) : "memory");
}

auto runSpeed() -> void {
    using clock = std::chrono::steady_clock;
    const auto ns_per = [](clock::duration d, std::size_t n) {
        return std::chrono::duration<double, std::nano>(d).count() / static_cast<double>(n);
    };
    for (const std::size_t depth : {1UZ, 16UZ, 256UZ, 1000UZ}) {
        prefix_recognizer base;
        check(base.push(std::string(depth, '[')).has_value(), "speed fixture");
        constexpr std::size_t kCopies = 2'000'000;
        const auto t0 = clock::now();
        for (std::size_t i = 0; i < kCopies; ++i) {
            prefix_recognizer copy = base;
            keep(copy);
        }
        std::println("speed: state copy at depth {:4}: {:.2f} ns ({} bytes)", depth, ns_per(clock::now() - t0, kCopies),
                     sizeof(prefix_recognizer));
    }
    prefix_recognizer mid;
    check(mid.push(R"({"items":[{"id":12,"name":"wid)").has_value(), "speed fixture (mid string)");
    const std::array<std::string_view, 8> tokens{"get", "\",\"", " the", "\\n", "\xC3\xA9", "\"}", "]", "\"}]}"};
    constexpr std::size_t kTrials = 1'000'000;
    std::size_t accepted = 0;
    const auto t1 = clock::now();
    for (std::size_t i = 0; i < kTrials; ++i) {
        auto r = mid.feed(tokens[i & 7U]);
        accepted += r.has_value() ? 1 : 0;
        keep(r);
    }
    std::println("speed: candidate feed (copy + push of a 1-4 byte token): {:.2f} ns ({} of {} accepted)",
                 ns_per(clock::now() - t1, kTrials), accepted, kTrials);
    Generator g{7};
    std::string big = "[";
    while (big.size() < (8U << 20U)) {
        std::string doc = g.document();
        if (!parses(doc)) { continue; }  // one refused number would end both runs early
        if (big.size() > 1) { big += ","; }
        big += doc;
    }
    big += "]";
    const auto t2 = clock::now();
    prefix_recognizer whole;
    const bool ok = whole.push(big).has_value() && whole.complete();
    const auto push_ns = ns_per(clock::now() - t2, 1);
    const auto t3 = clock::now();
    const bool parsed_ok = parses(big);
    const auto parse_ns = ns_per(clock::now() - t3, 1);
    check(ok && parsed_ok, "speed corpus: the whole document is recognized and parsed");
    std::println("speed: push of {:.1f} MiB: {:.0f} MB/s (parse(): {:.0f} MB/s), tier {}",
                 static_cast<double>(big.size()) / (1U << 20U), static_cast<double>(big.size()) * 1e3 / push_ns,
                 static_cast<double>(big.size()) * 1e3 / parse_ns, fastjson::string_scan_tier());
}

}  // namespace

auto main(int argc, char** argv) -> int {
    if (argc > 2 && std::string_view{argv[1]} == "--emit-smt") { return emitSmt(argv[2]); }
    const std::string_view tier = argc > 1 ? std::string_view{argv[1]} : std::string_view{"default"};
    const std::optional<std::filesystem::path> committed =
        argc > 2 ? std::optional<std::filesystem::path>{argv[2]} : std::nullopt;
    runCorpus();
    runRandom();
    runEvents();
    runSmt(committed);
    runConcurrent();
    if (std::getenv("FASTJSON_PREFIX_NO_SPEED") == nullptr) { runSpeed(); }
    std::println("test_prefix_recognizer [{} / scan tier {}]: {} checks, {} failed", tier, fastjson::string_scan_tier(),
                 g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
