// Tests for text that comes from outside the process (escape_string_untrusted, writer::value_untrusted, writer::key_untrusted)
// and for the one-leaf fast path (decode_one_value).
//
// The untrusted forms never fail: a byte that is not part of well-formed UTF-8 is written as \ufffd (one per byte); DEL, the
// C1 controls U+0080..U+009F, U+2028 and U+2029 -- which JSON allows raw and a log reader or a terminal acts on -- are written
// as \u escapes; everything else is written exactly as escape_string writes it. The output always parses, and for text that is
// valid UTF-8 and holds none of those characters it equals escape_string's.
//
// decode_one_value is the fast path for a caller that already has the span of ONE leaf (index_members gives it): it decodes a
// string or a number without building a json_value. It must agree with parse() on every span -- what parse() accepts as one
// string or number it accepts, with the same text, and what parse() refuses it refuses.
// Run: ./test_escape_untrusted (exits 0 on pass, non-zero on any failure)

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

static auto untrusted(std::string_view text) -> std::string { return fastjson::escape_string_untrusted(text); }

// What the document decodes to, so a test can say what a reader sees and not only how it is spelled.
static auto decoded(std::string_view quoted) -> std::string {
    auto parsed = fastjson::parse(quoted);
    return parsed && parsed->is_string() ? std::string(parsed->as_string()) : std::string("<not a string>");
}

// What parse() makes of `span` as ONE value: its text when it is a string or a number, and whether it is one at all.
struct parsed_leaf {
    bool is_leaf = false;
    bool is_string = false;
    std::string text;
};

static auto parse_one(std::string_view span) -> parsed_leaf {
    auto parsed = fastjson::parse(span);
    if (!parsed) { return {}; }
    if (parsed->is_string()) { return {true, true, std::string(parsed->as_string())}; }
    if (parsed->is_number()) { return {true, false, std::string(span)}; }
    return {};
}

static auto check_untrusted() -> void {
    // Text that needs nothing from the untrusted rules is written exactly as escape_string writes it.
    for (const std::string_view text : {"", "plain", "say \"hi\"", "back\\slash", "tab\tnl\ncr\r", "\x01\x1b[31m\x1f",
                                        "caf\xC3\xA9", "\xE1\xBA\xB8\xCC\x80k\xE1\xBB\x8D\xCC\x81 Yor\xC3\xB9" "b\xC3\xA1",
                                        "\xF0\x9F\x98\x80", "\xC2\xA0 nbsp", "\xE2\x80\x8B zwsp"}) {
        const auto trusted = fastjson::escape_string(text);
        CHECK(trusted.has_value());
        CHECK(trusted && untrusted(text) == *trusted);
    }

    // Bytes that are not UTF-8: one \ufffd per byte, and the neighbours are untouched.
    CHECK_TEXT(untrusted("a\xFF" "b"), "\"a\\ufffdb\"");
    CHECK_TEXT(untrusted("\x80"), "\"\\ufffd\"");
    CHECK_TEXT(untrusted("\xC3"), "\"\\ufffd\"");               // truncated 2-byte sequence at the end
    CHECK_TEXT(untrusted("\xE2\x82"), "\"\\ufffd\\ufffd\"");     // truncated 3-byte sequence: both bytes
    CHECK_TEXT(untrusted("\xC0\xAF"), "\"\\ufffd\\ufffd\"");     // overlong '/'
    CHECK_TEXT(untrusted("\xED\xA0\x80"), "\"\\ufffd\\ufffd\\ufffd\"");  // UTF-16 surrogate encoded as UTF-8
    CHECK_TEXT(untrusted("\xF4\x90\x80\x80"), "\"\\ufffd\\ufffd\\ufffd\\ufffd\"");  // above U+10FFFF
    CHECK_TEXT(untrusted("ok \xE2\x82\xAC \xFF end"), "\"ok \xE2\x82\xAC \\ufffd end\"");

    // What a reader acts on instead of showing: escaped, as their own code point.
    CHECK_TEXT(untrusted("a\x7f" "b"), "\"a\\u007fb\"");
    CHECK_TEXT(untrusted("\xC2\x80"), "\"\\u0080\"");
    CHECK_TEXT(untrusted("\xC2\x85"), "\"\\u0085\"");           // NEL
    CHECK_TEXT(untrusted("\xC2\x9F"), "\"\\u009f\"");
    CHECK_TEXT(untrusted("\xC2\xA0"), "\"\xC2\xA0\"");           // U+00A0 is text, not a control
    CHECK_TEXT(untrusted("\xE2\x80\xA8"), "\"\\u2028\"");
    CHECK_TEXT(untrusted("\xE2\x80\xA9"), "\"\\u2029\"");
    CHECK_TEXT(untrusted("\xE2\x80\xAA"), "\"\xE2\x80\xAA\"");   // U+202A is not one of them

    // Control characters keep the writer's spelling (the same as escape_string's).
    CHECK_TEXT(untrusted("\x01\n\"\\"), "\"\\u0001\\n\\\"\\\\\"");

    // Whatever comes in, what goes out is JSON: it parses, and a reader sees the replacement characters.
    std::string every_byte;
    for (int b = 0; b < 256; ++b) { every_byte += static_cast<char>(b); }
    const auto all = untrusted(every_byte);
    CHECK(fastjson::parse(all).has_value());
    CHECK(fastjson::escape_string(decoded(all)).has_value());
    CHECK_TEXT(decoded(untrusted("a\xFF" "b")), "a\xEF\xBF\xBD" "b");
    CHECK_TEXT(decoded(untrusted("\xE2\x80\xA8")), "\xE2\x80\xA8");  // escaped on the wire, the same character once read

    // The writer: value_untrusted and key_untrusted never fail the document where value() and key() do.
    CHECK(!fastjson::writer{}.value("a\xFF").finish().has_value());
    CHECK(!fastjson::writer{}.begin_object().key("a\xFF").value(1).end_object().finish().has_value());
    const auto doc = fastjson::writer{}
                         .begin_object()
                         .key("path").value_untrusted("/a\xFF\x7f\xE2\x80\xA8")
                         .key_untrusted("k\xFF").value(1)
                         .key("n").value(2)
                         .end_object()
                         .finish();
    CHECK(doc.has_value());
    CHECK_TEXT(doc ? *doc : std::string("<error>"), "{\"path\":\"/a\\ufffd\\u007f\\u2028\",\"k\\ufffd\":1,\"n\":2}");
    // Layouts apply to the untrusted forms as to the trusted ones.
    const auto spaced = fastjson::writer{}.layout(fastjson::writer_layout::python()).begin_array().value_untrusted("x\xFF").value_untrusted("y").end_array().finish();
    CHECK_TEXT(spaced ? *spaced : std::string("<error>"), "[\"x\\ufffd\", \"y\"]");
    // A value of the wrong kind in the wrong place is still the call-order error it was.
    CHECK(!fastjson::writer{}.begin_object().value_untrusted("v").end_object().finish().has_value());
}

