// FastestJSONInTheWest - Parallel SIMD JSON Parser Module
// Copyright (c) 2025 - High-performance JSON parsing with OpenMP, SIMD, and GPU support
// ============================================================================

module;

// Global module fragment - all includes must be here
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <expected>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "unicode.h"
#include "fastjson_simd_index.h"
#include "numa_allocator.h"

#ifdef _OPENMP
    #include <omp.h>
#endif

// SIMD intrinsics
#if defined(__x86_64__) || defined(_M_X64)
    #include <cpuid.h>
    #include <immintrin.h>
#endif

#ifdef __ARM_NEON
    #include <arm_neon.h>
    #if defined(__linux__) && (defined(__aarch64__) || defined(_M_ARM64))
        #include <asm/hwcap.h>
        #include <sys/auxv.h>
    #endif
#endif

export module fastjson_parallel;

namespace fastjson_parallel {

// ============================================================================
// Configuration and Settings
// ============================================================================

// Text encoding types for JSON input
export enum class text_encoding {
    UTF8,         // UTF-8 (default, standard JSON)
    UTF16_LE,     // UTF-16 Little Endian
    UTF16_BE,     // UTF-16 Big Endian
    UTF32_LE,     // UTF-32 Little Endian
    UTF32_BE,     // UTF-32 Big Endian
    AUTO_DETECT   // Auto-detect from BOM or heuristics
};

export struct parse_config {
    // Parallelism control
    int num_threads = -1; // -1 = auto (use 30% of hardware max)
                          // 0 = disable parallelism (single-threaded)
                          // >0 = use exactly this many threads

    size_t parallel_threshold = 1000;  // Minimum array/object size for parallel parsing

    // SIMD control
    bool enable_simd = true;    // Enable SIMD optimizations
    bool enable_avx512 = true;  // Enable AVX-512 if available
    bool enable_neon = true;    // Enable ARM NEON if available

    // NUMA control
    bool enable_numa = true;            // Enable NUMA-aware allocations
    bool bind_threads_to_numa = true;   // Bind OpenMP threads to NUMA nodes
    bool use_numa_interleaved = false;  // Use interleaved vs node-local allocation
    bool enable_avx2 = true;            // Enable AVX2 if available
    bool enable_sse42 = true;           // Enable SSE4.2 if available

    // GPU control
    bool enable_gpu = false;       // Enable GPU acceleration (CUDA/ROCm/SYCL)
    size_t gpu_threshold = 10000;  // Minimum size for GPU offload

    // Parsing limits
    size_t max_depth = 1000;                      // Maximum nesting depth
    size_t max_string_length = 1024 * 1024 * 10;  // 10MB max string

    // Performance tuning
    size_t chunk_size = 100;      // Elements per thread chunk in parallel parsing
    bool use_memory_pool = true;  // Use memory pooling for allocations

    // Encoding support
    text_encoding input_encoding = text_encoding::AUTO_DETECT;  // Input encoding (default: auto-detect)
};

// Global configuration (thread-local for safety)
inline thread_local parse_config g_config;

// NUMA topology (shared across threads, initialized once)
inline fastjson::numa::numa_topology g_numa_topo;
inline std::atomic<bool> g_numa_initialized{false};

// Configuration API
export auto set_parse_config(const parse_config& config) -> void;
export auto get_parse_config() -> const parse_config&;
export auto set_num_threads(int threads) -> void;
export auto get_num_threads() -> int;

auto set_parse_config(const parse_config& config) -> void {
    g_config = config;
}

auto get_parse_config() -> const parse_config& {
    return g_config;
}

inline auto set_num_threads(int threads) -> void {
    g_config.num_threads = threads;
#ifdef _OPENMP
    if (threads > 0) {
        omp_set_num_threads(threads);
    }
#endif
}

inline auto get_num_threads() -> int {
#ifdef _OPENMP
    if (g_config.num_threads == 0) {
        return 1;  // Parallelism disabled
    } else if (g_config.num_threads > 0) {
        return g_config.num_threads;
    } else {
        // Use 30% of available threads as per requirement
        int max_threads = omp_get_max_threads();
        int target = static_cast<int>(max_threads * 0.3);
        return std::max(1, target);
    }
#else
    return 1;
#endif
}

inline auto get_effective_num_threads(size_t data_size) -> int {
    int max_threads = get_num_threads();
    if (max_threads == 1 || data_size < g_config.parallel_threshold) {
        return 1;
    }
    // Scale threads based on data size
    int effective = std::min(max_threads, static_cast<int>(data_size / g_config.chunk_size));
    return std::max(1, effective);
}

// ============================================================================
// Text Encoding Detection and Conversion
// ============================================================================

// Detect encoding from BOM (Byte Order Mark)
inline auto detect_encoding_from_bom(std::span<const uint8_t> data) -> std::pair<text_encoding, size_t> {
    if (data.size() < 2) {
        return {text_encoding::UTF8, 0};
    }

    // UTF-32 LE: FF FE 00 00
    if (data.size() >= 4 && data[0] == 0xFF && data[1] == 0xFE && 
        data[2] == 0x00 && data[3] == 0x00) {
        return {text_encoding::UTF32_LE, 4};
    }

    // UTF-32 BE: 00 00 FE FF
    if (data.size() >= 4 && data[0] == 0x00 && data[1] == 0x00 && 
        data[2] == 0xFE && data[3] == 0xFF) {
        return {text_encoding::UTF32_BE, 4};
    }

    // UTF-16 LE: FF FE
    if (data[0] == 0xFF && data[1] == 0xFE) {
        return {text_encoding::UTF16_LE, 2};
    }

    // UTF-16 BE: FE FF
    if (data[0] == 0xFE && data[1] == 0xFF) {
        return {text_encoding::UTF16_BE, 2};
    }

    // UTF-8: EF BB BF
    if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return {text_encoding::UTF8, 3};
    }

    // No BOM detected
    return {text_encoding::UTF8, 0};
}

// Detect encoding using heuristics (when no BOM present)
inline auto detect_encoding_heuristic(std::span<const uint8_t> data) -> text_encoding {
    if (data.size() < 4) {
        return text_encoding::UTF8;
    }

    // Check for UTF-32 patterns (lots of 00 bytes)
    if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] != 0x00) {
        return text_encoding::UTF32_BE;
    }
    if (data[0] != 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00) {
        return text_encoding::UTF32_LE;
    }

    // Check for UTF-16 patterns (alternating nulls)
    if (data[0] == 0x00 && data[2] == 0x00) {
        return text_encoding::UTF16_BE;
    }
    if (data[1] == 0x00 && data[3] == 0x00) {
        return text_encoding::UTF16_LE;
    }

    // Default to UTF-8
    return text_encoding::UTF8;
}

// Convert UTF-16 to UTF-8
inline auto convert_utf16_to_utf8(std::span<const uint8_t> data, bool big_endian) -> std::expected<std::string, std::string> {
    std::string result;
    result.reserve(data.size());  // Estimate

    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(data.data());
    size_t len = data.size() / 2;

    for (size_t i = 0; i < len; ++i) {
        uint16_t code_unit = big_endian ? 
            ((data[i*2] << 8) | data[i*2 + 1]) : 
            (data[i*2] | (data[i*2 + 1] << 8));

        // Check for surrogate pair
        if (fastjson::unicode::is_high_surrogate(code_unit)) {
            if (i + 1 >= len) {
                return std::unexpected("Incomplete UTF-16 surrogate pair");
            }
            uint16_t low = big_endian ? 
                ((data[(i+1)*2] << 8) | data[(i+1)*2 + 1]) : 
                (data[(i+1)*2] | (data[(i+1)*2 + 1] << 8));
            
            if (!fastjson::unicode::is_low_surrogate(low)) {
                return std::unexpected("Invalid UTF-16 surrogate pair");
            }

            uint32_t codepoint = fastjson::unicode::decode_surrogate_pair(code_unit, low);
            if (!fastjson::unicode::encode_utf8(codepoint, result)) {
                return std::unexpected("Invalid codepoint in surrogate pair");
            }
            ++i;  // Skip low surrogate
        } else if (fastjson::unicode::is_low_surrogate(code_unit)) {
            return std::unexpected("Unexpected low surrogate without high surrogate");
        } else {
            // BMP codepoint
            if (!fastjson::unicode::encode_utf8(code_unit, result)) {
                return std::unexpected("Invalid UTF-16 codepoint");
            }
        }
    }

    return result;
}

// Convert UTF-32 to UTF-8
inline auto convert_utf32_to_utf8(std::span<const uint8_t> data, bool big_endian) -> std::expected<std::string, std::string> {
    std::string result;
    result.reserve(data.size());  // Estimate

    size_t len = data.size() / 4;

    for (size_t i = 0; i < len; ++i) {
        uint32_t codepoint = big_endian ?
            ((data[i*4] << 24) | (data[i*4 + 1] << 16) | (data[i*4 + 2] << 8) | data[i*4 + 3]) :
            (data[i*4] | (data[i*4 + 1] << 8) | (data[i*4 + 2] << 16) | (data[i*4 + 3] << 24));

        if (!fastjson::unicode::is_valid_codepoint(codepoint)) {
            return std::unexpected("Invalid UTF-32 codepoint");
        }

        if (!fastjson::unicode::encode_utf8(codepoint, result)) {
            return std::unexpected("Failed to encode UTF-8");
        }
    }

    return result;
}

// Convert input data to UTF-8 based on detected or specified encoding
export auto convert_to_utf8(std::string_view input, text_encoding encoding = text_encoding::AUTO_DETECT) 
    -> std::expected<std::string, std::string>;

auto convert_to_utf8(std::string_view input, text_encoding encoding) 
    -> std::expected<std::string, std::string> {
    
    std::span<const uint8_t> data(reinterpret_cast<const uint8_t*>(input.data()), input.size());
    
    // Auto-detect encoding if requested
    size_t bom_size = 0;
    if (encoding == text_encoding::AUTO_DETECT) {
        auto [detected, bom_len] = detect_encoding_from_bom(data);
        if (bom_len > 0) {
            encoding = detected;
            bom_size = bom_len;
        } else {
            encoding = detect_encoding_heuristic(data);
        }
    }

    // Skip BOM if present
    data = data.subspan(bom_size);

    // Convert based on encoding
    switch (encoding) {
        case text_encoding::UTF8:
            // Already UTF-8, just return as string
            return std::string(reinterpret_cast<const char*>(data.data()), data.size());

        case text_encoding::UTF16_LE:
            return convert_utf16_to_utf8(data, false);

        case text_encoding::UTF16_BE:
            return convert_utf16_to_utf8(data, true);

        case text_encoding::UTF32_LE:
            return convert_utf32_to_utf8(data, false);

        case text_encoding::UTF32_BE:
            return convert_utf32_to_utf8(data, true);

        default:
            return std::unexpected("Unknown encoding");
    }
}

// ============================================================================
// JSON Type System
// ============================================================================
//
// Type Mappings (fastjson_parallel):
// ===================================
//
// JSON Type          C++ Type                              Usage
// ---------          --------                              -----
// null               std::nullptr_t                        json_value(nullptr)
// boolean            bool                                  json_value(true)
// number             double                                json_value(42.0)
// number (128-bit)   __float128                           json_value((__float128)3.14...)
// integer (128-bit)  __int128                             json_value((__int128)large_int)
// uint (128-bit)     unsigned __int128                    json_value((unsigned __int128)...)
// string             std::string                           json_value("text")
// array              std::vector<json_value>               json_value(json_array{...})
// object             std::unordered_map<string, value>     json_value(json_object{...})
//
// Key Design Decisions:
// ---------------------
// - Object → std::unordered_map: O(1) average lookup, ideal for key-value access
//   (not std::map: would be O(log n), slower for large objects)
// - Array → std::vector: O(1) random access, dynamic sizing, cache-friendly
//   (not std::array: requires compile-time size, JSON arrays are dynamic)
// - String → std::string: SSO optimization, move semantics, UTF-8 native
// - Number → double: Standard 64-bit float, sufficient for most JSON use cases
// - 128-bit types: Extended precision for financial/scientific applications
//
// Thread Safety:
// - Parsing is thread-safe (OpenMP parallelization)
// - json_value instances are NOT thread-safe after construction
// - Use separate json_value per thread or protect with mutex
//
// Performance Characteristics:
// - Object lookup: O(1) average, O(n) worst case
// - Array access: O(1) random access, O(1) amortized append
// - Memory: Contiguous for arrays, hash-based for objects
// ============================================================================

