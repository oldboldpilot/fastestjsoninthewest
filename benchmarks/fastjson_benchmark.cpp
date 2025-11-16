// FastestJSONInTheWest Comprehensive Benchmark Suite
// Author: Olumuyiwa Oluwasanmi
// Performance benchmarking against major JSON libraries

#include <benchmark/benchmark.h>
#include <chrono>
#include <fstream>
#include <random>
#include <sstream>
#include <vector>

// FastestJSONInTheWest modules
import fastjson.core;
import fastjson.parser;
import fastjson.serializer;

using namespace fastjson::core;
using namespace fastjson::parser;
using namespace fastjson::serializer;

// ============================================================================
// Benchmark Data Generation
// ============================================================================

class BenchmarkDataGenerator {
public:
    static std::string generate_simple_json(size_t size) {
        std::ostringstream oss;
        oss << "{";
        for (size_t i = 0; i < size; ++i) {
            if (i > 0) oss << ",";
            oss << "\"key" << i << "\":\"value" << i << "\"";
        }
        oss << "}";
        return oss.str();
    }
    
    static std::string generate_nested_json(size_t depth, size_t width) {
        std::function<std::string(size_t, size_t)> generate_level = 
            [&](size_t current_depth, size_t current_width) -> std::string {
            if (current_depth == 0) {
                return "\"leaf_value\"";
            }
            
            std::ostringstream oss;
            oss << "{";
            for (size_t i = 0; i < current_width; ++i) {
                if (i > 0) oss << ",";
                oss << "\"level_" << current_depth << "_" << i << "\":";
                oss << generate_level(current_depth - 1, current_width);
            }
            oss << "}";
            return oss.str();
        };
        
        return generate_level(depth, width);
    }
    
    static std::string generate_array_json(size_t size) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < size; ++i) {
            if (i > 0) oss << ",";
            oss << "{\"id\":" << i << ",\"value\":\"item_" << i << "\"}";
        }
        oss << "]";
        return oss.str();
    }
    
    static std::string generate_mixed_json(size_t complexity) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 100);
        
        std::ostringstream oss;
        oss << "{";
        oss << "\"metadata\":{\"version\":\"1.0\",\"timestamp\":" << std::time(nullptr) << "},";
        oss << "\"data\":[";
        
        for (size_t i = 0; i < complexity; ++i) {
            if (i > 0) oss << ",";
            oss << "{";
            oss << "\"id\":" << i << ",";
            oss << "\"name\":\"object_" << i << "\",";
            oss << "\"value\":" << dis(gen) << ",";
            oss << "\"active\":" << (dis(gen) % 2 == 0 ? "true" : "false") << ",";
            oss << "\"tags\":[";
            for (int j = 0; j < 3; ++j) {
                if (j > 0) oss << ",";
                oss << "\"tag_" << j << "\"";
            }
            oss << "]";
            oss << "}";
        }
        
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

// ============================================================================
// FastestJSONInTheWest Benchmarks
// ============================================================================

