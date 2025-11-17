// Test full parsing with debug
#include <iostream>
#include <iomanip>
#include "../modules/fastjson_parallel.cpp"

int main() {
    // Test musical note
    std::string json = R"({"text": "\uD834\uDD1E"})";
    
    std::cout << "Parsing: " << json << "\n\n";
    
    auto result = fastjson::parse(json);
    
    if (result.has_value()) {
        std::cout << "✓ Parse succeeded\n";
        if (result->is_object()) {
            const auto& obj = result->as_object();
            auto it = obj.find("text");
            if (it != obj.end() && it->second.is_string()) {
                const auto& str = it->second.as_string();
                std::cout << "Decoded: \"" << str << "\"\n";
                std::cout << "Bytes: ";
                for (unsigned char c : str) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') 
                              << static_cast<int>(c) << " ";
                }
                std::cout << std::dec << "\n";
            }
        }
    } else {
        std::cout << "✗ Parse failed: " << result.error().message << "\n";
    }
    
    return 0;
}
