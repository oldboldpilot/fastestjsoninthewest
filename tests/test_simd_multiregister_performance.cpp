#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <fstream>
#include <algorithm>
#include <numeric>

#include "../modules/fastjson_simd_api.h"
#include "../modules/fastjson_simd_multiregister.h"

namespace fastjson::simd::test {

// ============================================================================
// Test Fixtures and Utilities
// ============================================================================

class SIMDPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        context_ = SIMDContext::create();
        operations_ = std::make_unique<SIMDOperations>(
            std::make_shared<SIMDContext>(std::move(*context_)));
        scanner_ = std::make_unique<JSONScanner>(
            std::make_shared<SIMDContext>(std::move(*SIMDContext::create())));
        
        GenerateTestData();
    }

    void TearDown() override {
        // Print performance summary
        std::cout << "\n=== Performance Test Summary ===\n";
        auto stats = operations_->get_stats();
        std::cout << "Total bytes processed: " << stats.bytes_processed << "\n";
        std::cout << "Total operations: " << stats.operations_count << "\n";
        std::cout << "Throughput: " << stats.throughput_mbps() << " MB/s\n";
        std::cout << "Average time per operation: " 
                  << (stats.total_time.count() / stats.operations_count) << " ns\n";
    }

    // Benchmark helper that measures performance
    template<typename Func>
    auto BenchmarkFunction(const std::string& test_name, 
                          std::span<const char> data,
                          Func&& func, 
                          size_t iterations = 1000) -> double {
        
        std::vector<std::chrono::nanoseconds> times;
        times.reserve(iterations);
        
        // Warm up
        for (size_t i = 0; i < 10; ++i) {
            std::forward<Func>(func)();
        }
        
        // Measure
        for (size_t i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            auto result = std::forward<Func>(func)();
            auto end = std::chrono::high_resolution_clock::now();
            
            times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
            benchmark::DoNotOptimize(result);
        }
        
        // Calculate statistics
        auto total_time = std::accumulate(times.begin(), times.end(), 
                                         std::chrono::nanoseconds{0});
        auto avg_time = total_time / iterations;
        
        std::sort(times.begin(), times.end());
        auto median_time = times[times.size() / 2];
        auto p95_time = times[static_cast<size_t>(times.size() * 0.95)];
        
        double throughput_mbps = (static_cast<double>(data.size()) * 1000.0) / avg_time.count();
        
        std::cout << "\n" << test_name << " Results:\n";
        std::cout << "  Data size: " << data.size() << " bytes\n";
        std::cout << "  Iterations: " << iterations << "\n";
        std::cout << "  Average time: " << avg_time.count() << " ns\n";
        std::cout << "  Median time: " << median_time.count() << " ns\n";
        std::cout << "  95th percentile: " << p95_time.count() << " ns\n";
        std::cout << "  Throughput: " << throughput_mbps << " MB/s\n";
        
        return throughput_mbps;
    }

    void GenerateTestData() {
        std::mt19937 gen(42);
        
        // Generate whitespace-heavy data (realistic JSON)
        for (size_t size : {1024, 4096, 16384, 65536, 262144}) {
            std::string data;
            data.reserve(size);
            
            while (data.size() < size) {
                // Add realistic JSON patterns
                std::string_view patterns[] = {
                    "  {\n    \"",
                    "\": \"",
                    "\",\n    \"",
                    "\": [\n      ",
                    ",\n      ",
                    "\n    ],\n    \"",
                    "\": true,\n    \"",
                    "\": null\n  }",
                    "    \t\n\r  "
                };
                
                auto pattern = patterns[gen() % std::size(patterns)];
                if (data.size() + pattern.size() <= size) {
                    data += pattern;
                }
            }
            
            if (data.size() > size) {
                data.resize(size);
            }
            
            whitespace_test_data_[size] = std::move(data);
        }
        
        // Generate string-heavy data
        for (size_t size : {1024, 4096, 16384, 65536}) {
            std::string data;
            data.reserve(size);
            
            while (data.size() < size) {
                data += "\"This is a test string with some content and no special characters\"";
                if (data.size() < size) data += ",\n";
            }
            
            if (data.size() > size) {
                data.resize(size);
            }
            
            string_test_data_[size] = std::move(data);
        }
        
        // Generate number-heavy data
        for (size_t size : {1024, 4096, 16384, 65536}) {
            std::string data;
            data.reserve(size);
            
            std::uniform_int_distribution<> num_dist(100000, 999999);
            while (data.size() < size) {
                data += std::to_string(num_dist(gen));
                if (data.size() < size) data += ", ";
            }
            
            if (data.size() > size) {
                data.resize(size);
            }
            
            number_test_data_[size] = std::move(data);
        }
        
        // Generate structural character heavy data
        for (size_t size : {1024, 4096, 16384, 65536}) {
            std::string data;
            data.reserve(size);
            
            while (data.size() < size) {
                char structural[] = {'{', '}', '[', ']', ':', ',', ' ', '\n'};
                data += structural[gen() % std::size(structural)];
            }
            
            structural_test_data_[size] = std::move(data);
        }
        
        // Load real JSON files if available
        LoadRealJSONData();
    }
    
    void LoadRealJSONData() {
        std::vector<std::string> test_files = {
            "test_data/canada.json",
            "test_data/twitter.json",
            "test_data/citm_catalog.json",
            "test_data/sample.json"
        };
        
        for (const auto& filename : test_files) {
            std::ifstream file(filename);
            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                if (!content.empty()) {
                    real_json_data_[filename] = std::move(content);
                }
            }
        }
    }

