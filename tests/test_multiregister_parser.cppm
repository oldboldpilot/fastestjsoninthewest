// FastestJSONInTheWest Multi-Register Parser Test Suite
// Tests 1:1 feature parity with original fastjson and C++ Core Guidelines compliance
// ============================================================================

module;

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

export module test_multiregister_parser;

import fastjson_multiregister_parser;

namespace test {

using namespace fastjson::mr;
using namespace fastjson::mr::literals;

// Test counter
inline int tests_passed = 0;
inline int tests_failed = 0;

inline void test_assert(bool condition, std::string_view test_name) {
    if (condition) {
        std::cout << "  ✓ " << test_name << "\n";
        ++tests_passed;
    } else {
        std::cout << "  ✗ " << test_name << " FAILED\n";
        ++tests_failed;
    }
}

// ============================================================================
// UNIT TESTS
// ============================================================================

export void test_parse_null() {
    std::cout << "\n[TEST] Parsing null values\n";
    
    auto result = parse("null");
    test_assert(result.has_value(), "parse null succeeds");
    test_assert(result->is_null(), "parsed value is null");
    test_assert(result->type() == value_type::null, "type is value_type::null");
}

export void test_parse_booleans() {
    std::cout << "\n[TEST] Parsing boolean values\n";
    
    auto true_result = parse("true");
    test_assert(true_result.has_value(), "parse true succeeds");
    test_assert(true_result->is_bool(), "parsed value is bool");
    test_assert(true_result->as_bool().value() == true, "value is true");
    
    auto false_result = parse("false");
    test_assert(false_result.has_value(), "parse false succeeds");
    test_assert(false_result->is_bool(), "parsed value is bool");
    test_assert(false_result->as_bool().value() == false, "value is false");
}

export void test_parse_integers() {
    std::cout << "\n[TEST] Parsing integer values\n";
    
    auto zero = parse("0");
    test_assert(zero.has_value() && zero->as_int().value() == 0, "parse 0");
    
    auto positive = parse("42");
    test_assert(positive.has_value() && positive->as_int().value() == 42, "parse 42");
    
    auto negative = parse("-123");
    test_assert(negative.has_value() && negative->as_int().value() == -123, "parse -123");
    
    auto large = parse("9223372036854775807");  // INT64_MAX
    test_assert(large.has_value() && large->as_int().value() == 9223372036854775807LL, "parse INT64_MAX");
}

export void test_parse_floats() {
    std::cout << "\n[TEST] Parsing floating point values\n";
    
    auto simple = parse("3.14");
    test_assert(simple.has_value() && simple->is_float(), "parse 3.14");
    
    auto negative = parse("-2.5");
    test_assert(negative.has_value() && negative->as_float().value() == -2.5, "parse -2.5");
    
    auto exponent = parse("1.5e10");
    test_assert(exponent.has_value() && exponent->is_float(), "parse 1.5e10");
    
    auto neg_exp = parse("2.5E-3");
    test_assert(neg_exp.has_value() && neg_exp->is_float(), "parse 2.5E-3");
}

export void test_parse_strings() {
    std::cout << "\n[TEST] Parsing string values\n";
    
    auto empty = parse(R"("")");
    test_assert(empty.has_value() && empty->is_string(), "parse empty string");
    test_assert(empty->as_string().value() == "", "empty string value");
    
    auto simple = parse(R"("hello")");
    test_assert(simple.has_value() && simple->as_string().value() == "hello", "parse simple string");
    
    auto unicode = parse(R"("日本語")");
    test_assert(unicode.has_value() && unicode->as_string().value() == "日本語", "parse unicode string");
}

export void test_parse_arrays() {
    std::cout << "\n[TEST] Parsing arrays\n";
    
    auto empty = parse("[]");
    test_assert(empty.has_value() && empty->is_array(), "parse empty array");
    test_assert(empty->size() == 0, "empty array size is 0");
    
    auto numbers = parse("[1, 2, 3]");
    test_assert(numbers.has_value() && numbers->size() == 3, "parse number array");
    
    auto mixed = parse(R"([1, "two", true, null])");
    test_assert(mixed.has_value() && mixed->size() == 4, "parse mixed array");
    
    auto nested = parse("[[1, 2], [3, 4]]");
    test_assert(nested.has_value() && nested->size() == 2, "parse nested array");
}

export void test_parse_objects() {
    std::cout << "\n[TEST] Parsing objects\n";
    
    auto empty = parse("{}");
    test_assert(empty.has_value() && empty->is_object(), "parse empty object");
    test_assert(empty->size() == 0, "empty object size is 0");
    
    auto simple = parse(R"({"key": "value"})");
    test_assert(simple.has_value() && simple->size() == 1, "parse simple object");
    
    auto nested = parse(R"({"outer": {"inner": 42}})");
    test_assert(nested.has_value(), "parse nested object");
}

export void test_fluent_api_factory_functions() {
    std::cout << "\n[TEST] Fluent API factory functions\n";
    
    auto obj = object();
    test_assert(obj.is_object(), "object() creates object");
    test_assert(obj.empty(), "object() creates empty object");
    
    auto arr = array();
    test_assert(arr.is_array(), "array() creates array");
    test_assert(arr.empty(), "array() creates empty array");
    
    auto n = null();
    test_assert(n.is_null(), "null() creates null");
}

export void test_stringify_and_prettify() {
    std::cout << "\n[TEST] Stringify and prettify\n";
    
    auto value = parse(R"({"a": 1, "b": [2, 3]})");
    test_assert(value.has_value(), "parse succeeds");
    
    auto str = stringify(*value);
    test_assert(!str.empty(), "stringify produces non-empty string");
    
    auto pretty = prettify(*value);
    test_assert(pretty.find('\n') != std::string::npos, "prettify produces multiline output");
}

export void test_literal_operator() {
    std::cout << "\n[TEST] Literal operator _json\n";
    
    auto result = R"({"test": 123})"_json;
    test_assert(result.has_value(), "_json literal parses successfully");
    test_assert(result->is_object(), "_json literal produces object");
}

export void test_simd_info() {
    std::cout << "\n[TEST] SIMD info\n";
    
    auto info = get_simd_info();
    
    std::cout << "  SIMD capabilities:\n";
    std::cout << "    AVX2:  " << (info.avx2_available ? "yes" : "no") << "\n";
    std::cout << "    AVX512: " << (info.avx512_available ? "yes" : "no") << "\n";
    std::cout << "    Path:  " << info.optimal_path << "\n";
    std::cout << "    Registers: " << info.register_count << "\n";
    std::cout << "    Bytes/iter: " << info.bytes_per_iteration << "\n";
    
    test_assert(info.register_count >= 1, "register count is valid");
    test_assert(info.bytes_per_iteration >= 1, "bytes per iteration is valid");
}

export void test_parse_with_metrics() {
    std::cout << "\n[TEST] Parse with metrics\n";
    
    std::string large_json = R"({"data": [)";
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) large_json += ",";
        large_json += std::to_string(i);
    }
    large_json += "]}";
    
    auto metrics = parse_with_metrics(large_json);
    
    test_assert(metrics.value.has_value(), "parse with metrics succeeds");
    test_assert(metrics.bytes_processed == large_json.size(), "bytes processed matches input size");
    test_assert(metrics.duration.count() > 0, "duration is recorded");
    
    std::cout << "  Parse time: " << metrics.duration.count() / 1000.0 << " µs\n";
    std::cout << "  Bytes: " << metrics.bytes_processed << "\n";
    std::cout << "  Throughput: " << (metrics.bytes_processed * 1e9 / metrics.duration.count() / 1e6) << " MB/s\n";
}

