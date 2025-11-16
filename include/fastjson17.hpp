// FastestJSONInTheWest - C++17 Compatible Header-Only Implementation
// Author: Olumuyiwa Oluwasanmi
// High-Performance JSON Library with SIMD Optimizations and C++17 Compatibility

#ifndef FASTJSON17_HPP
#define FASTJSON17_HPP

// C++17 Standard Library
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <string_view>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <type_traits>
#include <cstdint>
#include <cmath>
#include <atomic>
#include <mutex>
#include <thread>
#include <array>
#include <cctype>
#include <cstring>
#include <charconv>
#include <iomanip>

// SIMD Headers (conditionally included)
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#include <cpuid.h>
#include <x86intrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

namespace fastjson17 {

// C++17 Compatibility Layer
// ============================================================================

// Custom expected-like class for C++17 compatibility
template<typename T, typename E>
class expected {
private:
    std::variant<T, E> data_;
    
public:
    expected(T&& value) : data_(std::move(value)) {}
    expected(const T& value) : data_(value) {}
    expected(E&& error) : data_(std::move(error)) {}
    expected(const E& error) : data_(error) {}
    
    bool has_value() const noexcept {
        return std::holds_alternative<T>(data_);
    }
    
    explicit operator bool() const noexcept {
        return has_value();
    }
    
    const T& value() const & {
        if (!has_value()) {
            throw std::runtime_error("Expected contains error");
        }
        return std::get<T>(data_);
    }
    
    T&& value() && {
        if (!has_value()) {
            throw std::runtime_error("Expected contains error");
        }
        return std::get<T>(std::move(data_));
    }
    
    const T& operator*() const & {
        return value();
    }
    
    T&& operator*() && {
        return std::move(*this).value();
    }
    
    const T* operator->() const {
        return &value();
    }
    
    const E& error() const & {
        if (has_value()) {
            throw std::runtime_error("Expected contains value, not error");
        }
        return std::get<E>(data_);
    }
};

template<typename E>
class unexpected {
private:
    E error_;
    
public:
    explicit unexpected(E&& error) : error_(std::move(error)) {}
    explicit unexpected(const E& error) : error_(error) {}
    
    const E& value() const & { return error_; }
    E&& value() && { return std::move(error_); }
};

// Error handling for JSON parsing
// ============================================================================

enum class json_error_code {
    none = 0,
    empty_input,
    invalid_syntax,
    invalid_literal,
    invalid_number,
    invalid_string,
    unexpected_end,
    extra_tokens,
    max_depth_exceeded
};

struct json_error {
    json_error_code code;
    std::string message;
    std::size_t line;
    std::size_t column;
};

// Forward declarations
class json_value;
using json_array = std::vector<json_value>;
using json_object = std::unordered_map<std::string, json_value>;
using json_string = std::string;
using json_null = std::monostate;

template<typename T>
using json_result = expected<T, json_error>;

// JSON Value Implementation with C++17 std::variant
// ============================================================================

class json_value {
private:
    using value_type = std::variant<json_null, bool, double, json_string, json_array, json_object>;
    value_type data_;

public:
    // Constructors
    json_value() : data_(json_null{}) {}
    json_value(std::nullptr_t) : data_(json_null{}) {}
    json_value(bool b) : data_(b) {}
    json_value(int i) : data_(static_cast<double>(i)) {}
    json_value(long l) : data_(static_cast<double>(l)) {}
    json_value(long long ll) : data_(static_cast<double>(ll)) {}
    json_value(unsigned u) : data_(static_cast<double>(u)) {}
    json_value(unsigned long ul) : data_(static_cast<double>(ul)) {}
    json_value(unsigned long long ull) : data_(static_cast<double>(ull)) {}
    json_value(float f) : data_(static_cast<double>(f)) {}
    json_value(double d) : data_(d) {}
    json_value(const char* s) : data_(json_string{s}) {}
    json_value(const std::string& s) : data_(s) {}
    json_value(std::string&& s) : data_(std::move(s)) {}
    json_value(std::string_view sv) : data_(json_string{sv}) {}
    json_value(const json_array& arr) : data_(arr) {}
    json_value(json_array&& arr) : data_(std::move(arr)) {}
    json_value(const json_object& obj) : data_(obj) {}
    json_value(json_object&& obj) : data_(std::move(obj)) {}
    json_value(std::initializer_list<json_value> list) : data_(json_array{list}) {}
    
