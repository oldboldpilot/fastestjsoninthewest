/**
 * Parallel STL with Intel TBB Example for fastjson
 *
 * This example demonstrates using C++23 Parallel STL (std::execution policies)
 * with fastjson for high-performance parallel JSON processing.
 *
 * Requirements:
 * - ENABLE_PARALLEL_STL=ON in CMake (default)
 * - Intel TBB (automatically fetched)
 * - Clang 21+ with libc++
 *
 * Build:
 *   cmake .. -DENABLE_PARALLEL_STL=ON -G Ninja
 *   ninja
 */

#include <iostream>
#include <fstream>
#include <chrono>
#include <numeric>

import std;
import fastjson;

// Helper: Load file contents
std::string load_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        return "{}";  // Return empty JSON on error
    }
    return std::string(std::istreambuf_iterator<char>(file),
                      std::istreambuf_iterator<char>());
}

// Example 1: Parallel Batch JSON Parsing
void example_parallel_batch_parsing() {
    std::cout << "\n=== Example 1: Parallel Batch JSON Parsing ===\n";

    // Sample JSON strings (in real app, these would be loaded from files)
    std::vector<std::string> json_strings = {
        R"({"id": 1, "name": "Alice", "score": 95})",
        R"({"id": 2, "name": "Bob", "score": 87})",
        R"({"id": 3, "name": "Charlie", "score": 92})",
        R"({"id": 4, "name": "Diana", "score": 88})",
        R"({"id": 5, "name": "Eve", "score": 91})",
        R"({"id": 6, "name": "Frank", "score": 85})",
        R"({"id": 7, "name": "Grace", "score": 94})",
        R"({"id": 8, "name": "Henry", "score": 89})"
    };

    // Parse all JSON documents in parallel using std::execution::par_unseq
    // par_unseq = parallel + vectorized (SIMD)
    std::vector<fastjson::json_doc> documents(json_strings.size());

    auto start = std::chrono::high_resolution_clock::now();

    std::transform(std::execution::par_unseq,
        json_strings.begin(),
        json_strings.end(),
        documents.begin(),
        [](const auto& json_str) {
            return fastjson::parse(json_str);
        }
    );

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << std::format("Parsed {} documents in {} µs\n",
                            documents.size(), duration.count());

    // Process results
    std::cout << "Results:\n";
    for (const auto& doc : documents) {
        std::cout << std::format("  ID: {}, Name: {}, Score: {}\n",
                                doc["id"].as_int(),
                                doc["name"].as_string(),
                                doc["score"].as_int());
    }
}

// Example 2: Parallel Data Extraction with std::ranges
void example_parallel_data_extraction() {
    std::cout << "\n=== Example 2: Parallel Data Extraction ===\n";

    std::vector<std::string> json_strings = {
        R"({"product": "Laptop", "price": 1200, "stock": 15})",
        R"({"product": "Mouse", "price": 25, "stock": 150})",
        R"({"product": "Keyboard", "price": 75, "stock": 80})",
        R"({"product": "Monitor", "price": 300, "stock": 45})",
        R"({"product": "Headset", "price": 85, "stock": 60})"
    };

    // Parse in parallel
    std::vector<fastjson::json_doc> products(json_strings.size());
    std::transform(std::execution::par,
        json_strings.begin(),
        json_strings.end(),
        products.begin(),
        [](const auto& json_str) { return fastjson::parse(json_str); }
    );

    // Extract prices in parallel
    std::vector<int> prices(products.size());
    std::transform(std::execution::par,
        products.begin(),
        products.end(),
        prices.begin(),
        [](const auto& product) { return product["price"].as_int(); }
    );

    // Calculate total value in parallel
    int total_value = std::transform_reduce(std::execution::par,
        products.begin(),
        products.end(),
        0,  // Initial value
        std::plus<>{},  // Reduction operation
        [](const auto& product) {
            return product["price"].as_int() * product["stock"].as_int();
        }
    );

    std::cout << std::format("Total inventory value: ${}\n", total_value);
    std::cout << "Prices: ";
    for (auto price : prices) {
        std::cout << std::format("${} ", price);
    }
    std::cout << "\n";
}

// Example 3: Parallel Filter and Transform
void example_parallel_filter_transform() {
    std::cout << "\n=== Example 3: Parallel Filter and Transform ===\n";

    std::vector<std::string> json_strings = {
        R"({"user": "alice", "age": 25, "active": true})",
        R"({"user": "bob", "age": 17, "active": true})",
        R"({"user": "charlie", "age": 30, "active": false})",
        R"({"user": "diana", "age": 22, "active": true})",
        R"({"user": "eve", "age": 16, "active": true})",
        R"({"user": "frank", "age": 35, "active": true})"
    };

    // Parse all documents in parallel
    std::vector<fastjson::json_doc> users(json_strings.size());
    std::transform(std::execution::par_unseq,
        json_strings.begin(),
        json_strings.end(),
        users.begin(),
        [](const auto& json_str) { return fastjson::parse(json_str); }
    );

    // Find all active adult users (age >= 18)
    // This uses sequential execution for the filter, but you can parallelize
    // the subsequent processing
    std::vector<fastjson::json_doc> active_adults;
    std::copy_if(users.begin(), users.end(),
        std::back_inserter(active_adults),
        [](const auto& user) {
            return user["age"].as_int() >= 18 && user["active"].as_bool();
        }
    );

    std::cout << std::format("Found {} active adult users:\n", active_adults.size());
    for (const auto& user : active_adults) {
        std::cout << std::format("  {} (age {})\n",
                                user["user"].as_string(),
                                user["age"].as_int());
    }
}

