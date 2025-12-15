#include <iostream>
#include <string>

import fastjson;

int main() {
    std::string json1 = R"({"name": "test"})";
    std::string json2 = R"({})";
    std::string json3 = R"({"a": 1, "b": 2})";

    std::cout << "Testing: " << json1 << std::endl;
    auto result1 = fastjson::parse(json1);
    if (result1.has_value()) {
        std::cout << "SUCCESS: Parsed object with name:test" << std::endl;
    } else {
        std::cout << "FAILED: " << result1.error() << std::endl;
    }

    std::cout << "\nTesting: " << json2 << std::endl;
    auto result2 = fastjson::parse(json2);
    if (result2.has_value()) {
        std::cout << "SUCCESS: Parsed empty object" << std::endl;
    } else {
        std::cout << "FAILED: " << result2.error() << std::endl;
    }

    std::cout << "\nTesting: " << json3 << std::endl;
    auto result3 = fastjson::parse(json3);
    if (result3.has_value()) {
        std::cout << "SUCCESS: Parsed object with a:1, b:2" << std::endl;
    } else {
        std::cout << "FAILED: " << result3.error() << std::endl;
    }

    return 0;
}