protected:
    std::unique_ptr<SIMDContext> context_;
    std::unique_ptr<SIMDOperations> operations_;
    std::unique_ptr<JSONScanner> scanner_;
    
    std::unordered_map<size_t, std::string> whitespace_test_data_;
    std::unordered_map<size_t, std::string> string_test_data_;
    std::unordered_map<size_t, std::string> number_test_data_;
    std::unordered_map<size_t, std::string> structural_test_data_;
    std::unordered_map<std::string, std::string> real_json_data_;
};

// ============================================================================
// Correctness Tests
// ============================================================================

TEST_F(SIMDPerformanceTest, WhitespaceSkippingCorrectness) {
    std::string test_data = "   \t\n\r    hello world";
    
    // Test scalar implementation
    size_t scalar_result = 0;
    while (scalar_result < test_data.size() && 
           (test_data[scalar_result] == ' ' || test_data[scalar_result] == '\t' ||
            test_data[scalar_result] == '\n' || test_data[scalar_result] == '\r')) {
        scalar_result++;
    }
    
    // Test SIMD implementations
    size_t simd_result = operations_->skip_whitespace({test_data.data(), test_data.size()});
    size_t multi_avx2_result = multi::skip_whitespace_4x_avx2(test_data.data(), test_data.size(), 0);
    
    EXPECT_EQ(scalar_result, simd_result);
    EXPECT_EQ(scalar_result, multi_avx2_result);
    EXPECT_EQ(scalar_result, 9);  // Expected position of 'h'
}

TEST_F(SIMDPerformanceTest, StringScanningCorrectness) {
    std::string test_data = "This is a test string with \"quoted content\" and more";
    size_t start_pos = test_data.find('"') + 1;  // Start after opening quote
    
    // Test scalar implementation
    size_t scalar_result = start_pos;
    while (scalar_result < test_data.size()) {
        char c = test_data[scalar_result];
        if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
            break;
        }
        scalar_result++;
    }
    
    // Test SIMD implementations
    size_t simd_result = operations_->find_string_end({test_data.data(), test_data.size()}, start_pos);
    size_t multi_result = multi::find_string_end_4x_avx2(test_data.data(), test_data.size(), start_pos);
    
    EXPECT_EQ(scalar_result, simd_result);
    EXPECT_EQ(scalar_result, multi_result);
}