export enum class json_error_code {
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

export struct json_error {
    json_error_code code;
    std::string message;
    size_t line;
    size_t column;
};

template <typename T> using json_result = std::expected<T, json_error>;

// Core JSON type aliases
export using json_string = std::string;
export using json_number = double;
export using json_number_128 = __float128;       // Extended 128-bit float
export using json_int_128 = __int128;            // 128-bit signed integer
export using json_uint_128 = unsigned __int128;  // 128-bit unsigned integer
export using json_boolean = bool;
export using json_null = std::nullptr_t;

// Forward declaration
export class json_value;

// Container types - optimized for parallel processing with Copy-On-Write (COW)
export using json_array = std::vector<json_value>;
export using json_object = std::unordered_map<std::string, json_value>;

// Pointer types for COW implementation
using json_array_ptr = std::shared_ptr<json_array>;
using json_object_ptr = std::shared_ptr<json_object>;

export class json_value {
    // Helper for Copy-On-Write (COW)
    template <typename T>
    auto ensure_unique(std::shared_ptr<T>& ptr) -> T& {
        if (ptr.use_count() > 1) {
            ptr = std::make_shared<T>(*ptr);
        }
        return *ptr;
    }

public:
    json_value() : data_(nullptr) {}

    json_value(std::nullptr_t) : data_(nullptr) {}

    json_value(bool b) : data_(b) {}

    json_value(double d) : data_(d) {}

    json_value(int i) : data_(static_cast<__int128>(i)) {}

    json_value(int64_t i) : data_(static_cast<__int128>(i)) {}

    json_value(uint64_t i) : data_(static_cast<unsigned __int128>(i)) {}

    json_value(__float128 value) : data_(value) {}

    json_value(__int128 value) : data_(value) {}

    json_value(unsigned __int128 value) : data_(value) {}

    json_value(const std::string& s) : data_(s) {}

    json_value(std::string&& s) : data_(std::move(s)) {}

    json_value(const json_array& a) : data_(std::make_shared<json_array>(a)) {}

    json_value(json_array&& a) : data_(std::make_shared<json_array>(std::move(a))) {}

    json_value(const json_object& o) : data_(std::make_shared<json_object>(o)) {}

    json_value(json_object&& o) : data_(std::make_shared<json_object>(std::move(o))) {}

    // Destructor
    ~json_value() = default;

    json_value(const json_value&) = default;
    json_value(json_value&&) noexcept = default;
    json_value& operator=(const json_value&) = default;
    json_value& operator=(json_value&&) noexcept = default;

    auto is_null() const -> bool { return std::holds_alternative<json_null>(data_); }

    auto is_bool() const -> bool { return std::holds_alternative<json_boolean>(data_); }

    auto is_number() const -> bool { return std::holds_alternative<json_number>(data_); }

    auto is_string() const -> bool { return std::holds_alternative<json_string>(data_); }

    auto is_array() const -> bool { return std::holds_alternative<json_array_ptr>(data_); }

    auto is_object() const -> bool { return std::holds_alternative<json_object_ptr>(data_); }

    auto is_number_128() const -> bool { return std::holds_alternative<json_number_128>(data_); }

    auto is_int_128() const -> bool { return std::holds_alternative<json_int_128>(data_); }

    auto is_uint_128() const -> bool { return std::holds_alternative<json_uint_128>(data_); }

    // Value accessors
    auto as_bool() const -> bool { return std::get<json_boolean>(data_); }

    auto as_number() const -> double { return std::get<json_number>(data_); }

    auto as_string() const -> const std::string& { return std::get<json_string>(data_); }

    auto as_array() const -> const json_array& { return *std::get<json_array_ptr>(data_); }

    auto as_object() const -> const json_object& { return *std::get<json_object_ptr>(data_); }

    auto as_array() -> json_array& {
        if (!is_array()) data_ = std::make_shared<json_array>();
        return ensure_unique(std::get<json_array_ptr>(data_));
    }

    auto as_object() -> json_object& {
        if (!is_object()) data_ = std::make_shared<json_object>();
        return ensure_unique(std::get<json_object_ptr>(data_));
    }

    auto as_number_128() const -> json_number_128 {
        if (!is_number_128()) return static_cast<__float128>(std::numeric_limits<double>::quiet_NaN());
        return std::get<json_number_128>(data_);
    }

    auto as_int_128() const -> json_int_128 {
        if (!is_int_128()) return 0;
        return std::get<json_int_128>(data_);
    }

    auto as_uint_128() const -> json_uint_128 {
        if (!is_uint_128()) return 0;
        return std::get<json_uint_128>(data_);
    }

    // Conversion helpers
    auto as_int64() const -> int64_t {
        if (is_number()) return static_cast<int64_t>(std::get<json_number>(data_));
        if (is_int_128()) return static_cast<int64_t>(std::get<json_int_128>(data_));
        if (is_uint_128()) return static_cast<int64_t>(std::get<json_uint_128>(data_));
        if (is_number_128()) return static_cast<int64_t>(std::get<json_number_128>(data_));
        return 0;
    }

    auto as_float64() const -> double {
        if (is_number()) return std::get<json_number>(data_);
        if (is_int_128()) return static_cast<double>(std::get<json_int_128>(data_));
        if (is_uint_128()) return static_cast<double>(std::get<json_uint_128>(data_));
        if (is_number_128()) return static_cast<double>(std::get<json_number_128>(data_));
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto as_int128() const -> __int128 {
        if (is_int_128()) return std::get<json_int_128>(data_);
        if (is_uint_128()) return static_cast<__int128>(std::get<json_uint_128>(data_));
        if (is_number()) return static_cast<__int128>(std::get<json_number>(data_));
        if (is_number_128()) return static_cast<__int128>(std::get<json_number_128>(data_));
        return 0;
    }

    auto as_float128() const -> __float128 {
        if (is_number_128()) return std::get<json_number_128>(data_);
        if (is_number()) return static_cast<__float128>(std::get<json_number>(data_));
        if (is_int_128()) return static_cast<__float128>(std::get<json_int_128>(data_));
        if (is_uint_128()) return static_cast<__float128>(std::get<json_uint_128>(data_));
        return static_cast<__float128>(std::numeric_limits<double>::quiet_NaN());
    }

    // Array/Object size operations
    auto size() const noexcept -> size_t {
        if (is_array()) return std::get<json_array_ptr>(data_)->size();
        if (is_object()) return std::get<json_object_ptr>(data_)->size();
        return 0;
    }

    auto empty() const noexcept -> bool {
        if (is_array()) return std::get<json_array_ptr>(data_)->empty();
        if (is_object()) return std::get<json_object_ptr>(data_)->empty();
        return true;
    }

    // Array subscript operator
    auto operator[](size_t index) const -> const json_value& {
        return (*std::get<json_array_ptr>(data_))[index];
    }

    auto operator[](size_t index) -> json_value& {
        return as_array()[index];
    }

    // Object subscript operator
    auto operator[](const std::string& key) const -> const json_value& {
        return std::get<json_object_ptr>(data_)->at(key);
    }

    auto operator[](const std::string& key) -> json_value& {
        return as_object()[key];
    }

    // Serialization methods
    auto serialize_to_buffer(std::string& buffer, int indent) const -> void;
    auto serialize_pretty_to_buffer(std::string& buffer, int indent_size, int current_indent) const -> void;
    auto serialize_string_to_buffer(std::string& buffer, const std::string& str) const -> void;
    auto serialize_array_to_buffer(std::string& buffer, const json_array& arr, int indent) const -> void;
    auto serialize_object_to_buffer(std::string& buffer, const json_object& obj, int indent) const -> void;
    auto serialize_pretty_array(std::string& buffer, const json_array& arr, int indent_size, int current_indent) const -> void;
    auto serialize_pretty_object(std::string& buffer, const json_object& obj, int indent_size, int current_indent) const -> void;

    auto clear() -> json_value& {
        std::visit(
            [this](auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, json_array_ptr> || std::is_same_v<T, json_object_ptr>) {
                    if (v.use_count() > 1) {
                        v = std::make_shared<typename T::element_type>();
                    } else {
                        v->clear();
                    }
                } else if constexpr (std::is_same_v<T, std::string>) {
                    v.clear();
                } else {
                    data_ = nullptr;
                }
            },
            data_);
        return *this;
    }

private:
    std::variant<json_null, json_boolean, json_number, json_number_128, json_int_128, json_uint_128, json_string, json_array_ptr, json_object_ptr> data_;
};

// ============================================================================
// Type Mapping Utilities
// ============================================================================
//
// Helper functions for working with JSON type mappings:
// - Type-safe extraction with std::optional for failure handling
// - Batch conversion utilities for arrays
// - Object key iteration and filtering
// - Type validation and coercion

export namespace json_utils {
    // Extract value with optional for safe access
    inline auto try_get_string(const json_value& val) -> std::optional<std::string> {
        if (val.is_string()) return val.as_string();
        return std::nullopt;
    }

    inline auto try_get_number(const json_value& val) -> std::optional<double> {
        if (val.is_number()) return val.as_number();
        return std::nullopt;
    }

    inline auto try_get_bool(const json_value& val) -> std::optional<bool> {
        if (val.is_bool()) return val.as_bool();
        return std::nullopt;
    }

    inline auto try_get_array(const json_value& val) -> std::optional<json_array> {
        if (val.is_array()) return val.as_array();
        return std::nullopt;
    }

    inline auto try_get_object(const json_value& val) -> std::optional<json_object> {
        if (val.is_object()) return val.as_object();
        return std::nullopt;
    }

    // Extract array of specific type with transformation
    template<typename T, typename F>
    auto map_array(const json_value& val, F&& transform) -> std::optional<std::vector<T>> {
        if (!val.is_array()) return std::nullopt;
        
        std::vector<T> result;
        result.reserve(val.size());
        
        for (const auto& item : val.as_array()) {
            if (auto transformed = transform(item)) {
                result.push_back(*transformed);
            } else {
                return std::nullopt; // Transformation failed
            }
        }
        return result;
    }

    // Extract all string values from array
    inline auto array_to_strings(const json_value& val) -> std::optional<std::vector<std::string>> {
        return map_array<std::string>(val, [](const json_value& v) {
            return try_get_string(v);
        });
    }

    // Extract all numeric values from array
    inline auto array_to_numbers(const json_value& val) -> std::optional<std::vector<double>> {
        return map_array<double>(val, [](const json_value& v) {
            return try_get_number(v);
        });
    }

    // Get all keys from an object
    inline auto object_keys(const json_value& val) -> std::optional<std::vector<std::string>> {
        if (!val.is_object()) return std::nullopt;
        
        std::vector<std::string> keys;
        const auto& obj = val.as_object();
        keys.reserve(obj.size());
        
        for (const auto& [key, _] : obj) {
            keys.push_back(key);
        }
        return keys;
    }

    // Get all values from an object
    inline auto object_values(const json_value& val) -> std::optional<std::vector<json_value>> {
        if (!val.is_object()) return std::nullopt;
        
        std::vector<json_value> values;
        const auto& obj = val.as_object();
        values.reserve(obj.size());
        
        for (const auto& [_, value] : obj) {
            values.push_back(value);
        }
        return values;
    }

    // Check if object has key
    inline auto has_key(const json_value& val, const std::string& key) -> bool {
        if (!val.is_object()) return false;
        return val.as_object().contains(key);
    }

    // Safe object key access with default
    inline auto get_or(const json_value& val, const std::string& key, const json_value& default_val) -> json_value {
        if (!val.is_object()) return default_val;
        const auto& obj = val.as_object();
        auto it = obj.find(key);
        return (it != obj.end()) ? it->second : default_val;
    }

    // Filter object by predicate
    template<typename Predicate>
    auto filter_object(const json_value& val, Predicate&& pred) -> std::optional<json_object> {
        if (!val.is_object()) return std::nullopt;
        
        json_object result;
        for (const auto& [key, value] : val.as_object()) {
            if (pred(key, value)) {
                result[key] = value;
            }
        }
        return result;
    }

    // Filter array by predicate
    template<typename Predicate>
    auto filter_array(const json_value& val, Predicate&& pred) -> std::optional<json_array> {
        if (!val.is_array()) return std::nullopt;
        
        json_array result;
        for (const auto& item : val.as_array()) {
            if (pred(item)) {
                result.push_back(item);
            }
        }
        return result;
    }

    // Convert object to flat map of paths (for nested access)
    auto flatten_object(const json_value& val, const std::string& prefix = "") -> std::unordered_map<std::string, json_value> {
        std::unordered_map<std::string, json_value> result;
        
        if (!val.is_object()) {
            result[prefix] = val;
            return result;
        }
        
        for (const auto& [key, value] : val.as_object()) {
            std::string path = prefix.empty() ? key : prefix + "." + key;
            
            if (value.is_object()) {
                auto nested = flatten_object(value, path);
                result.insert(nested.begin(), nested.end());
            } else {
                result[path] = value;
            }
        }
        return result;
    }

    // Type coercion with fallback
    inline auto coerce_to_string(const json_value& val) -> std::string {
        if (val.is_string()) return val.as_string();
        if (val.is_number()) return std::to_string(val.as_number());
        if (val.is_bool()) return val.as_bool() ? "true" : "false";
        if (val.is_null()) return "null";
        return "[complex type]";
    }

