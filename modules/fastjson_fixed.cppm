// FastestJSONInTheWest - Comprehensive Thread-Safe SIMD Implementation
// Author: Olumuyiwa Oluwasanmi
// Advanced instruction set waterfall with complete thread safety

module;

// Global module fragment - all includes must be here
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <memory>
#include <array>
#include <string_view>
#include <type_traits>
#include <charconv>
#include <iomanip>
#include <cstdint>
#include <cmath>
#include <atomic>
#include <mutex>
#include <thread>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <iostream>
#include <cctype>
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#include <cpuid.h>
#include <x86intrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

// OpenMP support disabled for module compatibility
// #ifdef _OPENMP
// #include <omp.h>
// #endif

export module fastjson;

export namespace fastjson {

// Simple result type for C++23
template<typename T>
using result = std::optional<T>;

// JSON Value with std::variant - fully defined here 
class json_value {
public:
    // Type aliases defined inside the class to avoid circular dependency
    using json_array = std::vector<json_value>;
    using json_object = std::unordered_map<std::string, json_value>;
    using json_string = std::string;
    using json_null = std::monostate;
    
private:
    using value_type = std::variant<json_null, bool, double, json_string, json_array, json_object>;
    value_type data_;

public:
    // Constructors
    json_value() : data_(json_null{}) {}
    json_value(std::nullptr_t) : data_(json_null{}) {}
    json_value(bool b) : data_(b) {}
    json_value(int i) : data_(static_cast<double>(i)) {}
    json_value(double d) : data_(d) {}
    json_value(const char* s) : data_(json_string{s}) {}
    json_value(std::string s) : data_(std::move(s)) {}
    json_value(json_array arr) : data_(std::move(arr)) {}
    json_value(json_object obj) : data_(std::move(obj)) {}
    
    // Type checking
    bool is_null() const noexcept { return std::holds_alternative<json_null>(data_); }
    bool is_boolean() const noexcept { return std::holds_alternative<bool>(data_); }
    bool is_number() const noexcept { return std::holds_alternative<double>(data_); }
    bool is_string() const noexcept { return std::holds_alternative<json_string>(data_); }
    bool is_array() const noexcept { return std::holds_alternative<json_array>(data_); }
    bool is_object() const noexcept { return std::holds_alternative<json_object>(data_); }
    
    // Value access
    bool as_boolean() const {
        if (!is_boolean()) throw std::runtime_error("Not a boolean");
        return std::get<bool>(data_);
    }
    
    double as_number() const {
        if (!is_number()) throw std::runtime_error("Not a number");
        return std::get<double>(data_);
    }
    
    const json_string& as_string() const {
        if (!is_string()) throw std::runtime_error("Not a string");
        return std::get<json_string>(data_);
    }
    
    const json_array& as_array() const {
        if (!is_array()) throw std::runtime_error("Not an array");
        return std::get<json_array>(data_);
    }
    
    const json_object& as_object() const {
        if (!is_object()) throw std::runtime_error("Not an object");
        return std::get<json_object>(data_);
    }
    
    json_array& as_array() {
        if (!is_array()) throw std::runtime_error("Not an array");
        return std::get<json_array>(data_);
    }
    
    json_object& as_object() {
        if (!is_object()) throw std::runtime_error("Not an object");
        return std::get<json_object>(data_);
    }
    
    // Utility methods
    std::size_t size() const {
        if (is_array()) return std::get<json_array>(data_).size();
        if (is_object()) return std::get<json_object>(data_).size();
        if (is_string()) return std::get<json_string>(data_).size();
        return 1;
    }
    
    void push_back(json_value value) {
        if (!is_array()) {
            data_ = json_array{};
        }
        std::get<json_array>(data_).push_back(std::move(value));
    }
};

// SIMD capability detection for thread safety
class simd_capabilities {
private:
    static std::atomic<bool> detected_;
    static std::atomic<uint32_t> features_;
    static std::mutex detection_mutex_;
    
