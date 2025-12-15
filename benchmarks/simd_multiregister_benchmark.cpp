#include <benchmark/benchmark.h>
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>

#include "../modules/fastjson_simd_multiregister.h"

// ============================================================================
// Test Data Generation
// ============================================================================

class SIMDBenchmarkFixture : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) override {
        // Generate various test data sizes
        GenerateWhitespaceData();
        GenerateStringData();
        GenerateNumberData();
        GenerateJSONData();
        GenerateStructuralData();
    }

private:
    void GenerateWhitespaceData() {
        std::mt19937 gen(42);
        std::uniform_int_distribution<> ws_dist(0, 3);
        
        // Create data with varying whitespace density
        for (size_t size : {1024, 4096, 16384, 65536, 262144, 1048576}) {
            std::string data;
            data.reserve(size);
            
            // 70% whitespace, 30% non-whitespace
            for (size_t i = 0; i < size; ++i) {
                if (gen() % 10 < 7) {
                    char ws_chars[] = {' ', '\t', '\n', '\r'};
                    data += ws_chars[ws_dist(gen)];
                } else {
                    data += 'a';  // Non-whitespace
                }
            }
            
            whitespace_data_[size] = std::move(data);
        }
        
        // Create data with runs of whitespace (realistic JSON)
        for (size_t size : {1024, 4096, 16384, 65536}) {
            std::string data;
            data.reserve(size);
            
            while (data.size() < size) {
                // Add run of whitespace
                size_t ws_run = gen() % 20 + 1;
                for (size_t i = 0; i < ws_run && data.size() < size; ++i) {
                    char ws_chars[] = {' ', '\t', '\n', '\r'};
                    data += ws_chars[ws_dist(gen)];
                }
                
                // Add non-whitespace content
                if (data.size() < size) {
                    data += '{';
                }
            }
            
            whitespace_runs_data_[size] = std::move(data);
        }
    }
    
    void GenerateStringData() {
        std::mt19937 gen(42);
        
        for (size_t size : {1024, 4096, 16384, 65536, 262144}) {
            std::string data;
            data.reserve(size);
            
            // Generate string content with occasional special characters
            for (size_t i = 0; i < size; ++i) {
                if (gen() % 100 < 2) {  // 2% special characters
                    char special[] = {'"', '\\', '\n', '\r', '\t'};
                    data += special[gen() % 5];
                } else {
                    // Regular printable ASCII
                    data += static_cast<char>('a' + (gen() % 26));
                }
            }
            
            string_data_[size] = std::move(data);
        }
        
        // Generate string with no special characters (worst case)
        for (size_t size : {1024, 4096, 16384, 65536}) {
            std::string data(size, 'a');
            string_clean_data_[size] = std::move(data);
        }
    }
    
    void GenerateNumberData() {
        std::mt19937 gen(42);
        
        for (size_t size : {1024, 4096, 16384, 65536}) {
            std::string data;
            data.reserve(size);
            
            // Generate valid number characters
            while (data.size() < size) {
                char num_chars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                   '-', '+', '.', 'e', 'E'};
                data += num_chars[gen() % 15];
            }
            
            number_data_[size] = std::move(data);
        }
    }
    
    void GenerateJSONData() {
        // Load real JSON files for realistic testing
        std::vector<std::string> json_files = {
            "canada.json",
            "twitter.json", 
            "citm_catalog.json",
            "large-file.json"
        };
        
        for (const auto& filename : json_files) {
            std::ifstream file("test_data/" + filename);
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                real_json_data_[filename] = std::move(content);
            }
        }
        
        // Generate synthetic JSON with structural characters
        for (size_t size : {1024, 4096, 16384, 65536}) {
            std::string data;
            data.reserve(size);
            
            while (data.size() < size) {
                char structural[] = {'{', '}', '[', ']', ':', ',', ' ', '"'};
                data += structural[gen() % 8];
            }
            
            synthetic_json_data_[size] = std::move(data);
        }
    }
    
    void GenerateStructuralData() {
        std::mt19937 gen(42);
        
        for (size_t size : {1024, 4096, 16384, 65536}) {
            std::string data;
            data.reserve(size);
            
            // 20% structural characters, 80% other content
            for (size_t i = 0; i < size; ++i) {
                if (gen() % 10 < 2) {
                    char structural[] = {'{', '}', '[', ']', ':', ','};
                    data += structural[gen() % 6];
                } else {
                    data += static_cast<char>('a' + (gen() % 26));
                }
            }
            
            structural_data_[size] = std::move(data);
        }
    }

protected:
    std::unordered_map<size_t, std::string> whitespace_data_;
    std::unordered_map<size_t, std::string> whitespace_runs_data_;
    std::unordered_map<size_t, std::string> string_data_;
    std::unordered_map<size_t, std::string> string_clean_data_;
    std::unordered_map<size_t, std::string> number_data_;
    std::unordered_map<size_t, std::string> structural_data_;
    std::unordered_map<std::string, std::string> real_json_data_;
    std::unordered_map<size_t, std::string> synthetic_json_data_;
    std::mt19937 gen{42};
};