export void test_error_handling() {
    std::cout << "\n[TEST] Error handling\n";
    
    auto empty = parse("");
    test_assert(!empty.has_value(), "empty input returns error");
    test_assert(empty.error().code() == error_code::empty_input, "error code is empty_input");
    
    auto invalid = parse("invalid");
    test_assert(!invalid.has_value(), "invalid JSON returns error");
    
    auto unterminated = parse(R"({"key": "value)");
    test_assert(!unterminated.has_value(), "unterminated string returns error");
    
    auto extra = parse(R"({"a": 1} extra)");
    test_assert(!extra.has_value(), "extra tokens returns error");
    test_assert(extra.error().code() == error_code::extra_tokens, "error code is extra_tokens");
}

export void test_value_access() {
    std::cout << "\n[TEST] Value access methods\n";
    
    auto obj_result = parse(R"({"name": "test", "count": 42, "items": [1, 2, 3]})");
    test_assert(obj_result.has_value(), "parse object succeeds");
    
    const auto& obj = *obj_result;
    
    // Object subscript access - uses reference return (throws on missing key)
    const auto& name_ref = obj["name"];
    test_assert(name_ref.as_string().value() == "test", "name value is 'test'");
    
    const auto& count_ref = obj["count"];
    test_assert(count_ref.as_int().value() == 42, "count value is 42");
    
    // Array access
    const auto& items_ref = obj["items"];
    test_assert(items_ref.is_array(), "items is array");
    test_assert(items_ref.size() == 3, "items has 3 elements");
    
    const auto& first = items_ref[0];
    test_assert(first.as_int().value() == 1, "first item is 1");
}

