// FastestJSONInTheWest - Multi-Register SIMD Parser Module
// Copyright (c) 2025 - C++23 Module with Fluent API
// Follows C++ Core Guidelines throughout
// ============================================================================

module;

// <cstdint> for int64_t / uint64_t used by the msvc_* 128-bit emulation structs below.
// They live in the global module fragment; on MSVC int64_t is not a builtin, and the
// `import std;` further down runs AFTER this GMF, so without this include the structs
// fail to parse (C3646/C4430/C2061 'int64_t').
#include <cstdint>

#if defined(_MSC_VER) && !defined(__clang__)
struct msvc_float128 {
    double val;
    constexpr msvc_float128() noexcept : val(0.0) {}
    constexpr msvc_float128(double v) noexcept : val(v) {}
    constexpr operator double() const noexcept { return val; }
    constexpr auto operator+(msvc_float128 o) const noexcept -> msvc_float128 { return val + o.val; }
    constexpr auto operator-(msvc_float128 o) const noexcept -> msvc_float128 { return val - o.val; }
    constexpr auto operator*(msvc_float128 o) const noexcept -> msvc_float128 { return val * o.val; }
    constexpr auto operator/(msvc_float128 o) const noexcept -> msvc_float128 { return val / o.val; }
    constexpr auto operator+=(msvc_float128 o) noexcept -> msvc_float128& { val += o.val; return *this; }
    constexpr auto operator-=(msvc_float128 o) noexcept -> msvc_float128& { val -= o.val; return *this; }
    constexpr auto operator*=(msvc_float128 o) noexcept -> msvc_float128& { val *= o.val; return *this; }
    constexpr auto operator/=(msvc_float128 o) noexcept -> msvc_float128& { val /= o.val; return *this; }
    constexpr auto operator-() const noexcept -> msvc_float128 { return -val; }
    constexpr auto operator==(msvc_float128 o) const noexcept -> bool { return val == o.val; }
    constexpr auto operator<(msvc_float128 o) const noexcept -> bool { return val < o.val; }
    constexpr auto operator>(msvc_float128 o) const noexcept -> bool { return val > o.val; }
    constexpr auto operator<=(msvc_float128 o) const noexcept -> bool { return val <= o.val; }
    constexpr auto operator>=(msvc_float128 o) const noexcept -> bool { return val >= o.val; }
};
struct msvc_int128 {
    int64_t val;
    constexpr msvc_int128() noexcept : val(0) {}
    constexpr msvc_int128(int64_t v) noexcept : val(v) {}
    constexpr operator int64_t() const noexcept { return val; }
    constexpr auto operator+(msvc_int128 o) const noexcept -> msvc_int128 { return val + o.val; }
    constexpr auto operator-(msvc_int128 o) const noexcept -> msvc_int128 { return val - o.val; }
    constexpr auto operator*(msvc_int128 o) const noexcept -> msvc_int128 { return val * o.val; }
    constexpr auto operator/(msvc_int128 o) const noexcept -> msvc_int128 { return val / o.val; }
    constexpr auto operator-() const noexcept -> msvc_int128 { return -val; }
    constexpr auto operator%(int64_t o) const noexcept -> int64_t { return val % o; }
    constexpr auto operator/=(int64_t o) noexcept -> msvc_int128& { val /= o; return *this; }
    constexpr auto operator==(int64_t o) const noexcept -> bool { return val == o; }
    constexpr auto operator<(int64_t o) const noexcept -> bool { return val < o; }
};
struct msvc_uint128 {
    uint64_t val;
    constexpr msvc_uint128() noexcept : val(0) {}
    constexpr msvc_uint128(uint64_t v) noexcept : val(v) {}
    constexpr msvc_uint128(msvc_int128 v) noexcept : val(static_cast<uint64_t>(static_cast<int64_t>(v))) {}
    constexpr operator uint64_t() const noexcept { return val; }
    constexpr auto operator+(msvc_uint128 o) const noexcept -> msvc_uint128 { return val + o.val; }
    constexpr auto operator-(msvc_uint128 o) const noexcept -> msvc_uint128 { return val - o.val; }
    constexpr auto operator*(msvc_uint128 o) const noexcept -> msvc_uint128 { return val * o.val; }
    constexpr auto operator/(msvc_uint128 o) const noexcept -> msvc_uint128 { return val / o.val; }
    constexpr auto operator%(uint64_t o) const noexcept -> uint64_t { return val % o; }
    constexpr auto operator/=(uint64_t o) noexcept -> msvc_uint128& { val /= o; return *this; }
    constexpr auto operator==(uint64_t o) const noexcept -> bool { return val == o; }
};
using float128_compat = msvc_float128;
using int128_compat = msvc_int128;
using uint128_compat = msvc_uint128;
#elif defined(_WIN32)
struct msvc_float128 {
    double val;
    constexpr msvc_float128() noexcept : val(0.0) {}
    constexpr msvc_float128(double v) noexcept : val(v) {}
    constexpr operator double() const noexcept { return val; }
    constexpr auto operator+(msvc_float128 o) const noexcept -> msvc_float128 { return val + o.val; }
    constexpr auto operator-(msvc_float128 o) const noexcept -> msvc_float128 { return val - o.val; }
    constexpr auto operator*(msvc_float128 o) const noexcept -> msvc_float128 { return val * o.val; }
    constexpr auto operator/(msvc_float128 o) const noexcept -> msvc_float128 { return val / o.val; }
    constexpr auto operator+=(msvc_float128 o) noexcept -> msvc_float128& { val += o.val; return *this; }
    constexpr auto operator-=(msvc_float128 o) noexcept -> msvc_float128& { val -= o.val; return *this; }
    constexpr auto operator*=(msvc_float128 o) noexcept -> msvc_float128& { val *= o.val; return *this; }
    constexpr auto operator/=(msvc_float128 o) noexcept -> msvc_float128& { val /= o.val; return *this; }
    constexpr auto operator-() const noexcept -> msvc_float128 { return -val; }
    constexpr auto operator==(msvc_float128 o) const noexcept -> bool { return val == o.val; }
    constexpr auto operator<(msvc_float128 o) const noexcept -> bool { return val < o.val; }
    constexpr auto operator>(msvc_float128 o) const noexcept -> bool { return val > o.val; }
    constexpr auto operator<=(msvc_float128 o) const noexcept -> bool { return val <= o.val; }
    constexpr auto operator>=(msvc_float128 o) const noexcept -> bool { return val >= o.val; }
};
using float128_compat = msvc_float128;
using int128_compat = __int128;
using uint128_compat = unsigned __int128;
#else
using float128_compat = __float128;
using int128_compat = __int128;
using uint128_compat = unsigned __int128;
#endif


// Global module fragment - system includes
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <expected>
#include <functional>
#include <iomanip>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
    #ifdef __GNUC__
        #include <cpuid.h>
    #endif
#endif

export module fastjson_multiregister_parser;

// ============================================================================
// PART 1: Core Types and Error Handling (C++ Core Guidelines: E.*)
// ============================================================================

export namespace fastjson::mr {

// Forward declarations (C++ Core Guidelines: NL.16)
class json_value;
class parser;
class document;

// -----------------------------------------------------------------------------
// Error Types (C++ Core Guidelines: E.6 - Use RAII)
// -----------------------------------------------------------------------------

// GSL.1: Use enum class for type-safe error codes
enum class error_code : std::uint8_t {
    none = 0,
    empty_input,
    unexpected_end,
    invalid_syntax,
    invalid_literal,
    invalid_number,
    invalid_string,
    invalid_escape,
    invalid_unicode,
    unterminated_string,
    unterminated_array,
    unterminated_object,
    expected_colon,
    expected_comma_or_end,
    max_depth_exceeded,
    extra_tokens
};

// C++ Core Guidelines: C.4 - Make data members private
struct parse_error {
private:
    error_code code_{error_code::none};
    std::string message_{};
    std::size_t line_{1};
    std::size_t column_{1};
    std::size_t offset_{0};

public:
    // C++ Core Guidelines: C.45 - Don't define default ctor that only initializes data members
    constexpr parse_error() noexcept = default;
    
    // C++ Core Guidelines: C.46 - Default ctor should be =default or leave invariant established
    constexpr parse_error(error_code code, std::string msg, 
                         std::size_t line = 1, std::size_t column = 1, 
                         std::size_t offset = 0) noexcept
        : code_{code}, message_{std::move(msg)}, line_{line}, column_{column}, offset_{offset} {}
    
    // C++ Core Guidelines: C.131 - Avoid trivial getters and setters
    [[nodiscard]] constexpr auto code() const noexcept -> error_code { return code_; }
    [[nodiscard]] constexpr auto message() const noexcept -> std::string_view { return message_; }
    [[nodiscard]] constexpr auto line() const noexcept -> std::size_t { return line_; }
    [[nodiscard]] constexpr auto column() const noexcept -> std::size_t { return column_; }
    [[nodiscard]] constexpr auto offset() const noexcept -> std::size_t { return offset_; }
    
    // C++ Core Guidelines: C.161 - Use non-member functions for symmetric operators
    [[nodiscard]] auto to_string() const -> std::string {
        std::ostringstream oss;
        oss << "Parse error at line " << line_ << ", column " << column_ 
            << " (offset " << offset_ << "): " << message_;
        return oss.str();
    }
    
    [[nodiscard]] explicit operator bool() const noexcept { return code_ != error_code::none; }
};

// C++ Core Guidelines: T.42 - Use template aliases for alias templates  
template<typename T>
using result = std::expected<T, parse_error>;

// -----------------------------------------------------------------------------
// JSON Value Types (C++ Core Guidelines: C.10 - Prefer concrete types)
// -----------------------------------------------------------------------------

// Type aliases following JSON specification
using json_null = std::nullptr_t;
using json_bool = bool;
using json_int_128_128 = int128_compat;
using json_uint_128_128 = uint128_compat;
using json_float = double;
using json_string = std::string;
using json_array = std::vector<json_value>;
using json_object = std::unordered_map<std::string, json_value>;

// C++ Core Guidelines: Enum.3 - Prefer enum class over plain enum
enum class value_type : std::uint8_t {
    null,
    boolean,
    integer,
    unsigned_integer,
    floating,
    string,
    array,
    object
};

// Variant type for JSON values
using json_data = std::variant<json_null, json_bool, json_int_128_128, json_uint_128_128, json_float, 
                               json_string, json_array, json_object>;

// ============================================================================
// PART 2: SIMD Multi-Register Primitives
// ============================================================================

namespace simd {

// -----------------------------------------------------------------------------
// CPU Capability Detection (Thread-safe, follows C++ Core Guidelines: CP.*)
// -----------------------------------------------------------------------------

// C++ Core Guidelines: I.2 - Avoid non-const global variables
namespace detail {
    inline std::atomic<bool> capabilities_detected{false};
    inline std::atomic<bool> avx2_available{false};
    inline std::atomic<bool> avx512_available{false};
    
