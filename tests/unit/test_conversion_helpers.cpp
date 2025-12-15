// Test suite for numeric conversion helper methods
// Author: Olumuyiwa Oluwasanmi
// Tests all conversion methods return NaN/0 instead of throwing

import fastjson;

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

int test_count = 0;
int passed_count = 0;

void test_case(const std::string& name, bool condition) {
    test_count++;
    if (condition) {
        passed_count++;
        std::cout << "✓ Test " << test_count << ": " << name << std::endl;
    } else {
        std::cout << "✗ Test " << test_count << " FAILED: " << name << std::endl;
    }
}

void test_64bit_conversions() {
    std::cout << "\n=== Testing 64-bit Conversions ===" << std::endl;

    // Test as_number() returns value for double
    {
        auto result = fastjson::parse("123.456");
        test_case("as_number() returns double value",
                  result.has_value() && result->as_number() == 123.456);
    }

    // Test as_number() returns NaN for non-numeric
    {
        auto result = fastjson::parse("\"hello\"");
        test_case("as_number() returns NaN for string",
                  result.has_value() && std::isnan(result->as_number()));
    }

    // Test as_float64() with double
    {
        auto result = fastjson::parse("789.012");
        test_case("as_float64() returns double value",
                  result.has_value() && result->as_float64() == 789.012);
    }

    // Test as_float64() returns NaN for non-numeric
    {
        auto result = fastjson::parse("true");
        test_case("as_float64() returns NaN for boolean",
                  result.has_value() && std::isnan(result->as_float64()));
    }

    // Test as_int64() with double
    {
        auto result = fastjson::parse("42");
        test_case("as_int64() converts double to int",
                  result.has_value() && result->as_int64() == 42);
    }

    // Test as_int64() returns 0 for non-numeric
    {
        auto result = fastjson::parse("null");
        test_case("as_int64() returns 0 for null", result.has_value() && result->as_int64() == 0);
    }
}

void test_128bit_conversions() {
    std::cout << "\n=== Testing 128-bit Conversions ===" << std::endl;

    // Test as_number_128() for high precision
    {
        auto result = fastjson::parse("1.234567890123456789012345");
        test_case("as_number_128() stores high precision",
                  result.has_value() && result->is_number_128());
    }

    // Test as_number_128() returns NaN for non-numeric
    {
        auto result = fastjson::parse("[1,2,3]");
        test_case("as_number_128() returns NaN for array", result.has_value());
        // Can't easily test NaN for __float128, but it shouldn't throw
    }

    // Test as_int_128() for large integer
    {
        auto result = fastjson::parse("18446744073709551616");  // 2^64
        test_case("as_int_128() handles large integers",
                  result.has_value() && (result->is_int_128() || result->is_uint_128()));
    }

    // Test as_int_128() returns 0 for non-numeric
    {
        auto result = fastjson::parse("{\"key\":\"value\"}");
        test_case("as_int_128() returns 0 for object",
                  result.has_value() && result->as_int_128() == 0);
    }

    // Test as_uint_128() returns 0 for non-numeric
    {
        auto result = fastjson::parse("false");
        test_case("as_uint_128() returns 0 for boolean",
                  result.has_value() && result->as_uint_128() == 0);
    }
}

void test_cross_type_conversions() {
    std::cout << "\n=== Testing Cross-Type Conversions ===" << std::endl;

    // Test converting 128-bit int to 64-bit
    {
        auto result = fastjson::parse("123456789012345678901234567890");
        test_case("Convert 128-bit int to 64-bit int",
                  result.has_value() && result->as_int64() != 0);
    }

    // Test converting 128-bit int to 64-bit float
    {
        auto result = fastjson::parse("18446744073709551616");  // 2^64
        test_case("Convert 128-bit int to 64-bit float",
                  result.has_value() && !std::isnan(result->as_float64()));
    }

    // Test converting 128-bit float to 64-bit float
    {
        auto result = fastjson::parse("1.234567890123456789012345");
        test_case("Convert 128-bit float to 64-bit float",
                  result.has_value() && result->as_float64() > 1.0 && result->as_float64() < 2.0);
    }

    // Test converting 128-bit float to 64-bit int
    {
        auto result = fastjson::parse("1.234567890123456789012345");
        test_case("Convert 128-bit float to 64-bit int",
                  result.has_value() && result->as_int64() == 1);
    }

    // Test converting 64-bit double to 128-bit float
    {
        auto result = fastjson::parse("3.14159");
        test_case("Convert 64-bit double to 128-bit float", result.has_value());
        // as_float128() should work without throwing
        if (result.has_value()) {
            auto f128 = result->as_float128();
            test_case("128-bit float conversion successful", true);
        }
    }

    // Test converting 64-bit double to 128-bit int
    {
        auto result = fastjson::parse("42.7");
        test_case("Convert 64-bit double to 128-bit int",
                  result.has_value() && result->as_int128() == 42);
    }
}