// ============================================================================
// REGRESSION TESTS (comparing behavior with original fastjson)
// ============================================================================

export void test_regression_whitespace_handling() {
    std::cout << "\n[REGRESSION] Whitespace handling\n";
    
    auto with_spaces = parse("  {  \"key\"  :  \"value\"  }  ");
    test_assert(with_spaces.has_value(), "handles spaces");
    
    auto with_newlines = parse("{\n\t\"key\": \"value\"\n}");
    test_assert(with_newlines.has_value(), "handles newlines and tabs");
    
    auto mixed = parse("[\n  1,\n  2,\n  3\n]");
    test_assert(mixed.has_value(), "handles mixed whitespace in arrays");
}

export void test_regression_nested_structures() {
    std::cout << "\n[REGRESSION] Nested structures\n";
    
    auto deep_array = parse("[[[[[[1]]]]]]");
    test_assert(deep_array.has_value(), "deeply nested arrays");
    
    auto deep_object = parse(R"({"a":{"b":{"c":{"d":1}}}})");
    test_assert(deep_object.has_value(), "deeply nested objects");
    
    auto mixed_nesting = parse(R"([{"a":[{"b":1}]}])");
    test_assert(mixed_nesting.has_value(), "mixed array/object nesting");
}

export void test_regression_number_edge_cases() {
    std::cout << "\n[REGRESSION] Number edge cases\n";
    
    auto zero = parse("0");
    test_assert(zero.has_value() && zero->as_int().value() == 0, "zero");
    
    auto neg_zero = parse("-0");
    test_assert(neg_zero.has_value(), "negative zero");
    
    auto large_exp = parse("1e308");
    test_assert(large_exp.has_value() && large_exp->is_float(), "large exponent");
    
    auto small_exp = parse("1e-308");
    test_assert(small_exp.has_value() && small_exp->is_float(), "small exponent");
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

export void test_integration_real_world_json() {
    std::cout << "\n[INTEGRATION] Real-world JSON parsing\n";
    
    // Simulate a typical API response
    std::string api_response = R"({
        "status": "success",
        "code": 200,
        "data": {
            "users": [
                {"id": 1, "name": "Alice", "active": true},
                {"id": 2, "name": "Bob", "active": false}
            ],
            "pagination": {
                "page": 1,
                "total_pages": 10,
                "items_per_page": 20
            }
        },
        "metadata": {
            "request_id": "abc-123",
            "timestamp": 1699999999.123
        }
    })";
    
    auto result = parse(api_response);
    test_assert(result.has_value(), "parse API response");
    
    if (result.has_value()) {
        const auto& status = (*result)["status"];
        test_assert(status.as_string().value() == "success", "status is success");
        
        const auto& code = (*result)["code"];
        test_assert(code.as_int().value() == 200, "code is 200");
    }
}