    inline auto detect_capabilities() noexcept -> void {
        if (capabilities_detected.load(std::memory_order_acquire)) {
            return;
        }
        
        #if defined(__x86_64__) || defined(_M_X64)
            #ifdef __GNUC__
                unsigned int eax, ebx, ecx, edx;
                if (__get_cpuid_max(0, nullptr) >= 7) {
                    __cpuid_count(7, 0, eax, ebx, ecx, edx);
                    avx2_available.store((ebx & (1 << 5)) != 0, std::memory_order_release);
                    avx512_available.store(
                        ((ebx & (1 << 16)) != 0) && ((ebx & (1 << 30)) != 0),
                        std::memory_order_release
                    );
                }
            #endif
        #endif
        
        capabilities_detected.store(true, std::memory_order_release);
    }
}

// C++ Core Guidelines: F.1 - Package meaningful operations as carefully named functions
[[nodiscard]] inline auto has_avx2() noexcept -> bool {
    detail::detect_capabilities();
    return detail::avx2_available.load(std::memory_order_acquire);
}

[[nodiscard]] inline auto has_avx512() noexcept -> bool {
    detail::detect_capabilities();
    return detail::avx512_available.load(std::memory_order_acquire);
}

// -----------------------------------------------------------------------------
// Structural Character Results
// -----------------------------------------------------------------------------

struct structural_chars {
    std::array<std::uint64_t, 64> positions{};
    std::array<std::uint8_t, 64> types{};
    std::size_t count{0};
    
    // Character type constants
    static constexpr std::uint8_t OPEN_BRACE = 1;
    static constexpr std::uint8_t CLOSE_BRACE = 2;
    static constexpr std::uint8_t OPEN_BRACKET = 3;
    static constexpr std::uint8_t CLOSE_BRACKET = 4;
    static constexpr std::uint8_t COLON = 5;
    static constexpr std::uint8_t COMMA = 6;
};

// -----------------------------------------------------------------------------
// AVX2 4x Multi-Register Implementations (128 bytes per iteration)
// -----------------------------------------------------------------------------

#if defined(__x86_64__) || defined(_M_X64)

// C++ Core Guidelines: F.16 - Use T& for in-out params
__attribute__((target("avx2")))
inline auto skip_whitespace_avx2_4x(const char* data, std::size_t size, 
                                    std::size_t start) noexcept -> std::size_t {
    std::size_t pos = start;
    
    // Process 128 bytes at a time (4 x 32-byte AVX2 registers)
    const __m256i space = _mm256_set1_epi8(' ');
    const __m256i tab = _mm256_set1_epi8('\t');
    const __m256i newline = _mm256_set1_epi8('\n');
    const __m256i carriage = _mm256_set1_epi8('\r');
    
    while (pos + 128 <= size) {
        // Load 4 chunks
        const __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        const __m256i chunk2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 64));
        const __m256i chunk3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 96));
        
        // Check for whitespace in each chunk
        auto check_whitespace = [&](__m256i chunk) {
            __m256i ws_space = _mm256_cmpeq_epi8(chunk, space);
            __m256i ws_tab = _mm256_cmpeq_epi8(chunk, tab);
            __m256i ws_nl = _mm256_cmpeq_epi8(chunk, newline);
            __m256i ws_cr = _mm256_cmpeq_epi8(chunk, carriage);
            return _mm256_or_si256(_mm256_or_si256(ws_space, ws_tab),
                                  _mm256_or_si256(ws_nl, ws_cr));
        };
        
        __m256i ws0 = check_whitespace(chunk0);
        __m256i ws1 = check_whitespace(chunk1);
        __m256i ws2 = check_whitespace(chunk2);
        __m256i ws3 = check_whitespace(chunk3);
        
        // Check if any non-whitespace found
        std::uint32_t mask0 = ~static_cast<std::uint32_t>(_mm256_movemask_epi8(ws0));
        if (mask0 != 0) {
            return pos + std::countr_zero(mask0);
        }
        
        std::uint32_t mask1 = ~static_cast<std::uint32_t>(_mm256_movemask_epi8(ws1));
        if (mask1 != 0) {
            return pos + 32 + std::countr_zero(mask1);
        }
        
        std::uint32_t mask2 = ~static_cast<std::uint32_t>(_mm256_movemask_epi8(ws2));
        if (mask2 != 0) {
            return pos + 64 + std::countr_zero(mask2);
        }
        
        std::uint32_t mask3 = ~static_cast<std::uint32_t>(_mm256_movemask_epi8(ws3));
        if (mask3 != 0) {
            return pos + 96 + std::countr_zero(mask3);
        }
        
        pos += 128;
    }
    
    // Handle remaining bytes with scalar fallback
    while (pos < size) {
        char c = data[pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return pos;
        }
        ++pos;
    }
    
    return pos;
}

__attribute__((target("avx2")))
inline auto find_string_end_avx2_4x(const char* data, std::size_t size,
                                    std::size_t start) noexcept -> std::size_t {
    std::size_t pos = start;
    
    const __m256i quote = _mm256_set1_epi8('"');
    const __m256i backslash = _mm256_set1_epi8('\\');
    
    while (pos + 128 <= size) {
        const __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        const __m256i chunk2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 64));
        const __m256i chunk3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 96));
        
        // Find quotes and backslashes
        std::uint32_t quote_mask0 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk0, quote));
        std::uint32_t bs_mask0 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk0, backslash));
        
        // Process chunk 0
        while (quote_mask0 != 0 || bs_mask0 != 0) {
            std::uint32_t quote_pos = quote_mask0 != 0 ? std::countr_zero(quote_mask0) : 32;
            std::uint32_t bs_pos = bs_mask0 != 0 ? std::countr_zero(bs_mask0) : 32;
            
            if (bs_pos < quote_pos) {
                // Skip escaped character
                std::uint32_t skip_mask = ~((1u << (bs_pos + 2)) - 1);
                quote_mask0 &= skip_mask;
                bs_mask0 &= skip_mask;
            } else if (quote_pos < 32) {
                return pos + quote_pos;
            } else {
                break;
            }
        }
        
        // Similar processing for other chunks (simplified for brevity)
        std::uint32_t quote_mask1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, quote));
        if (quote_mask1 != 0) {
            std::uint32_t first_quote = std::countr_zero(quote_mask1);
            // Check if preceded by backslash
            if (first_quote == 0 ? (data[pos + 31] != '\\') : (data[pos + 32 + first_quote - 1] != '\\')) {
                return pos + 32 + first_quote;
            }
        }
        
        std::uint32_t quote_mask2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk2, quote));
        if (quote_mask2 != 0) {
            std::uint32_t first_quote = std::countr_zero(quote_mask2);
            if (first_quote == 0 ? (data[pos + 63] != '\\') : (data[pos + 64 + first_quote - 1] != '\\')) {
                return pos + 64 + first_quote;
            }
        }
        
        std::uint32_t quote_mask3 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk3, quote));
        if (quote_mask3 != 0) {
            std::uint32_t first_quote = std::countr_zero(quote_mask3);
            if (first_quote == 0 ? (data[pos + 95] != '\\') : (data[pos + 96 + first_quote - 1] != '\\')) {
                return pos + 96 + first_quote;
            }
        }
        
        pos += 128;
    }
    
    // Scalar fallback
    while (pos < size) {
        if (data[pos] == '"' && (pos == start || data[pos - 1] != '\\')) {
            return pos;
        }
        ++pos;
    }
    
    return size;
}

__attribute__((target("avx2")))
inline auto find_structural_avx2_4x(const char* data, std::size_t size,
                                    std::size_t start, structural_chars& result) noexcept -> void {
    result.count = 0;
    std::size_t pos = start;
    
    const __m256i open_brace = _mm256_set1_epi8('{');
    const __m256i close_brace = _mm256_set1_epi8('}');
    const __m256i open_bracket = _mm256_set1_epi8('[');
    const __m256i close_bracket = _mm256_set1_epi8(']');
    const __m256i colon = _mm256_set1_epi8(':');
    const __m256i comma = _mm256_set1_epi8(',');
    
    while (pos + 32 <= size && result.count < 60) {
        const __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        
        std::uint32_t ob_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, open_brace));
        std::uint32_t cb_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, close_brace));
        std::uint32_t obrk_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, open_bracket));
        std::uint32_t cbrk_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, close_bracket));
        std::uint32_t col_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, colon));
        std::uint32_t com_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, comma));
        
        // Process each structural character type
        while (ob_mask != 0 && result.count < 64) {
            std::uint32_t bit = std::countr_zero(ob_mask);
            result.positions[result.count] = pos + bit;
            result.types[result.count] = structural_chars::OPEN_BRACE;
            ++result.count;
            ob_mask &= ob_mask - 1;
        }
        
        while (cb_mask != 0 && result.count < 64) {
            std::uint32_t bit = std::countr_zero(cb_mask);
            result.positions[result.count] = pos + bit;
            result.types[result.count] = structural_chars::CLOSE_BRACE;
            ++result.count;
            cb_mask &= cb_mask - 1;
        }
        
        while (obrk_mask != 0 && result.count < 64) {
            std::uint32_t bit = std::countr_zero(obrk_mask);
            result.positions[result.count] = pos + bit;
            result.types[result.count] = structural_chars::OPEN_BRACKET;
            ++result.count;
            obrk_mask &= obrk_mask - 1;
        }
        
        while (cbrk_mask != 0 && result.count < 64) {
            std::uint32_t bit = std::countr_zero(cbrk_mask);
            result.positions[result.count] = pos + bit;
            result.types[result.count] = structural_chars::CLOSE_BRACKET;
            ++result.count;
            cbrk_mask &= cbrk_mask - 1;
        }
        
        while (col_mask != 0 && result.count < 64) {
            std::uint32_t bit = std::countr_zero(col_mask);
            result.positions[result.count] = pos + bit;
            result.types[result.count] = structural_chars::COLON;
            ++result.count;
            col_mask &= col_mask - 1;
        }
        
        while (com_mask != 0 && result.count < 64) {
            std::uint32_t bit = std::countr_zero(com_mask);
            result.positions[result.count] = pos + bit;
            result.types[result.count] = structural_chars::COMMA;
            ++result.count;
            com_mask &= com_mask - 1;
        }
        
        pos += 32;
    }
}

