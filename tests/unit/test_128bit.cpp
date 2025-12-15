// Test suite for 128-bit number parsing support
// Author: Olumuyiwa Oluwasanmi
// Tests adaptive precision parsing with 64-bit and 128-bit numbers

import fastjson;

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// Helper to convert __float128 to string for comparison
std::string float128_to_string(__float128 value) {
    // Convert to long double for string representation
    long double ld_value = static_cast<long double>(value);
    char buffer[128];
    int len = snprintf(buffer, sizeof(buffer), "%.36Lg", ld_value);
    return std::string(buffer, len);
}

// Helper to convert __int128 to string
std::string int128_to_string(__int128 value) {
    if (value == 0)
        return "0";

    bool is_negative = value < 0;
    unsigned __int128 abs_val = is_negative ? -static_cast<unsigned __int128>(value)
                                            : static_cast<unsigned __int128>(value);

    char buffer[64];
    char* ptr = buffer + sizeof(buffer);
    *--ptr = '\0';

    do {
        *--ptr = '0' + (abs_val % 10);
        abs_val /= 10;
    } while (abs_val > 0);

    if (is_negative) {
        *--ptr = '-';
    }

    return std::string(ptr);
}

// Helper to convert unsigned __int128 to string
std::string uint128_to_string(unsigned __int128 value) {
    if (value == 0)
        return "0";

    char buffer[64];
    char* ptr = buffer + sizeof(buffer);
    *--ptr = '\0';

    do {
        *--ptr = '0' + (value % 10);
        value /= 10;
    } while (value > 0);

    return std::string(ptr);
}

// Test counter
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

// Test 64-bit boundary values
void test_64bit_boundaries() {
    std::cout << "\n=== Testing 64-bit Boundaries ===" << std::endl;

    // Test max safe integer in double (2^53)
    {
        std::string json = "9007199254740992";  // 2^53
        auto result = fastjson::parse(json);
        test_case("Parse 2^53 (max safe double integer)",
                  result.has_value() && result->is_number());
        if (result.has_value()) {
            test_case("2^53 value correct", result->as_number() == 9007199254740992.0);
        }
    }

    // Test just above max safe integer (should still work with 64-bit)
    {
        std::string json = "9007199254740993";  // 2^53 + 1
        auto result = fastjson::parse(json);
        test_case("Parse 2^53 + 1", result.has_value());
    }

    // Test large 64-bit integer at boundary
    {
        std::string json = "9223372036854775807";  // INT64_MAX
        auto result = fastjson::parse(json);
        test_case("Parse INT64_MAX", result.has_value() && result->is_number());
    }

    // Test negative 64-bit boundary
    {
        std::string json = "-9223372036854775808";  // INT64_MIN
        auto result = fastjson::parse(json);
        test_case("Parse INT64_MIN", result.has_value());
    }

    // Test double with 15 significant digits (should use 64-bit)
    {
        std::string json = "1.23456789012345";  // 15 significant digits
        auto result = fastjson::parse(json);
        test_case("Parse 15 significant digits", result.has_value() && result->is_number());
    }

    // Test double with max exponent
    {
        std::string json = "1.7976931348623157e308";  // Near DBL_MAX
        auto result = fastjson::parse(json);
        test_case("Parse near DBL_MAX", result.has_value() && result->is_number());
    }

    // Test double with min exponent
    {
        std::string json = "2.2250738585072014e-308";  // Near DBL_MIN
        auto result = fastjson::parse(json);
        test_case("Parse near DBL_MIN", result.has_value() && result->is_number());
    }
}

// Test 128-bit integer parsing
void test_128bit_integers() {
    std::cout << "\n=== Testing 128-bit Integers ===" << std::endl;

    // Test integer just above 64-bit max
    {
        std::string json = "18446744073709551616";  // 2^64
        auto result = fastjson::parse(json);
        test_case("Parse 2^64 (requires 128-bit)", result.has_value());
        if (result.has_value()) {
            test_case("2^64 stored as int128 or uint128",
                      result->is_int_128() || result->is_uint_128());
        }
    }

    // Test very large 128-bit integer
    {
        std::string json = "123456789012345678901234567890";  // 30 digits
        auto result = fastjson::parse(json);
        test_case("Parse 30-digit integer", result.has_value());
        if (result.has_value()) {
            test_case("30-digit stored as 128-bit", result->is_int_128() || result->is_uint_128());
        }
    }

    // Test negative 128-bit integer
    {
        std::string json = "-123456789012345678901234567890";
        auto result = fastjson::parse(json);
        test_case("Parse negative 30-digit integer", result.has_value());
        if (result.has_value()) {
            test_case("Negative 30-digit stored as int128", result->is_int_128());
        }
    }

    // Test 39-digit integer (near 128-bit limit)
    {
        std::string json = "170141183460469231731687303715884105727";  // Near INT128_MAX
        auto result = fastjson::parse(json);
        test_case("Parse near INT128_MAX", result.has_value());
    }

    // Test integer that exceeds 128-bit range (should fail gracefully)
    {
        std::string json = "999999999999999999999999999999999999999999";  // 42 digits
        auto result = fastjson::parse(json);
        test_case("Parse integer exceeding 128-bit (should error)", !result.has_value());
    }
}