    // Type checking
    bool is_null() const noexcept { return std::holds_alternative<json_null>(data_); }
    bool is_boolean() const noexcept { return std::holds_alternative<bool>(data_); }
    bool is_number() const noexcept { return std::holds_alternative<double>(data_); }
    bool is_string() const noexcept { return std::holds_alternative<json_string>(data_); }
    bool is_array() const noexcept { return std::holds_alternative<json_array>(data_); }
    bool is_object() const noexcept { return std::holds_alternative<json_object>(data_); }
    
    // Value accessors
    bool as_boolean() const {
        if (!is_boolean()) throw std::runtime_error("JSON value is not a boolean");
        return std::get<bool>(data_);
    }
    
    double as_number() const {
        if (!is_number()) throw std::runtime_error("JSON value is not a number");
        return std::get<double>(data_);
    }
    
    const std::string& as_string() const {
        if (!is_string()) throw std::runtime_error("JSON value is not a string");
        return std::get<json_string>(data_);
    }
    
    const json_array& as_array() const {
        if (!is_array()) throw std::runtime_error("JSON value is not an array");
        return std::get<json_array>(data_);
    }
    
    json_array& as_array() {
        if (!is_array()) throw std::runtime_error("JSON value is not an array");
        return std::get<json_array>(data_);
    }
    
    const json_object& as_object() const {
        if (!is_object()) throw std::runtime_error("JSON value is not an object");
        return std::get<json_object>(data_);
    }
    
    json_object& as_object() {
        if (!is_object()) throw std::runtime_error("JSON value is not an object");
        return std::get<json_object>(data_);
    }
    
    // Array operations
    json_value& push_back(json_value value) {
        if (!is_array()) throw std::runtime_error("Cannot push_back on non-array");
        std::get<json_array>(data_).emplace_back(std::move(value));
        return *this;
    }
    
    json_value& pop_back() {
        if (!is_array()) throw std::runtime_error("Cannot pop_back on non-array");
        auto& arr = std::get<json_array>(data_);
        if (arr.empty()) throw std::runtime_error("Cannot pop_back on empty array");
        arr.pop_back();
        return *this;
    }
    
    std::size_t size() const noexcept {
        return std::visit([](const auto& v) -> std::size_t {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_array> || std::is_same_v<T, json_object>) {
                return v.size();
            } else if constexpr (std::is_same_v<T, json_string>) {
                return v.length();
            } else {
                return 1;
            }
        }, data_);
    }
    
