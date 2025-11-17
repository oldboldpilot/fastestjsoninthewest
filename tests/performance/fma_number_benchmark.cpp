// FMA Number Parsing Benchmark
// Author: Olumuyiwa Oluwasanmi

#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include "modules/fastjson_parallel.cpp"

using namespace std::chrono;

auto generate_number_json(size_t count) -> std::string {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> int_dist(-1000000, 1000000);
    std::uniform_real_distribution<double> frac_dist(0.0, 1.0);
    
    std::string result = "[";
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) result += ",";
        
        // Mix of integers and decimals
        if (i % 2 == 0) {
            result += std::to_string(int_dist(rng));
        } else {
            double val = int_dist(rng) + frac_dist(rng);
            result += std::to_string(val);
        }
    }
    result += "]";
    return result;
}

auto main() -> int {
    constexpr size_t num_numbers = 100000;
    constexpr int iterations = 10;
    
    std::cout << "Generating test data with " << num_numbers << " numbers...\n";
    std::string json = generate_number_json(num_numbers);
    std::cout << "JSON size: " << json.size() << " bytes\n\n";
    
    // Test with FMA enabled
    std::cout << "=== Testing with FMA ENABLED ===\n";
    auto config_fma = fastjson::parse_config{};
    config_fma.enable_simd = true;
    config_fma.num_threads = 8;
    
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto result = fastjson::parse_with_config(json, config_fma);
        if (!result.has_value()) {
            std::cerr << "Parse error: " << result.error().message << "\n";
            return 1;
        }
    }
    auto end = high_resolution_clock::now();
    auto duration_fma = duration_cast<milliseconds>(end - start).count();
    
    std::cout << "Time (FMA): " << duration_fma << " ms for " << iterations << " iterations\n";
    std::cout << "Average: " << (duration_fma / static_cast<double>(iterations)) << " ms per parse\n";
    std::cout << "Throughput: " << (json.size() * iterations / 1024.0 / 1024.0) / (duration_fma / 1000.0) 
              << " MB/s\n\n";
    
    // Test with FMA disabled (use strtod)
    std::cout << "=== Testing with FMA DISABLED (strtod fallback) ===\n";
    auto config_no_fma = fastjson::parse_config{};
    config_no_fma.enable_simd = false;  // Disable SIMD to force strtod path
    config_no_fma.num_threads = 8;
    
    start = high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto result = fastjson::parse_with_config(json, config_no_fma);
        if (!result.has_value()) {
            std::cerr << "Parse error: " << result.error().message << "\n";
            return 1;
        }
    }
    end = high_resolution_clock::now();
    auto duration_no_fma = duration_cast<milliseconds>(end - start).count();
    
    std::cout << "Time (no FMA): " << duration_no_fma << " ms for " << iterations << " iterations\n";
    std::cout << "Average: " << (duration_no_fma / static_cast<double>(iterations)) << " ms per parse\n";
    std::cout << "Throughput: " << (json.size() * iterations / 1024.0 / 1024.0) / (duration_no_fma / 1000.0) 
              << " MB/s\n\n";
    
    // Calculate speedup
    double speedup = static_cast<double>(duration_no_fma) / duration_fma;
    std::cout << "========================================\n";
    std::cout << "FMA Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n";
    std::cout << "Improvement: " << std::fixed << std::setprecision(1) 
              << ((speedup - 1.0) * 100) << "%\n";
    std::cout << "========================================\n";
    
    return 0;
}
