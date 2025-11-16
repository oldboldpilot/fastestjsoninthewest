// FastestJSONInTheWest SIMD Module Implementation
// Author: Olumuyiwa Oluwasanmi
// C++23 module for SIMD-optimized JSON operations

export module fastjson.simd;

import <cstddef>;
import <cstring>;
import <bit>;

#ifdef __AVX2__
#include <immintrin.h>
#endif

export namespace fastjson::simd {

// ============================================================================
// SIMD Configuration and Detection  
// ============================================================================

export constexpr bool has_sse2() noexcept {
#ifdef __SSE2__
    return true;
#else
    return false;
#endif
}

export constexpr bool has_avx2() noexcept {
#ifdef __AVX2__
    return true;
#else
    return false;
#endif
}

export constexpr bool has_avx512() noexcept {
#ifdef __AVX512F__
    return true;
#else
    return false;
#endif
}

// ============================================================================
// SIMD Whitespace Skipping
// ============================================================================

#ifdef __AVX2__
export size_t skip_whitespace_avx2(const char* data, size_t length) noexcept {
    const char* start = data;
    const char* end = data + length;
    
    // Process 32 bytes at a time with AVX2
    while (data + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data));
        
        // Check for space, tab, newline, carriage return
        __m256i space = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(' '));
        __m256i tab = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\t'));
        __m256i newline = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\n'));
        __m256i cr = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\r'));
        
        // Combine all whitespace checks
        __m256i whitespace = _mm256_or_si256(_mm256_or_si256(space, tab), _mm256_or_si256(newline, cr));
        
        // Check if all bytes are whitespace
        uint32_t mask = _mm256_movemask_epi8(whitespace);
        if (mask != 0xFFFFFFFF) {
            // Found non-whitespace, find first non-whitespace byte
            int first_non_ws = __builtin_ctz(~mask);
            return (data - start) + first_non_ws;
        }
        
        data += 32;
    }
    
    // Handle remaining bytes
    while (data < end && (*data == ' ' || *data == '\t' || *data == '\n' || *data == '\r')) {
        ++data;
    }
    
    return data - start;
}
#endif

// ============================================================================
// SIMD String Search and Validation
// ============================================================================

#ifdef __AVX2__
export size_t find_string_end_avx2(const char* data, size_t length) noexcept {
    const char* start = data;
    const char* end = data + length;
    
    while (data + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data));
        
        // Look for quote and backslash
        __m256i quote = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('"'));
        __m256i backslash = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\\'));
        
        uint32_t quote_mask = _mm256_movemask_epi8(quote);
        uint32_t backslash_mask = _mm256_movemask_epi8(backslash);
        
        if (quote_mask || backslash_mask) {
            // Found quote or backslash, need to handle escapes manually
            break;
        }
        
        data += 32;
    }
    
    return data - start;
}
#endif

// ============================================================================
// SIMD Number Validation
// ============================================================================

#ifdef __AVX2__
export bool validate_number_avx2(const char* data, size_t length) noexcept {
    const char* end = data + length;
    
    while (data + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data));
        
        // Check for valid number characters: 0-9, +, -, ., e, E
        __m256i ge_0 = _mm256_cmpgt_epi8(chunk, _mm256_set1_epi8('0' - 1));
        __m256i le_9 = _mm256_cmpgt_epi8(_mm256_set1_epi8('9' + 1), chunk);
        __m256i digits = _mm256_and_si256(ge_0, le_9);
        
        __m256i plus = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('+'));
        __m256i minus = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('-'));
        __m256i dot = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('.'));
        __m256i e_lower = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('e'));
        __m256i e_upper = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('E'));
        
        __m256i valid = _mm256_or_si256(digits, _mm256_or_si256(plus, minus));
        valid = _mm256_or_si256(valid, _mm256_or_si256(dot, _mm256_or_si256(e_lower, e_upper)));
        
        uint32_t mask = _mm256_movemask_epi8(valid);
        if (mask != 0xFFFFFFFF) {
            return false; // Invalid character found
        }
        
        data += 32;
    }
    
    // Validate remaining bytes
    while (data < end) {
        char c = *data;
        if (!((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E')) {
            return false;
        }
        ++data;
    }
    
    return true;
}
#endif

// ============================================================================
// Fallback Scalar Implementations
// ============================================================================

export size_t skip_whitespace_scalar(const char* data, size_t length) noexcept {
    size_t pos = 0;
    while (pos < length && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\n' || data[pos] == '\r')) {
        ++pos;
    }
    return pos;
}

export size_t find_string_end_scalar(const char* data, size_t length) noexcept {
    size_t pos = 0;
    while (pos < length && data[pos] != '"' && data[pos] != '\\') {
        if (static_cast<unsigned char>(data[pos]) < 0x20) {
            break; // Invalid control character
        }
        ++pos;
    }
    return pos;
}

// ============================================================================
// Adaptive SIMD Function Selection
// ============================================================================

export size_t skip_whitespace_adaptive(const char* data, size_t length) noexcept {
#ifdef __AVX2__
    if constexpr (has_avx2()) {
        return skip_whitespace_avx2(data, length);
    }
#endif
    return skip_whitespace_scalar(data, length);
}

export size_t find_string_end_adaptive(const char* data, size_t length) noexcept {
#ifdef __AVX2__
    if constexpr (has_avx2()) {
        return find_string_end_avx2(data, length);
    }
#endif
    return find_string_end_scalar(data, length);
}

export bool validate_number_adaptive(const char* data, size_t length) noexcept {
#ifdef __AVX2__
    if constexpr (has_avx2()) {
        return validate_number_avx2(data, length);
    }
#endif
    // Fallback scalar validation
    for (size_t i = 0; i < length; ++i) {
        char c = data[i];
        if (!((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E')) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Memory Operations
// ============================================================================

export void fast_memcpy(void* dest, const void* src, size_t n) noexcept {
#ifdef __AVX2__
    if (n >= 32 && has_avx2()) {
        const char* s = static_cast<const char*>(src);
        char* d = static_cast<char*>(dest);
        
        // Copy 32-byte chunks
        while (n >= 32) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(d), chunk);
            s += 32;
            d += 32;
            n -= 32;
        }
        
        // Copy remaining bytes
        std::memcpy(d, s, n);
        return;
    }
#endif
    std::memcpy(dest, src, n);
}

} // namespace fastjson::simd