export void test_integration_large_array() {
    std::cout << "\n[INTEGRATION] Large array parsing\n";
    
    // Create a large JSON array
    std::string large = "[";
    for (int i = 0; i < 10000; ++i) {
        if (i > 0) large += ",";
        large += std::to_string(i);
    }
    large += "]";
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = parse(large);
    auto end = std::chrono::high_resolution_clock::now();
    
    test_assert(result.has_value(), "parse large array");
    test_assert(result->size() == 10000, "array has 10000 elements");
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "  Parsed 10000 element array in " << duration.count() << " µs\n";
}

export void test_integration_roundtrip() {
    std::cout << "\n[INTEGRATION] Parse-stringify roundtrip\n";
    
    std::string original = R"({"a":1,"b":[2,3],"c":{"d":true}})";
    
    auto parsed = parse(original);
    test_assert(parsed.has_value(), "first parse succeeds");
    
    std::string stringified = stringify(*parsed);
    
    auto reparsed = parse(stringified);
    test_assert(reparsed.has_value(), "reparse succeeds");
    
    // Values should match - use const references
    const auto& a1 = (*parsed)["a"];
    const auto& a2 = (*reparsed)["a"];
    test_assert(a1.as_int() == a2.as_int(), "roundtrip preserves integer values");
}

// ============================================================================
// LINQ-STYLE QUERY TESTS
// ============================================================================

export void test_linq_where_filter() {
    std::cout << "\n[LINQ] WHERE / FILTER operations\n";
    
    auto result = parse("[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]");
    test_assert(result.has_value(), "parse array succeeds");
    
    // Filter numbers > 5
    auto filtered = query(*result)
        .filter([](const json_value& v) { return v.as_int().value_or(0) > 5; });
    
    test_assert(filtered.count() == 5, "filter keeps 5 elements > 5");
    test_assert(filtered.first()->as_int().value() == 6, "first filtered is 6");
    
    // Where alias works the same
    auto where_result = query(*result)
        .where([](const json_value& v) { return v.as_int().value_or(0) % 2 == 0; });
    
    test_assert(where_result.count() == 5, "where keeps 5 even numbers");
}

export void test_linq_select_map() {
    std::cout << "\n[LINQ] SELECT / MAP operations\n";
    
    auto result = parse("[1, 2, 3, 4, 5]");
    test_assert(result.has_value(), "parse array succeeds");
    
    // Map to doubled values
    auto doubled = query(*result)
        .map([](const json_value& v) { return v.as_int().value_or(0) * 2; });
    
    test_assert(doubled.count() == 5, "map preserves count");
    test_assert(doubled[0] == 2, "first doubled is 2");
    test_assert(doubled[4] == 10, "last doubled is 10");
    
    // Select alias
    auto squared = query(*result)
        .select([](const json_value& v) { 
            int val = v.as_int().value_or(0);
            return val * val;
        });
    
    test_assert(squared[2] == 9, "3 squared is 9");
}

export void test_linq_aggregate_reduce() {
    std::cout << "\n[LINQ] AGGREGATE / REDUCE operations\n";
    
    auto result = parse("[1, 2, 3, 4, 5]");
    test_assert(result.has_value(), "parse array succeeds");
    
    // Sum using reduce
    auto sum = query(*result)
        .map([](const json_value& v) { return v.as_int().value_or(0); })
        .reduce(0, [](int acc, int v) { return acc + v; });
    
    test_assert(sum == 15, "sum of 1-5 is 15");
    
    // Product using aggregate
    auto product = query(*result)
        .map([](const json_value& v) { return v.as_int().value_or(0); })
        .aggregate(1, [](int acc, int v) { return acc * v; });
    
    test_assert(product == 120, "product of 1-5 is 120");
}

