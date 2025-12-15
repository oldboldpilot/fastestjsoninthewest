// Benchmark: Multi-Register Parser vs Original FastJSON Parser
// Compares throughput, latency, and LINQ operations performance
// ============================================================================

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

// Import both modules
import fastjson;
import fastjson_multiregister_parser;

namespace benchmark {

using Clock = std::chrono::high_resolution_clock;
using namespace std::chrono;

// ============================================================================
// Benchmark Utilities
// ============================================================================

struct benchmark_result {
    std::string name;
    double throughput_mbps;
    double avg_time_us;
    double min_time_us;
    double max_time_us;
    std::size_t iterations;
    std::size_t data_size;
};

template <typename Func>
auto run_benchmark(const std::string& name, std::string_view data, 
                   Func&& func, std::size_t iterations = 100) -> benchmark_result {
    std::vector<double> times;
    times.reserve(iterations);
    
    // Warmup
    for (int i = 0; i < 10; ++i) {
        auto result = func(data);
        (void)result;
    }
    
    // Benchmark runs
    for (std::size_t i = 0; i < iterations; ++i) {
        auto start = Clock::now();
        auto result = func(data);
        auto end = Clock::now();
        
        // Prevent optimization
        if (!result) {
            std::cerr << "Parse failed in benchmark\n";
        }
        
        auto duration = duration_cast<nanoseconds>(end - start).count();
        times.push_back(static_cast<double>(duration) / 1000.0); // microseconds
    }
    
    // Calculate statistics
    double sum = 0, min_t = times[0], max_t = times[0];
    for (double t : times) {
        sum += t;
        if (t < min_t) min_t = t;
        if (t > max_t) max_t = t;
    }
    double avg = sum / static_cast<double>(iterations);
    
    double throughput = (static_cast<double>(data.size()) / avg); // bytes/us = MB/s
    
    return {
        .name = name,
        .throughput_mbps = throughput,
        .avg_time_us = avg,
        .min_time_us = min_t,
        .max_time_us = max_t,
        .iterations = iterations,
        .data_size = data.size()
    };
}

void print_result(const benchmark_result& r) {
    std::cout << std::setw(35) << std::left << r.name
              << std::fixed << std::setprecision(2)
              << std::setw(12) << r.throughput_mbps << " MB/s  "
              << std::setw(10) << r.avg_time_us << " µs avg  "
              << std::setw(10) << r.min_time_us << " µs min  "
              << std::setw(10) << r.max_time_us << " µs max\n";
}

void print_comparison(const benchmark_result& original, const benchmark_result& multiregister) {
    double speedup = multiregister.throughput_mbps / original.throughput_mbps;
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2) 
              << speedup << "x";
    if (speedup > 1.0) {
        std::cout << " (Multi-Register " << ((speedup - 1.0) * 100) << "% faster)";
    } else if (speedup < 1.0) {
        std::cout << " (Original " << ((1.0 / speedup - 1.0) * 100) << "% faster)";
    }
    std::cout << "\n";
}

// ============================================================================
// Test Data Generation
// ============================================================================

auto generate_simple_json(std::size_t count) -> std::string {
    std::string json = "[";
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0) json += ",";
        json += std::to_string(i);
    }
    json += "]";
    return json;
}

auto generate_nested_objects(std::size_t count) -> std::string {
    std::string json = "[";
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0) json += ",";
        json += R"({"id":)" + std::to_string(i) + 
                R"(,"name":"item_)" + std::to_string(i) + 
                R"(","value":)" + std::to_string(i * 1.5) + 
                R"(,"active":)" + (i % 2 == 0 ? "true" : "false") + "}";
    }
    json += "]";
    return json;
}

auto generate_deep_nesting(std::size_t depth) -> std::string {
    std::string json;
    for (std::size_t i = 0; i < depth; ++i) {
        json += R"({"level":)" + std::to_string(i) + R"(,"child":)";
    }
    json += "null";
    for (std::size_t i = 0; i < depth; ++i) {
        json += "}";
    }
    return json;
}

auto generate_string_heavy(std::size_t count) -> std::string {
    std::string json = "[";
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0) json += ",";
        json += "\"Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.\"";
    }
    json += "]";
    return json;
}

auto generate_mixed_types(std::size_t count) -> std::string {
    std::string json = "[";
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0) json += ",";
        switch (i % 5) {
            case 0: json += std::to_string(i); break;
            case 1: json += std::to_string(i * 0.123456); break;
            case 2: json += "\"string_" + std::to_string(i) + "\""; break;
            case 3: json += (i % 2 == 0 ? "true" : "false"); break;
            case 4: json += "null"; break;
        }
    }
    json += "]";
    return json;
}

// ============================================================================
// Parse Wrappers
// ============================================================================