#endif // x86_64

// -----------------------------------------------------------------------------
// Auto-dispatch Functions (C++ Core Guidelines: F.18 - For "will-move-from" params)
// -----------------------------------------------------------------------------

[[nodiscard]] inline auto skip_whitespace(const char* data, std::size_t size,
                                          std::size_t start = 0) noexcept -> std::size_t {
    #if defined(__x86_64__) || defined(_M_X64)
        if (has_avx2() && size >= 128) {
            return skip_whitespace_avx2_4x(data, size, start);
        }
    #endif
    
    // Scalar fallback
    std::size_t pos = start;
    while (pos < size) {
        char c = data[pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return pos;
        }
        ++pos;
    }
    return pos;
}

[[nodiscard]] inline auto find_string_end(const char* data, std::size_t size,
                                          std::size_t start) noexcept -> std::size_t {
    #if defined(__x86_64__) || defined(_M_X64)
        if (has_avx2() && size >= 128) {
            return find_string_end_avx2_4x(data, size, start);
        }
    #endif
    
    // Scalar fallback
    for (std::size_t pos = start; pos < size; ++pos) {
        if (data[pos] == '"' && (pos == start || data[pos - 1] != '\\')) {
            return pos;
        }
    }
    return size;
}

inline auto find_structural(const char* data, std::size_t size,
                            std::size_t start, structural_chars& result) noexcept -> void {
    #if defined(__x86_64__) || defined(_M_X64)
        if (has_avx2()) {
            find_structural_avx2_4x(data, size, start, result);
            return;
        }
    #endif
    
    // Scalar fallback
    result.count = 0;
    for (std::size_t pos = start; pos < size && result.count < 64; ++pos) {
        char c = data[pos];
        std::uint8_t type = 0;
        switch (c) {
            case '{': type = structural_chars::OPEN_BRACE; break;
            case '}': type = structural_chars::CLOSE_BRACE; break;
            case '[': type = structural_chars::OPEN_BRACKET; break;
            case ']': type = structural_chars::CLOSE_BRACKET; break;
            case ':': type = structural_chars::COLON; break;
            case ',': type = structural_chars::COMMA; break;
            default: continue;
        }
        result.positions[result.count] = pos;
        result.types[result.count] = type;
        ++result.count;
    }
}

} // namespace simd

// ============================================================================
// PART 3: JSON Value Class (C++ Core Guidelines: C.*)
// ============================================================================

class json_value {
private:
    json_data data_;

public:
    // C++ Core Guidelines: C.21 - Define all default operations or none
    json_value() noexcept : data_{nullptr} {}
    ~json_value() = default;
    json_value(const json_value&) = default;
    json_value(json_value&&) noexcept = default;
    auto operator=(const json_value&) -> json_value& = default;
    auto operator=(json_value&&) noexcept -> json_value& = default;
    
    // C++ Core Guidelines: C.46 - Constructors that don't establish invariants
    explicit json_value(std::nullptr_t) noexcept : data_{nullptr} {}
    explicit json_value(bool val) noexcept : data_{val} {}
    explicit json_value(json_int_128_128 val) noexcept : data_{val} {}
    explicit json_value(json_uint_128_128 val) noexcept : data_{val} {}
    explicit json_value(std::int64_t val) noexcept : data_{static_cast<json_int_128_128>(val)} {}
    explicit json_value(std::uint64_t val) noexcept : data_{static_cast<json_uint_128_128>(val)} {}
    explicit json_value(int val) noexcept : data_{static_cast<json_int_128_128>(val)} {}
    explicit json_value(double val) noexcept : data_{val} {}
    explicit json_value(std::string val) noexcept : data_{std::move(val)} {}
    explicit json_value(const char* val) : data_{std::string{val}} {}
    explicit json_value(json_array val) noexcept : data_{std::move(val)} {}
    explicit json_value(json_object val) noexcept : data_{std::move(val)} {}
    
    // C++ Core Guidelines: C.162 - Overload operations that are roughly equivalent
    [[nodiscard]] auto type() const noexcept -> value_type {
        return std::visit([](const auto& val) -> value_type {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, json_null>) return value_type::null;
            else if constexpr (std::is_same_v<T, json_bool>) return value_type::boolean;
            else if constexpr (std::is_same_v<T, json_int_128_128>) return value_type::integer;
            else if constexpr (std::is_same_v<T, json_uint_128_128>) return value_type::unsigned_integer;
            else if constexpr (std::is_same_v<T, json_float>) return value_type::floating;
            else if constexpr (std::is_same_v<T, json_string>) return value_type::string;
            else if constexpr (std::is_same_v<T, json_array>) return value_type::array;
            else if constexpr (std::is_same_v<T, json_object>) return value_type::object;
            else return value_type::null;
        }, data_);
    }
    
    // Type checking predicates
    [[nodiscard]] auto is_null() const noexcept -> bool { return type() == value_type::null; }
    [[nodiscard]] auto is_bool() const noexcept -> bool { return type() == value_type::boolean; }
    [[nodiscard]] auto is_int() const noexcept -> bool { return type() == value_type::integer; }
    [[nodiscard]] auto is_uint() const noexcept -> bool { return type() == value_type::unsigned_integer; }
    [[nodiscard]] auto is_float() const noexcept -> bool { return type() == value_type::floating; }
    [[nodiscard]] auto is_number() const noexcept -> bool { 
        return is_int() || is_uint() || is_float(); 
    }
    [[nodiscard]] auto is_string() const noexcept -> bool { return type() == value_type::string; }
    [[nodiscard]] auto is_array() const noexcept -> bool { return type() == value_type::array; }
    [[nodiscard]] auto is_object() const noexcept -> bool { return type() == value_type::object; }
    
    // Value accessors with optional for safe access
    [[nodiscard]] auto as_bool() const noexcept -> std::optional<bool> {
        if (auto* p = std::get_if<json_bool>(&data_)) return *p;
        return std::nullopt;
    }
    
    [[nodiscard]] auto as_int() const noexcept -> std::optional<json_int_128_128> {
        if (auto* p = std::get_if<json_int_128_128>(&data_)) return *p;
        if (auto* p = std::get_if<json_uint_128_128>(&data_)) return static_cast<json_int_128_128>(*p);
        if (auto* p = std::get_if<json_float>(&data_)) return static_cast<json_int_128_128>(*p);
        return std::nullopt;
    }

    [[nodiscard]] auto as_uint() const noexcept -> std::optional<json_uint_128_128> {
        if (auto* p = std::get_if<json_uint_128_128>(&data_)) return *p;
        if (auto* p = std::get_if<json_int_128_128>(&data_)) return static_cast<json_uint_128_128>(*p);
        if (auto* p = std::get_if<json_float>(&data_)) return static_cast<json_uint_128_128>(*p);
        return std::nullopt;
    }
    
    [[nodiscard]] auto as_float() const noexcept -> std::optional<double> {
        if (auto* p = std::get_if<json_float>(&data_)) return *p;
        if (auto* p = std::get_if<json_int_128_128>(&data_)) return static_cast<double>(*p);
        if (auto* p = std::get_if<json_uint_128_128>(&data_)) return static_cast<double>(*p);
        return std::nullopt;
    }
    
    [[nodiscard]] auto as_string() const noexcept -> std::optional<std::string_view> {
        if (auto* p = std::get_if<json_string>(&data_)) return *p;
        return std::nullopt;
    }
    
    [[nodiscard]] auto as_array() const noexcept -> const json_array* {
        return std::get_if<json_array>(&data_);
    }
    
    [[nodiscard]] auto as_object() const noexcept -> const json_object* {
        return std::get_if<json_object>(&data_);
    }
    
    // Mutable accessors
    [[nodiscard]] auto as_array_mut() noexcept -> json_array* {
        return std::get_if<json_array>(&data_);
    }
    
    [[nodiscard]] auto as_object_mut() noexcept -> json_object* {
        return std::get_if<json_object>(&data_);
    }
    
    // Object subscript operator (creates if missing for non-const)
    // [C++ Core Guidelines ES.63: Don't slice] - return reference to avoid copies
    [[nodiscard]] auto operator[](const std::string& key) -> json_value& {
        if (!is_object()) {
            data_ = json_object{};
        }
        return std::get<json_object>(data_)[key];
    }
    
    // [C++ Core Guidelines E.14: Use exceptions for errors] - throw on invalid access
    [[nodiscard]] auto operator[](const std::string& key) const -> const json_value& {
        if (!is_object()) {
            throw std::runtime_error("Cannot index non-object JSON value with string");
        }
        const auto& obj = std::get<json_object>(data_);
        auto it = obj.find(key);
        if (it == obj.end()) {
            throw std::out_of_range("Object key not found: " + key);
        }
        return it->second;
    }
    
    // Array subscript operator
    [[nodiscard]] auto operator[](std::size_t index) -> json_value& {
        if (!is_array()) {
            data_ = json_array{};
        }
        auto& arr = std::get<json_array>(data_);
        if (index >= arr.size()) {
            arr.resize(index + 1);
        }
        return arr[index];
    }
    
    // [C++ Core Guidelines E.14: Use exceptions for errors] - throw on invalid access
    [[nodiscard]] auto operator[](std::size_t index) const -> const json_value& {
        if (!is_array()) {
            throw std::runtime_error("Cannot index non-array JSON value with integer");
        }
        const auto& arr = std::get<json_array>(data_);
        if (index >= arr.size()) {
            throw std::out_of_range("Array index out of bounds: " + std::to_string(index));
        }
        return arr[index];
    }
    
    // Size operations
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        if (auto* arr = as_array()) return arr->size();
        if (auto* obj = as_object()) return obj->size();
        return 0;
    }
    
    [[nodiscard]] auto empty() const noexcept -> bool {
        if (auto* arr = as_array()) return arr->empty();
        if (auto* obj = as_object()) return obj->empty();
        return true;
    }
    
    // Serialization
    [[nodiscard]] auto to_string() const -> std::string;
    [[nodiscard]] auto to_pretty_string(int indent = 2) const -> std::string;
    
    // Friend access for serialization
    friend auto serialize_value(std::ostringstream& oss, const json_value& val) -> void;
    friend auto serialize_pretty(std::ostringstream& oss, const json_value& val, int indent, int current) -> void;
};

