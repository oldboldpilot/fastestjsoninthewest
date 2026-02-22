// FastestJSONInTheWest - Comprehensive Thread-Safe SIMD Implementation
// Author: Olumuyiwa Oluwasanmi
// Advanced instruction set waterfall with complete thread safety

module;

// SIMD intrinsics MUST be in global module fragment to avoid declaration conflicts
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <array>
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

// C++ headers needed for module purview
#include <sstream>

// Structural indexing for ondemand parsing (SIMD tape builder)
#include "fastjson_simd_index.h"

// ============================================================================
// SIMD Implementations in Global Module Fragment
// All functions using SIMD intrinsic types (__m256i, __m512i, __m128i, etc.)
// MUST live here to avoid Clang 21 TemplateInstantiator segfault in module purview.
// The module purview exports thin wrappers that delegate to these detail:: functions.
// ============================================================================

namespace fastjson::detail {

#ifdef FASTJSON_ENABLE_SIMD

#if defined(__x86_64__) || defined(_M_X64)

// SIMD capability flags
static constexpr uint32_t SIMD_SSE2        = 0x002;
static constexpr uint32_t SIMD_SSE3        = 0x004;
static constexpr uint32_t SIMD_SSSE3       = 0x008;
static constexpr uint32_t SIMD_SSE41       = 0x010;
static constexpr uint32_t SIMD_SSE42       = 0x020;
static constexpr uint32_t SIMD_AVX         = 0x040;
static constexpr uint32_t SIMD_AVX2        = 0x080;
static constexpr uint32_t SIMD_AVX512F     = 0x100;
static constexpr uint32_t SIMD_AVX512BW    = 0x200;
static constexpr uint32_t SIMD_AVX512VBMI  = 0x400;
static constexpr uint32_t SIMD_AVX512VBMI2 = 0x800;
static constexpr uint32_t SIMD_AVX512VNNI  = 0x1000;
static constexpr uint32_t SIMD_AMX_TILE    = 0x2000;
static constexpr uint32_t SIMD_AMX_INT8    = 0x4000;

// Thread-safe SIMD capability detection (call_once + atomic cache)
inline auto detect_simd_capabilities() noexcept -> uint32_t {
    static std::atomic<uint32_t> cached_caps{0};
    static bool initialized = false;

    if (initialized) {
        return cached_caps.load(std::memory_order_relaxed);
    }

    uint32_t caps = 0;
    uint32_t eax, ebx, ecx, edx;

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

    __cpuid(0, eax, ebx, ecx, edx);
    if (eax >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        if (ebx & (1 << 5))  caps |= SIMD_AVX2;
        if (ebx & (1 << 16)) caps |= SIMD_AVX512F;
        if (ebx & (1 << 30)) caps |= SIMD_AVX512BW;
        if (ecx & (1 << 1))  caps |= SIMD_AVX512VBMI;
        if (ecx & (1 << 6))  caps |= SIMD_AVX512VBMI2;
        if (ecx & (1 << 11)) caps |= SIMD_AVX512VNNI;
        if (edx & (1 << 24)) caps |= SIMD_AMX_TILE;
        if (edx & (1 << 25)) caps |= SIMD_AMX_INT8;
    }

    cached_caps.store(caps, std::memory_order_release);
    initialized = true;
    return caps;
}

// --------------------------------------------------------------------------
// AVX-512 Whitespace Skip — 4x zmm registers (256 bytes per iteration)
// --------------------------------------------------------------------------
#ifdef HAVE_AVX512F
__attribute__((target("avx512f,avx512bw")))
inline auto skip_whitespace_avx512(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;

    const __m512i ws_space = _mm512_set1_epi8(' ');
    const __m512i ws_tab   = _mm512_set1_epi8('\t');
    const __m512i ws_nl    = _mm512_set1_epi8('\n');
    const __m512i ws_cr    = _mm512_set1_epi8('\r');

    auto check_ws = [&](__m512i chunk) -> __mmask64 {
        __mmask64 m0 = _mm512_cmpeq_epi8_mask(chunk, ws_space);
        __mmask64 m1 = _mm512_cmpeq_epi8_mask(chunk, ws_tab);
        __mmask64 m2 = _mm512_cmpeq_epi8_mask(chunk, ws_nl);
        __mmask64 m3 = _mm512_cmpeq_epi8_mask(chunk, ws_cr);
        return m0 | m1 | m2 | m3;
    };

    // 4x zmm multi-register: 256 bytes per iteration
    while (ptr + 256 <= end) {
        __m512i c0 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
        __m512i c1 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 64));
        __m512i c2 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 128));
        __m512i c3 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 192));

        __mmask64 ws0 = check_ws(c0);
        if (ws0 != 0xFFFFFFFFFFFFFFFF) return ptr + __builtin_ctzll(~ws0);

        __mmask64 ws1 = check_ws(c1);
        if (ws1 != 0xFFFFFFFFFFFFFFFF) return ptr + 64 + __builtin_ctzll(~ws1);

        __mmask64 ws2 = check_ws(c2);
        if (ws2 != 0xFFFFFFFFFFFFFFFF) return ptr + 128 + __builtin_ctzll(~ws2);

        __mmask64 ws3 = check_ws(c3);
        if (ws3 != 0xFFFFFFFFFFFFFFFF) return ptr + 192 + __builtin_ctzll(~ws3);

        ptr += 256;
    }

    // Single zmm tail loop
    while (ptr + 64 <= end) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
        __mmask64 ws = check_ws(chunk);
        if (ws != 0xFFFFFFFFFFFFFFFF) return ptr + __builtin_ctzll(~ws);
        ptr += 64;
    }

    // Scalar tail
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}
#endif // HAVE_AVX512F

// --------------------------------------------------------------------------
// AVX2 Whitespace Skip — 8x ymm registers (256 bytes per iteration)
// --------------------------------------------------------------------------
#ifdef HAVE_AVX2
__attribute__((target("avx2")))
inline auto skip_whitespace_avx2(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;

    const __m256i ws_space = _mm256_set1_epi8(' ');
    const __m256i ws_tab   = _mm256_set1_epi8('\t');
    const __m256i ws_nl    = _mm256_set1_epi8('\n');
    const __m256i ws_cr    = _mm256_set1_epi8('\r');

    auto check_ws = [&](__m256i chunk) -> __m256i {
        __m256i m0 = _mm256_cmpeq_epi8(chunk, ws_space);
        __m256i m1 = _mm256_cmpeq_epi8(chunk, ws_tab);
        __m256i m2 = _mm256_cmpeq_epi8(chunk, ws_nl);
        __m256i m3 = _mm256_cmpeq_epi8(chunk, ws_cr);
        return _mm256_or_si256(_mm256_or_si256(m0, m1), _mm256_or_si256(m2, m3));
    };

    // 8x ymm multi-register: 256 bytes per iteration
    while (ptr + 256 <= end) {
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 32));
        __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 64));
        __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 96));
        __m256i c4 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 128));
        __m256i c5 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 160));
        __m256i c6 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 192));
        __m256i c7 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 224));

        uint32_t m0 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c0)));
        if (m0) return ptr + __builtin_ctz(m0);
        uint32_t m1 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c1)));
        if (m1) return ptr + 32 + __builtin_ctz(m1);
        uint32_t m2 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c2)));
        if (m2) return ptr + 64 + __builtin_ctz(m2);
        uint32_t m3 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c3)));
        if (m3) return ptr + 96 + __builtin_ctz(m3);
        uint32_t m4 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c4)));
        if (m4) return ptr + 128 + __builtin_ctz(m4);
        uint32_t m5 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c5)));
        if (m5) return ptr + 160 + __builtin_ctz(m5);
        uint32_t m6 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c6)));
        if (m6) return ptr + 192 + __builtin_ctz(m6);
        uint32_t m7 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c7)));
        if (m7) return ptr + 224 + __builtin_ctz(m7);

        ptr += 256;
    }

    // Single ymm tail loop
    while (ptr + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        uint32_t mask = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(chunk)));
        if (mask) return ptr + __builtin_ctz(mask);
        ptr += 32;
    }

    // Scalar tail
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}
#endif // HAVE_AVX2

