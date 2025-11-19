#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "modules/fastjson_parallel.cpp"
#include "modules/json_linq.h"

using namespace fastjson;
using namespace fastjson::linq;

void test_map_filter_reduce() {
    std::cout << "=== Testing map, filter, reduce ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // FILTER - functional style
    auto evens = from(numbers).filter([](int n) { return n % 2 == 0; }).to_vector();
    assert(evens.size() == 5);
    std::cout << "✓ filter: " << evens.size() << " even numbers" << std::endl;

    // MAP - functional style
    auto squared = from(numbers).map([](int n) { return n * n; }).to_vector();
    assert(squared[0] == 1 && squared[9] == 100);
    std::cout << "✓ map: squared numbers" << std::endl;

    // REDUCE - functional style (sum)
    auto sum = from(numbers).reduce(0, [](int acc, int n) { return acc + n; });
    assert(sum == 55);
    std::cout << "✓ reduce: sum = " << sum << std::endl;

    // REDUCE - functional style (product)
    auto product = from(numbers).filter([](int n) { return n <= 5; }).reduce(1, [](int acc, int n) {
        return acc * n;
    });
    assert(product == 120);  // 1*2*3*4*5 = 120
    std::cout << "✓ reduce: product of first 5 = " << product << std::endl;

    std::cout << "All map/filter/reduce tests passed! ✓\n" << std::endl;
}

void test_find() {
    std::cout << "=== Testing find and find_index ===" << std::endl;

    std::vector<int> numbers = {10, 20, 30, 40, 50};

    // FIND - search for element
    auto found = from(numbers).find([](int n) { return n > 25; });
    assert(found.has_value() && found.value() == 30);
    std::cout << "✓ find: found " << found.value() << std::endl;

    // FIND - not found
    auto not_found = from(numbers).find([](int n) { return n > 100; });
    assert(!not_found.has_value());
    std::cout << "✓ find: correctly returns nullopt when not found" << std::endl;

    // FIND_INDEX
    auto index = from(numbers).find_index([](int n) { return n == 40; });
    assert(index.has_value() && index.value() == 3);
    std::cout << "✓ find_index: found at index " << index.value() << std::endl;

    // FIND_INDEX - not found
    auto no_index = from(numbers).find_index([](int n) { return n == 99; });
    assert(!no_index.has_value());
    std::cout << "✓ find_index: correctly returns nullopt when not found" << std::endl;

    std::cout << "All find tests passed! ✓\n" << std::endl;
}

void test_zip() {
    std::cout << "=== Testing zip ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::vector<std::string> words = {"one", "two", "three", "four", "five"};

    // ZIP - combine two sequences
    auto pairs = from(numbers)
                     .zip(from(words),
                          [](int n, const std::string& s) { return std::to_string(n) + "=" + s; })
                     .to_vector();

    assert(pairs.size() == 5);
    assert(pairs[0] == "1=one");
    assert(pairs[2] == "3=three");
    std::cout << "✓ zip: combined " << pairs.size() << " pairs" << std::endl;
    std::cout << "  Examples: " << pairs[0] << ", " << pairs[2] << std::endl;

    // ZIP - default pair version
    auto simple_pairs = from(numbers).zip(from(words)).to_vector();

    assert(simple_pairs.size() == 5);
    assert(simple_pairs[0].first == 1 && simple_pairs[0].second == "one");
    std::cout << "✓ zip (pairs): created " << simple_pairs.size() << " pairs" << std::endl;

    // ZIP - with different lengths (should truncate to shorter)
    std::vector<int> short_list = {1, 2};
    auto truncated =
        from(numbers).zip(from(short_list), [](int a, int b) { return a + b; }).to_vector();

    assert(truncated.size() == 2);
    assert(truncated[0] == 2 && truncated[1] == 4);
    std::cout << "✓ zip: correctly truncates to shorter sequence" << std::endl;

    std::cout << "All zip tests passed! ✓\n" << std::endl;
}

void test_foreach() {
    std::cout << "=== Testing forEach ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5};

    // FOREACH - with side effects
    int sum = 0;
    from(numbers).forEach([&sum](int n) { sum += n; });
    assert(sum == 15);
    std::cout << "✓ forEach: sum via side effect = " << sum << std::endl;

    // FOREACH - print (capturing output would be complex, just verify it runs)
    std::cout << "  forEach output: ";
    from(numbers).forEach([](int n) { std::cout << n << " "; });
    std::cout << std::endl;
    std::cout << "✓ forEach: executed for all elements" << std::endl;

    // FOR_EACH - with mutation via reference
    std::vector<int> mutable_nums = {1, 2, 3, 4, 5};
    from(mutable_nums).for_each([](const int& n) {
        // Note: can't mutate through const reference, but demonstrates pattern
        (void)n;
    });
    std::cout << "✓ for_each: iterator pattern works" << std::endl;

    std::cout << "All forEach tests passed! ✓\n" << std::endl;
}