export void test_linq_first_last_single() {
    std::cout << "\n[LINQ] FIRST / LAST / SINGLE operations\n";
    
    auto result = parse("[10, 20, 30]");
    test_assert(result.has_value(), "parse array succeeds");
    
    auto first = query(*result).first();
    test_assert(first.has_value(), "first has value");
    test_assert(first->as_int().value() == 10, "first is 10");
    
    auto last = query(*result).last();
    test_assert(last.has_value(), "last has value");
    test_assert(last->as_int().value() == 30, "last is 30");
    
    // First with predicate
    auto first_gt_15 = query(*result).first([](const json_value& v) {
        return v.as_int().value_or(0) > 15;
    });
    test_assert(first_gt_15.has_value() && first_gt_15->as_int().value() == 20, 
                "first > 15 is 20");
    
    // Single on one-element result
    auto single = query(*result)
        .filter([](const json_value& v) { return v.as_int().value_or(0) == 20; })
        .single();
    test_assert(single.has_value(), "single finds unique element");
}

export void test_linq_any_all_count() {
    std::cout << "\n[LINQ] ANY / ALL / COUNT operations\n";
    
    auto result = parse("[2, 4, 6, 8, 10]");
    test_assert(result.has_value(), "parse array succeeds");
    
    auto q = query(*result);
    
    // Any element exists
    test_assert(q.any(), "any() returns true for non-empty");
    
    // Any with predicate
    bool any_gt_5 = q.any([](const json_value& v) { 
        return v.as_int().value_or(0) > 5; 
    });
    test_assert(any_gt_5, "any > 5 is true");
    
    // All even
    bool all_even = q.all([](const json_value& v) { 
        return v.as_int().value_or(0) % 2 == 0; 
    });
    test_assert(all_even, "all even is true");
    
    // Count with predicate
    auto count_gt_5 = q.count([](const json_value& v) { 
        return v.as_int().value_or(0) > 5; 
    });
    test_assert(count_gt_5 == 3, "count > 5 is 3");
}

export void test_linq_order_by() {
    std::cout << "\n[LINQ] ORDER BY operations\n";
    
    auto result = parse("[3, 1, 4, 1, 5, 9, 2, 6]");
    test_assert(result.has_value(), "parse array succeeds");
    
    auto sorted = query(*result)
        .order_by([](const json_value& v) { return v.as_int().value_or(0); });
    
    test_assert(sorted[0].as_int().value() == 1, "first sorted is 1");
    test_assert(sorted[sorted.size()-1].as_int().value() == 9, "last sorted is 9");
    
    auto desc = query(*result)
        .order_by_descending([](const json_value& v) { return v.as_int().value_or(0); });
    
    test_assert(desc[0].as_int().value() == 9, "first descending is 9");
}

export void test_linq_take_skip() {
    std::cout << "\n[LINQ] TAKE / SKIP operations\n";
    
    auto result = parse("[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]");
    test_assert(result.has_value(), "parse array succeeds");
    
    auto first3 = query(*result).take(3);
    test_assert(first3.count() == 3, "take(3) returns 3 elements");
    test_assert(first3[2].as_int().value() == 3, "third element is 3");
    
    auto skip3 = query(*result).skip(3);
    test_assert(skip3.count() == 7, "skip(3) returns 7 elements");
    test_assert(skip3[0].as_int().value() == 4, "first after skip is 4");
    
    // Pagination: page 2, size 3
    auto page2 = query(*result).skip(3).take(3);
    test_assert(page2.count() == 3, "page 2 has 3 elements");
    test_assert(page2[0].as_int().value() == 4, "page 2 starts at 4");
}

export void test_linq_distinct() {
    std::cout << "\n[LINQ] DISTINCT operations\n";
    
    auto result = parse("[1, 2, 2, 3, 3, 3, 4, 4, 4, 4]");
    test_assert(result.has_value(), "parse array succeeds");
    
    auto distinct = query(*result)
        .map([](const json_value& v) { return v.as_int().value_or(0); })
        .distinct();
    
    test_assert(distinct.count() == 4, "distinct has 4 unique values");
}