// --------------------------------------------------------------------------
// SSE4.2 Whitespace Skip — 4x xmm registers (64 bytes per iteration)
// --------------------------------------------------------------------------
#ifdef HAVE_SSE42
__attribute__((target("sse4.2")))
inline auto skip_whitespace_sse42(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;

    const __m128i whitespace_chars =
        _mm_setr_epi8(' ', '\t', '\n', '\r', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    // 4x xmm multi-register: 64 bytes per iteration
    while (ptr + 64 <= end) {
        __m128i c0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        __m128i c1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 16));
        __m128i c2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 32));
        __m128i c3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 48));

        int r0 = _mm_cmpestri(whitespace_chars, 4, c0, 16,
                               _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
        if (r0 < 16) return ptr + r0;

        int r1 = _mm_cmpestri(whitespace_chars, 4, c1, 16,
                               _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
        if (r1 < 16) return ptr + 16 + r1;

        int r2 = _mm_cmpestri(whitespace_chars, 4, c2, 16,
                               _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
        if (r2 < 16) return ptr + 32 + r2;

        int r3 = _mm_cmpestri(whitespace_chars, 4, c3, 16,
                               _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
        if (r3 < 16) return ptr + 48 + r3;

        ptr += 64;
    }

    // Single xmm tail loop
    while (ptr + 16 <= end) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        int result = _mm_cmpestri(whitespace_chars, 4, chunk, 16,
                                   _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
        if (result < 16) return ptr + result;
        ptr += 16;
    }

    // Scalar tail
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}
#endif // HAVE_SSE42

// --------------------------------------------------------------------------
// SSE2 Whitespace Skip — 4x xmm registers (64 bytes per iteration)
// --------------------------------------------------------------------------
#ifdef HAVE_SSE2
__attribute__((target("sse2")))
inline auto skip_whitespace_sse2(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;

    const __m128i ws_space = _mm_set1_epi8(' ');
    const __m128i ws_tab   = _mm_set1_epi8('\t');
    const __m128i ws_nl    = _mm_set1_epi8('\n');
    const __m128i ws_cr    = _mm_set1_epi8('\r');

    auto check_ws = [&](__m128i chunk) -> __m128i {
        __m128i m0 = _mm_cmpeq_epi8(chunk, ws_space);
        __m128i m1 = _mm_cmpeq_epi8(chunk, ws_tab);
        __m128i m2 = _mm_cmpeq_epi8(chunk, ws_nl);
        __m128i m3 = _mm_cmpeq_epi8(chunk, ws_cr);
        return _mm_or_si128(_mm_or_si128(m0, m1), _mm_or_si128(m2, m3));
    };

    // 4x xmm multi-register: 64 bytes per iteration
    while (ptr + 64 <= end) {
        __m128i c0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        __m128i c1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 16));
        __m128i c2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 32));
        __m128i c3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 48));

        uint32_t m0 = ~_mm_movemask_epi8(check_ws(c0)) & 0xFFFF;
        if (m0) return ptr + __builtin_ctz(m0);
        uint32_t m1 = ~_mm_movemask_epi8(check_ws(c1)) & 0xFFFF;
        if (m1) return ptr + 16 + __builtin_ctz(m1);
        uint32_t m2 = ~_mm_movemask_epi8(check_ws(c2)) & 0xFFFF;
        if (m2) return ptr + 32 + __builtin_ctz(m2);
        uint32_t m3 = ~_mm_movemask_epi8(check_ws(c3)) & 0xFFFF;
        if (m3) return ptr + 48 + __builtin_ctz(m3);

        ptr += 64;
    }

    // Single xmm tail loop
    while (ptr + 16 <= end) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        uint32_t mask = ~_mm_movemask_epi8(check_ws(chunk)) & 0xFFFF;
        if (mask) return ptr + __builtin_ctz(mask);
        ptr += 16;
    }

    // Scalar tail
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}
#endif // HAVE_SSE2

// --------------------------------------------------------------------------
// Runtime SIMD Dispatcher for Whitespace Skipping
// --------------------------------------------------------------------------
inline auto skip_whitespace_simd_impl(const char* data, size_t size) -> const char* {
    static const uint32_t caps = detect_simd_capabilities();

#ifdef HAVE_AVX512F
    if ((caps & SIMD_AVX512F) && (caps & SIMD_AVX512BW))
        return skip_whitespace_avx512(data, size);
#endif
#ifdef HAVE_AVX2
    if (caps & SIMD_AVX2)
        return skip_whitespace_avx2(data, size);
#endif
#ifdef HAVE_SSE42
    if (caps & SIMD_SSE42)
        return skip_whitespace_sse42(data, size);
#endif
#ifdef HAVE_SSE2
    if (caps & SIMD_SSE2)
        return skip_whitespace_sse2(data, size);
#endif

    // Scalar fallback
    const char* ptr = data;
    const char* end = data + size;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}

// --------------------------------------------------------------------------
// AVX2 String End Detection — 8x ymm registers (256 bytes per iteration)
// Finds the first quote, backslash, or control char in [start, end).
// --------------------------------------------------------------------------
#ifdef HAVE_AVX2
__attribute__((target("avx2")))
inline auto find_string_end_avx2(const char* start, const char* end) -> const char* {
    const char* ptr = start;

    const __m256i quote     = _mm256_set1_epi8('"');
    const __m256i backslash = _mm256_set1_epi8('\\');
    const __m256i ctrl_limit = _mm256_set1_epi8(0x20);
    const __m256i sign_flip = _mm256_set1_epi8(static_cast<char>(0x80));

    auto check_special = [&](__m256i chunk) -> uint32_t {
        __m256i q  = _mm256_cmpeq_epi8(chunk, quote);
        __m256i bs = _mm256_cmpeq_epi8(chunk, backslash);
        // Control chars: unsigned < 0x20, use signed comparison with XOR trick
        __m256i ctrl = _mm256_cmpgt_epi8(ctrl_limit,
                       _mm256_xor_si256(chunk, sign_flip));
        return static_cast<uint32_t>(_mm256_movemask_epi8(
            _mm256_or_si256(_mm256_or_si256(q, bs), ctrl)));
    };

    // 8x ymm multi-register: 256 bytes per iteration
    while (ptr + 256 <= end) {
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 32));
        __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 64));
        __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 96));
        __m256i c4 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 128));
        __m256i c5 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 160));
        __m256i c6 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 192));
        __m256i c7 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 224));

        uint32_t m0 = check_special(c0); if (m0) return ptr + __builtin_ctz(m0);
        uint32_t m1 = check_special(c1); if (m1) return ptr + 32 + __builtin_ctz(m1);
        uint32_t m2 = check_special(c2); if (m2) return ptr + 64 + __builtin_ctz(m2);
        uint32_t m3 = check_special(c3); if (m3) return ptr + 96 + __builtin_ctz(m3);
        uint32_t m4 = check_special(c4); if (m4) return ptr + 128 + __builtin_ctz(m4);
        uint32_t m5 = check_special(c5); if (m5) return ptr + 160 + __builtin_ctz(m5);
        uint32_t m6 = check_special(c6); if (m6) return ptr + 192 + __builtin_ctz(m6);
        uint32_t m7 = check_special(c7); if (m7) return ptr + 224 + __builtin_ctz(m7);

        ptr += 256;
    }

    // Single ymm tail loop
    while (ptr + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        uint32_t mask = check_special(chunk);
        if (mask) return ptr + __builtin_ctz(mask);
        ptr += 32;
    }

    // Scalar tail
    while (ptr < end) {
        if (*ptr == '"' || *ptr == '\\' || static_cast<unsigned char>(*ptr) < 0x20)
            return ptr;
        ++ptr;
    }
    return end;
}
#endif // HAVE_AVX2

// --------------------------------------------------------------------------
// String End Detection Dispatcher
// --------------------------------------------------------------------------
inline auto find_string_end_simd_impl(const char* start, const char* end) -> const char* {
    [[maybe_unused]] static const uint32_t caps = detect_simd_capabilities();

#ifdef HAVE_AVX2
    if (caps & SIMD_AVX2)
        return find_string_end_avx2(start, end);
#endif

    // Scalar fallback
    const char* ptr = start;
    while (ptr < end && *ptr != '"' && *ptr != '\\' && static_cast<unsigned char>(*ptr) >= 0x20)
        ++ptr;
    return ptr;
}

// --------------------------------------------------------------------------
// Serialization Escape Position Finder — 8x AVX2 (256 bytes per iteration)
// Finds the first character that needs JSON escaping (", \, or control char < 0x20).
// --------------------------------------------------------------------------
inline auto find_escape_position_simd_impl(const char* ptr, const char* end) -> const char* {
    [[maybe_unused]] static const uint32_t caps = detect_simd_capabilities();

#ifdef HAVE_AVX512BW
    if (caps & SIMD_AVX512BW) {
        while (ptr + 64 <= end) {
            __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
            __mmask64 m1 = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('"'));
            __mmask64 m2 = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\\'));
            __mmask64 m3 = _mm512_cmp_epi8_mask(chunk, _mm512_set1_epi8(32), _MM_CMPINT_LT);
            __mmask64 mask = m1 | m2 | m3;
            if (mask != 0) return ptr + __builtin_ctzll(mask);
            ptr += 64;
        }
    }
#endif

#ifdef HAVE_AVX2
    if (caps & SIMD_AVX2) {
        const __m256i quote     = _mm256_set1_epi8('"');
        const __m256i backslash = _mm256_set1_epi8('\\');

        auto check = [&](__m256i c) -> int {
            __m256i m1 = _mm256_cmpeq_epi8(c, quote);
            __m256i m2 = _mm256_cmpeq_epi8(c, backslash);
            __m256i m3 = _mm256_cmpgt_epi8(_mm256_set1_epi8(32), c);
            return _mm256_movemask_epi8(_mm256_or_si256(_mm256_or_si256(m1, m2), m3));
        };

        // 8x ymm multi-register: 256 bytes per iteration
        while (ptr + 256 <= end) {
            __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
            __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 32));
            __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 64));
            __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 96));
            __m256i c4 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 128));
            __m256i c5 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 160));
            __m256i c6 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 192));
            __m256i c7 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 224));

            if (int m = check(c0)) return ptr + __builtin_ctz(m);
            if (int m = check(c1)) return ptr + 32 + __builtin_ctz(m);
            if (int m = check(c2)) return ptr + 64 + __builtin_ctz(m);
            if (int m = check(c3)) return ptr + 96 + __builtin_ctz(m);
            if (int m = check(c4)) return ptr + 128 + __builtin_ctz(m);
            if (int m = check(c5)) return ptr + 160 + __builtin_ctz(m);
            if (int m = check(c6)) return ptr + 192 + __builtin_ctz(m);
            if (int m = check(c7)) return ptr + 224 + __builtin_ctz(m);
            ptr += 256;
        }

        // Single ymm tail
        while (ptr + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
            if (int m = check(chunk)) return ptr + __builtin_ctz(m);
            ptr += 32;
        }
    }
