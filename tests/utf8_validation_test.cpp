// UTF-8 Validation Test
// Author: Olumuyiwa Oluwasanmi

#include <iostream>
#include <string>
#include <vector>
import logger;
#include "../modules/fastjson_parallel.cpp"

struct test_case {
    std::string name;
    std::string json;
    bool should_pass;
};

auto main() -> int {
    auto& log = logger::Logger::getInstance();
    
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
    
    log.info("Running UTF-8 validation tests...\n");
    
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
                log.info("✓ PASS: {}", test.name);
                passed++;
            } else {
                log.error("✗ FAIL: {}", test.name);
                if (test.should_pass) {
                    log.error("  Expected: success");
                    log.error("  Got: {}", (result.has_value() ? "success" : "error"));
                    if (!result.has_value()) {
                        log.error("  Error: {}", result.error().message);
                    }
                } else {
                    log.error("  Expected: error");
                    log.error("  Got: success");
                }
                failed++;
            }
        } catch (const std::exception& e) {
            if (!test.should_pass) {
                log.info("✓ PASS: {} (caught exception)", test.name);
                passed++;
            } else {
                log.error("✗ FAIL: {} (unexpected exception: {})", test.name, e.what());
                failed++;
            }
        }
    }
    
    log.info("\n========================================");
    log.info("Results: {} passed, {} failed", passed, failed);
    log.info("========================================");
    
    return failed > 0 ? 1 : 0;
}
