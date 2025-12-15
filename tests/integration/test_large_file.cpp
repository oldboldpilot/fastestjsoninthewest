#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

import fastjson;

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
        std::cerr << "Example: " << argv[0] << " /tmp/test_100mb.json\n";
        return 1;
    }

    std::string filename = argv[1];

    std::cout << "=== Large File Benchmark ===" << std::endl;
    std::cout << "File: " << filename << std::endl;

    // Load the file
    std::cout << "\nLoading file..." << std::endl;
    auto start_load = std::chrono::high_resolution_clock::now();
    std::string json_data = load_file(filename);
    auto end_load = std::chrono::high_resolution_clock::now();
    auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_load - start_load);

    double size_mb = json_data.size() / (1024.0 * 1024.0);
    std::cout << "File size: " << size_mb << " MB" << std::endl;
    std::cout << "Load time: " << load_time.count() << " ms" << std::endl;
    std::cout << "Load speed: " << (size_mb / (load_time.count() / 1000.0)) << " MB/s" << std::endl;

    // Parse the JSON
    std::cout << "\nParsing JSON..." << std::endl;
    auto start_parse = std::chrono::high_resolution_clock::now();
    auto result = fastjson::parse(json_data);
    auto end_parse = std::chrono::high_resolution_clock::now();
    auto parse_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_parse - start_parse);

    if (!result) {
        std::cerr << "Parse failed!" << std::endl;
        return 1;
    }

    std::cout << "Parse time: " << parse_time.count() << " ms" << std::endl;
    std::cout << "Parse speed: " << (size_mb / (parse_time.count() / 1000.0)) << " MB/s" << std::endl;

    // Access some data to verify parsing worked
    auto& root = result.value();
    if (root.is_object()) {
        if (root.contains("data")) {
            auto& data = root["data"];
            if (data.is_array()) {
                std::cout << "\nArray contains " << data.size() << " elements" << std::endl;
                
                // Sample first element
                if (data.size() > 0) {
                    auto& first = data[0];
                    if (first.is_object() && first.contains("id")) {
                        std::cout << "First element ID: " << first["id"].as_number() << std::endl;
                    }
                }
            }
        }
    }

    auto total_time = load_time.count() + parse_time.count();
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total time: " << total_time << " ms" << std::endl;
    std::cout << "Overall throughput: " << (size_mb / (total_time / 1000.0)) << " MB/s" << std::endl;

    return 0;
}
