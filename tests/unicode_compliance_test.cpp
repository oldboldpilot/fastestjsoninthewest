// UTF-16 and UTF-32 Compliance Test
// Author: Olumuyiwa Oluwasanmi
// Tests Unicode escape sequences including surrogate pairs

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include "../modules/fastjson_parallel.cpp"

struct test_case {
    std::string name;
    std::string json;
    bool should_pass;
    std::string expected_value;  // Expected decoded string (UTF-8)
};

// Helper to print UTF-8 string as hex
auto print_hex(const std::string& str) -> void {
    std::cout << "    Hex: ";
    for (unsigned char c : str) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << "\n";
}

auto main() -> int {
    std::vector<test_case> tests = {
        // ===== Basic BMP Characters (U+0000-U+FFFF) =====
        {"ASCII via escape", R"({"text": "\u0048\u0065\u006C\u006C\u006F"})", true, "Hello"},
        {"Latin-1", R"({"text": "\u00E9\u00E8\u00EA"})", true, "éèê"},
        {"Cyrillic", R"({"text": "\u0417\u0434\u0440\u0430\u0432\u0441\u0442\u0432\u0443\u0439\u0442\u0435"})", true, "Здравствуйте"},
        {"CJK", R"({"text": "\u4F60\u597D"})", true, "你好"},
        {"Japanese", R"({"text": "\u3053\u3093\u306B\u3061\u306F"})", true, "こんにちは"},
        {"Arabic", R"({"text": "\u0645\u0631\u062D\u0628\u0627"})", true, "مرحبا"},
        {"Hebrew", R"({"text": "\u05E9\u05DC\u05D5\u05DD"})", true, "שלום"},
        {"Greek", R"({"text": "\u0393\u03B5\u03B9\u03B1"})", true, "Γεια"},
        
        // ===== UTF-16 Surrogate Pairs (U+10000-U+10FFFF) =====
        // Musical symbols (U+1D100-U+1D1FF)
        {"Musical note", R"({"text": "\uD834\uDD1E"})", true, "𝄞"},  // Treble clef
        {"Musical sharp", R"({"text": "\uD834\uDD2A"})", true, "𝄪"},  // Double sharp
        
        // Emoji (U+1F300-U+1F6FF)
        {"Emoji grinning", R"({"text": "\uD83D\uDE00"})", true, "😀"},
        {"Emoji heart", R"({"text": "\uD83D\uDC96"})", true, "💖"},
        {"Emoji rocket", R"({"text": "\uD83D\uDE80"})", true, "🚀"},
        {"Emoji thumbs up", R"({"text": "\uD83D\uDC4D"})", true, "👍"},
        
        // Ancient scripts (U+10000-U+1007F)
        {"Linear B", R"({"text": "\uD800\uDC00"})", true, "𐀀"},
        {"Cuneiform", R"({"text": "\uD808\uDC00"})", true, "𒀀"},
        
        // Mathematical symbols (U+1D400-U+1D7FF)
        {"Math bold A", R"({"text": "\uD835\uDC00"})", true, "𝐀"},
        {"Math italic a", R"({"text": "\uD835\uDC1A"})", true, "𝐚"},
        
        // Edge cases at plane boundaries
        {"First SMP char", R"({"text": "\uD800\uDC00"})", true, "𐀀"},  // U+10000
        {"Last valid char", R"({"text": "\uDBFF\uDFFF"})", true, "􏿿"},  // U+10FFFF
        
        // ===== Mixed Content =====
        {"BMP + Surrogate pair", R"({"text": "Hello \uD83D\uDE00 World"})", true, "Hello 😀 World"},
        {"Multiple pairs", R"({"text": "\uD83D\uDE00\uD83D\uDE80\uD83D\uDC4D"})", true, "😀🚀👍"},
        {"ASCII + CJK + Emoji", R"({"text": "Hello \u4F60\u597D \uD83D\uDE00"})", true, "Hello 你好 😀"},
        
        // ===== Invalid UTF-16 Sequences (Should Fail) =====
        {"Lone high surrogate", R"({"text": "\uD800"})", false, ""},
        {"Lone high surrogate mid", R"({"text": "Hello\uD800World"})", false, ""},
        {"Lone low surrogate", R"({"text": "\uDC00"})", false, ""},
        {"High surrogate + ASCII", R"({"text": "\uD800A"})", false, ""},
        {"High surrogate + BMP", R"({"text": "\uD800\u0041"})", false, ""},
        {"Reversed surrogates", R"({"text": "\uDC00\uD800"})", false, ""},
        {"Two high surrogates", R"({"text": "\uD800\uD800"})", false, ""},
        {"Two low surrogates", R"({"text": "\uDC00\uDC00"})", false, ""},
        
        // ===== Edge Cases =====
        {"NULL character", R"({"text": "\u0000"})", true, std::string("\0", 1)},
        {"Control chars escaped", R"({"text": "\u0001\u001F"})", true, "\x01\x1F"},
        {"Last BMP char", R"({"text": "\uFFFF"})", true, "\xEF\xBF\xBF"},
        {"Surrogate boundary -1", R"({"text": "\uD7FF"})", true, "\xED\x9F\xBF"},  // U+D7FF (valid)
        {"Surrogate boundary +1", R"({"text": "\uE000"})", true, "\xEE\x80\x80"},  // U+E000 (valid)
        
        // ===== Real-World Examples =====
        {"Flag emoji (GB)", R"({"text": "\uD83C\uDDEC\uD83C\uDDE7"})", true, "🇬🇧"},
        {"Family emoji", R"({"text": "\uD83D\uDC68\u200D\uD83D\uDC69\u200D\uD83D\uDC67"})", true, "👨‍👩‍👧"},
        {"Skin tone modifier", R"({"text": "\uD83D\uDC4B\uD83C\uDFFB"})", true, "👋🏻"},
    };
    
    int passed = 0;
    int failed = 0;
    
    std::cout << "=== UTF-16/UTF-32 Unicode Compliance Tests ===\n\n";
    
    for (const auto& test : tests) {
        try {
            auto result = fastjson::parse(test.json);
            
            bool test_passed = false;
            
            if (test.should_pass && result.has_value()) {
                // Extract the "text" field
                if (result->is_object()) {
                    const auto& obj = result->as_object();
                    auto it = obj.find("text");
                    if (it != obj.end() && it->second.is_string()) {
                        const auto& decoded = it->second.as_string();
                        if (decoded == test.expected_value) {
                            test_passed = true;
                        } else {
                            std::cout << "✗ FAIL: " << test.name << "\n";
                            std::cout << "  Expected: \"" << test.expected_value << "\"\n";
                            print_hex(test.expected_value);
                            std::cout << "  Got:      \"" << decoded << "\"\n";
                            print_hex(decoded);
                            failed++;
                            continue;
                        }
                    }
                }
            } else if (!test.should_pass && !result.has_value()) {
                test_passed = true;
            }
            
            if (test_passed) {
                std::cout << "✓ PASS: " << test.name << "\n";
                passed++;
            } else {
                std::cout << "✗ FAIL: " << test.name << "\n";
                if (test.should_pass) {
                    std::cout << "  Expected: success with \"" << test.expected_value << "\"\n";
                    std::cout << "  Got: " << (result.has_value() ? "success (wrong value)" : "error") << "\n";
                    if (!result.has_value()) {
                        std::cout << "  Error: " << result.error().message << "\n";
                    }
                } else {
                    std::cout << "  Expected: error\n";
                    std::cout << "  Got: success\n";
                }
                failed++;
            }
        } catch (const std::exception& e) {
            if (!test.should_pass) {
                std::cout << "✓ PASS: " << test.name << " (caught exception)\n";
                passed++;
            } else {
                std::cout << "✗ FAIL: " << test.name << " (unexpected exception: " 
                         << e.what() << ")\n";
                failed++;
            }
        }
    }
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "Test Coverage:\n";
    std::cout << "  ✓ BMP characters (U+0000-U+FFFF)\n";
    std::cout << "  ✓ UTF-16 surrogate pairs (U+10000-U+10FFFF)\n";
    std::cout << "  ✓ Emoji and modern Unicode\n";
    std::cout << "  ✓ Invalid surrogate sequences\n";
    std::cout << "  ✓ Edge cases and boundaries\n";
    std::cout << "========================================\n";
    
    return failed > 0 ? 1 : 0;
}