    inline auto coerce_to_number(const json_value& val) -> std::optional<double> {
        if (val.is_number()) return val.as_number();
        if (val.is_bool()) return val.as_bool() ? 1.0 : 0.0;
        if (val.is_string()) {
            try {
                return std::stod(val.as_string());
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
}

// ============================================================================
// SIMD Detection and Optimization
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64)

struct simd_capabilities {
    bool sse2 = false;
    bool sse42 = false;
    bool avx2 = false;
    bool avx512f = false;
    bool avx512bw = false;
    bool fma = false;
    bool vnni = false;
    bool amx = false;
    bool neon = false;  // For ARM
};

#elif defined(__aarch64__) || defined(_M_ARM64)

struct simd_capabilities {
    bool neon = false;
    bool sve = false;      // Scalable Vector Extension (future)
    bool sve2 = false;     // SVE2 (future)
    bool dotprod = false;  // Dot product instructions
    bool i8mm = false;     // Int8 matrix multiply
    // Placeholders for x86 compatibility
    bool sse2 = false;
    bool sse42 = false;
    bool avx2 = false;
    bool avx512f = false;
    bool avx512bw = false;
    bool fma = false;
    bool vnni = false;
    bool amx = false;
};

#else

struct simd_capabilities {
    bool neon = false;
    bool sse2 = false;
    bool sse42 = false;
    bool avx2 = false;
    bool avx512f = false;
    bool avx512bw = false;
    bool fma = false;
    bool vnni = false;
    bool amx = false;
    bool sve = false;
    bool sve2 = false;
    bool dotprod = false;
    bool i8mm = false;
};

#endif

#if defined(__x86_64__) || defined(_M_X64)

__attribute__((target("default"))) static auto detect_simd_capabilities() -> simd_capabilities {
    unsigned int eax, ebx, ecx, edx;

    bool has_sse2 = false, has_sse42 = false, has_fma = false;
    bool has_avx2 = false, has_avx512f = false, has_avx512bw = false;
    bool has_vnni = false, has_amx = false;

    // Check CPUID support
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        has_sse2 = (edx & bit_SSE2) != 0;
        has_sse42 = (ecx & bit_SSE4_2) != 0;
        has_fma = (ecx & bit_FMA) != 0;

        if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
            has_avx2 = (ebx & bit_AVX2) != 0;
            has_avx512f = (ebx & bit_AVX512F) != 0;
            has_avx512bw = (ebx & bit_AVX512BW) != 0;
            has_vnni = (ecx & bit_AVX512VNNI) != 0;
            has_amx = (edx & (1 << 24)) != 0;  // AMX-TILE
        }
    }

    return simd_capabilities{.sse2 = has_sse2,
                             .sse42 = has_sse42,
                             .avx2 = has_avx2,
                             .avx512f = has_avx512f,
                             .avx512bw = has_avx512bw,
                             .fma = has_fma,
                             .vnni = has_vnni,
                             .amx = has_amx};
}

static const simd_capabilities g_simd_caps = detect_simd_capabilities();

#elif defined(__aarch64__) || defined(_M_ARM64)

// ARM NEON detection
static auto detect_simd_capabilities() -> simd_capabilities {
    simd_capabilities caps;

    // NEON is mandatory on AArch64
    caps.neon = true;

    // Check for optional extensions via hwcaps on Linux
    #ifdef __linux__
        #include <asm/hwcap.h>
        #include <sys/auxv.h>

    unsigned long hwcaps = getauxval(AT_HWCAP);

        #ifdef HWCAP_ASIMDDP
    caps.dotprod = (hwcaps & HWCAP_ASIMDDP) != 0;
        #endif

        #ifdef HWCAP_SVE
    caps.sve = (hwcaps & HWCAP_SVE) != 0;
        #endif

        #ifdef HWCAP_SVE2
    unsigned long hwcaps2 = getauxval(AT_HWCAP2);
    caps.sve2 = (hwcaps2 & HWCAP_SVE2) != 0;
        #endif

        #ifdef HWCAP_I8MM
    caps.i8mm = (hwcaps & HWCAP_I8MM) != 0;
        #endif

    #else
    // On non-Linux systems, assume basic NEON only
    caps.neon = true;
    #endif

    return caps;
}

static const simd_capabilities g_simd_caps = detect_simd_capabilities();

#else

static auto detect_simd_capabilities() -> simd_capabilities {
    return simd_capabilities{};  // No SIMD on unknown platforms
}

static const simd_capabilities g_simd_caps = detect_simd_capabilities();

#endif

// ============================================================================
// SIMD Implementations
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64)

// SIMD whitespace skipping
__attribute__((target("sse2"))) static auto skip_whitespace_sse2(std::span<const char> data,
                                                                 size_t start_pos) -> size_t {
    size_t pos = start_pos;
    const size_t size = data.size();

    while (pos + 16 <= size) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data.data() + pos));
        __m128i cmp_space = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(' '));
        __m128i cmp_tab = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\t'));
        __m128i cmp_newline = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\n'));
        __m128i cmp_return = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\r'));

        __m128i is_ws =
            _mm_or_si128(_mm_or_si128(cmp_space, cmp_tab), _mm_or_si128(cmp_newline, cmp_return));

        int mask = _mm_movemask_epi8(is_ws);
        if (mask != 0xFFFF) {
            int first_non_ws = __builtin_ctz(~mask & 0xFFFF);
            return pos + first_non_ws;
        }
        pos += 16;
    }

    while (pos < size
           && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\n' || data[pos] == '\r')) {
        pos++;
    }
    return pos;
}

__attribute__((target("avx2"))) static auto skip_whitespace_avx2(std::span<const char> data,
                                                                 size_t start_pos) -> size_t {
    size_t pos = start_pos;
    const size_t size = data.size();

    while (pos + 32 <= size) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data.data() + pos));
        __m256i cmp_space = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(' '));
        __m256i cmp_tab = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\t'));
        __m256i cmp_newline = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\n'));
        __m256i cmp_return = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\r'));

        __m256i is_ws = _mm256_or_si256(_mm256_or_si256(cmp_space, cmp_tab),
                                        _mm256_or_si256(cmp_newline, cmp_return));

        int mask = _mm256_movemask_epi8(is_ws);
        if (mask != 0xFFFFFFFF) {
            int first_non_ws = __builtin_ctz(~mask);
            return pos + first_non_ws;
        }
        pos += 32;
    }

    return skip_whitespace_sse2(data, pos);
}

__attribute__((target("avx512f,avx512bw"))) static auto
skip_whitespace_avx512(std::span<const char> data, size_t start_pos) -> size_t {
    size_t pos = start_pos;
    const size_t size = data.size();

    while (pos + 64 <= size) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(data.data() + pos));
        __mmask64 cmp_space = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8(' '));
        __mmask64 cmp_tab = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\t'));
        __mmask64 cmp_newline = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\n'));
        __mmask64 cmp_return = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\r'));

        __mmask64 is_ws = cmp_space | cmp_tab | cmp_newline | cmp_return;

        if (is_ws != 0xFFFFFFFFFFFFFFFF) {
            int first_non_ws = __builtin_ctzll(~is_ws);
            return pos + first_non_ws;
        }
        pos += 64;
    }

    return skip_whitespace_avx2(data, pos);
}

#elif defined(__aarch64__) || defined(_M_ARM64)

    // ARM NEON whitespace skipping
    #include <arm_neon.h>

static auto skip_whitespace_neon(std::span<const char> data, size_t start_pos) -> size_t {
    size_t pos = start_pos;
    const size_t size = data.size();

    while (pos + 16 <= size) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data.data() + pos));

        // Compare with whitespace characters
        uint8x16_t cmp_space = vceqq_u8(chunk, vdupq_n_u8(' '));
        uint8x16_t cmp_tab = vceqq_u8(chunk, vdupq_n_u8('\t'));
        uint8x16_t cmp_newline = vceqq_u8(chunk, vdupq_n_u8('\n'));
        uint8x16_t cmp_return = vceqq_u8(chunk, vdupq_n_u8('\r'));

        // OR all comparisons together
        uint8x16_t is_ws =
            vorrq_u8(vorrq_u8(cmp_space, cmp_tab), vorrq_u8(cmp_newline, cmp_return));

        // Create mask from high bit of each byte
        // Check if all bytes are whitespace
        uint64_t mask_low = vgetq_lane_u64(vreinterpretq_u64_u8(is_ws), 0);
        uint64_t mask_high = vgetq_lane_u64(vreinterpretq_u64_u8(is_ws), 1);

        if (mask_low != 0xFFFFFFFFFFFFFFFFULL || mask_high != 0xFFFFFFFFFFFFFFFFULL) {
            // Found non-whitespace, find first occurrence
            for (size_t i = 0; i < 16 && pos + i < size; ++i) {
                char c = data[pos + i];
                if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                    return pos + i;
                }
            }
        }
        pos += 16;
    }

    // Scalar fallback for remaining bytes
    while (pos < size
           && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\n' || data[pos] == '\r')) {
        pos++;
    }
    return pos;
}

#endif

static auto skip_whitespace_simd(std::span<const char> data, size_t start_pos) -> size_t {
    if (!g_config.enable_simd) {
        goto scalar_fallback;
    }

#if defined(__x86_64__) || defined(_M_X64)
    if (g_config.enable_avx512 && g_simd_caps.avx512f && g_simd_caps.avx512bw) {
        return skip_whitespace_avx512(data, start_pos);
    } else if (g_config.enable_avx2 && g_simd_caps.avx2) {
        return skip_whitespace_avx2(data, start_pos);
    } else if (g_config.enable_sse42 && g_simd_caps.sse2) {
        return skip_whitespace_sse2(data, start_pos);
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    if (g_config.enable_neon && g_simd_caps.neon) {
        return skip_whitespace_neon(data, start_pos);
    }
#endif

scalar_fallback:
    size_t pos = start_pos;
    while (pos < data.size()
           && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\n' || data[pos] == '\r')) {
        pos++;
    }
    return pos;
}

#if defined(__x86_64__) || defined(_M_X64)

// SIMD string scanning - find end quote or escape
__attribute__((target("avx2"))) static auto find_string_end_avx2(std::span<const char> data,
                                                                 size_t start_pos) -> size_t {
    const __m256i quote = _mm256_set1_epi8('"');
    const __m256i backslash = _mm256_set1_epi8('\\');
    // For unsigned comparison (chars < 0x20), we use saturating subtract:
    // If c < 0x20, then (c - 0x20) saturates to 0. Otherwise it's non-zero.
    // This correctly handles UTF-8 high bytes (0x80-0xFF) as non-control chars.
    const __m256i control_threshold = _mm256_set1_epi8(0x20);

    size_t pos = start_pos;

    while (pos + 32 <= data.size()) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data.data() + pos));

        __m256i is_quote = _mm256_cmpeq_epi8(chunk, quote);
        __m256i is_backslash = _mm256_cmpeq_epi8(chunk, backslash);
        // Unsigned comparison: is_control = (chunk < 0x20) using saturating subtract
        // _mm256_subs_epu8(chunk, 0x1F) == 0 when chunk <= 0x1F (i.e., chunk < 0x20)
        __m256i sub_result = _mm256_subs_epu8(chunk, _mm256_set1_epi8(0x1F));
        __m256i is_control = _mm256_cmpeq_epi8(sub_result, _mm256_setzero_si256());

        __m256i special = _mm256_or_si256(_mm256_or_si256(is_quote, is_backslash), is_control);

        uint32_t mask = _mm256_movemask_epi8(special);

        if (mask != 0) {
            int first_special = __builtin_ctz(mask);
            return pos + first_special;
        }

        pos += 32;
    }

    // Scalar fallback for remaining bytes
    while (pos < data.size()) {
        char c = data[pos];
        if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
            return pos;
        }
        pos++;
    }

    return pos;
}

#elif defined(__aarch64__) || defined(_M_ARM64)

// ARM NEON string scanning - find end quote or escape
static auto find_string_end_neon(std::span<const char> data, size_t start_pos) -> size_t {
    size_t pos = start_pos;

    while (pos + 16 <= data.size()) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data.data() + pos));

        // Create comparison vectors
        uint8x16_t quote = vdupq_n_u8('"');
        uint8x16_t backslash = vdupq_n_u8('\\');
        uint8x16_t control = vdupq_n_u8(0x20);

        // Compare
        uint8x16_t is_quote = vceqq_u8(chunk, quote);
        uint8x16_t is_backslash = vceqq_u8(chunk, backslash);
        uint8x16_t is_control = vcltq_u8(chunk, control);  // chunk < 0x20

        // OR all special character checks
        uint8x16_t special = vorrq_u8(vorrq_u8(is_quote, is_backslash), is_control);

        // Check if any special character found
        uint64_t mask_low = vgetq_lane_u64(vreinterpretq_u64_u8(special), 0);
        uint64_t mask_high = vgetq_lane_u64(vreinterpretq_u64_u8(special), 1);

        if (mask_low != 0 || mask_high != 0) {
            // Found special character, scan to find exact position
            for (size_t i = 0; i < 16 && pos + i < data.size(); ++i) {
                char c = data[pos + i];
                if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
                    return pos + i;
                }
            }
        }

        pos += 16;
    }

    // Scalar fallback for remaining bytes
    while (pos < data.size()) {
        char c = data[pos];
        if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
            return pos;
        }
        pos++;
    }

    return pos;
}

#endif

#if defined(__x86_64__) || defined(_M_X64)

