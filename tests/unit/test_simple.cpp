#include <iostream>
import fastjson;

int main() {
    std::cout << "Testing JSON object parsing after SIMD fix..." << std::endl;

    auto r1 = fastjson::parse(R"({"name": "test"})");
    auto r2 = fastjson::parse("{}");
    auto r3 = fastjson::parse(R"({"a": 1, "b": 2})");

    int pass = 0, fail = 0;

    if (r1.has_value()) {
        std::cout << "✓ Test 1 PASSED\n";
        pass++;
    } else {
        std::cout << "✗ Test 1 FAILED\n";
        fail++;
    }

    if (r2.has_value()) {
        std::cout << "✓ Test 2 PASSED\n";
        pass++;
    } else {
        std::cout << "✗ Test 2 FAILED\n";
        fail++;
    }

    if (r3.has_value()) {
        std::cout << "✓ Test 3 PASSED\n";
        pass++;
    } else {
        std::cout << "✗ Test 3 FAILED\n";
        fail++;
    }

    std::cout << "\n" << pass << " passed, " << fail << " failed\n";
    return (fail == 0) ? 0 : 1;
}
