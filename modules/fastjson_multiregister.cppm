// FastestJSONInTheWest - Multi-Register SIMD Parser Module
// Copyright (c) 2025 - High-performance JSON parsing with multi-register SIMD
// ============================================================================

module;

// Global module fragment - all includes must be here
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <expected>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

export module fastjson_multiregister;

import fastjson_simd_multiregister;
import fastjson;  // Import base types and utilities

export namespace fastjson::multiregister {

// Re-export core types from base fastjson module
using fastjson::json_value;
using fastjson::json_object;
using fastjson::json_array;
using fastjson::json_error;
using fastjson::json_error_code;
template<typename T>
using json_result = fastjson::json_result<T>;

// ============================================================================
// Multi-Register SIMD Parser Implementation
// ============================================================================

class parser {
public:
    explicit parser(std::string_view input);
    auto parse() -> json_result<json_value>;

private:
    // Parsing methods using multi-register SIMD
    auto parse_value() -> json_result<json_value>;
    auto parse_null() -> json_result<json_value>;
    auto parse_boolean() -> json_result<json_value>;
    auto parse_number() -> json_result<json_value>;
    auto parse_string() -> json_result<json_value>;
    auto parse_array() -> json_result<json_value>;
    auto parse_object() -> json_result<json_value>;

    // Multi-register SIMD helper methods
    auto skip_whitespace() -> void;
    auto skip_whitespace_multiregister() -> const char*;
    auto find_string_end_multiregister(const char* start) -> const char*;
    auto parse_string_multiregister() -> json_result<std::string>;
    auto scan_structural_chars_multiregister() -> simd::multi::StructuralChars;

    // Standard helper methods
    auto peek() const noexcept -> char;
    auto advance() noexcept -> char;
    auto match(char expected) noexcept -> bool;
    auto is_at_end() const noexcept -> bool;
    auto make_error(json_error_code code, std::string message) const -> json_error;

    // Member variables
    const char* data_;
    const char* end_;
    const char* current_;
    size_t line_;
    size_t column_;
    size_t depth_;
    static constexpr size_t max_depth_ = 1000;
    