// ============================================================================
// Whitespace Skipping Benchmarks
// ============================================================================

// Test scalar vs single-register vs multi-register whitespace skipping
static void BM_WhitespaceSkip_Scalar(benchmark::State& state) {
    std::string data(state.range(0), ' ');
    // Add non-whitespace at the end
    data[data.size() - 1] = 'a';
    
    for (auto _ : state) {
        size_t pos = 0;
        while (pos < data.size() && (data[pos] == ' ' || data[pos] == '\t' || 
                                    data[pos] == '\n' || data[pos] == '\r')) {
            pos++;
        }
        benchmark::DoNotOptimize(pos);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
    state.SetItemsProcessed(state.iterations());
}

static void BM_WhitespaceSkip_SingleAVX2(benchmark::State& state) {
    std::string data(state.range(0), ' ');
    data[data.size() - 1] = 'a';  // Ensure we find non-whitespace
    
    // Use existing single-register AVX2 implementation
    for (auto _ : state) {
        // This would call the original single-register version
        size_t result = fastjson::simd::multi::skip_whitespace_multi(data.data(), data.size(), 0);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
    state.SetItemsProcessed(state.iterations());
}

static void BM_WhitespaceSkip_MultiAVX2(benchmark::State& state) {
    std::string data(state.range(0), ' ');
    data[data.size() - 1] = 'a';
    
    for (auto _ : state) {
        size_t result = fastjson::simd::multi::skip_whitespace_4x_avx2(data.data(), data.size(), 0);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
    state.SetItemsProcessed(state.iterations());
}

static void BM_WhitespaceSkip_MultiAVX512(benchmark::State& state) {
    std::string data(state.range(0), ' ');
    data[data.size() - 1] = 'a';
    
    for (auto _ : state) {
        size_t result = fastjson::simd::multi::skip_whitespace_8x_avx512(data.data(), data.size(), 0);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
    state.SetItemsProcessed(state.iterations());
}

// ============================================================================
// String Scanning Benchmarks
// ============================================================================

static void BM_StringScan_Scalar(benchmark::State& state) {
    std::string data(state.range(0), 'a');
    data[data.size() - 1] = '"';  // Ensure we find quote
    
    for (auto _ : state) {
        size_t pos = 0;
        while (pos < data.size()) {
            char c = data[pos];
            if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
                break;
            }
            pos++;
        }
        benchmark::DoNotOptimize(pos);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}

static void BM_StringScan_MultiAVX2(benchmark::State& state) {
    std::string data(state.range(0), 'a');
    data[data.size() - 1] = '"';
    
    for (auto _ : state) {
        size_t result = fastjson::simd::multi::find_string_end_4x_avx2(data.data(), data.size(), 0);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}

static void BM_StringScan_MultiAVX512(benchmark::State& state) {
    std::string data(state.range(0), 'a');
    data[data.size() - 1] = '"';
    
    for (auto _ : state) {
        size_t result = fastjson::simd::multi::find_string_end_8x_avx512(data.data(), data.size(), 0);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}

// ============================================================================
// Number Validation Benchmarks
// ============================================================================

static void BM_NumberValidation_Scalar(benchmark::State& state) {
    std::string data;
    for (size_t i = 0; i < state.range(0); ++i) {
        data += '0' + (i % 10);
    }
    
    for (auto _ : state) {
        bool valid = true;
        for (size_t i = 0; i < data.size(); ++i) {
            char c = data[i];
            if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')) {
                valid = false;
                break;
            }
        }
        benchmark::DoNotOptimize(valid);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}

static void BM_NumberValidation_MultiAVX2(benchmark::State& state) {
    std::string data;
    for (size_t i = 0; i < state.range(0); ++i) {
        data += '0' + (i % 10);
    }
    
    for (auto _ : state) {
        bool result = fastjson::simd::multi::validate_number_chars_4x_avx2(data.data(), 0, data.size());
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}

// ============================================================================
// Structural Character Detection Benchmarks
// ============================================================================

static void BM_StructuralChars_Scalar(benchmark::State& state) {
    std::string data;
    for (size_t i = 0; i < state.range(0); ++i) {
        char chars[] = {'a', 'b', 'c', '{', '}', '[', ']', ':', ','};
        data += chars[i % 9];
    }
    
    for (auto _ : state) {
        std::vector<size_t> positions;
        for (size_t i = 0; i < data.size(); ++i) {
            char c = data[i];
            if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',') {
                positions.push_back(i);
            }
        }
        benchmark::DoNotOptimize(positions.size());
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}

static void BM_StructuralChars_MultiAVX2(benchmark::State& state) {
    std::string data;
    for (size_t i = 0; i < state.range(0); ++i) {
        char chars[] = {'a', 'b', 'c', '{', '}', '[', ']', ':', ','};
        data += chars[i % 9];
    }
    
    for (auto _ : state) {
        fastjson::simd::multi::StructuralChars result;
        fastjson::simd::multi::find_structural_chars_4x_avx2(data.data(), data.size(), 0, result);
        benchmark::DoNotOptimize(result.count);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}

// ============================================================================
// End-to-End JSON Parsing Benchmarks  
// ============================================================================

BENCHMARK_DEFINE_F(SIMDBenchmarkFixture, FullJSONParse_Baseline)(benchmark::State& state) {
    const auto& json_data = synthetic_json_data_[state.range(0)];
    
    for (auto _ : state) {
        // Simulate full JSON parsing with existing single-register SIMD
        size_t pos = 0;
        size_t total_operations = 0;
        
        while (pos < json_data.size()) {
            // Whitespace skip
            pos = fastjson::simd::multi::skip_whitespace_multi(json_data.data(), json_data.size(), pos);
            if (pos >= json_data.size()) break;
            
            // Character classification
            char c = json_data[pos];
            if (c == '"') {
                // String scanning
                pos = fastjson::simd::multi::find_string_end_multi(json_data.data(), json_data.size(), pos + 1);
            } else if (c >= '0' && c <= '9' || c == '-') {
                // Number scanning (simplified)
                size_t start = pos;
                while (pos < json_data.size() && 
                       ((json_data[pos] >= '0' && json_data[pos] <= '9') || 
                        json_data[pos] == '.' || json_data[pos] == '-' || 
                        json_data[pos] == '+' || json_data[pos] == 'e' || json_data[pos] == 'E')) {
                    pos++;
                }
                fastjson::simd::multi::validate_number_chars_multi(json_data.data(), start, pos);
            } else {
                pos++;
            }
            total_operations++;
        }
        
        benchmark::DoNotOptimize(total_operations);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}

BENCHMARK_DEFINE_F(SIMDBenchmarkFixture, FullJSONParse_MultiRegister)(benchmark::State& state) {
    const auto& json_data = synthetic_json_data_[state.range(0)];
    
    for (auto _ : state) {
        // Simulate full JSON parsing with multi-register SIMD
        size_t pos = 0;
        size_t total_operations = 0;
        
        while (pos < json_data.size()) {
            // Multi-register whitespace skip
            pos = fastjson::simd::multi::skip_whitespace_4x_avx2(json_data.data(), json_data.size(), pos);
            if (pos >= json_data.size()) break;
            
            // Character classification with structural detection
            fastjson::simd::multi::StructuralChars structural;
            size_t new_pos = fastjson::simd::multi::find_structural_chars_4x_avx2(
                json_data.data(), json_data.size(), pos, structural);
            
            if (structural.count > 0) {
                pos = structural.positions[0] + 1;
            } else {
                char c = json_data[pos];
                if (c == '"') {
                    pos = fastjson::simd::multi::find_string_end_4x_avx2(json_data.data(), json_data.size(), pos + 1);
                } else if (c >= '0' && c <= '9' || c == '-') {
                    size_t start = pos;
                    while (pos < json_data.size() && 
                           ((json_data[pos] >= '0' && json_data[pos] <= '9') || 
                            json_data[pos] == '.' || json_data[pos] == '-' || 
                            json_data[pos] == '+' || json_data[pos] == 'e' || json_data[pos] == 'E')) {
                        pos++;
                    }
                    fastjson::simd::multi::validate_number_chars_4x_avx2(json_data.data(), start, pos);
                } else {
                    pos++;
                }
            }
            total_operations++;
        }
        
        benchmark::DoNotOptimize(total_operations);
    }
    
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}

// ============================================================================
// Register Benchmark Suites
// ============================================================================

// Whitespace skipping benchmarks
BENCHMARK(BM_WhitespaceSkip_Scalar)->Range(1024, 1048576)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_WhitespaceSkip_SingleAVX2)->Range(1024, 1048576)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_WhitespaceSkip_MultiAVX2)->Range(1024, 1048576)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_WhitespaceSkip_MultiAVX512)->Range(1024, 1048576)->Unit(benchmark::kMicrosecond);

// String scanning benchmarks
BENCHMARK(BM_StringScan_Scalar)->Range(1024, 262144)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_StringScan_MultiAVX2)->Range(1024, 262144)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_StringScan_MultiAVX512)->Range(1024, 262144)->Unit(benchmark::kMicrosecond);

// Number validation benchmarks
BENCHMARK(BM_NumberValidation_Scalar)->Range(1024, 65536)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_NumberValidation_MultiAVX2)->Range(1024, 65536)->Unit(benchmark::kMicrosecond);

// Structural character detection benchmarks
BENCHMARK(BM_StructuralChars_Scalar)->Range(1024, 65536)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_StructuralChars_MultiAVX2)->Range(1024, 65536)->Unit(benchmark::kMicrosecond);

// End-to-end JSON parsing benchmarks
BENCHMARK_REGISTER_F(SIMDBenchmarkFixture, FullJSONParse_Baseline)->Range(1024, 65536)->Unit(benchmark::kMicrosecond);
BENCHMARK_REGISTER_F(SIMDBenchmarkFixture, FullJSONParse_MultiRegister)->Range(1024, 65536)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();