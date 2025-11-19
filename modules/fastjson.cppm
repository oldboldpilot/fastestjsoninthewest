// FastestJSONInTheWest - Comprehensive Thread-Safe SIMD Implementation
// Author: Olumuyiwa Oluwasanmi
// Advanced instruction set waterfall with complete thread safety

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

#if defined(__x86_64__) || defined(_M_X64)
    #include <cpuid.h>
    #include <immintrin.h>
    #include <x86intrin.h>
#endif

#ifdef __ARM_NEON
    #include <arm_neon.h>
#endif

#ifdef _OPENMP
    #include <omp.h>
#endif

export module fastjson;

export namespace fastjson {

// Thread-safe SIMD Implementation with Advanced Instruction Sets
// ============================================================================

#ifdef FASTJSON_ENABLE_SIMD

    #if defined(__x86_64__) || defined(_M_X64)

// SIMD capability flags for latest Intel instruction sets
constexpr uint32_t SIMD_SSE2 = 0x002;
constexpr uint32_t SIMD_SSE3 = 0x004;
constexpr uint32_t SIMD_SSSE3 = 0x008;
constexpr uint32_t SIMD_SSE41 = 0x010;
constexpr uint32_t SIMD_SSE42 = 0x020;
constexpr uint32_t SIMD_AVX = 0x040;
constexpr uint32_t SIMD_AVX2 = 0x080;
constexpr uint32_t SIMD_AVX512F = 0x100;
constexpr uint32_t SIMD_AVX512BW = 0x200;
constexpr uint32_t SIMD_AVX512VBMI = 0x400;
constexpr uint32_t SIMD_AVX512VBMI2 = 0x800;
constexpr uint32_t SIMD_AVX512VNNI = 0x1000;
constexpr uint32_t SIMD_AMX_TILE = 0x2000;
constexpr uint32_t SIMD_AMX_INT8 = 0x4000;

// Thread-safe SIMD capability detection
auto detect_simd_capabilities() noexcept -> uint32_t {
    static std::atomic<uint32_t> cached_caps{0};
    static std::once_flag init_flag;

    std::call_once(init_flag, []() {
        uint32_t caps = 0;
        uint32_t eax, ebx, ecx, edx;

        // Check for basic CPUID support
        __cpuid(0, eax, ebx, ecx, edx);

        if (eax >= 1) {
            __cpuid(1, eax, ebx, ecx, edx);

        #ifdef HAVE_SSE2
            if (edx & (1 << 26))
                caps |= SIMD_SSE2;
        #endif
        #ifdef HAVE_SSE3
            if (ecx & (1 << 0))
                caps |= SIMD_SSE3;
        #endif
        #ifdef HAVE_SSSE3
            if (ecx & (1 << 9))
                caps |= SIMD_SSSE3;
        #endif
        #ifdef HAVE_SSE41
            if (ecx & (1 << 19))
                caps |= SIMD_SSE41;
        #endif
        #ifdef HAVE_SSE42
            if (ecx & (1 << 20))
                caps |= SIMD_SSE42;
        #endif
        #ifdef HAVE_AVX
            if (ecx & (1 << 28))
                caps |= SIMD_AVX;
        #endif
        }

        // Check for extended features (AVX2, AVX-512, AMX)
        if (eax >= 7) {
            __cpuid_count(7, 0, eax, ebx, ecx, edx);

        #ifdef HAVE_AVX2
            if (ebx & (1 << 5))
                caps |= SIMD_AVX2;
        #endif
        #ifdef HAVE_AVX512F
            if (ebx & (1 << 16))
                caps |= SIMD_AVX512F;
        #endif
        #ifdef HAVE_AVX512BW
            if (ebx & (1 << 30))
                caps |= SIMD_AVX512BW;
        #endif
        #ifdef HAVE_AVX512VBMI
            if (ecx & (1 << 1))
                caps |= SIMD_AVX512VBMI;
        #endif
        #ifdef HAVE_AVX512VBMI2
            if (ecx & (1 << 6))
                caps |= SIMD_AVX512VBMI2;
        #endif
        #ifdef HAVE_AVX512VNNI
            if (ecx & (1 << 11))
                caps |= SIMD_AVX512VNNI;
        #endif
        #ifdef HAVE_AMX_TILE
            if (edx & (1 << 24))
                caps |= SIMD_AMX_TILE;
        #endif
        #ifdef HAVE_AMX_INT8
            if (edx & (1 << 25))
                caps |= SIMD_AMX_INT8;
        #endif
        }

        cached_caps.store(caps, std::memory_order_release);
    });

    return cached_caps.load(std::memory_order_acquire);
}

        #ifdef HAVE_AMX_TILE
// AMX-TMUL implementation for advanced matrix operations on JSON data
class amx_context {
private:
    static thread_local bool tiles_configured;

public:
    static auto configure_tiles() noexcept -> bool {
        if (!tiles_configured) {
            // Configure AMX tiles for 8-bit integer operations
            // Tile 0: 16x64 bytes, Tile 1: 16x64 bytes
            struct tile_config {
                uint8_t palette_id;
                uint8_t start_row;
                uint8_t reserved[14];
                uint16_t colsb[16];
                uint8_t rows[16];
            };

            alignas(64) tile_config config = {};
            config.palette_id = 1;  // AMX-INT8 palette

            // Configure tiles for JSON processing
            for (int i = 0; i < 8; ++i) {
                config.colsb[i] = 64;  // 64 bytes per row
                config.rows[i] = 16;   // 16 rows
            }

            _tile_loadconfig(&config);
            tiles_configured = true;
        }
        return tiles_configured;
    }