    // Multi-register SIMD context
    bool simd_available_;
    bool avx2_available_;
    bool avx512_available_;
};

// ============================================================================
// Parser Implementation
// ============================================================================

parser::parser(std::string_view input)
    : data_(input.data()), 
      end_(input.data() + input.size()), 
      current_(input.data()),
      line_(1), 
      column_(1), 
      depth_(0),
      simd_available_(simd::multi::has_avx2_support() || simd::multi::has_avx512_support()),
      avx2_available_(simd::multi::has_avx2_support()),
      avx512_available_(simd::multi::has_avx512_support()) {}

auto parser::parse() -> json_result<json_value> {
    skip_whitespace();
    if (is_at_end()) {
        return std::unexpected(make_error(json_error_code::empty_input, "Empty input"));
    }

    auto result = parse_value();
    if (!result) {
        return result;
    }

    skip_whitespace();
    if (!is_at_end()) {
        return std::unexpected(
            make_error(json_error_code::extra_tokens, "Unexpected characters after JSON value"));
    }

    return result;
}

auto parser::parse_value() -> json_result<json_value> {
    if (depth_ >= max_depth_) {
        return std::unexpected(
            make_error(json_error_code::recursion_too_deep, "Recursion depth exceeded"));
    }

    skip_whitespace();

    if (is_at_end()) {
        return std::unexpected(make_error(json_error_code::unexpected_end, "Unexpected end of input"));
    }

    switch (peek()) {
        case 'n':
            return parse_null();
        case 't':
        case 'f':
            return parse_boolean();
        case '"':
            return parse_string();
        case '[':
            return parse_array();
        case '{':
            return parse_object();
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return parse_number();
        default:
            return std::unexpected(
                make_error(json_error_code::invalid_character, 
                          std::string("Unexpected character: ") + peek()));
    }
}

auto parser::parse_null() -> json_result<json_value> {
    if (std::string_view(current_, std::min(4UL, static_cast<size_t>(end_ - current_))) == "null") {
        current_ += 4;
        column_ += 4;
        return json_value{};
    }
    return std::unexpected(make_error(json_error_code::invalid_null, "Expected 'null'"));
}

auto parser::parse_boolean() -> json_result<json_value> {
    if (std::string_view(current_, std::min(4UL, static_cast<size_t>(end_ - current_))) == "true") {
        current_ += 4;
        column_ += 4;
        return json_value{true};
    }
    if (std::string_view(current_, std::min(5UL, static_cast<size_t>(end_ - current_))) == "false") {
        current_ += 5;
        column_ += 5;
        return json_value{false};
    }
    return std::unexpected(make_error(json_error_code::invalid_boolean, "Expected 'true' or 'false'"));
}

auto parser::parse_string() -> json_result<json_value> {
    if (simd_available_) {
        auto result = parse_string_multiregister();
        if (result) {
            return json_value{std::move(*result)};
        }
        return std::unexpected(result.error());
    }
    
    // Fallback to scalar implementation
    return std::unexpected(make_error(json_error_code::invalid_string, "SIMD string parsing unavailable"));
}

auto parser::parse_string_multiregister() -> json_result<std::string> {
    if (peek() != '"') {
        return std::unexpected(make_error(json_error_code::invalid_string, "Expected '\"'"));
    }
    
    advance(); // Skip opening quote
    const char* start = current_;
    
    // Use multi-register SIMD to find string end
    const char* end_pos = find_string_end_multiregister(start);
    
    if (end_pos >= end_) {
        return std::unexpected(make_error(json_error_code::unterminated_string, "Unterminated string"));
    }
    
    std::string result(start, end_pos);
    current_ = end_pos + 1; // Skip closing quote
    column_ += (end_pos - start) + 2;
    
    return result;
}

auto parser::find_string_end_multiregister(const char* start) -> const char* {
    size_t start_offset = start - data_;
    size_t end_offset = simd::multi::find_string_end_multiregister(data_, end_ - data_, start_offset);
    return data_ + end_offset;
}

auto parser::parse_number() -> json_result<json_value> {
    const char* start = current_;
    
    // Handle sign
    if (peek() == '-') {
        advance();
    }
    
    // Parse integer part
    if (peek() == '0') {
        advance();
    } else if (std::isdigit(peek())) {
        while (!is_at_end() && std::isdigit(peek())) {
            advance();
        }
    } else {
        return std::unexpected(make_error(json_error_code::invalid_number, "Invalid number format"));
    }
    
    bool is_double = false;
    
    // Parse fractional part
    if (!is_at_end() && peek() == '.') {
        is_double = true;
        advance();
        if (is_at_end() || !std::isdigit(peek())) {
            return std::unexpected(make_error(json_error_code::invalid_number, "Invalid decimal number"));
        }
        while (!is_at_end() && std::isdigit(peek())) {
            advance();
        }
    }
    
    // Parse exponent
    if (!is_at_end() && (peek() == 'e' || peek() == 'E')) {
        is_double = true;
        advance();
        if (!is_at_end() && (peek() == '+' || peek() == '-')) {
            advance();
        }
        if (is_at_end() || !std::isdigit(peek())) {
            return std::unexpected(make_error(json_error_code::invalid_number, "Invalid exponent"));
        }
        while (!is_at_end() && std::isdigit(peek())) {
            advance();
        }
    }
    
    std::string_view number_str(start, current_ - start);
    
    if (is_double) {
        double value;
        auto [ptr, ec] = std::from_chars(number_str.data(), number_str.data() + number_str.size(), value);
        if (ec != std::errc{}) {
            return std::unexpected(make_error(json_error_code::invalid_number, "Failed to parse double"));
        }
        return json_value{value};
    } else {
        int64_t value;
        auto [ptr, ec] = std::from_chars(number_str.data(), number_str.data() + number_str.size(), value);
        if (ec != std::errc{}) {
            return std::unexpected(make_error(json_error_code::invalid_number, "Failed to parse integer"));
        }
        return json_value{value};
    }
}

auto parser::parse_array() -> json_result<json_value> {
    if (!match('[')) {
        return std::unexpected(make_error(json_error_code::invalid_array, "Expected '['"));
    }
    
    ++depth_;
    json_array arr;
    
    skip_whitespace();
    if (match(']')) {
        --depth_;
        return json_value{std::move(arr)};
    }
    
    while (true) {
        auto element = parse_value();
        if (!element) {
            --depth_;
            return std::unexpected(element.error());
        }
        
        arr.push_back(std::move(*element));
        
        skip_whitespace();
        if (match(',')) {
            skip_whitespace();
            continue;
        } else if (match(']')) {
            break;
        } else {
            --depth_;
            return std::unexpected(make_error(json_error_code::invalid_array, "Expected ',' or ']'"));
        }
    }
    
    --depth_;
    return json_value{std::move(arr)};
}

auto parser::parse_object() -> json_result<json_value> {
    if (!match('{')) {
        return std::unexpected(make_error(json_error_code::invalid_object, "Expected '{'"));
    }
    
    ++depth_;
    json_object obj;
    
    skip_whitespace();
    if (match('}')) {
        --depth_;
        return json_value{std::move(obj)};
    }
    
    while (true) {
        skip_whitespace();
        
        // Parse key
        if (peek() != '"') {
            --depth_;
            return std::unexpected(make_error(json_error_code::invalid_object, "Expected string key"));
        }
        
        auto key_result = parse_string_multiregister();
        if (!key_result) {
            --depth_;
            return std::unexpected(key_result.error());
        }
        
        skip_whitespace();
        if (!match(':')) {
            --depth_;
            return std::unexpected(make_error(json_error_code::invalid_object, "Expected ':'"));
        }
        
        // Parse value
        auto value = parse_value();
        if (!value) {
            --depth_;
            return std::unexpected(value.error());
        }
        
        obj[*key_result] = std::move(*value);
        
        skip_whitespace();
        if (match(',')) {
            skip_whitespace();
            continue;
        } else if (match('}')) {
            break;
        } else {
            --depth_;
            return std::unexpected(make_error(json_error_code::invalid_object, "Expected ',' or '}'"));
        }
    }
    
    --depth_;
    return json_value{std::move(obj)};
}

auto parser::skip_whitespace() -> void {
    if (simd_available_) {
        current_ = skip_whitespace_multiregister();
    } else {
        while (!is_at_end() && std::isspace(peek())) {
            if (peek() == '\n') {
                ++line_;
                column_ = 1;
            } else {
                ++column_;
            }
            advance();
        }
    }
}

auto parser::skip_whitespace_multiregister() -> const char* {
    size_t current_offset = current_ - data_;
    size_t new_offset = simd::multi::skip_whitespace_multiregister(data_, end_ - data_, current_offset);
    
    // Update line/column tracking for newlines
    for (size_t i = current_offset; i < new_offset; ++i) {
        if (data_[i] == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
    }
    
    return data_ + new_offset;
}

auto parser::scan_structural_chars_multiregister() -> simd::multi::StructuralChars {
    size_t current_offset = current_ - data_;
    return simd::multi::find_structural_chars_multiregister(data_, end_ - data_, current_offset);
}

auto parser::peek() const noexcept -> char {
    return is_at_end() ? '\0' : *current_;
}

auto parser::advance() noexcept -> char {
    if (is_at_end()) {
        return '\0';
    }
    
    char c = *current_++;
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

auto parser::match(char expected) noexcept -> bool {
    if (peek() != expected) {
        return false;
    }
    advance();
    return true;
}

auto parser::is_at_end() const noexcept -> bool {
    return current_ >= end_;
}

auto parser::make_error(json_error_code code, std::string message) const -> json_error {
    return json_error{code, std::move(message), line_, column_};
}

// ============================================================================
// Public API Functions (Fluent Interface)
// ============================================================================

// Main parsing function - exactly matches fastjson::parse signature
export auto parse(std::string_view input) -> json_result<json_value> {
    parser p(input);
    return p.parse();
}

// Convenience functions - exact matches with fastjson
export auto object() -> json_value {
    return json_value{json_object{}};
}

export auto array() -> json_value {
    return json_value{json_array{}};
}

export auto null() -> json_value {
    return json_value{};
}

export auto stringify(const json_value& value) -> std::string {
    return value.to_string();
}

export auto prettify(const json_value& value, int indent = 2) -> std::string {
    return value.to_pretty_string(indent);
}

// ============================================================================
// Multi-Register SIMD Specific Extensions
// ============================================================================

// Performance information
export struct simd_performance_info {
    bool avx2_available;
    bool avx512_available;
    std::string optimal_path;
    size_t register_count;
    size_t bytes_per_iteration;
};

export auto get_simd_info() -> simd_performance_info {
    bool avx2 = simd::multi::has_avx2_support();
    bool avx512 = simd::multi::has_avx512_support();
    
    return simd_performance_info{
        .avx2_available = avx2,
        .avx512_available = avx512,
        .optimal_path = avx512 ? "AVX-512 (8x registers)" : 
                       avx2 ? "AVX2 (4x registers)" : "Scalar fallback",
        .register_count = avx512 ? 8 : avx2 ? 4 : 1,
        .bytes_per_iteration = avx512 ? 512 : avx2 ? 128 : 1
    };
}

// Parse with performance metrics
export struct parse_result_with_metrics {
    json_result<json_value> result;
    simd_performance_info simd_info;
    std::chrono::nanoseconds parse_duration;
    size_t bytes_processed;
};

export auto parse_with_metrics(std::string_view input) -> parse_result_with_metrics {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = parse(input);
    auto end = std::chrono::high_resolution_clock::now();
    
    return parse_result_with_metrics{
        .result = std::move(result),
        .simd_info = get_simd_info(),
        .parse_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start),
        .bytes_processed = input.size()
    };
}

// Batch parsing for multiple JSON documents
export auto parse_batch(std::span<const std::string_view> inputs) -> std::vector<json_result<json_value>> {
    std::vector<json_result<json_value>> results;
    results.reserve(inputs.size());
    
    for (const auto& input : inputs) {
        results.push_back(parse(input));
    }
    
    return results;
}

// ============================================================================
// Literals Support (Exact Match)
// ============================================================================

export inline namespace literals {
    auto operator""_json_multi(const char* str, std::size_t len) -> json_result<json_value> {
        return parse(std::string_view{str, len});
    }
}

} // namespace fastjson::multiregister