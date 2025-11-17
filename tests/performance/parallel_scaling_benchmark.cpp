// FastestJSONInTheWest - OpenMP Scaling and GPU Benchmark
// Tests parallel SIMD performance with configurable thread counts
// ============================================================================

#include "../../modules/fastjson_parallel.cpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std::chrono;

// ============================================================================
// Test Data Generation
// ============================================================================

auto generate_large_json(size_t target_size_mb) -> std::string {
    std::ostringstream json;
    json << "{\n";
    json << "  \"metadata\": {\n";
    json << "    \"version\": \"1.0\",\n";
    json << "    \"generated\": true,\n";
    json << "    \"size_mb\": " << target_size_mb << "\n";
    json << "  },\n";
    json << "  \"data\": [\n";
    
    size_t current_size = json.str().size();
    size_t target_bytes = target_size_mb * 1024 * 1024;
    size_t item_count = 0;
    
    while (current_size < target_bytes) {
        if (item_count > 0) {
            json << ",\n";
        }
        
        json << "    {\n";
        json << "      \"id\": " << item_count << ",\n";
        json << "      \"name\": \"Item " << item_count << "\",\n";
        json << "      \"value\": " << (item_count * 3.14159) << ",\n";
        json << "      \"active\": " << (item_count % 2 == 0 ? "true" : "false") << ",\n";
        json << "      \"tags\": [\"tag1\", \"tag2\", \"tag3\"],\n";
        json << "      \"metadata\": {\n";
        json << "        \"created\": \"2025-01-01T00:00:00Z\",\n";
        json << "        \"modified\": \"2025-01-01T00:00:00Z\",\n";
        json << "        \"version\": 1\n";
        json << "      }\n";
        json << "    }";
        
        item_count++;
        current_size = json.str().size();
    }
    
    json << "\n  ]\n";
    json << "}\n";
    
    return json.str();
}

// ============================================================================
// Benchmark Results Structure
// ============================================================================

struct benchmark_result {
    int num_threads = 1;
    std::string execution_mode = "CPU";
    bool simd_enabled = false;
    size_t input_size_bytes = 0;
    
    double parse_time_ms = 0.0;
    double throughput_mbs = 0.0;
    double speedup_vs_baseline = 1.0;
    double parallel_efficiency = 1.0;
    
    // Additional metrics
    double cpu_time_ms = 0.0;
    double wall_time_ms = 0.0;
    size_t memory_used_bytes = 0;
    
    void print() const {
        std::cout << std::setw(20) << execution_mode
                  << std::setw(8) << num_threads
                  << std::setw(8) << (simd_enabled ? "Yes" : "No")
                  << std::setw(12) << std::fixed << std::setprecision(2) << parse_time_ms
                  << std::setw(12) << std::fixed << std::setprecision(2) << throughput_mbs
                  << std::setw(10) << std::fixed << std::setprecision(2) << speedup_vs_baseline << "x"
                  << std::setw(10) << std::fixed << std::setprecision(1) << (parallel_efficiency * 100) << "%"
                  << "\n";
    }
};

// ============================================================================
// Benchmark Execution
// ============================================================================

