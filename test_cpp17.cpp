// Test C++17 Compatible Header-Only JSON Library
// Author: Olumuyiwa Oluwasanmi

#include "include/fastjson17.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <cassert>

using namespace fastjson17;
using namespace fastjson17::literals;

void test_basic_operations() {
    std::cout << "Testing basic JSON operations...\n";
    
    // Test null
    auto null_val = json_value{};
    assert(null_val.is_null());
    assert(null_val.to_string() == "null");
    
    // Test boolean
    auto true_val = json_value{true};
    auto false_val = json_value{false};
    assert(true_val.is_boolean());
    assert(true_val.as_boolean() == true);
    assert(true_val.to_string() == "true");
    assert(false_val.to_string() == "false");
    
    // Test numbers
    auto int_val = json_value{42};
    auto double_val = json_value{3.14159};
    assert(int_val.is_number());
    assert(int_val.as_number() == 42.0);
    assert(double_val.as_number() == 3.14159);
    
    // Test strings
    auto str_val = json_value{"Hello, World!"};
    assert(str_val.is_string());
    assert(str_val.as_string() == "Hello, World!");
    assert(str_val.to_string() == "\"Hello, World!\"");
    
    std::cout << "✓ Basic operations passed\n";
}

void test_arrays() {
    std::cout << "Testing JSON arrays...\n";
    
    auto arr = json_value{json_array{}};
    arr.push_back(json_value{1});
    arr.push_back(json_value{2});
    arr.push_back(json_value{3});
    
    assert(arr.is_array());
    assert(arr.size() == 3);
    assert(arr[0].as_number() == 1);
    assert(arr[1].as_number() == 2);
    assert(arr[2].as_number() == 3);
    
    // Test array with mixed types
    auto mixed_arr = json_value{json_array{
        json_value{42},
        json_value{"hello"},
        json_value{true},
        json_value{}
    }};
    
    assert(mixed_arr.size() == 4);
    assert(mixed_arr[0].is_number());
    assert(mixed_arr[1].is_string());
    assert(mixed_arr[2].is_boolean());
    assert(mixed_arr[3].is_null());
    
    std::cout << "✓ Arrays passed\n";
}

void test_objects() {
    std::cout << "Testing JSON objects...\n";
    
    auto obj = json_value{json_object{}};
    obj["name"] = json_value{"John"};
    obj["age"] = json_value{30};
    obj["active"] = json_value{true};
    
    assert(obj.is_object());
    assert(obj.size() == 3);
    assert(obj.contains("name"));
    assert(obj["name"].as_string() == "John");
    assert(obj["age"].as_number() == 30);
    assert(obj["active"].as_boolean() == true);
    
    std::cout << "✓ Objects passed\n";
}

void test_parsing() {
    std::cout << "Testing JSON parsing...\n";
    
    // Test simple values
    auto null_result = parse("null");
    assert(null_result && null_result->is_null());
    
    auto bool_result = parse("true");
    assert(bool_result && bool_result->as_boolean() == true);
    
    auto num_result = parse("123.45");
    assert(num_result && num_result->as_number() == 123.45);
    
    auto str_result = parse("\"Hello\"");
    assert(str_result && str_result->as_string() == "Hello");
    
    // Test array
    auto arr_result = parse("[1, 2, 3]");
    assert(arr_result && arr_result->is_array());
    assert(arr_result->size() == 3);
    assert((*arr_result)[0].as_number() == 1);
    
    // Test object
    auto obj_result = parse(R"({"name":"John","age":30})");
    assert(obj_result && obj_result->is_object());
    assert((*obj_result)["name"].as_string() == "John");
    assert((*obj_result)["age"].as_number() == 30);
    
    // Test nested structure
    auto nested_result = parse(R"({
        "users": [
            {"name": "Alice", "age": 25},
            {"name": "Bob", "age": 30}
        ],
        "total": 2
    })");
    
    assert(nested_result);
    assert((*nested_result)["users"].is_array());
    assert((*nested_result)["users"].size() == 2);
    assert((*nested_result)["users"][0]["name"].as_string() == "Alice");
    assert((*nested_result)["total"].as_number() == 2);
    
    std::cout << "✓ Parsing passed\n";
}

void test_serialization() {
    std::cout << "Testing JSON serialization...\n";
    
    // Create complex JSON structure
    auto obj = make_object()
        .add("name", "John Doe")
        .add("age", 30)
        .add("active", true)
        .add("score", 95.5)
        .add("address", make_object()
            .add("street", "123 Main St")
            .add("city", "Anytown")
            .add("zip", "12345")
            .build())
        .add("hobbies", make_array()
            .append("reading")
            .append("swimming")
            .append("coding")
            .build())
        .build();
    
    std::string json_str = obj.to_string();
    std::cout << "Compact JSON:\n" << json_str << "\n\n";
    
    std::string pretty_json = obj.to_pretty_string();
    std::cout << "Pretty JSON:\n" << pretty_json << "\n\n";
    
    // Test round-trip
    auto parsed = parse(json_str);
    assert(parsed);
    assert((*parsed)["name"].as_string() == "John Doe");
    assert((*parsed)["age"].as_number() == 30);
    assert((*parsed)["hobbies"].size() == 3);
    
    std::cout << "✓ Serialization passed\n";
}

