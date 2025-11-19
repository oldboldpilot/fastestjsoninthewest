// FastestJSONInTheWest - Parallel SIMD JSON Parser Implementation
// Copyright (c) 2025 - High-performance JSON parsing with OpenMP, SIMD, and GPU support
// ============================================================================

import logger;

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

#ifdef _OPENMP
    #include <omp.h>
#endif

// SIMD intrinsics
#if defined(__x86_64__) || defined(_M_X64)
    #include <cpuid.h>
    #include <immintrin.h>
#endif

// SIMD structural indexing (Phase 1)
#include "fastjson_simd_index.h"

// NUMA-aware memory allocation
#include "numa_allocator.h"

namespace fastjson {

// ============================================================================
// Configuration and Settings
// ============================================================================

struct parse_config {
    // Parallelism control
    int num_threads = 8;  // Default to 8 (physical cores)
                          // -1 = auto (use OMP_NUM_THREADS or hardware max)
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
};

// Global configuration (thread-local for safety)
thread_local parse_config g_config;

// NUMA topology (shared across threads, initialized once)
static numa::numa_topology g_numa_topo;
static std::atomic<bool> g_numa_initialized{false};

// Configuration API
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
        return omp_get_max_threads();
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
// JSON Type System
// ============================================================================

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

using json_string = std::string;
using json_number = double;
using json_boolean = bool;
using json_null = std::nullptr_t;

class json_value;
using json_array = std::vector<json_value>;
using json_object = std::unordered_map<std::string, json_value>;

class json_value {
public:
    json_value() : data_(nullptr) {}

    json_value(std::nullptr_t) : data_(nullptr) {}

    json_value(bool b) : data_(b) {}

    json_value(double d) : data_(d) {}

    json_value(int i) : data_(static_cast<double>(i)) {}

    json_value(const std::string& s) : data_(s) {}

    json_value(std::string&& s) : data_(std::move(s)) {}

    json_value(const json_array& a) : data_(a) {}

    json_value(json_array&& a) : data_(std::move(a)) {}

    json_value(const json_object& o) : data_(o) {}

    json_value(json_object&& o) : data_(std::move(o)) {}

    // Destructor must be explicitly defined to handle recursive cleanup
    ~json_value() {
#ifdef DEBUG_DESTRUCTOR
        static std::atomic<size_t> destroy_count{0};
        size_t count = ++destroy_count;

        auto& log = logger::Logger::getInstance();
        std::visit(
            [&log, count](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, json_array>) {
                    log.debug("Destroying json_array {} with {} elements",
                              logger::Logger::named_args("count", count, "size", arg.size()));
                } else if constexpr (std::is_same_v<T, json_object>) {
                    log.debug("Destroying json_object {} with {} keys",
                              logger::Logger::named_args("count", count, "size", arg.size()));
                } else if constexpr (std::is_same_v<T, json_string>) {
                    log.debug("Destroying json_string {} length={}",
                              logger::Logger::named_args("count", count, "length", arg.length()));
                } else {
                    log.debug("Destroying json_value {} (primitive)", count);
                }
            },
            data_);
#endif
        // The variant destructor will call the destructor of the active alternative.
        // For json_array (std::vector<json_value>) and json_object (std::unordered_map<std::string,
        // json_value>), their destructors will recursively destroy all contained json_values.
    }

    json_value(const json_value&) = default;
    json_value(json_value&&) noexcept = default;
    json_value& operator=(const json_value&) = default;
    json_value& operator=(json_value&&) noexcept = default;

    auto is_null() const -> bool { return std::holds_alternative<json_null>(data_); }

    auto is_bool() const -> bool { return std::holds_alternative<json_boolean>(data_); }

    auto is_number() const -> bool { return std::holds_alternative<json_number>(data_); }

    auto is_string() const -> bool { return std::holds_alternative<json_string>(data_); }

    auto is_array() const -> bool { return std::holds_alternative<json_array>(data_); }

    auto is_object() const -> bool { return std::holds_alternative<json_object>(data_); }

    auto as_bool() const -> bool { return std::get<json_boolean>(data_); }

    auto as_number() const -> double { return std::get<json_number>(data_); }

    auto as_string() const -> const std::string& { return std::get<json_string>(data_); }

    auto as_array() const -> const json_array& { return std::get<json_array>(data_); }

    auto as_object() const -> const json_object& { return std::get<json_object>(data_); }

private:
    std::variant<json_null, json_boolean, json_number, json_string, json_array, json_object> data_;
};

// Destructor was moved inline to the class definition above since this is a header-only library

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
    const __m256i control = _mm256_set1_epi8(0x20);  // Characters < 0x20 are control chars

    size_t pos = start_pos;

    while (pos + 32 <= data.size()) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data.data() + pos));

        __m256i is_quote = _mm256_cmpeq_epi8(chunk, quote);
        __m256i is_backslash = _mm256_cmpeq_epi8(chunk, backslash);
        __m256i is_control = _mm256_cmpgt_epi8(control, chunk);  // Find chars < 0x20

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

    if (peek() == '-')
        advance();

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
        advance();
        if (!(peek() >= '0' && peek() <= '9')) {
            return std::unexpected(
                make_error(json_error_code::invalid_number, "Invalid decimal number"));
        }
        while (peek() >= '0' && peek() <= '9')
            advance();
    }

    if (peek() == 'e' || peek() == 'E') {
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

    thread_local std::array<char, 64> buffer;
    size_t length = std::min(pos_ - start_pos, buffer.size() - 1);
    std::memcpy(buffer.data(), input_.data() + start_pos, length);
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
                        auto result = unicode::decode_json_unicode_escape(input_.data() + pos_,
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
                        unicode::decode_json_unicode_escape(input_.data() + pos_, remaining, value);

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
            g_numa_topo = numa::detect_numa_topology();
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
                int node = numa::get_optimal_node_for_thread(thread_id, num_threads,
                                                             g_numa_topo.num_nodes);
                numa::bind_thread_to_numa_node(node);
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
                int node = numa::get_optimal_node_for_thread(thread_id, num_threads,
                                                             g_numa_topo.num_nodes);
                numa::bind_thread_to_numa_node(node);
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

auto parse(std::string_view input) -> json_result<json_value> {
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

// Serialization stubs (to be implemented)
auto stringify(const json_value& v) -> std::string {
    return "{}";
}

auto prettify(const json_value& v, int indent) -> std::string {
    return "{}";
}

}  // namespace fastjson