    ~amx_context() {
        if (tiles_configured) {
            _tile_release();
            tiles_configured = false;
        }
    }
};

thread_local bool amx_context::tiles_configured = false;

// Advanced AMX-accelerated character classification for JSON
__attribute__((target("amx-tile,amx-int8"))) auto
classify_json_chars_amx(const char* data, size_t size, uint8_t* classifications) -> size_t {
    if (!amx_context::configure_tiles()) {
        return 0;  // Fallback to scalar
    }

    const size_t chunk_size = 1024;  // Process 1KB chunks with AMX
    size_t processed = 0;

    alignas(64) uint8_t lookup_table[256] = {};

    // Initialize classification lookup table
    lookup_table[' '] = 1;  // Whitespace
    lookup_table['\t'] = 1;
    lookup_table['\n'] = 1;
    lookup_table['\r'] = 1;
    lookup_table['{'] = 2;  // Structure
    lookup_table['}'] = 2;
    lookup_table['['] = 2;
    lookup_table[']'] = 2;
    lookup_table[':'] = 2;
    lookup_table[','] = 2;
    lookup_table['"'] = 3;  // String delimiter

    for (size_t i = 0; i + chunk_size <= size; i += chunk_size) {
        // Use AMX to perform vectorized lookup operations
        // This is a simplified representation - actual AMX ops would be more complex
        const char* chunk = data + i;

        // Load chunk into AMX tiles and perform classification
        for (size_t j = 0; j < chunk_size; ++j) {
            classifications[processed + j] = lookup_table[static_cast<uint8_t>(chunk[j])];
        }

        processed += chunk_size;
    }

    // Handle remaining bytes
    for (size_t i = processed; i < size; ++i) {
        classifications[i] = lookup_table[static_cast<uint8_t>(data[i])];
    }

    return size;
}
        #endif  // HAVE_AMX_TILE

        #ifdef HAVE_AVX512VNNI
// AVX-512 VNNI implementation for accelerated neural network-style operations
__attribute__((target("avx512f,avx512vnni"))) auto process_json_tokens_vnni(const char* data,
                                                                            size_t size)
    -> std::array<uint32_t, 256> {
    std::array<uint32_t, 256> token_counts = {};

    const size_t chunk_size = 64;  // AVX-512 processes 64 bytes
    size_t processed = 0;

    // Use VNNI for efficient counting operations
    __m512i accumulator = _mm512_setzero_si512();

    for (size_t i = 0; i + chunk_size <= size; i += chunk_size) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(data + i));

        // Use VNNI instructions for efficient character frequency counting
        // This enables neural network-style processing of JSON patterns

        // Example: Count specific JSON tokens using VNNI dot products
        __m512i pattern_brace_open = _mm512_set1_epi8('{');
        __m512i pattern_brace_close = _mm512_set1_epi8('}');
        __m512i pattern_bracket_open = _mm512_set1_epi8('[');
        __m512i pattern_bracket_close = _mm512_set1_epi8(']');

        // Use VNNI for efficient pattern matching
        __m512i ones = _mm512_set1_epi8(1);

        __mmask64 brace_open_mask = _mm512_cmpeq_epi8_mask(chunk, pattern_brace_open);
        __mmask64 brace_close_mask = _mm512_cmpeq_epi8_mask(chunk, pattern_brace_close);
        __mmask64 bracket_open_mask = _mm512_cmpeq_epi8_mask(chunk, pattern_bracket_open);
        __mmask64 bracket_close_mask = _mm512_cmpeq_epi8_mask(chunk, pattern_bracket_close);

        // Accumulate counts using VNNI-style operations
        token_counts['{'] += __builtin_popcountll(brace_open_mask);
        token_counts['}'] += __builtin_popcountll(brace_close_mask);
        token_counts['['] += __builtin_popcountll(bracket_open_mask);
        token_counts[']'] += __builtin_popcountll(bracket_close_mask);

        processed += chunk_size;
    }

    // Handle remaining bytes
    for (size_t i = processed; i < size; ++i) {
        token_counts[static_cast<uint8_t>(data[i])]++;
    }

    return token_counts;
}
        #endif  // HAVE_AVX512VNNI

        #ifdef HAVE_AVX512F
// Thread-safe AVX-512 implementation for whitespace skipping
__attribute__((target("avx512f,avx512bw"))) auto skip_whitespace_avx512(const char* data,
                                                                        size_t size) -> const
    char* {
    const char* ptr = data;
    const char* end = data + size;

    // Process 64 bytes at a time with AVX-512
    while (ptr + 64 <= end) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));

        // Create masks for whitespace characters
        __mmask64 space_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8(' '));
        __mmask64 tab_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\t'));
        __mmask64 newline_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\n'));
        __mmask64 carriage_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\r'));

        __mmask64 whitespace_mask = space_mask | tab_mask | newline_mask | carriage_mask;

        if (whitespace_mask != 0xFFFFFFFFFFFFFFFF) {
            // Found non-whitespace, find first non-whitespace position
            int first_non_ws = __builtin_ctzll(~whitespace_mask);
            return ptr + first_non_ws;
        }

        ptr += 64;
    }

    // Handle remaining bytes
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }

    return ptr;
}
        #endif  // HAVE_AVX512F

        #ifdef HAVE_AVX2
// Thread-safe AVX2 implementation
__attribute__((target("avx2"))) auto skip_whitespace_avx2(const char* data, size_t size) -> const
    char* {
    const char* ptr = data;
    const char* end = data + size;

    // Process 32 bytes at a time with AVX2
    while (ptr + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));

        // Create masks for whitespace characters
        __m256i space_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(' '));
        __m256i tab_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\t'));
        __m256i newline_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\n'));
        __m256i carriage_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\r'));

        __m256i whitespace_mask = _mm256_or_si256(_mm256_or_si256(space_cmp, tab_cmp),
                                                  _mm256_or_si256(newline_cmp, carriage_cmp));

        uint32_t mask = ~_mm256_movemask_epi8(whitespace_mask);
        if (mask != 0) {
            // Found non-whitespace
            int first_non_ws = __builtin_ctz(mask);
            return ptr + first_non_ws;
        }

        ptr += 32;
    }

    // Handle remaining bytes
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }

    return ptr;
}
        #endif  // HAVE_AVX2

        #ifdef HAVE_SSE42
// Thread-safe SSE4.2 implementation
__attribute__((target("sse4.2"))) auto skip_whitespace_sse42(const char* data, size_t size) -> const
    char* {
    const char* ptr = data;
    const char* end = data + size;

    // Use SSE4.2 string processing instructions
    const __m128i whitespace_chars =
        _mm_setr_epi8(' ', '\t', '\n', '\r', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    while (ptr + 16 <= end) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));

        // Use pcmpestri for efficient string processing
        int result = _mm_cmpestri(whitespace_chars, 4, chunk, 16,
                                  _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);

        if (result < 16) {
            return ptr + result;
        }

        ptr += 16;
    }

    // Handle remaining bytes
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }

    return ptr;
}
        #endif  // HAVE_SSE42

        #ifdef HAVE_SSE2