// Example 4: Parallel Aggregation
void example_parallel_aggregation() {
    std::cout << "\n=== Example 4: Parallel Aggregation ===\n";

    std::vector<std::string> json_strings;

    // Generate sample transaction data
    for (int i = 0; i < 100; ++i) {
        json_strings.push_back(
            std::format(R"({{"transaction_id": {}, "amount": {}, "category": "{}"}}})",
                       i, 10 + (i % 50), (i % 3 == 0) ? "food" : (i % 3 == 1) ? "tech" : "other")
        );
    }

    // Parse in parallel
    std::vector<fastjson::json_doc> transactions(json_strings.size());
    std::transform(std::execution::par_unseq,
        json_strings.begin(),
        json_strings.end(),
        transactions.begin(),
        [](const auto& json_str) { return fastjson::parse(json_str); }
    );

    // Calculate total amount using parallel reduce
    int total_amount = std::transform_reduce(std::execution::par,
        transactions.begin(),
        transactions.end(),
        0,
        std::plus<>{},
        [](const auto& transaction) {
            return transaction["amount"].as_int();
        }
    );

    // Find max transaction
    auto max_transaction = *std::max_element(std::execution::par,
        transactions.begin(),
        transactions.end(),
        [](const auto& a, const auto& b) {
            return a["amount"].as_int() < b["amount"].as_int();
        }
    );

    std::cout << std::format("Processed {} transactions\n", transactions.size());
    std::cout << std::format("Total amount: ${}\n", total_amount);
    std::cout << std::format("Max transaction: ${} (ID: {})\n",
                            max_transaction["amount"].as_int(),
                            max_transaction["transaction_id"].as_int());
}

// Example 5: Execution Policy Comparison
void example_execution_policy_comparison() {
    std::cout << "\n=== Example 5: Execution Policy Comparison ===\n";

    // Generate large dataset
    std::vector<std::string> json_strings;
    for (int i = 0; i < 1000; ++i) {
        json_strings.push_back(
            std::format(R"({{"id": {}, "value": {}}})", i, i * i)
        );
    }

    std::vector<fastjson::json_doc> docs(json_strings.size());

    // Sequential execution
    auto start_seq = std::chrono::high_resolution_clock::now();
    std::transform(std::execution::seq,
        json_strings.begin(),
        json_strings.end(),
        docs.begin(),
        [](const auto& json_str) { return fastjson::parse(json_str); }
    );
    auto end_seq = std::chrono::high_resolution_clock::now();
    auto duration_seq = std::chrono::duration_cast<std::chrono::microseconds>(end_seq - start_seq);

    // Parallel execution
    auto start_par = std::chrono::high_resolution_clock::now();
    std::transform(std::execution::par,
        json_strings.begin(),
        json_strings.end(),
        docs.begin(),
        [](const auto& json_str) { return fastjson::parse(json_str); }
    );
    auto end_par = std::chrono::high_resolution_clock::now();
    auto duration_par = std::chrono::duration_cast<std::chrono::microseconds>(end_par - start_par);

    // Parallel + Vectorized execution
    auto start_par_unseq = std::chrono::high_resolution_clock::now();
    std::transform(std::execution::par_unseq,
        json_strings.begin(),
        json_strings.end(),
        docs.begin(),
        [](const auto& json_str) { return fastjson::parse(json_str); }
    );
    auto end_par_unseq = std::chrono::high_resolution_clock::now();
    auto duration_par_unseq = std::chrono::duration_cast<std::chrono::microseconds>(end_par_unseq - start_par_unseq);

    std::cout << "Performance comparison (1000 documents):\n";
    std::cout << std::format("  Sequential (seq):        {} µs (1.00x)\n", duration_seq.count());
    std::cout << std::format("  Parallel (par):          {} µs ({:.2f}x speedup)\n",
                            duration_par.count(),
                            static_cast<double>(duration_seq.count()) / duration_par.count());
    std::cout << std::format("  Parallel+SIMD (par_unseq): {} µs ({:.2f}x speedup)\n",
                            duration_par_unseq.count(),
                            static_cast<double>(duration_seq.count()) / duration_par_unseq.count());
}

int main() {
    std::cout << "FastJSON Parallel STL Examples\n";
    std::cout << "================================\n";
    std::cout << "Using C++23 Parallel STL with Intel TBB backend\n";

    try {
        example_parallel_batch_parsing();
        example_parallel_data_extraction();
        example_parallel_filter_transform();
        example_parallel_aggregation();
        example_execution_policy_comparison();

        std::cout << "\n✅ All examples completed successfully!\n";
        std::cout << "\nKey Takeaways:\n";
        std::cout << "  • std::execution::seq - Single-threaded\n";
        std::cout << "  • std::execution::par - Multi-threaded with TBB\n";
        std::cout << "  • std::execution::par_unseq - Multi-threaded + SIMD\n";
        std::cout << "  • Works seamlessly with std::ranges and C++23 features\n";
        std::cout << "  • No manual thread management needed\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