    bool empty() const noexcept {
        return std::visit([](const auto& v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_array> || std::is_same_v<T, json_object>) {
                return v.empty();
            } else if constexpr (std::is_same_v<T, json_string>) {
                return v.empty();
            } else if constexpr (std::is_same_v<T, json_null>) {
                return true;
            } else {
                return false;
            }
        }, data_);
    }
    
    // Object operations
    json_value& insert(const std::string& key, json_value value) {
        if (!is_object()) throw std::runtime_error("Cannot insert on non-object");
        std::get<json_object>(data_)[key] = std::move(value);
        return *this;
    }
    
    bool contains(const std::string& key) const noexcept {
        if (!is_object()) return false;
        const auto& obj = std::get<json_object>(data_);
        return obj.find(key) != obj.end();
    }
    
    // Indexing operators
    json_value& operator[](std::size_t index) {
        if (!is_array()) throw std::runtime_error("Cannot index non-array with size_t");
        auto& arr = std::get<json_array>(data_);
        if (index >= arr.size()) throw std::out_of_range("Array index out of range");
        return arr[index];
    }
    
    const json_value& operator[](std::size_t index) const {
        if (!is_array()) throw std::runtime_error("Cannot index non-array with size_t");
        const auto& arr = std::get<json_array>(data_);
        if (index >= arr.size()) throw std::out_of_range("Array index out of range");
        return arr[index];
    }
    
    json_value& operator[](const std::string& key) {
        if (!is_object()) throw std::runtime_error("Cannot index non-object with string");
        return std::get<json_object>(data_)[key];
    }
    
    const json_value& operator[](const std::string& key) const {
        if (!is_object()) throw std::runtime_error("Cannot index non-object with string");
        const auto& obj = std::get<json_object>(data_);
        auto it = obj.find(key);
        if (it == obj.end()) throw std::out_of_range("Object key not found: " + key);
        return it->second;
    }
    
    // Serialization
    std::string to_string() const;
    std::string to_pretty_string(int indent = 2) const;
    
private:
    void serialize_to_buffer(std::string& buffer) const;
    void serialize_pretty_to_buffer(std::string& buffer, int indent_size, int current_indent) const;
    void serialize_string_to_buffer(std::string& buffer, const std::string& str) const;
};

// SIMD Optimizations for C++17
// ============================================================================

#ifdef FASTJSON_ENABLE_SIMD

#if defined(__x86_64__) || defined(_M_X64)

// SIMD capability flags
constexpr uint32_t SIMD_SSE2 = 0x002;
constexpr uint32_t SIMD_SSE3 = 0x004;
constexpr uint32_t SIMD_SSSE3 = 0x008;
constexpr uint32_t SIMD_SSE41 = 0x010;
constexpr uint32_t SIMD_SSE42 = 0x020;
constexpr uint32_t SIMD_AVX = 0x040;
constexpr uint32_t SIMD_AVX2 = 0x080;
constexpr uint32_t SIMD_AVX512F = 0x100;
constexpr uint32_t SIMD_AVX512BW = 0x200;

// Thread-safe SIMD capability detection
inline uint32_t detect_simd_capabilities() noexcept {
    static std::atomic<uint32_t> cached_caps{0};
    static std::once_flag init_flag;
    
    std::call_once(init_flag, []() {
        uint32_t caps = 0;
        uint32_t eax, ebx, ecx, edx;
        
        // Check for basic CPUID support
        __cpuid(0, eax, ebx, ecx, edx);
        
        if (eax >= 1) {
            __cpuid(1, eax, ebx, ecx, edx);
            
            if (edx & (1 << 26)) caps |= SIMD_SSE2;
            if (ecx & (1 << 0))  caps |= SIMD_SSE3;
            if (ecx & (1 << 9))  caps |= SIMD_SSSE3;
            if (ecx & (1 << 19)) caps |= SIMD_SSE41;
            if (ecx & (1 << 20)) caps |= SIMD_SSE42;
            if (ecx & (1 << 28)) caps |= SIMD_AVX;
        }
        
        // Check for extended features
        if (eax >= 7) {
            __cpuid_count(7, 0, eax, ebx, ecx, edx);
            
            if (ebx & (1 << 5))  caps |= SIMD_AVX2;
            if (ebx & (1 << 16)) caps |= SIMD_AVX512F;
            if (ebx & (1 << 30)) caps |= SIMD_AVX512BW;
        }
        
        cached_caps.store(caps, std::memory_order_release);
    });
    
    return cached_caps.load(std::memory_order_acquire);
}