void test_error_handling() {
    std::cout << "Testing error handling...\n";
    
    // Test invalid JSON
    auto invalid_result = parse("invalid");
    assert(!invalid_result);
    
    auto incomplete_result = parse("{\"name\":");
    assert(!incomplete_result);
    
    auto extra_tokens_result = parse("true false");
    assert(!extra_tokens_result);
    
    std::cout << "✓ Error handling passed\n";
}

void test_string_escapes() {
    std::cout << "Testing string escaping...\n";
    
    auto escape_result = parse(R"("Hello\nWorld\t\"quoted\"\\backslash")");
    assert(escape_result);
    std::string expected = "Hello\nWorld\t\"quoted\"\\backslash";
    assert(escape_result->as_string() == expected);
    
    // Test Unicode escapes
    auto unicode_result = parse(R"("\u0048\u0065\u006C\u006C\u006F")");
    assert(unicode_result);
    assert(unicode_result->as_string() == "Hello");
    
    std::cout << "✓ String escaping passed\n";
}

void benchmark_performance() {
    std::cout << "Running performance benchmarks...\n";
    
    // Create large JSON structure
    auto large_obj = make_object().build();
    for (int i = 0; i < 1000; ++i) {
        large_obj[std::to_string(i)] = make_array()
            .append(i)
            .append(std::string("item_") + std::to_string(i))
            .append(i % 2 == 0)
            .build();
    }
    
    // Benchmark serialization
    auto start = std::chrono::high_resolution_clock::now();
    std::string json_str = large_obj.to_string();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto serialize_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Serialization time: " << serialize_time << " µs\n";
    std::cout << "JSON size: " << json_str.size() << " bytes\n";
    
    // Benchmark parsing
    start = std::chrono::high_resolution_clock::now();
    auto parsed_result = parse(json_str);
    end = std::chrono::high_resolution_clock::now();
    
    auto parse_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Parsing time: " << parse_time << " µs\n";
    
    assert(parsed_result);
    assert(parsed_result->size() == 1000);
    
    std::cout << "✓ Performance benchmark completed\n";
}

#ifdef FASTJSON_ENABLE_SIMD
void test_simd_capabilities() {
    std::cout << "Testing SIMD capabilities...\n";
    
    #if defined(__x86_64__) || defined(_M_X64)
    auto caps = fastjson17::detect_simd_capabilities();
    
    std::cout << "Detected SIMD capabilities:\n";
    if (caps & fastjson17::SIMD_SSE2) std::cout << "  ✓ SSE2\n";
    if (caps & fastjson17::SIMD_SSE3) std::cout << "  ✓ SSE3\n";
    if (caps & fastjson17::SIMD_SSSE3) std::cout << "  ✓ SSSE3\n";
    if (caps & fastjson17::SIMD_SSE41) std::cout << "  ✓ SSE4.1\n";
    if (caps & fastjson17::SIMD_SSE42) std::cout << "  ✓ SSE4.2\n";
    if (caps & fastjson17::SIMD_AVX) std::cout << "  ✓ AVX\n";
    if (caps & fastjson17::SIMD_AVX2) std::cout << "  ✓ AVX2\n";
    if (caps & fastjson17::SIMD_AVX512F) std::cout << "  ✓ AVX-512F\n";
    if (caps & fastjson17::SIMD_AVX512BW) std::cout << "  ✓ AVX-512BW\n";
    #endif
    
    std::cout << "✓ SIMD capabilities detected\n";
}
#endif

void test_json_literals() {
    std::cout << "Testing JSON literals...\n";
    
    auto obj_literal = R"({
        "name": "Test",
        "values": [1, 2, 3],
        "active": true
    })"_json;
    
    assert(obj_literal);
    assert((*obj_literal)["name"].as_string() == "Test");
    assert((*obj_literal)["values"].size() == 3);
    assert((*obj_literal)["active"].as_boolean() == true);
    
    std::cout << "✓ JSON literals passed\n";
}

int main() {
    std::cout << "FastJSON C++17 Compatibility Test Suite\n";
    std::cout << "=======================================\n\n";
    
    try {
        test_basic_operations();
        test_arrays();
        test_objects();
        test_parsing();
        test_serialization();
        test_error_handling();
        test_string_escapes();
        test_json_literals();
        
#ifdef FASTJSON_ENABLE_SIMD
        test_simd_capabilities();
#endif
        
        benchmark_performance();
        
        std::cout << "\n✅ All tests passed!\n";
        std::cout << "C++17 compatibility verified.\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}