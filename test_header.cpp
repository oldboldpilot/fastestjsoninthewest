#include "include/fastjson.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace fastjson;

auto test_basic_functionality() -> void {
    std::cout << "Testing basic functionality...\n";
    
    // Test parsing
    auto result = parse(R"({"name": "John", "age": 30, "hobbies": ["reading", "coding"]})");
    assert(result.has_value());
    
    auto& json = *result;
    assert(json.is_object());
    assert(json["name"].as_string() == "John");
    assert(json["age"].as_number() == 30);
    assert(json["hobbies"].is_array());
    assert(json["hobbies"].size() == 2);
    
    // Test building with fluent API
    auto person = make_object()
        .add("name", string_value("Alice"))
        .add("age", number_value(25))
        .add("active", boolean_value(true))
        .build();
    
    assert(person["name"].as_string() == "Alice");
    assert(person["age"].as_number() == 25);
    assert(person["active"].as_boolean() == true);
    
    // Test serialization
    auto json_str = stringify(person);
    std::cout << "Serialized: " << json_str << "\n";
    
    auto pretty_str = stringify(person, true);
    std::cout << "Pretty: \n" << pretty_str << "\n";
    
    std::cout << "✅ All tests passed!\n";
}

auto test_array_operations() -> void {
    std::cout << "Testing array operations...\n";
    
    auto arr = make_array()
        .append(number_value(1))
        .append(string_value("hello"))
        .append(boolean_value(true))
        .append(null_value())
        .build();
    
    assert(arr.size() == 4);
    assert(arr[0].as_number() == 1);
    assert(arr[1].as_string() == "hello");
    assert(arr[2].as_boolean() == true);
    assert(arr[3].is_null());
    
    std::cout << "Array JSON: " << stringify(arr) << "\n";
    std::cout << "✅ Array tests passed!\n";
}

auto test_error_handling() -> void {
    std::cout << "Testing error handling...\n";
    
    auto bad_result = parse(R"({"invalid": json})");
    assert(!bad_result.has_value());
    
    std::cout << "Error occurred (as expected)\n";
    std::cout << "✅ Error handling tests passed!\n";
}

auto test_literals() -> void {
    std::cout << "Testing JSON literals...\n";
    
    auto json_lit = R"({"test": "value", "number": 42})"_json;
    assert(json_lit.has_value());
    assert((*json_lit)["test"].as_string() == "value");
    assert((*json_lit)["number"].as_number() == 42);
    
    std::cout << "✅ Literal tests passed!\n";
}

auto main() -> int {
    std::cout << "FastestJSONInTheWest - Header-Only Version Test\n";
    std::cout << "================================================\n\n";
    
    try {
        test_basic_functionality();
        test_array_operations();
        test_error_handling();
        test_literals();
        
        std::cout << "\n🎉 All tests completed successfully!\n";
        std::cout << "Header-only version is working correctly.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}