// AVX-512 optimized whitespace skipping
inline const char* skip_whitespace_avx512(const char* data, std::size_t size) {
    const char* ptr = data;
    const char* end = data + size;
    
#ifdef __AVX512F__
    while (ptr + 64 <= end) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
        
        __mmask64 space_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8(' '));
        __mmask64 tab_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\t'));
        __mmask64 newline_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\n'));
        __mmask64 carriage_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\r'));
        
        __mmask64 whitespace_mask = space_mask | tab_mask | newline_mask | carriage_mask;
        
        if (whitespace_mask != 0xFFFFFFFFFFFFFFFF) {
            int first_non_ws = __builtin_ctzll(~whitespace_mask);
            return ptr + first_non_ws;
        }
        
        ptr += 64;
    }
#endif
    
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }
    
    return ptr;
}

// AVX2 optimized whitespace skipping
inline const char* skip_whitespace_avx2(const char* data, std::size_t size) {
    const char* ptr = data;
    const char* end = data + size;
    
#ifdef __AVX2__
    while (ptr + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        
        __m256i space_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(' '));
        __m256i tab_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\t'));
        __m256i newline_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\n'));
        __m256i carriage_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\r'));
        
        __m256i whitespace_mask = _mm256_or_si256(
            _mm256_or_si256(space_cmp, tab_cmp),
            _mm256_or_si256(newline_cmp, carriage_cmp)
        );
        
        uint32_t mask = ~_mm256_movemask_epi8(whitespace_mask);
        if (mask != 0) {
            int first_non_ws = __builtin_ctz(mask);
            return ptr + first_non_ws;
        }
        
        ptr += 32;
    }
#endif
    
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }
    
    return ptr;
}

// SSE2 optimized whitespace skipping
inline const char* skip_whitespace_sse2(const char* data, std::size_t size) {
    const char* ptr = data;
    const char* end = data + size;
    
#ifdef __SSE2__
    while (ptr + 16 <= end) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        
        __m128i space_cmp = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(' '));
        __m128i tab_cmp = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\t'));
        __m128i newline_cmp = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\n'));
        __m128i carriage_cmp = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\r'));
        
        __m128i whitespace_mask = _mm_or_si128(
            _mm_or_si128(space_cmp, tab_cmp),
            _mm_or_si128(newline_cmp, carriage_cmp)
        );
        
        uint32_t mask = ~_mm_movemask_epi8(whitespace_mask);
        if (mask != 0) {
            int first_non_ws = __builtin_ctz(mask);
            return ptr + first_non_ws;
        }
        
        ptr += 16;
    }
#endif
    
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }
    
    return ptr;
}

// Runtime SIMD dispatcher
inline const char* skip_whitespace_simd(const char* data, std::size_t size) {
    static const uint32_t simd_caps = detect_simd_capabilities();
    
    if (simd_caps & SIMD_AVX512F) {
        return skip_whitespace_avx512(data, size);
    } else if (simd_caps & SIMD_AVX2) {
        return skip_whitespace_avx2(data, size);
    } else if (simd_caps & SIMD_SSE2) {
        return skip_whitespace_sse2(data, size);
    }
    
    // Scalar fallback
    const char* ptr = data;
    const char* end = data + size;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }
    return ptr;
}

#endif // x86_64

#else // !FASTJSON_ENABLE_SIMD

inline const char* skip_whitespace_simd(const char* data, std::size_t size) {
    const char* ptr = data;
    const char* end = data + size;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }
    return ptr;
}

#endif // FASTJSON_ENABLE_SIMD

// JSON Parser for C++17
// ============================================================================

class parser {
private:
    const char* data_;
    const char* end_;
    const char* current_;
    std::size_t line_;
    std::size_t column_;
    int depth_;
    static constexpr int max_depth_ = 512;

public:
    explicit parser(std::string_view input) 
        : data_(input.data())
        , end_(input.data() + input.size())
        , current_(input.data())
        , line_(1)
        , column_(1)
        , depth_(0) {}
    
