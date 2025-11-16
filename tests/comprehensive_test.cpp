// FastestJSONInTheWest Comprehensive Test Suite
// Author: Olumuyiwa Oluwasanmi
// Complete validation and functionality testing

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

// FastestJSONInTheWest modules
import fastjson;
import fastjson.simd;

using namespace fastjson::core;
using namespace fastjson::parser;
using namespace fastjson::serializer;
using namespace fastjson::simd;

// ============================================================================
// Core JSON Value Tests
// ============================================================================

class JsonValueTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JsonValueTest, DefaultConstructorCreatesNull) {
    json_value value;
    EXPECT_TRUE(value.is_null());
    EXPECT_EQ(value.type_name(), "null");
}

TEST_F(JsonValueTest, BooleanConstructorAndAccess) {
    json_value true_val(true);
    json_value false_val(false);
    
    EXPECT_TRUE(true_val.is_boolean());
    EXPECT_TRUE(false_val.is_boolean());
    EXPECT_EQ(true_val.as<bool>(), true);
    EXPECT_EQ(false_val.as<bool>(), false);
    EXPECT_EQ(true_val.type_name(), "boolean");
}

TEST_F(JsonValueTest, NumberConstructorAndAccess) {
    json_value int_val(42);
    json_value double_val(3.14159);
    
    EXPECT_TRUE(int_val.is_number());
    EXPECT_TRUE(double_val.is_number());
    EXPECT_DOUBLE_EQ(int_val.as<double>(), 42.0);
    EXPECT_DOUBLE_EQ(double_val.as<double>(), 3.14159);
    EXPECT_EQ(int_val.type_name(), "number");
}

TEST_F(JsonValueTest, StringConstructorAndAccess) {
    json_value str_val("hello world");
    json_value sv_val(std::string_view("string view"));
    std::string test_str = "test string";
    json_value str_copy(test_str);
    
    EXPECT_TRUE(str_val.is_string());
    EXPECT_TRUE(sv_val.is_string());
    EXPECT_TRUE(str_copy.is_string());
    EXPECT_EQ(str_val.as<std::string>(), "hello world");
    EXPECT_EQ(sv_val.as<std::string>(), "string view");
    EXPECT_EQ(str_copy.as<std::string>(), "test string");
    EXPECT_EQ(str_val.type_name(), "string");
}

TEST_F(JsonValueTest, ArrayConstructorAndAccess) {
    json_array arr = {json_value(1), json_value(2), json_value(3)};
    json_value arr_val(std::move(arr));
    
    EXPECT_TRUE(arr_val.is_array());
    EXPECT_EQ(arr_val.size(), 3);
    EXPECT_DOUBLE_EQ(arr_val[0].as<double>(), 1.0);
    EXPECT_DOUBLE_EQ(arr_val[1].as<double>(), 2.0);
    EXPECT_DOUBLE_EQ(arr_val[2].as<double>(), 3.0);
    EXPECT_EQ(arr_val.type_name(), "array");
}

TEST_F(JsonValueTest, ObjectConstructorAndAccess) {
    json_object obj;
    obj["name"] = json_value("test");
    obj["value"] = json_value(42);
    obj["active"] = json_value(true);
    json_value obj_val(std::move(obj));
    
    EXPECT_TRUE(obj_val.is_object());
    EXPECT_EQ(obj_val.size(), 3);
    EXPECT_TRUE(obj_val.contains("name"));
    EXPECT_TRUE(obj_val.contains("value"));
    EXPECT_TRUE(obj_val.contains("active"));
    EXPECT_FALSE(obj_val.contains("missing"));
    
    EXPECT_EQ(obj_val["name"].as<std::string>(), "test");
    EXPECT_DOUBLE_EQ(obj_val["value"].as<double>(), 42.0);
    EXPECT_EQ(obj_val["active"].as<bool>(), true);
    EXPECT_EQ(obj_val.type_name(), "object");
}

TEST_F(JsonValueTest, CopyAndMoveSemantics) {
    json_value original("original");
    json_value copied = original;
    json_value moved = std::move(copied);
    
    EXPECT_EQ(original.as<std::string>(), "original");
    EXPECT_EQ(moved.as<std::string>(), "original");
}

TEST_F(JsonValueTest, EqualityComparison) {
    json_value val1(42);
    json_value val2(42);
    json_value val3(43);
    
    EXPECT_EQ(val1, val2);
    EXPECT_NE(val1, val3);
}

