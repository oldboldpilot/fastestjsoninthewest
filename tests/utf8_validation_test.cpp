// UTF-8 Validation Test
// Author: Olumuyiwa Oluwasanmi

#include <iostream>
#include <string>
#include <vector>
#include "../modules/fastjson_parallel.cpp"

struct test_case {
    std::string name;
    std::string json;
    bool should_pass;
};

auto main() -> int {
    std::vector<test_case> tests = {
        // Valid UTF-8
        {"ASCII only", R"({"text": "Hello World"})", true},
        {"2-byte UTF-8", R"({"text": "Héllo"})", true},
        {"3-byte UTF-8", R"({"text": "你好"})", true},
        {"4-byte UTF-8", R"({"text": "𝄞"})", true},
        {"Mixed UTF-8", R"({"text": "Hello 世界 🌍"})", true},
        {"Escaped Unicode", R"({"text": "\u4f60\u597d"})", true},
        
        // Invalid UTF-8 sequences (these should fail with proper validation)
        // Note: We're testing raw UTF-8 bytes here, not JSON escapes
        {"Invalid continuation byte", "{\"text\": \"Hello\x80World\"}", false},
        {"Truncated 2-byte", "{\"text\": \"Hello\xC2\"}", false},
        {"Truncated 3-byte", "{\"text\": \"Hello\xE0\xA0\"}", false},
        {"Overlong 2-byte", "{\"text\": \"Hello\xC0\x80\"}", false},
        {"Surrogate half", "{\"text\": \"Hello\xED\xA0\x80\"}", false},
        {"Invalid 4-byte start", "{\"text\": \"Hello\xF5\x80\x80\x80\"}", false},
    };
    
    int passed = 0;
    int failed = 0;
    
    std::cout << "Running UTF-8 validation tests...\n\n";
    
    for (const auto& test : tests) {
        try {
            auto result = fastjson::parse(test.json);
            
            bool test_passed = false;
            if (test.should_pass && result.has_value()) {
                test_passed = true;
            } else if (!test.should_pass && !result.has_value()) {
                test_passed = true;
            }
            
            if (test_passed) {
                std::cout << "✓ PASS: " << test.name << "\n";
                passed++;
            } else {
                std::cout << "✗ FAIL: " << test.name << "\n";
                if (test.should_pass) {
                    std::cout << "  Expected: success\n";
                    std::cout << "  Got: " << (result.has_value() ? "success" : "error") << "\n";
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
    std::cout << "========================================\n";
    
    return failed > 0 ? 1 : 0;
}
