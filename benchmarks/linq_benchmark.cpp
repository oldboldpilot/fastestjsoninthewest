#include "modules/fastjson_parallel.cpp"
#include "modules/json_linq.h"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <fstream>

using namespace fastjson;
using namespace fastjson::linq;

// Benchmark timer
class timer {
public:
    void start() { start_ = std::chrono::high_resolution_clock::now(); }
    
    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_;
};

// Generate test data
std::string generate_json_array(size_t count) {
    std::string json = "[";
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(1, 1000);
    
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"id\":" + std::to_string(i) + ",";
        json += "\"value\":" + std::to_string(dist(rng)) + ",";
        json += "\"name\":\"item_" + std::to_string(i) + "\",";
        json += "\"active\":" + std::string(i % 2 == 0 ? "true" : "false");
        json += "}";
    }
    json += "]";
    return json;
}

// Extract objects from JSON array
std::vector<json_object> extract_objects(const json_value& val) {
    std::vector<json_object> result;
    if (val.is_array()) {
        for (const auto& item : val.as_array()) {
            if (item.is_object()) {
                result.push_back(item.as_object());
            }
        }
    }
    return result;
}

// Benchmark: Filter (WHERE)
void benchmark_filter(size_t count) {
    std::cout << "\n=== Filter Benchmark (WHERE) - " << count << " items ===" << std::endl;
    
    auto json = generate_json_array(count);
    auto parsed = parse(json);
    if (!parsed.has_value()) return;
    
    auto objects = extract_objects(*parsed);
    timer t;
    
    // Manual loop
    t.start();
    std::vector<json_object> manual_result;
    for (const auto& obj : objects) {
        if (obj.at("value").as_number() > 500) {
            manual_result.push_back(obj);
        }
    }
    double manual_time = t.elapsed_ms();
    
    // LINQ sequential
    t.start();
    auto linq_result = from(objects)
        .where([](const json_object& obj) {
            return obj.at("value").as_number() > 500;
        })
        .to_vector();
    double linq_time = t.elapsed_ms();
    
    // LINQ parallel
    t.start();
    auto plinq_result = from_parallel(objects)
        .where([](const json_object& obj) {
            return obj.at("value").as_number() > 500;
        })
        .to_vector();
    double plinq_time = t.elapsed_ms();
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Manual loop:     " << std::setw(10) << manual_time << " ms (baseline)" << std::endl;
    std::cout << "  LINQ sequential: " << std::setw(10) << linq_time << " ms (" 
              << (linq_time / manual_time) << "x)" << std::endl;
    std::cout << "  LINQ parallel:   " << std::setw(10) << plinq_time << " ms (" 
              << (plinq_time / manual_time) << "x)" << std::endl;
    std::cout << "  Results match: " << (linq_result.size() == manual_result.size() ? "✓" : "✗") << std::endl;
}

// Benchmark: Transform (SELECT)
void benchmark_transform(size_t count) {
    std::cout << "\n=== Transform Benchmark (SELECT) - " << count << " items ===" << std::endl;
    
    auto json = generate_json_array(count);
    auto parsed = parse(json);
    if (!parsed.has_value()) return;
    
    auto objects = extract_objects(*parsed);
    timer t;
    
    // Manual loop
    t.start();
    std::vector<double> manual_result;
    for (const auto& obj : objects) {
        manual_result.push_back(obj.at("value").as_number() * 2.0);
    }
    double manual_time = t.elapsed_ms();
    
    // LINQ sequential
    t.start();
    auto linq_result = from(objects)
        .select([](const json_object& obj) {
            return obj.at("value").as_number() * 2.0;
        })
        .to_vector();
    double linq_time = t.elapsed_ms();
    
    // LINQ parallel
    t.start();
    auto plinq_result = from_parallel(objects)
        .select([](const json_object& obj) {
            return obj.at("value").as_number() * 2.0;
        })
        .to_vector();
    double plinq_time = t.elapsed_ms();
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Manual loop:     " << std::setw(10) << manual_time << " ms (baseline)" << std::endl;
    std::cout << "  LINQ sequential: " << std::setw(10) << linq_time << " ms (" 
              << (linq_time / manual_time) << "x)" << std::endl;
    std::cout << "  LINQ parallel:   " << std::setw(10) << plinq_time << " ms (" 
              << (plinq_time / manual_time) << "x)" << std::endl;
    std::cout << "  Results match: " << (linq_result.size() == manual_result.size() ? "✓" : "✗") << std::endl;
}

// Benchmark: Aggregate (SUM)
void benchmark_aggregate(size_t count) {
    std::cout << "\n=== Aggregate Benchmark (SUM) - " << count << " items ===" << std::endl;
    
    auto json = generate_json_array(count);
    auto parsed = parse(json);
    if (!parsed.has_value()) return;
    
    auto objects = extract_objects(*parsed);
    timer t;
    
    // Manual loop
    t.start();
    double manual_result = 0;
    for (const auto& obj : objects) {
        manual_result += obj.at("value").as_number();
    }
    double manual_time = t.elapsed_ms();
    
    // LINQ sequential
    t.start();
    auto linq_result = from(objects)
        .sum([](const json_object& obj) {
            return obj.at("value").as_number();
        });
    double linq_time = t.elapsed_ms();
    
    // LINQ parallel
    t.start();
    auto plinq_result = from_parallel(objects)
        .sum([](const json_object& obj) {
            return obj.at("value").as_number();
        });
    double plinq_time = t.elapsed_ms();
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Manual loop:     " << std::setw(10) << manual_time << " ms (baseline)" << std::endl;
    std::cout << "  LINQ sequential: " << std::setw(10) << linq_time << " ms (" 
              << (linq_time / manual_time) << "x)" << std::endl;
    std::cout << "  LINQ parallel:   " << std::setw(10) << plinq_time << " ms (" 
              << (plinq_time / manual_time) << "x)" << std::endl;
    std::cout << "  Results match: " << (std::abs(linq_result - manual_result) < 0.01 ? "✓" : "✗") << std::endl;
}

