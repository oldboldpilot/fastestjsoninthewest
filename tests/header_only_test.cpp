// FastestJSONInTheWest Header-Only Test
// Author: Olumuyiwa Oluwasanmi
// Simple test to verify the header-only implementation

#include <iostream>
#include <fastjson.hpp>

int main() {
    std::cout << "=== FastestJSONInTheWest Header-Only Test ===" << std::endl;
    
    try {
        // Test 1: Parse simple JSON
        std::cout << "\nTest 1: Parsing simple JSON" << std::endl;
        auto parse_result = fastjson::parse(R"({"name": "John", "age": 30, "active": true})");
        
        if (parse_result) {
            auto& json = *parse_result;
            std::cout << "✓ Parse successful" << std::endl;
            std::cout << "  Name: " << json["name"].as_string() << std::endl;
            std::cout << "  Age: " << json["age"].as_number() << std::endl;
            std::cout << "  Active: " << (json["active"].as_boolean() ? "true" : "false") << std::endl;
        } else {
            std::cout << "✗ Parse failed: " << parse_result.error().message << std::endl;
            return 1;
        }
        
        // Test 2: Build JSON with fluent API
        std::cout << "\nTest 2: Building JSON with fluent API" << std::endl;
        auto person = fastjson::make_object()
            .add("name", "Alice")
            .add("age", 25)
            .add("hobbies", fastjson::make_array()
                .add("reading")
                .add("swimming")
                .add("coding")
            )
            .add("address", fastjson::make_object()
                .add("city", "New York")
                .add("zip", "10001")
            );
        
        // Test 3: Serialize with pretty printing
        std::cout << "\nTest 3: Serialization with pretty printing" << std::endl;
        std::string json_str = fastjson::serialize(person, true);
        std::cout << "Generated JSON:\n" << json_str << std::endl;
        
        // Test 4: Round-trip test
        std::cout << "\nTest 4: Round-trip test" << std::endl;
        auto round_trip = fastjson::parse(json_str);
        if (round_trip) {
            std::cout << "✓ Round-trip successful" << std::endl;
            std::cout << "  Name: " << (*round_trip)["name"].as_string() << std::endl;
            std::cout << "  City: " << (*round_trip)["address"]["city"].as_string() << std::endl;
        } else {
            std::cout << "✗ Round-trip failed: " << round_trip.error().message << std::endl;
            return 1;
        }
        
        // Test 5: Trail calling syntax
        std::cout << "\nTest 5: Trail calling syntax" << std::endl;
        auto result = fastjson::object()
            .set("status", "success")
            .set("code", 200)
            .set("data", fastjson::array()
                .push_back("item1")
                .push_back("item2")
                .push_back("item3")
            );
        
        std::cout << "Trail calling result: " << fastjson::serialize(result) << std::endl;
        
        std::cout << "\n=== All tests passed! ✓ ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
        return 1;
    }
}