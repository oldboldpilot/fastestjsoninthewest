/**
 * FastestJSONInTheWest Test Framework
 * 
 * Zero-dependency, modern C++23 test framework using std::expected
 * Based on sensen test framework patterns
 * 
 * Features:
 * - Thread-safe test registration and execution
 * - std::expected for explicit error handling
 * - Fluent API for assertions
 * - Zero external dependencies (no gtest, no cassert)
 * - Move-only test cases for performance
 * - Colored console output
 * - Auto test registration
 * 
 * Usage:
 *   #include "test_framework.h"
 *   
 *   TEST(SuiteName, TestName) {
 *       EXPECT_TRUE(condition);
 *       EXPECT_EQ(a, b);
 *       return {};  // Success
 *   }
 *   
 *   int main(int argc, char** argv) {
 *       return RUN_ALL_TESTS();
 *   }
 * 
 * Author: Olumuyiwa Oluwasanmi
 * Date: 2025-12-15
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fastjson::test_framework {

// ============================================================================
// Type Definitions
// ============================================================================

/// Test result type using std::expected for explicit error handling
/// Success = void, Failure = error message string
using TestResult = std::expected<void, std::string>;

/// Test function signature
using TestFunction = std::function<TestResult()>;

// ============================================================================
// Console Colors (ANSI escape codes)
// ============================================================================

namespace colors {
    constexpr const char* RESET   = "\033[0m";
    constexpr const char* RED     = "\033[31m";
    constexpr const char* GREEN   = "\033[32m";
    constexpr const char* YELLOW  = "\033[33m";
    constexpr const char* BLUE    = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN    = "\033[36m";
    constexpr const char* BOLD    = "\033[1m";
} // namespace colors

// ============================================================================
// Test Case Structure
// ============================================================================

struct TestCase {
    std::string suite;
    std::string name;
    TestFunction fn;
    bool run{false};
    bool passed{false};
    std::string error;
    double duration_ms{0.0};

    // Move-only for performance
    TestCase(const TestCase&) = delete;
    auto operator=(const TestCase&) -> TestCase& = delete;
    TestCase(TestCase&&) noexcept = default;
    auto operator=(TestCase&&) noexcept -> TestCase& = default;
    ~TestCase() = default;

    TestCase(std::string suite_name, std::string test_name, TestFunction test_fn)
        : suite(std::move(suite_name))
        , name(std::move(test_name))
        , fn(std::move(test_fn)) {}
};

// ============================================================================
// Thread-Safe Test Registry (Singleton)
// ============================================================================

class TestRegistry {
public:
    static auto instance() -> TestRegistry& {
        static TestRegistry registry;
        return registry;
    }

    auto add(std::string suite, std::string name, TestFunction fn) -> void {
        std::unique_lock lock(mutex_);
        tests_.emplace_back(std::move(suite), std::move(name), std::move(fn));
    }

    auto run_all(const std::string& filter = "") -> int {
        std::unique_lock lock(mutex_);
        
        size_t total = 0;
        size_t passed = 0;
        size_t failed = 0;
        size_t skipped = 0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::cout << colors::BOLD << colors::CYAN 
                  << "\n╔══════════════════════════════════════════════════════════════╗\n"
                  << "║              FastestJSONInTheWest Test Suite                 ║\n"
                  << "╚══════════════════════════════════════════════════════════════╝\n"
                  << colors::RESET << "\n";

        std::string current_suite;
        
        for (auto& test : tests_) {
            // Filter check
            if (!filter.empty()) {
                std::string full_name = test.suite + "." + test.name;
                if (full_name.find(filter) == std::string::npos) {
                    skipped++;
                    continue;
                }
            }
            
            // Print suite header
            if (test.suite != current_suite) {
                current_suite = test.suite;
                std::cout << colors::BOLD << colors::BLUE 
                          << "\n━━━ " << current_suite << " ━━━\n"
                          << colors::RESET;
            }
            
            total++;
            test.run = true;
            
            // Run test with timing
            auto test_start = std::chrono::high_resolution_clock::now();
            
            try {
                auto result = test.fn();
                
                auto test_end = std::chrono::high_resolution_clock::now();
                test.duration_ms = std::chrono::duration<double, std::milli>(
                    test_end - test_start).count();
                
                if (result.has_value()) {
                    test.passed = true;
                    passed++;
                    std::cout << colors::GREEN << "  ✓ " << colors::RESET 
                              << test.name 
                              << colors::CYAN << " (" << test.duration_ms << " ms)"
                              << colors::RESET << "\n";
                } else {
                    test.passed = false;
                    test.error = result.error();
                    failed++;
                    std::cout << colors::RED << "  ✗ " << colors::RESET 
                              << test.name << "\n"
                              << colors::RED << "    Error: " << test.error 
                              << colors::RESET << "\n";
                }
            } catch (const std::exception& e) {
                auto test_end = std::chrono::high_resolution_clock::now();
                test.duration_ms = std::chrono::duration<double, std::milli>(
                    test_end - test_start).count();
                test.passed = false;
                test.error = std::string("Exception: ") + e.what();
                failed++;
                std::cout << colors::RED << "  ✗ " << colors::RESET 
                          << test.name << "\n"
                          << colors::RED << "    Exception: " << e.what() 
                          << colors::RESET << "\n";
            } catch (...) {
                auto test_end = std::chrono::high_resolution_clock::now();
                test.duration_ms = std::chrono::duration<double, std::milli>(
                    test_end - test_start).count();
                test.passed = false;
                test.error = "Unknown exception";
                failed++;
                std::cout << colors::RED << "  ✗ " << colors::RESET 
                          << test.name << "\n"
                          << colors::RED << "    Unknown exception" 
                          << colors::RESET << "\n";
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double total_ms = std::chrono::duration<double, std::milli>(
            end_time - start_time).count();
        
        // Summary
        std::cout << colors::BOLD << colors::CYAN 
                  << "\n══════════════════════════════════════════════════════════════\n"
                  << colors::RESET;
        
        std::cout << "Tests run: " << total;
        if (skipped > 0) {
            std::cout << " (skipped: " << skipped << ")";
        }
        std::cout << " | ";
        
        if (passed > 0) {
            std::cout << colors::GREEN << "Passed: " << passed << colors::RESET << " | ";
        }
        if (failed > 0) {
            std::cout << colors::RED << "Failed: " << failed << colors::RESET;
        } else {
            std::cout << "Failed: 0";
        }
        
        std::cout << " | Total time: " << total_ms << " ms\n";
        
        if (failed == 0 && total > 0) {
            std::cout << colors::BOLD << colors::GREEN 
                      << "\n✓ All tests passed!\n" 
                      << colors::RESET;
        } else if (failed > 0) {
            std::cout << colors::BOLD << colors::RED 
                      << "\n✗ " << failed << " test(s) failed\n" 
                      << colors::RESET;
        }
        
        return failed > 0 ? 1 : 0;
    }

    auto clear() -> void {
        std::unique_lock lock(mutex_);
        tests_.clear();
    }

private:
    TestRegistry() = default;
    std::vector<TestCase> tests_;
    mutable std::shared_mutex mutex_;
};

// ============================================================================
// RAII Test Registrar
// ============================================================================

struct TestRegistrar {
    TestRegistrar(const char* suite, const char* name, TestFunction fn) {
        TestRegistry::instance().add(suite, name, std::move(fn));
    }
};

} // namespace fastjson::test_framework

// ============================================================================
// Assertion Macros
// ============================================================================

// EXPECT_TRUE: 1 or 2 parameters (expr) or (expr, msg)
#define EXPECT_TRUE_1(expr)                                                    \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #expr " to be true";                          \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_TRUE_2(expr, msg)                                               \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__ << ": " << (msg);               \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define GET_EXPECT_TRUE_MACRO(_1, _2, NAME, ...) NAME
#define EXPECT_TRUE(...)                                                       \
    GET_EXPECT_TRUE_MACRO(__VA_ARGS__, EXPECT_TRUE_2, EXPECT_TRUE_1)(__VA_ARGS__)

// EXPECT_FALSE: 1 or 2 parameters
#define EXPECT_FALSE_1(expr)                                                   \
    do {                                                                       \
        if (expr) {                                                            \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #expr " to be false";                         \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_FALSE_2(expr, msg)                                              \
    do {                                                                       \
        if (expr) {                                                            \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__ << ": " << (msg);               \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define GET_EXPECT_FALSE_MACRO(_1, _2, NAME, ...) NAME
#define EXPECT_FALSE(...)                                                      \
    GET_EXPECT_FALSE_MACRO(__VA_ARGS__, EXPECT_FALSE_2, EXPECT_FALSE_1)(__VA_ARGS__)

// EXPECT_EQ: 2 or 3 parameters (a, b) or (a, b, msg)
#define EXPECT_EQ_2(a, b)                                                      \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (val_a != val_b) {                                                  \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #a " == " #b << " (got " << val_a             \
                << " != " << val_b << ")";                                     \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_EQ_3(a, b, msg)                                                 \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (val_a != val_b) {                                                  \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__ << ": " << (msg)                \
                << " (got " << val_a << " != " << val_b << ")";                \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define GET_EXPECT_EQ_MACRO(_1, _2, _3, NAME, ...) NAME
#define EXPECT_EQ(...)                                                         \
    GET_EXPECT_EQ_MACRO(__VA_ARGS__, EXPECT_EQ_3, EXPECT_EQ_2)(__VA_ARGS__)

// EXPECT_NE: 2 or 3 parameters
#define EXPECT_NE_2(a, b)                                                      \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (val_a == val_b) {                                                  \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #a " != " #b;                                 \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_NE_3(a, b, msg)                                                 \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (val_a == val_b) {                                                  \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__ << ": " << (msg);               \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define GET_EXPECT_NE_MACRO(_1, _2, _3, NAME, ...) NAME
#define EXPECT_NE(...)                                                         \
    GET_EXPECT_NE_MACRO(__VA_ARGS__, EXPECT_NE_3, EXPECT_NE_2)(__VA_ARGS__)

// EXPECT_LT: 2 or 3 parameters
#define EXPECT_LT_2(a, b)                                                      \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (!(val_a < val_b)) {                                                \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #a " < " #b << " (got " << val_a              \
                << " >= " << val_b << ")";                                     \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_LT_3(a, b, msg)                                                 \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (!(val_a < val_b)) {                                                \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__ << ": " << (msg)                \
                << " (got " << val_a << " >= " << val_b << ")";                \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define GET_EXPECT_LT_MACRO(_1, _2, _3, NAME, ...) NAME
#define EXPECT_LT(...)                                                         \
    GET_EXPECT_LT_MACRO(__VA_ARGS__, EXPECT_LT_3, EXPECT_LT_2)(__VA_ARGS__)

// EXPECT_LE: 2 or 3 parameters
#define EXPECT_LE_2(a, b)                                                      \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (!(val_a <= val_b)) {                                               \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #a " <= " #b << " (got " << val_a             \
                << " > " << val_b << ")";                                      \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_LE_3(a, b, msg)                                                 \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (!(val_a <= val_b)) {                                               \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__ << ": " << (msg)                \
                << " (got " << val_a << " > " << val_b << ")";                 \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define GET_EXPECT_LE_MACRO(_1, _2, _3, NAME, ...) NAME
#define EXPECT_LE(...)                                                         \
    GET_EXPECT_LE_MACRO(__VA_ARGS__, EXPECT_LE_3, EXPECT_LE_2)(__VA_ARGS__)

// EXPECT_GT: 2 or 3 parameters
#define EXPECT_GT_2(a, b)                                                      \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (!(val_a > val_b)) {                                                \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #a " > " #b << " (got " << val_a              \
                << " <= " << val_b << ")";                                     \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_GT_3(a, b, msg)                                                 \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (!(val_a > val_b)) {                                                \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__ << ": " << (msg)                \
                << " (got " << val_a << " <= " << val_b << ")";                \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define GET_EXPECT_GT_MACRO(_1, _2, _3, NAME, ...) NAME
#define EXPECT_GT(...)                                                         \
    GET_EXPECT_GT_MACRO(__VA_ARGS__, EXPECT_GT_3, EXPECT_GT_2)(__VA_ARGS__)

// EXPECT_GE: 2 or 3 parameters
#define EXPECT_GE_2(a, b)                                                      \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (!(val_a >= val_b)) {                                               \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #a " >= " #b << " (got " << val_a             \
                << " < " << val_b << ")";                                      \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_GE_3(a, b, msg)                                                 \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        if (!(val_a >= val_b)) {                                               \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__ << ": " << (msg)                \
                << " (got " << val_a << " < " << val_b << ")";                 \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define GET_EXPECT_GE_MACRO(_1, _2, _3, NAME, ...) NAME
#define EXPECT_GE(...)                                                         \
    GET_EXPECT_GE_MACRO(__VA_ARGS__, EXPECT_GE_3, EXPECT_GE_2)(__VA_ARGS__)

// EXPECT_NEAR: 3 or 4 parameters (a, b, epsilon) or (a, b, epsilon, msg)
#define EXPECT_NEAR_3(a, b, epsilon)                                           \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        auto eps = (epsilon);                                                  \
        if (std::abs(val_a - val_b) > eps) {                                   \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #a " ~= " #b " (within " << eps               \
                << "), got diff = " << std::abs(val_a - val_b);                \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_NEAR_4(a, b, epsilon, msg)                                      \
    do {                                                                       \
        auto val_a = (a);                                                      \
        auto val_b = (b);                                                      \
        auto eps = (epsilon);                                                  \
        if (std::abs(val_a - val_b) > eps) {                                   \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__ << ": " << (msg)                \
                << " (diff = " << std::abs(val_a - val_b)                      \
                << ", epsilon = " << eps << ")";                               \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define GET_EXPECT_NEAR_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define EXPECT_NEAR(...)                                                       \
    GET_EXPECT_NEAR_MACRO(__VA_ARGS__, EXPECT_NEAR_4, EXPECT_NEAR_3)(__VA_ARGS__)

// Convenience macros for floating-point comparisons
#define EXPECT_FLOAT_EQ(a, b) EXPECT_NEAR(a, b, 1e-6f)
#define EXPECT_DOUBLE_EQ(a, b) EXPECT_NEAR(a, b, 1e-12)

// ASSERT macros (same as EXPECT - return on failure)
#define ASSERT_TRUE(...) EXPECT_TRUE(__VA_ARGS__)
#define ASSERT_FALSE(...) EXPECT_FALSE(__VA_ARGS__)
#define ASSERT_EQ(...) EXPECT_EQ(__VA_ARGS__)
#define ASSERT_NE(...) EXPECT_NE(__VA_ARGS__)
#define ASSERT_LT(...) EXPECT_LT(__VA_ARGS__)
#define ASSERT_LE(...) EXPECT_LE(__VA_ARGS__)
#define ASSERT_GT(...) EXPECT_GT(__VA_ARGS__)
#define ASSERT_GE(...) EXPECT_GE(__VA_ARGS__)
#define ASSERT_NEAR(...) EXPECT_NEAR(__VA_ARGS__)

// Exception testing macros
#define EXPECT_THROW(statement, exception_type)                                \
    do {                                                                       \
        bool threw = false;                                                    \
        try {                                                                  \
            statement;                                                         \
        } catch (const exception_type&) {                                      \
            threw = true;                                                      \
        } catch (...) {                                                        \
        }                                                                      \
        if (!threw) {                                                          \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #statement " to throw " #exception_type;      \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_NO_THROW(statement)                                             \
    do {                                                                       \
        try {                                                                  \
            statement;                                                         \
        } catch (const std::exception& e) {                                    \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #statement                                    \
                << " not to throw, but threw: " << e.what();                   \
            return std::unexpected(oss.str());                                 \
        } catch (...) {                                                        \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #statement                                    \
                << " not to throw, but threw unknown exception";               \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

// EXPECT_NULL and EXPECT_NOT_NULL for pointer checks
#define EXPECT_NULL(ptr)                                                       \
    do {                                                                       \
        if ((ptr) != nullptr) {                                                \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #ptr " to be null";                           \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_NOT_NULL(ptr)                                                   \
    do {                                                                       \
        if ((ptr) == nullptr) {                                                \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected " #ptr " to be non-null";                       \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

// String comparison macros
#define EXPECT_STREQ(a, b)                                                     \
    do {                                                                       \
        std::string val_a = (a);                                               \
        std::string val_b = (b);                                               \
        if (val_a != val_b) {                                                  \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected \"" << val_a << "\" == \"" << val_b << "\"";    \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

#define EXPECT_STRNE(a, b)                                                     \
    do {                                                                       \
        std::string val_a = (a);                                               \
        std::string val_b = (b);                                               \
        if (val_a == val_b) {                                                  \
            std::ostringstream oss;                                            \
            oss << __FILE__ << ":" << __LINE__                                 \
                << ": Expected \"" << val_a << "\" != \"" << val_b << "\"";    \
            return std::unexpected(oss.str());                                 \
        }                                                                      \
    } while (0)

// ============================================================================
// Test Definition Macro
// ============================================================================

#define TEST(suite, name)                                                      \
    static auto suite##_##name##_impl()                                        \
        -> fastjson::test_framework::TestResult;                               \
    static fastjson::test_framework::TestRegistrar                             \
        suite##_##name##_reg(#suite, #name, suite##_##name##_impl);             \
    static auto suite##_##name##_impl()                                        \
        -> fastjson::test_framework::TestResult

// ============================================================================
// Main Function Helper
// ============================================================================

#define RUN_ALL_TESTS()                                                        \
    fastjson::test_framework::TestRegistry::instance().run_all(                \
        argc > 1 ? argv[1] : "")