TEST_F(SIMDPerformanceTest, NumberValidationCorrectness) {
    std::vector<std::string> valid_numbers = {
        "123", "-456", "78.90", "1.23e-45", "-6.78E+9", "0", "0.0"
    };
    
    std::vector<std::string> invalid_numbers = {
        "123abc", "12.34.56", "1e", "++123", "--456"
    };
    
    for (const auto& num : valid_numbers) {
        EXPECT_TRUE(operations_->validate_number_chars({num.data(), num.size()}))
            << "Number should be valid: " << num;
        EXPECT_TRUE(multi::validate_number_chars_4x_avx2(num.data(), 0, num.size()))
            << "Multi-register validation failed for: " << num;
    }
    
    for (const auto& num : invalid_numbers) {
        EXPECT_FALSE(operations_->validate_number_chars({num.data(), num.size()}))
            << "Number should be invalid: " << num;
        EXPECT_FALSE(multi::validate_number_chars_4x_avx2(num.data(), 0, num.size()))
            << "Multi-register validation should reject: " << num;
    }
}

TEST_F(SIMDPerformanceTest, StructuralCharacterDetection) {
    std::string test_data = R"({"key": [1, 2, 3], "nested": {"inner": true}})";
    
    auto structural_chars = operations_->find_structural_chars({test_data.data(), test_data.size()});
    
    // Expected structural characters: { " : [ , , ] , " : { " : } }
    EXPECT_GT(structural_chars.count, 10);
    
    // Verify positions and types
    bool found_open_brace = false;
    bool found_colon = false;
    bool found_open_bracket = false;
    
    for (size_t i = 0; i < structural_chars.count; ++i) {
        char c = test_data[structural_chars.positions[i]];
        uint8_t expected_type = 0;
        
        switch (c) {
            case '{': expected_type = 1; found_open_brace = true; break;
            case '}': expected_type = 2; break;
            case '[': expected_type = 3; found_open_bracket = true; break;
            case ']': expected_type = 4; break;
            case ':': expected_type = 5; found_colon = true; break;
            case ',': expected_type = 6; break;
        }
        
        EXPECT_EQ(structural_chars.types[i], expected_type)
            << "Type mismatch for character '" << c << "' at position " << structural_chars.positions[i];
    }
    
    EXPECT_TRUE(found_open_brace);
    EXPECT_TRUE(found_colon);
    EXPECT_TRUE(found_open_bracket);
}

// ============================================================================
// Performance Regression Tests
// ============================================================================

TEST_F(SIMDPerformanceTest, WhitespaceSkippingPerformance) {
    for (auto& [size, data] : whitespace_test_data_) {
        std::span<const char> data_span{data.data(), data.size()};
        
        // Benchmark scalar implementation
        auto scalar_throughput = BenchmarkFunction(
            "Whitespace Scalar " + std::to_string(size) + "B",
            data_span,
            [&]() {
                size_t pos = 0;
                while (pos < data.size() && 
                       (data[pos] == ' ' || data[pos] == '\t' || 
                        data[pos] == '\n' || data[pos] == '\r')) {
                    pos++;
                }
                return pos;
            }
        );
        
        // Benchmark single-register SIMD
        auto single_simd_throughput = BenchmarkFunction(
            "Whitespace Single SIMD " + std::to_string(size) + "B",
            data_span,
            [&]() {
                return multi::skip_whitespace_multi(data.data(), data.size(), 0);
            }
        );
        
        // Benchmark multi-register SIMD
        auto multi_simd_throughput = BenchmarkFunction(
            "Whitespace Multi SIMD " + std::to_string(size) + "B",
            data_span,
            [&]() {
                return operations_->skip_whitespace(data_span);
            }
        );
        
        // Performance assertions - multi-register should be faster
        EXPECT_GT(multi_simd_throughput, scalar_throughput * 1.2)  // At least 20% improvement
            << "Multi-register SIMD should be significantly faster than scalar for size " << size;
        
        if (context_->supports_multi_register()) {
            EXPECT_GT(multi_simd_throughput, single_simd_throughput * 1.1)  // At least 10% improvement
                << "Multi-register should be faster than single-register for size " << size;
        }
        
        std::cout << "  Speedup vs scalar: " << (multi_simd_throughput / scalar_throughput) << "x\n";
        std::cout << "  Speedup vs single SIMD: " << (multi_simd_throughput / single_simd_throughput) << "x\n";
    }
}

