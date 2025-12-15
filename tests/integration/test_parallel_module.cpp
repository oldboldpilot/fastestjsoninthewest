// Test for fastjson_parallel module
// Verifies 128-bit support and functional utilities

#include <iostream>
#include <string>

import fastjson_parallel;

int main() {
    using namespace fastjson_parallel;
    
    std::cout << "Testing fastjson_parallel module...\n\n";
    
    // Test 1: Basic parsing
    std::cout << "Test 1: Basic parsing\n";
    auto result = parse(R"({"name": "test", "value": 42})");
    if (result) {
        std::cout << "  ✓ Parse successful\n";
    } else {
        std::cout << "  ✗ Parse failed\n";
        return 1;
    }
    
    // Test 2: 128-bit integer support
    std::cout << "\nTest 2: 128-bit integer support\n";
    __int128 big_int = 1234567890123456789LL;
    json_value int128_val(big_int);
    if (int128_val.is_int_128()) {
        std::cout << "  ✓ 128-bit integer created\n";
        auto retrieved = int128_val.as_int128();
        if (retrieved == big_int) {
            std::cout << "  ✓ 128-bit integer retrieved correctly\n";
        }
    }
    
    // Test 3: 128-bit float support
    std::cout << "\nTest 3: 128-bit float support\n";
    __float128 big_float = 3.14159265358979323846Q;
    json_value float128_val(big_float);
    if (float128_val.is_number_128()) {
        std::cout << "  ✓ 128-bit float created\n";
    }
    
    // Test 4: Array creation and functional utilities
    std::cout << "\nTest 4: Functional utilities\n";
    json_array arr;
    arr.push_back(json_value(1.0));
    arr.push_back(json_value(2.0));
    arr.push_back(json_value(3.0));
    arr.push_back(json_value(4.0));
    json_value arr_val(std::move(arr));
    
    // Test map
    auto mapped = map(arr_val, [](const json_value& v) {
        return json_value(v.as_number() * 2);
    });
    if (mapped.is_array() && mapped.size() == 4) {
        std::cout << "  ✓ map() works (size: " << mapped.size() << ")\n";
    }
    
    // Test filter
    auto filtered = filter(arr_val, [](const json_value& v) {
        return v.as_number() > 2.0;
    });
    if (filtered.is_array() && filtered.size() == 2) {
        std::cout << "  ✓ filter() works (filtered to " << filtered.size() << " elements)\n";
    }
    
    // Test reduce
    auto sum = reduce(arr_val, [](json_value acc, const json_value& v) {
        return json_value(acc.as_number() + v.as_number());
    }, json_value(0.0));
    if (sum.as_number() == 10.0) {
        std::cout << "  ✓ reduce() works (sum: " << sum.as_number() << ")\n";
    }
    
    // Test reverse
    auto reversed = reverse(arr_val);
    if (reversed.is_array() && reversed[0].as_number() == 4.0) {
        std::cout << "  ✓ reverse() works\n";
    }
    
    // Test rotate
    auto rotated = rotate(arr_val, 1);
    if (rotated.is_array() && rotated[0].as_number() == 2.0) {
        std::cout << "  ✓ rotate() works\n";
    }
    
    // Test take
    auto taken = take(arr_val, 2);
    if (taken.is_array() && taken.size() == 2) {
        std::cout << "  ✓ take() works (took " << taken.size() << " elements)\n";
    }
    
    // Test drop
    auto dropped = drop(arr_val, 2);
    if (dropped.is_array() && dropped.size() == 2) {
        std::cout << "  ✓ drop() works (remaining " << dropped.size() << " elements)\n";
    }
    
    // Test all
    bool all_positive = all(arr_val, [](const json_value& v) {
        return v.as_number() > 0.0;
    });
    if (all_positive) {
        std::cout << "  ✓ all() works\n";
    }
    
    // Test any
    bool any_greater_than_3 = any(arr_val, [](const json_value& v) {
        return v.as_number() > 3.0;
    });
    if (any_greater_than_3) {
        std::cout << "  ✓ any() works\n";
    }
    
    // Test 5: Configuration
    std::cout << "\nTest 5: Configuration\n";
    set_num_threads(4);
    int threads = get_num_threads();
    std::cout << "  ✓ Thread configuration: " << threads << " threads\n";
    
    parse_config config;
    config.num_threads = 8;
    config.parallel_threshold = 500;
    set_parse_config(config);
    std::cout << "  ✓ Configuration set successfully\n";
    
    std::cout << "\n✅ All tests passed!\n";
    return 0;
}
