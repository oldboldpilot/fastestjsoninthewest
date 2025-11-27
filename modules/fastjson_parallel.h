// Simplified header for fastjson_parallel.cpp
#pragma once

#include <expected>
#include <string>

namespace fastjson {

struct parse_config {
    int num_threads = 8;
    bool enable_simd = true;
    size_t parallel_threshold = 100;
    size_t chunk_size = 1024 * 1024;
};

extern parse_config g_config;

// Forward declarations
class json_value;

enum class json_error_code {
    empty_input,
    extra_tokens,
    max_depth_exceeded,
    unexpected_end,
    invalid_syntax,
    invalid_literal,
    invalid_number,
    invalid_string,
    invalid_escape,
    invalid_unicode
};

struct json_error {
    json_error_code code;
    std::string message;
    size_t line;
    size_t column;
};

using parse_result = std::expected<json_value, json_error>;

auto parse(const std::string& input) -> parse_result;

}  // namespace fastjson