// SIMD number validation - check if all digits/valid number chars
__attribute__((target("avx2"))) static auto
validate_number_chars_avx2(std::span<const char> data, size_t start_pos, size_t end_pos) -> bool {
    const __m256i digit_0 = _mm256_set1_epi8('0' - 1);
    const __m256i digit_9 = _mm256_set1_epi8('9' + 1);
    const __m256i minus = _mm256_set1_epi8('-');
    const __m256i plus = _mm256_set1_epi8('+');
    const __m256i dot = _mm256_set1_epi8('.');
    const __m256i e_lower = _mm256_set1_epi8('e');
    const __m256i e_upper = _mm256_set1_epi8('E');

    size_t pos = start_pos;

    while (pos + 32 <= end_pos) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data.data() + pos));

        // Check if in range '0'-'9'
        __m256i gt_digit_0 = _mm256_cmpgt_epi8(chunk, digit_0);
        __m256i lt_digit_9 = _mm256_cmpgt_epi8(digit_9, chunk);
        __m256i is_digit = _mm256_and_si256(gt_digit_0, lt_digit_9);

        // Check for valid special chars
        __m256i is_minus = _mm256_cmpeq_epi8(chunk, minus);
        __m256i is_plus = _mm256_cmpeq_epi8(chunk, plus);
        __m256i is_dot = _mm256_cmpeq_epi8(chunk, dot);
        __m256i is_e_lower = _mm256_cmpeq_epi8(chunk, e_lower);
        __m256i is_e_upper = _mm256_cmpeq_epi8(chunk, e_upper);

        __m256i valid = _mm256_or_si256(
            is_digit,
            _mm256_or_si256(_mm256_or_si256(is_minus, is_plus),
                            _mm256_or_si256(_mm256_or_si256(is_dot, is_e_lower), is_e_upper)));

        uint32_t mask = _mm256_movemask_epi8(valid);

        if (mask != 0xFFFFFFFF) {
            return false;  // Found invalid character
        }

        pos += 32;
    }

    // Scalar validation for remaining bytes
    while (pos < end_pos) {
        char c = data[pos];
        if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')) {
            return false;
        }
        pos++;
    }

    return true;
}

// SIMD literal matching (true, false, null)
__attribute__((target("sse2"))) static auto match_literal_sse2(std::span<const char> data,
                                                               size_t pos, const char* literal,
                                                               size_t len) -> bool {
    if (pos + len > data.size()) {
        return false;
    }

    // For small literals (4, 5 chars), use single SSE load and compare
    if (len <= 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data.data() + pos));
        __m128i expected = _mm_loadu_si128(reinterpret_cast<const __m128i*>(literal));
        __m128i cmp = _mm_cmpeq_epi8(chunk, expected);
        int mask = _mm_movemask_epi8(cmp);

        // Check only the first 'len' bytes
        int len_mask = (1 << len) - 1;
        return (mask & len_mask) == len_mask;
    }

    // Fallback for longer strings
    return std::memcmp(data.data() + pos, literal, len) == 0;
}

// FMA-accelerated number parsing for simple integers
// Uses FMA for fast digit accumulation in chunks of 8 digits
// Based on techniques from lemire/fast_double_parser
__attribute__((target("fma,avx2"))) static auto parse_number_fma(std::string_view str)
    -> std::optional<double> {
    if (str.empty())
        return std::nullopt;

    const char* p = str.data();
    const char* end = p + str.length();
    bool negative = false;

    // Handle sign
    if (*p == '-') {
        negative = true;
        p++;
        if (p >= end)
            return std::nullopt;
    }

    // Only handle pure integers or simple decimals (no exponent) for fast path
    // Scan ahead to check
    bool has_decimal = false;
    for (const char* scan = p; scan < end; ++scan) {
        if (*scan == 'e' || *scan == 'E')
            return std::nullopt;  // Has exponent, use strtod
        if (*scan == '.') {
            if (has_decimal)
                return std::nullopt;  // Multiple decimal points
            has_decimal = true;
        }
    }

    // Fast integer parsing using chunks of 8 digits
    uint64_t int_part = 0;
    const char* decimal_pos = p;

    // Find decimal point if present
    while (decimal_pos < end && *decimal_pos != '.') {
        if (*decimal_pos < '0' || *decimal_pos > '9')
            return std::nullopt;
        decimal_pos++;
    }

    // Parse integer part
    size_t int_digits = decimal_pos - p;
    if (int_digits == 0)
        return std::nullopt;

    // Process 8 digits at a time using FMA for accumulation
    const char* chunk_end = p + (int_digits / 8) * 8;

    while (p < chunk_end) {
        // Process 8 digits: d0 d1 d2 d3 d4 d5 d6 d7
        // Result = int_part * 10^8 + (d0*10^7 + d1*10^6 + ... + d7)
        uint32_t chunk = 0;
        for (int i = 0; i < 8; ++i) {
            chunk = chunk * 10 + (*p++ - '0');
        }
        // Use FMA: int_part = int_part * 100000000 + chunk
        int_part = static_cast<uint64_t>(
            __builtin_fma(static_cast<double>(int_part), 100000000.0, static_cast<double>(chunk)));
    }

    // Handle remaining digits (< 8)
    while (p < decimal_pos) {
        int_part = int_part * 10 + (*p++ - '0');
    }

    double result = static_cast<double>(int_part);

    // Handle decimal part if present
    if (p < end && *p == '.') {
        p++;  // Skip decimal point

        uint64_t frac_part = 0;
        int frac_digits = 0;
        const int max_frac_digits = 15;  // Limit precision

        while (p < end && frac_digits < max_frac_digits) {
            if (*p < '0' || *p > '9')
                return std::nullopt;
            frac_part = frac_part * 10 + (*p++ - '0');
            frac_digits++;
        }

        // Skip any remaining fractional digits (beyond precision)
        while (p < end) {
            if (*p < '0' || *p > '9')
                return std::nullopt;
            p++;
        }

        if (frac_digits > 0) {
            // Use FMA to compute: result + frac_part * 10^(-frac_digits)
            static const double pow10[] = {1.0,
                                           0.1,
                                           0.01,
                                           0.001,
                                           0.0001,
                                           0.00001,
                                           0.000001,
                                           0.0000001,
                                           0.00000001,
                                           0.000000001,
                                           0.0000000001,
                                           0.00000000001,
                                           0.000000000001,
                                           0.0000000000001,
                                           0.00000000000001,
                                           0.000000000000001};
            result = __builtin_fma(static_cast<double>(frac_part), pow10[frac_digits], result);
        }
    }

    // Verify we consumed all input
    if (p != end)
        return std::nullopt;

    return negative ? -result : result;
}

// SIMD UTF-8 validation using AVX2
// Based on the algorithm from "Validating UTF-8 In Less Than One Instruction Per Byte"
// https://github.com/simdjson/simdjson/blob/master/src/generic/stage1/utf8_lookup4_algorithm.h
__attribute__((target("avx2"))) static auto validate_utf8_avx2(std::span<const char> data,
                                                               size_t start_pos, size_t end_pos)
    -> bool {
    size_t pos = start_pos;

    // Process 32 bytes at a time with AVX2
    while (pos + 32 <= end_pos) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data.data() + pos));

        // Check for ASCII (high bit clear)
        __m256i high_bit = _mm256_and_si256(chunk, _mm256_set1_epi8(static_cast<char>(0x80)));
        uint32_t ascii_mask =
            _mm256_movemask_epi8(_mm256_cmpeq_epi8(high_bit, _mm256_setzero_si256()));

        if (ascii_mask == 0xFFFFFFFF) {
            // All ASCII, continue
            pos += 32;
            continue;
        }

        // Need to check multi-byte sequences - fall back to scalar for this chunk
        for (size_t i = 0; i < 32 && pos < end_pos; ++i, ++pos) {
            unsigned char c = static_cast<unsigned char>(data[pos]);

            if (c < 0x80) {
                // ASCII
                continue;
            } else if ((c & 0xE0) == 0xC0) {
                // 2-byte sequence: 110xxxxx 10xxxxxx
                if (pos + 1 >= end_pos)
                    return false;
                unsigned char c2 = static_cast<unsigned char>(data[pos + 1]);
                if ((c2 & 0xC0) != 0x80)
                    return false;
                // Check for overlong encoding
                if (c < 0xC2)
                    return false;
                pos += 1;
            } else if ((c & 0xF0) == 0xE0) {
                // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
                if (pos + 2 >= end_pos)
                    return false;
                unsigned char c2 = static_cast<unsigned char>(data[pos + 1]);
                unsigned char c3 = static_cast<unsigned char>(data[pos + 2]);
                if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
                    return false;
                // Check for overlong encodings and surrogates
                if (c == 0xE0 && c2 < 0xA0)
                    return false;  // Overlong
                if (c == 0xED && c2 >= 0xA0)
                    return false;  // Surrogate
                pos += 2;
            } else if ((c & 0xF8) == 0xF0) {
                // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                if (pos + 3 >= end_pos)
                    return false;
                unsigned char c2 = static_cast<unsigned char>(data[pos + 1]);
                unsigned char c3 = static_cast<unsigned char>(data[pos + 2]);
                unsigned char c4 = static_cast<unsigned char>(data[pos + 3]);
                if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80)
                    return false;
                // Check for overlong and out-of-range
                if (c == 0xF0 && c2 < 0x90)
                    return false;  // Overlong
                if (c == 0xF4 && c2 >= 0x90)
                    return false;  // > U+10FFFF
                if (c > 0xF4)
                    return false;  // > U+10FFFF
                pos += 3;
            } else {
                // Invalid UTF-8
                return false;
            }
        }
        continue;
    }

    // Scalar validation for remaining bytes
    while (pos < end_pos) {
        unsigned char c = static_cast<unsigned char>(data[pos]);

        if (c < 0x80) {
            // ASCII
            pos++;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (pos + 1 >= end_pos)
                return false;
            unsigned char c2 = static_cast<unsigned char>(data[pos + 1]);
            if ((c2 & 0xC0) != 0x80)
                return false;
            if (c < 0xC2)
                return false;  // Overlong
            pos += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (pos + 2 >= end_pos)
                return false;
            unsigned char c2 = static_cast<unsigned char>(data[pos + 1]);
            unsigned char c3 = static_cast<unsigned char>(data[pos + 2]);
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
                return false;
            if (c == 0xE0 && c2 < 0xA0)
                return false;  // Overlong
            if (c == 0xED && c2 >= 0xA0)
                return false;  // Surrogate
            pos += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (pos + 3 >= end_pos)
                return false;
            unsigned char c2 = static_cast<unsigned char>(data[pos + 1]);
            unsigned char c3 = static_cast<unsigned char>(data[pos + 2]);
            unsigned char c4 = static_cast<unsigned char>(data[pos + 3]);
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80)
                return false;
            if (c == 0xF0 && c2 < 0x90)
                return false;  // Overlong
            if (c == 0xF4 && c2 >= 0x90)
                return false;  // > U+10FFFF
            if (c > 0xF4)
                return false;  // > U+10FFFF
            pos += 4;
        } else {
            return false;
        }
    }

    return true;
}

// Scalar UTF-8 validation fallback
static auto validate_utf8_scalar(std::span<const char> data, size_t start_pos, size_t end_pos)
    -> bool {
    size_t pos = start_pos;

    while (pos < end_pos) {
        unsigned char c = static_cast<unsigned char>(data[pos]);

        if (c < 0x80) {
            pos++;
        } else if ((c & 0xE0) == 0xC0) {
            if (pos + 1 >= end_pos)
                return false;
            unsigned char c2 = static_cast<unsigned char>(data[pos + 1]);
            if ((c2 & 0xC0) != 0x80 || c < 0xC2)
                return false;
            pos += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (pos + 2 >= end_pos)
                return false;
            unsigned char c2 = static_cast<unsigned char>(data[pos + 1]);
            unsigned char c3 = static_cast<unsigned char>(data[pos + 2]);
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
                return false;
            if (c == 0xE0 && c2 < 0xA0)
                return false;
            if (c == 0xED && c2 >= 0xA0)
                return false;
            pos += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (pos + 3 >= end_pos)
                return false;
            unsigned char c2 = static_cast<unsigned char>(data[pos + 1]);
            unsigned char c3 = static_cast<unsigned char>(data[pos + 2]);
            unsigned char c4 = static_cast<unsigned char>(data[pos + 3]);
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80)
                return false;
            if (c == 0xF0 && c2 < 0x90)
                return false;
            if (c == 0xF4 && c2 >= 0x90)
                return false;
            if (c > 0xF4)
                return false;
            pos += 4;
        } else {
            return false;
        }
    }

    return true;
}

#else  // Non-x86 platforms

static auto skip_whitespace_simd(std::span<const char> data, size_t start_pos) -> size_t {
    size_t pos = start_pos;
    while (pos < data.size()
           && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\n' || data[pos] == '\r')) {
        pos++;
    }
    return pos;
}

static auto find_string_end_avx2(std::span<const char> data, size_t start_pos) -> size_t {
    size_t pos = start_pos;
    while (pos < data.size()) {
        char c = data[pos];
        if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
            return pos;
        }
        pos++;
    }
    return pos;
}

