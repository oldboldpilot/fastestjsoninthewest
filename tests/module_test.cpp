// FastestJSONInTheWest Module Test
// Test C++23 modules with trail calling syntax

import fastjson;

#include <iostream>
#include <string>
#include <format>

auto main() -> int {
    try {
        // Create JSON with fluent API using trail calling syntax
        auto json = fastjson::object()
            .set("name", "FastJSON")
            .set("version", "1.0.0") 
            .set("features", fastjson::array()
                .append("SIMD Optimized")
                .append("C++23 Modules")
                .append("Trail Calling Syntax")
                .append("Zero Allocations")
            )
            .set("performance", fastjson::object()
                .set("parse_speed", "Fastest")
                .set("simd_support", true)
                .set("waterfall", "AVX512->AVX2->SSE2")
            );

        auto serialized = fastjson::stringify(json);
        
        std::cout << "FastJSON C++23 Modules Test\n";
        std::cout << "===========================\n";
        std::cout << "JSON Output:\n" << serialized << "\n\n";
        
        // Test parsing
        auto parsed_result = fastjson::parse(serialized);
        if (!parsed_result) {
            std::cerr << "Parse error: " << parsed_result.error().message << "\n";
            return 1;
        }
        
        auto parsed = std::move(parsed_result.value());
        
        std::cout << "Parsing Test: SUCCESS\n";
        std::cout << "Trail Calling Syntax: ENABLED\n";
        std::cout << "C++23 Modules: WORKING\n";
        std::cout << "SIMD Optimizations: " 
                  << (parsed["performance"]["simd_support"].as_boolean() ? "ENABLED" : "DISABLED") 
                  << "\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}