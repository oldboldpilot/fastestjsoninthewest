#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <random>
#include <filesystem>

#ifdef _OPENMP
#include <omp.h>
#endif

// Import the fastjson module
#include "../modules/fastjson_compat.h"

using namespace std::chrono;
namespace fs = std::filesystem;

class LargeFileBenchmark {
public:
    static auto generate_large_json_file(const std::string& filename, size_t target_size_mb) -> void {
        std::ofstream file(filename);
        if (!file) {
            throw std::runtime_error("Cannot create file: " + filename);
        }
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> int_dist(1, 1000000);
        std::uniform_real_distribution<> real_dist(0.0, 1000.0);
        
        file << "{\n";
        file << "  \"metadata\": {\n";
        file << "    \"generated_at\": \"" << duration_cast<seconds>(system_clock::now().time_since_epoch()).count() << "\",\n";
        file << "    \"target_size_mb\": " << target_size_mb << ",\n";
        file << "    \"purpose\": \"Large file parsing benchmark\"\n";
        file << "  },\n";
        
        file << "  \"large_array\": [\n";
        
        size_t current_size = 0;
        size_t target_bytes = target_size_mb * 1024 * 1024;
        bool first_item = true;
        
        while (current_size < target_bytes) {
            if (!first_item) {
                file << ",\n";
            }
            first_item = false;
            
            // Generate complex nested objects
            file << "    {\n";
            file << "      \"id\": " << int_dist(gen) << ",\n";
            file << "      \"value\": " << std::fixed << std::setprecision(6) << real_dist(gen) << ",\n";
            file << "      \"name\": \"item_" << int_dist(gen) << "\",\n";
            file << "      \"active\": " << (int_dist(gen) % 2 == 0 ? "true" : "false") << ",\n";
            file << "      \"nested\": {\n";
            file << "        \"coordinates\": [" << real_dist(gen) << ", " << real_dist(gen) << ", " << real_dist(gen) << "],\n";
            file << "        \"metadata\": {\n";
            file << "          \"timestamp\": " << int_dist(gen) << ",\n";
            file << "          \"category\": \"type_" << (int_dist(gen) % 10) << "\",\n";
            file << "          \"priority\": " << (int_dist(gen) % 5) << "\n";
            file << "        }\n";
            file << "      },\n";
            file << "      \"tags\": [";
            for (int i = 0; i < 3; ++i) {
                if (i > 0) file << ", ";
                file << "\"tag_" << (int_dist(gen) % 100) << "\"";
            }
            file << "]\n";
            file << "    }";
            
            current_size = file.tellp();
        }
        
        file << "\n  ]\n}\n";
        file.close();
        
        auto actual_size = fs::file_size(filename);
        std::cout << "Generated JSON file: " << filename << " (" 
                  << (actual_size / (1024.0 * 1024.0)) << " MB)\n";
    }
    
    static auto benchmark_parsing(const std::string& filename) -> void {
        std::cout << "\n=== Large File Parsing Benchmark ===\n";
        
        // OpenMP configuration
        #ifdef _OPENMP
        auto max_threads = omp_get_max_threads();
        std::cout << "OpenMP enabled - Max threads: " << max_threads << "\n";
        #else
        std::cout << "OpenMP not enabled\n";
        #endif
        
        // Read file into memory
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            throw std::runtime_error("Cannot open file: " + filename);
        }
        
        auto file_size = file.tellg();
        file.seekg(0);
        
        std::string json_content(file_size, '\0');
        file.read(json_content.data(), file_size);
        file.close();
        
        std::cout << "File size: " << (file_size / (1024.0 * 1024.0)) << " MB\n";
        
        // Benchmark different thread counts
        std::vector<int> thread_counts = {1};
        #ifdef _OPENMP
        if (max_threads > 1) {
            thread_counts.push_back(2);
            thread_counts.push_back(max_threads / 2);
            thread_counts.push_back(max_threads);
        }
        #endif
        
        for (auto thread_count : thread_counts) {
            #ifdef _OPENMP
            omp_set_num_threads(thread_count);
            #endif
            
            std::cout << "\n--- Testing with " << thread_count << " thread(s) ---\n";
            
            // Warm-up run
            try {
                auto warm_up_result = fastjson::parse("{\"test\": 123}");
                if (warm_up_result.has_value()) {
                    std::cout << "Warm-up completed\n";
                } else {
                    std::cout << "Warm-up failed: " << warm_up_result.error().message << "\n";
                }
            } catch (const std::exception& e) {
                std::cout << "Warm-up failed: " << e.what() << "\n";
            }
            
            // Benchmark runs
            const int num_runs = 3;
            std::vector<double> parse_times;
            
            for (int run = 0; run < num_runs; ++run) {
                auto start = high_resolution_clock::now();
                
                try {
                    auto parse_result = fastjson::parse(json_content);
                    
                    if (!parse_result.has_value()) {
                        throw std::runtime_error("Parse failed: " + std::string(parse_result.error().message));
                    }
                    
                    fastjson::json_value result = parse_result.value();
                    
                    auto end = high_resolution_clock::now();
                    auto duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
                    parse_times.push_back(duration_ms);
                    
                    std::cout << "Run " << (run + 1) << ": " 
                              << std::fixed << std::setprecision(2) << duration_ms << " ms\n";
                    
                } catch (const std::exception& e) {
                    std::cout << "Parse failed on run " << (run + 1) << ": " << e.what() << "\n";
                    continue;
                }
            }
            
            if (!parse_times.empty()) {
                auto avg_time = std::accumulate(parse_times.begin(), parse_times.end(), 0.0) / parse_times.size();
                auto throughput_mb_s = (file_size / (1024.0 * 1024.0)) / (avg_time / 1000.0);
                
                std::cout << "Average time: " << std::fixed << std::setprecision(2) << avg_time << " ms\n";
                std::cout << "Throughput: " << std::fixed << std::setprecision(2) << throughput_mb_s << " MB/s\n";
            }
        }
    }
};

int main(int argc, char* argv[]) {
    try {
        std::string filename = "large_test.json";
        size_t target_size_mb = 100; // Default 100MB
        
        if (argc > 1) {
            target_size_mb = std::stoul(argv[1]);
        }
        
        std::cout << "FastJSON Large File Benchmark\n";
        std::cout << "Target file size: " << target_size_mb << " MB\n";
        
        // Generate large JSON file
        std::cout << "\nGenerating large JSON file...\n";
        LargeFileBenchmark::generate_large_json_file(filename, target_size_mb);
        
        // Run benchmark
        LargeFileBenchmark::benchmark_parsing(filename);
        
        // Cleanup
        std::cout << "\nCleaning up generated file...\n";
        fs::remove(filename);
        
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}