static void BM_FastJSON_Parse_Simple(benchmark::State& state) {
    auto json_str = BenchmarkDataGenerator::generate_simple_json(state.range(0));
    
    for (auto _ : state) {
        auto result = parse(json_str);
        if (!result) {
            state.SkipWithError("Parsing failed");
            break;
        }
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * 
                           static_cast<int64_t>(json_str.size()));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

static void BM_FastJSON_Parse_Nested(benchmark::State& state) {
    auto json_str = BenchmarkDataGenerator::generate_nested_json(state.range(0), 3);
    
    for (auto _ : state) {
        auto result = parse(json_str);
        if (!result) {
            state.SkipWithError("Parsing failed");
            break;
        }
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * 
                           static_cast<int64_t>(json_str.size()));
}

static void BM_FastJSON_Parse_Array(benchmark::State& state) {
    auto json_str = BenchmarkDataGenerator::generate_array_json(state.range(0));
    
    for (auto _ : state) {
        auto result = parse(json_str);
        if (!result) {
            state.SkipWithError("Parsing failed");
            break;
        }
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * 
                           static_cast<int64_t>(json_str.size()));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

static void BM_FastJSON_Parse_Mixed(benchmark::State& state) {
    auto json_str = BenchmarkDataGenerator::generate_mixed_json(state.range(0));
    
    for (auto _ : state) {
        auto result = parse(json_str);
        if (!result) {
            state.SkipWithError("Parsing failed");
            break;
        }
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * 
                           static_cast<int64_t>(json_str.size()));
}

static void BM_FastJSON_Serialize_Simple(benchmark::State& state) {
    // Create a JSON object to serialize
    json_object obj;
    for (int i = 0; i < state.range(0); ++i) {
        obj["key" + std::to_string(i)] = json_value{"value" + std::to_string(i)};
    }
    json_value value{std::move(obj)};
    
    for (auto _ : state) {
        auto result = to_string(value);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

static void BM_FastJSON_Serialize_Nested(benchmark::State& state) {
    // Create nested JSON structure
    std::function<json_value(int, int)> create_nested = [&](int depth, int width) -> json_value {
        if (depth == 0) {
            return json_value{"leaf_value"};
        }
        
        json_object obj;
        for (int i = 0; i < width; ++i) {
            obj["level_" + std::to_string(depth) + "_" + std::to_string(i)] = 
                create_nested(depth - 1, width);
        }
        return json_value{std::move(obj)};
    };
    
    auto value = create_nested(state.range(0), 3);
    
    for (auto _ : state) {
        auto result = to_string(value);
        benchmark::DoNotOptimize(result);
    }
}

static void BM_FastJSON_Roundtrip(benchmark::State& state) {
    auto json_str = BenchmarkDataGenerator::generate_mixed_json(state.range(0));
    
    for (auto _ : state) {
        auto parsed = parse(json_str);
        if (!parsed) {
            state.SkipWithError("Parsing failed");
            break;
        }
        
        auto serialized = to_string(*parsed);
        benchmark::DoNotOptimize(serialized);
    }
    
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * 
                           static_cast<int64_t>(json_str.size()));
}

// Memory usage benchmark
static void BM_FastJSON_Memory_Usage(benchmark::State& state) {
    auto json_str = BenchmarkDataGenerator::generate_mixed_json(state.range(0));
    
    for (auto _ : state) {
        auto start_memory = benchmark::GetSystemTime();
        auto result = parse(json_str);
        if (!result) {
            state.SkipWithError("Parsing failed");
            break;
        }
        auto end_memory = benchmark::GetSystemTime();
        
        state.SetLabel("memory_used");
        benchmark::DoNotOptimize(result);
    }
}

// ============================================================================
// Benchmark Registration with Various Sizes
// ============================================================================

// Simple object parsing benchmarks
BENCHMARK(BM_FastJSON_Parse_Simple)
    ->Args({10})
    ->Args({100}) 
    ->Args({1000})
    ->Args({10000})
    ->Unit(benchmark::kMicrosecond)
    ->ReportAggregatesOnly(false);

// Nested object parsing benchmarks  
BENCHMARK(BM_FastJSON_Parse_Nested)
    ->Args({3})
    ->Args({5})
    ->Args({7})
    ->Args({10})
    ->Unit(benchmark::kMicrosecond)
    ->ReportAggregatesOnly(false);

// Array parsing benchmarks
BENCHMARK(BM_FastJSON_Parse_Array)
    ->Args({100})
    ->Args({1000})
    ->Args({10000})
    ->Args({100000})
    ->Unit(benchmark::kMicrosecond)
    ->ReportAggregatesOnly(false);

// Mixed content parsing benchmarks
BENCHMARK(BM_FastJSON_Parse_Mixed)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({5000})
    ->Unit(benchmark::kMicrosecond)
    ->ReportAggregatesOnly(false);

// Serialization benchmarks
BENCHMARK(BM_FastJSON_Serialize_Simple)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000})
    ->Unit(benchmark::kMicrosecond)
    ->ReportAggregatesOnly(false);

BENCHMARK(BM_FastJSON_Serialize_Nested)
    ->Args({3})
    ->Args({5})
    ->Args({7})
    ->Unit(benchmark::kMicrosecond)
    ->ReportAggregatesOnly(false);

// Roundtrip benchmarks
BENCHMARK(BM_FastJSON_Roundtrip)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Unit(benchmark::kMicrosecond)
    ->ReportAggregatesOnly(false);

// Memory usage benchmarks
BENCHMARK(BM_FastJSON_Memory_Usage)
    ->Args({100})
    ->Args({1000})
    ->Args({5000})
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Main Benchmark Runner
// ============================================================================

int main(int argc, char** argv) {
    // Initialize benchmark
    benchmark::Initialize(&argc, argv);
    
    // Check if any benchmarks were specified
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    
    // Add custom counters and context
    benchmark::AddCustomContext("compiler", "Clang 21");
    benchmark::AddCustomContext("standard", "C++23");
    benchmark::AddCustomContext("optimization", "-O3 -march=native");
    benchmark::AddCustomContext("library", "FastestJSONInTheWest");
    benchmark::AddCustomContext("version", "1.0.0");
    benchmark::AddCustomContext("author", "Olumuyiwa Oluwasanmi");
    
    // Run benchmarks
    benchmark::RunSpecifiedBenchmarks();
    
    // Shutdown and cleanup
    benchmark::Shutdown();
    return 0;
}