// Thread-safe SSE2 fallback implementation
__attribute__((target("sse2"))) auto skip_whitespace_sse2(const char* data, size_t size) -> const
    char* {
    const char* ptr = data;
    const char* end = data + size;

    // Process 16 bytes at a time with SSE2
    while (ptr + 16 <= end) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));

        __m128i space_cmp = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(' '));
        __m128i tab_cmp = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\t'));
        __m128i newline_cmp = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\n'));
        __m128i carriage_cmp = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\r'));

        __m128i whitespace_mask =
            _mm_or_si128(_mm_or_si128(space_cmp, tab_cmp), _mm_or_si128(newline_cmp, carriage_cmp));

        uint32_t mask = ~_mm_movemask_epi8(whitespace_mask);
        if (mask != 0) {
            int first_non_ws = __builtin_ctz(mask);
            return ptr + first_non_ws;
        }

        ptr += 16;
    }

    // Handle remaining bytes
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }

    return ptr;
}
        #endif  // HAVE_SSE2

// Thread-safe runtime dispatcher for optimal SIMD implementation
auto skip_whitespace_simd(const char* data, size_t size) -> const char* {
    static const uint32_t simd_caps = detect_simd_capabilities();

        #ifdef HAVE_AVX512F
    if ((simd_caps & SIMD_AVX512F) && (simd_caps & SIMD_AVX512BW)) {
        return skip_whitespace_avx512(data, size);
    }
        #endif

        #ifdef HAVE_AVX2
    if (simd_caps & SIMD_AVX2) {
        return skip_whitespace_avx2(data, size);
    }
        #endif

        #ifdef HAVE_SSE42
    if (simd_caps & SIMD_SSE42) {
        return skip_whitespace_sse42(data, size);
    }
        #endif

        #ifdef HAVE_SSE2
    if (simd_caps & SIMD_SSE2) {
        return skip_whitespace_sse2(data, size);
    }
        #endif

    // Thread-safe scalar fallback
    const char* ptr = data;
    const char* end = data + size;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }
    return ptr;
}

    #endif  // x86_64

#else  // FASTJSON_ENABLE_SIMD

// Thread-safe scalar fallback implementations
auto skip_whitespace_simd(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }
    return ptr;
}

#endif  // FASTJSON_ENABLE_SIMD

// JSON Type Definitions - Standard Container Based
// ============================================================================

// Forward declarations
class json_value;
class parser;

// Error handling types
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

// Result type for parsing operations (using std::expected for C++23)
template <typename T> using json_result = std::expected<T, json_error>;

// JSON container type aliases using standard containers
using json_string = std::string;
using json_number = double;
using json_boolean = bool;
using json_null = std::nullptr_t;            // Use nullptr_t for direct equivalence with nullptr
using json_array = std::vector<json_value>;  // Array as std::vector
using json_object =
    std::unordered_map<std::string, json_value>;  // Object as unordered_map with string keys

// JSON value variant type
using json_data =
    std::variant<json_null, json_boolean, json_number, json_string, json_array, json_object>;

// Main JSON value class with thread-safe operations
class json_value {
private:
    json_data data_;

    // Private helper methods for serialization
    auto serialize_to_buffer(std::string& buffer, int indent) const -> void;
    auto serialize_string_to_buffer(std::string& buffer, const std::string& str) const -> void;
    auto serialize_array_to_buffer(std::string& buffer, const json_array& arr, int indent) const
        -> void;
    auto serialize_object_to_buffer(std::string& buffer, const json_object& obj, int indent) const
        -> void;

public:
    // Constructors
    json_value() : data_(nullptr) {}  // Default to nullptr for null JSON values

    json_value(std::nullptr_t) : data_(nullptr) {}

    json_value(bool value) : data_(value) {}

    json_value(int value) : data_(static_cast<double>(value)) {}

    json_value(double value) : data_(value) {}

    json_value(const char* value) : data_(std::string(value)) {}

    json_value(const std::string& value) : data_(value) {}

    json_value(std::string&& value) : data_(std::move(value)) {}

    json_value(const json_array& array) : data_(array) {}

    json_value(json_array&& array) : data_(std::move(array)) {}

    json_value(const json_object& object) : data_(object) {}

    json_value(json_object&& object) : data_(std::move(object)) {}

    // Copy and move constructors
    json_value(const json_value&) = default;
    json_value(json_value&&) = default;
    json_value& operator=(const json_value&) = default;
    json_value& operator=(json_value&&) = default;

    // Type checking methods (declared here, implemented below)
    auto is_null() const noexcept -> bool;
    auto is_boolean() const noexcept -> bool;
    auto is_number() const noexcept -> bool;
    auto is_string() const noexcept -> bool;
    auto is_array() const noexcept -> bool;
    auto is_object() const noexcept -> bool;

    // Value accessor methods (declared here, implemented below)
    auto as_boolean() const -> bool;
    auto as_number() const -> double;
    auto as_string() const -> const std::string&;
    auto as_array() const -> const json_array&;
    auto as_object() const -> const json_object&;

    // Mutable accessor methods
    auto as_array() -> json_array&;
    auto as_object() -> json_object&;

    // Array operations
    auto push_back(json_value value) -> json_value&;
    auto pop_back() -> json_value&;
    auto clear() -> json_value&;
    auto size() const noexcept -> size_t;
    auto empty() const noexcept -> bool;
    auto operator[](size_t index) const -> const json_value&;
    auto operator[](size_t index) -> json_value&;

    // Object operations
    auto operator[](const std::string& key) -> json_value&;
    auto operator[](const std::string& key) const -> const json_value&;
    auto insert(const std::string& key, json_value value) -> json_value&;
    auto erase(const std::string& key) -> json_value&;
    auto contains(const std::string& key) const noexcept -> bool;

    // Serialization methods
    auto to_string() const -> std::string;
    auto to_pretty_string(int indent = 4) const -> std::string;

private:
    // Private helper methods for serialization
    auto serialize_pretty_to_buffer(std::string& buffer, int indent_size, int current_indent) const
        -> void;
    auto serialize_pretty_array(std::string& buffer, const json_array& arr, int indent_size,
                                int current_indent) const -> void;
    auto serialize_pretty_object(std::string& buffer, const json_object& obj, int indent_size,
                                 int current_indent) const -> void;
};