static auto check_decode_one_value() -> void {
    // A string: the decoded content, exactly what parse() gives.
    for (const std::string_view span : {R"("")", R"("plain")", R"("say \"hi\"")", R"("back\\slash")",
                                        R"("tab\tnl\ncr\r")", R"("\/")", R"("\b\f")", R"("caf\u00e9")",
                                        R"("\ud83d\ude00")", R"("café")", R"("Yorùbá")", R"("\u0000")",
                                        R"("\u001f")", R"("\uFFFD")", R"("a\u00e9b\u00e9c")"}) {
        const auto leaf = fastjson::decode_one_value(span);
        const auto reference = parse_one(span);
        if (!leaf) { std::println(std::cerr, "  refused a string parse() accepts: {}", span); }
        CHECK(leaf.has_value());
        CHECK(leaf && leaf->kind == fastjson::leaf_kind::string);
        CHECK(reference.is_leaf && reference.is_string);
        CHECK(leaf && leaf->text == reference.text);
    }

    // A number: exactly as written, and only when it is RFC 8259's grammar.
    for (const std::string_view span : {"0", "-0", "1", "-1", "30", "12.5", "-12.5e3", "1e10", "1E+10", "1e-10",
                                        "0.0001", "123456789012345678901234567890", "1.7976931348623157e308"}) {
        const auto leaf = fastjson::decode_one_value(span);
        const auto reference = parse_one(span);
        if (!leaf) { std::println(std::cerr, "  refused a number parse() accepts: {}", span); }
        CHECK(leaf.has_value());
        CHECK(leaf && leaf->kind == fastjson::leaf_kind::number);
        CHECK(reference.is_leaf && !reference.is_string);
        CHECK(leaf && leaf->text == span);
    }

    // What is NOT one leaf is refused, and parse() refuses it too (or it is a container/literal, which is not a leaf).
    for (const std::string_view span : {"", " ", "{}", "[]", "{\"a\":1}", "[1]", "true", "false", "null",
                                        "\"unterminated", "\"bad\\qescape\"", "\"\\u00\"", "\"\\ud800\"",
                                        "\"raw\x01control\"", "\"a\" \"b\"", "1 2", "01", "+1", ".5", "1.", "1e",
                                        "1e+", "-", "--1", "0x10", "1,2", "\"a\"x", "nul", "tru"}) {
        const auto leaf = fastjson::decode_one_value(span);
        if (leaf) { std::println(std::cerr, "  accepted what is not one leaf: {}", span); }
        CHECK(!leaf.has_value());
    }

    // A string that is not UTF-8 is refused, as parse() refuses it.
    CHECK(!fastjson::decode_one_value("\"a\xFF" "b\"").has_value());
    CHECK(!fastjson::parse("\"a\xFF" "b\"").has_value());

    // AGREEMENT WITH parse() OVER EVERY SPAN index_members PRODUCES. The fast path exists to be used on those spans, so the
    // property that matters is: for every member of a document, decode_one_value's answer equals parse()'s.
    const std::string_view document = R"({"a":"plain","b":"say \"hi\"","c":"caf\u00e9","d":"\ud83d\ude00","e":30,"f":-12.5e3,"g":true,"h":null,"i":{"x":1},"j":[1,2],"k":"","l":"Yor\u00f9b\u00e1"})";
    const auto index = fastjson::index_members(document);
    CHECK(index.opened() && index.complete());
    std::size_t leaves = 0;
    for (const auto& member : index.members()) {
        const auto span = member.value_in(document);
        const auto leaf = fastjson::decode_one_value(span);
        const auto reference = parse_one(span);
        if (reference.is_leaf) {
            ++leaves;
            if (!leaf) { std::println(std::cerr, "  refused an indexed leaf: {}", span); }
            CHECK(leaf.has_value());
            CHECK(leaf && leaf->text == reference.text);
            CHECK(leaf && (leaf->kind == fastjson::leaf_kind::string) == reference.is_string);
        } else {
            // A container or a literal: not a leaf, so the fast path must refuse it rather than guess.
            CHECK(!leaf.has_value());
        }
    }
    CHECK(leaves == 8);   // a b c d k l are strings, e f are numbers; g h are literals and i j are containers
}

int main() {
    check_untrusted();
    check_decode_one_value();
    if (g_failed == 0) { std::println("test_escape_untrusted: all checks passed"); }
    return g_failed == 0 ? 0 : 1;
}
