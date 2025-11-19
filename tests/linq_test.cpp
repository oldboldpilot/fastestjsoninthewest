#include <cassert>
#include <iostream>

#include "modules/fastjson_parallel.cpp"
#include "modules/json_linq.h"

using namespace fastjson;
using namespace fastjson::linq;

void test_basic_linq() {
    std::cout << "=== Testing Basic LINQ Operations ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // WHERE
    auto evens = from(numbers).where([](int n) { return n % 2 == 0; }).to_vector();
    assert(evens.size() == 5);
    std::cout << "✓ WHERE: " << evens.size() << " even numbers" << std::endl;

    // SELECT
    auto squared = from(numbers).select([](int n) { return n * n; }).to_vector();
    assert(squared[0] == 1 && squared[9] == 100);
    std::cout << "✓ SELECT: squared numbers" << std::endl;

    // ORDER BY
    auto desc = from(numbers).order_by_descending([](int n) { return n; }).to_vector();
    assert(desc[0] == 10 && desc[9] == 1);
    std::cout << "✓ ORDER BY: descending order" << std::endl;

    // SUM
    auto sum = from(numbers).sum([](int n) { return n; });
    assert(sum == 55);
    std::cout << "✓ SUM: " << sum << std::endl;

    // MIN/MAX
    auto min = from(numbers).min([](int n) { return n; });
    auto max = from(numbers).max([](int n) { return n; });
    assert(min.value() == 1 && max.value() == 10);
    std::cout << "✓ MIN/MAX: " << min.value() << " / " << max.value() << std::endl;

    // ANY/ALL
    bool has_even = from(numbers).any([](int n) { return n % 2 == 0; });
    bool all_positive = from(numbers).all([](int n) { return n > 0; });
    assert(has_even && all_positive);
    std::cout << "✓ ANY/ALL: predicates work" << std::endl;

    // TAKE/SKIP
    auto first_three = from(numbers).take(3).to_vector();
    auto skip_two = from(numbers).skip(2).to_vector();
    assert(first_three.size() == 3 && skip_two.size() == 8);
    std::cout << "✓ TAKE/SKIP: " << first_three.size() << " / " << skip_two.size() << std::endl;

    // DISTINCT
    std::vector<int> dupes = {1, 2, 2, 3, 3, 3, 4};
    auto unique = from(dupes).distinct().to_vector();
    assert(unique.size() == 4);
    std::cout << "✓ DISTINCT: " << unique.size() << " unique values" << std::endl;

    // Chaining
    auto result = from(numbers)
                      .where([](int n) { return n > 5; })
                      .select([](int n) { return n * 2; })
                      .order_by([](int n) { return n; })
                      .take(3)
                      .to_vector();
    assert(result.size() == 3);
    std::cout << "✓ CHAINED: filter→map→sort→take" << std::endl;

    std::cout << "All basic LINQ tests passed! ✓\n" << std::endl;
}

void test_parallel_linq() {
    std::cout << "=== Testing Parallel LINQ Operations ===" << std::endl;

    std::vector<int> numbers;
    for (int i = 1; i <= 1000; ++i) {
        numbers.push_back(i);
    }

    // Parallel WHERE
    auto evens = from_parallel(numbers).where([](int n) { return n % 2 == 0; }).to_vector();
    assert(evens.size() == 500);
    std::cout << "✓ Parallel WHERE: " << evens.size() << " results" << std::endl;

    // Parallel SELECT
    auto doubled = from_parallel(numbers).select([](int n) { return n * 2; }).to_vector();
    assert(doubled.size() == 1000);
    std::cout << "✓ Parallel SELECT: " << doubled.size() << " results" << std::endl;

    // Parallel SUM
    auto sum = from_parallel(numbers).sum([](int n) { return n; });
    assert(sum == 500500);  // 1+2+...+1000 = n(n+1)/2 = 500500
    std::cout << "✓ Parallel SUM: " << sum << std::endl;

    // Parallel MIN/MAX
    auto min = from_parallel(numbers).min([](int n) { return n; });
    auto max = from_parallel(numbers).max([](int n) { return n; });
    assert(min.value() == 1 && max.value() == 1000);
    std::cout << "✓ Parallel MIN/MAX: " << min.value() << " / " << max.value() << std::endl;

    // Parallel COUNT
    auto count = from_parallel(numbers).count([](int n) { return n > 500; });
    assert(count == 500);
    std::cout << "✓ Parallel COUNT: " << count << " items" << std::endl;

    // Parallel ANY/ALL
    bool has_large = from_parallel(numbers).any([](int n) { return n > 900; });
    bool all_positive = from_parallel(numbers).all([](int n) { return n > 0; });
    assert(has_large && all_positive);
    std::cout << "✓ Parallel ANY/ALL: predicates work" << std::endl;

    std::cout << "All parallel LINQ tests passed! ✓\n" << std::endl;
}