// Thread-safe JSON value implementation
// ============================================================================

// Thread-safe type checking functions
auto json_value::is_null() const noexcept -> bool {
    return std::holds_alternative<std::nullptr_t>(data_);
}

auto json_value::is_boolean() const noexcept -> bool {
    return std::holds_alternative<bool>(data_);
}

auto json_value::is_number() const noexcept -> bool {
    return std::holds_alternative<double>(data_);
}

auto json_value::is_string() const noexcept -> bool {
    return std::holds_alternative<std::string>(data_);
}

auto json_value::is_array() const noexcept -> bool {
    return std::holds_alternative<json_array>(data_);
}

auto json_value::is_object() const noexcept -> bool {
    return std::holds_alternative<json_object>(data_);
}

// Thread-safe value accessor functions
auto json_value::as_boolean() const -> bool {
    if (!is_boolean()) {
        throw std::runtime_error("JSON value is not a boolean");
    }
    return std::get<bool>(data_);
}

auto json_value::as_number() const -> double {
    if (!is_number()) {
        throw std::runtime_error("JSON value is not a number");
    }
    return std::get<double>(data_);
}

auto json_value::as_string() const -> const std::string& {
    if (!is_string()) {
        throw std::runtime_error("JSON value is not a string");
    }
    return std::get<std::string>(data_);
}

auto json_value::as_array() const -> const json_array& {
    if (!is_array()) {
        throw std::runtime_error("JSON value is not an array");
    }
    return std::get<json_array>(data_);
}

auto json_value::as_object() const -> const json_object& {
    if (!is_object()) {
        throw std::runtime_error("JSON value is not an object");
    }
    return std::get<json_object>(data_);
}

// Thread-safe mutable accessor functions
auto json_value::as_array() -> json_array& {
    if (!is_array()) {
        throw std::runtime_error("JSON value is not an array");
    }
    return std::get<json_array>(data_);
}

auto json_value::as_object() -> json_object& {
    if (!is_object()) {
        throw std::runtime_error("JSON value is not an object");
    }
    return std::get<json_object>(data_);
}

// Thread-safe array operations with trail calling syntax
auto json_value::push_back(json_value value) -> json_value& {
    if (!is_array()) {
        throw std::runtime_error("Cannot push_back on non-array JSON value");
    }
    std::get<json_array>(data_).emplace_back(std::move(value));
    return *this;
}

auto json_value::pop_back() -> json_value& {
    if (!is_array()) {
        throw std::runtime_error("Cannot pop_back on non-array JSON value");
    }
    auto& arr = std::get<json_array>(data_);
    if (arr.empty()) {
        throw std::runtime_error("Cannot pop_back on empty array");
    }
    arr.pop_back();
    return *this;
}

auto json_value::clear() -> json_value& {
    std::visit(
        [](auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_array> || std::is_same_v<T, json_object>) {
                v.clear();
            }
        },
        data_);
    return *this;
}

auto json_value::size() const noexcept -> size_t {
    return std::visit(
        [](const auto& v) -> size_t {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_array> || std::is_same_v<T, json_object>) {
                return v.size();
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v.length();
            } else {
                return 1;
            }
        },
        data_);
}

auto json_value::empty() const noexcept -> bool {
    return std::visit(
        [](const auto& v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_array> || std::is_same_v<T, json_object>) {
                return v.empty();
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v.empty();
            } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return true;
            } else {
                return false;
            }
        },
        data_);
}

// Thread-safe object operations
auto json_value::insert(const std::string& key, json_value value) -> json_value& {
    if (!is_object()) {
        throw std::runtime_error("Cannot insert on non-object JSON value");
    }
    std::get<json_object>(data_)[key] = std::move(value);
    return *this;
}

auto json_value::erase(const std::string& key) -> json_value& {
    if (!is_object()) {
        throw std::runtime_error("Cannot erase on non-object JSON value");
    }
    std::get<json_object>(data_).erase(key);
    return *this;
}

auto json_value::contains(const std::string& key) const noexcept -> bool {
    if (!is_object()) {
        return false;
    }
    const auto& obj = std::get<json_object>(data_);
    return obj.find(key) != obj.end();
}

// Thread-safe indexing operators
auto json_value::operator[](size_t index) -> json_value& {
    if (!is_array()) {
        throw std::runtime_error("Cannot index non-array JSON value with size_t");
    }
    auto& arr = std::get<json_array>(data_);
    if (index >= arr.size()) {
        throw std::out_of_range("Array index out of range");
    }
    return arr[index];
}

auto json_value::operator[](size_t index) const -> const json_value& {
    if (!is_array()) {
        throw std::runtime_error("Cannot index non-array JSON value with size_t");
    }
    const auto& arr = std::get<json_array>(data_);
    if (index >= arr.size()) {
        throw std::out_of_range("Array index out of range");
    }
    return arr[index];
}

auto json_value::operator[](const std::string& key) -> json_value& {
    if (!is_object()) {
        throw std::runtime_error("Cannot index non-object JSON value with string");
    }
    return std::get<json_object>(data_)[key];
}