static auto validate_number_chars_avx2(std::span<const char> data, size_t start_pos, size_t end_pos)
    -> bool {
    for (size_t pos = start_pos; pos < end_pos; ++pos) {
        char c = data[pos];
        if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')) {
            return false;
        }
    }
    return true;
}

static auto match_literal_sse2(std::span<const char> data, size_t pos, const char* literal,
                               size_t len) -> bool {
    if (pos + len > data.size()) {
        return false;
    }
    return std::memcmp(data.data() + pos, literal, len) == 0;
}

#endif

// ============================================================================
// Parser Implementation
// ============================================================================

class parser {
public:
    parser(std::string_view input);
    auto parse() -> json_result<json_value>;

private:
    auto parse_value() -> json_result<json_value>;
    auto parse_null() -> json_result<json_value>;
    auto parse_boolean() -> json_result<json_value>;
    auto parse_number() -> json_result<json_value>;
    auto parse_string() -> json_result<json_value>;
    auto parse_array() -> json_result<json_value>;
    auto parse_array_parallel(size_t estimated_size) -> json_result<json_value>;
    auto parse_object() -> json_result<json_value>;
    auto parse_object_parallel(size_t estimated_size) -> json_result<json_value>;

    auto skip_whitespace() -> void;
    auto peek() const noexcept -> char;
    auto advance() noexcept -> char;
    auto match(char expected) noexcept -> bool;
    auto is_at_end() const noexcept -> bool;
    auto make_error(json_error_code code, std::string message) const -> json_error;
    auto current_ptr() const noexcept -> const char*;

    std::string_view input_;
    size_t pos_;
    size_t line_;
    size_t column_;
    size_t depth_;
};

parser::parser(std::string_view input) : input_(input), pos_(0), line_(1), column_(1), depth_(0) {}

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

auto parser::current_ptr() const noexcept -> const char* {
    return input_.data() + pos_;
}

auto parser::skip_whitespace() -> void {
    pos_ = skip_whitespace_simd(std::span<const char>(input_.data(), input_.size()), pos_);
}

auto parser::peek() const noexcept -> char {
    return is_at_end() ? '\0' : input_[pos_];
}

auto parser::advance() noexcept -> char {
    if (is_at_end())
        return '\0';
    char c = input_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

auto parser::match(char expected) noexcept -> bool {
    if (is_at_end() || input_[pos_] != expected)
        return false;
    advance();
    return true;
}

auto parser::is_at_end() const noexcept -> bool {
    return pos_ >= input_.size();
}

auto parser::make_error(json_error_code code, std::string message) const -> json_error {
    return json_error{code, std::move(message), line_, column_};
}

auto parser::parse_value() -> json_result<json_value> {
    if (depth_ >= g_config.max_depth) {
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
    // SIMD literal matching for "null"
    if (g_config.enable_simd && g_config.enable_sse42 && g_simd_caps.sse2) {
        if (match_literal_sse2(input_, pos_, "null", 4)) {
            pos_ += 4;
            return json_value{};
        }
        return std::unexpected(
            make_error(json_error_code::invalid_literal, "Invalid null literal"));
    }

    // Fallback scalar matching
    if (!match('n') || !match('u') || !match('l') || !match('l')) {
        return std::unexpected(
            make_error(json_error_code::invalid_literal, "Invalid null literal"));
    }
    return json_value{};
}

auto parser::parse_boolean() -> json_result<json_value> {
    // SIMD literal matching for "true" and "false"
    if (g_config.enable_simd && g_config.enable_sse42 && g_simd_caps.sse2) {
        if (peek() == 't') {
            if (match_literal_sse2(input_, pos_, "true", 4)) {
                pos_ += 4;
                return json_value{true};
            }
            return std::unexpected(
                make_error(json_error_code::invalid_literal, "Invalid true literal"));
        } else if (peek() == 'f') {
            if (match_literal_sse2(input_, pos_, "false", 5)) {
                pos_ += 5;
                return json_value{false};
            }
            return std::unexpected(
                make_error(json_error_code::invalid_literal, "Invalid false literal"));
        }
        return std::unexpected(
            make_error(json_error_code::invalid_literal, "Invalid boolean literal"));
    }

    // Fallback scalar matching
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
    size_t start_pos = pos_;
    bool is_negative = false;
    bool has_decimal = false;
    bool has_exponent = false;

    if (peek() == '-') {
        is_negative = true;
        advance();
    }

    if (peek() == '0') {
        advance();
    } else if (peek() >= '1' && peek() <= '9') {
        advance();
        while (peek() >= '0' && peek() <= '9')
            advance();
    } else {
        return std::unexpected(
            make_error(json_error_code::invalid_number, "Invalid number format"));
    }

    if (peek() == '.') {
        has_decimal = true;
        advance();
        if (!(peek() >= '0' && peek() <= '9')) {
            return std::unexpected(
                make_error(json_error_code::invalid_number, "Invalid decimal number"));
        }
        while (peek() >= '0' && peek() <= '9')
            advance();
    }

    if (peek() == 'e' || peek() == 'E') {
        has_exponent = true;
        advance();
        if (peek() == '+' || peek() == '-')
            advance();
        if (!(peek() >= '0' && peek() <= '9')) {
            return std::unexpected(make_error(json_error_code::invalid_number, "Invalid exponent"));
        }
        while (peek() >= '0' && peek() <= '9')
            advance();
    }

    // Use string_view directly
    std::string_view number_str = input_.substr(start_pos, pos_ - start_pos);
    size_t length = pos_ - start_pos;
    size_t digit_count = length - (is_negative ? 1 : 0);

    // For pure integers (no decimal/exponent) that may exceed int64 range,
    // try to parse as 128-bit integer if digit count suggests overflow
    // int64 max is ~19 digits (9223372036854775807)
    // __int128 max is ~39 digits
    if (!has_decimal && !has_exponent && digit_count >= 19) {
        // Try parsing as 128-bit integer
        thread_local std::array<char, 128> int_buffer;
        size_t int_len = std::min(length, int_buffer.size() - 1);
        std::memcpy(int_buffer.data(), input_.data() + start_pos, int_len);
        int_buffer[int_len] = '\0';

        // Parse manually for 128-bit
        const char* p = int_buffer.data();
        bool neg = (*p == '-');
        if (neg) p++;

        unsigned __int128 result = 0;
        bool overflow = false;
        while (*p >= '0' && *p <= '9') {
            unsigned __int128 prev = result;
            result = result * 10 + (*p - '0');
            if (result < prev) { overflow = true; break; }
            p++;
        }

        if (!overflow) {
            if (neg) {
                // Return signed 128-bit
                return json_value{static_cast<__int128>(-static_cast<__int128>(result))};
            } else {
                // For very large positive numbers, use unsigned if it exceeds signed range
                if (result > static_cast<unsigned __int128>(std::numeric_limits<__int128>::max())) {
                    return json_value{result};  // unsigned __int128
                }
                return json_value{static_cast<__int128>(result)};
            }
        }
        // On overflow (> 128 bits), fall through to double parsing (will lose precision)
    }

    thread_local std::array<char, 64> buffer;
    size_t buf_len = std::min(length, buffer.size() - 1);
    std::memcpy(buffer.data(), input_.data() + start_pos, buf_len);
    buffer[buf_len] = '\0';

    char* end_ptr;
    double value = std::strtod(buffer.data(), &end_ptr);

    if (end_ptr != buffer.data() + buf_len) {
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

    std::string value;
    value.reserve(64);

// Use SIMD to quickly scan for quotes, escapes, or control chars
#if defined(__x86_64__) || defined(_M_X64)
    if (g_config.enable_simd && g_config.enable_avx2 && g_simd_caps.avx2) {
        size_t string_start = pos_;

        while (!is_at_end()) {
            size_t special_pos = find_string_end_avx2(input_, pos_);
#elif defined(__aarch64__) || defined(_M_ARM64)
    if (g_config.enable_simd && g_config.enable_neon && g_simd_caps.neon) {
        size_t string_start = pos_;

        while (!is_at_end()) {
            size_t special_pos = find_string_end_neon(input_, pos_);
#else
    if (false) {
        size_t string_start = pos_;

        while (!is_at_end()) {
            size_t special_pos = pos_;  // Dummy
#endif

            // Copy all the normal characters at once
            if (special_pos > pos_) {
                value.append(input_.data() + pos_, special_pos - pos_);
                pos_ = special_pos;
            }

            if (is_at_end()) {
                break;
            }

            char c = peek();

            if (c == '"') {
                // Found end quote - validate UTF-8 before returning
                advance();

                // Validate UTF-8 encoding of the complete string
                bool is_valid_utf8;
#if defined(__x86_64__) || defined(_M_X64)
                if (g_config.enable_simd && g_config.enable_avx2 && g_simd_caps.avx2) {
                    is_valid_utf8 = validate_utf8_avx2(
                        std::span<const char>(value.data(), value.size()), 0, value.size());
                } else {
                    is_valid_utf8 = validate_utf8_scalar(
                        std::span<const char>(value.data(), value.size()), 0, value.size());
                }
#elif defined(__aarch64__) || defined(_M_ARM64)
                if (g_config.enable_simd && g_config.enable_neon && g_simd_caps.neon) {
                    is_valid_utf8 = validate_utf8_neon(
                        std::span<const char>(value.data(), value.size()), 0, value.size());
                } else {
                    is_valid_utf8 = validate_utf8_scalar(
                        std::span<const char>(value.data(), value.size()), 0, value.size());
                }
#else
                is_valid_utf8 = validate_utf8_scalar(
                    std::span<const char>(value.data(), value.size()), 0, value.size());
#endif

                if (!is_valid_utf8) {
                    return std::unexpected(make_error(json_error_code::invalid_string,
                                                      "Invalid UTF-8 encoding in string"));
                }

                return json_value{std::move(value)};
            } else if (c == '\\') {
                // Handle escape sequence
                advance();  // Skip backslash

                if (is_at_end()) {
                    return std::unexpected(make_error(json_error_code::invalid_string,
                                                      "Unterminated escape sequence"));
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
                        // Use unicode module for proper UTF-16 surrogate pair handling
                        size_t remaining = input_.size() - pos_;
                        auto result = fastjson::unicode::decode_json_unicode_escape(input_.data() + pos_,
                                                                          remaining, value);

                        if (!result.has_value()) {
                            return std::unexpected(
                                make_error(json_error_code::invalid_unicode, result.error()));
                        }

                        // Advance position by number of bytes consumed
                        pos_ += result.value();
                        break;
                    }
                    default:
                        return std::unexpected(
                            make_error(json_error_code::invalid_escape, "Invalid escape sequence"));
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                return std::unexpected(
                    make_error(json_error_code::invalid_string, "Control character in string"));
            }

            if (value.size() > g_config.max_string_length) {
                return std::unexpected(
                    make_error(json_error_code::invalid_string, "String exceeds maximum length"));
            }
        }

        return std::unexpected(make_error(json_error_code::invalid_string, "Unterminated string"));
    }

    // Fallback: scalar string parsing
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
                    // Use unicode module for proper UTF-16 surrogate pair handling
                    size_t remaining = input_.size() - pos_;
                    auto result =
                        fastjson::unicode::decode_json_unicode_escape(input_.data() + pos_, remaining, value);

                    if (!result.has_value()) {
                        return std::unexpected(
                            make_error(json_error_code::invalid_unicode, result.error()));
                    }

                    // Advance position by number of bytes consumed
                    pos_ += result.value();
                    break;
                }
                default:
                    return std::unexpected(
                        make_error(json_error_code::invalid_escape, "Invalid escape sequence"));
            }
        } else if (static_cast<unsigned char>(c) < 0x20) {
            return std::unexpected(
                make_error(json_error_code::invalid_string, "Control character in string"));
        } else {
            value += c;
        }

        if (value.size() > g_config.max_string_length) {
            return std::unexpected(
                make_error(json_error_code::invalid_string, "String exceeds maximum length"));
        }
    }

    if (!match('"')) {
        return std::unexpected(make_error(json_error_code::invalid_string, "Unterminated string"));
    }

    // Validate UTF-8 encoding of the complete string
    bool is_valid_utf8 =
        validate_utf8_scalar(std::span<const char>(value.data(), value.size()), 0, value.size());

    if (!is_valid_utf8) {
        return std::unexpected(
            make_error(json_error_code::invalid_string, "Invalid UTF-8 encoding in string"));
    }

    return json_value{std::move(value)};
}

auto parser::parse_array() -> json_result<json_value> {
    if (!match('[')) {
        return std::unexpected(make_error(json_error_code::invalid_syntax, "Expected '['"));
    }

    ++depth_;

    skip_whitespace();

    if (match(']')) {
        --depth_;
        return json_value{json_array{}};
    }

    // ALWAYS try parallel version - it will decide internally whether to fall back to sequential
    return parse_array_parallel(0);  // 0 = unknown size, will be determined during scan
}

// SIMD-accelerated structural character scanner for arrays
__attribute__((target("avx2"))) static auto
scan_array_boundaries_simd(std::span<const char> input, size_t start_pos,
                           std::vector<std::pair<size_t, size_t>>& elements, size_t& array_end)
    -> bool {
    // Use AVX2 to quickly find structural characters: []{},"
    const __m256i left_bracket = _mm256_set1_epi8('[');
    const __m256i right_bracket = _mm256_set1_epi8(']');
    const __m256i left_brace = _mm256_set1_epi8('{');
    const __m256i right_brace = _mm256_set1_epi8('}');
    const __m256i comma = _mm256_set1_epi8(',');
    const __m256i quote = _mm256_set1_epi8('"');
    const __m256i backslash = _mm256_set1_epi8('\\');

    size_t pos = start_pos;
    size_t element_start = pos;
    int depth = 0;
    bool in_string = false;

    // Process 32 bytes at a time with AVX2
    while (pos + 32 <= input.size()) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input.data() + pos));

        // Find all structural characters and quotes
        __m256i is_left_bracket = _mm256_cmpeq_epi8(chunk, left_bracket);
        __m256i is_right_bracket = _mm256_cmpeq_epi8(chunk, right_bracket);
        __m256i is_left_brace = _mm256_cmpeq_epi8(chunk, left_brace);
        __m256i is_right_brace = _mm256_cmpeq_epi8(chunk, right_brace);
        __m256i is_comma = _mm256_cmpeq_epi8(chunk, comma);
        __m256i is_quote = _mm256_cmpeq_epi8(chunk, quote);
        __m256i is_backslash = _mm256_cmpeq_epi8(chunk, backslash);

        // Combine into mask
        __m256i structural =
            _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(is_left_bracket, is_right_bracket),
                                            _mm256_or_si256(is_left_brace, is_right_brace)),
                            _mm256_or_si256(_mm256_or_si256(is_comma, is_quote), is_backslash));

        uint32_t mask = _mm256_movemask_epi8(structural);

        if (mask == 0) {
            // No structural characters in this chunk - fast path
            pos += 32;
            continue;
        }

        // Process each structural character found
        for (int bit = 0; bit < 32 && mask; ++bit, mask >>= 1) {
            if (!(mask & 1))
                continue;

            size_t char_pos = pos + bit;
            char c = input[char_pos];

            if (in_string) {
                if (c == '\\' && char_pos + 1 < input.size()) {
                    // Skip escape sequence
                    bit++;
                    mask >>= 1;
                } else if (c == '"') {
                    in_string = false;
                }
            } else {
                switch (c) {
                    case '"':
                        in_string = true;
                        break;
                    case '[':
                    case '{':
                        depth++;
                        break;
                    case ']':
                        if (depth == 0) {
                            // Found array end
                            if (element_start < char_pos) {
                                elements.push_back({element_start, char_pos});
                            }
                            array_end = char_pos;
                            return true;
                        }
                        depth--;
                        break;
                    case '}':
                        depth--;
                        break;
                    case ',':
                        if (depth == 0) {
                            // Found element boundary
                            elements.push_back({element_start, char_pos});
                            element_start = char_pos + 1;
                        }
                        break;
                }
            }
        }

        pos += 32;
    }

    // Process remaining bytes with scalar code
    while (pos < input.size()) {
        char c = input[pos];

        if (in_string) {
            if (c == '\\' && pos + 1 < input.size()) {
                pos += 2;
                continue;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            switch (c) {
                case '"':
                    in_string = true;
                    break;
                case '[':
                case '{':
                    depth++;
                    break;
                case ']':
                    if (depth == 0) {
                        if (element_start < pos) {
                            elements.push_back({element_start, pos});
                        }
                        array_end = pos;
                        return true;
                    }
                    depth--;
                    break;
                case '}':
                    depth--;
                    break;
                case ',':
                    if (depth == 0) {
                        elements.push_back({element_start, pos});
                        element_start = pos + 1;
                    }
                    break;
            }
        }
        pos++;
    }

    return false;
}

