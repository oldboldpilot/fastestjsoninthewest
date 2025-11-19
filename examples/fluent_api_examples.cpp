#include <iomanip>
#include <iostream>

#include "modules/fastjson_parallel.cpp"
#include "modules/json_linq.h"

using namespace fastjson;
using namespace fastjson::linq;

// Example 1: Data Processing Pipeline
void example_data_pipeline() {
    std::cout << "\n=== Example 1: Data Processing Pipeline ===\n" << std::endl;

    // Sample sales data
    const char* sales_json = R"([
        {"product": "Laptop", "price": 1200, "quantity": 5, "category": "Electronics"},
        {"product": "Mouse", "price": 25, "quantity": 50, "category": "Electronics"},
        {"product": "Desk", "price": 350, "quantity": 10, "category": "Furniture"},
        {"product": "Chair", "price": 150, "quantity": 20, "category": "Furniture"},
        {"product": "Keyboard", "price": 75, "quantity": 30, "category": "Electronics"},
        {"product": "Monitor", "price": 300, "quantity": 15, "category": "Electronics"}
    ])";

    auto parsed = parse(sales_json);
    if (!parsed.has_value())
        return;

    std::vector<json_object> sales;
    for (const auto& item : parsed->as_array()) {
        sales.push_back(item.as_object());
    }

    // Calculate total revenue per product, filtered by category
    std::cout << "Electronics Revenue Report:" << std::endl;
    from(sales)
        .filter([](const json_object& sale) {
            return sale.at("category").as_string() == "Electronics";
        })
        .map([](const json_object& sale) {
            double revenue = sale.at("price").as_number() * sale.at("quantity").as_number();
            return std::make_pair(sale.at("product").as_string(), revenue);
        })
        .order_by([](const auto& pair) { return -pair.second; })  // Descending by revenue
        .forEach([](const auto& pair) {
            std::cout << "  " << std::setw(15) << std::left << pair.first << " $" << std::fixed
                      << std::setprecision(2) << pair.second << std::endl;
        });

    // Total revenue calculation
    auto total_revenue =
        from(sales)
            .map([](const json_object& sale) {
                return sale.at("price").as_number() * sale.at("quantity").as_number();
            })
            .reduce(0.0, [](double acc, double rev) { return acc + rev; });

    std::cout << "\nTotal Revenue: $" << std::fixed << std::setprecision(2) << total_revenue
              << std::endl;
}

// Example 2: Text Processing with Closures
void example_text_processing() {
    std::cout << "\n=== Example 2: Text Processing with Closures ===\n" << std::endl;

    std::vector<std::string> words = {"apple",  "banana", "apricot",    "blueberry", "avocado",
                                      "cherry", "date",   "elderberry", "fig",       "grape"};

    // User-defined filters using closures
    char prefix = 'a';
    int min_length = 5;

    std::cout << "Words starting with '" << prefix << "' and length >= " << min_length << ":"
              << std::endl;
    from(words)
        .filter([prefix](const std::string& w) { return !w.empty() && w[0] == prefix; })
        .filter([min_length](const std::string& w) {
            return w.length() >= static_cast<size_t>(min_length);
        })
        .map([](const std::string& w) {
            std::string upper = w;
            for (char& c : upper)
                c = std::toupper(c);
            return upper;
        })
        .forEach([](const std::string& w) { std::cout << "  - " << w << std::endl; });

    // Word length statistics
    auto lengths = from(words).map([](const std::string& w) { return w.length(); });

    auto min_len = lengths.min([](size_t n) { return n; });
    auto max_len = lengths.max([](size_t n) { return n; });
    auto avg_len = lengths.average([](size_t n) { return static_cast<double>(n); });

    std::cout << "\nWord Length Stats:" << std::endl;
    std::cout << "  Min: " << min_len.value() << std::endl;
    std::cout << "  Max: " << max_len.value() << std::endl;
    std::cout << "  Avg: " << std::fixed << std::setprecision(2) << avg_len.value() << std::endl;
}

// Example 3: Parallel Data Processing
void example_parallel_processing() {
    std::cout << "\n=== Example 3: Parallel Data Processing ===\n" << std::endl;

    // Generate large dataset
    std::vector<int> data;
    for (int i = 1; i <= 100000; ++i) {
        data.push_back(i);
    }

    std::cout << "Processing " << data.size() << " numbers in parallel..." << std::endl;

    // Parallel computation: sum of squares of even numbers
    auto result = from_parallel(data)
                      .filter([](int n) { return n % 2 == 0; })
                      .map([](int n) { return n * n; })
                      .reduce(0LL, [](long long acc, int n) { return acc + n; });

    std::cout << "Sum of squares of even numbers: " << result << std::endl;

    // Parallel statistics
    auto stats_min = from_parallel(data).min([](int n) { return n; });
    auto stats_max = from_parallel(data).max([](int n) { return n; });
    auto stats_sum = from_parallel(data).sum([](int n) { return n; });

    std::cout << "\nParallel Statistics:" << std::endl;
    std::cout << "  Min: " << stats_min.value() << std::endl;
    std::cout << "  Max: " << stats_max.value() << std::endl;
    std::cout << "  Sum: " << stats_sum << std::endl;
}

