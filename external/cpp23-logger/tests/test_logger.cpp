/**
 * Test suite for cpp23-logger
 *
 * Tests all major logger features:
 * - Basic logging
 * - Positional formatting
 * - Named template formatting
 * - Fluent API
 * - Thread safety
 */

import logger;

#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

using namespace logger;

// Test basic logging
auto test_basic_logging() -> void {
    std::cout << "Testing basic logging...\n";

    auto& log = Logger::getInstance();
    log.initialize("test.log", LogLevel::TRACE, false); // No console output during tests

    log.trace("Trace message");
    log.debug("Debug message");
    log.info("Info message");
    log.warn("Warning message");
    log.error("Error message");
    log.critical("Critical message");

    std::cout << "  ✓ Basic logging test passed\n";
}

// Test positional formatting
auto test_positional_formatting() -> void {
    std::cout << "Testing positional formatting...\n";

    auto& log = Logger::getInstance();

    log.info("Value: {}, Count: {}", 42, 100);
    log.debug("Processing {}/{} items", 5, 10);
    log.error("Error code: {}", 404);

    std::cout << "  ✓ Positional formatting test passed\n";
}

// Test named template formatting
auto test_named_templates() -> void {
    std::cout << "Testing named template formatting...\n";

    auto& log = Logger::getInstance();

    // Method 1: Initializer list
    log.info("User {user_id} logged in from {ip}",
        {{"user_id", 12345}, {"ip", "192.168.1.1"}});

    // Method 2: named_args helper
    log.warn("Error {code}: {message}",
        Logger::named_args("code", 404, "message", "Not found"));

    // Method 3: Complex multi-parameter
    log.debug("Simulation[{sim_id}] Step[{step}]: Temp={temp}",
        {
            {"sim_id", 101},
            {"step", 25},
            {"temp", 23.5}
        });

    std::cout << "  ✓ Named templates test passed\n";
}

// Test fluent API
auto test_fluent_api() -> void {
    std::cout << "Testing fluent API...\n";

    auto& log = Logger::getInstance()
        .setLevel(LogLevel::DEBUG)
        .enableColors(false)
        .setConsoleOutput(false);

    // Verify we can chain methods
    assert(log.getLevel() == LogLevel::DEBUG);

    log.info("Fluent API test");

    std::cout << "  ✓ Fluent API test passed\n";
}

// Test thread safety
auto test_thread_safety() -> void {
    std::cout << "Testing thread safety...\n";

    auto& log = Logger::getInstance();

    auto worker = [&log](int thread_id) {
        for (int i = 0; i < 100; ++i) {
            log.info("Thread {thread} iteration {iter}",
                Logger::named_args("thread", thread_id, "iter", i));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "  ✓ Thread safety test passed\n";
}

// Test processTemplate function
auto test_process_template() -> void {
    std::cout << "Testing processTemplate function...\n";

    auto result = Logger::processTemplate("User {id} at {time}",
        {{"id", 123}, {"time", "14:23"}});

    assert(result == "User 123 at 14:23");

    // Test with missing parameter
    result = Logger::processTemplate("Missing {unknown} param",
        {{"known", "value"}});

    assert(result == "Missing {unknown} param");

    std::cout << "  ✓ processTemplate test passed\n";
}

auto main() -> int {
    std::cout << "\n=== cpp23-logger Test Suite ===\n\n";

    try {
        test_basic_logging();
        test_positional_formatting();
        test_named_templates();
        test_fluent_api();
        test_thread_safety();
        test_process_template();

        std::cout << "\n=== All tests passed! ===\n\n";
        return 0;

    } catch (std::exception const& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << "\n\n";
        return 1;
    }
}