auto parse_original(std::string_view data) -> bool {
    auto result = fastjson::parse(data);
    return result.has_value();
}

auto parse_multiregister(std::string_view data) -> bool {
    auto result = fastjson::mr::parse(data);
    return result.has_value();
}

// ============================================================================
// Main Benchmark Suite
// ============================================================================

void run_parse_benchmarks() {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                           PARSING BENCHMARK: Multi-Register vs Original                          ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════════════════════════════════════════╝\n\n";
    
    // Get SIMD info
    auto simd = fastjson::mr::get_simd_info();
    std::cout << "SIMD Path: " << simd.optimal_path << "\n";
    std::cout << "Registers: " << simd.register_count << " x 32 bytes = " 
              << simd.bytes_per_iteration << " bytes/iteration\n\n";
    
    // Test configurations
    struct TestConfig {
        std::string name;
        std::string (*generator)(std::size_t);
        std::size_t param;
    };
    
    std::vector<TestConfig> configs = {
        {"Simple Array (100 ints)", generate_simple_json, 100},
        {"Simple Array (1000 ints)", generate_simple_json, 1000},
        {"Simple Array (10000 ints)", generate_simple_json, 10000},
        {"Nested Objects (100)", generate_nested_objects, 100},
        {"Nested Objects (1000)", generate_nested_objects, 1000},
        {"Deep Nesting (50 levels)", generate_deep_nesting, 50},
        {"Deep Nesting (100 levels)", generate_deep_nesting, 100},
        {"String Heavy (100)", generate_string_heavy, 100},
        {"String Heavy (1000)", generate_string_heavy, 1000},
        {"Mixed Types (1000)", generate_mixed_types, 1000},
        {"Mixed Types (10000)", generate_mixed_types, 10000},
    };
    
    std::cout << std::setw(35) << std::left << "Test"
              << std::setw(15) << "Throughput"
              << std::setw(15) << "Avg Time"
              << std::setw(15) << "Min Time"
              << std::setw(15) << "Max Time" << "\n";
    std::cout << std::string(95, '-') << "\n";
    
    for (const auto& config : configs) {
        std::string json = config.generator(config.param);
        
        std::cout << "\n" << config.name << " (" << json.size() << " bytes):\n";
        
        auto original_result = run_benchmark(
            "  Original FastJSON", json, parse_original, 100);
        print_result(original_result);
        
        auto mr_result = run_benchmark(
            "  Multi-Register SIMD", json, parse_multiregister, 100);
        print_result(mr_result);
        
        print_comparison(original_result, mr_result);
    }
}

void run_linq_benchmarks() {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                   LINQ OPERATIONS BENCHMARK                                       ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════════════════════════════════════════╝\n\n";
    
    // Generate large array for LINQ operations
    std::string json = generate_simple_json(10000);
    
    auto parse_result = fastjson::mr::parse(json);
    if (!parse_result.has_value()) {
        std::cerr << "Failed to parse test data for LINQ benchmarks\n";
        return;
    }
    
    const auto& value = *parse_result;
    
    // LINQ benchmarks
    std::cout << "Data size: 10000 elements\n\n";
    
    // Filter benchmark
    {
        auto start = Clock::now();
        for (int i = 0; i < 100; ++i) {
            auto filtered = fastjson::mr::query(value)
                .filter([](const fastjson::mr::json_value& v) { 
                    return v.as_int().value_or(0) > 5000; 
                });
            (void)filtered.count();
        }
        auto end = Clock::now();
        auto duration = duration_cast<microseconds>(end - start).count() / 100.0;
        std::cout << "filter (> 5000):     " << std::fixed << std::setprecision(2) 
                  << duration << " µs/op\n";
    }
    
    // Map benchmark
    {
        auto start = Clock::now();
        for (int i = 0; i < 100; ++i) {
            auto mapped = fastjson::mr::query(value)
                .map([](const fastjson::mr::json_value& v) { 
                    return v.as_int().value_or(0) * 2; 
                });
            (void)mapped.count();
        }
        auto end = Clock::now();
        auto duration = duration_cast<microseconds>(end - start).count() / 100.0;
        std::cout << "map (* 2):           " << std::fixed << std::setprecision(2) 
                  << duration << " µs/op\n";
    }
    
    // Reduce benchmark
    {
        auto start = Clock::now();
        for (int i = 0; i < 100; ++i) {
            auto sum = fastjson::mr::query(value)
                .map([](const fastjson::mr::json_value& v) { 
                    return v.as_int().value_or(0); 
                })
                .reduce(0L, [](long acc, int v) { return acc + v; });
            (void)sum;
        }
        auto end = Clock::now();
        auto duration = duration_cast<microseconds>(end - start).count() / 100.0;
        std::cout << "reduce (sum):        " << std::fixed << std::setprecision(2) 
                  << duration << " µs/op\n";
    }
    
    // Order by benchmark
    {
        auto start = Clock::now();
        for (int i = 0; i < 10; ++i) {  // Fewer iterations since sorting is expensive
            auto sorted = fastjson::mr::query(value)
                .order_by([](const fastjson::mr::json_value& v) { 
                    return v.as_int().value_or(0); 
                });
            (void)sorted.count();
        }
        auto end = Clock::now();
        auto duration = duration_cast<microseconds>(end - start).count() / 10.0;
        std::cout << "order_by:            " << std::fixed << std::setprecision(2) 
                  << duration << " µs/op\n";
    }
    
    // Chain benchmark
    {
        auto start = Clock::now();
        for (int i = 0; i < 100; ++i) {
            auto result = fastjson::mr::query(value)
                .filter([](const fastjson::mr::json_value& v) { 
                    return v.as_int().value_or(0) % 2 == 0; 
                })
                .map([](const fastjson::mr::json_value& v) { 
                    return v.as_int().value_or(0) * 2; 
                })
                .take(100)
                .reduce(0L, [](long acc, int v) { return acc + v; });
            (void)result;
        }
        auto end = Clock::now();
        auto duration = duration_cast<microseconds>(end - start).count() / 100.0;
        std::cout << "filter+map+take+sum: " << std::fixed << std::setprecision(2) 
                  << duration << " µs/op\n";
    }
}

