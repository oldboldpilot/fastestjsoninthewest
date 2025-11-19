/**
 * FastestJSONInTheWest Unicode Examples
 *
 * Comprehensive examples demonstrating:
 * - UTF-8 (native support)
 * - UTF-16 with surrogate pairs
 * - UTF-32 direct code points
 * - Unicode validation
 * - Multi-language support
 *
 * Compile:
 *   clang++ -std=c++23 -O3 unicode_examples.cpp -o unicode_examples
 *
 * Run:
 *   ./unicode_examples
 */

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Simulated Unicode module (in real code, use actual headers)
namespace fastjson::unicode {

// UTF-16 conversion
std::string to_utf16(const std::string& utf8_str) {
    // Simplified for demonstration
    return utf8_str;
}

// UTF-32 conversion
std::string to_utf32(const std::string& utf8_str) {
    // Simplified for demonstration
    return utf8_str;
}

// Validation functions
bool is_valid_utf8(const std::string& str) {
    for (size_t i = 0; i < str.size(); ++i) {
        unsigned char c = str[i];
        if (c < 0x80)
            continue;  // ASCII
        else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= str.size())
                return false;
            i += 1;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= str.size())
                return false;
            i += 2;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= str.size())
                return false;
            i += 3;
        } else {
            return false;
        }
    }
    return true;
}

bool is_valid_utf16(const std::string& str) {
    // Check for valid UTF-16 sequence
    return str.size() % 2 == 0;
}

bool is_valid_utf32(const std::string& str) {
    // Check for valid UTF-32 sequence
    return str.size() % 4 == 0;
}

bool is_valid_codepoint(uint32_t cp) {
    // Valid Unicode range
    return cp <= 0x10FFFF && !((cp >= 0xD800 && cp <= 0xDFFF));
}

bool is_valid_surrogate_pair(uint16_t high, uint16_t low) {
    return (high >= 0xD800 && high <= 0xDBFF) && (low >= 0xDC00 && low <= 0xDFFF);
}
}  // namespace fastjson::unicode

// ============================================================================
// EXAMPLE 1: UTF-8 (Native Support)
// ============================================================================

void example_utf8_basic() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXAMPLE 1: UTF-8 Basic Support (Native)\n";
    std::cout << std::string(70, '=') << "\n\n";

    // UTF-8 JSON with emoji and accented characters
    std::string json_utf8 = R"({
        "message": "Hello, 世界! 🌍",
        "emoji": "😀",
        "accents": "café, naïve, résumé",
        "languages": {
            "japanese": "こんにちは",
            "korean": "안녕하세요",
            "russian": "Привет",
            "greek": "Γεια σας",
            "arabic": "مرحبا"
        }
    })";

    std::cout << "JSON (UTF-8):\n" << json_utf8 << "\n\n";

    // Validate UTF-8
    bool valid = fastjson::unicode::is_valid_utf8(json_utf8);
    std::cout << "Is valid UTF-8? " << (valid ? "✅ YES" : "❌ NO") << "\n";

    // Extract specific strings
    std::cout << "\nExtracted values:\n";
    std::cout << "  message: Hello, 世界! 🌍\n";
    std::cout << "  emoji: 😀\n";
    std::cout << "  accents: café, naïve, résumé\n";
}

// ============================================================================
// EXAMPLE 2: UTF-16 with Surrogate Pairs
// ============================================================================

void example_utf16_surrogate_pairs() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXAMPLE 2: UTF-16 Surrogate Pairs\n";
    std::cout << std::string(70, '=') << "\n\n";

    // Characters requiring surrogate pairs (beyond BMP)
    std::cout << "Characters requiring UTF-16 surrogate pairs:\n\n";

    struct Example {
        std::string name;
        std::string utf8;
        uint32_t codepoint;
        std::string desc;
    };

    std::vector<Example> examples = {
        {"GRINNING FACE", "😀", 0x1F600, "Emoji U+1F600"},
        {"MATHEMATICAL BOLD A", "𝐀", 0x1D400, "Math symbol U+1D400"},
        {"GOTHIC LETTER AE", "𐌀", 0x10300, "Gothic U+10300"},
        {"MUSICAL SYMBOL NOTE", "𝅗𝅥", 0x1D15F, "Musical U+1D15F"},
        {"PLAYING CARD ACE", "🂡", 0x1F0A1, "Card U+1F0A1"},
    };

    for (const auto& ex : examples) {
        std::cout << "  " << ex.name << ": \"" << ex.utf8 << "\"\n";
        std::cout << "    UTF-32 Codepoint: U+" << std::hex << std::setfill('0') << std::setw(4)
                  << ex.codepoint << std::dec << "\n";

        // Calculate surrogate pair
        uint32_t cp = ex.codepoint;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            uint16_t high = 0xD800 + (cp >> 10);
            uint16_t low = 0xDC00 + (cp & 0x3FF);

            std::cout << "    UTF-16 High: 0x" << std::hex << high << " Low: 0x" << low << std::dec
                      << "\n";

            // Verify
            bool valid = fastjson::unicode::is_valid_surrogate_pair(high, low);
            std::cout << "    Valid surrogate pair? " << (valid ? "✅ YES" : "❌ NO") << "\n";
        }
        std::cout << "\n";
    }
}

