#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _OPENMP
    #include <omp.h>
#endif

#include "modules/fastjson_parallel.h"

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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <json_file>\n";
        std::cerr << "Example: " << argv[0] << " /tmp/test_2gb.json\n";
        return 1;
    }

    std::string filename = argv[1];

    std::cout << "=== Parallel JSON Parser Benchmark ===" << std::endl;
    std::cout << "File: " << filename << std::endl;

#ifdef _OPENMP
    int max_threads = omp_get_max_threads();
    std::cout << "OpenMP available: max threads = " << max_threads << std::endl;
#else
    std::cout << "OpenMP not available" << std::endl;
    int max_threads = 1;
#endif

    // Load the file
    std::cout << "\nLoading file..." << std::endl;
    auto start_load = std::chrono::high_resolution_clock::now();
    std::string json_data = load_file(filename);
    auto end_load = std::chrono::high_resolution_clock::now();
    auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_load - start_load);

    double size_mb = json_data.size() / (1024.0 * 1024.0);
    std::cout << "File size: " << size_mb << " MB" << std::endl;
    std::cout << "Load time: " << load_time.count() << " ms" << std::endl;

    // Test with different thread counts
    std::cout << "\n=== Parsing Benchmarks ===" << std::endl;
    
    std::vector<int> thread_counts = {1, 2, 4, 8, max_threads};
    std::sort(thread_counts.begin(), thread_counts.end());
    thread_counts.erase(std::unique(thread_counts.begin(), thread_counts.end()), thread_counts.end());

    double baseline_time = 0;

    for (int num_threads : thread_counts) {
        if (num_threads > max_threads) continue;
        
        // Configure parser
        fastjson::g_config.num_threads = num_threads;
        fastjson::g_config.enable_simd = true;
        fastjson::g_config.parallel_threshold = 1000;  // Lower threshold for better parallelism
        
        std::cout << "\nThreads: " << num_threads << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto result = fastjson::parse(json_data);
        auto end = std::chrono::high_resolution_clock::now();
        
        if (!result) {
            std::cerr << "  Parse failed!" << std::endl;
            continue;
        }
        
        auto parse_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        std::cout << "  Parse time: " << parse_time << " ms" << std::endl;
        std::cout << "  Parse speed: " << (size_mb / (parse_time / 1000.0)) << " MB/s" << std::endl;
        
        if (num_threads == 1) {
            baseline_time = parse_time;
            std::cout << "  (baseline)" << std::endl;
        } else if (baseline_time > 0) {
            double speedup = baseline_time / parse_time;
            std::cout << "  Speedup: " << speedup << "x" << std::endl;
            std::cout << "  Efficiency: " << (speedup / num_threads * 100) << "%" << std::endl;
        }
    }

    return 0;
}