void run_parallel_benchmarks() {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                              PARALLEL vs SEQUENTIAL BENCHMARK                                     ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════════════════════════════════════════╝\n\n";
    
    std::string json = generate_simple_json(100000);
    
    auto parse_result = fastjson::mr::parse(json);
    if (!parse_result.has_value()) {
        std::cerr << "Failed to parse test data for parallel benchmarks\n";
        return;
    }
    
    const auto& value = *parse_result;
    std::cout << "Data size: 100000 elements\n";
    std::cout << "Thread count: " << fastjson::mr::get_thread_count() << "\n\n";
    
    // Sequential filter
    {
        auto start = Clock::now();
        for (int i = 0; i < 10; ++i) {
            auto filtered = fastjson::mr::query(value)
                .filter([](const fastjson::mr::json_value& v) { 
                    return v.as_int().value_or(0) > 50000; 
                });
            (void)filtered.count();
        }
        auto end = Clock::now();
        auto duration = duration_cast<microseconds>(end - start).count() / 10.0;
        std::cout << "Sequential filter:   " << std::fixed << std::setprecision(2) 
                  << duration << " µs/op\n";
    }
    
    // Parallel filter
    {
        auto start = Clock::now();
        for (int i = 0; i < 10; ++i) {
            auto filtered = fastjson::mr::parallel_query(value)
                .filter([](const fastjson::mr::json_value& v) { 
                    return v.as_int().value_or(0) > 50000; 
                });
            (void)filtered.size();
        }
        auto end = Clock::now();
        auto duration = duration_cast<microseconds>(end - start).count() / 10.0;
        std::cout << "Parallel filter:     " << std::fixed << std::setprecision(2) 
                  << duration << " µs/op\n";
    }
    
    // Sequential map
    {
        auto start = Clock::now();
        for (int i = 0; i < 10; ++i) {
            auto mapped = fastjson::mr::query(value)
                .map([](const fastjson::mr::json_value& v) { 
                    return v.as_int().value_or(0) * 2; 
                });
            (void)mapped.count();
        }
        auto end = Clock::now();
        auto duration = duration_cast<microseconds>(end - start).count() / 10.0;
        std::cout << "Sequential map:      " << std::fixed << std::setprecision(2) 
                  << duration << " µs/op\n";
    }
    
    // Parallel map
    {
        auto start = Clock::now();
        for (int i = 0; i < 10; ++i) {
            auto mapped = fastjson::mr::parallel_query(value)
                .map([](const fastjson::mr::json_value& v) { 
                    return v.as_int().value_or(0) * 2; 
                });
            (void)mapped.size();
        }
        auto end = Clock::now();
        auto duration = duration_cast<microseconds>(end - start).count() / 10.0;
        std::cout << "Parallel map:        " << std::fixed << std::setprecision(2) 
                  << duration << " µs/op\n";
    }
}

} // namespace benchmark

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           FastJSON Multi-Register Parser Benchmark Suite                                          ║\n";
    std::cout << "║           Comparing SIMD Multi-Register vs Original Parser                                        ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════════════════════════════════════════╝\n";
    
    benchmark::run_parse_benchmarks();
    benchmark::run_linq_benchmarks();
    benchmark::run_parallel_benchmarks();
    
    std::cout << "\n════════════════════════════════════════════════════════════════════════════════════════════════════\n";
    std::cout << "Benchmark complete.\n";
    
    return 0;
}
