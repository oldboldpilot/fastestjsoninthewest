/**
 * Test that std module PCM is built with binary compatibility to logger
 *
 * This test verifies:
 * 1. We can import std module
 * 2. The std module works with the same compiler flags as the logger
 * 3. Binary compatibility is maintained
 */

#include <iostream>

import std;
import logger;

int main() {
    std::cout << "Testing std module with logger...\n";

    // Test std functionality
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::cout << "Vector: ";
    for (const auto& n : numbers) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // Test std::format (C++23)
    std::string msg = std::format("Vector has {} elements", numbers.size());
    std::cout << msg << "\n";

    // Test ranges
    auto doubled = numbers | std::views::transform([](int n) { return n * 2; });
    std::cout << "Doubled: ";
    for (const auto& n : doubled) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // Test logger with std types
    auto& logger_instance = logger::Logger::getInstance();
    logger_instance.info("Logger working with std module!");
    logger_instance.info("Vector size: {}", numbers.size());

    std::cout << "✅ std module + logger binary compatibility verified!\n";
    return 0;
}
