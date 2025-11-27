// Type Mappings Demonstration for fastjson_parallel
// Shows how JSON types map to C++ standard library containers
// Author: FastestJSONInTheWest Project

import fastjson_parallel;
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <unordered_map>

using namespace fastjson_parallel;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n\n";
}

void demo_basic_type_mappings() {
    print_separator("Basic Type Mappings");
    
    std::string json = R"({
        "null_value": null,
        "boolean": true,
        "number": 42.5,
        "string": "Hello, World!",
        "array": [1, 2, 3, 4, 5],
        "object": {
            "name": "Alice",
            "age": 30,
            "city": "New York"
        }
    })";
    
    auto result = parse(json);
    if (!result.has_value()) {
        std::cerr << "Parse error: " << result.error().message << "\n";
        return;
    }
    
    auto& root = result.value();
    
    std::cout << "Type Mappings:\n";
    std::cout << "  null       → std::nullptr_t     ✓\n";
    std::cout << "  boolean    → bool               ✓ " << std::boolalpha << root["boolean"].as_bool() << "\n";
    std::cout << "  number     → double             ✓ " << root["number"].as_number() << "\n";
    std::cout << "  string     → std::string        ✓ \"" << root["string"].as_string() << "\"\n";
    std::cout << "  array      → std::vector        ✓ size=" << root["array"].size() << "\n";
    std::cout << "  object     → std::unordered_map ✓ size=" << root["object"].size() << "\n";
}

void demo_object_operations() {
    print_separator("Object Operations (std::unordered_map)");
    
    std::string json = R"({
        "user_id": 12345,
        "username": "alice",
        "email": "alice@example.com",
        "active": true,
        "score": 95.5
    })";
    
    auto result = parse(json);
    if (!result.has_value()) return;
    
    auto& root = result.value();
    const json_object& obj = root.as_object();
    
    // O(1) key lookup
    std::cout << "1. Fast O(1) Key Lookup:\n";
    if (obj.contains("username")) {
        std::cout << "   Username: " << obj.at("username").as_string() << "\n";
    }
    
    // Iterate over key-value pairs (unordered!)
    std::cout << "\n2. Iteration (unordered):\n";
    for (const auto& [key, value] : obj) {
        std::cout << "   " << std::setw(12) << key << " = ";
        if (value.is_string()) std::cout << "\"" << value.as_string() << "\"";
        else if (value.is_number()) std::cout << value.as_number();
        else if (value.is_bool()) std::cout << std::boolalpha << value.as_bool();
        std::cout << "\n";
    }
    
    // Get all keys
    std::cout << "\n3. Extract All Keys:\n";
    if (auto keys = json_utils::object_keys(root)) {
        std::cout << "   Keys: ";
        for (size_t i = 0; i < keys->size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << (*keys)[i];
        }
        std::cout << "\n";
    }
    
    // Check key existence
    std::cout << "\n4. Key Existence Check:\n";
    std::cout << "   Has 'email': " << std::boolalpha << json_utils::has_key(root, "email") << "\n";
    std::cout << "   Has 'password': " << std::boolalpha << json_utils::has_key(root, "password") << "\n";
    
    // Default value access
    std::cout << "\n5. Safe Access with Default:\n";
    json_value default_role(std::string("guest"));
    json_value role = json_utils::get_or(root, "role", default_role);
    std::cout << "   Role: " << role.as_string() << " (defaulted)\n";
}

void demo_array_operations() {
    print_separator("Array Operations (std::vector)");
    
    std::string json = R"({
        "scores": [95, 87, 92, 88, 91],
        "names": ["Alice", "Bob", "Charlie", "Diana"]
    })";
    
    auto result = parse(json);
    if (!result.has_value()) return;
    
    auto& root = result.value();
    
    // O(1) indexed access
    std::cout << "1. Fast O(1) Index Access:\n";
    const json_array& scores = root["scores"].as_array();
    std::cout << "   First score: " << scores[0].as_number() << "\n";
    std::cout << "   Last score: " << scores[scores.size() - 1].as_number() << "\n";
    
    // Range-based iteration
    std::cout << "\n2. Range-Based Iteration:\n   Scores: ";
    for (const auto& score : scores) {
        std::cout << score.as_number() << " ";
    }
    std::cout << "\n";
    
    // STL algorithms
    std::cout << "\n3. STL Algorithms (std::max_element):\n";
    auto max_it = std::max_element(scores.begin(), scores.end(),
        [](const json_value& a, const json_value& b) {
            return a.as_number() < b.as_number();
        });
    if (max_it != scores.end()) {
        std::cout << "   Max score: " << max_it->as_number() << "\n";
    }
    
    // Extract typed array
    std::cout << "\n4. Extract Typed Array:\n";
    if (auto names = json_utils::array_to_strings(root["names"])) {
        std::cout << "   Names (std::vector<std::string>): ";
        for (size_t i = 0; i < names->size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << (*names)[i];
        }
        std::cout << "\n";
    }
    
    // Filter array
    std::cout << "\n5. Filter Array (scores > 90):\n";
    if (auto high_scores = json_utils::filter_array(root["scores"],
        [](const json_value& v) { return v.as_number() > 90; })) {
        std::cout << "   High scores: ";
        for (const auto& score : *high_scores) {
            std::cout << score.as_number() << " ";
        }
        std::cout << "\n";
    }
}