void test_json_linq() {
    std::cout << "=== Testing JSON LINQ Operations ===" << std::endl;

    const char* json = R"([
        {"id": 1, "name": "Alice", "age": 30, "active": true},
        {"id": 2, "name": "Bob", "age": 25, "active": false},
        {"id": 3, "name": "Charlie", "age": 35, "active": true},
        {"id": 4, "name": "Diana", "age": 28, "active": true},
        {"id": 5, "name": "Eve", "age": 32, "active": false}
    ])";

    auto parsed = parse(json);
    assert(parsed.has_value());

    // Extract objects
    std::vector<json_object> objects;
    for (const auto& item : parsed->as_array()) {
        objects.push_back(item.as_object());
    }

    // Filter active users
    auto active = from(objects)
                      .where([](const json_object& obj) { return obj.at("active").as_bool(); })
                      .to_vector();
    assert(active.size() == 3);
    std::cout << "✓ Filter active: " << active.size() << " users" << std::endl;

    // Extract names
    auto names = from(objects)
                     .select([](const json_object& obj) { return obj.at("name").as_string(); })
                     .to_vector();
    assert(names.size() == 5);
    std::cout << "✓ Extract names: " << names.size() << " names" << std::endl;

    // Sum ages
    auto total_age =
        from(objects).sum([](const json_object& obj) { return obj.at("age").as_number(); });
    assert(total_age == 150.0);
    std::cout << "✓ Sum ages: " << total_age << std::endl;

    // Find oldest
    auto oldest =
        from(objects)
            .order_by_descending([](const json_object& obj) { return obj.at("age").as_number(); })
            .first();
    assert(oldest.has_value());
    assert(oldest->at("name").as_string() == "Charlie");
    std::cout << "✓ Oldest: " << oldest->at("name").as_string() << std::endl;

    // Complex query: active users over 29
    auto result = from(objects)
                      .where([](const json_object& obj) {
                          return obj.at("active").as_bool() && obj.at("age").as_number() > 29;
                      })
                      .select([](const json_object& obj) { return obj.at("name").as_string(); })
                      .to_vector();
    // Alice (30, active), Charlie (35, active) = 2 results
    assert(result.size() == 2);
    std::cout << "✓ Complex query: " << result.size() << " results (";
    for (size_t i = 0; i < result.size(); ++i) {
        if (i > 0)
            std::cout << ", ";
        std::cout << result[i];
    }
    std::cout << ")" << std::endl;

    std::cout << "All JSON LINQ tests passed! ✓\n" << std::endl;
}

int main() {
    std::cout << "=============================================================\n";
    std::cout << "    LINQ and Parallel LINQ Tests\n";
    std::cout << "=============================================================\n\n";

    test_basic_linq();
    test_parallel_linq();
    test_json_linq();

    std::cout << "=============================================================\n";
    std::cout << "    All Tests Passed! ✓\n";
    std::cout << "=============================================================\n";

    return 0;
}