TEST_F(SIMDPerformanceTest, StringScanningPerformance) {
    for (auto& [size, data] : string_test_data_) {
        // Add a quote at the end to ensure termination
        std::string test_data = data + "\"";
        std::span<const char> data_span{test_data.data(), test_data.size()};
        
        auto scalar_throughput = BenchmarkFunction(
            "String Scan Scalar " + std::to_string(size) + "B",
            data_span,
            [&]() {
                size_t pos = 0;
                while (pos < test_data.size()) {
                    char c = test_data[pos];
                    if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
                        break;
                    }
                    pos++;
                }
                return pos;
            }
        );
        
        auto multi_simd_throughput = BenchmarkFunction(
            "String Scan Multi SIMD " + std::to_string(size) + "B",
            data_span,
            [&]() {
                return operations_->find_string_end(data_span);
            }
        );
        
        EXPECT_GT(multi_simd_throughput, scalar_throughput * 1.5)
            << "Multi-register string scanning should be significantly faster for size " << size;
        
        std::cout << "  Speedup vs scalar: " << (multi_simd_throughput / scalar_throughput) << "x\n";
    }
}

TEST_F(SIMDPerformanceTest, NumberValidationPerformance) {
    for (auto& [size, data] : number_test_data_) {
        std::span<const char> data_span{data.data(), data.size()};
        
        auto scalar_throughput = BenchmarkFunction(
            "Number Validation Scalar " + std::to_string(size) + "B",
            data_span,
            [&]() {
                for (size_t i = 0; i < data.size(); ++i) {
                    char c = data[i];
                    if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || 
                          c == '.' || c == 'e' || c == 'E' || c == ' ')) {
                        return false;
                    }
                }
                return true;
            }
        );
        
        auto multi_simd_throughput = BenchmarkFunction(
            "Number Validation Multi SIMD " + std::to_string(size) + "B",
            data_span,
            [&]() {
                return operations_->validate_number_chars(data_span);
            }
        );
        
        EXPECT_GT(multi_simd_throughput, scalar_throughput * 1.3)
            << "Multi-register number validation should be faster for size " << size;
        
        std::cout << "  Speedup vs scalar: " << (multi_simd_throughput / scalar_throughput) << "x\n";
    }
}

TEST_F(SIMDPerformanceTest, StructuralCharacterPerformance) {
    for (auto& [size, data] : structural_test_data_) {
        std::span<const char> data_span{data.data(), data.size()};
        
        auto scalar_throughput = BenchmarkFunction(
            "Structural Chars Scalar " + std::to_string(size) + "B",
            data_span,
            [&]() {
                size_t count = 0;
                for (size_t i = 0; i < data.size(); ++i) {
                    char c = data[i];
                    if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',') {
                        count++;
                    }
                }
                return count;
            }
        );
        
        auto multi_simd_throughput = BenchmarkFunction(
            "Structural Chars Multi SIMD " + std::to_string(size) + "B",
            data_span,
            [&]() {
                auto result = operations_->find_structural_chars(data_span);
                return result.count;
            }
        );
        
        EXPECT_GT(multi_simd_throughput, scalar_throughput * 1.4)
            << "Multi-register structural detection should be faster for size " << size;
        
        std::cout << "  Speedup vs scalar: " << (multi_simd_throughput / scalar_throughput) << "x\n";
    }
}

TEST_F(SIMDPerformanceTest, EndToEndJSONScanningPerformance) {
    // Test with synthetic JSON data
    for (auto& [size, data] : structural_test_data_) {
        std::span<const char> data_span{data.data(), data.size()};
        
        auto baseline_throughput = BenchmarkFunction(
            "JSON Scan Baseline " + std::to_string(size) + "B",
            data_span,
            [&]() {
                auto baseline_scanner = factory::create_scanner();
                baseline_scanner.reset_stats();
                
                // Simulate baseline scanning without multi-register optimizations
                auto tokens = baseline_scanner.scan_tokens(data_span);
                return tokens.size();
            }
        );
        
        auto optimized_throughput = BenchmarkFunction(
            "JSON Scan Optimized " + std::to_string(size) + "B",
            data_span,
            [&]() {
                scanner_->reset_stats();
                auto tokens = scanner_->scan_tokens(data_span);
                return tokens.size();
            }
        );
        
        EXPECT_GT(optimized_throughput, baseline_throughput * 1.2)
            << "Optimized JSON scanning should be faster for size " << size;
        
        std::cout << "  End-to-end speedup: " << (optimized_throughput / baseline_throughput) << "x\n";
    }
}