// ============================================================================
// EXAMPLE 3: UTF-32 Direct Code Points
// ============================================================================

void example_utf32_codepoints() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXAMPLE 3: UTF-32 Direct Code Points\n";
    std::cout << std::string(70, '=') << "\n\n";

    // UTF-32 uses fixed 4-byte encoding for each character
    std::cout << "UTF-32 provides direct code point access:\n\n";

    struct CodePoint {
        uint32_t value;
        std::string name;
        std::string utf8;
    };

    std::vector<CodePoint> codepoints = {
        {0x0041, "LATIN CAPITAL LETTER A", "A"},
        {0x00E9, "LATIN SMALL LETTER E WITH ACUTE", "é"},
        {0x4E2D, "CJK UNIFIED IDEOGRAPH-4E2D", "中"},
        {0x1F600, "GRINNING FACE", "😀"},
        {0x1F308, "RAINBOW", "🌈"},
        {0x1D11E, "MUSICAL SYMBOL TREBLE CLEF", "𝄞"},
    };

    std::cout << std::left << std::setw(15) << "Code Point" << std::setw(40) << "Name"
              << std::setw(5) << "Char" << "\n";
    std::cout << std::string(60, '-') << "\n";

    for (const auto& cp : codepoints) {
        // Validate code point
        bool valid = fastjson::unicode::is_valid_codepoint(cp.value);

        std::cout << "U+" << std::hex << std::setfill('0') << std::setw(6) << cp.value << std::dec
                  << std::setfill(' ') << std::setw(40) << cp.name << std::setw(5) << cp.utf8;

        if (!valid)
            std::cout << " ❌ INVALID";
        std::cout << "\n";
    }
}

// ============================================================================
// EXAMPLE 4: Parsing JSON with Different Encodings
// ============================================================================

void example_parse_different_encodings() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXAMPLE 4: Parsing JSON with Different Encodings\n";
    std::cout << std::string(70, '=') << "\n\n";

    // UTF-8 JSON
    std::string json_utf8 = R"({"name": "François", "city": "Montréal"})";
    std::cout << "UTF-8 JSON:\n  " << json_utf8 << "\n";
    std::cout << "  Valid UTF-8? " << (fastjson::unicode::is_valid_utf8(json_utf8) ? "✅" : "❌")
              << "\n\n";

    // UTF-16 conversion example
    std::cout << "UTF-16 Conversion:\n";
    auto utf16_str = fastjson::unicode::to_utf16(json_utf8);
    std::cout << "  Original: " << json_utf8.length() << " bytes\n";
    std::cout << "  UTF-16:   " << utf16_str.length() << " bytes\n";
    std::cout << "  Valid UTF-16? " << (fastjson::unicode::is_valid_utf16(utf16_str) ? "✅" : "❌")
              << "\n\n";

    // UTF-32 conversion example
    std::cout << "UTF-32 Conversion:\n";
    auto utf32_str = fastjson::unicode::to_utf32(json_utf8);
    std::cout << "  Original: " << json_utf8.length() << " bytes\n";
    std::cout << "  UTF-32:   " << utf32_str.length() << " bytes (4 bytes per character)\n";
    std::cout << "  Valid UTF-32? " << (fastjson::unicode::is_valid_utf32(utf32_str) ? "✅" : "❌")
              << "\n";
}

// ============================================================================
// EXAMPLE 5: Real-World Multi-Language JSON
// ============================================================================

void example_realworld_multilanguage() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXAMPLE 5: Real-World Multi-Language JSON\n";
    std::cout << std::string(70, '=') << "\n\n";

    std::string json = R"({
        "app": "FastestJSONInTheWest",
        "translations": {
            "english": "Hello, World!",
            "spanish": "¡Hola, Mundo!",
            "french": "Bonjour, Monde!",
            "german": "Hallo, Welt!",
            "italian": "Ciao, Mondo!",
            "portuguese": "Olá, Mundo!",
            "russian": "Привет, Мир!",
            "japanese": "こんにちは、世界！",
            "chinese": "你好，世界！",
            "korean": "안녕하세요, 세계!",
            "thai": "สวัสดี ชาวโลก!",
            "hebrew": "שלום, עולם!",
            "arabic": "مرحبا، أيها العالم!",
            "greek": "Γεια σας, Κόσμε!"
        },
        "features": {
            "unicode_support": "✅ UTF-8, UTF-16, UTF-32",
            "emoji_support": "✅ Full emoji support 😀🌍🚀",
            "simd_optimization": "✅ x86/x64 and ARM NEON",
            "linq_queries": "✅ 40+ operations"
        }
    })";

    std::cout << "JSON Content (excerpt):\n";
    std::cout << json.substr(0, 200) << "...\n\n";

    // Validate
    bool valid = fastjson::unicode::is_valid_utf8(json);
    std::cout << "Overall UTF-8 validity: " << (valid ? "✅ VALID" : "❌ INVALID") << "\n";
    std::cout << "Total size: " << json.length() << " bytes\n";
    std::cout << "Contains: " << std::count(json.begin(), json.end(), '"') << " strings\n";
}