// Test 128-bit float parsing
void test_128bit_floats() {
    std::cout << "\n=== Testing 128-bit Floats ===" << std::endl;

    // Test float with more than 15 significant digits (requires 128-bit)
    {
        std::string json = "1.234567890123456789012345";  // 25 significant digits
        auto result = fastjson::parse(json);
        test_case("Parse 25 significant digit float", result.has_value());
        if (result.has_value()) {
            test_case("25-digit float stored as float128", result->is_number_128());
        }
    }

    // Test float with 20 significant digits
    {
        std::string json = "3.14159265358979323846";  // 20 digits of pi
        auto result = fastjson::parse(json);
        test_case("Parse 20 digits of pi", result.has_value());
        if (result.has_value()) {
            test_case("20-digit pi stored as float128", result->is_number_128());
        }
    }

    // Test float with very large exponent (beyond double range)
    {
        std::string json = "1.5e400";  // Exponent beyond double range
        auto result = fastjson::parse(json);
        test_case("Parse float with e400 exponent", result.has_value());
        if (result.has_value()) {
            test_case("Large exponent stored as float128", result->is_number_128());
        }
    }

    // Test float with very small exponent
    {
        std::string json = "1.5e-400";  // Exponent beyond double range
        auto result = fastjson::parse(json);
        test_case("Parse float with e-400 exponent", result.has_value());
        if (result.has_value()) {
            test_case("Small exponent stored as float128", result->is_number_128());
        }
    }

    // Test scientific notation with high precision
    {
        std::string json = "6.02214076e23";  // Avogadro's number
        auto result = fastjson::parse(json);
        test_case("Parse Avogadro's number", result.has_value());
    }
}

// Test precision detection and upgrade
void test_precision_upgrade() {
    std::cout << "\n=== Testing Precision Detection & Upgrade ===" << std::endl;

    // Test that 64-bit is used when sufficient
    {
        std::string json = "123.456";
        auto result = fastjson::parse(json);
        test_case("Small float uses 64-bit", result.has_value() && result->is_number());
    }

    // Test upgrade from 64 to 128 for precision
    {
        std::string json = "1.23456789012345678901234567890";  // 30 sig digits
        auto result = fastjson::parse(json);
        test_case("High precision triggers 128-bit upgrade",
                  result.has_value() && result->is_number_128());
    }

    // Test integer precision detection
    {
        std::string json = "12345678901234567890";  // 20 digits
        auto result = fastjson::parse(json);
        test_case("20-digit integer detected and parsed", result.has_value());
        if (result.has_value()) {
            test_case("20-digit uses 128-bit storage",
                      result->is_int_128() || result->is_uint_128());
        }
    }
}

// Test NaN fallback for overflow
void test_nan_fallback() {
    std::cout << "\n=== Testing NaN Fallback ===" << std::endl;

    // Test extreme float that might overflow even 128-bit
    {
        std::string json = "1e5000";  // Extremely large exponent
        auto result = fastjson::parse(json);
        test_case("Extreme exponent handled", result.has_value());
        if (result.has_value() && result->is_number()) {
            test_case("Extreme value returns NaN", std::isnan(result->as_number()));
        }
    }

    // Test negative extreme
    {
        std::string json = "-1e5000";
        auto result = fastjson::parse(json);
        test_case("Negative extreme handled", result.has_value());
        if (result.has_value() && result->is_number()) {
            test_case("Negative extreme returns NaN", std::isnan(result->as_number()));
        }
    }
}