// Example 4: Complex Transformations with Zip
void example_zip_operations() {
    std::cout << "\n=== Example 4: Zip Operations ===\n" << std::endl;

    std::vector<std::string> names = {"Alice", "Bob", "Charlie", "Diana"};
    std::vector<int> scores = {95, 87, 92, 88};
    std::vector<std::string> grades = {"A", "B", "A", "B"};

    // Create student report using zip
    std::cout << "Student Report Card:" << std::endl;
    from(names)
        .zip(from(scores),
             [](const std::string& name, int score) { return std::make_pair(name, score); })
        .zip(from(grades),
             [](const auto& pair, const std::string& grade) {
                 return std::make_tuple(pair.first, pair.second, grade);
             })
        .forEach([](const auto& student) {
            std::cout << "  " << std::setw(10) << std::left << std::get<0>(student)
                      << " Score: " << std::setw(3) << std::get<1>(student)
                      << " Grade: " << std::get<2>(student) << std::endl;
        });

    // Calculate weighted average
    std::vector<double> weights = {0.3, 0.3, 0.2, 0.2};  // Different weights per student

    auto weighted_sum =
        from(scores)
            .zip(from(weights), [](int score, double weight) { return score * weight; })
            .reduce(0.0, [](double acc, double val) { return acc + val; });

    std::cout << "\nWeighted Average Score: " << std::fixed << std::setprecision(2) << weighted_sum
              << std::endl;
}

// Example 5: Finding and Searching
void example_find_operations() {
    std::cout << "\n=== Example 5: Find and Search Operations ===\n" << std::endl;

    const char* users_json = R"([
        {"id": 1, "username": "alice123", "email": "alice@example.com", "active": true},
        {"id": 2, "username": "bob456", "email": "bob@example.com", "active": false},
        {"id": 3, "username": "charlie789", "email": "charlie@example.com", "active": true},
        {"id": 4, "username": "diana012", "email": "diana@example.com", "active": true}
    ])";

    auto parsed = parse(users_json);
    if (!parsed.has_value())
        return;

    std::vector<json_object> users;
    for (const auto& item : parsed->as_array()) {
        users.push_back(item.as_object());
    }

    // Find active user by username pattern
    auto found_user = from(users).find([](const json_object& user) {
        return user.at("active").as_bool()
               && user.at("username").as_string().find("charlie") != std::string::npos;
    });

    if (found_user.has_value()) {
        std::cout << "Found active user: " << found_user->at("username").as_string() << " ("
                  << found_user->at("email").as_string() << ")" << std::endl;
    }

    // Find index of user with specific ID
    int target_id = 3;
    auto user_index = from(users).find_index(
        [target_id](const json_object& user) { return user.at("id").as_number() == target_id; });

    if (user_index.has_value()) {
        std::cout << "User with ID " << target_id << " is at index " << user_index.value()
                  << std::endl;
    }

    // Check if any inactive users exist
    bool has_inactive =
        from(users).any([](const json_object& user) { return !user.at("active").as_bool(); });

    std::cout << "Has inactive users: " << (has_inactive ? "Yes" : "No") << std::endl;

    // Get all active user emails
    std::cout << "\nActive user emails:" << std::endl;
    from(users)
        .filter([](const json_object& user) { return user.at("active").as_bool(); })
        .map([](const json_object& user) { return user.at("email").as_string(); })
        .forEach([](const std::string& email) { std::cout << "  - " << email << std::endl; });
}

// Example 6: Nested Fluent Chains
void example_nested_chains() {
    std::cout << "\n=== Example 6: Nested Fluent Chains ===\n" << std::endl;

    // Complex data transformation pipeline
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Multi-step transformation
    std::cout << "Pipeline: numbers → filter(even) → map(square) → filter(>20) → map(+100) → "
                 "sort(desc) → take(3)"
              << std::endl;

    auto result = from(numbers)
                      .filter([](int n) { return n % 2 == 0; })
                      .map([](int n) { return n * n; })
                      .filter([](int n) { return n > 20; })
                      .map([](int n) { return n + 100; })
                      .order_by_descending([](int n) { return n; })
                      .take(3);

    std::cout << "Results: ";
    result.forEach([](int n) { std::cout << n << " "; });
    std::cout << std::endl;

    // Verification with intermediate results
    std::cout << "\nStep-by-step breakdown:" << std::endl;

    auto step1 = from(numbers).filter([](int n) { return n % 2 == 0; });
    std::cout << "  After filter(even): ";
    step1.forEach([](int n) { std::cout << n << " "; });
    std::cout << std::endl;

    auto step2 = step1.map([](int n) { return n * n; });
    std::cout << "  After map(square): ";
    step2.forEach([](int n) { std::cout << n << " "; });
    std::cout << std::endl;

    auto step3 = step2.filter([](int n) { return n > 20; });
    std::cout << "  After filter(>20): ";
    step3.forEach([](int n) { std::cout << n << " "; });
    std::cout << std::endl;
}

int main() {
    std::cout << "=============================================================\n";
    std::cout << "    Fluent API Examples\n";
    std::cout << "    Showcasing map, filter, reduce, zip, find, forEach\n";
    std::cout << "=============================================================\n";

    example_data_pipeline();
    example_text_processing();
    example_parallel_processing();
    example_zip_operations();
    example_find_operations();
    example_nested_chains();

    std::cout << "\n=============================================================\n";
    std::cout << "    All Examples Complete!\n";
    std::cout << "=============================================================\n";

    return 0;
}
