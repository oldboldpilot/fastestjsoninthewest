/**
 * test_member_index -- the top-level member index (fastjson::index_members / index_elements) against parse().
 *
 * Run once per string-scan tier (the tier is chosen by FASTJSON_SIMD, set by the registering build; argv[1] only names it).
 *
 * Sections:
 *   spans       -- keys, key offsets and value spans of hand-written documents, nested values and every value kind,
 *                  members in input order, duplicates kept, an escaped key kept as written and decoded by unescaped_key.
 *   stops       -- every way the walk can stop, with the byte it stops at and the members read before it.
 *   leaves      -- the content of a leaf is the reader's business: `null-1`, `007`, `NULL`, a raw control byte and invalid
 *                  UTF-8 do not stop the index, and parse() of the span refuses each of them.
 *   agreement   -- on random documents the span of every member parses to the value parse() gives that member; every
 *                  prefix of a valid document is incomplete; a document parse() accepts is complete in the index.
 *   strings     -- escaped quotes and backslashes at every offset around the SIMD block edges.
 *   elements    -- index_elements on arrays: spans, empty, nested.
 *   depth       -- the depth limit, and that a document at the limit is indexed.
 *   reuse       -- one index reused across documents carries nothing over.
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

constexpr auto npos = std::string_view::npos;

/// The span text of member `i`, or "<none>".
[[nodiscard]] auto spanOf(const fastjson::member_index& index, std::string_view input, std::size_t i) -> std::string {
    if (i >= index.members().size()) { return "<none>"; }
    const auto text = index.members()[i].value_in(input);
    return text.empty() ? std::string{"<open>"} : std::string{text};
}

auto testSpans() -> void {
    {
        const std::string_view doc = R"( {"model" : "qwen" , "max_tokens":  42,"nested":{"a":[1,{"b":"}"}],"c":null}, "list":[ ] ,"t":true } )";
        const auto index = fastjson::index_members(doc);
        check(index.opened() && index.complete() && index.stopped_at() == npos, "spans: a well-formed object is complete");
        const auto m = index.members();
        check(m.size() == 5, std::format("spans: five members (got {})", m.size()));
        const std::vector<std::pair<std::string, std::string>> want{
            {"model", R"("qwen")"},
            {"max_tokens", "42"},
            {"nested", R"({"a":[1,{"b":"}"}],"c":null})"},
            {"list", "[ ]"},
            {"t", "true"},
        };
        check(m.size() == 5 && m[0].key == "model" && m[1].key == "max_tokens" && m[2].key == "nested" && m[3].key == "list" && m[4].key == "t" ,
              "spans: keys in input order");
        check(spanOf(index, doc, 0) == want[0].second, std::format("spans: model value is {} (got {})", want[0].second, spanOf(index, doc, 0)));
        check(spanOf(index, doc, 1) == want[1].second, std::format("spans: max_tokens value (got {})", spanOf(index, doc, 1)));
        check(spanOf(index, doc, 2) == want[2].second, std::format("spans: nested value (got {})", spanOf(index, doc, 2)));
        check(spanOf(index, doc, 3) == want[3].second, std::format("spans: list value (got {})", spanOf(index, doc, 3)));
        check(spanOf(index, doc, 4) == want[4].second, std::format("spans: t value (got {})", spanOf(index, doc, 4)));
        // key_begin is the key's opening quote.
        check(m.size() > 0 && doc[m[0].key_begin] == '"' && doc.substr(m[0].key_begin, 7) == "\"model\"", "spans: key_begin is the opening quote");
        // A key nested one level down is not a top-level member.
        check(index.find("a") == nullptr && index.find("b") == nullptr && index.find("c") == nullptr, "spans: nested keys are not members");
        // Every span parses on its own.
        for (const auto& member : m) {
            check(fastjson::parse(member.value_in(doc)).has_value(), std::format("spans: the span of {} parses", member.key));
        }
    }
    {
        const std::string_view doc = R"({"a":1,"b":2,"a":3,"a" :4})";
        const auto index = fastjson::index_members(doc);
        check(index.complete() && index.members().size() == 4 && index.count("a") == 3 && index.count("b") == 1,
              "spans: duplicates are kept, in order");
        check(spanOf(index, doc, 0) == "1" && spanOf(index, doc, 2) == "3" && spanOf(index, doc, 3) == "4", "spans: each duplicate has its own span");
        check(index.find("a") == &index.members()[0], "spans: find returns the first duplicate");
    }
    {
        const std::string_view doc = R"({"max\u005ftokens":5,"Models":1,"s\"q":2})";
        const auto index = fastjson::index_members(doc);
        check(index.complete() && index.any_key_escaped() && index.members().size() == 3, "spans: escaped keys are indexed");
        check(index.members()[0].key == R"(max\u005ftokens)" && index.members()[0].key_escaped, "spans: an escaped key is kept as written");
        const auto decoded = index.members()[0].unescaped_key();
        check(decoded.has_value() && *decoded == "max_tokens", "spans: unescaped_key decodes \\u005f");
        check(index.members()[1].key == "Models" && !index.members()[1].key_escaped, "spans: case is kept");
        const auto quoted = index.members()[2].unescaped_key();
        check(quoted.has_value() && *quoted == "s\"q", "spans: unescaped_key decodes an escaped quote");
        check(index.count("max_tokens") == 0 && index.count(R"(max\u005ftokens)") == 1, "spans: count is by the key as written");
    }
    {
        const auto empty = fastjson::index_members("  { \t\n}\r\n ");
        check(empty.opened() && empty.complete() && empty.members().empty(), "spans: an empty object is complete with no members");
        for (const std::string_view doc : {"", "   ", "[1]", "\"x\"", "7", "null", "}"}) {
            const auto none = fastjson::index_members(doc);
            check(!none.opened() && !none.complete() && none.members().empty(), std::format("spans: {} is not an object", shown(doc)));
        }
    }
    {
        const std::string_view doc = "{\"a\":\"x\",\"b\":-1.5e3,\"c\":false}";
        const auto index = fastjson::index_members(doc);
        check(spanOf(index, doc, 0) == "\"x\"" && spanOf(index, doc, 1) == "-1.5e3" && spanOf(index, doc, 2) == "false",
              "spans: a string, a number and a literal end exactly at their last byte");
    }
}

struct StopCase {
    std::string_view doc;
    std::size_t at;           // stopped_at
    std::size_t members;      // members read before the stop
    std::string_view why;
};

auto testStops() -> void {
    const std::vector<StopCase> cases{
        {R"({"a":1)", 6, 1, "ran out after a value"},
        {R"({"a")", 4, 0, "a key and then nothing"},
        {R"({"a" 1})", 5, 0, "a key with no colon"},
        {R"({"a":})", 5, 1, "a delimiter where a value belongs"},
        {R"({"a":1,})", 7, 1, "a trailing comma"},
        {R"({"a":1 "b":2})", 7, 1, "no comma between members"},
        {R"({"a":[1,]})", 8, 1, "a trailing comma in a nested list"},
        {R"({"a":[1}})", 7, 1, "a list closed by a brace"},
        {R"({"a":{"b":1]})", 11, 1, "an object closed by a bracket"},
        {R"({"a":"x)", 5, 1, "an unterminated string value"},
        {R"({"a)", 1, 0, "an unterminated key"},
        {R"({a:1})", 1, 0, "a key that is not a string"},
        {R"({"a":1}})", 7, 1, "a second closing brace"},
        {R"({"a":1} x)", 8, 1, "text after the object"},
        {R"({"a":1}{})", 7, 1, "a second object"},
        {R"({"a":tru"e"})", 8, 1, "a token that runs into a string"},
        {R"({"a":1:2})", 6, 1, "a colon after a value"},
        {R"({"a":[}})", 6, 1, "a closer where a list's first value belongs"},
        {R"({"a":1,,"b":2})", 7, 1, "two commas"},
        {R"({,"a":1})", 1, 0, "a comma first"},
        {"{\"a\":\"\\", 5, 1, "a string ending in a lone backslash"},
        {R"({"a":"x\"})", 5, 1, "an escaped quote leaves the string open"},
    };
    for (const auto& c : cases) {
        const auto index = fastjson::index_members(c.doc);
        check(index.opened() && !index.complete(), std::format("stops: {:?} ({}) is opened and not complete", c.doc, c.why));
        check(index.stopped_at() == c.at, std::format("stops: {:?} ({}) stops at {} (got {})", c.doc, c.why, c.at, index.stopped_at() == npos ? -1 : static_cast<int>(index.stopped_at())));
        check(index.members().size() == c.members, std::format("stops: {:?} ({}) keeps {} member(s) (got {})", c.doc, c.why, c.members, index.members().size()));
        // Every one of these is refused by parse() too: the index never accepts what the grammar's structure rejects.
        check(!fastjson::parse(c.doc).has_value(), std::format("stops: parse() refuses {:?} as well", c.doc));
    }
    {
        // An open value has no span.
        const std::string_view doc = R"({"a":1,"b":[1,2)";
        const auto index = fastjson::index_members(doc);
        check(index.members().size() == 2 && spanOf(index, doc, 0) == "1" && spanOf(index, doc, 1) == "<open>", "stops: a value the walk never closed has no span");
    }
}

auto testLeaves() -> void {
    // The content of a leaf is not the index's business; the reader that parses the span refuses it.
    const std::vector<std::string> leaves{"null-1", "NULL", "nullx", "007", "1e", "+1", ".5", "0x10", "tru", "-", "1.", "Infinity", "NaN"};
    for (const auto& leaf : leaves) {
        const std::string doc = std::format(R"({{"a":{},"b":2}})", leaf);
        const auto index = fastjson::index_members(doc);
        check(index.complete() && index.members().size() == 2 && spanOf(index, doc, 0) == leaf && spanOf(index, doc, 1) == "2",
              std::format("leaves: the token {} is delimited and the walk goes on", leaf));
        check(!fastjson::parse(doc).has_value(), std::format("leaves: parse() refuses the document with {}", leaf));
        check(!fastjson::parse(leaf).has_value(), std::format("leaves: parse() of the span {} is the refusal the reader sees", leaf));
    }
    {
        const std::string doc = std::string{"{\"a\":\"x\x01y\",\"b\":\"\xff\xfe\",\"c\":1}"};
        const auto index = fastjson::index_members(doc);
        check(index.complete() && index.members().size() == 3, "leaves: a raw control byte and invalid UTF-8 inside strings do not stop the index");
        check(!fastjson::parse(index.members()[0].value_in(doc)).has_value() && !fastjson::parse(index.members()[1].value_in(doc)).has_value(),
              "leaves: parse() of those string spans refuses them");
        check(fastjson::parse(index.members()[2].value_in(doc)).has_value(), "leaves: and accepts the clean member");
    }
    {
        const std::string_view doc = "{\"a\":\"\\q\",\"b\":1}";
        const auto index = fastjson::index_members(doc);
        check(index.complete() && index.members().size() == 2, "leaves: a bad escape is the reader's refusal, not the index's");
        check(!fastjson::parse(index.members()[0].value_in(doc)).has_value(), "leaves: parse() refuses \\q");
    }
}

/// A random JSON value of bounded depth, written with random whitespace.
struct Gen {
    std::mt19937_64 rng;
    explicit Gen(std::uint64_t seed) : rng(seed) {}
    auto pick(std::size_t n) -> std::size_t { return static_cast<std::size_t>(rng() % n); }
    auto ws() -> std::string {
        static constexpr std::string_view w[] = {"", "", "", " ", "  ", "\n", "\t", "\r\n ", " \n\t "};
        return std::string{w[pick(std::size(w))]};
    }
    auto str() -> std::string {
        static constexpr std::string_view parts[] = {"a", "model", "x y", "\\\"", "\\\\", "\\n", "\\u00e9", "\xc3\xa9", "}", "]", "{", "[", ",", ":", "\\/", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"};
        std::string out = "\"";
        const auto count = pick(6);
        for (std::size_t i = 0; i < count; ++i) { out += parts[pick(std::size(parts))]; }
        return out + "\"";
    }
    auto scalar() -> std::string {
        static constexpr std::string_view s[] = {"null", "true", "false", "0", "-0", "7", "-12", "3.25", "1e5", "-2.5E-3", "123456789012345678901234567890"};
        return pick(3) == 0 ? str() : std::string{s[pick(std::size(s))]};
    }
    auto value(int depth) -> std::string {
        if (depth <= 0 || pick(3) == 0) { return scalar(); }
        const bool object = pick(2) == 0;
        const auto count = pick(5);
        std::string out = object ? "{" : "[";
        out += ws();
        for (std::size_t i = 0; i < count; ++i) {
            if (i != 0) { out += ws() + "," + ws(); }
            if (object) { out += str() + ws() + ":" + ws(); }
            out += value(depth - 1);
        }
        out += ws();
        out += object ? "}" : "]";
        return out;
    }
    auto document() -> std::string {
        const auto count = pick(8);
        std::string out = ws() + "{" + ws();
        for (std::size_t i = 0; i < count; ++i) {
            if (i != 0) { out += ws() + "," + ws(); }
            out += str() + ws() + ":" + ws() + value(3);
        }
        return out + ws() + "}" + ws();
    }
};

auto testAgreement() -> void {
    Gen gen{0x5eed5eedULL};
    fastjson::member_index index;  // ONE index reused: also the reuse proof
    std::size_t documents = 0;
    std::size_t members_checked = 0;
    for (int round = 0; round < 6000; ++round) {
        const std::string doc = gen.document();
        const auto parsed = fastjson::parse(doc);
        if (!parsed.has_value()) { continue;  /* a generated string part may be invalid UTF-8 on purpose: none is, but stay honest */ }
        ++documents;
        fastjson::index_members_into(index, doc);
        check(index.opened() && index.complete(), std::format("agreement: {} is complete", shown(doc)));
        // Every member's span parses to the value parse() holds for that key (the LAST duplicate wins in the object; the
        // index keeps all, so compare the last span of a key with the object).
        std::map<std::string, std::vector<std::string>> spans;
        for (const auto& member : index.members()) {
            const auto key = member.unescaped_key();
            check(key.has_value(), "agreement: a key decodes");
            if (!key.has_value()) { continue; }
            spans[*key].emplace_back(member.value_in(doc));
            const auto value = fastjson::parse(member.value_in(doc));
            check(value.has_value(), std::format("agreement: a span of {} parses", shown(doc)));
            ++members_checked;
        }
        const auto& object = parsed->as_object();
        check(object.size() == spans.size(), std::format("agreement: {} distinct keys (index {}, parse {})", shown(doc), spans.size(), object.size()));
        for (const auto& [key, texts] : spans) {
            const auto it = object.find(key);
            // A key stated twice is held ONCE by an object, as whichever duplicate the parser keeps: the value it holds is one of the spans.
            bool held = false;
            for (const auto& text : texts) {
                const auto value = fastjson::parse(text);
                held = held || (it != object.end() && value.has_value() && it->second.to_string() == value->to_string());
            }
            check(held, std::format("agreement: key {} of {} has a value parse() holds", shown(key), shown(doc)));
        }
        // No strict prefix is a complete document.
        if (round % 5 == 0) {
            const auto start = doc.find('{');
            const auto close = doc.rfind('}');
            for (std::size_t cut = start + 1; cut < close; cut += 1 + (doc.size() / 64)) {
                const auto prefix = fastjson::index_members(std::string_view{doc}.substr(0, cut));
                check(prefix.opened() && !prefix.complete() && prefix.stopped_at() <= cut, std::format("agreement: the prefix of {} cut at {} is incomplete", shown(doc), cut));
            }
        }
    }
    check(documents > 3000 && members_checked > 6000, std::format("agreement: the generator produced enough documents ({}, {} members)", documents, members_checked));
    // Mutations: a document parse() accepts is complete; a structural break parse() reports is never complete here
    // unless the break is in a leaf (the index leaves leaf content to the reader).
    std::size_t accepted = 0;
    for (int round = 0; round < 4000; ++round) {
        std::string doc = gen.document();
        const auto at = gen.pick(doc.size());
        static constexpr std::string_view bytes = "{}[],:\"\\ x1";
        doc[at] = bytes[gen.pick(bytes.size())];
        fastjson::index_members_into(index, doc);
        if (fastjson::parse(doc).has_value()) {
            ++accepted;
            check(index.opened() && index.complete(), std::format("agreement: parse() accepts {} so the index completes", shown(doc)));
        }
        check(index.stopped_at() == npos || index.stopped_at() <= doc.size(), "agreement: stopped_at is within the input");
        check(index.complete() == (index.stopped_at() == npos && index.opened()), "agreement: complete() and stopped_at() agree");
    }
    check(accepted > 100, std::format("agreement: enough mutants stayed valid ({})", accepted));
}