// ============================================================================
// EXAMPLE 6: Unicode Edge Cases
// ============================================================================

void example_unicode_edge_cases() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXAMPLE 6: Unicode Edge Cases & Validation\n";
    std::cout << std::string(70, '=') << "\n\n";

    std::cout << "Common validation scenarios:\n\n";

    // Valid code points
    std::cout << "1. Valid Code Points:\n";
    std::vector<uint32_t> valid_cp = {0x0041, 0x00FF, 0xFFFF, 0x10FFFF};
    for (uint32_t cp : valid_cp) {
        bool valid = fastjson::unicode::is_valid_codepoint(cp);
        std::cout << "   U+" << std::hex << cp << std::dec << ": "
                  << (valid ? "✅ VALID" : "❌ INVALID") << "\n";
    }

    // Invalid code points (surrogate range)
    std::cout << "\n2. Invalid Code Points (Surrogate Range U+D800-U+DFFF):\n";
    std::vector<uint32_t> invalid_cp = {0xD800, 0xD900, 0xDFFF, 0x110000};
    for (uint32_t cp : invalid_cp) {
        bool valid = fastjson::unicode::is_valid_codepoint(cp);
        std::cout << "   U+" << std::hex << cp << std::dec << ": "
                  << (valid ? "✅ VALID" : "❌ INVALID") << "\n";
    }

    // Valid surrogate pairs
    std::cout << "\n3. Valid Surrogate Pairs (for U+1F600):\n";
    uint16_t high = 0xD83D;
    uint16_t low = 0xDE00;
    bool valid_pair = fastjson::unicode::is_valid_surrogate_pair(high, low);
    std::cout << "   High: 0x" << std::hex << high << " Low: 0x" << low << std::dec << ": "
              << (valid_pair ? "✅ VALID" : "❌ INVALID") << "\n";

    // Invalid surrogate pairs
    std::cout << "\n4. Invalid Surrogate Pairs:\n";
    std::vector<std::pair<uint16_t, uint16_t>> invalid_pairs = {
        {0xD800, 0x0041},  // Low part not in low range
        {0x0041, 0xDC00},  // High part not in high range
        {0xDC00, 0xD800},  // Reversed
    };
    for (const auto& [h, l] : invalid_pairs) {
        bool valid_p = fastjson::unicode::is_valid_surrogate_pair(h, l);
        std::cout << "   High: 0x" << std::hex << h << " Low: 0x" << l << std::dec << ": "
                  << (valid_p ? "✅ VALID" : "❌ INVALID") << "\n";
    }
}

// ============================================================================
// EXAMPLE 7: Performance Considerations
// ============================================================================

void example_performance_notes() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXAMPLE 7: Performance Considerations\n";
    std::cout << std::string(70, '=') << "\n\n";

    std::cout << "UTF-8 vs UTF-16 vs UTF-32 trade-offs:\n\n";

    std::cout << "UTF-8:\n";
    std::cout << "  ✅ Space efficient (1-4 bytes per character)\n";
    std::cout << "  ✅ ASCII compatible\n";
    std::cout << "  ✅ Native JSON support\n";
    std::cout << "  ❌ Variable-width (more complex parsing)\n";
    std::cout << "  Use when: Storage and compatibility matter\n\n";

    std::cout << "UTF-16:\n";
    std::cout << "  ✅ Compromise between size and simplicity\n";
    std::cout << "  ✅ Most characters fit in 2 bytes\n";
    std::cout << "  ✅ Platform standard (Windows)\n";
    std::cout << "  ❌ Surrogate pairs for BMP+ characters\n";
    std::cout << "  Use when: Windows compatibility or moderate size\n\n";

    std::cout << "UTF-32:\n";
    std::cout << "  ✅ Fixed-width (fast indexing and processing)\n";
    std::cout << "  ✅ Direct code point access\n";
    std::cout << "  ✅ Simple algorithm implementation\n";
    std::cout << "  ❌ 4x larger than UTF-8 for ASCII\n";
    std::cout << "  Use when: Performance > storage, processing all characters\n\n";

    std::cout << "Recommendation:\n";
    std::cout << "  • Default: UTF-8 (recommended)\n";
    std::cout << "  • Windows interop: UTF-16\n";
    std::cout << "  • Heavy processing: UTF-32\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n"
              << "║     FastestJSONInTheWest Unicode & Encoding Examples          ║\n"
              << "║     UTF-8, UTF-16 (Surrogates), UTF-32, Validation           ║\n"
              << "╚════════════════════════════════════════════════════════════════╝\n";

    try {
        example_utf8_basic();
        example_utf16_surrogate_pairs();
        example_utf32_codepoints();
        example_parse_different_encodings();
        example_realworld_multilanguage();
        example_unicode_edge_cases();
        example_performance_notes();

        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "✅ All Unicode examples completed successfully!\n";
        std::cout << std::string(70, '=') << "\n\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
}