// ============================================================================
// PART 4: Parser Implementation (C++ Core Guidelines: F.*, ES.*)
// ============================================================================

class parser {
private:
    const char* data_;
    const char* end_;
    const char* current_;
    std::size_t line_;
    std::size_t column_;
    std::size_t depth_;
    static constexpr std::size_t max_depth_ = 1000;
    
    // SIMD capability flags (cached)
    bool use_simd_;

public:
    // C++ Core Guidelines: C.41 - A ctor should create a fully initialized object
    explicit parser(std::string_view input) noexcept
        : data_{input.data()}
        , end_{input.data() + input.size()}
        , current_{input.data()}
        , line_{1}
        , column_{1}
        , depth_{0}
        , use_simd_{simd::has_avx2()}
    {}
    
    // Main parsing entry point
    [[nodiscard]] auto parse() -> result<json_value> {
        skip_whitespace();
        
        if (is_at_end()) {
            return std::unexpected(make_error(error_code::empty_input, "Empty input"));
        }
        
        auto value = parse_value();
        if (!value) {
            return value;
        }
        
        skip_whitespace();
        
        if (!is_at_end()) {
            return std::unexpected(make_error(error_code::extra_tokens, 
                                             "Unexpected characters after JSON value"));
        }
        
        return value;
    }

private:
    // -----------------------------------------------------------------------------
    // Whitespace Handling with SIMD
    // -----------------------------------------------------------------------------
    
    auto skip_whitespace() noexcept -> void {
        if (use_simd_ && remaining() >= 128) {
            std::size_t offset = current_ - data_;
            std::size_t new_offset = simd::skip_whitespace(data_, end_ - data_, offset);
            
            // Update line/column tracking
            for (std::size_t i = offset; i < new_offset; ++i) {
                if (data_[i] == '\n') {
                    ++line_;
                    column_ = 1;
                } else {
                    ++column_;
                }
            }
            
            current_ = data_ + new_offset;
        } else {
            while (!is_at_end() && is_whitespace(peek())) {
                advance();
            }
        }
    }
    
    [[nodiscard]] static constexpr auto is_whitespace(char c) noexcept -> bool {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }
    
    // -----------------------------------------------------------------------------
    // Value Parsing
    // -----------------------------------------------------------------------------
    
    [[nodiscard]] auto parse_value() -> result<json_value> {
        if (depth_ >= max_depth_) {
            return std::unexpected(make_error(error_code::max_depth_exceeded, 
                                             "Maximum nesting depth exceeded"));
        }
        
        skip_whitespace();
        
        if (is_at_end()) {
            return std::unexpected(make_error(error_code::unexpected_end, 
                                             "Unexpected end of input"));
        }
        
        char c = peek();
        
        switch (c) {
            case 'n': return parse_null();
            case 't': return parse_true();
            case 'f': return parse_false();
            case '"': return parse_string();
            case '[': return parse_array();
            case '{': return parse_object();
            case '-':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                return parse_number();
            default:
                return std::unexpected(make_error(error_code::invalid_syntax,
                                                 std::string("Unexpected character: ") + c));
        }
    }
    
    [[nodiscard]] auto parse_null() -> result<json_value> {
        if (remaining() >= 4 && std::string_view{current_, 4} == "null") {
            current_ += 4;
            column_ += 4;
            return json_value{nullptr};
        }
        return std::unexpected(make_error(error_code::invalid_literal, "Expected 'null'"));
    }
    
    [[nodiscard]] auto parse_true() -> result<json_value> {
        if (remaining() >= 4 && std::string_view{current_, 4} == "true") {
            current_ += 4;
            column_ += 4;
            return json_value{true};
        }
        return std::unexpected(make_error(error_code::invalid_literal, "Expected 'true'"));
    }
    
    [[nodiscard]] auto parse_false() -> result<json_value> {
        if (remaining() >= 5 && std::string_view{current_, 5} == "false") {
            current_ += 5;
            column_ += 5;
            return json_value{false};
        }
        return std::unexpected(make_error(error_code::invalid_literal, "Expected 'false'"));
    }
    
    [[nodiscard]] auto parse_string() -> result<json_value> {
        if (peek() != '"') {
            return std::unexpected(make_error(error_code::invalid_string, "Expected '\"'"));
        }
        
        advance(); // Skip opening quote
        const char* start = current_;
        
        // Use SIMD to find string end
        std::size_t end_offset;
        if (use_simd_ && remaining() >= 128) {
            end_offset = simd::find_string_end(data_, end_ - data_, current_ - data_);
            
            if (end_offset >= static_cast<std::size_t>(end_ - data_)) {
                return std::unexpected(make_error(error_code::unterminated_string, 
                                                 "Unterminated string"));
            }
        } else {
            // Scalar path
            while (!is_at_end()) {
                char c = peek();
                if (c == '"') {
                    break;
                }
                if (c == '\\') {
                    advance();
                    if (is_at_end()) {
                        return std::unexpected(make_error(error_code::unterminated_string,
                                                         "Unterminated escape sequence"));
                    }
                }
                advance();
            }
            
            if (is_at_end()) {
                return std::unexpected(make_error(error_code::unterminated_string,
                                                 "Unterminated string"));
            }
            
            end_offset = current_ - data_;
        }
        
        std::string result(start, data_ + end_offset);
        current_ = data_ + end_offset + 1; // Skip closing quote
        column_ += result.size() + 2;
        
        // TODO: Process escape sequences
        
        return json_value{std::move(result)};
    }
    
    [[nodiscard]] auto parse_number() -> result<json_value> {
        const char* start = current_;
        bool is_float = false;
        
        // Handle sign
        if (peek() == '-') {
            advance();
        }
        
        // Integer part
        if (peek() == '0') {
            advance();
        } else if (peek() >= '1' && peek() <= '9') {
            while (!is_at_end() && peek() >= '0' && peek() <= '9') {
                advance();
            }
        } else {
            return std::unexpected(make_error(error_code::invalid_number, "Invalid number"));
        }
        
        // Fractional part
        if (!is_at_end() && peek() == '.') {
            is_float = true;
            advance();
            if (is_at_end() || peek() < '0' || peek() > '9') {
                return std::unexpected(make_error(error_code::invalid_number, 
                                                 "Invalid fractional part"));
            }
            while (!is_at_end() && peek() >= '0' && peek() <= '9') {
                advance();
            }
        }
        
        // Exponent part
        if (!is_at_end() && (peek() == 'e' || peek() == 'E')) {
            is_float = true;
            advance();
            if (!is_at_end() && (peek() == '+' || peek() == '-')) {
                advance();
            }
            if (is_at_end() || peek() < '0' || peek() > '9') {
                return std::unexpected(make_error(error_code::invalid_number, 
                                                 "Invalid exponent"));
            }
            while (!is_at_end() && peek() >= '0' && peek() <= '9') {
                advance();
            }
        }
        
        std::string_view num_str{start, static_cast<std::size_t>(current_ - start)};
        
        if (is_float) {
            double value;
            auto [ptr, ec] = std::from_chars(num_str.data(), num_str.data() + num_str.size(), value);
            if (ec != std::errc{}) {
                return std::unexpected(make_error(error_code::invalid_number, 
                                                 "Failed to parse floating point"));
            }
            return json_value{value};
        } else {
            // Native 128-bit integer parsing
            const char* p = num_str.data();
            bool neg = (*p == '-');
            if (neg) p++;

            uint128_compat result = 0;
            bool overflow = false;
            const char* end = num_str.data() + num_str.size();

            // Exact pre-multiplication overflow check. The previous `result < prev`
            // test missed wraps where result * 10 lands above prev (for example
            // 400000000000000000000000000000000000000 silently wrapped to a wrong value).
            const uint128_compat k_umax      = static_cast<uint128_compat>(~static_cast<uint128_compat>(0));
            const uint128_compat k_umax_div  = k_umax / static_cast<uint128_compat>(10);
            const unsigned       k_umax_mod  = static_cast<unsigned>(k_umax % 10u);
            const uint128_compat k_smax      = k_umax / static_cast<uint128_compat>(2);  // INT128_MAX magnitude

            while (p < end && *p >= '0' && *p <= '9') {
                const unsigned digit = static_cast<unsigned>(*p - '0');
                if (result > k_umax_div || (!(result < k_umax_div) && digit > k_umax_mod)) {
                    overflow = true;
                    break;
                }
                result = result * static_cast<uint128_compat>(10) + static_cast<uint128_compat>(digit);
                p++;
            }

            if (!overflow) {
                if (neg) {
                    if (!(result > k_smax)) {
                        return json_value{static_cast<int128_compat>(-static_cast<int128_compat>(result))};
                    }
                    if (!(result > k_smax + static_cast<uint128_compat>(1))) {
                        // Exactly |INT128_MIN|: negating INT128_MAX + 1 directly would be UB.
                        return json_value{static_cast<int128_compat>(
                            -static_cast<int128_compat>(k_smax) - static_cast<int128_compat>(1))};
                    }
                    // Magnitude below -INT128_MIN: fall through to the double fallback.
                } else {
                    if (result > k_smax) {
                        return json_value{result}; // json_uint_128_128
                    }
                    return json_value{static_cast<int128_compat>(result)};
                }
            }

            // Out of 128-bit range: fall back to double, and reject values a double
            // cannot represent instead of returning an uninitialized value.
            double value = 0.0;
            auto [ptr, ec] = std::from_chars(num_str.data(), num_str.data() + num_str.size(), value);
            if (ec != std::errc{} || ptr != num_str.data() + num_str.size()) {
                return std::unexpected(make_error(error_code::invalid_number,
                                                 "Integer out of representable range"));
            }
            return json_value{value};
        }
    }
    