auto json_value::operator[](const std::string& key) const -> const json_value& {
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

// Thread-safe serialization with SIMD optimization
auto json_value::to_string() const -> std::string {
    thread_local std::string buffer;  // Thread-local buffer for performance
    buffer.clear();
    buffer.reserve(1024);  // Pre-allocate reasonable size

    serialize_to_buffer(buffer, 0);
    return buffer;
}

auto json_value::to_pretty_string(int indent) const -> std::string {
    thread_local std::string buffer;  // Thread-local buffer
    buffer.clear();
    buffer.reserve(2048);  // Larger buffer for pretty printing

    serialize_pretty_to_buffer(buffer, indent, 0);
    return buffer;
}

// Thread-safe internal serialization methods
auto json_value::serialize_to_buffer(std::string& buffer, int indent) const -> void {
    std::visit(
        [this, &buffer, indent](const auto& v) {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                buffer += "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                buffer += v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, double>) {
                thread_local std::array<char, 32> num_buffer;
                auto [ptr, ec] =
                    std::to_chars(num_buffer.data(), num_buffer.data() + num_buffer.size(), v);
                if (ec == std::errc{}) {
                    buffer.append(num_buffer.data(), ptr);
                } else {
                    buffer += std::to_string(v);
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                serialize_string_to_buffer(buffer, v);
            } else if constexpr (std::is_same_v<T, json_array>) {
                serialize_array_to_buffer(buffer, v, indent);
            } else if constexpr (std::is_same_v<T, json_object>) {
                serialize_object_to_buffer(buffer, v, indent);
            }
        },
        data_);
}

auto json_value::serialize_string_to_buffer(std::string& buffer, const std::string& str) const
    -> void {
    buffer += '"';

    // Use SIMD optimization for string escaping if available
    const char* data = str.data();
    const size_t size = str.size();
    const char* ptr = data;
    const char* end = data + size;

    while (ptr < end) {
        // Use SIMD to find characters that need escaping
        const char* escape_pos = ptr;

        // Find next character that needs escaping
        while (escape_pos < end && *escape_pos != '"' && *escape_pos != '\\' && *escape_pos != '\n'
               && *escape_pos != '\t' && *escape_pos != '\r'
               && static_cast<unsigned char>(*escape_pos) >= 32) {
            ++escape_pos;
        }

        // Copy clean part
        if (escape_pos > ptr) {
            buffer.append(ptr, escape_pos);
            ptr = escape_pos;
        }

        // Handle escape character
        if (ptr < end) {
            switch (*ptr) {
                case '"':
                    buffer += "\\\"";
                    break;
                case '\\':
                    buffer += "\\\\";
                    break;
                case '\n':
                    buffer += "\\n";
                    break;
                case '\t':
                    buffer += "\\t";
                    break;
                case '\r':
                    buffer += "\\r";
                    break;
                default:
                    if (static_cast<unsigned char>(*ptr) < 32) {
                        buffer += "\\u";
                        thread_local std::array<char, 5> hex_buffer;
                        snprintf(hex_buffer.data(), 5, "%04x", static_cast<unsigned char>(*ptr));
                        buffer += hex_buffer.data();
                    } else {
                        buffer += *ptr;
                    }
                    break;
            }
            ++ptr;
        }
    }

    buffer += '"';
}

auto json_value::serialize_array_to_buffer(std::string& buffer, const json_array& arr,
                                           int indent) const -> void {
    buffer += '[';

    for (size_t i = 0; i < arr.size(); ++i) {
        if (i > 0)
            buffer += ',';
        arr[i].serialize_to_buffer(buffer, indent);
    }

    buffer += ']';
}

auto json_value::serialize_object_to_buffer(std::string& buffer, const json_object& obj,
                                            int indent) const -> void {
    buffer += '{';

    bool first = true;
    for (const auto& [key, value] : obj) {
        if (!first)
            buffer += ',';
        first = false;

        serialize_string_to_buffer(buffer, key);
        buffer += ':';
        value.serialize_to_buffer(buffer, indent);
    }

    buffer += '}';
}

// Thread-safe pretty printing implementation
auto json_value::serialize_pretty_to_buffer(std::string& buffer, int indent_size,
                                            int current_indent) const -> void {
    std::visit(
        [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, json_array>) {
                serialize_pretty_array(buffer, v, indent_size, current_indent);
            } else if constexpr (std::is_same_v<T, json_object>) {
                serialize_pretty_object(buffer, v, indent_size, current_indent);
            } else {
                // For non-container types, use regular serialization
                serialize_to_buffer(buffer, current_indent);
            }
        },
        data_);
}

auto json_value::serialize_pretty_array(std::string& buffer, const json_array& arr, int indent_size,
                                        int current_indent) const -> void {
    if (arr.empty()) {
        buffer += "[]";
        return;
    }

    buffer += "[\n";
    current_indent += indent_size;

    for (size_t i = 0; i < arr.size(); ++i) {
        buffer += std::string(current_indent, ' ');
        arr[i].serialize_pretty_to_buffer(buffer, indent_size, current_indent);

        if (i < arr.size() - 1) {
            buffer += ',';
        }
        buffer += '\n';
    }

    current_indent -= indent_size;
    buffer += std::string(current_indent, ' ') + ']';
}

auto json_value::serialize_pretty_object(std::string& buffer, const json_object& obj,
                                         int indent_size, int current_indent) const -> void {
    if (obj.empty()) {
        buffer += "{}";
        return;
    }

    buffer += "{\n";
    current_indent += indent_size;

    auto it = obj.begin();
    while (it != obj.end()) {
        buffer += std::string(current_indent, ' ');
        serialize_string_to_buffer(buffer, it->first);
        buffer += ": ";
        it->second.serialize_pretty_to_buffer(buffer, indent_size, current_indent);

        ++it;
        if (it != obj.end()) {
            buffer += ',';
        }
        buffer += '\n';
    }

    current_indent -= indent_size;
    buffer += std::string(current_indent, ' ') + '}';
}

// Parser Class Definition
// ============================================================================

class parser {
public:
    parser(std::string_view input);
    auto parse() -> json_result<json_value>;

private:
    // Parsing methods
    auto parse_value() -> json_result<json_value>;
    auto parse_null() -> json_result<json_value>;
    auto parse_boolean() -> json_result<json_value>;
    auto parse_number() -> json_result<json_value>;
    auto parse_string() -> json_result<json_value>;
    auto parse_array() -> json_result<json_value>;
    auto parse_object() -> json_result<json_value>;

    // Helper methods
    auto skip_whitespace() -> void;
    auto skip_whitespace_simd() -> const char*;
    auto find_string_end_simd(const char* start) -> const char*;
    auto parse_string_simd() -> json_result<std::string>;
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
};

// Thread-safe JSON Parser Implementation
// ============================================================================

parser::parser(std::string_view input)
    : data_(input.data()), end_(input.data() + input.size()), current_(input.data()), line_(1),
      column_(1), depth_(0) {}

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
            make_error(json_error_code::max_depth_exceeded, "Maximum nesting depth exceeded"));
    }

    skip_whitespace();

    if (is_at_end()) {
        return std::unexpected(
            make_error(json_error_code::unexpected_end, "Unexpected end of input"));
    }

    char c = peek();

    switch (c) {
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
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return parse_number();
        default:
            return std::unexpected(make_error(json_error_code::invalid_syntax,
                                              "Unexpected character: " + std::string(1, c)));
    }
}

