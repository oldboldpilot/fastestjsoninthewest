// FastestJSONInTheWest - Multi-Register SIMD Implementation
// Copyright (c) 2025 - High-performance JSON parsing with multi-register SIMD
// ============================================================================

module;

// Global module fragment - includes must be here
#include <cstdint>
#include <cstring>
#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
    #ifdef __GNUC__
        #include <cpuid.h>
    #endif
#endif

module fastjson_simd_multiregister;

namespace fastjson::simd::multi {

// ============================================================================
// CPU Feature Detection
// ============================================================================

bool has_avx2_support() {
#if defined(__x86_64__) || defined(_M_X64)
    #ifdef __GNUC__
        unsigned int eax, ebx, ecx, edx;
        if (__get_cpuid_max(0, nullptr) >= 7) {
            __cpuid_count(7, 0, eax, ebx, ecx, edx);
            return (ebx & (1 << 5)) != 0;  // AVX2 bit
        }
    #endif
#endif
    return false;
}

bool has_avx512_support() {
#if defined(__x86_64__) || defined(_M_X64)
    #ifdef __GNUC__
        unsigned int eax, ebx, ecx, edx;
        if (__get_cpuid_max(0, nullptr) >= 7) {
            __cpuid_count(7, 0, eax, ebx, ecx, edx);
            return (ebx & (1 << 16)) != 0 && // AVX-512F
                   (ebx & (1 << 30)) != 0;   // AVX-512BW
        }
    #endif
#endif
    return false;
}

// ============================================================================
// Multi-Register Whitespace Skipping (AVX2)
// ============================================================================

__attribute__((target("avx2")))
auto skip_whitespace_4x_avx2(const char* data, size_t size, size_t start_pos) -> size_t {
    size_t pos = start_pos;
    
    // Process 128 bytes (4 x 32-byte AVX2 registers) per iteration
    while (pos + 128 <= size) {
        // Load 4 AVX2 registers in parallel
        __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        __m256i chunk2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 64));
        __m256i chunk3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 96));
        
        // Whitespace comparison vectors
        __m256i space = _mm256_set1_epi8(' ');
        __m256i tab = _mm256_set1_epi8('\t');
        __m256i newline = _mm256_set1_epi8('\n');
        __m256i carriage = _mm256_set1_epi8('\r');
        
        // Compare all 4 chunks in parallel
        __m256i ws0 = _mm256_or_si256(
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk0, space), _mm256_cmpeq_epi8(chunk0, tab)),
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk0, newline), _mm256_cmpeq_epi8(chunk0, carriage))
        );
        __m256i ws1 = _mm256_or_si256(
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk1, space), _mm256_cmpeq_epi8(chunk1, tab)),
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk1, newline), _mm256_cmpeq_epi8(chunk1, carriage))
        );
        __m256i ws2 = _mm256_or_si256(
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk2, space), _mm256_cmpeq_epi8(chunk2, tab)),
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk2, newline), _mm256_cmpeq_epi8(chunk2, carriage))
        );
        __m256i ws3 = _mm256_or_si256(
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk3, space), _mm256_cmpeq_epi8(chunk3, tab)),
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk3, newline), _mm256_cmpeq_epi8(chunk3, carriage))
        );
        
        // Create masks for each register
        uint32_t mask0 = ~_mm256_movemask_epi8(ws0);
        uint32_t mask1 = ~_mm256_movemask_epi8(ws1);
        uint32_t mask2 = ~_mm256_movemask_epi8(ws2);
        uint32_t mask3 = ~_mm256_movemask_epi8(ws3);
        
        // Find first non-whitespace character
        if (mask0 != 0) {
            return pos + __builtin_ctz(mask0);
        }
        pos += 32;
        if (mask1 != 0) {
            return pos + __builtin_ctz(mask1);
        }
        pos += 32;
        if (mask2 != 0) {
            return pos + __builtin_ctz(mask2);
        }
        pos += 32;
        if (mask3 != 0) {
            return pos + __builtin_ctz(mask3);
        }
        pos += 32;
    }
    
    // Fallback to scalar processing for remaining bytes
    while (pos < size) {
        char c = data[pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return pos;
        }
        pos++;
    }
    
    return pos;
}