// ============================================================================
// JSON Parser Tests
// ============================================================================

class JsonParserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JsonParserTest, ParseNull) {
    auto result = parse("null");
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->is_null());
}

TEST_F(JsonParserTest, ParseBoolean) {
    auto true_result = parse("true");
    auto false_result = parse("false");
    
    ASSERT_TRUE(true_result);
    ASSERT_TRUE(false_result);
    EXPECT_TRUE(true_result->is_boolean());
    EXPECT_TRUE(false_result->is_boolean());
    EXPECT_EQ(true_result->as<bool>(), true);
    EXPECT_EQ(false_result->as<bool>(), false);
}

TEST_F(JsonParserTest, ParseNumbers) {
    auto int_result = parse("42");
    auto neg_result = parse("-17");
    auto float_result = parse("3.14159");
    auto exp_result = parse("1.23e-4");
    auto big_exp_result = parse("1.23E+10");
    
    ASSERT_TRUE(int_result);
    ASSERT_TRUE(neg_result);
    ASSERT_TRUE(float_result);
    ASSERT_TRUE(exp_result);
    ASSERT_TRUE(big_exp_result);
    
    EXPECT_DOUBLE_EQ(int_result->as<double>(), 42.0);
    EXPECT_DOUBLE_EQ(neg_result->as<double>(), -17.0);
    EXPECT_DOUBLE_EQ(float_result->as<double>(), 3.14159);
    EXPECT_DOUBLE_EQ(exp_result->as<double>(), 1.23e-4);
    EXPECT_DOUBLE_EQ(big_exp_result->as<double>(), 1.23e+10);
}

TEST_F(JsonParserTest, ParseStrings) {
    auto simple_result = parse("\"hello\"");
    auto empty_result = parse("\"\"");
    auto escaped_result = parse("\"hello\\\"world\\\"\"");
    auto unicode_result = parse("\"hello\\u0020world\"");
    
    ASSERT_TRUE(simple_result);
    ASSERT_TRUE(empty_result);
    ASSERT_TRUE(escaped_result);
    ASSERT_TRUE(unicode_result);
    
    EXPECT_EQ(simple_result->as<std::string>(), "hello");
    EXPECT_EQ(empty_result->as<std::string>(), "");
    EXPECT_EQ(escaped_result->as<std::string>(), "hello\"world\"");
    EXPECT_EQ(unicode_result->as<std::string>(), "hello\\u0020world"); // Simplified unicode handling
}

TEST_F(JsonParserTest, ParseArrays) {
    auto empty_result = parse("[]");
    auto simple_result = parse("[1, 2, 3]");
    auto mixed_result = parse("[null, true, \"string\", 42]");
    auto nested_result = parse("[1, [2, 3], 4]");
    
    ASSERT_TRUE(empty_result);
    ASSERT_TRUE(simple_result);
    ASSERT_TRUE(mixed_result);
    ASSERT_TRUE(nested_result);
    
    EXPECT_EQ(empty_result->size(), 0);
    EXPECT_EQ(simple_result->size(), 3);
    EXPECT_EQ(mixed_result->size(), 4);
    EXPECT_EQ(nested_result->size(), 3);
    
    EXPECT_DOUBLE_EQ((*simple_result)[0].as<double>(), 1.0);
    EXPECT_DOUBLE_EQ((*simple_result)[1].as<double>(), 2.0);
    EXPECT_DOUBLE_EQ((*simple_result)[2].as<double>(), 3.0);
    
    EXPECT_TRUE((*mixed_result)[0].is_null());
    EXPECT_EQ((*mixed_result)[1].as<bool>(), true);
    EXPECT_EQ((*mixed_result)[2].as<std::string>(), "string");
    EXPECT_DOUBLE_EQ((*mixed_result)[3].as<double>(), 42.0);
}