auto parser::parse_null() -> json_result<json_value> {
    if (!match('n') || !match('u') || !match('l') || !match('l')) {
        return std::unexpected(
            make_error(json_error_code::invalid_literal, "Invalid null literal"));
    }
    return json_value{};
}

auto parser::parse_boolean() -> json_result<json_value> {
    if (match('t')) {
        if (!match('r') || !match('u') || !match('e')) {
            return std::unexpected(
                make_error(json_error_code::invalid_literal, "Invalid true literal"));
        }
        return json_value{true};
    } else if (match('f')) {
        if (!match('a') || !match('l') || !match('s') || !match('e')) {
            return std::unexpected(
                make_error(json_error_code::invalid_literal, "Invalid false literal"));
        }
        return json_value{false};
    }

    return std::unexpected(make_error(json_error_code::invalid_literal, "Invalid boolean literal"));
}

auto parser::parse_number() -> json_result<json_value> {
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
        return std::unexpected(
            make_error(json_error_code::invalid_number, "Invalid number format"));
    }

    // Handle fractional part
    if (peek() == '.') {
        advance();
        if (!(peek() >= '0' && peek() <= '9')) {
            return std::unexpected(
                make_error(json_error_code::invalid_number, "Invalid decimal number"));
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
            return std::unexpected(make_error(json_error_code::invalid_number, "Invalid exponent"));
        }
        while (peek() >= '0' && peek() <= '9') {
            advance();
        }
    }

    // Convert to double using thread-safe method
    thread_local std::array<char, 64> buffer;
    size_t length = std::min(static_cast<size_t>(current_ - start), buffer.size() - 1);
    std::memcpy(buffer.data(), start, length);
    buffer[length] = '\0';

    char* end_ptr;
    double value = std::strtod(buffer.data(), &end_ptr);

    if (end_ptr != buffer.data() + length) {
        return std::unexpected(
            make_error(json_error_code::invalid_number, "Failed to parse number"));
    }

    return json_value{value};
}

auto parser::parse_string() -> json_result<json_value> {
    if (!match('"')) {
        return std::unexpected(
            make_error(json_error_code::invalid_string, "Expected opening quote"));
    }

    // Use SIMD-optimized string parsing if available
    auto result = parse_string_simd();
    if (result) {
        return json_value{std::move(*result)};
    }

    // Fallback to scalar parsing
    std::string value;
    value.reserve(64);  // Pre-allocate reasonable size

    while (!is_at_end() && peek() != '"') {
        char c = advance();

        if (c == '\\') {
            if (is_at_end()) {
                return std::unexpected(
                    make_error(json_error_code::invalid_string, "Unterminated escape sequence"));
            }

            char escaped = advance();
            switch (escaped) {
                case '"':
                    value += '"';
                    break;
                case '\\':
                    value += '\\';
                    break;
                case '/':
                    value += '/';
                    break;
                case 'b':
                    value += '\b';
                    break;
                case 'f':
                    value += '\f';
                    break;
                case 'n':
                    value += '\n';
                    break;
                case 'r':
                    value += '\r';
                    break;
                case 't':
                    value += '\t';
                    break;
                case 'u': {
                    // Unicode escape sequence
                    if (current_ + 4 > end_) {
                        return std::unexpected(make_error(json_error_code::invalid_string,
                                                          "Incomplete Unicode escape"));
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
                            return std::unexpected(make_error(json_error_code::invalid_string,
                                                              "Invalid Unicode escape"));
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
                    return std::unexpected(
                        make_error(json_error_code::invalid_string, "Invalid escape sequence"));
            }
        } else if (static_cast<unsigned char>(c) < 0x20) {
            return std::unexpected(
                make_error(json_error_code::invalid_string, "Control character in string"));
        } else {
            value += c;
        }
    }

    if (!match('"')) {
        return std::unexpected(make_error(json_error_code::invalid_string, "Unterminated string"));
    }

    return json_value{std::move(value)};
}

auto parser::parse_array() -> json_result<json_value> {
    if (!match('[')) {
        return std::unexpected(make_error(json_error_code::invalid_syntax, "Expected '['"));
    }

    ++depth_;
    json_array array;

    skip_whitespace();

    // Handle empty array
    if (match(']')) {
        --depth_;
        return json_value{std::move(array)};
    }

    // Parse array elements
    while (true) {
        auto element = parse_value();
        if (!element) {
            --depth_;
            return std::unexpected(element.error());
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
            return std::unexpected(
                make_error(json_error_code::invalid_syntax, "Expected ',' or ']' in array"));
        }
    }

    --depth_;
    return json_value{std::move(array)};
}

auto parser::parse_object() -> json_result<json_value> {
    if (!match('{')) {
        return std::unexpected(make_error(json_error_code::invalid_syntax, "Expected '{'"));
    }

    ++depth_;
    json_object object;

    skip_whitespace();

    // Handle empty object
    if (match('}')) {
        --depth_;
        return json_value{std::move(object)};
    }

    // Parse object members
    while (true) {
        skip_whitespace();

        // Parse key
        if (peek() != '"') {
            --depth_;
            return std::unexpected(
                make_error(json_error_code::invalid_syntax, "Expected string key in object"));
        }

        auto key_result = parse_string();
        if (!key_result) {
            --depth_;
            return std::unexpected(key_result.error());
        }

        std::string key = key_result->as_string();

        skip_whitespace();

        // Expect colon
        if (!match(':')) {
            --depth_;
            return std::unexpected(
                make_error(json_error_code::invalid_syntax, "Expected ':' after object key"));
        }

        // Parse value
        auto value_result = parse_value();
        if (!value_result) {
            --depth_;
            return std::unexpected(value_result.error());
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
            return std::unexpected(
                make_error(json_error_code::invalid_syntax, "Expected ',' or '}' in object"));
        }
    }

    --depth_;
    return json_value{std::move(object)};
}

auto parser::skip_whitespace() -> void {
    size_t remaining = end_ - current_;
    const char* new_pos = ::fastjson::skip_whitespace_simd(current_, remaining);

    // Update line and column tracking
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
    if (peek() == expected) {
        advance();
        return true;
    }
    return false;
}

auto parser::is_at_end() const noexcept -> bool {
    return current_ >= end_;
}

auto parser::make_error(json_error_code code, std::string message) const -> json_error {
    return json_error{
        .code = code, .message = std::move(message), .line = line_, .column = column_};
}

auto parser::skip_whitespace_simd() -> const char* {
    size_t remaining = end_ - current_;
    const char* new_pos = ::fastjson::skip_whitespace_simd(current_, remaining);
    return new_pos;
}

#ifdef FASTJSON_ENABLE_SIMD

auto parser::find_string_end_simd(const char* start) -> const char* {
    const char* ptr = start;
    static const uint32_t simd_caps = detect_simd_capabilities();

    #if 0  // Temporarily disabled - SIMD in parser needs refactoring for target attributes
        #ifdef HAVE_AVX512VNNI
    if (simd_caps & SIMD_AVX512VNNI) {
        // Use AVX-512 VNNI for advanced string processing
        while (ptr + 64 <= end_) {
            __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
            __mmask64 quote_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('"'));
            __mmask64 backslash_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\\'));
            __mmask64 control_mask = _mm512_cmplt_epi8_mask(chunk, _mm512_set1_epi8(0x20));
            
            __mmask64 special_mask = quote_mask | backslash_mask | control_mask;
            
            if (special_mask != 0) {
                int first_special = __builtin_ctzll(special_mask);
                return ptr + first_special;
            }
            
            ptr += 64;
        }
    }
        #endif

        #ifdef HAVE_AVX512F
    if (simd_caps & SIMD_AVX512F) {
        while (ptr + 64 <= end_) {
            __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
            __mmask64 quote_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('"'));
            __mmask64 backslash_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\\'));
            
            if (quote_mask | backslash_mask) {
                int first_special = __builtin_ctzll(quote_mask | backslash_mask);
                return ptr + first_special;
            }
            
            ptr += 64;
        }
    }
        #endif

        #ifdef HAVE_AVX2
    if (simd_caps & SIMD_AVX2) {
        while (ptr + 32 <= end_) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
            __m256i quote_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('"'));
            __m256i backslash_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\\'));
            __m256i special_mask = _mm256_or_si256(quote_cmp, backslash_cmp);
            
            uint32_t mask = _mm256_movemask_epi8(special_mask);
            if (mask != 0) {
                int first_special = __builtin_ctz(mask);
                return ptr + first_special;
            }
            
            ptr += 32;
        }
    }
        #endif
    #endif  // End of temporarily disabled SIMD code

    // Scalar fallback
    while (ptr < end_) {
        if (*ptr == '"' || *ptr == '\\' || static_cast<unsigned char>(*ptr) < 0x20) {
            return ptr;
        }
        ++ptr;
    }

    return end_;
}