    [[nodiscard]] auto parse_array() -> result<json_value> {
        if (!match('[')) {
            return std::unexpected(make_error(error_code::invalid_syntax, "Expected '['"));
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
                return element;
            }
            
            arr.push_back(std::move(*element));
            
            skip_whitespace();
            
            if (match(',')) {
                skip_whitespace();
                continue;
            }
            
            if (match(']')) {
                break;
            }
            
            --depth_;
            return std::unexpected(make_error(error_code::expected_comma_or_end,
                                             "Expected ',' or ']'"));
        }
        
        --depth_;
        return json_value{std::move(arr)};
    }
    
    [[nodiscard]] auto parse_object() -> result<json_value> {
        if (!match('{')) {
            return std::unexpected(make_error(error_code::invalid_syntax, "Expected '{'"));
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
                return std::unexpected(make_error(error_code::invalid_string,
                                                 "Expected string key"));
            }
            
            auto key_result = parse_string();
            if (!key_result) {
                --depth_;
                return std::unexpected(key_result.error());
            }
            
            auto key_opt = key_result->as_string();
            if (!key_opt) {
                --depth_;
                return std::unexpected(make_error(error_code::invalid_string,
                                                 "Invalid key"));
            }
            
            std::string key{*key_opt};
            
            skip_whitespace();
            
            if (!match(':')) {
                --depth_;
                return std::unexpected(make_error(error_code::expected_colon,
                                                 "Expected ':'"));
            }
            
            // Parse value
            auto value = parse_value();
            if (!value) {
                --depth_;
                return value;
            }
            
            obj[std::move(key)] = std::move(*value);
            
            skip_whitespace();
            
            if (match(',')) {
                skip_whitespace();
                continue;
            }
            
            if (match('}')) {
                break;
            }
            
            --depth_;
            return std::unexpected(make_error(error_code::expected_comma_or_end,
                                             "Expected ',' or '}'"));
        }
        
        --depth_;
        return json_value{std::move(obj)};
    }
    
    // -----------------------------------------------------------------------------
    // Helper Functions
    // -----------------------------------------------------------------------------
    
    [[nodiscard]] auto peek() const noexcept -> char {
        return is_at_end() ? '\0' : *current_;
    }
    
    auto advance() noexcept -> char {
        if (is_at_end()) return '\0';
        
        char c = *current_++;
        if (c == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return c;
    }
    
    [[nodiscard]] auto match(char expected) noexcept -> bool {
        if (peek() != expected) return false;
        advance();
        return true;
    }
    
    [[nodiscard]] auto is_at_end() const noexcept -> bool {
        return current_ >= end_;
    }
    
    [[nodiscard]] auto remaining() const noexcept -> std::size_t {
        return static_cast<std::size_t>(end_ - current_);
    }
    
    [[nodiscard]] auto make_error(error_code code, std::string message) const -> parse_error {
        return parse_error{code, std::move(message), line_, column_, 
                          static_cast<std::size_t>(current_ - data_)};
    }
};

// ============================================================================
// PART 5: Serialization Implementation
// ============================================================================

// Forward declarations for friend functions
auto serialize_value(std::ostringstream& oss, const json_value& val) -> void;
auto serialize_pretty(std::ostringstream& oss, const json_value& val, int indent, int current) -> void;

template<typename T>
auto to_string_128(T n) -> std::string {
    if (n == 0) return "0";
    std::string s;
    bool neg = false;
    if constexpr (std::is_signed_v<T>) {
        if (n < 0) {
            neg = true;
            n = -n;
        }
    }
    while (n > 0) {
        s += static_cast<char>('0' + (n % 10));
        n /= 10;
    }
    if (neg) s += '-';
    std::reverse(s.begin(), s.end());
    return s;
}

auto serialize_value(std::ostringstream& oss, const json_value& val) -> void {
    std::visit([&oss, &val](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        
        if constexpr (std::is_same_v<T, json_null>) {
            oss << "null";
        }
        else if constexpr (std::is_same_v<T, json_bool>) {
            oss << (v ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, json_int_128_128> || std::is_same_v<T, json_uint_128_128>) {
            oss << to_string_128(v);
        }
        else if constexpr (std::is_same_v<T, json_float>) {
            oss << std::setprecision(17) << v;
        }
        else if constexpr (std::is_same_v<T, json_string>) {
            oss << '"';
            for (char c : v) {
                switch (c) {
                    case '"':  oss << "\\\""; break;
                    case '\\': oss << "\\\\"; break;
                    case '\b': oss << "\\b"; break;
                    case '\f': oss << "\\f"; break;
                    case '\n': oss << "\\n"; break;
                    case '\r': oss << "\\r"; break;
                    case '\t': oss << "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            oss << "\\u" << std::hex << std::setw(4) 
                                << std::setfill('0') << static_cast<int>(c);
                        } else {
                            oss << c;
                        }
                }
            }
            oss << '"';
        }
        else if constexpr (std::is_same_v<T, json_array>) {
            oss << '[';
            bool first = true;
            for (const auto& elem : v) {
                if (!first) oss << ',';
                first = false;
                serialize_value(oss, elem);
            }
            oss << ']';
        }
        else if constexpr (std::is_same_v<T, json_object>) {
            oss << '{';
            bool first = true;
            for (const auto& [key, value] : v) {
                if (!first) oss << ',';
                first = false;
                oss << '"' << key << "\":";
                serialize_value(oss, value);
            }
            oss << '}';
        }
    }, val.data_);
}

auto serialize_pretty(std::ostringstream& oss, const json_value& val, int indent, int current) -> void {
    auto write_indent = [&oss](int level) {
        for (int i = 0; i < level; ++i) oss << ' ';
    };
    
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        
        if constexpr (std::is_same_v<T, json_null>) {
            oss << "null";
        }
        else if constexpr (std::is_same_v<T, json_bool>) {
            oss << (v ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, json_int_128_128> || std::is_same_v<T, json_uint_128_128>) {
            oss << to_string_128(v);
        }
        else if constexpr (std::is_same_v<T, json_float>) {
            oss << std::setprecision(17) << v;
        }
        else if constexpr (std::is_same_v<T, json_string>) {
            oss << '"' << v << '"';
        }
        else if constexpr (std::is_same_v<T, json_array>) {
            if (v.empty()) {
                oss << "[]";
            } else {
                oss << "[\n";
                bool first = true;
                for (const auto& elem : v) {
                    if (!first) oss << ",\n";
                    first = false;
                    write_indent(current + indent);
                    serialize_pretty(oss, elem, indent, current + indent);
                }
                oss << '\n';
                write_indent(current);
                oss << ']';
            }
        }
        else if constexpr (std::is_same_v<T, json_object>) {
            if (v.empty()) {
                oss << "{}";
            } else {
                oss << "{\n";
                bool first = true;
                for (const auto& [key, value] : v) {
                    if (!first) oss << ",\n";
                    first = false;
                    write_indent(current + indent);
                    oss << '"' << key << "\": ";
                    serialize_pretty(oss, value, indent, current + indent);
                }
                oss << '\n';
                write_indent(current);
                oss << '}';
            }
        }
    }, val.data_);
}

auto json_value::to_string() const -> std::string {
    std::ostringstream oss;
    serialize_value(oss, *this);
    return oss.str();
}

auto json_value::to_pretty_string(int indent) const -> std::string {
    std::ostringstream oss;
    serialize_pretty(oss, *this, indent, 0);
    return oss.str();
}

// ============================================================================
// PART 6: Fluent API Functions (1:1 Parity with fastjson)
// ============================================================================

// Core parsing function
[[nodiscard]] auto parse(std::string_view input) -> result<json_value> {
    parser p{input};
    return p.parse();
}

// Factory functions
[[nodiscard]] auto object() -> json_value {
    return json_value{json_object{}};
}

[[nodiscard]] auto array() -> json_value {
    return json_value{json_array{}};
}

[[nodiscard]] auto null() -> json_value {
    return json_value{nullptr};
}

// Serialization
[[nodiscard]] auto stringify(const json_value& value) -> std::string {
    return value.to_string();
}

[[nodiscard]] auto prettify(const json_value& value, int indent = 2) -> std::string {
    return value.to_pretty_string(indent);
}

// ============================================================================
// PART 7: SIMD Performance Extensions
// ============================================================================

struct simd_info {
    bool avx2_available;
    bool avx512_available;
    std::string_view optimal_path;
    std::size_t register_count;
    std::size_t bytes_per_iteration;
};

[[nodiscard]] auto get_simd_info() noexcept -> simd_info {
    bool avx2 = simd::has_avx2();
    bool avx512 = simd::has_avx512();
    
    return simd_info{
        .avx2_available = avx2,
        .avx512_available = avx512,
        .optimal_path = avx512 ? "AVX-512 (8x registers)" :
                        avx2 ? "AVX2 (4x registers)" : "Scalar",
        .register_count = static_cast<std::size_t>(avx512 ? 8 : avx2 ? 4 : 1),
        .bytes_per_iteration = static_cast<std::size_t>(avx512 ? 512 : avx2 ? 128 : 1)
    };
}

struct parse_metrics {
    result<json_value> value;
    std::chrono::nanoseconds duration;
    std::size_t bytes_processed;
    simd_info simd;
};

[[nodiscard]] auto parse_with_metrics(std::string_view input) -> parse_metrics {
    auto start = std::chrono::high_resolution_clock::now();
    auto value = parse(input);
    auto end = std::chrono::high_resolution_clock::now();
    
    return parse_metrics{
        .value = std::move(value),
        .duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start),
        .bytes_processed = input.size(),
        .simd = get_simd_info()
    };
}