#endif

    // Scalar fallback
    while (ptr < end && *ptr != '"' && *ptr != '\\'
           && static_cast<unsigned char>(*ptr) >= 32) {
        ++ptr;
    }
    return ptr;
}

// --------------------------------------------------------------------------
// AMX / VNNI Implementations (kept for specialized use cases)
// --------------------------------------------------------------------------
#ifdef HAVE_AMX_TILE
class amx_context {
    static thread_local bool tiles_configured;
public:
    static auto configure_tiles() noexcept -> bool {
        if (!tiles_configured) {
            struct tile_config {
                uint8_t palette_id;
                uint8_t start_row;
                uint8_t reserved[14];
                uint16_t colsb[16];
                uint8_t rows[16];
            };
            alignas(64) tile_config config = {};
            config.palette_id = 1;
            for (int i = 0; i < 8; ++i) {
                config.colsb[i] = 64;
                config.rows[i] = 16;
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

__attribute__((target("amx-tile,amx-int8")))
inline auto classify_json_chars_amx(const char* data, size_t size, uint8_t* classifications)
    -> size_t {
    if (!amx_context::configure_tiles()) return 0;
    const size_t chunk_size = 1024;
    size_t processed = 0;
    alignas(64) uint8_t lookup_table[256] = {};
    lookup_table[' '] = 1; lookup_table['\t'] = 1;
    lookup_table['\n'] = 1; lookup_table['\r'] = 1;
    lookup_table['{'] = 2; lookup_table['}'] = 2;
    lookup_table['['] = 2; lookup_table[']'] = 2;
    lookup_table[':'] = 2; lookup_table[','] = 2;
    lookup_table['"'] = 3;
    for (size_t i = 0; i + chunk_size <= size; i += chunk_size) {
        const char* chunk = data + i;
        for (size_t j = 0; j < chunk_size; ++j)
            classifications[processed + j] = lookup_table[static_cast<uint8_t>(chunk[j])];
        processed += chunk_size;
    }
    for (size_t i = processed; i < size; ++i)
        classifications[i] = lookup_table[static_cast<uint8_t>(data[i])];
    return size;
}
#endif // HAVE_AMX_TILE

#ifdef HAVE_AVX512VNNI
__attribute__((target("avx512f,avx512vnni")))
inline auto process_json_tokens_vnni(const char* data, size_t size)
    -> std::array<uint32_t, 256> {
    std::array<uint32_t, 256> token_counts = {};
    const size_t chunk_size = 64;
    size_t processed = 0;
    for (size_t i = 0; i + chunk_size <= size; i += chunk_size) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(data + i));
        __mmask64 brace_open  = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('{'));
        __mmask64 brace_close = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('}'));
        __mmask64 brack_open  = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('['));
        __mmask64 brack_close = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8(']'));
        token_counts['{'] += __builtin_popcountll(brace_open);
        token_counts['}'] += __builtin_popcountll(brace_close);
        token_counts['['] += __builtin_popcountll(brack_open);
        token_counts[']'] += __builtin_popcountll(brack_close);
        processed += chunk_size;
    }
    for (size_t i = processed; i < size; ++i)
        token_counts[static_cast<uint8_t>(data[i])]++;
    return token_counts;
}
#endif // HAVE_AVX512VNNI

#endif // x86_64

#else // !FASTJSON_ENABLE_SIMD

// SIMD capability constants (still needed for API compatibility when SIMD disabled)
constexpr uint32_t SIMD_SSE2        = 0x002;
constexpr uint32_t SIMD_SSE3        = 0x004;
constexpr uint32_t SIMD_SSSE3       = 0x008;
constexpr uint32_t SIMD_SSE41       = 0x010;
constexpr uint32_t SIMD_SSE42       = 0x020;
constexpr uint32_t SIMD_AVX         = 0x040;
constexpr uint32_t SIMD_AVX2        = 0x080;
constexpr uint32_t SIMD_AVX512F     = 0x100;
constexpr uint32_t SIMD_AVX512BW    = 0x200;
constexpr uint32_t SIMD_AVX512VBMI  = 0x400;
constexpr uint32_t SIMD_AVX512VBMI2 = 0x800;
constexpr uint32_t SIMD_AVX512VNNI  = 0x1000;
constexpr uint32_t SIMD_AMX_TILE    = 0x2000;
constexpr uint32_t SIMD_AMX_INT8    = 0x4000;

// Scalar-only implementations when SIMD is disabled at compile time
inline auto detect_simd_capabilities() noexcept -> uint32_t { return 0; }

inline auto skip_whitespace_simd_impl(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}

inline auto find_string_end_simd_impl(const char* start, const char* end) -> const char* {
    const char* ptr = start;
    while (ptr < end && *ptr != '"' && *ptr != '\\' && static_cast<unsigned char>(*ptr) >= 0x20)
        ++ptr;
    return ptr;
}

inline auto find_escape_position_simd_impl(const char* ptr, const char* end) -> const char* {
    while (ptr < end && *ptr != '"' && *ptr != '\\'
           && static_cast<unsigned char>(*ptr) >= 32)
        ++ptr;
    return ptr;
}

#endif // FASTJSON_ENABLE_SIMD

} // namespace fastjson::detail

export module fastjson;

import std;

export namespace fastjson {

// SIMD Wrappers — delegate to detail:: implementations in global module fragment
// All actual SIMD intrinsic usage lives in the GMF to avoid Clang 21 module BMI segfault.
// ============================================================================
// Re-export SIMD capability constants from detail:: for public API
constexpr uint32_t SIMD_SSE2        = detail::SIMD_SSE2;
constexpr uint32_t SIMD_SSE3        = detail::SIMD_SSE3;
constexpr uint32_t SIMD_SSSE3       = detail::SIMD_SSSE3;
constexpr uint32_t SIMD_SSE41       = detail::SIMD_SSE41;
constexpr uint32_t SIMD_SSE42       = detail::SIMD_SSE42;
constexpr uint32_t SIMD_AVX         = detail::SIMD_AVX;
constexpr uint32_t SIMD_AVX2        = detail::SIMD_AVX2;
constexpr uint32_t SIMD_AVX512F     = detail::SIMD_AVX512F;
constexpr uint32_t SIMD_AVX512BW    = detail::SIMD_AVX512BW;
constexpr uint32_t SIMD_AVX512VBMI  = detail::SIMD_AVX512VBMI;
constexpr uint32_t SIMD_AVX512VBMI2 = detail::SIMD_AVX512VBMI2;
constexpr uint32_t SIMD_AVX512VNNI  = detail::SIMD_AVX512VNNI;
constexpr uint32_t SIMD_AMX_TILE    = detail::SIMD_AMX_TILE;
constexpr uint32_t SIMD_AMX_INT8    = detail::SIMD_AMX_INT8;

// Thread-safe SIMD capability detection — delegates to GMF implementation
[[nodiscard]] inline auto detect_simd_capabilities() noexcept -> uint32_t {
    return detail::detect_simd_capabilities();
}

// Whitespace skip — runtime SIMD dispatch via GMF detail:: implementation
[[nodiscard]] inline auto skip_whitespace_simd(const char* data, size_t size) -> const char* {
    return detail::skip_whitespace_simd_impl(data, size);
}

// (All SIMD implementations removed from module purview — see GMF detail:: namespace)
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

    // String conversion for debugging
    auto to_string() const -> std::string {
        std::ostringstream oss;
        oss << "JSON Error at line " << line << ", column " << column << ": " << message;
        return oss.str();
    }
};

// Stream output operator for json_error
inline auto operator<<(std::ostream& os, const json_error& error) -> std::ostream& {
    return os << error.to_string();
}

// Result type for parsing operations (using std::expected for C++23)
template <typename T> using json_result = std::expected<T, json_error>;

// Zero-copy string with COW semantics: holds string_view into input buffer
// or owned std::string for escaped/mutated strings.
// Lifetime rule: string_view points into the original JSON input buffer.
// The caller must keep the input alive while json_value objects exist.
class json_string_data {
    std::variant<std::string_view, std::string> data_;

public:
    json_string_data() noexcept : data_(std::string_view{}) {}
    json_string_data(std::string_view sv) noexcept : data_(sv) {}
    json_string_data(std::string&& s) noexcept : data_(std::move(s)) {}
    json_string_data(const std::string& s) : data_(s) {}
    json_string_data(const char* s) noexcept : data_(std::string_view(s)) {}