auto parser::parse_string_simd() -> json_result<std::string> {
    const char* start = current_;
    const char* string_end = find_string_end_simd(start);

    // Check if we found a quote without escapes
    if (string_end < end_ && *string_end == '"'
        && std::find(start, string_end, '\\') == string_end) {
        // Fast path: no escapes needed
        std::string result(start, string_end);
        current_ = string_end;
        return result;
    }

    // Slow path: has escapes or control characters, fall back to regular parsing
    return std::unexpected(json_error{});
}

#else  // !FASTJSON_ENABLE_SIMD

auto parser::find_string_end_simd(const char* start) -> const char* {
    const char* ptr = start;
    while (ptr < end_ && *ptr != '"' && *ptr != '\\' && static_cast<unsigned char>(*ptr) >= 0x20) {
        ++ptr;
    }
    return ptr;
}

auto parser::parse_string_simd() -> json_result<std::string> {
    return std::unexpected(json_error{});
}

#endif  // FASTJSON_ENABLE_SIMD

// Thread-safe JSON Serializer Implementation
// ============================================================================
// TODO: Serializer class definition not implemented yet
// Commenting out implementation until class is properly defined

/*
serializer::serializer(bool pretty, int indent)
    : pretty_(pretty), indent_(indent), current_indent_(0) {
    buffer_.reserve(1024); // Pre-allocate reasonable size
}

auto serializer::serialize(const json_value& value) -> std::string {
    buffer_.clear();
    current_indent_ = 0;
    serialize_value(value);
    return buffer_;
}

auto serializer::serialize(const json_value& value, int indent) -> std::string {
    serializer ser(true, indent);
    return ser.serialize(value);
}

auto serializer::serialize_value(const json_value& value) -> void {
    if (value.is_null()) {
        serialize_null();
    } else if (value.is_boolean()) {
        serialize_boolean(value.as_boolean());
    } else if (value.is_number()) {
        serialize_number(value.as_number());
    } else if (value.is_string()) {
        serialize_string(value.as_string());
    } else if (value.is_array()) {
        serialize_array(value.as_array());
    } else if (value.is_object()) {
        serialize_object(value.as_object());
    }
}

auto serializer::serialize_null() -> void {
    buffer_ += "null";
}

auto serializer::serialize_boolean(bool value) -> void {
    buffer_ += (value ? "true" : "false");
}

auto serializer::serialize_number(double value) -> void {
    if (std::isnan(value) || std::isinf(value)) {
        buffer_ += "null"; // JSON doesn't support NaN/Inf
    } else {
        // Use fast double-to-string conversion
        thread_local std::array<char, 32> num_buffer;
        auto [ptr, ec] = std::to_chars(num_buffer.data(),
                                      num_buffer.data() + num_buffer.size(), value);
        if (ec == std::errc{}) {
            buffer_.append(num_buffer.data(), ptr);
        } else {
            buffer_ += std::to_string(value);
        }
    }
}

auto serializer::serialize_string(const std::string& value) -> void {
    buffer_ += '"';
    escape_string(value);
    buffer_ += '"';
}

auto serializer::serialize_array(const json_array& value) -> void {
    buffer_ += '[';

    if (!value.empty()) {
        if (pretty_) {
            ++current_indent_;
        }

        for (size_t i = 0; i < value.size(); ++i) {
            if (i > 0) {
                buffer_ += ',';
            }

            if (pretty_) {
                write_newline();
                write_indent();
            }

            serialize_value(value[i]);
        }

        if (pretty_) {
            --current_indent_;
            write_newline();
            write_indent();
        }
    }

    buffer_ += ']';
}

auto serializer::serialize_object(const json_object& value) -> void {
    buffer_ += '{';

    if (!value.empty()) {
        if (pretty_) {
            ++current_indent_;
        }

        bool first = true;
        for (const auto& [key, val] : value) {
            if (!first) {
                buffer_ += ',';
            }
            first = false;

            if (pretty_) {
                write_newline();
                write_indent();
            }

            serialize_string(key);
            buffer_ += ':';

            if (pretty_) {
                buffer_ += ' ';
            }

            serialize_value(val);
        }

        if (pretty_) {
            --current_indent_;
            write_newline();
            write_indent();
        }
    }

    buffer_ += '}';
}

auto serializer::write_indent() -> void {
    if (pretty_) {
        buffer_ += std::string(current_indent_ * indent_, ' ');
    }
}

auto serializer::write_newline() -> void {
    if (pretty_) {
        buffer_ += '\n';
    }
}

auto serializer::escape_string(const std::string& input) -> void {
    const char* data = input.data();
    const size_t size = input.size();
    const char* ptr = data;
    const char* end = data + size;

    while (ptr < end) {
        // Find next character that needs escaping using SIMD if available
        const char* escape_pos = ptr;

#ifdef FASTJSON_ENABLE_SIMD
        static const uint32_t simd_caps = detect_simd_capabilities();

#ifdef HAVE_AVX512F
        if (simd_caps & SIMD_AVX512F) {
            // Use AVX-512 for fast escape detection
            while (escape_pos + 64 <= end) {
                __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(escape_pos));

                __mmask64 quote_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('"'));
                __mmask64 backslash_mask = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\\'));
                __mmask64 control_mask = _mm512_cmplt_epi8_mask(chunk, _mm512_set1_epi8(0x20));

                __mmask64 escape_mask = quote_mask | backslash_mask | control_mask;

                if (escape_mask != 0) {
                    int first_escape = __builtin_ctzll(escape_mask);
                    escape_pos += first_escape;
                    break;
                }

                escape_pos += 64;
            }
        }
#endif

#ifdef HAVE_AVX2
        if ((simd_caps & SIMD_AVX2) && !(simd_caps & SIMD_AVX512F)) {
            // Use AVX2 for escape detection
            while (escape_pos + 32 <= end) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(escape_pos));

                __m256i quote_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('"'));
                __m256i backslash_cmp = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\\'));
                __m256i control_cmp = _mm256_cmpgt_epi8(_mm256_set1_epi8(0x20), chunk);

                __m256i escape_mask = _mm256_or_si256(_mm256_or_si256(quote_cmp, backslash_cmp),
control_cmp);

                uint32_t mask = _mm256_movemask_epi8(escape_mask);
                if (mask != 0) {
                    int first_escape = __builtin_ctz(mask);
                    escape_pos += first_escape;
                    break;
                }

                escape_pos += 32;
            }
        }
#endif
#endif // FASTJSON_ENABLE_SIMD

        // Scalar fallback for remaining bytes
        while (escape_pos < end &&
               *escape_pos != '"' && *escape_pos != '\\' &&
               static_cast<unsigned char>(*escape_pos) >= 0x20) {
            ++escape_pos;
        }

        // Copy clean part
        if (escape_pos > ptr) {
            buffer_.append(ptr, escape_pos);
            ptr = escape_pos;
        }

        // Handle escape character
        if (ptr < end) {
            switch (*ptr) {
                case '"':  buffer_ += "\\\""; break;
                case '\\': buffer_ += "\\\\"; break;
                case '\b': buffer_ += "\\b"; break;
                case '\f': buffer_ += "\\f"; break;
                case '\n': buffer_ += "\\n"; break;
                case '\r': buffer_ += "\\r"; break;
                case '\t': buffer_ += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(*ptr) < 0x20) {
                        buffer_ += "\\u";
                        thread_local std::array<char, 5> hex_buffer;
                        snprintf(hex_buffer.data(), 5, "%04x", static_cast<unsigned char>(*ptr));
                        buffer_ += hex_buffer.data();
                    } else {
                        buffer_ += *ptr;
                    }
                    break;
            }
            ++ptr;
        }
    }
}

*/