auto benchmark_parse(const std::string& json_data, int num_threads, bool enable_simd) -> benchmark_result {
    benchmark_result result;
    result.num_threads = num_threads;
    result.simd_enabled = enable_simd;
    result.input_size_bytes = json_data.size();
    
#ifdef _OPENMP
    if (num_threads > 0) {
        omp_set_num_threads(num_threads);
        result.execution_mode = "CPU+OpenMP";
    } else {
        result.execution_mode = "CPU";
    }
#else
    result.execution_mode = "CPU";
    result.num_threads = 1;
#endif
    
    // Configure parser
    fastjson::parse_config config;
    config.num_threads = num_threads;
    config.enable_simd = enable_simd;
    config.parallel_threshold = 100;  // Lower threshold for testing
    
    // Warmup run
    {
        auto warmup_result = fastjson::parse_with_config(json_data, config);
        if (!warmup_result) {
            std::cerr << "Warmup parse failed: " << warmup_result.error().message << "\n";
            result.parse_time_ms = -1.0;
            return result;
        }
        // Explicitly destroy warmup result
    }
    
    // Benchmark run (multiple iterations for accuracy)
    const int iterations = 5;
    std::vector<double> times;
    times.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        auto start = high_resolution_clock::now();
        
#ifdef _OPENMP
        double cpu_start = omp_get_wtime();
#endif
        
        {
            auto parse_result = fastjson::parse_with_config(json_data, config);
            
#ifdef _OPENMP
            double cpu_end = omp_get_wtime();
            result.cpu_time_ms = (cpu_end - cpu_start) * 1000.0;
#endif
            
            auto end = high_resolution_clock::now();
            
            if (!parse_result) {
                std::cerr << "Parse error: " << parse_result.error().message << "\n";
                result.parse_time_ms = -1.0;
                return result;
            }
            
            double elapsed_ms = duration<double, std::milli>(end - start).count();
            times.push_back(elapsed_ms);
            
            // parse_result explicitly destroyed here
        }
    }
    
    // Calculate median time
    std::sort(times.begin(), times.end());
    result.parse_time_ms = times[times.size() / 2];
    result.wall_time_ms = result.parse_time_ms;
    
    // Calculate throughput (MB/s)
    double size_mb = static_cast<double>(json_data.size()) / (1024.0 * 1024.0);
    result.throughput_mbs = size_mb / (result.parse_time_ms / 1000.0);
    
    return result;
}

// ============================================================================
// OpenMP Scaling Test
// ============================================================================

void test_openmp_scaling(const std::string& json_data, const std::vector<int>& thread_counts) {
    std::cout << "\n=== OpenMP + SIMD Scaling Benchmark ===\n\n";
    std::cout << "Input size: " << (json_data.size() / 1024.0 / 1024.0) << " MB\n\n";
    
    std::cout << std::setw(20) << "Execution Mode"
              << std::setw(8) << "Threads"
              << std::setw(8) << "SIMD"
              << std::setw(12) << "Time (ms)"
              << std::setw(12) << "MB/s"
              << std::setw(10) << "Speedup"
              << std::setw(10) << "Efficiency"
              << "\n";
    std::cout << std::string(90, '-') << "\n";
    
    std::vector<benchmark_result> results;
    
    // Baseline: Single thread, no SIMD
    auto baseline = benchmark_parse(json_data, 1, false);
    baseline.speedup_vs_baseline = 1.0;
    baseline.parallel_efficiency = 1.0;
    results.push_back(baseline);
    baseline.print();
    
    double baseline_time = baseline.parse_time_ms;
    
    // Test: Single thread with SIMD
    {
        auto result = benchmark_parse(json_data, 1, true);
        result.speedup_vs_baseline = baseline_time / result.parse_time_ms;
        result.parallel_efficiency = result.speedup_vs_baseline;
        results.push_back(result);
        result.print();
    }
    
#ifdef _OPENMP
    // Test: Multiple threads without SIMD
    for (int threads : thread_counts) {
        if (threads <= 1) continue;
        
        auto result = benchmark_parse(json_data, threads, false);
        result.speedup_vs_baseline = baseline_time / result.parse_time_ms;
        result.parallel_efficiency = result.speedup_vs_baseline / threads;
        results.push_back(result);
        result.print();
    }
    
    std::cout << std::string(90, '-') << "\n";
    
    // Test: Multiple threads with SIMD
    for (int threads : thread_counts) {
        if (threads <= 1) continue;
        
        auto result = benchmark_parse(json_data, threads, true);
        result.speedup_vs_baseline = baseline_time / result.parse_time_ms;
        result.parallel_efficiency = result.speedup_vs_baseline / threads;
        results.push_back(result);
        result.print();
    }
#else
    std::cout << "\nOpenMP not available - skipping parallel tests\n";
#endif
    
    // Save results to JSON
    std::ofstream results_file("openmp_scaling_results.json");
    if (results_file.is_open()) {
        results_file << "{\n";
        results_file << "  \"benchmark\": \"OpenMP + SIMD Scaling\",\n";
        results_file << "  \"input_size_mb\": " << (json_data.size() / 1024.0 / 1024.0) << ",\n";
        results_file << "  \"results\": [\n";
        
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            results_file << "    {\n";
            results_file << "      \"execution_mode\": \"" << r.execution_mode << "\",\n";
            results_file << "      \"num_threads\": " << r.num_threads << ",\n";
            results_file << "      \"simd_enabled\": " << (r.simd_enabled ? "true" : "false") << ",\n";
            results_file << "      \"parse_time_ms\": " << r.parse_time_ms << ",\n";
            results_file << "      \"throughput_mbs\": " << r.throughput_mbs << ",\n";
            results_file << "      \"speedup\": " << r.speedup_vs_baseline << ",\n";
            results_file << "      \"efficiency\": " << r.parallel_efficiency << "\n";
            results_file << "    }";
            if (i < results.size() - 1) results_file << ",";
            results_file << "\n";
        }
        
        results_file << "  ]\n";
        results_file << "}\n";
        results_file.close();
        
        std::cout << "\nResults saved to openmp_scaling_results.json\n";
    }
    
    // Print summary
    std::cout << "\n=== Summary ===\n";
    std::cout << "Baseline (1 thread, no SIMD): " << baseline_time << " ms\n";
    
    auto best = std::max_element(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.speedup_vs_baseline < b.speedup_vs_baseline; });
    
    if (best != results.end()) {
        std::cout << "Best configuration: " << best->execution_mode 
                  << " (" << best->num_threads << " threads, "
                  << (best->simd_enabled ? "SIMD enabled" : "SIMD disabled") << ")\n";
        std::cout << "Best speedup: " << best->speedup_vs_baseline << "x\n";
        std::cout << "Best throughput: " << best->throughput_mbs << " MB/s\n";
    }
}