// ============================================================================
// Real-World JSON Performance Tests
// ============================================================================

TEST_F(SIMDPerformanceTest, RealWorldJSONPerformance) {
    for (auto& [filename, content] : real_json_data_) {
        if (content.size() < 1000) continue;  // Skip small files
        
        std::span<const char> data_span{content.data(), content.size()};
        
        std::cout << "\n=== Real JSON File: " << filename << " (" << content.size() << " bytes) ===\n";
        
        auto baseline_throughput = BenchmarkFunction(
            "Real JSON Baseline " + filename,
            data_span,
            [&]() {
                // Simulate traditional parsing without multi-register SIMD
                size_t operations = 0;
                size_t pos = 0;
                
                while (pos < content.size()) {
                    // Scalar whitespace skipping
                    while (pos < content.size() && 
                           (content[pos] == ' ' || content[pos] == '\t' || 
                            content[pos] == '\n' || content[pos] == '\r')) {
                        pos++;
                    }
                    
                    if (pos < content.size()) {
                        pos++;
                        operations++;
                    }
                }
                
                return operations;
            },
            100  // Fewer iterations for large files
        );
        
        auto optimized_throughput = BenchmarkFunction(
            "Real JSON Optimized " + filename,
            data_span,
            [&]() {
                auto tokens = scanner_->scan_tokens(data_span);
                return tokens.size();
            },
            100
        );
        
        EXPECT_GT(optimized_throughput, baseline_throughput * 1.15)
            << "Real-world JSON should show performance improvement: " << filename;
        
        std::cout << "  Real-world speedup: " << (optimized_throughput / baseline_throughput) << "x\n";
        
        // Additional metrics for real JSON
        auto scanner_stats = scanner_->get_operations_stats();
        std::cout << "  Scanner throughput: " << scanner_stats.throughput_mbps() << " MB/s\n";
        std::cout << "  Operations per byte: " 
                  << (static_cast<double>(scanner_stats.operations_count) / content.size()) << "\n";
    }
}

// ============================================================================
// Memory Usage and Cache Performance Tests
// ============================================================================

TEST_F(SIMDPerformanceTest, CachePerformanceTest) {
    // Test with data sizes that exceed L1, L2, L3 caches
    std::vector<size_t> cache_test_sizes = {
        32 * 1024,    // 32KB - L1 cache size
        256 * 1024,   // 256KB - L2 cache size  
        8 * 1024 * 1024,  // 8MB - L3 cache size
        64 * 1024 * 1024  // 64MB - exceeds L3
    };
    
    for (size_t size : cache_test_sizes) {
        // Generate test data of specific size
        std::string large_data;
        large_data.reserve(size);
        
        std::mt19937 gen(42);
        while (large_data.size() < size) {
            large_data += "  { \"key\": \"value\",\n    \"number\": 12345 },\n";
        }
        large_data.resize(size);
        
        std::span<const char> data_span{large_data.data(), large_data.size()};
        
        auto single_register_time = BenchmarkFunction(
            "Cache Test Single " + std::to_string(size / 1024) + "KB",
            data_span,
            [&]() {
                return multi::skip_whitespace_multi(large_data.data(), large_data.size(), 0);
            },
            50  // Fewer iterations for large data
        );
        
        auto multi_register_time = BenchmarkFunction(
            "Cache Test Multi " + std::to_string(size / 1024) + "KB", 
            data_span,
            [&]() {
                return operations_->skip_whitespace(data_span);
            },
            50
        );
        
        std::cout << "  Cache performance ratio (multi/single): " 
                  << (multi_register_time / single_register_time) << "x\n";
        
        // Multi-register should maintain performance advantage even with cache pressure
        EXPECT_GT(multi_register_time, single_register_time * 0.9)
            << "Multi-register should maintain performance under cache pressure";
    }
}

} // namespace fastjson::simd::test