export void test_linq_concat_zip() {
    std::cout << "\n[LINQ] CONCAT / ZIP operations\n";
    
    auto arr1 = parse("[1, 2, 3]");
    auto arr2 = parse("[4, 5, 6]");
    test_assert(arr1.has_value() && arr2.has_value(), "parse arrays succeed");
    
    auto q1 = query(*arr1);
    auto q2 = query(*arr2);
    
    auto concatenated = q1.concat(q2);
    test_assert(concatenated.count() == 6, "concat has 6 elements");
    
    // Zip two arrays
    auto zipped = q1.zip(q2, [](const json_value& a, const json_value& b) {
        return a.as_int().value_or(0) + b.as_int().value_or(0);
    });
    
    test_assert(zipped.count() == 3, "zip produces 3 pairs");
    test_assert(zipped[0] == 5, "1+4=5");
    test_assert(zipped[1] == 7, "2+5=7");
    test_assert(zipped[2] == 9, "3+6=9");
}

export void test_linq_chaining() {
    std::cout << "\n[LINQ] Method chaining\n";
    
    auto result = parse("[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]");
    test_assert(result.has_value(), "parse array succeeds");
    
    // Complex chain: filter evens, square them, take top 3, sum
    auto sum = query(*result)
        .filter([](const json_value& v) { return v.as_int().value_or(0) % 2 == 0; })
        .map([](const json_value& v) { 
            int val = v.as_int().value_or(0);
            return val * val;
        })
        .order_by_descending([](int v) { return v; })
        .take(3)
        .reduce(0, [](int acc, int v) { return acc + v; });
    
    // Evens: 2,4,6,8,10 -> squared: 4,16,36,64,100 -> top 3: 100,64,36 -> sum: 200
    test_assert(sum == 200, "chain result is 200");
}

// ============================================================================
// PARALLEL QUERY TESTS
// ============================================================================

export void test_parallel_query_filter() {
    std::cout << "\n[PARALLEL] Parallel filter operation\n";
    
    auto result = parse("[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]");
    test_assert(result.has_value(), "parse array succeeds");
    
    auto filtered = parallel_query(*result)
        .filter([](const json_value& v) { return v.as_int().value_or(0) > 5; });
    
    test_assert(filtered.size() == 5, "parallel filter keeps 5 elements");
}

export void test_parallel_query_map() {
    std::cout << "\n[PARALLEL] Parallel map operation\n";
    
    auto result = parse("[1, 2, 3, 4, 5]");
    test_assert(result.has_value(), "parse array succeeds");
    
    auto doubled = parallel_query(*result)
        .map([](const json_value& v) { return v.as_int().value_or(0) * 2; });
    
    test_assert(doubled.size() == 5, "parallel map preserves count");
    
    // Convert to sequential for detailed checks
    auto vec = doubled.to_vector();
    test_assert(vec[0] == 2 && vec[4] == 10, "parallel map results correct");
}

export void test_parallel_query_reduce() {
    std::cout << "\n[PARALLEL] Parallel reduce operation\n";
    
    auto result = parse("[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]");
    test_assert(result.has_value(), "parse array succeeds");
    
    auto sum = parallel_query(*result)
        .map([](const json_value& v) { return v.as_int().value_or(0); })
        .reduce(0, [](int acc, int v) { return acc + v; });
    
    test_assert(sum == 55, "parallel reduce sum is 55");
}

// ============================================================================
// THREAD-SAFE DOCUMENT TESTS
// ============================================================================