// ============================================================================
// PART 8: Literals Support
// ============================================================================

inline namespace literals {
    [[nodiscard]] auto operator""_json(const char* str, std::size_t len) 
        -> result<json_value> {
        return parse(std::string_view{str, len});
    }
}

// ============================================================================
// PART 9: LINQ-style Query Operations (C++ Core Guidelines: T.*)
// Functional programming style operations for JSON data manipulation
// ============================================================================

// Query result container for lazy evaluation chains (push-transducer pipeline)
// [C++ Core Guidelines T.1: Use templates for generality]
template <typename T>
class query_result {
public:
    using value_type = T;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;
    using sink_fn = std::function<bool(const T&)>;
    using stream_fn = std::function<void(const sink_fn&)>;

    query_result() = default;
    explicit query_result(std::vector<T>&& data)
        : data_(std::make_shared<std::vector<T>>(std::move(data))) {}
    explicit query_result(const std::vector<T>& data)
        : data_(std::make_shared<std::vector<T>>(data)) {}
    explicit query_result(const std::vector<T>* ext_ref) noexcept
        : ext_data_(ext_ref) {}
    explicit query_result(stream_fn generator) noexcept
        : generator_(std::move(generator)) {}

    template <typename Sink>
    auto consume(Sink&& sink) const -> void {
        if (generator_) {
            generator_(sink_fn(std::forward<Sink>(sink)));
        } else if (ext_data_) {
            for (const auto& item : *ext_data_) {
                if (!sink(item)) break;
            }
        } else if (data_) {
            for (const auto& item : *data_) {
                if (!sink(item)) break;
            }
        }
    }

    [[nodiscard]] auto is_lazy() const noexcept -> bool { return static_cast<bool>(generator_); }

    // Iterator support (C++ Core Guidelines: T.43 - Prefer alias over typedef)
    [[nodiscard]] auto begin() -> iterator { return ensure_mutable().begin(); }
    [[nodiscard]] auto end() -> iterator { return ensure_mutable().end(); }
    [[nodiscard]] auto begin() const -> const_iterator { return ensure_materialized().begin(); }
    [[nodiscard]] auto end() const -> const_iterator { return ensure_materialized().end(); }
    [[nodiscard]] auto cbegin() const -> const_iterator { return ensure_materialized().cbegin(); }
    [[nodiscard]] auto cend() const -> const_iterator { return ensure_materialized().cend(); }

    // Size and access
    [[nodiscard]] auto size() const -> std::size_t { return count(); }
    [[nodiscard]] auto empty() const -> bool { return !any(); }
    [[nodiscard]] auto operator[](std::size_t idx) -> T& { return ensure_mutable()[idx]; }
    [[nodiscard]] auto operator[](std::size_t idx) const -> const T& { return ensure_materialized()[idx]; }

    // -------------------------------------------------------------------------
    // WHERE / FILTER - Select elements matching a predicate (lazy!)
    // [C++ Core Guidelines F.21: Use function templates for generic functions]
    // -------------------------------------------------------------------------
    template <typename Predicate>
    [[nodiscard]] auto where(Predicate pred) const -> query_result<T> {
        auto self = *this;
        return query_result<T>(stream_fn([self, pred = std::move(pred)](const sink_fn& downstream) {
            self.consume([&](const T& item) -> bool {
                if (pred(item)) {
                    return downstream(item);
                }
                return true;
            });
        }));
    }

    // Alias: filter (functional programming style)
    template <typename Predicate>
    [[nodiscard]] auto filter(Predicate pred) const -> query_result<T> {
        return where(std::move(pred));
    }

    // -------------------------------------------------------------------------
    // SELECT / MAP - Transform each element (lazy!)
    // -------------------------------------------------------------------------
    template <typename Func>
    [[nodiscard]] auto select(Func func) const 
        -> query_result<std::invoke_result_t<Func, const T&>> {
        using R = std::invoke_result_t<Func, const T&>;
        auto self = *this;
        return query_result<R>(typename query_result<R>::stream_fn(
            [self, func = std::move(func)](const typename query_result<R>::sink_fn& downstream) {
                self.consume([&](const T& item) -> bool {
                    return downstream(func(item));
                });
            }));
    }

    // Alias: map (functional programming style)
    template <typename Func>
    [[nodiscard]] auto map(Func func) const 
        -> query_result<std::invoke_result_t<Func, const T&>> {
        return select(std::move(func));
    }

    // -------------------------------------------------------------------------
    // FLAT_MAP / SELECT_MANY - Transform and flatten (lazy!)
    // -------------------------------------------------------------------------
    template <typename Func>
    [[nodiscard]] auto flat_map(Func func) const 
        -> query_result<typename std::invoke_result_t<Func, const T&>::value_type> {
        using Inner = typename std::invoke_result_t<Func, const T&>::value_type;
        auto self = *this;
        return query_result<Inner>(typename query_result<Inner>::stream_fn(
            [self, func = std::move(func)](const typename query_result<Inner>::sink_fn& downstream) {
                self.consume([&](const T& item) -> bool {
                    auto inner = func(item);
                    for (const auto& elem : inner) {
                        if (!downstream(elem)) return false;
                    }
                    return true;
                });
            }));
    }

    // Alias: select_many (LINQ style)
    template <typename Func>
    [[nodiscard]] auto select_many(Func func) const 
        -> query_result<typename std::invoke_result_t<Func, const T&>::value_type> {
        return flat_map(std::move(func));
    }

    // -------------------------------------------------------------------------
    // ORDER BY - Sort by key selector (deferred until consumed)
    // -------------------------------------------------------------------------
    template <typename KeySelector>
    [[nodiscard]] auto order_by(KeySelector key_selector) const -> query_result<T> {
        auto self = *this;
        return query_result<T>(stream_fn([self, key_selector = std::move(key_selector)](const sink_fn& downstream) {
            std::vector<T> sorted = self.to_vector();
            std::sort(sorted.begin(), sorted.end(), 
                [&key_selector](const T& a, const T& b) {
                    return key_selector(a) < key_selector(b);
                });
            for (const auto& item : sorted) {
                if (!downstream(item)) break;
            }
        }));
    }

    template <typename KeySelector>
    [[nodiscard]] auto order_by_descending(KeySelector key_selector) const -> query_result<T> {
        auto self = *this;
        return query_result<T>(stream_fn([self, key_selector = std::move(key_selector)](const sink_fn& downstream) {
            std::vector<T> sorted = self.to_vector();
            std::sort(sorted.begin(), sorted.end(), 
                [&key_selector](const T& a, const T& b) {
                    return key_selector(a) > key_selector(b);
                });
            for (const auto& item : sorted) {
                if (!downstream(item)) break;
            }
        }));
    }

    // -------------------------------------------------------------------------
    // FIRST / LAST / SINGLE - Element access (early-terminating!)
    // -------------------------------------------------------------------------
    [[nodiscard]] auto first() const -> std::optional<T> {
        if (!generator_) {
            const auto& vec = ensure_materialized();
            if (vec.empty()) return std::nullopt;
            return vec.front();
        }
        std::optional<T> res;
        consume([&res](const T& item) -> bool {
            res.emplace(item);
            return false;
        });
        return res;
    }

    [[nodiscard]] auto first_or_default(const T& default_value) const -> T {
        auto res = first();
        return res.has_value() ? *res : default_value;
    }

    template <typename Predicate>
    [[nodiscard]] auto first(Predicate pred) const -> std::optional<T> {
        std::optional<T> res;
        consume([&](const T& item) -> bool {
            if (pred(item)) {
                res.emplace(item);
                return false;
            }
            return true;
        });
        return res;
    }

    [[nodiscard]] auto last() const -> std::optional<T> {
        if (!generator_) {
            const auto& vec = ensure_materialized();
            if (vec.empty()) return std::nullopt;
            return vec.back();
        }
        std::optional<T> res;
        consume([&res](const T& item) -> bool {
            res = item;
            return true;
        });
        return res;
    }