    json_result<json_value> parse() {
        skip_whitespace();
        if (is_at_end()) {
            return unexpected<json_error>({json_error_code::empty_input, "Empty input", line_, column_});
        }
        
        auto result = parse_value();
        if (!result) {
            return result;
        }
        
        skip_whitespace();
        if (!is_at_end()) {
            return unexpected<json_error>({json_error_code::extra_tokens, "Unexpected characters after JSON value", line_, column_});
        }
        
        return result;
    }
    
    json_result<json_value> parse_value() {
        if (depth_ >= max_depth_) {
            return unexpected<json_error>({json_error_code::max_depth_exceeded, "Maximum nesting depth exceeded", line_, column_});
        }
        
        skip_whitespace();
        
        if (is_at_end()) {
            return unexpected<json_error>({json_error_code::unexpected_end, "Unexpected end of input", line_, column_});
        }
        
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
                return unexpected<json_error>({json_error_code::invalid_syntax, "Unexpected character: " + std::string(1, c), line_, column_});
        }
    }

private:
    json_result<json_value> parse_null() {
        if (!match('n') || !match('u') || !match('l') || !match('l')) {
            return unexpected<json_error>({json_error_code::invalid_literal, "Invalid null literal", line_, column_});
        }
        return json_value{};
    }
    
    json_result<json_value> parse_boolean() {
        if (match('t')) {
            if (!match('r') || !match('u') || !match('e')) {
                return unexpected<json_error>({json_error_code::invalid_literal, "Invalid true literal", line_, column_});
            }
            return json_value{true};
        } else if (match('f')) {
            if (!match('a') || !match('l') || !match('s') || !match('e')) {
                return unexpected<json_error>({json_error_code::invalid_literal, "Invalid false literal", line_, column_});
            }
            return json_value{false};
        }
        
        return unexpected<json_error>({json_error_code::invalid_literal, "Invalid boolean literal", line_, column_});
    }
    
    json_result<json_value> parse_number() {
        const char* start = current_;
        
        // Handle negative sign
        if (peek() == '-') {
            advance();
        }
        
        // Handle integer part
        if (peek() == '0') {
            advance();
        } else if (peek() >= '1' && peek() <= '9') {
            advance();
            while (peek() >= '0' && peek() <= '9') {
                advance();
            }
        } else {
            return unexpected<json_error>({json_error_code::invalid_number, "Invalid number format", line_, column_});
        }
        
        // Handle fractional part
        if (peek() == '.') {
            advance();
            if (!(peek() >= '0' && peek() <= '9')) {
                return unexpected<json_error>({json_error_code::invalid_number, "Invalid decimal number", line_, column_});
            }
            while (peek() >= '0' && peek() <= '9') {
                advance();
            }
        }
        
        // Handle exponent
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') {
                advance();
            }
            if (!(peek() >= '0' && peek() <= '9')) {
                return unexpected<json_error>({json_error_code::invalid_number, "Invalid exponent", line_, column_});
            }
            while (peek() >= '0' && peek() <= '9') {
                advance();
            }
        }
        
        // Convert to double
        std::string num_str(start, current_);
        double value;
        
#if __has_include(<charconv>) && __cpp_lib_to_chars >= 201611L
        auto [ptr, ec] = std::from_chars(start, current_, value);
        if (ec != std::errc{} || ptr != current_) {
            return unexpected<json_error>({json_error_code::invalid_number, "Failed to parse number", line_, column_});
        }
#else
        try {
            value = std::stod(num_str);
        } catch (const std::exception&) {
            return unexpected<json_error>({json_error_code::invalid_number, "Failed to parse number", line_, column_});
        }