    // Always returns a view (zero-copy for unescaped strings)
    [[nodiscard]] auto view() const noexcept -> std::string_view {
        return std::visit([](const auto& v) -> std::string_view { return v; }, data_);
    }

    // Materializes an owned string if needed
    [[nodiscard]] auto to_string() const -> std::string {
        return std::visit(
            [](const auto& v) -> std::string {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, json_string_data>) {
                    return v;
                } else {
                    return std::string(v);
                }
            },
            data_);
    }

    // COW: ensures the string is owned before mutation
    auto ensure_owned() -> std::string& {
        if (std::holds_alternative<std::string_view>(data_)) {
            auto sv = std::get<std::string_view>(data_);
            data_ = std::string(sv);
        }
        return std::get<std::string>(data_);
    }

    [[nodiscard]] auto is_view() const noexcept -> bool {
        return std::holds_alternative<std::string_view>(data_);
    }

    auto operator==(const json_string_data& other) const noexcept -> bool {
        return view() == other.view();
    }

    auto operator==(std::string_view sv) const noexcept -> bool {
        return view() == sv;
    }

    // Implicit conversion to string_view for seamless interop
    operator std::string_view() const noexcept { return view(); }

    // For STL container compat (e.g., unordered_map key hashing)
    [[nodiscard]] auto size() const noexcept -> size_t { return view().size(); }
    [[nodiscard]] auto length() const noexcept -> size_t { return view().length(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return view().empty(); }
    [[nodiscard]] auto data() const noexcept -> const char* { return view().data(); }

    auto clear() -> void { data_ = std::string_view{}; }
};

// JSON container type aliases using standard containers
using json_string = json_string_data;
using json_number = double;               // Default 64-bit float
using json_number_128 = __float128;       // Extended 128-bit float
using json_int_128 = __int128;            // 128-bit signed integer
using json_uint_128 = unsigned __int128;  // 128-bit unsigned integer
using json_boolean = bool;
using json_null = std::nullptr_t;            // Use nullptr_t for direct equivalence with nullptr
using json_array = std::vector<json_value>;  // Array as std::vector
using json_object =
    std::unordered_map<std::string, json_value>;  // Object as unordered_map with string keys

// Precision information for adaptive number parsing
struct number_precision_info {
    int significant_digits = 0;  // Count of significant digits
    int exponent = 0;            // Exponent value (0 if no exponent)
    bool has_decimal = false;    // Whether number has decimal point
    bool has_exponent = false;   // Whether number has exponent
    bool is_negative = false;    // Whether number is negative
    bool needs_128bit = false;   // Whether 128-bit precision is required
    bool is_integer = false;     // Whether number is an integer (no decimal/exponent)
};

// Internal storage for complex types to support Copy-On-Write (COW)
using json_array_ptr = std::shared_ptr<json_array>;
using json_object_ptr = std::shared_ptr<json_object>;

// JSON value variant type with 128-bit support and COW pointers
using json_data = std::variant<json_null, json_boolean, json_number, json_number_128, json_int_128,
                               json_uint_128, json_string_data, json_array_ptr, json_object_ptr>;

// Main JSON value class with thread-safe operations
class json_value {
private:
    json_data data_;

    // COW helper to ensure unique ownership before modification
    template<typename T, typename Ptr>
    auto ensure_unique(Ptr& ptr) -> T& {
        if (ptr.use_count() > 1) {
            ptr = std::make_shared<T>(*ptr);
        }
        return *ptr;
    }

    // Private helper methods for serialization
    auto serialize_to_buffer(std::string& buffer, int indent) const -> void;
    auto serialize_string_to_buffer(std::string& buffer, std::string_view str) const -> void;
    auto serialize_array_to_buffer(std::string& buffer, const json_array& arr, int indent) const
        -> void;
    auto serialize_object_to_buffer(std::string& buffer, const json_object& obj, int indent) const
        -> void;

public:
    // Constructors
    json_value() : data_(nullptr) {}  // Default to nullptr for null JSON values

    json_value(std::nullptr_t) : data_(nullptr) {}

    json_value(bool value) : data_(value) {}

    json_value(int value) : data_(static_cast<__int128>(value)) {}

    json_value(int64_t value) : data_(static_cast<__int128>(value)) {}

    json_value(uint64_t value) : data_(static_cast<unsigned __int128>(value)) {}

    json_value(double value) : data_(value) {}

    json_value(__float128 value) : data_(value) {}

    json_value(__int128 value) : data_(value) {}

    json_value(unsigned __int128 value) : data_(value) {}

    json_value(const char* value) : data_(json_string_data(std::string_view(value))) {}

    json_value(std::string_view value) : data_(json_string_data(value)) {}

    json_value(const std::string& value) : data_(json_string_data(value)) {}

    json_value(std::string&& value) : data_(json_string_data(std::move(value))) {}

    json_value(json_string_data value) : data_(std::move(value)) {}

    json_value(const json_array& array) : data_(std::make_shared<json_array>(array)) {}

    json_value(json_array&& array) : data_(std::make_shared<json_array>(std::move(array))) {}

    json_value(const json_object& object) : data_(std::make_shared<json_object>(object)) {}

    json_value(json_object&& object) : data_(std::make_shared<json_object>(std::move(object))) {}

    // Convenience COW constructors for pointers
    json_value(json_array_ptr array) : data_(std::move(array)) {}
    json_value(json_object_ptr object) : data_(std::move(object)) {}

    // Copy and move constructors
    json_value(const json_value&) = default;
    json_value(json_value&&) = default;
    json_value& operator=(const json_value&) = default;
    json_value& operator=(json_value&&) = default;

    // Type checking methods (declared here, implemented below)
    auto is_null() const noexcept -> bool;
    auto is_boolean() const noexcept -> bool;
    auto is_number() const noexcept -> bool;
    auto is_number_128() const noexcept -> bool;
    auto is_int_128() const noexcept -> bool;
    auto is_uint_128() const noexcept -> bool;
    auto is_string() const noexcept -> bool;
    auto is_array() const noexcept -> bool;
    auto is_object() const noexcept -> bool;

    // Value accessor methods (declared here, implemented below)
    auto as_boolean() const -> bool;
    auto as_number() const -> double;
    auto as_number_128() const -> __float128;
    auto as_int_128() const -> __int128;
    auto as_uint_128() const -> unsigned __int128;
    auto as_string() const -> std::string_view;
    auto as_string_data() const -> const json_string_data&;
    auto as_array() const -> const json_array&;
    auto as_object() const -> const json_object&;