void test_fluent_chains() {
    std::cout << "=== Testing Fluent API Chains ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Chain 1: filter > map > reduce
    auto result1 = from(numbers)
                       .filter([](int n) { return n % 2 == 0; })  // {2, 4, 6, 8, 10}
                       .map([](int n) { return n * n; })          // {4, 16, 36, 64, 100}
                       .reduce(0, [](int acc, int n) { return acc + n; });
    assert(result1 == 220);  // 4+16+36+64+100 = 220
    std::cout << "✓ Chain: filter→map→reduce = " << result1 << std::endl;

    // Chain 2: map > filter > find
    auto result2 = from(numbers)
                       .map([](int n) { return n * 2; })  // {2, 4, 6, 8, 10, 12, 14, 16, 18, 20}
                       .filter([](int n) { return n > 10; })     // {12, 14, 16, 18, 20}
                       .find([](int n) { return n % 3 == 0; });  // Find first divisible by 3
    assert(result2.has_value() && result2.value() == 12);
    std::cout << "✓ Chain: map→filter→find = " << result2.value() << std::endl;

    // Chain 3: filter > map > order_by > take
    auto result3 = from(numbers)
                       .filter([](int n) { return n > 3; })  // {4, 5, 6, 7, 8, 9, 10}
                       .map([](int n) { return n * n; })     // {16, 25, 36, 49, 64, 81, 100}
                       .order_by([](int n) { return -n; })   // Descending
                       .take(3)
                       .to_vector();
    assert(result3.size() == 3);
    assert(result3[0] == 100 && result3[1] == 81 && result3[2] == 64);
    std::cout << "✓ Chain: filter→map→order_by→take = top 3" << std::endl;

    // Chain 4: Complex with zip and reduce
    std::vector<int> weights = {1, 2, 3, 4, 5};
    auto result4 = from(numbers)
                       .take(5)  // {1, 2, 3, 4, 5}
                       .zip(from(weights),
                            [](int n, int w) {
                                return n * w;  // {1, 4, 9, 16, 25}
                            })
                       .reduce(0, [](int acc, int n) { return acc + n; });
    assert(result4 == 55);  // 1+4+9+16+25 = 55
    std::cout << "✓ Chain: take→zip→reduce = " << result4 << std::endl;

    std::cout << "All fluent chain tests passed! ✓\n" << std::endl;
}

void test_closures() {
    std::cout << "=== Testing Closures (Lambda Captures) ===" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5};

    // Capture by value
    int threshold = 3;
    auto filtered = from(numbers).filter([threshold](int n) { return n > threshold; }).to_vector();
    assert(filtered.size() == 2);
    std::cout << "✓ Closure [value]: filtered " << filtered.size() << " items > " << threshold
              << std::endl;

    // Capture by reference
    int multiplier = 2;
    auto mapped = from(numbers).map([&multiplier](int n) { return n * multiplier; }).to_vector();
    assert(mapped[0] == 2 && mapped[4] == 10);
    std::cout << "✓ Closure [ref]: multiplied by " << multiplier << std::endl;

    // Modify captured variable
    multiplier = 3;
    auto mapped2 = from(numbers).map([&multiplier](int n) { return n * multiplier; }).to_vector();
    assert(mapped2[0] == 3 && mapped2[4] == 15);
    std::cout << "✓ Closure [ref]: multiplier changed to " << multiplier << std::endl;

    // Capture multiple variables
    int min_val = 2;
    int max_val = 4;
    auto range_filtered =
        from(numbers)
            .filter([min_val, max_val](int n) { return n >= min_val && n <= max_val; })
            .to_vector();
    assert(range_filtered.size() == 3);  // {2, 3, 4}
    std::cout << "✓ Closure [multi]: filtered range [" << min_val << ", " << max_val << "]"
              << std::endl;

    // Capture and accumulate (complex closure)
    std::vector<int> results;
    int running_sum = 0;
    from(numbers).forEach([&results, &running_sum](int n) {
        running_sum += n;
        results.push_back(running_sum);
    });
    assert(results.size() == 5);
    assert(results[4] == 15);  // 1+2+3+4+5
    std::cout << "✓ Closure [accumulate]: running sum calculated" << std::endl;

    std::cout << "All closure tests passed! ✓\n" << std::endl;
}