// ============================================================================
// Multi-Register String End Finding (AVX2)
// ============================================================================

__attribute__((target("avx2")))
auto find_string_end_4x_avx2(const char* data, size_t size, size_t start_pos) -> size_t {
    size_t pos = start_pos;
    
    // Process 128 bytes with 4 AVX2 registers
    while (pos + 128 <= size) {
        __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        __m256i chunk2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 64));
        __m256i chunk3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 96));
        
        __m256i quote = _mm256_set1_epi8('\"');
        __m256i escape = _mm256_set1_epi8('\\');
        
        // Find quotes and escapes in parallel
        uint32_t quote_mask0 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk0, quote));
        uint32_t quote_mask1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, quote));
        uint32_t quote_mask2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk2, quote));
        uint32_t quote_mask3 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk3, quote));
        
        // Check for any quotes found
        if (quote_mask0 | quote_mask1 | quote_mask2 | quote_mask3) {
            // Fall back to scalar processing for proper escape handling
            break;
        }
        
        pos += 128;
    }
    
    // Scalar fallback for proper quote/escape handling
    bool escaped = false;
    while (pos < size) {
        char c = data[pos];
        if (escaped) {
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '\"') {
            return pos;
        }
        pos++;
    }
    
    return size;
}

// ============================================================================
// Number Validation (AVX2)
// ============================================================================

__attribute__((target("avx2")))
auto validate_number_chars_4x_avx2(const char* data, size_t start_pos, size_t end_pos) -> bool {
    size_t pos = start_pos;
    
    // Valid number characters
    __m256i digit0 = _mm256_set1_epi8('0');
    __m256i digit9 = _mm256_set1_epi8('9');
    __m256i dot = _mm256_set1_epi8('.');
    __m256i minus = _mm256_set1_epi8('-');
    __m256i plus = _mm256_set1_epi8('+');
    __m256i exp_e = _mm256_set1_epi8('e');
    __m256i exp_E = _mm256_set1_epi8('E');
    
    while (pos + 128 <= end_pos) {
        __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        __m256i chunk2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 64));
        __m256i chunk3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 96));
        
        // Check if all characters are valid number characters
        auto validate_chunk = [&](__m256i chunk) -> bool {
            __m256i is_digit = _mm256_and_si256(
                _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(digit0, _mm256_set1_epi8(1))),
                _mm256_cmpgt_epi8(_mm256_add_epi8(digit9, _mm256_set1_epi8(1)), chunk)
            );
            __m256i is_special = _mm256_or_si256(
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk, dot), _mm256_cmpeq_epi8(chunk, minus)),
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk, plus), 
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk, exp_e), _mm256_cmpeq_epi8(chunk, exp_E)))
            );
            __m256i valid = _mm256_or_si256(is_digit, is_special);
            return _mm256_movemask_epi8(valid) == 0xFFFFFFFF;
        };
        
        if (!validate_chunk(chunk0) || !validate_chunk(chunk1) || 
            !validate_chunk(chunk2) || !validate_chunk(chunk3)) {
            return false;
        }
        
        pos += 128;
    }
    
    // Scalar fallback for remaining characters
    while (pos < end_pos) {
        char c = data[pos];
        if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || 
              c == 'e' || c == 'E')) {
            return false;
        }
        pos++;
    }
    
    return true;
}

// ============================================================================
// Structural Character Detection (AVX2)
// ============================================================================