#endif
        
        return json_value{value};
    }
    
    json_result<json_value> parse_string() {
        if (!match('"')) {
            return unexpected<json_error>({json_error_code::invalid_string, "Expected opening quote", line_, column_});
        }
        
        std::string value;
        value.reserve(64);
        
        while (!is_at_end() && peek() != '"') {
            char c = advance();
            
            if (c == '\\') {
                if (is_at_end()) {
                    return unexpected<json_error>({json_error_code::invalid_string, "Unterminated escape sequence", line_, column_});
                }
                
                char escaped = advance();
                switch (escaped) {
                    case '"':  value += '"'; break;
                    case '\\': value += '\\'; break;
                    case '/':  value += '/'; break;
                    case 'b':  value += '\b'; break;
                    case 'f':  value += '\f'; break;
                    case 'n':  value += '\n'; break;
                    case 'r':  value += '\r'; break;
                    case 't':  value += '\t'; break;
                    case 'u': {
                        // Unicode escape sequence
                        if (current_ + 4 > end_) {
                            return unexpected<json_error>({json_error_code::invalid_string, "Incomplete Unicode escape", line_, column_});
                        }
                        
                        uint32_t codepoint = 0;
                        for (int i = 0; i < 4; ++i) {
                            char hex = advance();
                            if (hex >= '0' && hex <= '9') {
                                codepoint = (codepoint << 4) | (hex - '0');
                            } else if (hex >= 'a' && hex <= 'f') {
                                codepoint = (codepoint << 4) | (hex - 'a' + 10);
                            } else if (hex >= 'A' && hex <= 'F') {
                                codepoint = (codepoint << 4) | (hex - 'A' + 10);
                            } else {
                                return unexpected<json_error>({json_error_code::invalid_string, "Invalid Unicode escape", line_, column_});
                            }
                        }
                        
                        // Convert Unicode codepoint to UTF-8
                        if (codepoint <= 0x7F) {
                            value += static_cast<char>(codepoint);
                        } else if (codepoint <= 0x7FF) {
                            value += static_cast<char>(0xC0 | (codepoint >> 6));
                            value += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else {
                            value += static_cast<char>(0xE0 | (codepoint >> 12));
                            value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            value += static_cast<char>(0x80 | (codepoint & 0x3F));
                        }
                        break;
                    }
                    default:
                        return unexpected<json_error>({json_error_code::invalid_string, "Invalid escape sequence", line_, column_});
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                return unexpected<json_error>({json_error_code::invalid_string, "Control character in string", line_, column_});
            } else {
                value += c;
            }
        }
        
        if (!match('"')) {
            return unexpected<json_error>({json_error_code::invalid_string, "Unterminated string", line_, column_});
        }
        
        return json_value{std::move(value)};
    }
    
    json_result<json_value> parse_array() {
        if (!match('[')) {
            return unexpected<json_error>({json_error_code::invalid_syntax, "Expected '['", line_, column_});
        }
        
        ++depth_;
        json_array array;
        
        skip_whitespace();
        
        if (match(']')) {
            --depth_;
            return json_value{std::move(array)};
        }
        
        while (true) {
            auto element = parse_value();
            if (!element) {
                --depth_;
                return element;
            }
            
            array.emplace_back(std::move(*element));
            
            skip_whitespace();
            
            if (match(']')) {
                break;
            } else if (match(',')) {
                skip_whitespace();
                continue;
            } else {
                --depth_;
                return unexpected<json_error>({json_error_code::invalid_syntax, "Expected ',' or ']' in array", line_, column_});
            }
        }
        
        --depth_;
        return json_value{std::move(array)};
    }
    
    json_result<json_value> parse_object() {
        if (!match('{')) {
            return unexpected<json_error>({json_error_code::invalid_syntax, "Expected '{'", line_, column_});
        }
        
        ++depth_;
        json_object object;
        
        skip_whitespace();
        
        if (match('}')) {
            --depth_;
            return json_value{std::move(object)};
        }
        
        while (true) {
            skip_whitespace();
            
            if (peek() != '"') {
                --depth_;
                return unexpected<json_error>({json_error_code::invalid_syntax, "Expected string key in object", line_, column_});
            }
            
            auto key_result = parse_string();
            if (!key_result) {
                --depth_;
                return unexpected<json_error>(key_result.error());
            }
            
            std::string key = key_result->as_string();
            
            skip_whitespace();
            
            if (!match(':')) {
                --depth_;
                return unexpected<json_error>({json_error_code::invalid_syntax, "Expected ':' after object key", line_, column_});
            }
            
            auto value_result = parse_value();
            if (!value_result) {
                --depth_;
                return unexpected<json_error>(value_result.error());
            }
            
            object[std::move(key)] = std::move(*value_result);
            
            skip_whitespace();
            
            if (match('}')) {
                break;
            } else if (match(',')) {
                skip_whitespace();
                continue;
            } else {
                --depth_;
                return unexpected<json_error>({json_error_code::invalid_syntax, "Expected ',' or '}' in object", line_, column_});
            }
        }
        
        --depth_;
        return json_value{std::move(object)};
    }
    
    void skip_whitespace() {
        const char* new_pos = skip_whitespace_simd(current_, end_ - current_);
        
        while (current_ < new_pos) {
            if (*current_ == '\n') {
                ++line_;
                column_ = 1;
            } else {
                ++column_;
            }
            ++current_;
        }
    }
    
    char peek() const noexcept {
        return is_at_end() ? '\0' : *current_;
    }
    
    char advance() noexcept {
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
    
    bool match(char expected) noexcept {
        if (peek() == expected) {
            advance();
            return true;
        }
        return false;
    }
    
    bool is_at_end() const noexcept {
        return current_ >= end_;
    }
};

// Serialization Implementation
// ============================================================================

inline void json_value::serialize_to_buffer(std::string& buffer) const {
    std::visit([&buffer](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        
        if constexpr (std::is_same_v<T, json_null>) {
            buffer += "null";
        } else if constexpr (std::is_same_v<T, bool>) {
            buffer += v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, double>) {
            // Fast double-to-string conversion
#if __has_include(<charconv>) && __cpp_lib_to_chars >= 201611L
            thread_local std::array<char, 32> num_buffer;
            auto [ptr, ec] = std::to_chars(num_buffer.data(), 
                                          num_buffer.data() + num_buffer.size(), v);
            if (ec == std::errc{}) {
                buffer.append(num_buffer.data(), ptr);
            } else {
                buffer += std::to_string(v);
            }
#else
            buffer += std::to_string(v);
#endif
        } else if constexpr (std::is_same_v<T, json_string>) {
            serialize_string_to_buffer(buffer, v);
        } else if constexpr (std::is_same_v<T, json_array>) {
            buffer += '[';
            for (std::size_t i = 0; i < v.size(); ++i) {
                if (i > 0) buffer += ',';
                v[i].serialize_to_buffer(buffer);
            }
            buffer += ']';
        } else if constexpr (std::is_same_v<T, json_object>) {
            buffer += '{';
            bool first = true;
            for (const auto& [key, value] : v) {
                if (!first) buffer += ',';
                first = false;
                serialize_string_to_buffer(buffer, key);
                buffer += ':';
                value.serialize_to_buffer(buffer);
            }
            buffer += '}';
        }
    }, data_);
}

inline void json_value::serialize_string_to_buffer(std::string& buffer, const std::string& str) const {
    buffer += '"';
    
    for (char c : str) {
        switch (c) {
            case '"':  buffer += "\\\""; break;
            case '\\': buffer += "\\\\"; break;
            case '\b': buffer += "\\b"; break;
            case '\f': buffer += "\\f"; break;
            case '\n': buffer += "\\n"; break;
            case '\r': buffer += "\\r"; break;
            case '\t': buffer += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    std::ostringstream oss;
                    oss << "\\u" << std::hex << std::setfill('0') << std::setw(4) 
                        << static_cast<int>(static_cast<unsigned char>(c));
                    buffer += oss.str();
                } else {
                    buffer += c;
                }
                break;
        }
    }
    
    buffer += '"';
}

inline std::string json_value::to_string() const {
    std::string buffer;
    buffer.reserve(1024);
    serialize_to_buffer(buffer);
    return buffer;
}

inline std::string json_value::to_pretty_string(int indent) const {
    std::string buffer;
    buffer.reserve(2048);
    serialize_pretty_to_buffer(buffer, indent, 0);
    return buffer;
}

inline void json_value::serialize_pretty_to_buffer(std::string& buffer, int indent_size, int current_indent) const {
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        
        if constexpr (std::is_same_v<T, json_array>) {
            if (v.empty()) {
                buffer += "[]";
                return;
            }
            
            buffer += "[\n";
            current_indent += indent_size;
            
            for (std::size_t i = 0; i < v.size(); ++i) {
                buffer += std::string(current_indent, ' ');
                v[i].serialize_pretty_to_buffer(buffer, indent_size, current_indent);
                
                if (i < v.size() - 1) {
                    buffer += ',';
                }
                buffer += '\n';
            }
            
            current_indent -= indent_size;
            buffer += std::string(current_indent, ' ') + ']';
        } else if constexpr (std::is_same_v<T, json_object>) {
            if (v.empty()) {
                buffer += "{}";
                return;
            }
            
            buffer += "{\n";
            current_indent += indent_size;
            
            auto it = v.begin();
            while (it != v.end()) {
                buffer += std::string(current_indent, ' ');
                serialize_string_to_buffer(buffer, it->first);
                buffer += ": ";
                it->second.serialize_pretty_to_buffer(buffer, indent_size, current_indent);
                
                ++it;
                if (it != v.end()) {
                    buffer += ',';
                }
                buffer += '\n';
            }
            
            current_indent -= indent_size;
            buffer += std::string(current_indent, ' ') + '}';
        } else {
            serialize_to_buffer(buffer);
        }
    }, data_);
}