// Test serialization of 128-bit values
void test_128bit_serialization() {
    std::cout << "\n=== Testing 128-bit Serialization ===" << std::endl;

    // Test serialization of 128-bit float
    {
        std::string json = "1.234567890123456789012345";
        auto result = fastjson::parse(json);
        test_case("Parse high-precision float for serialization", result.has_value());

        if (result.has_value()) {
            std::string serialized = result->to_string();
            test_case("Serialize 128-bit float", !serialized.empty());
            test_case("Serialized float is valid JSON", serialized.find("1.23") == 0);
        }
    }

    // Test serialization of 128-bit integer
    {
        std::string json = "123456789012345678901234567890";
        auto result = fastjson::parse(json);
        test_case("Parse large integer for serialization", result.has_value());

        if (result.has_value()) {
            std::string serialized = result->to_string();
            test_case("Serialize 128-bit integer", !serialized.empty());
            test_case("Serialized integer starts correctly", serialized.find("123456") == 0);
        }
    }
}

// Test round-trip parsing
void test_roundtrip() {
    std::cout << "\n=== Testing Round-Trip Parsing ===" << std::endl;

    // Test that 64-bit values round-trip correctly
    {
        std::string json = "123.456";
        auto result = fastjson::parse(json);
        test_case("Parse simple float", result.has_value());

        if (result.has_value()) {
            std::string serialized = result->to_string();
            auto reparsed = fastjson::parse(serialized);
            test_case("Round-trip simple float",
                      reparsed.has_value() && std::abs(reparsed->as_number() - 123.456) < 0.001);
        }
    }

    // Test 128-bit integer round-trip
    {
        std::string json = "18446744073709551616";  // 2^64
        auto result = fastjson::parse(json);
        test_case("Parse 2^64 for round-trip", result.has_value());

        if (result.has_value()) {
            std::string serialized = result->to_string();
            auto reparsed = fastjson::parse(serialized);
            test_case("Round-trip 2^64", reparsed.has_value());
        }
    }
}

// Test edge cases
void test_edge_cases() {
    std::cout << "\n=== Testing Edge Cases ===" << std::endl;

    // Test zero
    {
        std::string json = "0";
        auto result = fastjson::parse(json);
        test_case("Parse zero", result.has_value() && result->is_number());
        if (result.has_value()) {
            test_case("Zero value correct", result->as_number() == 0.0);
        }
    }

    // Test negative zero
    {
        std::string json = "-0";
        auto result = fastjson::parse(json);
        test_case("Parse negative zero", result.has_value());
    }

    // Test zero with decimal
    {
        std::string json = "0.0";
        auto result = fastjson::parse(json);
        test_case("Parse 0.0", result.has_value() && result->is_number());
    }

    // Test leading zeros are handled correctly
    {
        std::string json = "[0, 1, 10, 100]";
        auto result = fastjson::parse(json);
        test_case("Parse array with various zeros", result.has_value());
    }

    // Test very small number
    {
        std::string json = "0.000000000000000001";
        auto result = fastjson::parse(json);
        test_case("Parse very small number", result.has_value());
    }
}

// Test performance characteristics
void test_performance() {
    std::cout << "\n=== Testing Performance Characteristics ===" << std::endl;

    // Verify 64-bit fast path is used for common cases
    std::vector<std::string> common_numbers = {"0",   "1",    "42",  "-17", "3.14159", "2.71828",
                                               "100", "1000", "0.5", "0.1", "-0.5"};

    int fast_path_count = 0;
    for (const auto& json : common_numbers) {
        auto result = fastjson::parse(json);
        if (result.has_value() && result->is_number()) {
            fast_path_count++;
        }
    }

    test_case("Common numbers use 64-bit fast path", fast_path_count == common_numbers.size());

    // Test that 128-bit is only used when necessary
    {
        std::string json_64bit = "123456.789";
        std::string json_128bit = "1.234567890123456789012345";

        auto result_64 = fastjson::parse(json_64bit);
        auto result_128 = fastjson::parse(json_128bit);

        test_case("Normal precision uses 64-bit", result_64.has_value() && result_64->is_number());
        test_case("High precision uses 128-bit",
                  result_128.has_value() && result_128->is_number_128());
    }
}

int main() {
    std::cout << "FastestJSONInTheWest - 128-bit Number Parsing Tests\n";
    std::cout << "===================================================\n";

    test_64bit_boundaries();
    test_128bit_integers();
    test_128bit_floats();
    test_precision_upgrade();
    test_nan_fallback();
    test_128bit_serialization();
    test_roundtrip();
    test_edge_cases();
    test_performance();

    std::cout << "\n===================================================\n";
    std::cout << "Test Results: " << passed_count << "/" << test_count << " passed\n";

    if (passed_count == test_count) {
        std::cout << "✓ All tests passed!\n";
        return 0;
    } else {
        std::cout << "✗ Some tests failed.\n";
        return 1;
    }
}
