#include <iostream>
#include <string>

import fastjson;

int main() {
    using namespace fastjson;

    std::cout << "=== Testing Object Parsing Fix ===" << std::endl << std::endl;

    // Test 1: Simple object
    {
        std::string json = R"({"name": "test"})";
        std::cout << "Test 1: " << json << std::endl;
        auto result = parse(json);
        if (result.has_value()) {
            std::cout << "  ✓ SUCCESS - Parsed successfully" << std::endl;
            auto& obj = result->as_object();
            std::cout << "  ✓ name = '" << obj.at("name").as_string() << "'" << std::endl;
        } else {
            std::cout << "  ✗ FAILED: " << result.error() << std::endl;
            return 1;
        }
    }

    // Test 2: Empty object
    {
        std::string json = "{}";
        std::cout << "\nTest 2: " << json << std::endl;
        auto result = parse(json);
        if (result.has_value()) {
            std::cout << "  ✓ SUCCESS - Parsed successfully" << std::endl;
            auto& obj = result->as_object();
            std::cout << "  ✓ Object is empty (size=" << obj.size() << ")" << std::endl;
        } else {
            std::cout << "  ✗ FAILED: " << result.error() << std::endl;
            return 1;
        }
    }

    // Test 3: Multiple fields
    {
        std::string json = R"({"a": 1, "b": 2, "c": "hello"})";
        std::cout << "\nTest 3: " << json << std::endl;
        auto result = parse(json);
        if (result.has_value()) {
            std::cout << "  ✓ SUCCESS - Parsed successfully" << std::endl;
            auto& obj = result->as_object();
            std::cout << "  ✓ a = " << obj.at("a").as_number() << std::endl;
            std::cout << "  ✓ b = " << obj.at("b").as_number() << std::endl;
            std::cout << "  ✓ c = '" << obj.at("c").as_string() << "'" << std::endl;
        } else {
            std::cout << "  ✗ FAILED: " << result.error() << std::endl;
            return 1;
        }
    }

    // Test 4: Nested object
    {
        std::string json = R"({"user": {"name": "Alice", "age": 30}})";
        std::cout << "\nTest 4: " << json << std::endl;
        auto result = parse(json);
        if (result.has_value()) {
            std::cout << "  ✓ SUCCESS - Parsed successfully" << std::endl;
            auto& obj = result->as_object();
            auto& user = obj.at("user").as_object();
            std::cout << "  ✓ user.name = '" << user.at("name").as_string() << "'" << std::endl;
            std::cout << "  ✓ user.age = " << user.at("age").as_number() << std::endl;
        } else {
            std::cout << "  ✗ FAILED: " << result.error() << std::endl;
            return 1;
        }
    }

    // Test 5: Array of objects
    {
        std::string json = R"([{"id": 1}, {"id": 2}])";
        std::cout << "\nTest 5: " << json << std::endl;
        auto result = parse(json);
        if (result.has_value()) {
            std::cout << "  ✓ SUCCESS - Parsed successfully" << std::endl;
            auto& arr = result->as_array();
            std::cout << "  ✓ Array size = " << arr.size() << std::endl;
            std::cout << "  ✓ arr[0].id = " << arr[0].as_object().at("id").as_number() << std::endl;
            std::cout << "  ✓ arr[1].id = " << arr[1].as_object().at("id").as_number() << std::endl;
        } else {
            std::cout << "  ✗ FAILED: " << result.error() << std::endl;
            return 1;
        }
    }

    std::cout << "\n=== All Tests Passed! ===" << std::endl;
    return 0;
}