__attribute__((target("avx2")))
auto find_structural_chars_4x_avx2(const char* data, size_t size, size_t start_pos, 
                                   StructuralChars& result) -> size_t {
    size_t pos = start_pos;
    result.count = 0;
    
    __m256i open_brace = _mm256_set1_epi8('{');
    __m256i close_brace = _mm256_set1_epi8('}');
    __m256i open_bracket = _mm256_set1_epi8('[');
    __m256i close_bracket = _mm256_set1_epi8(']');
    __m256i colon = _mm256_set1_epi8(':');
    __m256i comma = _mm256_set1_epi8(',');
    
    while (pos + 128 <= size && result.count < 60) {
        __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        __m256i chunk2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 64));
        __m256i chunk3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 96));
        
        // Find structural characters in all chunks
        auto process_chunk = [&](const __m256i& chunk, size_t base_pos) {
            uint32_t open_brace_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, open_brace));
            uint32_t close_brace_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, close_brace));
            uint32_t open_bracket_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, open_bracket));
            uint32_t close_bracket_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, close_bracket));
            uint32_t colon_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, colon));
            uint32_t comma_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, comma));
            
            uint32_t any_structural = open_brace_mask | close_brace_mask | open_bracket_mask | 
                                     close_bracket_mask | colon_mask | comma_mask;
            
            while (any_structural && result.count < 64) {
                int bit_pos = __builtin_ctz(any_structural);
                size_t char_pos = base_pos + bit_pos;
                
                result.positions[result.count] = char_pos;
                
                if (open_brace_mask & (1U << bit_pos)) result.types[result.count] = 1; // '{'
                else if (close_brace_mask & (1U << bit_pos)) result.types[result.count] = 2; // '}'
                else if (open_bracket_mask & (1U << bit_pos)) result.types[result.count] = 3; // '['
                else if (close_bracket_mask & (1U << bit_pos)) result.types[result.count] = 4; // ']'
                else if (colon_mask & (1U << bit_pos)) result.types[result.count] = 5; // ':'
                else if (comma_mask & (1U << bit_pos)) result.types[result.count] = 6; // ','
                
                result.count++;
                any_structural &= ~(1U << bit_pos);
            }
        };
        
        process_chunk(chunk0, pos);
        process_chunk(chunk1, pos + 32);
        process_chunk(chunk2, pos + 64);
        process_chunk(chunk3, pos + 96);
        
        pos += 128;
    }
    
    return pos;
}

// ============================================================================
// Auto-Dispatch Functions
// ============================================================================

auto skip_whitespace_multi(const char* data, size_t size, size_t start_pos) -> size_t {
    if (has_avx2_support()) {
        return skip_whitespace_4x_avx2(data, size, start_pos);
    }
    
    // Scalar fallback
    for (size_t pos = start_pos; pos < size; ++pos) {
        char c = data[pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return pos;
        }
    }
    return size;
}

auto find_string_end_multi(const char* data, size_t size, size_t start_pos) -> size_t {
    if (has_avx2_support()) {
        return find_string_end_4x_avx2(data, size, start_pos);
    }
    
    // Scalar fallback
    bool escaped = false;
    for (size_t pos = start_pos; pos < size; ++pos) {
        char c = data[pos];
        if (escaped) {
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '\"') {
            return pos;
        }
    }
    return size;
}

auto validate_number_chars_multi(const char* data, size_t start_pos, size_t end_pos) -> bool {
    if (has_avx2_support()) {
        return validate_number_chars_4x_avx2(data, start_pos, end_pos);
    }
    
    // Scalar fallback
    for (size_t pos = start_pos; pos < end_pos; ++pos) {
        char c = data[pos];
        if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || 
              c == 'e' || c == 'E')) {
            return false;
        }
    }
    return true;
}

