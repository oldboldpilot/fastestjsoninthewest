#include <iostream>
#include "../include/fastjson.hpp"

int main() {
    std::cout << "=== SIMD Detection Test ===" << std::endl;
    
    fastjson::simd::initialize_simd();
    std::cout << "SIMD initialization complete" << std::endl;
    
    std::cout << fastjson::simd::get_simd_info() << std::endl;
    
    std::cout << "Creating parser..." << std::endl;
    fastjson::parser p;
    std::cout << "Parser created successfully" << std::endl;
    
    std::cout << "Testing simple parsing..." << std::endl;
    auto result = p.parse("{}");
    if (result) {
        std::cout << "✓ Basic parsing works" << std::endl;
    } else {
        std::cout << "✗ Basic parsing failed: " << result.error().message << std::endl;
    }
    
    return 0;
}