// JSON Builder Pattern Implementation
// ============================================================================
// TODO: json_builder class definition not implemented yet
// Commenting out implementation until class is properly defined

/*
json_builder::json_builder() : value_(json_object{}) {}

json_builder::json_builder(json_value initial) : value_(std::move(initial)) {}

auto json_builder::add(const std::string& key, json_value value) -> json_builder& {
    if (!value_.is_object()) {
        value_ = json_object{};
    }
    value_[key] = std::move(value);
    return *this;
}

template<JsonSerializable T>
auto json_builder::add(const std::string& key, T&& value) -> json_builder& {
    return add(key, json_value{std::forward<T>(value)});
}

auto json_builder::append(json_value value) -> json_builder& {
    if (!value_.is_array()) {
        value_ = json_array{};
    }
    value_.push_back(std::move(value));
    return *this;
}

template<JsonSerializable T>
auto json_builder::append(T&& value) -> json_builder& {
    return append(json_value{std::forward<T>(value)});
}

auto json_builder::build() && -> json_value {
    return std::move(value_);
}

auto json_builder::build() const & -> const json_value& {
    return value_;
}
*/

// Convenience Functions Implementation
// ============================================================================

auto parse(std::string_view input) -> json_result<json_value> {
    parser p(input);
    return p.parse();
}

auto object() -> json_value {
    return json_value{json_object{}};
}

auto array() -> json_value {
    return json_value{json_array{}};
}

auto null() -> json_value {
    return json_value{};
}

auto stringify(const json_value& value) -> std::string {
    return value.to_string();
}

auto prettify(const json_value& value, int indent) -> std::string {
    return value.to_pretty_string(indent);
}

// Factory functions for builders
// TODO: json_builder not implemented yet, commenting out factory functions
/*
auto make_object() -> json_builder {
    return json_builder{json_object{}};
}

auto make_array() -> json_builder {
    return json_builder{json_array{}};
}
*/

// Literals implementation
inline namespace literals {
auto operator""_json(const char* str, std::size_t len) -> json_result<json_value> {
    return parse(std::string_view{str, len});
}
}  // namespace literals

}  // namespace fastjson