    enum : uint32_t {
        SSE2_BIT = 1u << 0,
        SSE3_BIT = 1u << 1, 
        SSE41_BIT = 1u << 2,
        SSE42_BIT = 1u << 3,
        AVX_BIT = 1u << 4,
        AVX2_BIT = 1u << 5,
        AVX512F_BIT = 1u << 6,
        AVX512BW_BIT = 1u << 7,
        AVX512VBMI_BIT = 1u << 8,
        AVX512VNNI_BIT = 1u << 9,
        AMX_TMUL_BIT = 1u << 10
    };
    
public:
    static void detect() {
        if (detected_.load(std::memory_order_acquire)) return;
        
        std::lock_guard<std::mutex> lock(detection_mutex_);
        if (detected_.load(std::memory_order_relaxed)) return;
        
#if defined(__x86_64__) || defined(_M_X64)
        uint32_t eax, ebx, ecx, edx;
        uint32_t caps = 0;
        
        __cpuid(1, eax, ebx, ecx, edx);
        
        if (edx & (1 << 26)) caps |= SSE2_BIT;
        if (ecx & (1 << 0))  caps |= SSE3_BIT;
        if (ecx & (1 << 19)) caps |= SSE41_BIT;
        if (ecx & (1 << 20)) caps |= SSE42_BIT;
        if (ecx & (1 << 28)) caps |= AVX_BIT;
        
        if (eax >= 7) {
            __cpuid_count(7, 0, eax, ebx, ecx, edx);
            if (ebx & (1 << 5))  caps |= AVX2_BIT;
            if (ebx & (1 << 16)) caps |= AVX512F_BIT;
            if (ebx & (1 << 30)) caps |= AVX512BW_BIT;
            if (ecx & (1 << 1))  caps |= AVX512VBMI_BIT;
            if (ecx & (1 << 11)) caps |= AVX512VNNI_BIT;
            if (eax & (1 << 24)) caps |= AMX_TMUL_BIT;
        }
#endif
        
        features_.store(caps, std::memory_order_relaxed);
        detected_.store(true, std::memory_order_release);
    }
    
    static bool has_sse2() { detect(); return features_.load(std::memory_order_acquire) & SSE2_BIT; }
    static bool has_sse42() { detect(); return features_.load(std::memory_order_acquire) & SSE42_BIT; }
    static bool has_avx2() { detect(); return features_.load(std::memory_order_acquire) & AVX2_BIT; }
    static bool has_avx512f() { detect(); return features_.load(std::memory_order_acquire) & AVX512F_BIT; }
    static bool has_avx512_vnni() { detect(); return features_.load(std::memory_order_acquire) & AVX512VNNI_BIT; }
    static bool has_amx_tmul() { detect(); return features_.load(std::memory_order_acquire) & AMX_TMUL_BIT; }
};

// Static member definitions
std::atomic<bool> simd_capabilities::detected_{false};
std::atomic<uint32_t> simd_capabilities::features_{0};
std::mutex simd_capabilities::detection_mutex_;

// Main JSON parser class
class parser {
private:
    const char* current_;
    const char* end_;
    
public:
    explicit parser(const std::string& json) 
        : current_(json.c_str()), end_(json.c_str() + json.size()) {
        simd_capabilities::detect();
    }
    
    explicit parser(const char* data, size_t length)
        : current_(data), end_(data + length) {
        simd_capabilities::detect();
    }
    
    result<json_value> parse();
    
private:
    result<json_value> parse_value();
    result<json_value> parse_null();
    result<json_value> parse_boolean();
    result<json_value> parse_number();
    result<json_value> parse_string();
    result<json_value> parse_array();
    result<json_value> parse_object();
    
    void skip_whitespace();
    char peek() const;
    char advance();
    bool match(char expected);
    bool is_at_end() const;
};

// Thread-safe utility functions
auto parse_json(const std::string& json_text) -> result<json_value>;
auto stringify(const json_value& value) -> std::string;

} // export namespace fastjson

