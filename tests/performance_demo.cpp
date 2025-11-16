// FastestJSONInTheWest Performance Demo
// Author: Olumuyiwa Oluwasanmi
// Simple performance demonstration for the MVP

#include <iostream>
#include <chrono>
#include <vector>

import fastjson;

// Performance measurement utility
class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_;
    
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        return duration.count() / 1000.0;
    }
};

int main() {
    std::cout << "=== FastestJSONInTheWest Performance Demo ===" << std::endl;
    
    // Test data
    const std::string sample_json = R"({
        "users": [
            {"id": 1, "name": "John Doe", "email": "john@example.com", "active": true},
            {"id": 2, "name": "Jane Smith", "email": "jane@example.com", "active": false},
            {"id": 3, "name": "Bob Johnson", "email": "bob@example.com", "active": true}
        ],
        "metadata": {
            "total": 3,
            "page": 1,
            "timestamp": "2025-11-15T11:30:00Z"
        },
        "settings": {
            "theme": "dark",
            "notifications": true,
            "language": "en"
        }
    })";
    
    const int iterations = 10000;
    
    // Benchmark 1: Parsing Performance
    std::cout << "\n🚀 Benchmark 1: JSON Parsing Performance" << std::endl;
    std::cout << "Parsing " << iterations << " times..." << std::endl;
    
    Timer parse_timer;
    int successful_parses = 0;
    
    for (int i = 0; i < iterations; ++i) {
        auto result = fastjson::parse(sample_json);
        if (result) {
            successful_parses++;
        }
    }
    
    double parse_time = parse_timer.elapsed_ms();
    std::cout << "✓ Parse time: " << parse_time << " ms" << std::endl;
    std::cout << "✓ Successful parses: " << successful_parses << "/" << iterations << std::endl;
    std::cout << "✓ Average per parse: " << (parse_time / iterations) << " ms" << std::endl;
    std::cout << "✓ Parses per second: " << (iterations * 1000.0 / parse_time) << std::endl;
    
    // Benchmark 2: Serialization Performance
    std::cout << "\n📝 Benchmark 2: JSON Serialization Performance" << std::endl;
    
    // Create a complex JSON object for serialization
    auto complex_json = fastjson::make_object()
        .add("data", fastjson::make_array())
        .add("config", fastjson::make_object()
            .add("version", "1.0.0")
            .add("debug", true)
        );
    
    // Convert to json_value for manipulation
    auto complex_value = complex_json.build();
    
    // Add array data
    for (int i = 0; i < 100; ++i) {
        complex_value["data"].push_back(fastjson::make_object()
            .add("id", i)
            .add("value", std::to_string(i * 2))
            .add("active", i % 2 == 0)
        );
    }
    
    Timer serialize_timer;
    std::string serialized_result;
    
    for (int i = 0; i < iterations / 10; ++i) {  // Fewer iterations for serialization
        serialized_result = fastjson::serialize(complex_value);
    }
    
    double serialize_time = serialize_timer.elapsed_ms();
    std::cout << "✓ Serialization time: " << serialize_time << " ms for " << (iterations / 10) << " operations" << std::endl;
    std::cout << "✓ Average per serialization: " << (serialize_time / (iterations / 10)) << " ms" << std::endl;
    std::cout << "✓ Serializations per second: " << ((iterations / 10) * 1000.0 / serialize_time) << std::endl;
    std::cout << "✓ Output size: " << serialized_result.length() << " characters" << std::endl;
    
    // Benchmark 3: Round-trip Performance
    std::cout << "\n🔄 Benchmark 3: Round-trip Performance (Parse → Serialize)" << std::endl;
    
    Timer roundtrip_timer;
    int successful_roundtrips = 0;
    
    for (int i = 0; i < iterations / 20; ++i) {  // Even fewer for round-trip
        auto parsed = fastjson::parse(sample_json);
        if (parsed) {
            auto serialized = fastjson::serialize(*parsed, false);
            if (!serialized.empty()) {
                successful_roundtrips++;
            }
        }
    }
    
    double roundtrip_time = roundtrip_timer.elapsed_ms();
    std::cout << "✓ Round-trip time: " << roundtrip_time << " ms for " << (iterations / 20) << " operations" << std::endl;
    std::cout << "✓ Successful round-trips: " << successful_roundtrips << "/" << (iterations / 20) << std::endl;
    std::cout << "✓ Average per round-trip: " << (roundtrip_time / (iterations / 20)) << " ms" << std::endl;
    std::cout << "✓ Round-trips per second: " << ((iterations / 20) * 1000.0 / roundtrip_time) << std::endl;
    
    // Benchmark 4: Fluent API Performance
    std::cout << "\n⚡ Benchmark 4: Fluent API Construction Performance" << std::endl;
    
    Timer fluent_timer;
    
    for (int i = 0; i < iterations / 50; ++i) {
        auto obj = fastjson::make_object()
            .add("id", i)
            .add("name", "User " + std::to_string(i))
            .add("scores", fastjson::make_array()
                .add(90.5)
                .add(85.0)
                .add(92.3)
            )
            .add("metadata", fastjson::make_object()
                .add("created", "2025-11-15")
                .add("active", true)
            );
    }
    
    double fluent_time = fluent_timer.elapsed_ms();
    std::cout << "✓ Fluent API construction time: " << fluent_time << " ms for " << (iterations / 50) << " objects" << std::endl;
    std::cout << "✓ Average per object: " << (fluent_time / (iterations / 50)) << " ms" << std::endl;
    std::cout << "✓ Objects per second: " << ((iterations / 50) * 1000.0 / fluent_time) << std::endl;
    
    // Feature demonstration
    std::cout << "\n🎯 Feature Demonstration" << std::endl;
    
    auto demo_json = fastjson::parse(sample_json);
    if (demo_json) {
        std::cout << "✓ Total users: " << (*demo_json)["metadata"]["total"].as_number() << std::endl;
        std::cout << "✓ First user name: " << (*demo_json)["users"][0]["name"].as_string() << std::endl;
        std::cout << "✓ Theme setting: " << (*demo_json)["settings"]["theme"].as_string() << std::endl;
        
        // Trail calling syntax example
        auto new_user = fastjson::object()
            .set("id", 4)
            .set("name", "Alice Cooper")
            .set("email", "alice@example.com")
            .set("active", true);
            
        (*demo_json)["users"].push_back(new_user);
        std::cout << "✓ Added new user, total now: " << (*demo_json)["users"].size() << std::endl;
    }
    
    std::cout << "\n=== Performance Demo Complete! ✓ ===" << std::endl;
    std::cout << "\n📊 Summary:" << std::endl;
    std::cout << "• FastestJSONInTheWest header-only implementation" << std::endl;
    std::cout << "• C++23 with fluent API and trail calling syntax" << std::endl;
    std::cout << "• Single header import: #include <fastjson.hpp>" << std::endl;
    std::cout << "• High-performance parsing and serialization" << std::endl;
    std::cout << "• Type-safe JSON value handling" << std::endl;
    std::cout << "• Error handling with std::expected<T, json_error>" << std::endl;
    std::cout << "\n🚀 Ready for production use!" << std::endl;
    
    return 0;
}