auto testStrings() -> void {
    // A string whose closing quote, and a backslash-quote before it, sit at every offset across SIMD block edges.
    for (std::size_t length = 0; length < 200; ++length) {
        for (const std::string_view tail : {"", "\\\"", "\\\\", "\\\\\\\"", "\\n"}) {
            const std::string body = std::string(length, 'x') + std::string{tail};
            const std::string doc = std::format(R"({{"k":"{}","after":7}})", body);
            const auto index = fastjson::index_members(doc);
            check(index.complete() && index.members().size() == 2 && spanOf(index, doc, 1) == "7" && index.members()[0].value_in(doc).size() == body.size() + 2,
                  std::format("strings: {} bytes then {} ends at the right quote", length, shown(tail)));
            // Same string as the KEY.
            const std::string keyed = std::format(R"({{"{}":1,"z":2}})", body);
            const auto key_index = fastjson::index_members(keyed);
            check(key_index.complete() && key_index.members().size() == 2 && key_index.members()[0].key.size() == body.size() && spanOf(key_index, keyed, 1) == "2",
                  std::format("strings: a key of {} bytes then {}", length, shown(tail)));
        }
    }
    // Raw control bytes and every escape: a string's interior never moves the depth.
    const std::string doc = "{\"a\":\"{[}]\\\\\\\"\x01\x1f{\",\"b\":[\"]\",\"}\"],\"c\":0}";
    const auto index = fastjson::index_members(doc);
    check(index.complete() && index.members().size() == 3 && spanOf(index, doc, 2) == "0", "strings: brackets and a raw control byte inside strings do not move the walk");
    // A very long string and a very long member list.
    const std::string big = "{\"k\":\"" + std::string(1 << 20, 'y') + "\",\"n\":1}";
    const auto big_index = fastjson::index_members(big);
    check(big_index.complete() && big_index.members().size() == 2 && big_index.members()[0].value_in(big).size() == (1U << 20) + 2, "strings: a 1 MiB string");
    std::string many = "{";
    for (int i = 0; i < 20000; ++i) { many += std::format("{}\"k{}\":{}", i == 0 ? "" : ",", i, i); }
    many += "}";
    const auto many_index = fastjson::index_members(many);
    check(many_index.complete() && many_index.members().size() == 20000 && many_index.members()[19999].key == "k19999", "strings: 20000 members");
}