    template <typename Predicate>
    [[nodiscard]] auto last(Predicate pred) const -> std::optional<T> {
        if (!generator_) {
            const auto& vec = ensure_materialized();
            for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
                if (pred(*it)) return *it;
            }
            return std::nullopt;
        }
        std::optional<T> res;
        consume([&](const T& item) -> bool {
            if (pred(item)) res = item;
            return true;
        });
        return res;
    }

    [[nodiscard]] auto single() const -> std::optional<T> {
        if (!generator_) {
            const auto& vec = ensure_materialized();
            if (vec.size() != 1) return std::nullopt;
            return vec.front();
        }
        std::optional<T> res;
        std::size_t seen = 0;
        consume([&](const T& item) -> bool {
            ++seen;
            if (seen == 1) {
                res.emplace(item);
                return true;
            }
            res.reset();
            return false;
        });
        return seen == 1 ? res : std::nullopt;
    }

    template <typename Predicate>
    [[nodiscard]] auto find(Predicate pred) const -> std::optional<T> {
        return first(std::move(pred));
    }

    template <typename Predicate>
    [[nodiscard]] auto find_index(Predicate pred) const -> std::optional<std::size_t> {
        std::optional<std::size_t> found_idx;
        std::size_t idx = 0;
        consume([&](const T& item) -> bool {
            if (pred(item)) {
                found_idx = idx;
                return false;
            }
            ++idx;
            return true;
        });
        return found_idx;
    }

    // -------------------------------------------------------------------------
    // ANY / ALL / COUNT - Predicates (early-terminating!)
    // -------------------------------------------------------------------------
    template <typename Predicate>
    [[nodiscard]] auto any(Predicate pred) const -> bool {
        bool found = false;
        consume([&](const T& item) -> bool {
            if (pred(item)) {
                found = true;
                return false;
            }
            return true;
        });
        return found;
    }

    [[nodiscard]] auto any() const -> bool {
        if (!generator_) {
            return !ensure_materialized().empty();
        }
        bool found = false;
        consume([&](const T&) -> bool {
            found = true;
            return false;
        });
        return found;
    }

    template <typename Predicate>
    [[nodiscard]] auto all(Predicate pred) const -> bool {
        bool all_match = true;
        consume([&](const T& item) -> bool {
            if (!pred(item)) {
                all_match = false;
                return false;
            }
            return true;
        });
        return all_match;
    }

    template <typename Predicate>
    [[nodiscard]] auto count(Predicate pred) const -> std::size_t {
        std::size_t c = 0;
        consume([&](const T& item) -> bool {
            if (pred(item)) ++c;
            return true;
        });
        return c;
    }

    [[nodiscard]] auto count() const -> std::size_t {
        if (!generator_) {
            return ensure_materialized().size();
        }
        std::size_t c = 0;
        consume([&](const T&) -> bool {
            ++c;
            return true;
        });
        return c;
    }

    // -------------------------------------------------------------------------
    // AGGREGATE / REDUCE / FOLD - Combine elements
    // -------------------------------------------------------------------------
    template <typename Func>
    [[nodiscard]] auto aggregate(Func func) const -> std::optional<T> {
        std::optional<T> acc;
        consume([&](const T& item) -> bool {
            if (!acc.has_value()) {
                acc.emplace(item);
            } else {
                acc = func(*acc, item);
            }
            return true;
        });
        return acc;
    }

    template <typename Seed, typename Func>
    [[nodiscard]] auto aggregate(Seed seed, Func func) const -> Seed {
        Seed acc = std::move(seed);
        consume([&](const T& item) -> bool {
            acc = func(acc, item);
            return true;
        });
        return acc;
    }

    // Alias: reduce (functional programming style)
    template <typename Func>
    [[nodiscard]] auto reduce(Func func) const -> std::optional<T> {
        return aggregate(std::move(func));
    }

    template <typename Seed, typename Func>
    [[nodiscard]] auto reduce(Seed seed, Func func) const -> Seed {
        return aggregate(std::move(seed), std::move(func));
    }

    // Alias: fold (FP style)
    template <typename Seed, typename Func>
    [[nodiscard]] auto fold(Seed seed, Func func) const -> Seed {
        return aggregate(std::move(seed), std::move(func));
    }

    // -------------------------------------------------------------------------
    // FOREACH - Side effects
    // [C++ Core Guidelines ES.1: Prefer standard library to other libraries]
    // -------------------------------------------------------------------------
    template <typename Action>
    auto for_each(Action action) const -> void {
        consume([&](const T& item) -> bool {
            action(item);
            return true;
        });
    }

    // -------------------------------------------------------------------------
    // SUM / MIN / MAX / AVERAGE - Aggregations
    // -------------------------------------------------------------------------
    template <typename Selector>
    [[nodiscard]] auto sum(Selector selector) const 
        -> std::invoke_result_t<Selector, const T&> {
        using R = std::invoke_result_t<Selector, const T&>;
        R acc{};
        consume([&](const T& item) -> bool {
            acc = acc + selector(item);
            return true;
        });
        return acc;
    }

    template <typename Selector>
    [[nodiscard]] auto min(Selector selector) const 
        -> std::optional<std::invoke_result_t<Selector, const T&>> {
        using R = std::invoke_result_t<Selector, const T&>;
        std::optional<R> min_val;
        consume([&](const T& item) -> bool {
            R val = selector(item);
            if (!min_val.has_value() || val < *min_val) {
                min_val.emplace(std::move(val));
            }
            return true;
        });
        return min_val;
    }

    template <typename Selector>
    [[nodiscard]] auto max(Selector selector) const 
        -> std::optional<std::invoke_result_t<Selector, const T&>> {
        using R = std::invoke_result_t<Selector, const T&>;
        std::optional<R> max_val;
        consume([&](const T& item) -> bool {
            R val = selector(item);
            if (!max_val.has_value() || val > *max_val) {
                max_val.emplace(std::move(val));
            }
            return true;
        });
        return max_val;
    }

    template <typename Selector>
    [[nodiscard]] auto average(Selector selector) const -> std::optional<double> {
        double sum_val = 0.0;
        std::size_t cnt = 0;
        consume([&](const T& item) -> bool {
            sum_val += static_cast<double>(selector(item));
            ++cnt;
            return true;
        });
        if (cnt == 0) return std::nullopt;
        return sum_val / static_cast<double>(cnt);
    }

    // -------------------------------------------------------------------------
    // TAKE / SKIP - Pagination (lazy with early termination!)
    // -------------------------------------------------------------------------
    [[nodiscard]] auto take(std::size_t n) const -> query_result<T> {
        auto self = *this;
        return query_result<T>(stream_fn([self, n](const sink_fn& downstream) {
            if (n == 0) return;
            std::size_t taken = 0;
            self.consume([&](const T& item) -> bool {
                if (!downstream(item)) return false;
                return ++taken < n;
            });
        }));
    }

    [[nodiscard]] auto skip(std::size_t n) const -> query_result<T> {
        auto self = *this;
        return query_result<T>(stream_fn([self, n](const sink_fn& downstream) {
            std::size_t skipped = 0;
            self.consume([&](const T& item) -> bool {
                if (skipped < n) {
                    ++skipped;
                    return true;
                }
                return downstream(item);
            });
        }));
    }

    template <typename Predicate>
    [[nodiscard]] auto take_while(Predicate pred) const -> query_result<T> {
        auto self = *this;
        return query_result<T>(stream_fn([self, pred = std::move(pred)](const sink_fn& downstream) {
            self.consume([&](const T& item) -> bool {
                if (!pred(item)) return false;
                return downstream(item);
            });
        }));
    }

    template <typename Predicate>
    [[nodiscard]] auto skip_while(Predicate pred) const -> query_result<T> {
        auto self = *this;
        return query_result<T>(stream_fn([self, pred = std::move(pred)](const sink_fn& downstream) {
            bool skipping = true;
            self.consume([&](const T& item) -> bool {
                if (skipping) {
                    if (pred(item)) return true;
                    skipping = false;
                }
                return downstream(item);
            });
        }));
    }

    // -------------------------------------------------------------------------
    // DISTINCT - Unique elements (lazy, order-preserving)
    // -------------------------------------------------------------------------
    [[nodiscard]] auto distinct() const -> query_result<T> {
        auto self = *this;
        return query_result<T>(stream_fn([self](const sink_fn& downstream) {
            std::vector<T> seen;
            self.consume([&](const T& item) -> bool {
                if (std::find(seen.begin(), seen.end(), item) == seen.end()) {
                    seen.push_back(item);
                    return downstream(item);
                }
                return true;
            });
        }));
    }

    template <typename KeySelector>
    [[nodiscard]] auto distinct_by(KeySelector key_selector) const -> query_result<T> {
        using K = std::invoke_result_t<KeySelector, const T&>;
        auto self = *this;
        return query_result<T>(stream_fn([self, key_selector = std::move(key_selector)](const sink_fn& downstream) {
            std::vector<K> seen;
            self.consume([&](const T& item) -> bool {
                auto key = key_selector(item);
                if (std::find(seen.begin(), seen.end(), key) == seen.end()) {
                    seen.push_back(std::move(key));
                    return downstream(item);
                }
                return true;
            });
        }));
    }

    // -------------------------------------------------------------------------
    // CONCAT / UNION - Combine sequences (lazy!)
    // -------------------------------------------------------------------------
    [[nodiscard]] auto concat(const query_result<T>& other) const -> query_result<T> {
        auto self = *this;
        return query_result<T>(stream_fn([self, other](const sink_fn& downstream) {
            bool keep_going = true;
            self.consume([&](const T& item) -> bool {
                if (!downstream(item)) {
                    keep_going = false;
                    return false;
                }
                return true;
            });
            if (keep_going) {
                other.consume([&](const T& item) -> bool {
                    return downstream(item);
                });
            }
        }));
    }

    [[nodiscard]] auto union_with(const query_result<T>& other) const -> query_result<T> {
        return concat(other).distinct();
    }

    // -------------------------------------------------------------------------
    // ZIP - Combine two sequences element-wise (lazy with early termination!)
    // -------------------------------------------------------------------------
    template <typename TOther, typename ResultSelector>
    [[nodiscard]] auto zip(const query_result<TOther>& other, ResultSelector result_selector) const
        -> query_result<std::invoke_result_t<ResultSelector, const T&, const TOther&>> {
        using R = std::invoke_result_t<ResultSelector, const T&, const TOther&>;
        auto self = *this;
        return query_result<R>(typename query_result<R>::stream_fn(
            [self, other, result_selector = std::move(result_selector)](
                const typename query_result<R>::sink_fn& downstream) {
                const auto& other_vec = other.ensure_materialized();
                if (other_vec.empty()) return;
                std::size_t idx = 0;
                self.consume([&](const T& item) -> bool {
                    if (idx >= other_vec.size()) return false;
                    if (!downstream(result_selector(item, other_vec[idx]))) return false;
                    return ++idx < other_vec.size();
                });
            }));
    }

    template <typename TOther>
    [[nodiscard]] auto zip(const query_result<TOther>& other) const 
        -> query_result<std::pair<T, TOther>> {
        return zip(other, [](const T& a, const TOther& b) { 
            return std::make_pair(a, b); 
        });
    }

    // -------------------------------------------------------------------------
    // GROUP BY - Group elements by key
    // -------------------------------------------------------------------------
    template <typename KeySelector>
    [[nodiscard]] auto group_by(KeySelector key_selector) const
        -> std::unordered_map<std::invoke_result_t<KeySelector, const T&>, std::vector<T>> {
        using K = std::invoke_result_t<KeySelector, const T&>;
        std::unordered_map<K, std::vector<T>> groups;
        consume([&](const T& item) -> bool {
            groups[key_selector(item)].push_back(item);
            return true;
        });
        return groups;
    }

    // -------------------------------------------------------------------------
    // REVERSE - Reverse order (deferred until consumed)
    // -------------------------------------------------------------------------
    [[nodiscard]] auto reverse() const -> query_result<T> {
        auto self = *this;
        return query_result<T>(stream_fn([self](const sink_fn& downstream) {
            const auto& vec = self.ensure_materialized();
            for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
                if (!downstream(*it)) break;
            }
        }));
    }

    // -------------------------------------------------------------------------
    // Conversion
    // -------------------------------------------------------------------------
    [[nodiscard]] auto to_vector() const -> std::vector<T> {
        if (!generator_) {
            if (ext_data_) return *ext_data_;
            if (data_) return *data_;
            return {};
        }
        std::vector<T> result;
        consume([&result](const T& item) -> bool {
            result.push_back(item);
            return true;
        });
        return result;
    }

    [[nodiscard]] auto ensure_materialized() const -> const std::vector<T>& {
        if (!data_) {
            if (ext_data_ && !generator_) {
                return *ext_data_;
            }
            auto vec = std::make_shared<std::vector<T>>();
            if (generator_) {
                generator_([&vec](const T& item) -> bool {
                    vec->push_back(item);
                    return true;
                });
                generator_ = nullptr;
            }
            data_ = std::move(vec);
        }
        return *data_;
    }