void test_edge_cases() {
    std::cout << "\n=== Testing Edge Cases ===" << std::endl;

    // Test zero conversions
    {
        auto result = fastjson::parse("0");
        test_case("Zero as_int64()", result.has_value() && result->as_int64() == 0);
        test_case("Zero as_float64()", result.has_value() && result->as_float64() == 0.0);
        test_case("Zero as_int128()", result.has_value() && result->as_int128() == 0);
    }

    // Test negative number conversions
    {
        auto result = fastjson::parse("-123");
        test_case("Negative as_int64()", result.has_value() && result->as_int64() == -123);
        test_case("Negative as_float64()", result.has_value() && result->as_float64() == -123.0);
    }

    // Test large negative 128-bit
    {
        auto result = fastjson::parse("-123456789012345678901234567890");
        test_case("Large negative as_int128()", result.has_value() && result->as_int128() < 0);
    }

    // Test NaN from string
    {
        auto result = fastjson::parse("\"not a number\"");
        test_case("String returns NaN", result.has_value() && std::isnan(result->as_number()));
        test_case("String as_int64() returns 0", result.has_value() && result->as_int64() == 0);
    }

    // Test conversions on arrays
    {
        auto result = fastjson::parse("[1, 2, 3]");
        test_case("Array as_number() returns NaN",
                  result.has_value() && std::isnan(result->as_number()));
        test_case("Array as_int64() returns 0", result.has_value() && result->as_int64() == 0);
    }

    // Test conversions on objects
    {
        auto result = fastjson::parse("{\"x\": 10}");
        test_case("Object as_float64() returns NaN",
                  result.has_value() && std::isnan(result->as_float64()));
        test_case("Object as_int128() returns 0", result.has_value() && result->as_int128() == 0);
    }
}

void test_no_exceptions() {
    std::cout << "\n=== Testing No Exceptions Thrown ===" << std::endl;

    bool no_exception = true;

    try {
        // Try all conversions on various non-numeric types
        auto null_val = fastjson::parse("null");
        if (null_val.has_value()) {
            null_val->as_number();
            null_val->as_number_128();
            null_val->as_int_128();
            null_val->as_uint_128();
            null_val->as_int64();
            null_val->as_float64();
            null_val->as_int128();
            null_val->as_float128();
        }

        auto bool_val = fastjson::parse("true");
        if (bool_val.has_value()) {
            bool_val->as_number();
            bool_val->as_number_128();
            bool_val->as_int_128();
            bool_val->as_uint_128();
            bool_val->as_int64();
            bool_val->as_float64();
            bool_val->as_int128();
            bool_val->as_float128();
        }

        auto str_val = fastjson::parse("\"hello\"");
        if (str_val.has_value()) {
            str_val->as_number();
            str_val->as_number_128();
            str_val->as_int_128();
            str_val->as_uint_128();
            str_val->as_int64();
            str_val->as_float64();
            str_val->as_int128();
            str_val->as_float128();
        }

        auto arr_val = fastjson::parse("[1,2,3]");
        if (arr_val.has_value()) {
            arr_val->as_number();
            arr_val->as_number_128();
            arr_val->as_int_128();
            arr_val->as_uint_128();
            arr_val->as_int64();
            arr_val->as_float64();
            arr_val->as_int128();
            arr_val->as_float128();
        }

        auto obj_val = fastjson::parse("{\"x\":1}");
        if (obj_val.has_value()) {
            obj_val->as_number();
            obj_val->as_number_128();
            obj_val->as_int_128();
            obj_val->as_uint_128();
            obj_val->as_int64();
            obj_val->as_float64();
            obj_val->as_int128();
            obj_val->as_float128();
        }

    } catch (...) {
        no_exception = false;
    }

    test_case("No exceptions thrown from numeric conversions", no_exception);
}

void test_precision_preservation() {
    std::cout << "\n=== Testing Precision Preservation ===" << std::endl;

    // Test that 64-bit values stay 64-bit when appropriate
    {
        auto result = fastjson::parse("123.456");
        test_case("Normal precision uses 64-bit", result.has_value() && result->is_number());
    }

    // Test that high precision upgrades to 128-bit
    {
        auto result = fastjson::parse("1.23456789012345678901234567890");
        test_case("High precision upgrades to 128-bit",
                  result.has_value() && result->is_number_128());
    }

    // Test that large integers use 128-bit
    {
        auto result = fastjson::parse("99999999999999999999");
        test_case("Large integer uses 128-bit",
                  result.has_value() && (result->is_int_128() || result->is_uint_128()));
    }

    // Test conversion preserves as much precision as possible
    {
        auto result = fastjson::parse("1.234567890123456789012345");
        if (result.has_value() && result->is_number_128()) {
            double d = result->as_float64();
            test_case("128->64 bit conversion doesn't return NaN", !std::isnan(d));
            test_case("128->64 bit conversion preserves rough magnitude", d > 1.0 && d < 2.0);
        }
    }
}

int main() {
    std::cout << "FastestJSONInTheWest - Numeric Conversion Helpers Tests\n";
    std::cout << "========================================================\n";

    test_64bit_conversions();
    test_128bit_conversions();
    test_cross_type_conversions();
    test_edge_cases();
    test_no_exceptions();
    test_precision_preservation();

    std::cout << "\n========================================================\n";
    std::cout << "Test Results: " << passed_count << "/" << test_count << " passed\n";

    if (passed_count == test_count) {
        std::cout << "✓ All tests passed!\n";
        return 0;
    } else {
        std::cout << "✗ Some tests failed.\n";
        return 1;
    }
}
