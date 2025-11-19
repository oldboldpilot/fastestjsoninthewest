// Compatibility header for tests when module BMI isn't available
#pragma once
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fastjson {
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

template <typename T> using json_result = std::expected<T, json_error>;

// Forward-declare json_value with minimal API used by benchmarks
class json_value {
public:
    json_value() noexcept;
    json_value(const json_value&) = default;
    json_value(json_value&&) = default;
    ~json_value();
    auto to_string() const -> std::string;
    auto to_pretty_string(int indent) const -> std::string;
};

// Parse configuration structure
struct parse_config {
    int num_threads = -1;              // -1=auto, 0=disable, >0=exact count
    size_t parallel_threshold = 1000;  // Min size for parallel parsing
    bool enable_simd = true;
    bool enable_avx512 = true;
    bool enable_avx2 = true;
    bool enable_sse42 = true;
    bool enable_gpu = false;
    size_t gpu_threshold = 10000;
    size_t max_depth = 1000;
    size_t max_string_length = 1024 * 1024 * 10;
    size_t chunk_size = 100;
    bool use_memory_pool = true;
};

// API functions (defined in compiled library)
auto parse(std::string_view input) -> json_result<json_value>;
auto parse_with_config(std::string_view input, const parse_config& config)
    -> json_result<json_value>;
auto object() -> json_value;
auto array() -> json_value;
auto null() -> json_value;
auto stringify(const json_value& value) -> std::string;
auto prettify(const json_value& value, int indent) -> std::string;

// Configuration API
auto set_parse_config(const parse_config& config) -> void;
auto get_parse_config() -> const parse_config&;
auto set_num_threads(int threads) -> void;
auto get_num_threads() -> int;
}  // namespace fastjson
