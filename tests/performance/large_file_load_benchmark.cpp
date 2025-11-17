#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>
#include <omp.h>

// Include the parallel parser implementation
#include "../../modules/fastjson_parallel.cpp"

auto load_file(const std::string& filename) -> std::string {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string content(size, '\0');
    file.read(content.data(), size);
    
    return content;
}

auto benchmark_parse(const std::string& json_data, int num_threads, bool enable_simd) -> double {
    // Set global config
    fastjson::g_config.num_threads = num_threads;
    fastjson::g_config.enable_simd = enable_simd;
    fastjson::g_config.parallel_threshold = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    auto result = fastjson::parse(json_data);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (!result) {
        std::cerr << "Parse failed\n";
        return -1.0;
    }
    
    return duration.count();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <json_file>\n";
        std::cerr << "Example: " << argv[0] << " /tmp/test_2gb.json\n";
        return 1;
    }
    
    std::string filename = argv[1];
    
    std::cout << "=== Large File Benchmark ===" << std::endl;
    std::cout << "File: " << filename << std::endl;
    
    // Load the file
    std::cout << "\nLoading file..." << std::endl;
    auto load_start = std::chrono::high_resolution_clock::now();
    
    std::string json_data;
    try {
        json_data = load_file(filename);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    auto load_end = std::chrono::high_resolution_clock::now();
    auto load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start);
    
    double size_mb = json_data.size() / (1024.0 * 1024.0);
    std::cout << "Loaded " << size_mb << " MB in " << load_duration.count() << " ms\n";
    std::cout << "Load throughput: " << (size_mb / (load_duration.count() / 1000.0)) << " MB/s\n";
    
    int max_threads = omp_get_max_threads();
    std::cout << "\nOpenMP available: " << max_threads << " threads max\n";
    
    // Test configurations
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    
    std::cout << "\n=== Parse Benchmark Results ===\n\n";
    std::cout << "Threads   Time (ms)     MB/s   Speedup\n";
    std::cout << "-------------------------------------------\n";
    
    double baseline_time = 0.0;
    
    for (int threads : thread_counts) {
        if (threads > max_threads) continue;
        
        double time_ms = benchmark_parse(json_data, threads, false);
        
        if (time_ms < 0) {
            std::cout << threads << "        FAILED\n";
            continue;
        }
        
        double throughput = size_mb / (time_ms / 1000.0);
        double speedup = 1.0;
        
        if (threads == 1) {
            baseline_time = time_ms;
        } else if (baseline_time > 0) {
            speedup = baseline_time / time_ms;
        }
        
        printf("%7d   %9.2f   %7.2f    %.2fx\n", 
               threads, time_ms, throughput, speedup);
    }
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "File size: " << size_mb << " MB\n";
    std::cout << "Baseline (1 thread): " << baseline_time << " ms\n";
    
    return 0;
}