void demo_building_json() {
    print_separator("Building JSON Programmatically");
    
    // Create object using unordered_map
    json_object user;
    user["id"] = json_value(12345);
    user["name"] = json_value("Alice Johnson");
    user["email"] = json_value("alice@example.com");
    user["active"] = json_value(true);
    
    // Create array using vector
    json_array scores;
    scores.push_back(json_value(95.0));
    scores.push_back(json_value(87.0));
    scores.push_back(json_value(92.0));
    user["scores"] = json_value(scores);
    
    // Nested object
    json_object address;
    address["street"] = json_value("123 Main St");
    address["city"] = json_value("New York");
    address["zip"] = json_value("10001");
    user["address"] = json_value(address);
    
    // Create root value
    json_value root(user);
    
    std::cout << "Created user object with:\n";
    std::cout << "  - 6 top-level fields (unordered_map)\n";
    std::cout << "  - 1 nested array with " << scores.size() << " elements (vector)\n";
    std::cout << "  - 1 nested object with 3 fields (unordered_map)\n";
    
    // Convert to JSON string
    std::string json_str = stringify(root);
    std::cout << "\nGenerated JSON:\n" << json_str << "\n";
}

void demo_complex_operations() {
    print_separator("Complex Operations");
    
    std::string json = R"({
        "products": [
            {"id": 1, "name": "Laptop", "price": 999.99, "stock": 5, "category": "electronics"},
            {"id": 2, "name": "Mouse", "price": 29.99, "stock": 0, "category": "electronics"},
            {"id": 3, "name": "Desk", "price": 299.99, "stock": 12, "category": "furniture"},
            {"id": 4, "name": "Keyboard", "price": 79.99, "stock": 8, "category": "electronics"},
            {"id": 5, "name": "Chair", "price": 199.99, "stock": 3, "category": "furniture"}
        ]
    })";
    
    auto result = parse(json);
    if (!result.has_value()) return;
    
    auto& root = result.value();
    const json_array& products = root["products"].as_array();
    
    // 1. Filter in-stock electronics
    std::cout << "1. In-Stock Electronics:\n";
    for (const auto& product : products) {
        if (product["category"].as_string() == "electronics" && 
            product["stock"].as_number() > 0) {
            std::cout << "   - " << product["name"].as_string() 
                      << " ($" << product["price"].as_number() 
                      << ", stock: " << product["stock"].as_number() << ")\n";
        }
    }
    
    // 2. Calculate total inventory value
    std::cout << "\n2. Total Inventory Value:\n";
    double total_value = 0.0;
    for (const auto& product : products) {
        total_value += product["price"].as_number() * product["stock"].as_number();
    }
    std::cout << "   Total: $" << std::fixed << std::setprecision(2) << total_value << "\n";
    
    // 3. Group by category
    std::cout << "\n3. Products by Category:\n";
    std::unordered_map<std::string, int> category_counts;
    for (const auto& product : products) {
        category_counts[product["category"].as_string()]++;
    }
    for (const auto& [category, count] : category_counts) {
        std::cout << "   " << category << ": " << count << " products\n";
    }
    
    // 4. Find most expensive item
    std::cout << "\n4. Most Expensive Item:\n";
    auto max_price_it = std::max_element(products.begin(), products.end(),
        [](const json_value& a, const json_value& b) {
            return a["price"].as_number() < b["price"].as_number();
        });
    if (max_price_it != products.end()) {
        std::cout << "   " << (*max_price_it)["name"].as_string() 
                  << " at $" << (*max_price_it)["price"].as_number() << "\n";
    }
}

void demo_performance_characteristics() {
    print_separator("Performance Characteristics");
    
    std::cout << "Container Performance:\n\n";
    
    std::cout << "json_object (std::unordered_map):\n";
    std::cout << "  ✓ Lookup by key:    O(1) average, O(n) worst\n";
    std::cout << "  ✓ Insert:           O(1) average\n";
    std::cout << "  ✓ Erase:            O(1) average\n";
    std::cout << "  ✓ Memory:           Hash table overhead\n";
    std::cout << "  ✓ Iteration:        Unordered\n";
    std::cout << "  ✓ Best for:         Key-value lookups, caching\n\n";
    
    std::cout << "json_array (std::vector):\n";
    std::cout << "  ✓ Lookup by index:  O(1)\n";
    std::cout << "  ✓ Insert (append):  O(1) amortized\n";
    std::cout << "  ✓ Insert (middle):  O(n)\n";
    std::cout << "  ✓ Erase:            O(n)\n";
    std::cout << "  ✓ Memory:           Contiguous, cache-friendly\n";
    std::cout << "  ✓ Iteration:        Sequential, very fast\n";
    std::cout << "  ✓ Best for:         Indexed access, sequential processing\n";
}

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   FastestJSONInTheWest - Type Mappings Demonstration      ║\n";
    std::cout << "║   Parallel Module: Optimized for Multi-Threaded Parsing   ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    try {
        demo_basic_type_mappings();
        demo_object_operations();
        demo_array_operations();
        demo_building_json();
        demo_complex_operations();
        demo_performance_characteristics();
        
        print_separator("Summary");
        std::cout << "✓ json_object → std::unordered_map (fast lookups)\n";
        std::cout << "✓ json_array  → std::vector (dynamic sizing, O(1) access)\n";
        std::cout << "✓ json_string → std::string (UTF-8 native)\n";
        std::cout << "✓ json_number → double (standard precision)\n";
        std::cout << "✓ Plus 128-bit types for extended precision\n\n";
        
        std::cout << "All type mappings demonstrated successfully! ✨\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
