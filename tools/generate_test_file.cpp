#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

auto generate_large_json_file(const std::string& filename, size_t size_mb) -> void {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Failed to create file: " << filename << "\n";
        return;
    }

    std::cout << "Generating " << size_mb << " MB JSON file: " << filename << "\n";

    file << "{\"data\":[";
    
    const size_t target_bytes = size_mb * 1024 * 1024;
    size_t bytes_written = 12; // Account for {"data":[ and closing ]}
    
    bool first = true;
    size_t record_count = 0;
    
    while (bytes_written < target_bytes) {
        if (!first) {
            file << ",";
            bytes_written += 1;
        }
        first = false;
        
        // Generate a record with various data types
        std::string record = 
            "{\"id\":" + std::to_string(record_count) + 
            ",\"name\":\"User" + std::to_string(record_count) + 
            "\",\"email\":\"user" + std::to_string(record_count) + "@example.com\"" +
            ",\"age\":" + std::to_string(20 + (record_count % 60)) +
            ",\"active\":" + ((record_count % 2 == 0) ? "true" : "false") +
            ",\"score\":" + std::to_string(100.0 + (record_count % 900) / 10.0) +
            ",\"tags\":[\"tag1\",\"tag2\",\"tag3\"]" +
            ",\"metadata\":{\"created\":\"2024-01-01\",\"updated\":\"2024-12-01\",\"version\":1}}";
        
        file << record;
        bytes_written += record.size();
        record_count++;
        
        if (record_count % 10000 == 0) {
            std::cout << "Progress: " << (bytes_written / (1024.0 * 1024.0)) << " MB / " 
                      << size_mb << " MB (" 
                      << (100.0 * bytes_written / target_bytes) << "%)\n";
        }
    }
    
    file << "]}";
    
    std::cout << "Generated " << record_count << " records\n";
    std::cout << "Final size: " << (bytes_written / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "File saved to: " << filename << "\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <output_file> <size_in_mb>\n";
        std::cerr << "Example: " << argv[0] << " test_2gb.json 2048\n";
        return 1;
    }
    
    std::string filename = argv[1];
    size_t size_mb = std::atoi(argv[2]);
    
    if (size_mb == 0 || size_mb > 10000) {
        std::cerr << "Size must be between 1 and 10000 MB\n";
        return 1;
    }
    
    generate_large_json_file(filename, size_mb);
    
    return 0;
}