void test_parallel_functional() {
    std::cout << "=== Testing Parallel Functional Operations ===" << std::endl;

    std::vector<int> large_data;
    for (int i = 1; i <= 10000; ++i) {
        large_data.push_back(i);
    }

    // Parallel filter
    auto evens = from_parallel(large_data).filter([](int n) { return n % 2 == 0; }).to_vector();
    assert(evens.size() == 5000);
    std::cout << "✓ Parallel filter: " << evens.size() << " results" << std::endl;

    // Parallel map
    auto squared = from_parallel(large_data).map([](int n) { return n * n; }).to_vector();
    assert(squared.size() == 10000);
    std::cout << "✓ Parallel map: " << squared.size() << " results" << std::endl;

    // Parallel reduce
    auto sum = from_parallel(large_data).reduce(0, [](int acc, int n) { return acc + n; });
    assert(sum == 50005000);  // Sum 1..10000 = n(n+1)/2
    std::cout << "✓ Parallel reduce: sum = " << sum << std::endl;

    // Parallel forEach
    std::atomic<int> counter{0};
    from_parallel(large_data).forEach([&counter](int n) {
        if (n % 100 == 0) {
            counter++;
        }
    });
    assert(counter == 100);
    std::cout << "✓ Parallel forEach: processed " << counter << " special items" << std::endl;

    // Parallel chain
    auto result = from_parallel(large_data)
                      .filter([](int n) { return n % 3 == 0; })
                      .map([](int n) { return n * 2; })
                      .as_sequential()
                      .take(5)
                      .to_vector();
    assert(result.size() == 5);
    assert(result[0] == 6);  // First multiple of 3 is 3, * 2 = 6
    std::cout << "✓ Parallel chain: filter→map→take = " << result.size() << " results" << std::endl;

    std::cout << "All parallel functional tests passed! ✓\n" << std::endl;
}

void test_json_functional() {
    std::cout << "=== Testing JSON with Functional API ===" << std::endl;

    const char* json = R"([
        {"id": 1, "name": "Alice", "score": 85},
        {"id": 2, "name": "Bob", "score": 92},
        {"id": 3, "name": "Charlie", "score": 78},
        {"id": 4, "name": "Diana", "score": 95},
        {"id": 5, "name": "Eve", "score": 88}
    ])";

    auto parsed = parse(json);
    assert(parsed.has_value());

    std::vector<json_object> users;
    for (const auto& item : parsed->as_array()) {
        users.push_back(item.as_object());
    }

    // Filter high scorers, map to names
    auto high_scorers =
        from(users)
            .filter([](const json_object& u) { return u.at("score").as_number() >= 90; })
            .map([](const json_object& u) { return u.at("name").as_string(); })
            .to_vector();

    assert(high_scorers.size() == 2);  // Bob and Diana
    std::cout << "✓ Filter→map: " << high_scorers.size() << " high scorers (";
    for (size_t i = 0; i < high_scorers.size(); ++i) {
        if (i > 0)
            std::cout << ", ";
        std::cout << high_scorers[i];
    }
    std::cout << ")" << std::endl;

    // Find student with score > 90
    auto found =
        from(users).find([](const json_object& u) { return u.at("score").as_number() > 90; });
    assert(found.has_value());
    assert(found->at("name").as_string() == "Bob");
    std::cout << "✓ Find: " << found->at("name").as_string() << " (score > 90)" << std::endl;

    // Reduce - calculate average score
    auto total_score = from(users).reduce(
        0.0, [](double acc, const json_object& u) { return acc + u.at("score").as_number(); });
    double average = total_score / users.size();
    assert(std::abs(average - 87.6) < 0.1);  // (85+92+78+95+88)/5 = 87.6
    std::cout << "✓ Reduce: average score = " << average << std::endl;

    // Zip with ranks
    std::vector<int> ranks = {1, 2, 3, 4, 5};
    auto ranked = from(users)
                      .zip(from(ranks),
                           [](const json_object& u, int rank) {
                               return std::to_string(rank) + ". " + u.at("name").as_string() + " ("
                                      + std::to_string(static_cast<int>(u.at("score").as_number()))
                                      + ")";
                           })
                      .to_vector();

    assert(ranked.size() == 5);
    std::cout << "✓ Zip: created ranked list:" << std::endl;
    for (const auto& r : ranked) {
        std::cout << "    " << r << std::endl;
    }

    // ForEach - print all names
    std::cout << "  All users: ";
    from(users).forEach([](const json_object& u) { std::cout << u.at("name").as_string() << " "; });
    std::cout << std::endl;
    std::cout << "✓ forEach: iterated all users" << std::endl;

    std::cout << "All JSON functional tests passed! ✓\n" << std::endl;
}

int main() {
    std::cout << "=============================================================\n";
    std::cout << "    Functional Programming API Tests\n";
    std::cout << "    (map, filter, reduce, zip, find, forEach)\n";
    std::cout << "=============================================================\n\n";

    test_map_filter_reduce();
    test_find();
    test_zip();
    test_foreach();
    test_fluent_chains();
    test_closures();
    test_parallel_functional();
    test_json_functional();

    std::cout << "=============================================================\n";
    std::cout << "    All Functional API Tests Passed! ✓\n";
    std::cout << "=============================================================\n";

    return 0;
}