auto find_structural_chars_multi(const char* data, size_t size, size_t start_pos, 
                                 StructuralChars& result) -> size_t {
    if (has_avx2_support()) {
        return find_structural_chars_4x_avx2(data, size, start_pos, result);
    }
    
    // Scalar fallback
    result.count = 0;
    for (size_t pos = start_pos; pos < size && result.count < 64; ++pos) {
        char c = data[pos];
        uint8_t type = 0;
        
        switch (c) {
            case '{': type = 1; break;
            case '}': type = 2; break;
            case '[': type = 3; break;
            case ']': type = 4; break;
            case ':': type = 5; break;
            case ',': type = 6; break;
            default: continue;
        }
        
        result.positions[result.count] = pos;
        result.types[result.count] = type;
        result.count++;
    }
    
    return size;
}

// ============================================================================
// Auto-Dispatch Functions (Runtime CPU Detection)
// ============================================================================

auto skip_whitespace_multiregister(const char* data, size_t size, size_t start_pos) -> size_t {
    if (has_avx2_support()) {
        return skip_whitespace_4x_avx2(data, size, start_pos);
    }
    
    // Scalar fallback
    size_t pos = start_pos;
    while (pos < size && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\n' || data[pos] == '\r')) {
        ++pos;
    }
    return pos;
}

auto find_string_end_multiregister(const char* data, size_t size, size_t start_pos) -> size_t {
    if (has_avx2_support()) {
        return find_string_end_4x_avx2(data, size, start_pos);
    }
    
    // Scalar fallback
    for (size_t pos = start_pos; pos < size; ++pos) {
        if (data[pos] == '"' && (pos == start_pos || data[pos-1] != '\\')) {
            return pos;
        }
    }
    return size;
}

auto validate_number_chars_multiregister(const char* data, size_t start_pos, size_t end_pos) -> bool {
    if (has_avx2_support()) {
        return validate_number_chars_4x_avx2(data, start_pos, end_pos);
    }
    
    // Scalar fallback - validate all characters are valid number chars
    for (size_t pos = start_pos; pos < end_pos; ++pos) {
        char c = data[pos];
        if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E')) {
            return false;
        }
    }
    return true;
}

auto find_structural_chars_multiregister(const char* data, size_t size, size_t start_pos) -> StructuralChars {
    StructuralChars result{};
    
    if (has_avx2_support()) {
        find_structural_chars_4x_avx2(data, size, start_pos, result);
    } else {
        // Scalar fallback
        result.count = 0;
        for (size_t pos = start_pos; pos < size && result.count < 64; ++pos) {
            char c = data[pos];
            uint8_t type = 0;
            
            switch (c) {
                case '{': type = 1; break;
                case '}': type = 2; break;
                case '[': type = 3; break;
                case ']': type = 4; break;
                case ':': type = 5; break;
                case ',': type = 6; break;
                default: continue;
            }
            
            result.positions[result.count] = pos;
            result.types[result.count] = type;
            result.count++;
        }
    }
    
    return result;
}

// ============================================================================
// AVX-512 Stub Implementations (For Future Development)
// ============================================================================

auto skip_whitespace_8x_avx512(const char* data, size_t size, size_t start_pos) -> size_t {
    // Fallback to AVX2 until AVX-512 is available and tested
    return skip_whitespace_4x_avx2(data, size, start_pos);
}

auto find_string_end_8x_avx512(const char* data, size_t size, size_t start_pos) -> size_t {
    // Fallback to AVX2 until AVX-512 is available and tested  
    return find_string_end_4x_avx2(data, size, start_pos);
}

auto validate_number_chars_8x_avx512(const char* data, size_t start_pos, size_t end_pos) -> bool {
    // Fallback to AVX2 until AVX-512 is available and tested
    return validate_number_chars_4x_avx2(data, start_pos, end_pos);
}

auto find_structural_chars_8x_avx512(const char* data, size_t size, size_t start_pos, StructuralChars& result) -> size_t {
    // Fallback to AVX2 until AVX-512 is available and tested
    return find_structural_chars_4x_avx2(data, size, start_pos, result);
}

} // namespace fastjson::simd::multi