/**
 * PREFIX_SUM / SCAN OPERATIONS TEST
 * Tests cumulative operations with both sequential and parallel implementations
 */

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../modules/json_linq.h"

using namespace fastjson;
using namespace fastjson::linq;

void test_basic_prefix_sum() {
    std::cout << "\n=== Test: Basic Prefix Sum ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5};
    auto result = from(numbers).prefix_sum().to_vector();

    std::vector<int> expected = {1, 3, 6, 10, 15};
    assert(result == expected);

    std::cout << "✓ Basic cumulative sum: [1,2,3,4,5] → [1,3,6,10,15]" << std::endl;
}

void test_scan_alias() {
    std::cout << "\n=== Test: Scan Alias ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5};
    auto result = from(numbers).scan().to_vector();

    std::vector<int> expected = {1, 3, 6, 10, 15};
    assert(result == expected);

    std::cout << "✓ scan() alias works: [1,2,3,4,5] → [1,3,6,10,15]" << std::endl;
}

void test_prefix_sum_with_custom_operation() {
    std::cout << "\n=== Test: Prefix Sum with Custom Operation ===" << std::endl;

    // Cumulative multiplication
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    auto result = from(numbers).prefix_sum([](int a, int b) { return a * b; }).to_vector();

    std::vector<int> expected = {1, 2, 6, 24, 120};  // Factorials
    assert(result == expected);

    std::cout << "✓ Cumulative product: [1,2,3,4,5] → [1,2,6,24,120]" << std::endl;
}

void test_prefix_sum_with_max() {
    std::cout << "\n=== Test: Prefix Sum with Max ===" << std::endl;

    std::vector<int> numbers = {3, 1, 4, 1, 5, 9, 2, 6};
    auto result = from(numbers).scan([](int a, int b) { return std::max(a, b); }).to_vector();

    std::vector<int> expected = {3, 3, 4, 4, 5, 9, 9, 9};  // Running maximum
    assert(result == expected);

    std::cout << "✓ Running maximum: [3,1,4,1,5,9,2,6] → [3,3,4,4,5,9,9,9]" << std::endl;
}

void test_prefix_sum_with_seed() {
    std::cout << "\n=== Test: Prefix Sum with Seed Value ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5};
    auto result = from(numbers).prefix_sum(100, [](int a, int b) { return a + b; }).to_vector();

    // Seed 100 plus cumulative sums
    std::vector<int> expected = {100, 101, 103, 106, 110, 115};
    assert(result == expected);
    assert(result.size() == numbers.size() + 1);  // Seed adds one element

    std::cout << "✓ Prefix sum with seed 100: [1,2,3,4,5] → [100,101,103,106,110,115]" << std::endl;
}

void test_prefix_sum_string_concatenation() {
    std::cout << "\n=== Test: Prefix Sum String Concatenation ===" << std::endl;

    std::vector<std::string> words = {"The", "quick", "brown", "fox"};
    auto result =
        from(words)
            .prefix_sum([](const std::string& a, const std::string& b) { return a + " " + b; })
            .to_vector();

    std::vector<std::string> expected = {"The", "The quick", "The quick brown",
                                         "The quick brown fox"};
    assert(result == expected);

    std::cout << "✓ String concatenation: " << result.back() << std::endl;
}

void test_prefix_sum_empty_collection() {
    std::cout << "\n=== Test: Prefix Sum on Empty Collection ===" << std::endl;

    std::vector<int> empty;
    auto result = from(empty).prefix_sum().to_vector();

    assert(result.empty());

    std::cout << "✓ Empty collection returns empty result" << std::endl;
}

void test_prefix_sum_single_element() {
    std::cout << "\n=== Test: Prefix Sum Single Element ===" << std::endl;

    std::vector<int> single = {42};
    auto result = from(single).prefix_sum().to_vector();

    assert(result.size() == 1);
    assert(result[0] == 42);

    std::cout << "✓ Single element [42] → [42]" << std::endl;
}

void test_parallel_prefix_sum() {
    std::cout << "\n=== Test: Parallel Prefix Sum ===" << std::endl;

    std::vector<int> numbers(10000);
    for (int i = 0; i < 10000; ++i) {
        numbers[i] = i + 1;
    }

    auto result = from_parallel(numbers).prefix_sum().to_vector();

    // Verify first few and last few values
    assert(result[0] == 1);
    assert(result[1] == 3);   // 1+2
    assert(result[2] == 6);   // 1+2+3
    assert(result[9] == 55);  // Sum of 1..10

    // Sum of 1..10000 = 10000*10001/2 = 50005000
    assert(result.back() == 50005000);

    std::cout << "✓ Parallel prefix sum on 10,000 elements: last value = " << result.back()
              << std::endl;
}