// Convenience Functions
// ============================================================================

inline json_result<json_value> parse(std::string_view input) {
    parser p(input);
    return p.parse();
}

inline json_value object() {
    return json_value{json_object{}};
}

inline json_value array() {
    return json_value{json_array{}};
}

inline json_value null() {
    return json_value{};
}

inline std::string stringify(const json_value& value) {
    return value.to_string();
}

inline std::string prettify(const json_value& value, int indent = 2) {
    return value.to_pretty_string(indent);
}

// JSON Builder Pattern for C++17
// ============================================================================

class json_builder {
private:
    json_value value_;

public:
    json_builder() : value_(json_object{}) {}
    explicit json_builder(json_value initial) : value_(std::move(initial)) {}
    
    // Object building
    json_builder& add(const std::string& key, json_value value) {
        if (!value_.is_object()) {
            value_ = json_object{};
        }
        value_[key] = std::move(value);
        return *this;
    }
    
    template<typename T>
    json_builder& add(const std::string& key, T&& value) {
        return add(key, json_value{std::forward<T>(value)});
    }
    
    // Array building
    json_builder& append(json_value value) {
        if (!value_.is_array()) {
            value_ = json_array{};
        }
        value_.push_back(std::move(value));
        return *this;
    }
    
    template<typename T>
    json_builder& append(T&& value) {
        return append(json_value{std::forward<T>(value)});
    }
    
    // Finalization
    json_value build() && {
        return std::move(value_);
    }
    
    const json_value& build() const & {
        return value_;
    }
};

// Factory functions for builders
inline json_builder make_object() {
    return json_builder{json_object{}};
}

inline json_builder make_array() {
    return json_builder{json_array{}};
}

// JSON Literals for C++17
// ============================================================================

namespace literals {
    inline json_result<json_value> operator""_json(const char* str, std::size_t len) {
        return parse(std::string_view{str, len});
    }
}

} // namespace fastjson17

#endif // FASTJSON17_HPP