auto testElements() -> void {
    {
        const std::string_view doc = R"( [ "a" , 1,[2,[3]],{"x":"]"} , null,"b\"c" ] )";
        const auto index = fastjson::index_elements(doc);
        check(index.opened() && index.complete() && index.members().size() == 6, std::format("elements: six elements (got {})", index.members().size()));
        check(spanOf(index, doc, 0) == "\"a\"" && spanOf(index, doc, 1) == "1" && spanOf(index, doc, 2) == "[2,[3]]" && spanOf(index, doc, 3) == R"({"x":"]"})" &&
                  spanOf(index, doc, 4) == "null" && spanOf(index, doc, 5) == R"("b\"c")",
              "elements: each span");
        check(index.members()[0].key.empty() && index.members()[0].key_begin == npos, "elements: an element has no key");
    }
    {
        const auto empty = fastjson::index_elements("[ ]");
        check(empty.opened() && empty.complete() && empty.members().empty(), "elements: an empty array");
        check(!fastjson::index_elements("{}").opened() && !fastjson::index_elements("").opened(), "elements: an object is not an array");
        const auto bad = fastjson::index_elements("[1,]");
        check(bad.opened() && !bad.complete() && bad.stopped_at() == 3, "elements: a trailing comma stops at the bracket");
        const auto bad2 = fastjson::index_elements(R"(["a" "b"])");
        check(bad2.opened() && !bad2.complete() && bad2.stopped_at() == 5 && bad2.members().size() == 1, "elements: no comma between elements");
    }
}

