// Test ARM NEON SIMD functionality
#include <chrono>
#include <iomanip>
#include <iostream>

#include "modules/fastjson_parallel.cpp"

void print_result(const char* test_name, bool passed) {
    std::cout << (passed ? "✓ PASS: " : "✗ FAIL: ") << test_name << std::endl;
}

void test_basic_parsing() {
    std::cout << "\n=== Basic Parsing Tests ===" << std::endl;

    // Test 1: Simple object
    const char* json1 = R"({"name": "test", "value": 123})";
    auto result1 = fastjson::parse(json1);
    print_result("Simple object", result1.has_value() && result1->is_object());

    // Test 2: Array with whitespace
    const char* json2 = R"(  [  1  ,  2  ,  3  ,  4  ,  5  ]  )";
    auto result2 = fastjson::parse(json2);
    print_result("Array with whitespace", result2.has_value() && result2->is_array());

    // Test 3: Nested structures
    const char* json3 = R"({
        "users": [
            {"id": 1, "name": "Alice"},
            {"id": 2, "name": "Bob"}
        ]
    })";
    auto result3 = fastjson::parse(json3);
    print_result("Nested structures", result3.has_value());

    // Test 4: String with escapes
    const char* json4 = R"({"text": "Hello\nWorld\t\"\\"})";
    auto result4 = fastjson::parse(json4);
    print_result("String with escapes", result4.has_value());

    // Test 5: Unicode (UTF-8)
    const char* json5 = R"({"emoji": "😀🚀", "chinese": "中文", "japanese": "日本語"})";
    auto result5 = fastjson::parse(json5);
    print_result("Unicode UTF-8", result5.has_value());

    // Test 6: UTF-16 surrogate pairs
    const char* json6 = R"({"music": "\uD834\uDD1E", "emoji": "\uD83D\uDE80"})";
    auto result6 = fastjson::parse(json6);
    print_result("UTF-16 surrogate pairs", result6.has_value());
}

void test_whitespace_handling() {
    std::cout << "\n=== Whitespace Handling Tests ===" << std::endl;

    // Test various whitespace patterns
    const char* patterns[] = {"   {\"a\":1}   ", "\t\t{\"a\":1}\t\t", "\n\n{\"a\":1}\n\n",
                              "\r\n{\"a\":1}\r\n", "  \t\n\r  {\"a\":1}  \r\n\t  "};

    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
        auto result = fastjson::parse(patterns[i]);
        print_result(("Whitespace pattern " + std::to_string(i + 1)).c_str(), result.has_value());
    }
}

void test_string_scanning() {
    std::cout << "\n=== String Scanning Tests ===" << std::endl;

    // Test strings of various lengths to exercise NEON paths
    std::vector<std::string> test_strings;

    // Short strings (< 16 bytes)
    test_strings.push_back(R"({"s":"abc"})");

    // Medium strings (16-32 bytes)
    test_strings.push_back(R"({"s":"This is a test string."})");

    // Long strings (> 32 bytes)
    test_strings.push_back(
        R"({"s":"This is a much longer test string that should exercise the SIMD code paths."})");

    // Very long strings (> 64 bytes)
    test_strings.push_back(
        R"({"s":"This is an even longer string designed to test the NEON SIMD implementation with strings that span multiple 16-byte chunks to ensure proper handling."})");

    for (size_t i = 0; i < test_strings.size(); ++i) {
        auto result = fastjson::parse(test_strings[i]);
        print_result(("String length " + std::to_string(i + 1)).c_str(), result.has_value());
    }
}

void test_utf8_validation() {
    std::cout << "\n=== UTF-8 Validation Tests ===" << std::endl;

    // Valid UTF-8 sequences
    const char* valid[] = {
        R"({"text":"ASCII only"})",
        R"({"text":"Café"})",           // 2-byte UTF-8
        R"({"text":"中文"})",           // 3-byte UTF-8
        R"({"text":"😀"})",             // 4-byte UTF-8
        R"({"text":"Hello 世界 🌍"})",  // Mixed
    };

    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        auto result = fastjson::parse(valid[i]);
        print_result(("Valid UTF-8 " + std::to_string(i + 1)).c_str(), result.has_value());
    }
}