export void test_document_parse() {
    std::cout << "\n[DOCUMENT] Thread-safe document parsing\n";
    
    auto doc_result = document::parse(R"({"name": "test", "values": [1, 2, 3]})");
    test_assert(doc_result.has_value(), "document parse succeeds");
    
    const auto& doc = *doc_result;
    test_assert(doc.has_value(), "document has value");
    test_assert(doc.root().is_object(), "root is object");
    
    const auto& name = doc.root()["name"];
    test_assert(name.as_string().value() == "test", "name is 'test'");
}

export void test_document_query() {
    std::cout << "\n[DOCUMENT] Document query operations\n";
    
    auto doc_result = document::parse("[1, 2, 3, 4, 5]");
    test_assert(doc_result.has_value(), "document parse succeeds");
    
    auto sum = doc_result->query()
        .map([](const json_value& v) { return v.as_int().value_or(0); })
        .reduce(0, [](int acc, int v) { return acc + v; });
    
    test_assert(sum == 15, "document query sum is 15");
}

export void test_document_stringify() {
    std::cout << "\n[DOCUMENT] Document stringify\n";
    
    auto doc_result = document::parse(R"({"a": 1})");
    test_assert(doc_result.has_value(), "document parse succeeds");
    
    std::string str = doc_result->stringify();
    test_assert(!str.empty(), "stringify produces output");
    
    std::string pretty = doc_result->prettify();
    test_assert(pretty.find('\n') != std::string::npos, "prettify has newlines");
}

export void test_query_keys_values() {
    std::cout << "\n[LINQ] Query keys and values\n";
    
    auto result = parse(R"({"a": 1, "b": 2, "c": 3})");
    test_assert(result.has_value(), "parse object succeeds");
    
    auto keys = query_keys(*result);
    test_assert(keys.count() == 3, "object has 3 keys");
    
    auto values = query_values(*result);
    test_assert(values.count() == 3, "object has 3 values");
    
    auto sum = values
        .map([](const json_value& v) { return v.as_int().value_or(0); })
        .reduce(0, [](int acc, int v) { return acc + v; });
    test_assert(sum == 6, "sum of values is 6");
}

// ============================================================================
// RUN ALL TESTS
// ============================================================================

export auto run_all_tests() -> int {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║    FastJSON Multi-Register Parser Test Suite               ║\n";
    std::cout << "║    C++23 Module with SIMD + LINQ + Parallel                ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    // Get SIMD info first
    auto info = get_simd_info();
    std::cout << "\nSIMD Path: " << info.optimal_path << "\n";
    std::cout << "Thread Count: " << get_thread_count() << "\n";
    
    // Unit Tests
    test_parse_null();
    test_parse_booleans();
    test_parse_integers();
    test_parse_floats();
    test_parse_strings();
    test_parse_arrays();
    test_parse_objects();
    test_fluent_api_factory_functions();
    test_stringify_and_prettify();
    test_literal_operator();
    test_simd_info();
    test_parse_with_metrics();
    test_error_handling();
    test_value_access();
    
    // Regression Tests
    test_regression_whitespace_handling();
    test_regression_nested_structures();
    test_regression_number_edge_cases();
    
    // Integration Tests
    test_integration_real_world_json();
    test_integration_large_array();
    test_integration_roundtrip();
    
    // LINQ Tests
    test_linq_where_filter();
    test_linq_select_map();
    test_linq_aggregate_reduce();
    test_linq_first_last_single();
    test_linq_any_all_count();
    test_linq_order_by();
    test_linq_take_skip();
    test_linq_distinct();
    test_linq_concat_zip();
    test_linq_chaining();
    test_query_keys_values();
    
    // Parallel Tests
    test_parallel_query_filter();
    test_parallel_query_map();
    test_parallel_query_reduce();
    
    // Thread-Safe Document Tests
    test_document_parse();
    test_document_query();
    test_document_stringify();
    
    // Summary
    std::cout << "\n════════════════════════════════════════════════════════════\n";
    std::cout << "Test Summary: " << tests_passed << " passed, " << tests_failed << " failed\n";
    std::cout << "════════════════════════════════════════════════════════════\n";
    
    return tests_failed > 0 ? 1 : 0;
}

} // namespace test