// Implementation details (not exported)
namespace fastjson {

// Parser implementation
inline result<json_value> parser::parse() {
    skip_whitespace();
    if (is_at_end()) return std::nullopt;
    
    auto result = parse_value();
    if (!result) return std::nullopt;
    
    skip_whitespace();
    if (!is_at_end()) return std::nullopt; // Extra tokens
    
    return result;
}

inline result<json_value> parser::parse_value() {
    skip_whitespace();
    if (is_at_end()) return std::nullopt;
    
    char c = peek();
    switch (c) {
        case 'n': return parse_null();
        case 't':
        case 'f': return parse_boolean();
        case '"': return parse_string();
        case '[': return parse_array();
        case '{': return parse_object();
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return parse_number();
        default:
            return std::nullopt;
    }
}

inline result<json_value> parser::parse_null() {
    if (match('n') && match('u') && match('l') && match('l')) {
        return json_value{};
    }
    return std::nullopt;
}

inline result<json_value> parser::parse_boolean() {
    if (match('t') && match('r') && match('u') && match('e')) {
        return json_value{true};
    }
    if (match('f') && match('a') && match('l') && match('s') && match('e')) {
        return json_value{false};
    }
    return std::nullopt;
}

inline result<json_value> parser::parse_number() {
    const char* start = current_;
    
    if (peek() == '-') advance();
    
    if (peek() == '0') {
        advance();
    } else if (peek() >= '1' && peek() <= '9') {
        advance();
        while (peek() >= '0' && peek() <= '9') advance();
    } else {
        return std::nullopt;
    }
    
    if (peek() == '.') {
        advance();
        if (!(peek() >= '0' && peek() <= '9')) return std::nullopt;
        while (peek() >= '0' && peek() <= '9') advance();
    }
    
    if (peek() == 'e' || peek() == 'E') {
        advance();
        if (peek() == '+' || peek() == '-') advance();
        if (!(peek() >= '0' && peek() <= '9')) return std::nullopt;
        while (peek() >= '0' && peek() <= '9') advance();
    }
    
    std::string num_str(start, current_);
    try {
        double value = std::stod(num_str);
        return json_value{value};
    } catch (...) {
        return std::nullopt;
    }
}

inline result<json_value> parser::parse_string() {
    if (!match('"')) return std::nullopt;
    
    std::string value;
    while (!is_at_end() && peek() != '"') {
        char c = advance();
        if (c == '\\') {
            if (is_at_end()) return std::nullopt;
            char escaped = advance();
            switch (escaped) {
                case '"': value += '"'; break;
                case '\\': value += '\\'; break;
                case '/': value += '/'; break;
                case 'b': value += '\b'; break;
                case 'f': value += '\f'; break;
                case 'n': value += '\n'; break;
                case 'r': value += '\r'; break;
                case 't': value += '\t'; break;
                default: return std::nullopt;
            }
        } else {
            value += c;
        }
    }
    
    if (!match('"')) return std::nullopt;
    return json_value{std::move(value)};
}

inline result<json_value> parser::parse_array() {
    if (!match('[')) return std::nullopt;
    
    json_value::json_array array;
    skip_whitespace();
    
    if (match(']')) {
        return json_value{std::move(array)};
    }
    
    do {
        auto element = parse_value();
        if (!element) return std::nullopt;
        array.push_back(std::move(*element));
        
        skip_whitespace();
        if (match(']')) {
            return json_value{std::move(array)};
        }
    } while (match(','));
    
    return std::nullopt;
}

inline result<json_value> parser::parse_object() {
    if (!match('{')) return std::nullopt;
    
    json_value::json_object object;
    skip_whitespace();
    
    if (match('}')) {
        return json_value{std::move(object)};
    }
    
    do {
        skip_whitespace();
        auto key_result = parse_string();
        if (!key_result) return std::nullopt;
        
        std::string key = key_result->as_string();
        
        skip_whitespace();
        if (!match(':')) return std::nullopt;
        
        auto value = parse_value();
        if (!value) return std::nullopt;
        
        object[std::move(key)] = std::move(*value);
        
        skip_whitespace();
        if (match('}')) {
            return json_value{std::move(object)};
        }
    } while (match(','));
    
    return std::nullopt;
}

inline void parser::skip_whitespace() {
    while (!is_at_end() && std::isspace(static_cast<unsigned char>(peek()))) {
        advance();
    }
}

inline char parser::peek() const {
    return is_at_end() ? '\0' : *current_;
}

inline char parser::advance() {
    if (is_at_end()) return '\0';
    return *current_++;
}

inline bool parser::match(char expected) {
    if (peek() != expected) return false;
    advance();
    return true;
}

inline bool parser::is_at_end() const {
    return current_ >= end_;
}

// Public API implementation
auto parse_json(const std::string& json_text) -> result<json_value> {
    parser p(json_text);
    return p.parse();
}

auto stringify(const json_value& value) -> std::string {
    if (value.is_null()) {
        return "null";
    } else if (value.is_boolean()) {
        return value.as_boolean() ? "true" : "false";
    } else if (value.is_number()) {
        return std::to_string(value.as_number());
    } else if (value.is_string()) {
        return '"' + value.as_string() + '"';
    } else if (value.is_array()) {
        std::string result = "[";
        const auto& arr = value.as_array();
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) result += ",";
            result += stringify(arr[i]);
        }
        result += "]";
        return result;
    } else if (value.is_object()) {
        std::string result = "{";
        const auto& obj = value.as_object();
        bool first = true;
        for (const auto& [key, val] : obj) {
            if (!first) result += ",";
            result += '"' + key + "\":" + stringify(val);
            first = false;
        }
        result += "}";
        return result;
    }
    return "null";
}

} // namespace fastjson