void test_error_handling() {
    std::cout << "\n=== Error Handling Tests ===" << std::endl;

    // Various invalid JSON inputs
    const char* invalid[] = {
        "{",                    // Unterminated object
        "[1,2,3",               // Unterminated array
        R"({"a":})",            // Missing value
        R"({"a":1,})",          // Trailing comma
        R"("unclosed string)",  // Unclosed string
        R"({"\u123"})",         // Incomplete Unicode escape
        R"({"\uD800"})",        // Lone surrogate
    };

    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        auto result = fastjson::parse(invalid[i]);
        print_result(("Error detection " + std::to_string(i + 1)).c_str(), !result.has_value());
    }
}

void benchmark_neon() {
    std::cout << "\n=== NEON Performance Benchmark ===" << std::endl;

    // Generate a large JSON document
    std::string json = "[";
    for (int i = 0; i < 1000; ++i) {
        if (i > 0)
            json += ",";
        json += R"({"id":)" + std::to_string(i);
        json += R"(,"name":"User )" + std::to_string(i) + R"(")";
        json += R"(,"email":"user)" + std::to_string(i) + R"(@example.com")";
        json += R"(,"active":true})";
    }
    json += "]";

    std::cout << "JSON size: " << json.size() << " bytes" << std::endl;

    // Warm-up
    for (int i = 0; i < 10; ++i) {
        auto result = fastjson::parse(json);
    }

    // Benchmark
    const int iterations = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto result = fastjson::parse(json);
        if (!result.has_value()) {
            std::cout << "Parse failed during benchmark!" << std::endl;
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time = duration.count() / (double)iterations;
    double throughput = (json.size() * iterations) / (duration.count() / 1000000.0) / (1024 * 1024);

    std::cout << "Average parse time: " << std::fixed << std::setprecision(2) << avg_time << " μs"
              << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) << throughput << " MB/s"
              << std::endl;
}

void print_simd_capabilities() {
    std::cout << "\n=== SIMD Capabilities ===" << std::endl;

#if defined(__aarch64__) || defined(_M_ARM64)
    std::cout << "Architecture: ARM64 (AArch64)" << std::endl;
    std::cout << "NEON: Available" << std::endl;

    // Try to detect additional features
    #ifdef __linux__
    std::cout << "\nOptional ARM features:" << std::endl;

        #include <asm/hwcap.h>
        #include <sys/auxv.h>

    unsigned long hwcaps = getauxval(AT_HWCAP);

        #ifdef HWCAP_ASIMD
    std::cout << "  Advanced SIMD: " << ((hwcaps & HWCAP_ASIMD) ? "Yes" : "No") << std::endl;
        #endif

        #ifdef HWCAP_ASIMDDP
    std::cout << "  Dot Product: " << ((hwcaps & HWCAP_ASIMDDP) ? "Yes" : "No") << std::endl;
        #endif

        #ifdef HWCAP_SVE
    std::cout << "  SVE: " << ((hwcaps & HWCAP_SVE) ? "Yes" : "No") << std::endl;
        #endif

        #ifdef HWCAP_I8MM
    std::cout << "  I8MM: " << ((hwcaps & HWCAP_I8MM) ? "Yes" : "No") << std::endl;
        #endif
    #endif

#elif defined(__x86_64__) || defined(_M_X64)
    std::cout << "Architecture: x86-64" << std::endl;
    std::cout << "NEON: Not available (x86 platform)" << std::endl;

#else
    std::cout << "Architecture: Unknown" << std::endl;
    std::cout << "SIMD: Unknown" << std::endl;
#endif
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   ARM NEON SIMD JSON Parser Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    print_simd_capabilities();

    test_basic_parsing();
    test_whitespace_handling();
    test_string_scanning();
    test_utf8_validation();
    test_error_handling();

#if defined(__aarch64__) || defined(_M_ARM64)
    benchmark_neon();
#else
    std::cout << "\n=== NEON Benchmark ===" << std::endl;
    std::cout << "Skipped (not running on ARM64)" << std::endl;
#endif

    std::cout << "\n========================================" << std::endl;
    std::cout << "   All tests completed!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