    // Numeric conversion helpers with automatic type handling
    // These methods convert between numeric types intelligently
    auto as_int64() const -> int64_t;
    auto as_uint64() const -> uint64_t;
    auto as_float64() const -> double;
    auto as_int128() const -> __int128;
    auto as_uint128() const -> unsigned __int128;
    auto as_float128() const -> __float128;

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

auto json_value::is_number_128() const noexcept -> bool {
    return std::holds_alternative<__float128>(data_);
}

auto json_value::is_int_128() const noexcept -> bool {
    return std::holds_alternative<__int128>(data_);
}

auto json_value::is_uint_128() const noexcept -> bool {
    return std::holds_alternative<unsigned __int128>(data_);
}

auto json_value::is_string() const noexcept -> bool {
    return std::holds_alternative<json_string_data>(data_);
}

auto json_value::is_array() const noexcept -> bool {
    return std::holds_alternative<json_array_ptr>(data_);
}

auto json_value::is_object() const noexcept -> bool {
    return std::holds_alternative<json_object_ptr>(data_);
}

// Thread-safe value accessor functions
auto json_value::as_boolean() const -> bool {
    if (!is_boolean()) {
        throw std::runtime_error("JSON value is not a boolean");
    }
    return std::get<bool>(data_);
}

auto json_value::as_number() const -> double {
    // Try 64-bit double first (fast path)
    if (is_number()) {
        return std::get<double>(data_);
    }

    // Fallback: Convert from 128-bit types (with potential precision loss)
    if (is_number_128()) {
        return static_cast<double>(std::get<__float128>(data_));
    }
    if (is_int_128()) {
        return static_cast<double>(std::get<__int128>(data_));
    }
    if (is_uint_128()) {
        return static_cast<double>(std::get<unsigned __int128>(data_));
    }

    // Not a numeric type at all
    return std::numeric_limits<double>::quiet_NaN();
}

auto json_value::as_number_128() const -> __float128 {
    // Try 128-bit float first
    if (is_number_128()) {
        return std::get<__float128>(data_);
    }

    // Fallback: Convert from other numeric types (upcast or precision loss)
    if (is_number()) {
        return static_cast<__float128>(std::get<double>(data_));
    }
    if (is_int_128()) {
        return static_cast<__float128>(std::get<__int128>(data_));
    }
    if (is_uint_128()) {
        return static_cast<__float128>(std::get<unsigned __int128>(data_));
    }

    // Not a numeric type at all
    return static_cast<__float128>(std::numeric_limits<double>::quiet_NaN());
}

auto json_value::as_int_128() const -> __int128 {
    // Try signed 128-bit first
    if (is_int_128()) {
        return std::get<__int128>(data_);
    }
    // Fallback: Convert from other numeric types
    if (is_uint_128()) {
        return static_cast<__int128>(std::get<unsigned __int128>(data_));
    }
    if (is_number()) {
        const double val = std::get<double>(data_);
        if (std::isnan(val)) {
            return 0;
        }
        return static_cast<__int128>(val);
    }
    if (is_number_128()) {
        return static_cast<__int128>(std::get<__float128>(data_));
    }
    return 0;
}

auto json_value::as_uint_128() const -> unsigned __int128 {
    // Try unsigned 128-bit first
    if (is_uint_128()) {
        return std::get<unsigned __int128>(data_);
    }
    // Fallback: Convert from other numeric types
    if (is_int_128()) {
        return static_cast<unsigned __int128>(std::get<__int128>(data_));
    }
    if (is_number()) {
        double val = std::get<double>(data_);
        if (std::isnan(val))
            return 0;
        return static_cast<unsigned __int128>(val);
    }
    if (is_number_128()) {
        return static_cast<unsigned __int128>(std::get<__float128>(data_));
    }
    return 0;
}

auto json_value::as_string() const -> std::string_view {
    if (!is_string()) {
        throw std::runtime_error("JSON value is not a string");
    }
    return std::get<json_string_data>(data_).view();
}

auto json_value::as_string_data() const -> const json_string_data& {
    if (!is_string()) {
        throw std::runtime_error("JSON value is not a string");
    }
    return std::get<json_string_data>(data_);
}

// Numeric conversion helpers with automatic type handling
// Returns NaN for non-numeric types instead of throwing
auto json_value::as_int64() const -> int64_t {
    if (is_number()) {
        double val = std::get<double>(data_);
        if (std::isnan(val))
            return 0;
        return static_cast<int64_t>(val);
    } else if (is_int_128()) {
        return static_cast<int64_t>(std::get<__int128>(data_));
    } else if (is_uint_128()) {
        return static_cast<int64_t>(std::get<unsigned __int128>(data_));
    } else if (is_number_128()) {
        return static_cast<int64_t>(std::get<__float128>(data_));
    }
    return 0;  // Non-numeric type returns 0
}

auto json_value::as_uint64() const -> uint64_t {
    if (is_number()) {
        double val = std::get<double>(data_);
        if (std::isnan(val))
            return 0;
        return static_cast<uint64_t>(val);
    } else if (is_uint_128()) {
        return static_cast<uint64_t>(std::get<unsigned __int128>(data_));
    } else if (is_int_128()) {
        return static_cast<uint64_t>(std::get<__int128>(data_));
    } else if (is_number_128()) {
        return static_cast<uint64_t>(std::get<__float128>(data_));
    }
    return 0;  // Non-numeric type returns 0
}

auto json_value::as_float64() const -> double {
    if (is_number()) {
        return std::get<double>(data_);
    } else if (is_int_128()) {
        return static_cast<double>(std::get<__int128>(data_));
    } else if (is_uint_128()) {
        return static_cast<double>(std::get<unsigned __int128>(data_));
    } else if (is_number_128()) {
        return static_cast<double>(std::get<__float128>(data_));
    }
    return std::numeric_limits<double>::quiet_NaN();
}

auto json_value::as_int128() const -> __int128 {
    if (is_int_128()) {
        return std::get<__int128>(data_);
    } else if (is_uint_128()) {
        return static_cast<__int128>(std::get<unsigned __int128>(data_));
    }
    if (is_number()) {
        const double val = std::get<double>(data_);
        if (std::isnan(val)) {
            return 0;
        }
        return static_cast<__int128>(val);
    }
    if (is_number_128()) {
        return static_cast<__int128>(std::get<__float128>(data_));
    }
    return 0;  // Non-numeric type returns 0
}

auto json_value::as_uint128() const -> unsigned __int128 {
    if (is_uint_128()) {
        return std::get<unsigned __int128>(data_);
    }
    if (is_int_128()) {
        return static_cast<unsigned __int128>(std::get<__int128>(data_));
    }
    if (is_number()) {
        const double val = std::get<double>(data_);
        if (std::isnan(val)) {
            return 0;
        }
        return static_cast<unsigned __int128>(val);
    }
    if (is_number_128()) {
        return static_cast<unsigned __int128>(std::get<__float128>(data_));
    }
    return 0;  // Non-numeric type returns 0
}

auto json_value::as_float128() const -> __float128 {
    if (is_number_128()) {
        return std::get<__float128>(data_);
    } else if (is_number()) {
        return static_cast<__float128>(std::get<double>(data_));
    } else if (is_int_128()) {
        return static_cast<__float128>(std::get<__int128>(data_));
    } else if (is_uint_128()) {
        return static_cast<__float128>(std::get<unsigned __int128>(data_));
    }
    return static_cast<__float128>(std::numeric_limits<double>::quiet_NaN());
}

auto json_value::as_array() const -> const json_array& {
    if (!is_array()) {
        throw std::runtime_error("JSON value is not an array");
    }
    return *std::get<json_array_ptr>(data_);
}

auto json_value::as_object() const -> const json_object& {
    if (!is_object()) {
        throw std::runtime_error("JSON value is not an object");
    }
    return *std::get<json_object_ptr>(data_);
}

// Thread-safe mutable accessor functions with Copy-On-Write (COW)
auto json_value::as_array() -> json_array& {
    if (!is_array()) {
        data_ = std::make_shared<json_array>();
    }
    return ensure_unique<json_array>(std::get<json_array_ptr>(data_));
}

auto json_value::as_object() -> json_object& {
    if (!is_object()) {
        data_ = std::make_shared<json_object>();
    }
    return ensure_unique<json_object>(std::get<json_object_ptr>(data_));
}

// Thread-safe array operations with trail calling syntax
auto json_value::push_back(json_value value) -> json_value& {
    as_array().emplace_back(std::move(value));
    return *this;
}

auto json_value::pop_back() -> json_value& {
    auto& arr = as_array();
    if (arr.empty()) {
        throw std::runtime_error("Cannot pop_back on empty array");
    }
    arr.pop_back();
    return *this;
}

auto json_value::clear() -> json_value& {
    std::visit(
        [this](auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_array_ptr> || std::is_same_v<T, json_object_ptr>) {
                if (v.use_count() > 1) {
                    v = std::make_shared<typename T::element_type>();
                } else {
                    v->clear();
                }
            } else if constexpr (std::is_same_v<T, json_string_data>) {
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
            if constexpr (std::is_same_v<T, json_array_ptr> || std::is_same_v<T, json_object_ptr>) {
                return v->size();
            } else if constexpr (std::is_same_v<T, json_string_data>) {
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
            if constexpr (std::is_same_v<T, json_array_ptr> || std::is_same_v<T, json_object_ptr>) {
                return v->empty();
            } else if constexpr (std::is_same_v<T, json_string_data>) {
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
    as_object()[key] = std::move(value);
    return *this;
}

auto json_value::erase(const std::string& key) -> json_value& {
    as_object().erase(key);
    return *this;
}

auto json_value::contains(const std::string& key) const noexcept -> bool {
    if (!is_object()) {
        return false;
    }
    const auto& obj = as_object();
    return obj.find(key) != obj.end();
}

// Thread-safe indexing operators
auto json_value::operator[](size_t index) -> json_value& {
    auto& arr = as_array();
    if (index >= arr.size()) {
        throw std::out_of_range("Array index out of range");
    }
    return arr[index];
}

auto json_value::operator[](size_t index) const -> const json_value& {
    const auto& arr = as_array();
    if (index >= arr.size()) {
        throw std::out_of_range("Array index out of range");
    }
    return arr[index];
}

auto json_value::operator[](const std::string& key) -> json_value& {
    return as_object()[key];
}

auto json_value::operator[](const std::string& key) const -> const json_value& {
    const auto& obj = as_object();
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
            } else if constexpr (std::is_same_v<T, __float128>) {
                // Convert __float128 to long double for string conversion
                // This preserves most precision while using standard library
                long double ld_value = static_cast<long double>(v);
                thread_local std::array<char, 128> num_buffer;
                auto [ptr, ec] = std::to_chars(num_buffer.data(),
                                               num_buffer.data() + num_buffer.size(), ld_value);
                if (ec == std::errc{}) {
                    buffer.append(num_buffer.data(), ptr);
                }
            } else if constexpr (std::is_same_v<T, __int128>) {
                // Convert __int128 to string manually
                bool is_negative = v < 0;
                unsigned __int128 abs_val = is_negative ? -static_cast<unsigned __int128>(v)
                                                        : static_cast<unsigned __int128>(v);
                thread_local std::array<char, 64> num_buffer;
                char* ptr = num_buffer.data() + num_buffer.size();
                *--ptr = '\0';
                do {
                    *--ptr = '0' + (abs_val % 10);
                    abs_val /= 10;
                } while (abs_val > 0);
                if (is_negative) {
                    *--ptr = '-';
                }
                buffer += ptr;
            } else if constexpr (std::is_same_v<T, unsigned __int128>) {
                // Convert unsigned __int128 to string manually
                unsigned __int128 val = v;
                thread_local std::array<char, 64> num_buffer;
                char* ptr = num_buffer.data() + num_buffer.size();
                *--ptr = '\0';
                do {
                    *--ptr = '0' + (val % 10);
                    val /= 10;
                } while (val > 0);
                buffer += ptr;
            } else if constexpr (std::is_same_v<T, json_string_data>) {
                serialize_string_to_buffer(buffer, v.view());
            } else if constexpr (std::is_same_v<T, json_array_ptr>) {
                serialize_array_to_buffer(buffer, *v, indent);
            } else if constexpr (std::is_same_v<T, json_object_ptr>) {
                serialize_object_to_buffer(buffer, *v, indent);
            }
        },
        data_);
}

auto json_value::serialize_string_to_buffer(std::string& buffer, std::string_view str) const
    -> void {
    buffer += '"';

    const char* data = str.data();
    const size_t size = str.size();
    const char* ptr = data;
    const char* end = data + size;

    while (ptr < end) {
        // SIMD-accelerated escape position detection (delegates to GMF detail:: impl)
        const char* escape_pos = detail::find_escape_position_simd_impl(ptr, end);

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
// Stream output operator for json_value
auto operator<<(std::ostream& os, const json_value& value) -> std::ostream& {
    return os << value.to_string();
}

auto json_value::serialize_pretty_to_buffer(std::string& buffer, int indent_size,
                                            int current_indent) const -> void {
    std::visit(
        [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, json_array_ptr>) {
                serialize_pretty_array(buffer, *v, indent_size, current_indent);
            } else if constexpr (std::is_same_v<T, json_object_ptr>) {
                serialize_pretty_object(buffer, *v, indent_size, current_indent);
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

// Number Precision Analysis for Adaptive Parsing
// ============================================================================

// Analyze a JSON number string to determine precision requirements
inline auto analyze_number_precision(const char* start, const char* end) -> number_precision_info {
    number_precision_info info;
    const char* ptr = start;

    // Check for negative sign
    if (ptr < end && *ptr == '-') {
        info.is_negative = true;
        ++ptr;
    }

    // Count significant digits
    bool leading_zero = false;
    bool after_decimal = false;
    int digits_before_decimal = 0;
    int digits_after_decimal = 0;

    while (ptr < end && (*ptr >= '0' && *ptr <= '9')) {
        if (*ptr == '0' && digits_before_decimal == 0 && !after_decimal) {
            leading_zero = true;
        } else {
            ++digits_before_decimal;
        }
        ++ptr;
    }

    // Check for decimal point
    if (ptr < end && *ptr == '.') {
        info.has_decimal = true;
        after_decimal = true;
        ++ptr;

        while (ptr < end && (*ptr >= '0' && *ptr <= '9')) {
            ++digits_after_decimal;
            ++ptr;
        }
    }

    info.significant_digits = digits_before_decimal + digits_after_decimal;

    // Check for exponent
    if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
        info.has_exponent = true;
        ++ptr;

        bool exp_negative = false;
        if (ptr < end && (*ptr == '+' || *ptr == '-')) {
            exp_negative = (*ptr == '-');
            ++ptr;
        }

        int exp_value = 0;
        while (ptr < end && (*ptr >= '0' && *ptr <= '9')) {
            exp_value = exp_value * 10 + (*ptr - '0');
            ++ptr;
        }

        info.exponent = exp_negative ? -exp_value : exp_value;
    }

    // Determine if this is an integer (no decimal point, no exponent)
    info.is_integer = !info.has_decimal && !info.has_exponent;

    // Determine if 128-bit precision is needed
    // For floats: > 15 significant digits (double precision limit)
    // For exponents: outside ±308 range (double range)
    // For integers: > 2^63 - 1 (int64_t limit) or > 2^64 - 1 (uint64_t limit)
    if (info.is_integer) {
        // Check if integer fits in 64-bit
        if (digits_before_decimal > 19) {
            // Definitely needs 128-bit
            info.needs_128bit = true;
        } else if (digits_before_decimal == 19) {
            // Might need 128-bit, check exact value later
            info.needs_128bit = true;
        }
    } else {
        // Float precision check
        if (info.significant_digits > 15) {
            info.needs_128bit = true;
        }
        if (info.has_exponent && (info.exponent > 308 || info.exponent < -308)) {
            info.needs_128bit = true;
        }
    }

    return info;
}

// Parse __float128 from string using Clang's native support
inline auto parse_float128(const char* str, size_t length) -> std::optional<__float128> {
    if (length == 0 || length > 100) {
        return std::nullopt;
    }

    // Use strtold for parsing and convert to __float128
    // This preserves more precision than double
    thread_local std::array<char, 128> buffer;
    std::memcpy(buffer.data(), str, length);
    buffer[length] = '\0';

    char* endptr = nullptr;
    long double value = std::strtold(buffer.data(), &endptr);

    if (endptr != buffer.data() + length) {
        return std::nullopt;
    }

    return static_cast<__float128>(value);
}

// Parse __int128 from string manually
inline auto parse_int128(const char* str, size_t length, bool is_negative)
    -> std::optional<__int128> {
    if (length == 0 || length > 40) {  // Max 39 digits for __int128
        return std::nullopt;
    }

    unsigned __int128 value = 0;
    const char* ptr = str;
    const char* end = str + length;

    // Skip leading zeros
    while (ptr < end && *ptr == '0') {
        ++ptr;
    }

    // Parse digits
    while (ptr < end) {
        if (*ptr < '0' || *ptr > '9') {
            return std::nullopt;
        }

        unsigned __int128 digit = *ptr - '0';

        // Check for overflow
        unsigned __int128 old_value = value;
        value = value * 10 + digit;

        if (value < old_value) {
            return std::nullopt;  // Overflow
        }

        ++ptr;
    }

    if (is_negative) {
        // Check if value fits in signed __int128
        constexpr unsigned __int128 max_neg = static_cast<unsigned __int128>(1) << 127;
        if (value > max_neg) {
            return std::nullopt;
        }
        return -static_cast<__int128>(value);
    } else {
        // Check if value fits in signed __int128
        constexpr unsigned __int128 max_pos = (static_cast<unsigned __int128>(1) << 127) - 1;
        if (value > max_pos) {
            return std::nullopt;
        }
        return static_cast<__int128>(value);
    }
}

// Parse unsigned __int128 from string manually
inline auto parse_uint128(const char* str, size_t length) -> std::optional<unsigned __int128> {
    if (length == 0 || length > 40) {  // Max 39 digits for unsigned __int128
        return std::nullopt;
    }

    unsigned __int128 value = 0;
    const char* ptr = str;
    const char* end = str + length;

    // Skip leading zeros
    while (ptr < end && *ptr == '0') {
        ++ptr;
    }

    // Parse digits
    while (ptr < end) {
        if (*ptr < '0' || *ptr > '9') {
            return std::nullopt;
        }

        unsigned __int128 digit = *ptr - '0';

        // Check for overflow
        unsigned __int128 old_value = value;
        value = value * 10 + digit;

        if (value < old_value) {
            return std::nullopt;  // Overflow
        }

        ++ptr;
    }

    return value;
}

// Parser Class Definition
// ============================================================================

class parser {
public:
    explicit parser(std::string_view input);
    parser(std::string_view input, std::pmr::memory_resource* arena);
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
    auto parse_string_simd() -> json_result<json_string_data>;
    auto peek() const noexcept -> char;
    auto advance() noexcept -> char;
    auto match(char expected) noexcept -> bool;
    auto is_at_end() const noexcept -> bool;
    auto make_error(json_error_code code, std::string message) const -> json_error;

    // Arena-aware allocation helpers
    [[nodiscard]] auto make_array_ptr(json_array&& arr) -> json_array_ptr;
    [[nodiscard]] auto make_object_ptr(json_object&& obj) -> json_object_ptr;

    // Member variables
    const char* data_;
    const char* end_;
    const char* current_;
    size_t line_;
    size_t column_;
    size_t depth_;
    std::pmr::memory_resource* arena_ = nullptr;  // nullptr = use default heap
    static constexpr size_t max_depth_ = 1000;
};

// ============================================================================
// json_document: owns input buffer + arena + root value
// Ensures string_view lifetimes are valid as long as the document exists.
// ============================================================================

class json_document {
    std::string input_;                                                  // Owned input (string_view lifetime source)
    std::unique_ptr<std::pmr::monotonic_buffer_resource> arena_;         // Heap-allocated arena (movable)
    json_value root_;

public:
    explicit json_document(std::string input, size_t arena_hint = 0)
        : input_(std::move(input)),
          arena_(std::make_unique<std::pmr::monotonic_buffer_resource>(
              arena_hint > 0 ? arena_hint : input_.size() * 2)),
          root_(nullptr) {}

    json_document(json_document&&) = default;
    json_document& operator=(json_document&&) = default;
    json_document(const json_document&) = delete;
    json_document& operator=(const json_document&) = delete;

    [[nodiscard]] auto root() const noexcept -> const json_value& { return root_; }
    [[nodiscard]] auto root() noexcept -> json_value& { return root_; }
    [[nodiscard]] auto input() const noexcept -> std::string_view { return input_; }
    [[nodiscard]] auto arena() noexcept -> std::pmr::memory_resource* { return arena_.get(); }

    // Allow parse_arena to set root_
    friend auto parse_arena(std::string input) -> json_result<json_document>;
};

// Thread-safe JSON Parser Implementation
// ============================================================================

parser::parser(std::string_view input)
    : data_(input.data()), end_(input.data() + input.size()), current_(input.data()), line_(1),
      column_(1), depth_(0), arena_(nullptr) {}

parser::parser(std::string_view input, std::pmr::memory_resource* arena)
    : data_(input.data()), end_(input.data() + input.size()), current_(input.data()), line_(1),
      column_(1), depth_(0), arena_(arena) {}

auto parser::make_array_ptr(json_array&& arr) -> json_array_ptr {
    if (arena_) {
        return std::allocate_shared<json_array>(
            std::pmr::polymorphic_allocator<json_array>(arena_), std::move(arr));
    }
    return std::make_shared<json_array>(std::move(arr));
}

auto parser::make_object_ptr(json_object&& obj) -> json_object_ptr {
    if (arena_) {
        return std::allocate_shared<json_object>(
            std::pmr::polymorphic_allocator<json_object>(arena_), std::move(obj));
    }
    return std::make_shared<json_object>(std::move(obj));
}

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

    // Analyze precision requirements
    size_t length = current_ - start;
    auto precision_info = analyze_number_precision(start, current_);

    // Adaptive precision parsing strategy:
    // 1. Try 64-bit parsing first (fast path)
    // 2. If precision info indicates need for 128-bit, or if 64-bit parsing loses precision,
    // upgrade
    // 3. If 128-bit parsing also fails, return NaN

    if (!precision_info.needs_128bit) {
        // Fast path: Try 64-bit parsing first
        thread_local std::array<char, 64> buffer;
        if (length < buffer.size()) {
            std::memcpy(buffer.data(), start, length);
            buffer[length] = '\0';

            char* end_ptr;
            double value = std::strtod(buffer.data(), &end_ptr);

            if (end_ptr == buffer.data() + length) {
                // Verify no precision loss for integers
                if (precision_info.is_integer) {
                    // For integers, check if value exactly represents the parsed number
                    // by converting back to string and comparing
                    thread_local std::array<char, 32> verify_buffer;
                    auto [ptr, ec] = std::to_chars(verify_buffer.data(),
                                                   verify_buffer.data() + verify_buffer.size(),
                                                   value, std::chars_format::fixed);

                    if (ec == std::errc{}) {
                        std::string_view original(start, length);
                        std::string_view converted(verify_buffer.data(),
                                                   ptr - verify_buffer.data());

                        // If they match, no precision loss
                        if (original == converted) {
                            return json_value{value};
                        }
                        // Precision loss detected, upgrade to 128-bit
                    } else {
                        // Conversion failed, upgrade to 128-bit
                    }
                } else {
                    // For floats with <= 15 significant digits and exponent in range,
                    // 64-bit is sufficient
                    return json_value{value};
                }
            }
        }
    }

    // Slow path: 128-bit parsing required
    if (precision_info.is_integer) {
        // Try 128-bit integer parsing
        const char* num_start = start;
        if (*num_start == '-') {
            ++num_start;
        }

        auto result = parse_int128(num_start, current_ - num_start, precision_info.is_negative);
        if (result) {
            return json_value{*result};
        }

        // If signed parsing failed, try unsigned for positive numbers
        if (!precision_info.is_negative) {
            auto uresult = parse_uint128(num_start, current_ - num_start);
            if (uresult) {
                return json_value{*uresult};
            }
        }
    } else {
        // Try 128-bit float parsing
        auto result = parse_float128(start, length);
        if (result) {
            return json_value{*result};
        }
    }

    // All parsing attempts failed - return NaN for floats or error for integers
    if (precision_info.is_integer) {
        return std::unexpected(
            make_error(json_error_code::invalid_number, "Integer value exceeds 128-bit range"));
    } else {
        // Return NaN for floating point overflow
        return json_value{std::numeric_limits<double>::quiet_NaN()};
    }
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
        return json_value{make_array_ptr(std::move(array))};
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
    return json_value{make_array_ptr(std::move(array))};
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
        return json_value{make_object_ptr(std::move(object))};
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

        std::string key(key_result->as_string());

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
    return json_value{make_object_ptr(std::move(object))};
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

// find_string_end_simd and parse_string_simd — delegate to GMF detail:: implementation
auto parser::find_string_end_simd(const char* start) -> const char* {
    return detail::find_string_end_simd_impl(start, end_);
}

auto parser::parse_string_simd() -> json_result<json_string_data> {
    const char* start = current_;
    const char* string_end = find_string_end_simd(start);

    // Fast path: found closing quote with no escapes — zero-copy string_view
    if (string_end < end_ && *string_end == '"'
        && std::find(start, string_end, '\\') == string_end) {
        json_string_data result(std::string_view(start, string_end - start));
        current_ = string_end + 1;  // Skip the closing quote
        return result;
    }

    // Slow path: has escapes or control characters, fall back to regular parsing
    return std::unexpected(json_error{});
}

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

auto serializer::serialize_string(std::string_view value) -> void {
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

// Arena-based parsing: all allocations from a single monotonic arena.
// Returns json_document that owns the arena, input buffer, and root value.
// string_view lifetimes valid as long as json_document exists.
auto parse_arena(std::string input) -> json_result<json_document> {
    json_document doc(std::move(input));
    parser p(doc.input(), doc.arena());
    auto result = p.parse();
    if (!result) {
        return std::unexpected(result.error());
    }
    doc.root_ = std::move(*result);
    return std::move(doc);
}

// ============================================================================
// Ondemand/Lazy JSON Parsing — Two-Stage: SIMD structural index + lazy access
// Only materializes values on explicit get_*() calls. Zero-copy for strings.
// ============================================================================

enum class json_type : uint8_t {
    null_value, boolean_value, number_value,
    string_value, array_value, object_value
};

class ondemand_value;
class ondemand_object;
class ondemand_array;

class ondemand_document {
    std::string input_;
    std::vector<structural_index> tape_;
    size_t tape_pos_ = 0;

public:
    static auto parse(std::string input) -> json_result<ondemand_document>;
    [[nodiscard]] auto root() -> ondemand_value;
    [[nodiscard]] auto materialize() -> json_result<json_value>;
    [[nodiscard]] auto input() const noexcept -> std::string_view { return input_; }
    [[nodiscard]] auto tape_size() const noexcept -> size_t { return tape_.size(); }

    // Internal access for ondemand_value/object/array
    [[nodiscard]] auto raw_input() const noexcept -> const char* { return input_.data(); }
    [[nodiscard]] auto tape() const noexcept -> const std::vector<structural_index>& { return tape_; }
    auto set_tape_pos(size_t pos) noexcept -> void { tape_pos_ = pos; }
    [[nodiscard]] auto tape_pos() const noexcept -> size_t { return tape_pos_; }
    [[nodiscard]] auto input_size() const noexcept -> size_t { return input_.size(); }
};

class ondemand_value {
    ondemand_document* doc_;
    size_t tape_start_;

public:
    ondemand_value(ondemand_document* doc, size_t tape_pos) noexcept
        : doc_(doc), tape_start_(tape_pos) {}

    [[nodiscard]] auto type() const noexcept -> json_type;
    [[nodiscard]] auto get_string() const -> json_result<std::string_view>;
    [[nodiscard]] auto get_double() const -> json_result<double>;
    [[nodiscard]] auto get_int64() const -> json_result<int64_t>;
    [[nodiscard]] auto get_bool() const -> json_result<bool>;
    [[nodiscard]] auto is_null() const noexcept -> bool;
    [[nodiscard]] auto get_object() const -> json_result<ondemand_object>;
    [[nodiscard]] auto get_array() const -> json_result<ondemand_array>;

    // Skip past this value in the tape (for iteration)
    [[nodiscard]] auto skip() const -> size_t;
};

class ondemand_object {
    ondemand_document* doc_;
    size_t tape_start_;  // Points to '{'

public:
    ondemand_object(ondemand_document* doc, size_t tape_pos) noexcept
        : doc_(doc), tape_start_(tape_pos) {}

    [[nodiscard]] auto find_field(std::string_view key) const -> json_result<ondemand_value>;
    [[nodiscard]] auto operator[](std::string_view key) const -> json_result<ondemand_value>;
};

class ondemand_array {
    ondemand_document* doc_;
    size_t tape_start_;  // Points to '['

public:
    ondemand_array(ondemand_document* doc, size_t tape_pos) noexcept
        : doc_(doc), tape_start_(tape_pos) {}

    [[nodiscard]] auto count_elements() const -> size_t;

    // Simple indexed access
    [[nodiscard]] auto at(size_t index) const -> json_result<ondemand_value>;
};

// ============================================================================
// Ondemand Implementation
// ============================================================================

auto ondemand_document::parse(std::string input) -> json_result<ondemand_document> {
    ondemand_document doc;
    doc.input_ = std::move(input);
    doc.tape_ = build_structural_index(
        std::span<const char>(doc.input_.data(), doc.input_.size()));
    if (doc.tape_.empty()) {
        return std::unexpected(json_error{json_error_code::empty_input, "Empty or whitespace-only input", 0, 0});
    }
    return std::move(doc);
}

auto ondemand_document::root() -> ondemand_value {
    return ondemand_value(this, 0);
}

auto ondemand_document::materialize() -> json_result<json_value> {
    return fastjson::parse(input_);
}

// Helper: skip to the matching closing bracket/brace in the structural tape
inline auto skip_container(const std::vector<structural_index>& tape,
                           size_t start_pos) -> size_t {
    auto open_type = tape[start_pos].type;
    structural_type close_type;
    if (open_type == structural_type::left_brace)
        close_type = structural_type::right_brace;
    else if (open_type == structural_type::left_bracket)
        close_type = structural_type::right_bracket;
    else
        return start_pos + 1;  // Not a container

    int depth = 1;
    size_t pos = start_pos + 1;
    while (pos < tape.size() && depth > 0) {
        if (tape[pos].type == open_type) ++depth;
        else if (tape[pos].type == close_type) --depth;
        ++pos;
    }
    return pos;  // One past the closing bracket
}

auto ondemand_value::type() const noexcept -> json_type {
    if (tape_start_ >= doc_->tape().size()) return json_type::null_value;
    auto t = doc_->tape()[tape_start_].type;
    switch (t) {
        case structural_type::left_brace: return json_type::object_value;
        case structural_type::left_bracket: return json_type::array_value;
        case structural_type::quote: return json_type::string_value;
        case structural_type::null_start: return json_type::null_value;
        case structural_type::true_start:
        case structural_type::false_start: return json_type::boolean_value;
        case structural_type::number_start: return json_type::number_value;
        default: break;
    }
    // Check if the character at the position is a number
    size_t pos = doc_->tape()[tape_start_].position;
    if (pos < doc_->input_size()) {
        char c = doc_->raw_input()[pos];
        if (c == '-' || (c >= '0' && c <= '9')) return json_type::number_value;
    }
    return json_type::null_value;
}

auto ondemand_value::get_string() const -> json_result<std::string_view> {
    if (tape_start_ >= doc_->tape().size()) {
        return std::unexpected(json_error{json_error_code::invalid_string, "Invalid tape position", 0, 0});
    }
    if (doc_->tape()[tape_start_].type != structural_type::quote) {
        return std::unexpected(json_error{json_error_code::invalid_string, "Value is not a string", 0, 0});
    }
    size_t start = doc_->tape()[tape_start_].position + 1;  // Skip opening quote
    // Find closing quote in tape
    if (tape_start_ + 1 < doc_->tape().size() &&
        doc_->tape()[tape_start_ + 1].type == structural_type::quote) {
        size_t end = doc_->tape()[tape_start_ + 1].position;
        return std::string_view(doc_->raw_input() + start, end - start);
    }
    // Fallback: scan for closing quote
    const char* ptr = doc_->raw_input() + start;
    const char* input_end = doc_->raw_input() + doc_->input_size();
    while (ptr < input_end && *ptr != '"') {
        if (*ptr == '\\') ++ptr;  // Skip escaped char
        ++ptr;
    }
    return std::string_view(doc_->raw_input() + start, ptr - (doc_->raw_input() + start));
}

auto ondemand_value::get_double() const -> json_result<double> {
    if (tape_start_ >= doc_->tape().size()) {
        return std::unexpected(json_error{json_error_code::invalid_number, "Invalid tape position", 0, 0});
    }
    size_t pos = doc_->tape()[tape_start_].position;
    const char* start = doc_->raw_input() + pos;
    char* end_ptr = nullptr;
    double val = std::strtod(start, &end_ptr);
    if (end_ptr == start) {
        return std::unexpected(json_error{json_error_code::invalid_number, "Not a number", 0, 0});
    }
    return val;
}

auto ondemand_value::get_int64() const -> json_result<int64_t> {
    if (tape_start_ >= doc_->tape().size()) {
        return std::unexpected(json_error{json_error_code::invalid_number, "Invalid tape position", 0, 0});
    }
    size_t pos = doc_->tape()[tape_start_].position;
    const char* start = doc_->raw_input() + pos;
    char* end_ptr = nullptr;
    long long val = std::strtoll(start, &end_ptr, 10);
    if (end_ptr == start) {
        return std::unexpected(json_error{json_error_code::invalid_number, "Not an integer", 0, 0});
    }
    return static_cast<int64_t>(val);
}

auto ondemand_value::get_bool() const -> json_result<bool> {
    if (tape_start_ >= doc_->tape().size()) {
        return std::unexpected(json_error{json_error_code::invalid_syntax, "Invalid tape position", 0, 0});
    }
    auto t = doc_->tape()[tape_start_].type;
    if (t == structural_type::true_start) return true;
    if (t == structural_type::false_start) return false;
    return std::unexpected(json_error{json_error_code::invalid_syntax, "Value is not a boolean", 0, 0});
}

auto ondemand_value::is_null() const noexcept -> bool {
    if (tape_start_ >= doc_->tape().size()) return true;
    return doc_->tape()[tape_start_].type == structural_type::null_start;
}

auto ondemand_value::get_object() const -> json_result<ondemand_object> {
    if (tape_start_ >= doc_->tape().size() ||
        doc_->tape()[tape_start_].type != structural_type::left_brace) {
        return std::unexpected(json_error{json_error_code::invalid_syntax, "Value is not an object", 0, 0});
    }
    return ondemand_object(doc_, tape_start_);
}

auto ondemand_value::get_array() const -> json_result<ondemand_array> {
    if (tape_start_ >= doc_->tape().size() ||
        doc_->tape()[tape_start_].type != structural_type::left_bracket) {
        return std::unexpected(json_error{json_error_code::invalid_syntax, "Value is not an array", 0, 0});
    }
    return ondemand_array(doc_, tape_start_);
}

auto ondemand_value::skip() const -> size_t {
    if (tape_start_ >= doc_->tape().size()) return tape_start_;
    auto t = doc_->tape()[tape_start_].type;
    if (t == structural_type::left_brace || t == structural_type::left_bracket) {
        return skip_container(doc_->tape(), tape_start_);
    }
    if (t == structural_type::quote) {
        // Skip opening + closing quote pair
        return tape_start_ + 2;
    }
    // Primitive (null, true, false, number) = 1 tape entry
    return tape_start_ + 1;
}

auto ondemand_object::find_field(std::string_view key) const -> json_result<ondemand_value> {
    const auto& tape = doc_->tape();
    const char* input = doc_->raw_input();
    size_t input_size = doc_->input_size();

    // Start after the opening brace
    size_t pos = tape_start_ + 1;

    while (pos < tape.size() && tape[pos].type != structural_type::right_brace) {
        // Expect a string key (quote)
        if (tape[pos].type != structural_type::quote) {
            ++pos;
            continue;
        }

        // Extract the key
        size_t key_start = tape[pos].position + 1;
        size_t key_end = key_start;
        if (pos + 1 < tape.size() && tape[pos + 1].type == structural_type::quote) {
            key_end = tape[pos + 1].position;
        } else {
            // Scan for closing quote
            const char* ptr = input + key_start;
            const char* end = input + input_size;
            while (ptr < end && *ptr != '"') {
                if (*ptr == '\\') ++ptr;
                ++ptr;
            }
            key_end = ptr - input;
        }

        std::string_view field_key(input + key_start, key_end - key_start);

        // Skip past closing quote + colon
        pos += 2;  // Past both quote markers
        // Skip colon
        while (pos < tape.size() && tape[pos].type == structural_type::colon) ++pos;

        // Now pos points to the value
        if (field_key == key) {
            return ondemand_value(doc_, pos);
        }

        // Skip the value to get to the next field
        ondemand_value val(doc_, pos);
        pos = val.skip();

        // Skip comma
        while (pos < tape.size() && tape[pos].type == structural_type::comma) ++pos;
    }

    return std::unexpected(json_error{json_error_code::invalid_syntax,
                                       "Field not found: " + std::string(key), 0, 0});
}

auto ondemand_object::operator[](std::string_view key) const -> json_result<ondemand_value> {
    return find_field(key);
}

auto ondemand_array::count_elements() const -> size_t {
    const auto& tape = doc_->tape();
    size_t count = 0;
    size_t pos = tape_start_ + 1;

    if (pos >= tape.size() || tape[pos].type == structural_type::right_bracket) {
        return 0;
    }

    while (pos < tape.size() && tape[pos].type != structural_type::right_bracket) {
        ++count;
        ondemand_value val(doc_, pos);
        pos = val.skip();
        // Skip comma
        while (pos < tape.size() && tape[pos].type == structural_type::comma) ++pos;
    }
    return count;
}

auto ondemand_array::at(size_t index) const -> json_result<ondemand_value> {
    const auto& tape = doc_->tape();
    size_t pos = tape_start_ + 1;
    size_t current = 0;

    while (pos < tape.size() && tape[pos].type != structural_type::right_bracket) {
        if (current == index) {
            return ondemand_value(doc_, pos);
        }
        ondemand_value val(doc_, pos);
        pos = val.skip();
        // Skip comma
        while (pos < tape.size() && tape[pos].type == structural_type::comma) ++pos;
        ++current;
    }

    return std::unexpected(json_error{json_error_code::invalid_syntax, "Array index out of range", 0, 0});
}

// Convenience function for ondemand parsing
auto parse_ondemand(std::string input) -> json_result<ondemand_document> {
    return ondemand_document::parse(std::move(input));
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