// ============================================================================
// GPU Benchmark
// ============================================================================

void test_gpu_backends(const std::string& json_data) {
    std::cout << "\n=== GPU Backend Detection and Benchmark ===\n\n";
    
    // Test CUDA
    std::cout << "Testing CUDA support: ";
    // TODO: Implement CUDA detection and benchmark
    std::cout << "Not implemented yet\n";
    
    // Test ROCm
    std::cout << "Testing ROCm support: ";
    // TODO: Implement ROCm detection and benchmark
    std::cout << "Not implemented yet\n";
    
    // Test SYCL
    std::cout << "Testing Intel oneAPI SYCL support: ";
    // TODO: Implement SYCL detection and benchmark
    std::cout << "Not implemented yet\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== FastestJSONInTheWest - Parallel SIMD + GPU Scaling Benchmark ===\n";
    
    // Parse command line arguments
    size_t test_size_mb = 10;
    if (argc > 1) {
        test_size_mb = std::atoi(argv[1]);
        if (test_size_mb < 1) test_size_mb = 1;
        if (test_size_mb > 1000) test_size_mb = 1000;
    }
    
    std::cout << "\nGenerating test data (" << test_size_mb << " MB)...\n";
    auto json_data = generate_large_json(test_size_mb);
    std::cout << "Generated " << (json_data.size() / 1024.0 / 1024.0) << " MB of JSON data\n";
    
    // Determine available thread counts
    std::vector<int> thread_counts = {1, 2, 4, 8};
    
#ifdef _OPENMP
    int max_threads = omp_get_max_threads();
    std::cout << "OpenMP available: " << max_threads << " threads max\n";
    
    // Add more thread counts if available
    if (max_threads > 8) {
        thread_counts.push_back(16);
    }
    if (max_threads > 16) {
        thread_counts.push_back(max_threads);
    }
#else
    std::cout << "OpenMP not available - single-threaded only\n";
    thread_counts = {1};
#endif
    
    // Run OpenMP scaling benchmark
    test_openmp_scaling(json_data, thread_counts);
    
    // Run GPU benchmarks
    test_gpu_backends(json_data);
    
    std::cout << "\nBenchmark completed!\n";
    
    return 0;
}