auto parser::parse_array_parallel(size_t estimated_size) -> json_result<json_value> {
    // Phase 1: Find element boundaries using SIMD-accelerated scanning

    struct element_span {
        size_t start;
        size_t end;
    };

    std::vector<element_span> element_spans;
    element_spans.reserve(estimated_size);
    size_t array_end_pos = pos_;

    // Try SIMD-accelerated scan first (AVX2)
    bool use_simd = g_config.enable_simd && g_config.enable_avx2 && g_simd_caps.avx2;

    if (use_simd) {
        std::vector<std::pair<size_t, size_t>> simd_elements;
        if (scan_array_boundaries_simd(input_, pos_, simd_elements, array_end_pos)) {
            // Convert to element_span format
            for (const auto& [start, end] : simd_elements) {
                element_spans.push_back({start, end});
            }
            goto parallel_parse;
        }
        // If SIMD scan failed, fall through to scalar scan
    }

    // Fallback: Direct scan for element boundaries (respects nesting and strings)
    {
        size_t scan_pos = pos_;
        int depth = 0;
        bool in_string = false;
        bool escape_next = false;
        size_t element_start = scan_pos;
        bool found_end = false;

        while (scan_pos < input_.size() && !found_end) {
            char c = input_[scan_pos];

            if (in_string) {
                if (escape_next) {
                    escape_next = false;
                } else if (c == '\\') {
                    escape_next = true;
                } else if (c == '"') {
                    in_string = false;
                }
            } else {
                switch (c) {
                    case '"':
                        in_string = true;
                        break;
                    case '[':
                    case '{':
                        depth++;
                        break;
                    case ']':
                        if (depth == 0) {
                            // End of array - add final element if we have one
                            if (element_start < scan_pos) {
                                element_spans.push_back({element_start, scan_pos});
                            }
                            array_end_pos = scan_pos;
                            found_end = true;
                        } else {
                            depth--;
                        }
                        break;
                    case '}':
                        depth--;
                        break;
                    case ',':
                        if (depth == 0) {
                            // Found element boundary at this level
                            element_spans.push_back({element_start, scan_pos});
                            element_start = scan_pos + 1;
                        }
                        break;
                }
            }
            if (!found_end) {
                scan_pos++;
            }
        }
    }

parallel_parse:
// If we found no elements or very few, fall back to sequential
// Debug: show when we use parallel vs sequential
#ifdef DEBUG_PARALLEL
    auto& log = logger::Logger::getInstance();
    log.debug("Array with {elements} elements, threshold={threshold}, using {mode}",
              logger::Logger::named_args(
                  "elements", element_spans.size(), "threshold", g_config.parallel_threshold / 100,
                  "mode",
                  (element_spans.size() >= (g_config.parallel_threshold / 100) ? "PARALLEL"
                                                                               : "SEQUENTIAL")));
#endif

    if (element_spans.size() < static_cast<size_t>(g_config.parallel_threshold / 100)) {
        // Sequential fallback: parse elements one by one
        json_array array;
        array.reserve(element_spans.size());

        for (auto& span : element_spans) {
            std::string_view element_input = input_.substr(span.start, span.end - span.start);

            // Trim whitespace
            while (!element_input.empty() && std::isspace(element_input.front())) {
                element_input.remove_prefix(1);
            }
            while (!element_input.empty() && std::isspace(element_input.back())) {
                element_input.remove_suffix(1);
            }

            parser element_parser(element_input);
            auto result = element_parser.parse_value();

            if (!result) {
                --depth_;
                return std::unexpected(result.error());
            }

            array.emplace_back(std::move(*result));
        }

        pos_ = array_end_pos + 1;  // +1 to skip the ']'
        --depth_;
        return json_value{std::move(array)};
    }

    // Phase 2: Parse elements in parallel using OpenMP
    json_array array;
    array.resize(element_spans.size());

    std::atomic<bool> has_error{false};
    json_error first_error{};

    // Initialize NUMA topology if enabled (once per process)
    if (g_config.enable_numa && !g_numa_initialized.load(std::memory_order_acquire)) {
        bool expected = false;
        if (g_numa_initialized.compare_exchange_strong(expected, true)) {
            g_numa_topo = fastjson::numa::detect_numa_topology();
        }
    }

#pragma omp parallel for schedule(dynamic) if (element_spans.size() >= 4)
    for (size_t i = 0; i < element_spans.size(); ++i) {
        // Bind thread to NUMA node on first iteration
        if (g_config.enable_numa && g_config.bind_threads_to_numa
            && g_numa_topo.is_numa_available) {
#ifdef _OPENMP
            static thread_local bool thread_bound = false;
            if (!thread_bound) {
                int thread_id = omp_get_thread_num();
                int num_threads = omp_get_num_threads();
                int node = fastjson::numa::get_optimal_node_for_thread(thread_id, num_threads,
                                                             g_numa_topo.num_nodes);
                fastjson::numa::bind_thread_to_numa_node(node);
                thread_bound = true;
            }
#endif
        }

        if (has_error.load(std::memory_order_relaxed)) {
            continue;  // Skip if another thread hit an error
        }

        // Prefetch next element's data (3-4 cache lines ahead)
        if (i + 3 < element_spans.size()) {
            const auto& next_span = element_spans[i + 3];
            __builtin_prefetch(input_.data() + next_span.start, 0,
                               3);  // Read, high temporal locality
            // Prefetch middle of the span too if it's large
            if (next_span.end - next_span.start > 128) {
                __builtin_prefetch(input_.data() + (next_span.start + next_span.end) / 2, 0, 3);
            }
        }

        // Create a thread-local parser for this element
        auto& span = element_spans[i];
        std::string_view element_input = input_.substr(span.start, span.end - span.start);

        // Trim whitespace from element
        while (!element_input.empty() && std::isspace(element_input.front())) {
            element_input.remove_prefix(1);
        }
        while (!element_input.empty() && std::isspace(element_input.back())) {
            element_input.remove_suffix(1);
        }

        parser element_parser(element_input);
        auto result = element_parser.parse_value();

        if (!result) {
            bool expected = false;
            if (has_error.compare_exchange_strong(expected, true)) {
                first_error = result.error();
            }
        } else {
            array[i] = std::move(*result);
        }
    }

    if (has_error) {
        --depth_;
        return std::unexpected(first_error);
    }

    // Update parser position to after the array
    pos_ = array_end_pos + 1;  // +1 to skip the ']'

    --depth_;
    return json_value{std::move(array)};
}

// Sequential fallback is handled by calling parse_array() directly

