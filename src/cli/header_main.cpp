// FastestJSONInTheWest CLI - Header-Only Version
// Author: Olumuyiwa Oluwasanmi
// Simple command-line interface for the JSON library

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <fastjson.hpp>

void print_usage() {
    std::cout << "FastestJSONInTheWest CLI - Header-Only Version\n";
    std::cout << "Usage: fastjson_cli [options] <input_file>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --validate    Validate JSON syntax\n";
    std::cout << "  --pretty      Pretty-print JSON\n";
    std::cout << "  --minify      Minify JSON\n";
    std::cout << "  --benchmark   Run performance benchmark\n";
    std::cout << "  --help        Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  fastjson_cli --validate data.json\n";
    std::cout << "  fastjson_cli --pretty data.json\n";
    std::cout << "  fastjson_cli --benchmark data.json\n";
}

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    
    return std::string{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };
}

void validate_json(const std::string& content) {
    std::cout << "Validating JSON...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = fastjson::parse(content);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (result) {
        std::cout << "✓ JSON is valid\n";
        std::cout << "Parse time: " << (duration.count() / 1000.0) << " ms\n";
    } else {
        std::cout << "✗ JSON is invalid\n";
        std::cout << "Error: " << result.error().message << "\n";
        std::cout << "Position: " << result.error().position << "\n";
    }
}

void pretty_print_json(const std::string& content) {
    std::cout << "Pretty-printing JSON...\n";
    
    auto result = fastjson::parse(content);
    if (result) {
        std::string pretty = fastjson::serialize(*result, true);
        std::cout << pretty << "\n";
    } else {
        std::cout << "✗ Cannot pretty-print invalid JSON\n";
        std::cout << "Error: " << result.error().message << "\n";
    }
}

void minify_json(const std::string& content) {
    std::cout << "Minifying JSON...\n";
    
    auto result = fastjson::parse(content);
    if (result) {
        std::string minified = fastjson::serialize(*result, false);
        std::cout << minified << "\n";
    } else {
        std::cout << "✗ Cannot minify invalid JSON\n";
        std::cout << "Error: " << result.error().message << "\n";
    }
}

void benchmark_json(const std::string& content) {
    std::cout << "Running performance benchmark...\n";
    
    const int iterations = 1000;
    
    // Parse benchmark
    auto start = std::chrono::high_resolution_clock::now();
    int successful_parses = 0;
    
    for (int i = 0; i < iterations; ++i) {
        auto result = fastjson::parse(content);
        if (result) {
            successful_parses++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto parse_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Parse Results:\n";
    std::cout << "  Total time: " << (parse_duration.count() / 1000.0) << " ms\n";
    std::cout << "  Successful parses: " << successful_parses << "/" << iterations << "\n";
    std::cout << "  Average per parse: " << (parse_duration.count() / 1000.0 / iterations) << " ms\n";
    std::cout << "  Parses per second: " << (iterations * 1000000.0 / parse_duration.count()) << "\n";
    
    // Serialization benchmark
    auto parsed = fastjson::parse(content);
    if (parsed) {
        start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto serialized = fastjson::serialize(*parsed, false);
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto serialize_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Serialize Results:\n";
        std::cout << "  Total time: " << (serialize_duration.count() / 1000.0) << " ms\n";
        std::cout << "  Average per serialize: " << (serialize_duration.count() / 1000.0 / iterations) << " ms\n";
        std::cout << "  Serializes per second: " << (iterations * 1000000.0 / serialize_duration.count()) << "\n";
    }
    
    std::cout << "Input size: " << content.length() << " bytes\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    std::string option = argv[1];
    
    if (option == "--help") {
        print_usage();
        return 0;
    }
    
    if (argc < 3) {
        std::cout << "Error: Input file required\n";
        print_usage();
        return 1;
    }
    
    std::string filename = argv[2];
    
    try {
        std::string content = read_file(filename);
        
        if (option == "--validate") {
            validate_json(content);
        } else if (option == "--pretty") {
            pretty_print_json(content);
        } else if (option == "--minify") {
            minify_json(content);
        } else if (option == "--benchmark") {
            benchmark_json(content);
        } else {
            std::cout << "Error: Unknown option " << option << "\n";
            print_usage();
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}