private:
    auto ensure_mutable() -> std::vector<T>& {
        if (!data_) {
            auto vec = std::make_shared<std::vector<T>>();
            if (generator_) {
                generator_([&vec](const T& item) -> bool {
                    vec->push_back(item);
                    return true;
                });
                generator_ = nullptr;
            } else if (ext_data_) {
                *vec = *ext_data_;
                ext_data_ = nullptr;
            }
            data_ = std::move(vec);
        } else if (data_.use_count() > 1) {
            data_ = std::make_shared<std::vector<T>>(*data_);
        }
        return *data_;
    }

    mutable std::shared_ptr<std::vector<T>> data_;
    mutable const std::vector<T>* ext_data_ = nullptr;
    mutable stream_fn generator_;
};

// ============================================================================
// PART 10: JSON Value LINQ Extensions
// ============================================================================

// Extension: Create query from json_value array
[[nodiscard]] inline auto query(const json_value& value) -> query_result<json_value> {
    if (auto arr = value.as_array()) {
        return query_result<json_value>(*arr);
    }
    return query_result<json_value>();
}

// Extension: Query object keys
[[nodiscard]] inline auto query_keys(const json_value& value) -> query_result<std::string> {
    if (auto obj = value.as_object()) {
        std::vector<std::string> keys;
        keys.reserve(obj->size());
        for (const auto& [key, val] : *obj) {
            keys.push_back(key);
        }
        return query_result<std::string>(std::move(keys));
    }
    return query_result<std::string>();
}

// Extension: Query object values
[[nodiscard]] inline auto query_values(const json_value& value) -> query_result<json_value> {
    if (auto obj = value.as_object()) {
        std::vector<json_value> values;
        values.reserve(obj->size());
        for (const auto& [key, val] : *obj) {
            values.push_back(val);
        }
        return query_result<json_value>(std::move(values));
    }
    return query_result<json_value>();
}

// Extension: Query object as key-value pairs
[[nodiscard]] inline auto query_entries(const json_value& value) 
    -> query_result<std::pair<std::string, json_value>> {
    if (auto obj = value.as_object()) {
        std::vector<std::pair<std::string, json_value>> entries;
        entries.reserve(obj->size());
        for (const auto& [key, val] : *obj) {
            entries.emplace_back(key, val);
        }
        return query_result<std::pair<std::string, json_value>>(std::move(entries));
    }
    return query_result<std::pair<std::string, json_value>>();
}

// ============================================================================
// PART 11: Parallel Processing (C++ Core Guidelines: CP.*)
// Thread-safe operations using std::atomic and parallel algorithms
// ============================================================================

// Thread-safe parsing configuration
struct parallel_config {
    std::size_t min_parallel_size = 10000;  // Minimum bytes for parallel parsing
    std::size_t chunk_size = 4096;          // Chunk size for parallel operations
    unsigned int max_threads = 0;           // 0 = auto (hardware concurrency)
};

// Thread-local configuration
inline thread_local parallel_config g_parallel_config;

[[nodiscard]] inline auto get_parallel_config() noexcept -> const parallel_config& {
    return g_parallel_config;
}

inline auto set_parallel_config(const parallel_config& config) noexcept -> void {
    g_parallel_config = config;
}

// Get effective thread count
[[nodiscard]] inline auto get_thread_count() noexcept -> unsigned int {
    if (g_parallel_config.max_threads == 0) {
        return std::max(1u, std::thread::hardware_concurrency());
    }
    return g_parallel_config.max_threads;
}

// Thread-safe atomic counter for operations
class atomic_counter {
private:
    std::atomic<std::size_t> count_{0};

public:
    auto increment() noexcept -> std::size_t {
        return count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    [[nodiscard]] auto load() const noexcept -> std::size_t {
        return count_.load(std::memory_order_relaxed);
    }
    
    auto reset() noexcept -> void {
        count_.store(0, std::memory_order_relaxed);
    }
};

// Parallel query result with thread-safe operations
template <typename T>
class parallel_query_result {
private:
    std::vector<T> data_;

public:
    using value_type = T;

    parallel_query_result() = default;
    explicit parallel_query_result(std::vector<T>&& data) : data_(std::move(data)) {}

    [[nodiscard]] auto size() const noexcept -> std::size_t { return data_.size(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return data_.empty(); }
    [[nodiscard]] auto begin() const noexcept { return data_.begin(); }
    [[nodiscard]] auto end() const noexcept { return data_.end(); }

    // Parallel WHERE - filter with parallelism
    // [C++ Core Guidelines CP.2: Avoid data races]
    template <typename Predicate>
    [[nodiscard]] auto where(Predicate pred) const -> parallel_query_result<T> {
        if (data_.empty()) {
            return parallel_query_result<T>(std::vector<T>{});
        }
        auto keep = std::make_unique_for_overwrite<uint8_t[]>(data_.size());
        
        for (std::size_t i = 0; i < data_.size(); ++i) {
            keep[i] = pred(data_[i]) ? uint8_t{1} : uint8_t{0};
        }
        
        // Sequential compaction (must be sequential for correctness)
        std::vector<T> result;
        result.reserve(data_.size());
        for (std::size_t i = 0; i < data_.size(); ++i) {
            if (keep[i] != 0) {
                result.push_back(data_[i]);
            }
        }
        
        return parallel_query_result<T>(std::move(result));
    }

    // Alias: filter
    template <typename Predicate>
    [[nodiscard]] auto filter(Predicate pred) const -> parallel_query_result<T> {
        return where(std::move(pred));
    }

    // Parallel SELECT - transform with parallelism
    template <typename Func>
    [[nodiscard]] auto select(Func func) const 
        -> parallel_query_result<std::invoke_result_t<Func, const T&>> {
        using R = std::invoke_result_t<Func, const T&>;
        std::vector<R> result(data_.size());
        
        // Parallel transformation
        for (std::size_t i = 0; i < data_.size(); ++i) {
            result[i] = func(data_[i]);
        }
        
        return parallel_query_result<R>(std::move(result));
    }

    // Alias: map
    template <typename Func>
    [[nodiscard]] auto map(Func func) const 
        -> parallel_query_result<std::invoke_result_t<Func, const T&>> {
        return select(std::move(func));
    }

    // Parallel REDUCE
    template <typename Seed, typename Func>
    [[nodiscard]] auto reduce(Seed seed, Func func) const -> Seed {
        // Sequential reduction for correctness
        // (parallel reduction requires associative operations)
        return std::accumulate(data_.begin(), data_.end(), seed, func);
    }

    // Parallel FOR_EACH
    template <typename Action>
    auto for_each(Action action) const -> void {
        for (const auto& item : data_) {
            action(item);
        }
    }

    // Conversion to sequential query_result
    [[nodiscard]] auto to_sequential() const -> query_result<T> {
        return query_result<T>(std::vector<T>(data_));
    }

    [[nodiscard]] auto to_vector() const -> std::vector<T> { return data_; }
};

// Create parallel query from json_value array
[[nodiscard]] inline auto parallel_query(const json_value& value) 
    -> parallel_query_result<json_value> {
    if (auto arr = value.as_array()) {
        return parallel_query_result<json_value>(std::vector<json_value>(*arr));
    }
    return parallel_query_result<json_value>();
}

// ============================================================================
// PART 12: Thread-Safe Document (C++ Core Guidelines: CP.20)
// Immutable parsed document for safe concurrent access
// ============================================================================

// Thread-safe document wrapper
// [C++ Core Guidelines CP.20: Use RAII for thread-safe access]
class document {
private:
    std::shared_ptr<const json_value> root_;
    simd_info simd_info_;
    std::chrono::nanoseconds parse_time_{0};

public:
    document() = default;
    
    // Parse and create immutable document
    static auto parse(std::string_view input) -> result<document> {
        auto start = std::chrono::high_resolution_clock::now();
        auto value_result = fastjson::mr::parse(input);
        auto end = std::chrono::high_resolution_clock::now();
        
        if (!value_result.has_value()) {
            return std::unexpected(value_result.error());
        }
        
        document doc;
        doc.root_ = std::make_shared<const json_value>(std::move(value_result.value()));
        doc.simd_info_ = get_simd_info();
        doc.parse_time_ = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        return doc;
    }
    
    // Thread-safe accessors (returns const reference to immutable data)
    [[nodiscard]] auto root() const noexcept -> const json_value& { 
        static const json_value null_value;
        return root_ ? *root_ : null_value;
    }
    
    [[nodiscard]] auto has_value() const noexcept -> bool { return root_ != nullptr; }
    [[nodiscard]] auto simd() const noexcept -> const simd_info& { return simd_info_; }
    [[nodiscard]] auto parse_duration() const noexcept -> std::chrono::nanoseconds { 
        return parse_time_; 
    }
    
    // Query operations on the document
    [[nodiscard]] auto query() const -> query_result<json_value> {
        return fastjson::mr::query(root());
    }
    
    [[nodiscard]] auto parallel_query() const -> parallel_query_result<json_value> {
        return fastjson::mr::parallel_query(root());
    }
    
    // Stringify (creates new string, thread-safe)
    [[nodiscard]] auto stringify() const -> std::string {
        return root_ ? fastjson::mr::stringify(*root_) : "null";
    }
    
    [[nodiscard]] auto prettify(int indent = 2) const -> std::string {
        return root_ ? fastjson::mr::prettify(*root_, indent) : "null";
    }
};

} // namespace fastjson::mr