auto parser::parse_object() -> json_result<json_value> {
    if (!match('{')) {
        return std::unexpected(make_error(json_error_code::invalid_syntax, "Expected '{'"));
    }

    ++depth_;

    skip_whitespace();

    if (match('}')) {
        --depth_;
        return json_value{json_object{}};
    }

    json_object object;

    while (true) {
        skip_whitespace();

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

        if (!match(':')) {
            --depth_;
            return std::unexpected(
                make_error(json_error_code::invalid_syntax, "Expected ':' after object key"));
        }

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

// SIMD-accelerated object boundary scanner
__attribute__((target("avx2"))) static auto
scan_object_boundaries_simd(std::span<const char> input, size_t start_pos,
                            std::vector<std::tuple<size_t, size_t, size_t, size_t>>& kv_pairs,
                            size_t& object_end) -> bool {
    // Use AVX2 to quickly find structural characters
    const __m256i left_brace = _mm256_set1_epi8('{');
    const __m256i right_brace = _mm256_set1_epi8('}');
    const __m256i left_bracket = _mm256_set1_epi8('[');
    const __m256i right_bracket = _mm256_set1_epi8(']');
    const __m256i comma = _mm256_set1_epi8(',');
    const __m256i colon = _mm256_set1_epi8(':');
    const __m256i quote = _mm256_set1_epi8('"');
    const __m256i backslash = _mm256_set1_epi8('\\');

    size_t pos = start_pos;
    size_t key_start = 0, key_end = 0, value_start = 0;
    int depth = 0;
    bool in_string = false;
    enum class state_t { need_key, need_colon, need_value, need_comma };
    state_t state = state_t::need_key;

    // Process 32 bytes at a time
    while (pos + 32 <= input.size()) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input.data() + pos));

        __m256i is_left_brace = _mm256_cmpeq_epi8(chunk, left_brace);
        __m256i is_right_brace = _mm256_cmpeq_epi8(chunk, right_brace);
        __m256i is_left_bracket = _mm256_cmpeq_epi8(chunk, left_bracket);
        __m256i is_right_bracket = _mm256_cmpeq_epi8(chunk, right_bracket);
        __m256i is_comma = _mm256_cmpeq_epi8(chunk, comma);
        __m256i is_colon = _mm256_cmpeq_epi8(chunk, colon);
        __m256i is_quote = _mm256_cmpeq_epi8(chunk, quote);
        __m256i is_backslash = _mm256_cmpeq_epi8(chunk, backslash);

        __m256i structural =
            _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(is_left_brace, is_right_brace),
                                            _mm256_or_si256(is_left_bracket, is_right_bracket)),
                            _mm256_or_si256(_mm256_or_si256(is_comma, is_colon),
                                            _mm256_or_si256(is_quote, is_backslash)));

        uint32_t mask = _mm256_movemask_epi8(structural);

        if (mask == 0) {
            pos += 32;
            continue;
        }

        // Process structural characters
        for (int bit = 0; bit < 32 && mask; ++bit, mask >>= 1) {
            if (!(mask & 1))
                continue;

            size_t char_pos = pos + bit;
            char c = input[char_pos];

            if (in_string) {
                if (c == '\\' && char_pos + 1 < input.size()) {
                    bit++;
                    mask >>= 1;
                } else if (c == '"') {
                    in_string = false;
                    if (state == state_t::need_key) {
                        key_end = char_pos;
                        state = state_t::need_colon;
                    }
                }
            } else {
                switch (c) {
                    case '"':
                        in_string = true;
                        if (state == state_t::need_key) {
                            key_start = char_pos + 1;
                        }
                        break;
                    case ':':
                        if (state == state_t::need_colon && depth == 0) {
                            value_start = char_pos + 1;
                            state = state_t::need_value;
                        }
                        break;
                    case '[':
                    case '{':
                        if (state == state_t::need_value) {
                            depth++;
                        }
                        break;
                    case ']':
                    case '}':
                        if (state == state_t::need_value && depth > 0) {
                            depth--;
                        } else if (c == '}' && depth == 0 && state == state_t::need_comma) {
                            object_end = char_pos;
                            return true;
                        } else if (c == '}' && depth == 0) {
                            // End of last value
                            if (state == state_t::need_value) {
                                kv_pairs.push_back({key_start, key_end, value_start, char_pos});
                            }
                            object_end = char_pos;
                            return true;
                        }
                        break;
                    case ',':
                        if (depth == 0 && state == state_t::need_comma) {
                            state = state_t::need_key;
                        } else if (depth == 0 && state == state_t::need_value) {
                            kv_pairs.push_back({key_start, key_end, value_start, char_pos});
                            state = state_t::need_key;
                        }
                        break;
                }

                // Transition from need_value to need_comma
                if (state == state_t::need_value && depth == 0 && c != ':' && c != '[' && c != '{'
                    && c != '"' && !std::isspace(c)) {
                    // We're in a value (primitive), scan to end
                    size_t value_end = char_pos;
                    while (value_end < input.size() && input[value_end] != ','
                           && input[value_end] != '}') {
                        value_end++;
                    }
                    if (value_end < input.size()) {
                        kv_pairs.push_back({key_start, key_end, value_start, value_end});
                        state = state_t::need_comma;
                        pos = value_end - 1;
                        break;
                    }
                }
            }
        }

        pos += 32;
    }

    // Scalar fallback for remaining bytes
    while (pos < input.size()) {
        char c = input[pos];

        if (in_string) {
            if (c == '\\' && pos + 1 < input.size()) {
                pos += 2;
                continue;
            } else if (c == '"') {
                in_string = false;
                if (state == state_t::need_key) {
                    key_end = pos;
                    state = state_t::need_colon;
                }
            }
        } else {
            if (!std::isspace(c)) {
                switch (c) {
                    case '"':
                        in_string = true;
                        if (state == state_t::need_key) {
                            key_start = pos + 1;
                        }
                        break;
                    case ':':
                        if (state == state_t::need_colon && depth == 0) {
                            value_start = pos + 1;
                            state = state_t::need_value;
                        }
                        break;
                    case '[':
                    case '{':
                        if (state == state_t::need_value) {
                            depth++;
                        }
                        break;
                    case ']':
                    case '}':
                        if (state == state_t::need_value && depth > 0) {
                            depth--;
                        } else if (c == '}' && depth == 0) {
                            if (state == state_t::need_value) {
                                kv_pairs.push_back({key_start, key_end, value_start, pos});
                            }
                            object_end = pos;
                            return true;
                        }
                        break;
                    case ',':
                        if (depth == 0 && state == state_t::need_value) {
                            kv_pairs.push_back({key_start, key_end, value_start, pos});
                            state = state_t::need_key;
                        }
                        break;
                }
            }
        }
        pos++;
    }

    return false;
}

auto parser::parse_object_parallel(size_t estimated_size) -> json_result<json_value> {
    // Phase 1: Scan to find key-value pair boundaries using SIMD
    struct kv_span {
        size_t key_start;
        size_t key_end;
        size_t value_start;
        size_t value_end;
    };

    std::vector<kv_span> kv_spans;
    kv_spans.reserve(estimated_size);
    size_t object_end_pos = pos_;

    // Try SIMD-accelerated scan first
    bool use_simd = g_config.enable_simd && g_config.enable_avx2 && g_simd_caps.avx2;
    bool simd_success = false;

    if (use_simd) {
        std::vector<std::tuple<size_t, size_t, size_t, size_t>> simd_kvs;
        if (scan_object_boundaries_simd(input_, pos_, simd_kvs, object_end_pos)) {
            for (const auto& [ks, ke, vs, ve] : simd_kvs) {
                kv_spans.push_back({ks, ke, vs, ve});
            }
            simd_success = true;
        }
    }

    // Fallback: Scan forward to find key-value boundaries
    if (!simd_success) {
        size_t scan_pos = pos_;
        int depth = 0;
        bool in_string = false;
        bool escape_next = false;

        size_t key_start = 0, key_end = 0, value_start = 0;
        enum class scan_state { need_key, need_colon, need_value, need_comma };
        scan_state state = scan_state::need_key;

        while (scan_pos < input_.size()) {
            char c = input_[scan_pos];

            if (in_string) {
                if (escape_next) {
                    escape_next = false;
                } else if (c == '\\') {
                    escape_next = true;
                } else if (c == '"') {
                    in_string = false;
                    if (state == scan_state::need_key) {
                        key_end = scan_pos;
                        state = scan_state::need_colon;
                    }
                }
            } else {
                if (std::isspace(c)) {
                    scan_pos++;
                    continue;
                }

                switch (c) {
                    case '"':
                        in_string = true;
                        if (state == scan_state::need_key) {
                            key_start = scan_pos + 1;
                        }
                        break;
                    case ':':
                        if (state == scan_state::need_colon && depth == 0) {
                            value_start = scan_pos + 1;
                            state = scan_state::need_value;
                        }
                        break;
                    case '[':
                    case '{':
                        depth++;
                        break;
                    case ']':
                        depth--;
                        break;
                    case '}':
                        if (depth == 0) {
                            // End of object
                            if (state == scan_state::need_value
                                || state == scan_state::need_comma) {
                                kv_spans.push_back({key_start, key_end, value_start, scan_pos});
                            }
                            object_end_pos = scan_pos;
                            break;
                        }
                        depth--;
                        break;
                    case ',':
                        if (depth == 0 && state == scan_state::need_value) {
                            kv_spans.push_back({key_start, key_end, value_start, scan_pos});
                            state = scan_state::need_key;
                        }
                        break;
                }
            }
            scan_pos++;
        }
    }  // End of if (!simd_success) block

    // If we found very few pairs, fall back to sequential
    if (kv_spans.size() < static_cast<size_t>(g_config.parallel_threshold / 100)) {
        return parse_object();  // Use sequential version
    }

    // Phase 2: Parse key-value pairs in parallel
    json_object object;

    // We need to use a vector of pairs since unordered_map isn't thread-safe for concurrent inserts
    std::vector<std::pair<std::string, json_value>> pairs(kv_spans.size());

    std::atomic<bool> has_error{false};
    json_error first_error{};

#pragma omp parallel for schedule(dynamic) if (kv_spans.size() >= 4)
    for (size_t i = 0; i < kv_spans.size(); ++i) {
        // Bind thread to NUMA node on first iteration
        if (g_config.enable_numa && g_config.bind_threads_to_numa
            && g_numa_topo.is_numa_available) {
#ifdef _OPENMP
            static thread_local bool thread_bound = false;
            if (!thread_bound) {
                int thread_id = omp_get_thread_num();
                int num_threads = omp_get_num_threads();
                int node = fastjson::numa::get_optimal_node_for_thread(thread_id, num_threads,
                                                             g_numa_topo.num_nodes);
                fastjson::numa::bind_thread_to_numa_node(node);
                thread_bound = true;
            }
#endif
        }

        if (has_error.load(std::memory_order_relaxed)) {
            continue;
        }

        // Prefetch next key-value pair's data (3-4 items ahead)
        if (i + 3 < kv_spans.size()) {
            const auto& next_kv = kv_spans[i + 3];
            // Prefetch key and value locations
            __builtin_prefetch(input_.data() + next_kv.key_start, 0, 3);
            __builtin_prefetch(input_.data() + next_kv.value_start, 0, 3);
            // Prefetch middle of value if it's large (>2 cache lines)
            if (next_kv.value_end - next_kv.value_start > 128) {
                __builtin_prefetch(input_.data() + (next_kv.value_start + next_kv.value_end) / 2, 0,
                                   3);
            }
        }

        auto& span = kv_spans[i];

        // Extract key
        std::string key = std::string(input_.substr(span.key_start, span.key_end - span.key_start));

        // Parse value
        std::string_view value_input =
            input_.substr(span.value_start, span.value_end - span.value_start);

        // Trim whitespace
        while (!value_input.empty() && std::isspace(value_input.front())) {
            value_input.remove_prefix(1);
        }
        while (!value_input.empty() && std::isspace(value_input.back())) {
            value_input.remove_suffix(1);
        }

        parser value_parser(value_input);
        auto result = value_parser.parse_value();

        if (!result) {
            bool expected = false;
            if (has_error.compare_exchange_strong(expected, true)) {
                first_error = result.error();
            }
        } else {
            pairs[i] = {std::move(key), std::move(*result)};
        }
    }

    if (has_error) {
        --depth_;
        return std::unexpected(first_error);
    }

    // Move pairs into unordered_map
    for (auto& pair : pairs) {
        object[std::move(pair.first)] = std::move(pair.second);
    }

    // Update parser position
    pos_ = object_end_pos + 1;  // +1 to skip the '}'

    --depth_;
    return json_value{std::move(object)};
}

// ============================================================================
// Public API
// ============================================================================

export auto parse(std::string_view input) -> json_result<json_value>;
export auto parse(std::string_view input, text_encoding encoding) -> json_result<json_value>;
export auto parse_with_config(std::string_view input, const parse_config& config) -> json_result<json_value>;
export auto object() -> json_value;
export auto array() -> json_value;
export auto null() -> json_value;
export auto stringify(const json_value& v) -> std::string;
export auto prettify(const json_value& v, int indent) -> std::string;

// Functional utilities
export auto map(const json_value& arr, auto&& func) -> json_value;
export auto filter(const json_value& arr, auto&& pred) -> json_value;
export auto reduce(const json_value& arr, auto&& func, json_value init) -> json_value;
export auto all(const json_value& arr, auto&& pred) -> bool;
export auto any(const json_value& arr, auto&& pred) -> bool;
export auto zip(const json_value& arr1, const json_value& arr2) -> json_value;
export auto unzip(const json_value& arr) -> std::pair<json_value, json_value>;
export auto reverse(const json_value& arr) -> json_value;
export auto rotate(const json_value& arr, int n) -> json_value;
export auto take(const json_value& arr, size_t n) -> json_value;
export auto drop(const json_value& arr, size_t n) -> json_value;

auto parse(std::string_view input) -> json_result<json_value> {
    // Use encoding from config, or auto-detect
    auto encoding = g_config.input_encoding;
    if (encoding != text_encoding::UTF8 && encoding != text_encoding::AUTO_DETECT) {
        // Need to convert to UTF-8
        auto utf8_result = convert_to_utf8(input, encoding);
        if (!utf8_result) {
            return std::unexpected(json_error{json_error_code::invalid_syntax, utf8_result.error(), 0, 0});
        }
        parser p(utf8_result.value());
        return p.parse();
    } else if (encoding == text_encoding::AUTO_DETECT) {
        // Auto-detect and convert if needed
        auto utf8_result = convert_to_utf8(input, text_encoding::AUTO_DETECT);
        if (!utf8_result) {
            return std::unexpected(json_error{json_error_code::invalid_syntax, utf8_result.error(), 0, 0});
        }
        parser p(utf8_result.value());
        return p.parse();
    }

    // Already UTF-8
    parser p(input);
    return p.parse();
}

auto parse(std::string_view input, text_encoding encoding) -> json_result<json_value> {
    // Convert to UTF-8 if needed
    if (encoding != text_encoding::UTF8) {
        auto utf8_result = convert_to_utf8(input, encoding);
        if (!utf8_result) {
            return std::unexpected(json_error{json_error_code::invalid_syntax, utf8_result.error(), 0, 0});
        }
        parser p(utf8_result.value());
        return p.parse();
    }

    // Already UTF-8
    parser p(input);
    return p.parse();
}

auto parse_with_config(std::string_view input, const parse_config& config)
    -> json_result<json_value> {
    auto old_config = g_config;
    g_config = config;
    auto result = parse(input);
    g_config = old_config;
    return result;
}

// Helper functions for creating JSON values
auto object() -> json_value {
    return json_value{json_object{}};
}

auto array() -> json_value {
    return json_value{json_array{}};
}