auto testDepth() -> void {
    for (const std::size_t depth : {std::size_t{1}, std::size_t{2}, std::size_t{127}, std::size_t{128}}) {
        std::string doc = "{\"a\":";
        for (std::size_t d = 1; d < depth; ++d) { doc += "["; }
        for (std::size_t d = 1; d < depth; ++d) { doc += "]"; }
        if (depth == 1) { doc += "1"; }
        doc += "}";
        const auto index = fastjson::index_members(doc);
        // The root is one level; `depth` levels in all are allowed up to the limit.
        const std::size_t levels = depth == 1 ? 1 : depth;
        check(index.complete() == (levels <= 128), std::format("depth: {} levels are {}", levels, levels <= 128 ? "indexed" : "refused"));
    }
    {
        std::string doc = "{\"a\":";
        for (int d = 0; d < 129; ++d) { doc += "["; }
        for (int d = 0; d < 129; ++d) { doc += "]"; }
        doc += "}";
        const auto index = fastjson::index_members(doc);
        check(index.opened() && !index.complete() && index.stopped_at() == 5 + 127, std::format("depth: past the limit stops at the bracket that would exceed it (got {})", index.stopped_at() == npos ? -1 : static_cast<int>(index.stopped_at())));
        const auto roomy = fastjson::index_members(doc, 1000);
        check(roomy.complete(), "depth: a caller-chosen limit is honoured");
        const auto tight = fastjson::index_members(R"({"a":[1]})", 2);
        check(tight.complete(), "depth: two levels fit a limit of two");
        const auto tighter = fastjson::index_members(R"({"a":[1]})", 1);
        check(tighter.opened() && !tighter.complete() && tighter.stopped_at() == 5, "depth: a limit of one refuses any nested container");
        // A deep document of 100000 levels neither crashes nor overflows the stack.
        std::string deep = "{\"a\":" + std::string(100000, '[') + std::string(100000, ']') + "}";
        const auto deep_index = fastjson::index_members(deep);
        check(deep_index.opened() && !deep_index.complete(), "depth: 100000 levels are refused, not crashed on");
    }
}