void test_parallel_prefix_product() {
    std::cout << "\n=== Test: Parallel Prefix Product ===" << std::endl;

    std::vector<int> numbers = {2, 3, 4, 5, 6};
    auto result = from_parallel(numbers).prefix_sum([](int a, int b) { return a * b; }).to_vector();

    std::vector<int> expected = {2, 6, 24, 120, 720};
    assert(result == expected);

    std::cout << "✓ Parallel cumulative product: [2,3,4,5,6] → [2,6,24,120,720]" << std::endl;
}

void test_chained_with_prefix_sum() {
    std::cout << "\n=== Test: Chained Operations with Prefix Sum ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Filter even numbers, then cumulative sum
    auto result = from(numbers)
                      .where([](int x) { return x % 2 == 0; })  // [2,4,6,8,10]
                      .prefix_sum()                             // [2,6,12,20,30]
                      .to_vector();

    std::vector<int> expected = {2, 6, 12, 20, 30};
    assert(result == expected);

    std::cout << "✓ Filter evens → prefix_sum: [2,6,12,20,30]" << std::endl;
}

void test_prefix_sum_then_filter() {
    std::cout << "\n=== Test: Prefix Sum then Filter ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5};

    // Cumulative sum, then filter values > 6
    auto result = from(numbers)
                      .prefix_sum()                        // [1,3,6,10,15]
                      .where([](int x) { return x > 6; })  // [10,15]
                      .to_vector();

    std::vector<int> expected = {10, 15};
    assert(result == expected);

    std::cout << "✓ prefix_sum → filter (>6): [10,15]" << std::endl;
}

void test_running_average() {
    std::cout << "\n=== Test: Running Average Using Prefix Sum ===" << std::endl;

    std::vector<double> numbers = {10.0, 20.0, 30.0, 40.0, 50.0};

    // First get cumulative sums
    auto cumulative = from(numbers).prefix_sum().to_vector();

    // Then compute running averages
    std::vector<double> running_avg;
    for (size_t i = 0; i < cumulative.size(); ++i) {
        running_avg.push_back(cumulative[i] / (i + 1));
    }

    assert(std::abs(running_avg[0] - 10.0) < 0.01);  // 10/1
    assert(std::abs(running_avg[1] - 15.0) < 0.01);  // 30/2
    assert(std::abs(running_avg[2] - 20.0) < 0.01);  // 60/3
    assert(std::abs(running_avg[3] - 25.0) < 0.01);  // 100/4
    assert(std::abs(running_avg[4] - 30.0) < 0.01);  // 150/5

    std::cout << "✓ Running average: [10.0, 15.0, 20.0, 25.0, 30.0]" << std::endl;
}

void test_real_world_sales_cumulative() {
    std::cout << "\n=== Test: Real-World Sales Cumulative Revenue ===" << std::endl;

    struct Sale {
        std::string month;
        double revenue;
    };

    std::vector<Sale> sales = {
        {"Jan", 1000.0}, {"Feb", 1500.0}, {"Mar", 1200.0}, {"Apr", 1800.0}, {"May", 2000.0}};

    // Extract revenues and compute cumulative
    auto cumulative_revenue =
        from(sales).select([](const Sale& s) { return s.revenue; }).prefix_sum().to_vector();

    assert(std::abs(cumulative_revenue[0] - 1000.0) < 0.01);
    assert(std::abs(cumulative_revenue[1] - 2500.0) < 0.01);  // 1000+1500
    assert(std::abs(cumulative_revenue[2] - 3700.0) < 0.01);  // 2500+1200
    assert(std::abs(cumulative_revenue[3] - 5500.0) < 0.01);  // 3700+1800
    assert(std::abs(cumulative_revenue[4] - 7500.0) < 0.01);  // 5500+2000

    std::cout << "✓ Cumulative revenue over 5 months: $7,500" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   PREFIX_SUM / SCAN OPERATIONS TEST" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // Basic tests
        test_basic_prefix_sum();
        test_scan_alias();

        // Custom operations
        test_prefix_sum_with_custom_operation();
        test_prefix_sum_with_max();
        test_prefix_sum_with_seed();

        // String operations
        test_prefix_sum_string_concatenation();

        // Edge cases
        test_prefix_sum_empty_collection();
        test_prefix_sum_single_element();

        // Parallel operations
        test_parallel_prefix_sum();
        test_parallel_prefix_product();

        // Chained operations
        test_chained_with_prefix_sum();
        test_prefix_sum_then_filter();

        // Real-world scenarios
        test_running_average();
        test_real_world_sales_cumulative();

        std::cout << "\n========================================" << std::endl;
        std::cout << "   ALL PREFIX_SUM TESTS PASSED ✓" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