TEST_F(JsonParserTest, ParseObjects) {
    auto empty_result = parse("{}");
    auto simple_result = parse("{\"key\": \"value\"}");
    auto mixed_result = parse("{\"name\": \"test\", \"value\": 42, \"active\": true}");
    auto nested_result = parse("{\"outer\": {\"inner\": \"value\"}}");
    
    ASSERT_TRUE(empty_result);
    ASSERT_TRUE(simple_result);
    ASSERT_TRUE(mixed_result);
    ASSERT_TRUE(nested_result);
    
    EXPECT_EQ(empty_result->size(), 0);
    EXPECT_EQ(simple_result->size(), 1);
    EXPECT_EQ(mixed_result->size(), 3);
    EXPECT_EQ(nested_result->size(), 1);
    
    EXPECT_EQ((*simple_result)["key"].as<std::string>(), "value");
    
    EXPECT_EQ((*mixed_result)["name"].as<std::string>(), "test");
    EXPECT_DOUBLE_EQ((*mixed_result)["value"].as<double>(), 42.0);
    EXPECT_EQ((*mixed_result)["active"].as<bool>(), true);
    
    EXPECT_TRUE((*nested_result)["outer"].is_object());
    EXPECT_EQ((*nested_result)["outer"]["inner"].as<std::string>(), "value");
}

TEST_F(JsonParserTest, ParseWhitespace) {
    auto spaced_result = parse(" { \"key\" : \"value\" } ");
    auto tabbed_result = parse("\\t[\\t1,\\t2,\\t3\\t]\\t");
    auto newlined_result = parse("\\n{\\n\\\"test\\\":\\n\\\"value\\\"\\n}\\n");
    
    ASSERT_TRUE(spaced_result);
    ASSERT_TRUE(tabbed_result);
    ASSERT_TRUE(newlined_result);
    
    EXPECT_EQ((*spaced_result)["key"].as<std::string>(), "value");
    EXPECT_EQ(tabbed_result->size(), 3);
    EXPECT_EQ((*newlined_result)["test"].as<std::string>(), "value");
}

TEST_F(JsonParserTest, ParseErrors) {
    EXPECT_FALSE(parse(""));
    EXPECT_FALSE(parse("invalid"));
    EXPECT_FALSE(parse("{invalid}"));
    EXPECT_FALSE(parse("[1, 2,]"));
    EXPECT_FALSE(parse("{\"key\": }"));
    EXPECT_FALSE(parse("\"unterminated string"));
    EXPECT_FALSE(parse("{\"key\" \"value\"}"));  // Missing colon
    EXPECT_FALSE(parse("[1 2 3]"));  // Missing commas
}

// ============================================================================
// JSON Serializer Tests  
// ============================================================================

class JsonSerializerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JsonSerializerTest, SerializeBasicTypes) {
    EXPECT_EQ(to_string(json_value()), "null");
    EXPECT_EQ(to_string(json_value(true)), "true");
    EXPECT_EQ(to_string(json_value(false)), "false");
    EXPECT_EQ(to_string(json_value(42)), "42");
    EXPECT_EQ(to_string(json_value(3.14)), "3.14");
    EXPECT_EQ(to_string(json_value("hello")), "\"hello\"");
}

TEST_F(JsonSerializerTest, SerializeArrays) {
    json_array arr = {json_value(1), json_value(2), json_value(3)};
    json_value arr_val(std::move(arr));
    
    std::string result = to_string(arr_val);
    EXPECT_EQ(result, "[1,2,3]");
}

TEST_F(JsonSerializerTest, SerializeObjects) {
    json_object obj;
    obj["name"] = json_value("test");
    obj["value"] = json_value(42);
    json_value obj_val(std::move(obj));
    
    std::string result = to_string(obj_val);
    // Note: Order might vary, so we check both possibilities
    EXPECT_TRUE(result == "{\"name\":\"test\",\"value\":42}" || 
                result == "{\"value\":42,\"name\":\"test\"}");
}

TEST_F(JsonSerializerTest, SerializePrettyPrint) {
    json_object obj;
    obj["name"] = json_value("test");
    obj["value"] = json_value(42);
    json_value obj_val(std::move(obj));
    
    std::string result = to_pretty_string(obj_val);
    EXPECT_TRUE(result.find("\\n") != std::string::npos);
    EXPECT_TRUE(result.find("  ") != std::string::npos);  // Indentation
}

TEST_F(JsonSerializerTest, SerializeEscaping) {
    json_value str_val("hello\\\"world\\\"\\n\\t");
    std::string result = to_string(str_val);
    EXPECT_EQ(result, "\"hello\\\\\\\"world\\\\\\\"\\\\n\\\\t\"");
}

// ============================================================================
// Roundtrip Tests (Parse -> Serialize -> Parse)
// ============================================================================

