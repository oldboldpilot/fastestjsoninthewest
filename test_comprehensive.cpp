#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
import fastjson;

// Test counter
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name, condition) \
    do { \
        if (condition) { \
            std::cout << "✓ Test " << (tests_passed + tests_failed + 1) << ": " << name << " PASSED\n"; \
            tests_passed++; \
        } else { \
            std::cout << "✗ Test " << (tests_passed + tests_failed + 1) << ": " << name << " FAILED\n"; \
            tests_failed++; \
        } \
    } while(0)

int main() {
    std::cout << "=== FastJSON Comprehensive Test Suite ===\n\n";

    // ========== Basic Literals (5 tests) ==========
    std::cout << "--- Basic Literals ---\n";
    
    auto r1 = fastjson::parse("null");
    TEST("Parse null", r1.has_value() && r1->is_null());
    
    auto r2 = fastjson::parse("true");
    TEST("Parse true", r2.has_value() && r2->is_boolean() && r2->as_boolean() == true);
    
    auto r3 = fastjson::parse("false");
    TEST("Parse false", r3.has_value() && r3->is_boolean() && r3->as_boolean() == false);
    
    auto r4 = fastjson::parse("42");
    TEST("Parse integer", r4.has_value() && r4->is_number() && r4->as_number() == 42.0);
    
    auto r5 = fastjson::parse("3.14159");
    TEST("Parse float", r5.has_value() && r5->is_number() && std::abs(r5->as_number() - 3.14159) < 0.00001);

    // ========== Strings (10 tests) ==========
    std::cout << "\n--- Strings ---\n";
    
    auto s1 = fastjson::parse(R"("")");
    TEST("Empty string", s1.has_value() && s1->is_string() && s1->as_string() == "");
    
    auto s2 = fastjson::parse(R"("hello")");
    TEST("Simple string", s2.has_value() && s2->is_string() && s2->as_string() == "hello");
    
    auto s3 = fastjson::parse(R"("hello world")");
    TEST("String with space", s3.has_value() && s3->is_string() && s3->as_string() == "hello world");
    
    auto s4 = fastjson::parse(R"("Hello\nWorld")");
    TEST("String with newline escape", s4.has_value() && s4->is_string());
    
    auto s5 = fastjson::parse(R"("Hello\tWorld")");
    TEST("String with tab escape", s5.has_value() && s5->is_string());
    
    auto s6 = fastjson::parse(R"("\"quoted\"")");
    TEST("String with quote escape", s6.has_value() && s6->is_string());
    
    auto s7 = fastjson::parse(R"("C:\\path\\to\\file")");
    TEST("String with backslash", s7.has_value() && s7->is_string());
    
    auto s8 = fastjson::parse(R"("🚀")");
    TEST("String with emoji", s8.has_value() && s8->is_string());
    
    auto s9 = fastjson::parse(R"("αβγδε")");
    TEST("String with Greek letters", s9.has_value() && s9->is_string());
    
    auto s10 = fastjson::parse(R"("日本語")");
    TEST("String with Japanese", s10.has_value() && s10->is_string());

    // ========== Numbers (10 tests) ==========
    std::cout << "\n--- Numbers ---\n";
    
    auto n1 = fastjson::parse("0");
    TEST("Zero", n1.has_value() && n1->as_number() == 0.0);
    
    auto n2 = fastjson::parse("-42");
    TEST("Negative integer", n2.has_value() && n2->as_number() == -42.0);
    
    auto n3 = fastjson::parse("1234567890");
    TEST("Large integer", n3.has_value() && n3->as_number() == 1234567890.0);
    
    auto n4 = fastjson::parse("0.5");
    TEST("Decimal fraction", n4.has_value() && n4->as_number() == 0.5);
    
    auto n5 = fastjson::parse("-0.5");
    TEST("Negative decimal", n5.has_value() && n5->as_number() == -0.5);
    
    auto n6 = fastjson::parse("1e10");
    TEST("Scientific notation positive exp", n6.has_value() && n6->as_number() == 1e10);
    
    auto n7 = fastjson::parse("1e-10");
    TEST("Scientific notation negative exp", n7.has_value() && n7->as_number() == 1e-10);
    
    auto n8 = fastjson::parse("1.5e2");
    TEST("Decimal with exponent", n8.has_value() && n8->as_number() == 150.0);
    
    auto n9 = fastjson::parse("-1.5e-2");
    TEST("Negative decimal with negative exp", n9.has_value() && std::abs(n9->as_number() + 0.015) < 0.0001);
    
    auto n10 = fastjson::parse("123.456");
    TEST("Multi-digit decimal", n10.has_value() && std::abs(n10->as_number() - 123.456) < 0.001);

    // ========== Arrays (10 tests) ==========
    std::cout << "\n--- Arrays ---\n";
    
    auto a1 = fastjson::parse("[]");
    TEST("Empty array", a1.has_value() && a1->is_array() && a1->as_array().size() == 0);
    
    auto a2 = fastjson::parse("[1]");
    TEST("Single element array", a2.has_value() && a2->is_array() && a2->as_array().size() == 1);
    
    auto a3 = fastjson::parse("[1,2,3]");
    TEST("Integer array", a3.has_value() && a3->is_array() && a3->as_array().size() == 3);
    
    auto a4 = fastjson::parse(R"(["a","b","c"])");
    TEST("String array", a4.has_value() && a4->is_array() && a4->as_array().size() == 3);
    
    auto a5 = fastjson::parse("[true,false,null]");
    TEST("Mixed literal array", a5.has_value() && a5->is_array() && a5->as_array().size() == 3);
    
    auto a6 = fastjson::parse("[[1,2],[3,4]]");
    TEST("Nested arrays", a6.has_value() && a6->is_array() && a6->as_array().size() == 2);
    
    auto a7 = fastjson::parse("[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]");
    TEST("Long array", a7.has_value() && a7->is_array() && a7->as_array().size() == 10);
    
    auto a8 = fastjson::parse(R"([{"a":1},{"b":2}])");
    TEST("Array of objects", a8.has_value() && a8->is_array() && a8->as_array().size() == 2);
    
    auto a9 = fastjson::parse("[1,2,3,4,5]");
    TEST("Array element access", a9.has_value() && a9->as_array()[2].as_number() == 3.0);
    
    auto a10 = fastjson::parse("[ 1 , 2 , 3 ]");
    TEST("Array with whitespace", a10.has_value() && a10->is_array() && a10->as_array().size() == 3);

    // ========== Objects (10 tests) ==========
    std::cout << "\n--- Objects ---\n";
    
    auto o1 = fastjson::parse("{}");
    TEST("Empty object", o1.has_value() && o1->is_object() && o1->as_object().size() == 0);
    
    auto o2 = fastjson::parse(R"({"name":"test"})");
    TEST("Single property object", o2.has_value() && o2->is_object() && o2->as_object().size() == 1);
    
    auto o3 = fastjson::parse(R"({"a":1,"b":2})");
    TEST("Multiple properties", o3.has_value() && o3->is_object() && o3->as_object().size() == 2);
    
    auto o4 = fastjson::parse(R"({"name":"Alice","age":30})");
    TEST("Mixed type properties", o4.has_value() && o4->is_object());
    
    auto o5 = fastjson::parse(R"({"nested":{"inner":true}})");
    TEST("Nested object", o5.has_value() && o5->is_object());
    
    auto o6 = fastjson::parse(R"({"array":[1,2,3]})");
    TEST("Object with array", o6.has_value() && o6->is_object());
    
    auto o7 = fastjson::parse(R"({"a":1,"b":2,"c":3,"d":4,"e":5})");
    TEST("Object with 5 properties", o7.has_value() && o7->as_object().size() == 5);
    
    auto o8 = fastjson::parse(R"({ "name" : "test" })");
    TEST("Object with whitespace", o8.has_value() && o8->is_object());
    
    auto o9 = fastjson::parse(R"({"":"empty key"})");
    TEST("Object with empty key", o9.has_value() && o9->is_object());
    
    auto o10 = fastjson::parse(R"({"key":"value","key":"overwritten"})");
    TEST("Duplicate keys (last wins)", o10.has_value() && o10->is_object());

    // ========== Complex Structures (5 tests) ==========
    std::cout << "\n--- Complex Structures ---\n";
    
    auto c1 = fastjson::parse(R"({"users":[{"name":"Alice","age":30},{"name":"Bob","age":25}]})");
    TEST("Array of objects in object", c1.has_value() && c1->is_object());
    
    auto c2 = fastjson::parse(R"([[[1,2],[3,4]],[[5,6],[7,8]]])");
    TEST("Triple nested array", c2.has_value() && c2->is_array());
    
    auto c3 = fastjson::parse(R"({"a":{"b":{"c":{"d":"deep"}}}})");
    TEST("Deep nested object", c3.has_value() && c3->is_object());
    
    auto c4 = fastjson::parse(R"({"numbers":[1,2,3],"strings":["a","b"],"bools":[true,false]})");
    TEST("Object with multiple arrays", c4.has_value() && c4->is_object());
    
    auto c5 = fastjson::parse(R"({"meta":{"version":"1.0","author":"test"},"data":[1,2,3]})");
    TEST("Realistic JSON structure", c5.has_value() && c5->is_object());

    // ========== Whitespace Handling (5 tests) ==========
    std::cout << "\n--- Whitespace Handling ---\n";
    
    auto w1 = fastjson::parse("  \n\t  true  \n\t  ");
    TEST("Whitespace around literal", w1.has_value() && w1->is_boolean());
    
    auto w2 = fastjson::parse("{\n  \"key\": \"value\"\n}");
    TEST("Formatted object", w2.has_value() && w2->is_object());
    
    auto w3 = fastjson::parse("[\n  1,\n  2,\n  3\n]");
    TEST("Formatted array", w3.has_value() && w3->is_array());
    
    auto w4 = fastjson::parse("{\"a\":1,\"b\":2}");
    TEST("No whitespace compact", w4.has_value() && w4->is_object());
    
    auto w5 = fastjson::parse("   [   ]   ");
    TEST("Whitespace empty array", w5.has_value() && w5->is_array());

    // ========== Edge Cases (5 tests) ==========
    std::cout << "\n--- Edge Cases ---\n";
    
    auto e1 = fastjson::parse(R"({"":""})");
    TEST("Empty string key and value", e1.has_value() && e1->is_object());
    
    auto e2 = fastjson::parse("[[[[[1]]]]]");
    TEST("Deeply nested array", e2.has_value() && e2->is_array());
    
    auto e3 = fastjson::parse("0.0");
    TEST("Zero as float", e3.has_value() && e3->as_number() == 0.0);
    
    auto e4 = fastjson::parse("-0");
    TEST("Negative zero", e4.has_value() && e4->as_number() == 0.0);
    
    auto e5 = fastjson::parse(R"("\u0041")");
    TEST("Unicode escape sequence", e5.has_value() && e5->is_string());

    // ========== Error Cases (5 tests) ==========
    std::cout << "\n--- Error Cases ---\n";
    
    auto err1 = fastjson::parse("");
    TEST("Empty input fails", !err1.has_value());
    
    auto err2 = fastjson::parse("{");
    TEST("Unterminated object fails", !err2.has_value());
    
    auto err3 = fastjson::parse("[1,2,");
    TEST("Unterminated array fails", !err3.has_value());
    
    auto err4 = fastjson::parse(R"({"key"})");
    TEST("Missing value fails", !err4.has_value());
    
    auto err5 = fastjson::parse("undefined");
    TEST("Invalid literal fails", !err5.has_value());

    // ========== Summary ==========
    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Total tests: " << (tests_passed + tests_failed) << "\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";
    std::cout << "Success rate: " << (100.0 * tests_passed / (tests_passed + tests_failed)) << "%\n";

    return (tests_failed == 0) ? 0 : 1;
}