auto null() -> json_value {
    return json_value{};
}

// Serialization implementations
auto json_value::serialize_to_buffer(std::string& buffer, int indent) const -> void {
    std::visit(
        [this, &buffer, indent](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_null>) buffer += "null";
            else if constexpr (std::is_same_v<T, json_boolean>) buffer += v ? "true" : "false";
            else if constexpr (std::is_same_v<T, json_number>) {
                char tmp[32];
                auto [ptr, ec] = std::to_chars(tmp, tmp + sizeof(tmp), v);
                if (ec == std::errc{}) buffer.append(tmp, ptr - tmp);
                else buffer += std::to_string(v);
            } else if constexpr (std::is_same_v<T, json_number_128>) {
                long double ld = static_cast<long double>(v);
                char tmp[128];
                auto [ptr, ec] = std::to_chars(tmp, tmp + sizeof(tmp), ld);
                if (ec == std::errc{}) buffer.append(tmp, ptr - tmp);
            } else if constexpr (std::is_same_v<T, json_int_128> || std::is_same_v<T, json_uint_128>) {
                if (v == 0) { buffer += "0"; return; }
                char tmp[64];
                char* p = tmp + 64;
                bool neg = false;
                unsigned __int128 uv;
                if constexpr (std::is_same_v<T, json_int_128>) {
                    if (v < 0) { neg = true; uv = (unsigned __int128)-v; }
                    else uv = (unsigned __int128)v;
                } else uv = v;
                while (uv > 0) { *--p = '0' + (uv % 10); uv /= 10; }
                if (neg) *--p = '-';
                buffer.append(p, (tmp + 64) - p);
            } else if constexpr (std::is_same_v<T, json_string>) {
                serialize_string_to_buffer(buffer, v);
            } else if constexpr (std::is_same_v<T, json_array_ptr>) {
                serialize_array_to_buffer(buffer, *v, indent);
            } else if constexpr (std::is_same_v<T, json_object_ptr>) {
                serialize_object_to_buffer(buffer, *v, indent);
            }
        }, data_);
}

auto json_value::serialize_string_to_buffer(std::string& buffer, const std::string& str) const -> void {
    buffer += '"';
    const char* data = str.data();
    const size_t size = str.size();
    const char* ptr = data;
    const char* end = data + size;

    while (ptr < end) {
        const char* escape_pos = ptr;

#if defined(HAVE_AVX2) || defined(HAVE_AVX512BW) || defined(HAVE_AMX_TILE)
        // Check for SIMD capabilities from g_simd_caps
        if (g_config.enable_simd) {
#ifdef HAVE_AVX512BW
            if (g_simd_caps.avx512bw) {
                while (escape_pos + 64 <= end) {
                    __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(escape_pos));
                    __mmask64 m1 = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('"'));
                    __mmask64 m2 = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\\'));
                    __mmask64 m3 = _mm512_cmp_epi8_mask(chunk, _mm512_set1_epi8(32), _MM_CMPINT_LT);
                    __mmask64 mask = m1 | m2 | m3;
                    if (mask != 0) { escape_pos += __builtin_ctzll(mask); goto handle_escape; }
                    escape_pos += 64;
                }
            }
#endif
#ifdef HAVE_AVX2
            if (g_simd_caps.avx2) {
                const __m256i v_quote = _mm256_set1_epi8('"');
                const __m256i v_backslash = _mm256_set1_epi8('\\');
                const __m256i v_space = _mm256_set1_epi8(31);
                while (escape_pos + 32 <= end) {
                    __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(escape_pos));
                    __m256i m1 = _mm256_cmpeq_epi8(chunk, v_quote);
                    __m256i m2 = _mm256_cmpeq_epi8(chunk, v_backslash);
                    __m256i m3 = _mm256_cmpgt_epi8(v_space, chunk);
                    int mask = _mm256_movemask_epi8(_mm256_or_si256(_mm256_or_si256(m1, m2), m3));
                    if (mask != 0) { escape_pos += __builtin_ctz(mask); goto handle_escape; }
                    escape_pos += 32;
                }
            }
#endif
        }
#endif

        while (escape_pos < end && *escape_pos != '"' && *escape_pos != '\\' && static_cast<unsigned char>(*escape_pos) >= 32) {
            ++escape_pos;
        }

    handle_escape:
        if (escape_pos > ptr) {
            buffer.append(ptr, escape_pos - ptr);
            ptr = escape_pos;
        }

        if (ptr < end) {
            char c = *ptr++;
            switch (c) {
                case '"':  buffer += "\\\""; break;
                case '\\': buffer += "\\\\"; break;
                case '\b': buffer += "\\b"; break;
                case '\f': buffer += "\\f"; break;
                case '\n': buffer += "\\n"; break;
                case '\r': buffer += "\\r"; break;
                case '\t': buffer += "\\t"; break;
                default: {
                    if (static_cast<unsigned char>(c) < 32) {
                        char h[7];
                        snprintf(h, sizeof(h), "\\u%04x", static_cast<unsigned char>(c));
                        buffer += h;
                    } else buffer += c;
                } break;
            }
        }
    }
    buffer += '"';
}

auto json_value::serialize_array_to_buffer(std::string& buffer, const json_array& arr, int indent) const -> void {
    buffer += '[';
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i > 0) buffer += ',';
        arr[i].serialize_to_buffer(buffer, indent);
    }
    buffer += ']';
}

auto json_value::serialize_object_to_buffer(std::string& buffer, const json_object& obj, int indent) const -> void {
    buffer += '{';
    bool first = true;
    for (const auto& [key, val] : obj) {
        if (!first) buffer += ',';
        first = false;
        serialize_string_to_buffer(buffer, key);
        buffer += ':';
        val.serialize_to_buffer(buffer, indent);
    }
    buffer += '}';
}

auto json_value::serialize_pretty_to_buffer(std::string& buffer, int indent_size, int current_indent) const -> void {
    std::visit(
        [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_array_ptr>) serialize_pretty_array(buffer, *v, indent_size, current_indent);
            else if constexpr (std::is_same_v<T, json_object_ptr>) serialize_pretty_object(buffer, *v, indent_size, current_indent);
            else serialize_to_buffer(buffer, current_indent);
        }, data_);
}

auto json_value::serialize_pretty_array(std::string& buffer, const json_array& arr, int indent_size, int current_indent) const -> void {
    if (arr.empty()) { buffer += "[]"; return; }
    buffer += "[\n";
    current_indent += indent_size;
    for (size_t i = 0; i < arr.size(); ++i) {
        buffer.append(current_indent, ' ');
        arr[i].serialize_pretty_to_buffer(buffer, indent_size, current_indent);
        if (i < arr.size() - 1) buffer += ',';
        buffer += '\n';
    }
    current_indent -= indent_size;
    buffer.append(current_indent, ' ');
    buffer += ']';
}

auto json_value::serialize_pretty_object(std::string& buffer, const json_object& obj, int indent_size, int current_indent) const -> void {
    if (obj.empty()) { buffer += "{}"; return; }
    buffer += "{\n";
    current_indent += indent_size;
    bool first = true;
    for (const auto& [key, val] : obj) {
        if (!first) buffer += ",\n";
        first = false;
        buffer.append(current_indent, ' ');
        serialize_string_to_buffer(buffer, key);
        buffer += ": ";
        val.serialize_pretty_to_buffer(buffer, indent_size, current_indent);
    }
    buffer += '\n';
    current_indent -= indent_size;
    buffer.append(current_indent, ' ');
    buffer += '}';
}

// Serialization stubs replaced with real implementations
auto stringify(const json_value& v) -> std::string {
    std::string buffer;
    buffer.reserve(1024);
    v.serialize_to_buffer(buffer, 0);
    return buffer;
}

auto prettify(const json_value& v, int indent) -> std::string {
    std::string buffer;
    buffer.reserve(2048);
    v.serialize_pretty_to_buffer(buffer, indent, 0);
    return buffer;
}


// ============================================================================
// Functional Programming Utilities for JSON Arrays
// ============================================================================

// Map: Transform each element in a JSON array
auto map(const json_value& arr, auto&& func) -> json_value {
    if (!arr.is_array()) {
        throw std::runtime_error("map() requires a JSON array");
    }
    
    json_array result;
    const auto& source = arr.as_array();
    result.reserve(source.size());
    
    for (const auto& elem : source) {
        result.push_back(func(elem));
    }
    
    return json_value{std::move(result)};
}

// Filter: Keep only elements that satisfy a predicate
auto filter(const json_value& arr, auto&& pred) -> json_value {
    if (!arr.is_array()) {
        throw std::runtime_error("filter() requires a JSON array");
    }
    
    json_array result;
    const auto& source = arr.as_array();
    
    for (const auto& elem : source) {
        if (pred(elem)) {
            result.push_back(elem);
        }
    }
    
    return json_value{std::move(result)};
}

// Reduce: Fold array elements into a single value
auto reduce(const json_value& arr, auto&& func, json_value init) -> json_value {
    if (!arr.is_array()) {
        throw std::runtime_error("reduce() requires a JSON array");
    }
    
    json_value accumulator = std::move(init);
    const auto& source = arr.as_array();
    
    for (const auto& elem : source) {
        accumulator = func(std::move(accumulator), elem);
    }
    
    return accumulator;
}

// All: Check if all elements satisfy a predicate
auto all(const json_value& arr, auto&& pred) -> bool {
    if (!arr.is_array()) {
        throw std::runtime_error("all() requires a JSON array");
    }
    
    const auto& source = arr.as_array();
    for (const auto& elem : source) {
        if (!pred(elem)) {
            return false;
        }
    }
    
    return true;
}

// Any: Check if any element satisfies a predicate
auto any(const json_value& arr, auto&& pred) -> bool {
    if (!arr.is_array()) {
        throw std::runtime_error("any() requires a JSON array");
    }
    
    const auto& source = arr.as_array();
    for (const auto& elem : source) {
        if (pred(elem)) {
            return true;
        }
    }
    
    return false;
}

// Zip: Combine two arrays element-wise
auto zip(const json_value& arr1, const json_value& arr2) -> json_value {
    if (!arr1.is_array() || !arr2.is_array()) {
        throw std::runtime_error("zip() requires two JSON arrays");
    }
    
    const auto& source1 = arr1.as_array();
    const auto& source2 = arr2.as_array();
    const size_t min_size = std::min(source1.size(), source2.size());
    
    json_array result;
    result.reserve(min_size);
    
    for (size_t i = 0; i < min_size; ++i) {
        json_array pair;
        pair.push_back(source1[i]);
        pair.push_back(source2[i]);
        result.push_back(json_value{std::move(pair)});
    }
    
    return json_value{std::move(result)};
}

// Unzip: Split an array of pairs into two arrays
auto unzip(const json_value& arr) -> std::pair<json_value, json_value> {
    if (!arr.is_array()) {
        throw std::runtime_error("unzip() requires a JSON array");
    }
    
    const auto& source = arr.as_array();
    json_array first, second;
    first.reserve(source.size());
    second.reserve(source.size());
    
    for (const auto& elem : source) {
        if (elem.is_array() && elem.as_array().size() >= 2) {
            const auto& pair = elem.as_array();
            first.push_back(pair[0]);
            second.push_back(pair[1]);
        }
    }
    
    return {json_value{std::move(first)}, json_value{std::move(second)}};
}

// Reverse: Reverse the order of array elements
auto reverse(const json_value& arr) -> json_value {
    if (!arr.is_array()) {
        throw std::runtime_error("reverse() requires a JSON array");
    }
    
    const auto& source = arr.as_array();
    json_array result(source.rbegin(), source.rend());
    
    return json_value{std::move(result)};
}

// Rotate: Rotate array elements left by n positions
auto rotate(const json_value& arr, int n) -> json_value {
    if (!arr.is_array()) {
        throw std::runtime_error("rotate() requires a JSON array");
    }
    
    const auto& source = arr.as_array();
    if (source.empty()) {
        return arr;
    }
    
    // Normalize n to be within [0, size)
    const size_t size = source.size();
    n = ((n % static_cast<int>(size)) + static_cast<int>(size)) % static_cast<int>(size);
    
    json_array result;
    result.reserve(size);
    
    for (size_t i = 0; i < size; ++i) {
        result.push_back(source[(i + n) % size]);
    }
    
    return json_value{std::move(result)};
}

// Take: Get first n elements
auto take(const json_value& arr, size_t n) -> json_value {
    if (!arr.is_array()) {
        throw std::runtime_error("take() requires a JSON array");
    }
    
    const auto& source = arr.as_array();
    const size_t count = std::min(n, source.size());
    
    json_array result(source.begin(), source.begin() + count);
    return json_value{std::move(result)};
}

// Drop: Skip first n elements
auto drop(const json_value& arr, size_t n) -> json_value {
    if (!arr.is_array()) {
        throw std::runtime_error("drop() requires a JSON array");
    }
    
    const auto& source = arr.as_array();
    if (n >= source.size()) {
        return json_value{json_array{}};
    }
    
    json_array result(source.begin() + n, source.end());
    return json_value{std::move(result)};
}


}  // namespace fastjson_parallel