// Benchmark: Chained operations
void benchmark_chained(size_t count) {
    std::cout << "\n=== Chained Operations Benchmark - " << count << " items ===" << std::endl;
    std::cout << "    (filter > transform > sort > take)" << std::endl;
    
    auto json = generate_json_array(count);
    auto parsed = parse(json);
    if (!parsed.has_value()) return;
    
    auto objects = extract_objects(*parsed);
    timer t;
    
    // Manual loop
    t.start();
    std::vector<json_object> filtered;
    for (const auto& obj : objects) {
        if (obj.at("value").as_number() > 500) {
            filtered.push_back(obj);
        }
    }
    std::vector<double> transformed;
    for (const auto& obj : filtered) {
        transformed.push_back(obj.at("value").as_number() * 2.0);
    }
    std::sort(transformed.begin(), transformed.end());
    std::vector<double> manual_result;
    for (size_t i = 0; i < std::min(size_t(10), transformed.size()); ++i) {
        manual_result.push_back(transformed[i]);
    }
    double manual_time = t.elapsed_ms();
    
    // LINQ sequential
    t.start();
    auto linq_result = from(objects)
        .where([](const json_object& obj) {
            return obj.at("value").as_number() > 500;
        })
        .select([](const json_object& obj) {
            return obj.at("value").as_number() * 2.0;
        })
        .order_by([](double val) { return val; })
        .take(10)
        .to_vector();
    double linq_time = t.elapsed_ms();
    
    // LINQ parallel
    t.start();
    auto plinq_result = from_parallel(objects)
        .where([](const json_object& obj) {
            return obj.at("value").as_number() > 500;
        })
        .select([](const json_object& obj) {
            return obj.at("value").as_number() * 2.0;
        })
        .order_by([](double val) { return val; })
        .as_sequential()
        .take(10)
        .to_vector();
    double plinq_time = t.elapsed_ms();
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Manual loop:     " << std::setw(10) << manual_time << " ms (baseline)" << std::endl;
    std::cout << "  LINQ sequential: " << std::setw(10) << linq_time << " ms (" 
              << (linq_time / manual_time) << "x)" << std::endl;
    std::cout << "  LINQ parallel:   " << std::setw(10) << plinq_time << " ms (" 
              << (plinq_time / manual_time) << "x)" << std::endl;
    std::cout << "  Results match: " << (linq_result.size() == manual_result.size() ? "✓" : "✗") << std::endl;
}

// Benchmark: ANY predicate
void benchmark_any(size_t count) {
    std::cout << "\n=== Any Predicate Benchmark - " << count << " items ===" << std::endl;
    
    auto json = generate_json_array(count);
    auto parsed = parse(json);
    if (!parsed.has_value()) return;
    
    auto objects = extract_objects(*parsed);
    timer t;
    
    // Manual loop
    t.start();
    bool manual_result = false;
    for (const auto& obj : objects) {
        if (obj.at("value").as_number() > 900) {
            manual_result = true;
            break;
        }
    }
    double manual_time = t.elapsed_ms();
    
    // LINQ sequential
    t.start();
    auto linq_result = from(objects)
        .any([](const json_object& obj) {
            return obj.at("value").as_number() > 900;
        });
    double linq_time = t.elapsed_ms();
    
    // LINQ parallel
    t.start();
    auto plinq_result = from_parallel(objects)
        .any([](const json_object& obj) {
            return obj.at("value").as_number() > 900;
        });
    double plinq_time = t.elapsed_ms();
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Manual loop:     " << std::setw(10) << manual_time << " ms (baseline)" << std::endl;
    std::cout << "  LINQ sequential: " << std::setw(10) << linq_time << " ms (" 
              << (linq_time / manual_time) << "x)" << std::endl;
    std::cout << "  LINQ parallel:   " << std::setw(10) << plinq_time << " ms (" 
              << (plinq_time / manual_time) << "x)" << std::endl;
    std::cout << "  Results match: " << (linq_result == manual_result ? "✓" : "✗") << std::endl;
}

int main() {
    std::cout << "=============================================================\n";
    std::cout << "    LINQ vs Parallel LINQ Performance Benchmarks\n";
    std::cout << "=============================================================\n";
    
    std::vector<size_t> sizes = {1000, 10000, 100000, 1000000};
    
    for (size_t size : sizes) {
        benchmark_filter(size);
        benchmark_transform(size);
        benchmark_aggregate(size);
        benchmark_chained(size);
        benchmark_any(size);
        std::cout << std::endl;
    }
    
    std::cout << "\n=============================================================\n";
    std::cout << "    Benchmarks Complete\n";
    std::cout << "=============================================================\n";
    
    return 0;
}