auto testReuse() -> void {
    fastjson::member_index index;
    fastjson::index_members_into(index, R"({"a":1,"b":{"c":2}})");
    check(index.complete() && index.members().size() == 2, "reuse: first document");
    fastjson::index_members_into(index, R"({"z":[)");
    check(index.opened() && !index.complete() && index.members().size() == 1 && index.members()[0].key == "z" && index.stopped_at() == 6,
          "reuse: a stopped document carries nothing over from the one before");
    fastjson::index_members_into(index, "[1]");
    check(!index.opened() && !index.complete() && index.members().empty() && index.stopped_at() == npos && !index.any_key_escaped(), "reuse: a non-object resets the report");
    fastjson::index_elements_into(index, "[1,2]");
    check(index.complete() && index.members().size() == 2, "reuse: elements after members");
}

auto testConcurrency() -> void {
    const std::string doc = R"({"model":"m","max_tokens":9,"nested":{"x":[1,2,3]},"stream":false})";
    std::atomic<int> bad{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            fastjson::member_index index;
            for (int i = 0; i < 20000; ++i) {
                fastjson::index_members_into(index, doc);
                if (!index.complete() || index.members().size() != 4 || index.members()[1].value_in(doc) != "9") { ++bad; }
            }
        });
    }
    for (auto& t : threads) { t.join(); }
    check(bad.load() == 0, "concurrency: eight threads, one index each, agree");
}

}  // namespace

auto main(int argc, char** argv) -> int {
    const std::string_view tier = argc > 1 ? argv[1] : "best";
    std::println("test_member_index: tier argument '{}', string-scan kernel '{}'", tier, fastjson::string_scan_tier());
    if (tier != "best" && fastjson::string_scan_tier() != tier) {
        std::println("[FAIL] the registered tier '{}' is not the kernel that runs ('{}'): FASTJSON_SIMD did not take effect", tier, fastjson::string_scan_tier());
        return 1;
    }
    testSpans();
    testStops();
    testLeaves();
    testAgreement();
    testStrings();
    testElements();
    testDepth();
    testReuse();
    testConcurrency();
    std::println("test_member_index: {} checks, {} failures", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
