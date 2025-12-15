#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#ifdef _OPENMP
    #include <omp.h>
#endif

import fastjson_parallel;

using namespace std::chrono;

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

auto benchmark_parse(const std::string& json_data, int num_threads) -> double {
    // Set thread count using the parallel module's API
    fastjson_parallel::set_num_threads(num_threads);

    auto start = std::chrono::high_resolution_clock::now();
    auto result = fastjson_parallel::parse(json_data);
    auto end = std::chrono::high_resolution_clock::now();

    if (!result) {
        std::cerr << "Parse failed!" << std::endl;
        return -1.0;
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return duration.count();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <json_file>\n";
        std::cerr << "Example: " << argv[0] << " /tmp/test_2gb.json\n";
        return 1;
    }

    std::string filename = argv[1];

    std::cout << "=== Large File Parallel Benchmark ===" << std::endl;
    std::cout << "File: " << filename << std::endl;

#ifdef _OPENMP
    int max_threads = omp_get_max_threads();
    std::cout << "OpenMP available: max threads = " << max_threads << std::endl;
#else
    std::cout << "OpenMP not available" << std::endl;
    int max_threads = 1;
#endif

    int hw_threads = std::thread::hardware_concurrency();
    std::cout << "Hardware threads: " << hw_threads << std::endl;

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
    // Remove duplicates
    std::sort(thread_counts.begin(), thread_counts.end());
    thread_counts.erase(std::unique(thread_counts.begin(), thread_counts.end()), thread_counts.end());

    for (int num_threads : thread_counts) {
        if (num_threads > max_threads) continue;
        
        std::cout << "\nThreads: " << num_threads << std::endl;
        double parse_time = benchmark_parse(json_data, num_threads);
        
        if (parse_time > 0) {
            std::cout << "  Parse time: " << parse_time << " ms" << std::endl;
            std::cout << "  Parse speed: " << (size_mb / (parse_time / 1000.0)) << " MB/s" << std::endl;
            
            if (num_threads == 1) {
                std::cout << "  (baseline)" << std::endl;
            } else {
                // Calculate speedup relative to single-threaded
                double baseline_time = benchmark_parse(json_data, 1);
                if (baseline_time > 0) {
                    double speedup = baseline_time / parse_time;
                    std::cout << "  Speedup: " << speedup << "x" << std::endl;
                    std::cout << "  Efficiency: " << (speedup / num_threads * 100) << "%" << std::endl;
                }
            }
        }
    }

    return 0;
}