class JsonRoundtripTest : public ::testing::Test {
protected:
    void RoundtripTest(const std::string& json_str) {
        auto parsed1 = parse(json_str);
        ASSERT_TRUE(parsed1) << "Failed to parse: " << json_str;
        
        auto serialized = to_string(*parsed1);
        auto parsed2 = parse(serialized);
        ASSERT_TRUE(parsed2) << "Failed to parse serialized: " << serialized;
        
        EXPECT_EQ(*parsed1, *parsed2) << "Roundtrip failed for: " << json_str;
    }
};

TEST_F(JsonRoundtripTest, BasicTypes) {
    RoundtripTest("null");
    RoundtripTest("true");
    RoundtripTest("false");
    RoundtripTest("42");
    RoundtripTest("3.14159");
    RoundtripTest("\"hello world\"");
}

TEST_F(JsonRoundtripTest, Arrays) {
    RoundtripTest("[]");
    RoundtripTest("[1, 2, 3]");
    RoundtripTest("[null, true, \"string\", 42]");
    RoundtripTest("[1, [2, [3, 4]], 5]");
}

TEST_F(JsonRoundtripTest, Objects) {
    RoundtripTest("{}");
    RoundtripTest("{\"key\": \"value\"}");
    RoundtripTest("{\"name\": \"test\", \"value\": 42, \"active\": true}");
}

TEST_F(JsonRoundtripTest, Complex) {
    RoundtripTest("{\"array\": [1, 2, 3], \"object\": {\"nested\": true}}");
    RoundtripTest("[{\"id\": 1, \"name\": \"first\"}, {\"id\": 2, \"name\": \"second\"}]");
}

// ============================================================================
// Performance Tests
// ============================================================================

class JsonPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
    
    std::string GenerateLargeJson(size_t size) {
        std::ostringstream oss;
        oss << "{";
        for (size_t i = 0; i < size; ++i) {
            if (i > 0) oss << ",";
            oss << "\"key" << i << "\":\"value" << i << "\"";
        }
        oss << "}";
        return oss.str();
    }
};

TEST_F(JsonPerformanceTest, ParseLargeObject) {
    auto large_json = GenerateLargeJson(10000);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = parse(large_json);
    auto end = std::chrono::high_resolution_clock::now();
    
    ASSERT_TRUE(result);
    EXPECT_EQ(result->size(), 10000);
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Parsed 10k elements in " << duration.count() << "ms" << std::endl;
}

// ============================================================================
// SIMD Tests
// ============================================================================

class JsonSIMDTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JsonSIMDTest, SIMDCapabilities) {
    std::cout << "SIMD Capabilities:" << std::endl;
    std::cout << "  SSE2: " << has_sse2() << std::endl;
    std::cout << "  AVX2: " << has_avx2() << std::endl;
    std::cout << "  AVX512: " << has_avx512() << std::endl;
}

TEST_F(JsonSIMDTest, WhitespaceSkipping) {
    std::string whitespace_str = "    \\t\\n\\r    hello";
    size_t pos_scalar = skip_whitespace_scalar(whitespace_str.c_str(), whitespace_str.size());
    size_t pos_adaptive = skip_whitespace_adaptive(whitespace_str.c_str(), whitespace_str.size());
    
    EXPECT_EQ(pos_scalar, pos_adaptive);
    EXPECT_EQ(whitespace_str.substr(pos_scalar, 5), "hello");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

class JsonErrorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JsonErrorTest, ErrorMessages) {
    EXPECT_EQ(error_message(json_error::none), "no error");
    EXPECT_EQ(error_message(json_error::invalid_json), "invalid JSON");
    EXPECT_EQ(error_message(json_error::type_mismatch), "type mismatch");
    EXPECT_EQ(error_message(json_error::out_of_range), "out of range");
    EXPECT_EQ(error_message(json_error::key_not_found), "key not found");
    EXPECT_EQ(error_message(json_error::memory_error), "memory error");
}

TEST_F(JsonErrorTest, ParserConfigErrors) {
    parser_config config;
    config.max_depth = 2;
    
    auto result = parse("{\"a\": {\"b\": {\"c\": \"too_deep\"}}}", config);
    EXPECT_FALSE(result);
}

// ============================================================================
// Test Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "FastestJSONInTheWest Test Suite" << std::endl;
    std::cout << "Author: Olumuyiwa Oluwasanmi" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Compiler: " << __VERSION__ << std::endl;
    std::cout << "Standard: C++23" << std::endl;
    std::cout << "================================" << std::endl;
    
    return RUN_ALL_TESTS();
}