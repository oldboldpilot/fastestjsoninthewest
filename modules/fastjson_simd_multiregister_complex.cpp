#include "fastjson_simd_multiregister.h"
#include <cstring>
#include <algorithm>

namespace fastjson::simd::multi {

// ============================================================================
// Multi-Register Whitespace Skipping Implementations
// ============================================================================

#if HAVE_AVX2
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
        __m256i ws0_space = _mm256_cmpeq_epi8(chunk0, space);
        __m256i ws0_tab = _mm256_cmpeq_epi8(chunk0, tab);
        __m256i ws0_newline = _mm256_cmpeq_epi8(chunk0, newline);
        __m256i ws0_carriage = _mm256_cmpeq_epi8(chunk0, carriage);
        
        __m256i ws1_space = _mm256_cmpeq_epi8(chunk1, space);
        __m256i ws1_tab = _mm256_cmpeq_epi8(chunk1, tab);
        __m256i ws1_newline = _mm256_cmpeq_epi8(chunk1, newline);
        __m256i ws1_carriage = _mm256_cmpeq_epi8(chunk1, carriage);
        
        __m256i ws2_space = _mm256_cmpeq_epi8(chunk2, space);
        __m256i ws2_tab = _mm256_cmpeq_epi8(chunk2, tab);
        __m256i ws2_newline = _mm256_cmpeq_epi8(chunk2, newline);
        __m256i ws2_carriage = _mm256_cmpeq_epi8(chunk2, carriage);
        
        __m256i ws3_space = _mm256_cmpeq_epi8(chunk3, space);
        __m256i ws3_tab = _mm256_cmpeq_epi8(chunk3, tab);
        __m256i ws3_newline = _mm256_cmpeq_epi8(chunk3, newline);
        __m256i ws3_carriage = _mm256_cmpeq_epi8(chunk3, carriage);
        
        // OR all whitespace comparisons for each chunk
        __m256i is_ws0 = _mm256_or_si256(_mm256_or_si256(ws0_space, ws0_tab),
                                         _mm256_or_si256(ws0_newline, ws0_carriage));
        __m256i is_ws1 = _mm256_or_si256(_mm256_or_si256(ws1_space, ws1_tab),
                                         _mm256_or_si256(ws1_newline, ws1_carriage));
        __m256i is_ws2 = _mm256_or_si256(_mm256_or_si256(ws2_space, ws2_tab),
                                         _mm256_or_si256(ws2_newline, ws2_carriage));
        __m256i is_ws3 = _mm256_or_si256(_mm256_or_si256(ws3_space, ws3_tab),
                                         _mm256_or_si256(ws3_newline, ws3_carriage));
        
        // Generate masks for all 4 chunks
        uint32_t mask0 = _mm256_movemask_epi8(is_ws0);
        uint32_t mask1 = _mm256_movemask_epi8(is_ws1);
        uint32_t mask2 = _mm256_movemask_epi8(is_ws2);
        uint32_t mask3 = _mm256_movemask_epi8(is_ws3);
        
        // Check for first non-whitespace in any of the 4 chunks
        if (mask0 != 0xFFFFFFFF) {
            return pos + __builtin_ctz(~mask0);
        }
        if (mask1 != 0xFFFFFFFF) {
            return pos + 32 + __builtin_ctz(~mask1);
        }
        if (mask2 != 0xFFFFFFFF) {
            return pos + 64 + __builtin_ctz(~mask2);
        }
        if (mask3 != 0xFFFFFFFF) {
            return pos + 96 + __builtin_ctz(~mask3);
        }
        
        pos += 128;
    }
    
    // Process remaining bytes with single AVX2 register
    while (pos + 32 <= size) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        __m256i space = _mm256_set1_epi8(' ');
        __m256i tab = _mm256_set1_epi8('\t');
        __m256i newline = _mm256_set1_epi8('\n');
        __m256i carriage = _mm256_set1_epi8('\r');
        
        __m256i is_ws = _mm256_or_si256(_mm256_or_si256(_mm256_cmpeq_epi8(chunk, space),
                                                        _mm256_cmpeq_epi8(chunk, tab)),
                                        _mm256_or_si256(_mm256_cmpeq_epi8(chunk, newline),
                                                        _mm256_cmpeq_epi8(chunk, carriage)));
        
        uint32_t mask = _mm256_movemask_epi8(is_ws);
        if (mask != 0xFFFFFFFF) {
            return pos + __builtin_ctz(~mask);
        }
        pos += 32;
    }
    
    // Scalar fallback for remaining bytes
    while (pos < size && (data[pos] == ' ' || data[pos] == '\t' || 
                          data[pos] == '\n' || data[pos] == '\r')) {
        pos++;
    }
    
    return pos;
}
#endif // HAVE_AVX2

#if HAVE_AVX512F && HAVE_AVX512BW
__attribute__((target("avx512f,avx512bw"))) 
auto skip_whitespace_8x_avx512(const char* data, size_t size, size_t start_pos) -> size_t {
    size_t pos = start_pos;
    
    // Process 512 bytes (8 x 64-byte AVX-512 registers) per iteration
    while (pos + 512 <= size) {
        // Load 8 AVX-512 registers in parallel
        __m512i chunks[8];
        for (int i = 0; i < 8; ++i) {
            chunks[i] = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(data + pos + i * 64));
        }
        
        // Process all 8 chunks in parallel
        __mmask64 ws_masks[8];
        for (int i = 0; i < 8; ++i) {
            __mmask64 space_mask = _mm512_cmpeq_epi8_mask(chunks[i], _mm512_set1_epi8(' '));
            __mmask64 tab_mask = _mm512_cmpeq_epi8_mask(chunks[i], _mm512_set1_epi8('\t'));
            __mmask64 newline_mask = _mm512_cmpeq_epi8_mask(chunks[i], _mm512_set1_epi8('\n'));
            __mmask64 carriage_mask = _mm512_cmpeq_epi8_mask(chunks[i], _mm512_set1_epi8('\r'));
            
            ws_masks[i] = space_mask | tab_mask | newline_mask | carriage_mask;
        }
        
        // Check for first non-whitespace in any of the 8 chunks
        for (int i = 0; i < 8; ++i) {
            if (ws_masks[i] != 0xFFFFFFFFFFFFFFFFULL) {
                return pos + i * 64 + __builtin_ctzll(~ws_masks[i]);
            }
        }
        
        pos += 512;
    }
    
    // Fallback to 4x AVX2 for remaining bytes
    return skip_whitespace_4x_avx2(data, size, pos);
}

// ============================================================================
// Multi-Register String End Finding
// ============================================================================

    
    return pos;
}
#endif // HAVE_AVX512F && HAVE_AVX512BW

// ============================================================================
// Multi-Register String End Finding
// ============================================================================

#if HAVE_AVX2
__attribute__((target("avx2"))) 
auto find_string_end_4x_avx2(const char* data, size_t size, size_t start_pos) -> size_t {
    size_t pos = start_pos;
    
    // Control characters and special characters to detect
    __m256i quote = _mm256_set1_epi8('"');
    __m256i backslash = _mm256_set1_epi8('\\');
    __m256i control_sub = _mm256_set1_epi8(0x1F);  // For unsigned comparison via saturating subtract
    
    // Process 128 bytes (4 x 32-byte registers) per iteration
    while (pos + 128 <= size) {
        __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        __m256i chunk2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 64));
        __m256i chunk3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 96));
        
        // Unsigned comparison: is_control = (chunk < 0x20) using saturating subtract
        // _mm256_subs_epu8(chunk, 0x1F) == 0 when chunk <= 0x1F (i.e., chunk < 0x20)
        __m256i zero = _mm256_setzero_si256();
        __m256i ctrl0 = _mm256_cmpeq_epi8(_mm256_subs_epu8(chunk0, control_sub), zero);
        __m256i ctrl1 = _mm256_cmpeq_epi8(_mm256_subs_epu8(chunk1, control_sub), zero);
        __m256i ctrl2 = _mm256_cmpeq_epi8(_mm256_subs_epu8(chunk2, control_sub), zero);
        __m256i ctrl3 = _mm256_cmpeq_epi8(_mm256_subs_epu8(chunk3, control_sub), zero);
        
        // Check all special conditions for each chunk
        __m256i special0 = _mm256_or_si256(
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk0, quote), _mm256_cmpeq_epi8(chunk0, backslash)),
            ctrl0);
        __m256i special1 = _mm256_or_si256(
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk1, quote), _mm256_cmpeq_epi8(chunk1, backslash)),
            ctrl1);
        __m256i special2 = _mm256_or_si256(
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk2, quote), _mm256_cmpeq_epi8(chunk2, backslash)),
            ctrl2);
        __m256i special3 = _mm256_or_si256(
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk3, quote), _mm256_cmpeq_epi8(chunk3, backslash)),
            ctrl3);
        
        // Generate masks
        uint32_t mask0 = _mm256_movemask_epi8(special0);
        uint32_t mask1 = _mm256_movemask_epi8(special1);
        uint32_t mask2 = _mm256_movemask_epi8(special2);
        uint32_t mask3 = _mm256_movemask_epi8(special3);
        
        // Find first special character in any chunk
        if (mask0 != 0) return pos + __builtin_ctz(mask0);
        if (mask1 != 0) return pos + 32 + __builtin_ctz(mask1);
        if (mask2 != 0) return pos + 64 + __builtin_ctz(mask2);
        if (mask3 != 0) return pos + 96 + __builtin_ctz(mask3);
        
        pos += 128;
    }
    
    // Single register fallback
    while (pos + 32 <= size) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        __m256i ctrl = _mm256_cmpeq_epi8(_mm256_subs_epu8(chunk, control_sub), _mm256_setzero_si256());
        __m256i special = _mm256_or_si256(
            _mm256_or_si256(_mm256_cmpeq_epi8(chunk, quote), _mm256_cmpeq_epi8(chunk, backslash)),
            ctrl);
        
        uint32_t mask = _mm256_movemask_epi8(special);
        if (mask != 0) {
            return pos + __builtin_ctz(mask);
        }
        pos += 32;
    }
    
    // Scalar fallback
    while (pos < size) {
        char c = data[pos];
        if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
            return pos;
        }
        pos++;
    }
    
    return pos;
}

__attribute__((target("avx512f,avx512bw"))) 
auto find_string_end_8x_avx512(const char* data, size_t size, size_t start_pos) -> size_t {
    size_t pos = start_pos;
    
    // Process 512 bytes (8 x 64-byte registers) per iteration
    while (pos + 512 <= size) {
        __m512i chunks[8];
        __mmask64 special_masks[8];
        
        // Load and process all 8 chunks in parallel
        for (int i = 0; i < 8; ++i) {
            chunks[i] = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(data + pos + i * 64));
            
            __mmask64 quote_mask = _mm512_cmpeq_epi8_mask(chunks[i], _mm512_set1_epi8('"'));
            __mmask64 backslash_mask = _mm512_cmpeq_epi8_mask(chunks[i], _mm512_set1_epi8('\\'));
            __mmask64 control_mask = _mm512_cmplt_epi8_mask(chunks[i], _mm512_set1_epi8(0x20));
            
            special_masks[i] = quote_mask | backslash_mask | control_mask;
        }
        
        // Find first special character
        for (int i = 0; i < 8; ++i) {
            if (special_masks[i] != 0) {
                return pos + i * 64 + __builtin_ctzll(special_masks[i]);
            }
        }
        
        pos += 512;
    }
    
    return find_string_end_4x_avx2(data, size, pos);
}

// ============================================================================
// Multi-Register Number Validation
// ============================================================================

__attribute__((target("avx2"))) 
auto validate_number_chars_4x_avx2(const char* data, size_t start_pos, size_t end_pos) -> bool {
    size_t pos = start_pos;
    
    // Valid number character ranges and specific characters
    __m256i digit_0 = _mm256_set1_epi8('0' - 1);
    __m256i digit_9 = _mm256_set1_epi8('9' + 1);
    __m256i minus = _mm256_set1_epi8('-');
    __m256i plus = _mm256_set1_epi8('+');
    __m256i dot = _mm256_set1_epi8('.');
    __m256i e_lower = _mm256_set1_epi8('e');
    __m256i e_upper = _mm256_set1_epi8('E');
    
    // Process 128 bytes (4 x 32-byte registers) per iteration
    while (pos + 128 <= end_pos) {
        __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        __m256i chunk2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 64));
        __m256i chunk3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 96));
        
        // Validate all 4 chunks in parallel
        for (int i = 0; i < 4; ++i) {
            __m256i chunk = (i == 0) ? chunk0 : (i == 1) ? chunk1 : (i == 2) ? chunk2 : chunk3;
            
            // Check digit range
            __m256i gt_digit_0 = _mm256_cmpgt_epi8(chunk, digit_0);
            __m256i lt_digit_9 = _mm256_cmpgt_epi8(digit_9, chunk);
            __m256i is_digit = _mm256_and_si256(gt_digit_0, lt_digit_9);
            
            // Check special characters
            __m256i is_minus = _mm256_cmpeq_epi8(chunk, minus);
            __m256i is_plus = _mm256_cmpeq_epi8(chunk, plus);
            __m256i is_dot = _mm256_cmpeq_epi8(chunk, dot);
            __m256i is_e_lower = _mm256_cmpeq_epi8(chunk, e_lower);
            __m256i is_e_upper = _mm256_cmpeq_epi8(chunk, e_upper);
            
            __m256i valid = _mm256_or_si256(is_digit,
                _mm256_or_si256(_mm256_or_si256(is_minus, is_plus),
                    _mm256_or_si256(_mm256_or_si256(is_dot, is_e_lower), is_e_upper)));
            
            uint32_t mask = _mm256_movemask_epi8(valid);
            if (mask != 0xFFFFFFFF) {
                return false;
            }
        }
        
        pos += 128;
    }
    
    // Single register validation for remaining bytes
    while (pos + 32 <= end_pos) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        
        __m256i gt_digit_0 = _mm256_cmpgt_epi8(chunk, digit_0);
        __m256i lt_digit_9 = _mm256_cmpgt_epi8(digit_9, chunk);
        __m256i is_digit = _mm256_and_si256(gt_digit_0, lt_digit_9);
        
        __m256i valid = _mm256_or_si256(is_digit,
            _mm256_or_si256(_mm256_or_si256(_mm256_cmpeq_epi8(chunk, minus),
                                            _mm256_cmpeq_epi8(chunk, plus)),
                _mm256_or_si256(_mm256_or_si256(_mm256_cmpeq_epi8(chunk, dot),
                                                _mm256_cmpeq_epi8(chunk, e_lower)),
                                _mm256_cmpeq_epi8(chunk, e_upper))));
        
        uint32_t mask = _mm256_movemask_epi8(valid);
        if (mask != 0xFFFFFFFF) {
            return false;
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

// ============================================================================
// Multi-Register Structural Character Detection
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
        // Process 4 chunks in parallel
        for (int chunk_idx = 0; chunk_idx < 4 && result.count < 60; ++chunk_idx) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + chunk_idx * 32));
            
            // Find all structural characters
            __m256i match_open_brace = _mm256_cmpeq_epi8(chunk, open_brace);
            __m256i match_close_brace = _mm256_cmpeq_epi8(chunk, close_brace);
            __m256i match_open_bracket = _mm256_cmpeq_epi8(chunk, open_bracket);
            __m256i match_close_bracket = _mm256_cmpeq_epi8(chunk, close_bracket);
            __m256i match_colon = _mm256_cmpeq_epi8(chunk, colon);
            __m256i match_comma = _mm256_cmpeq_epi8(chunk, comma);
            
            uint32_t mask_open_brace = _mm256_movemask_epi8(match_open_brace);
            uint32_t mask_close_brace = _mm256_movemask_epi8(match_close_brace);
            uint32_t mask_open_bracket = _mm256_movemask_epi8(match_open_bracket);
            uint32_t mask_close_bracket = _mm256_movemask_epi8(match_close_bracket);
            uint32_t mask_colon = _mm256_movemask_epi8(match_colon);
            uint32_t mask_comma = _mm256_movemask_epi8(match_comma);
            
            // Process each type of structural character
            size_t chunk_base = pos + chunk_idx * 32;
            
            while (mask_open_brace && result.count < 64) {
                int bit_pos = __builtin_ctz(mask_open_brace);
                result.positions[result.count] = chunk_base + bit_pos;
                result.types[result.count] = 1;  // '{'
                result.count++;
                mask_open_brace &= (mask_open_brace - 1);  // Clear lowest set bit
            }
            
            while (mask_close_brace && result.count < 64) {
                int bit_pos = __builtin_ctz(mask_close_brace);
                result.positions[result.count] = chunk_base + bit_pos;
                result.types[result.count] = 2;  // '}'
                result.count++;
                mask_close_brace &= (mask_close_brace - 1);
            }
            
            while (mask_open_bracket && result.count < 64) {
                int bit_pos = __builtin_ctz(mask_open_bracket);
                result.positions[result.count] = chunk_base + bit_pos;
                result.types[result.count] = 3;  // '['
                result.count++;
                mask_open_bracket &= (mask_open_bracket - 1);
            }
            
            while (mask_close_bracket && result.count < 64) {
                int bit_pos = __builtin_ctz(mask_close_bracket);
                result.positions[result.count] = chunk_base + bit_pos;
                result.types[result.count] = 4;  // ']'
                result.count++;
                mask_close_bracket &= (mask_close_bracket - 1);
            }
            
            while (mask_colon && result.count < 64) {
                int bit_pos = __builtin_ctz(mask_colon);
                result.positions[result.count] = chunk_base + bit_pos;
                result.types[result.count] = 5;  // ':'
                result.count++;
                mask_colon &= (mask_colon - 1);
            }
            
            while (mask_comma && result.count < 64) {
                int bit_pos = __builtin_ctz(mask_comma);
                result.positions[result.count] = chunk_base + bit_pos;
                result.types[result.count] = 6;  // ','
                result.count++;
                mask_comma &= (mask_comma - 1);
            }
        }
        
        pos += 128;
    }
    
    return pos;
}

// ============================================================================
// Dispatch Functions
// ============================================================================

auto skip_whitespace_multi(const char* data, size_t size, size_t start_pos) -> size_t {
#if defined(__x86_64__) || defined(_M_X64)
    // Try AVX-512 first
    if (__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw")) {
        return skip_whitespace_8x_avx512(data, size, start_pos);
    }
    // Fallback to AVX2
    if (__builtin_cpu_supports("avx2")) {
        return skip_whitespace_4x_avx2(data, size, start_pos);
    }
#endif
    
    // Scalar fallback
    size_t pos = start_pos;
    while (pos < size && (data[pos] == ' ' || data[pos] == '\t' || 
                          data[pos] == '\n' || data[pos] == '\r')) {
        pos++;
    }
    return pos;
}

auto find_string_end_multi(const char* data, size_t size, size_t start_pos) -> size_t {
#if defined(__x86_64__) || defined(_M_X64)
    if (__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw")) {
        return find_string_end_8x_avx512(data, size, start_pos);
    }
    if (__builtin_cpu_supports("avx2")) {
        return find_string_end_4x_avx2(data, size, start_pos);
    }
#endif
    
    // Scalar fallback
    size_t pos = start_pos;
    while (pos < size) {
        char c = data[pos];
        if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
            return pos;
        }
        pos++;
    }
    return pos;
}

auto validate_number_chars_multi(const char* data, size_t start_pos, size_t end_pos) -> bool {
#if defined(__x86_64__) || defined(_M_X64)
    if (__builtin_cpu_supports("avx2")) {
        return validate_number_chars_4x_avx2(data, start_pos, end_pos);
    }
#endif
    
    // Scalar fallback
    for (size_t pos = start_pos; pos < end_pos; ++pos) {
        char c = data[pos];
        if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')) {
            return false;
        }
    }
    return true;
}

auto find_structural_chars_multi(const char* data, size_t size, size_t start_pos, 
                                 StructuralChars& result) -> size_t {
#if defined(__x86_64__) || defined(_M_X64)
    if (__builtin_cpu_supports("avx2")) {
        return find_structural_chars_4x_avx2(data, size, start_pos, result);
    }
#endif
    
    // Scalar fallback
    size_t pos = start_pos;
    result.count = 0;
    
    while (pos < size && result.count < 64) {
        char c = data[pos];
        uint8_t type = 0;
        
        switch (c) {
            case '{': type = 1; break;
            case '}': type = 2; break;
            case '[': type = 3; break;
            case ']': type = 4; break;
            case ':': type = 5; break;
            case ',': type = 6; break;
            default: pos++; continue;
        }
        
        result.positions[result.count] = pos;
        result.types[result.count] = type;
        result.count++;
        pos++;
    }
    
    return pos;
}
#endif // HAVE_AVX2

// ============================================================================
// Auto-Dispatch Functions - Choose Best Implementation 
// ============================================================================

auto skip_whitespace_multi(const char* data, size_t size, size_t start_pos) -> size_t {
#if HAVE_AVX512F && HAVE_AVX512BW
    if constexpr (has_avx512_support()) {
        return skip_whitespace_8x_avx512(data, size, start_pos);
    }
#endif
#if HAVE_AVX2
    if constexpr (has_avx2_support()) {
        return skip_whitespace_4x_avx2(data, size, start_pos);
    }
#endif
    // Fallback to scalar implementation
    for (size_t pos = start_pos; pos < size; ++pos) {
        char c = data[pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return pos;
        }
    }
    return size;
}

auto find_string_end_multi(const char* data, size_t size, size_t start_pos) -> size_t {
#if HAVE_AVX512F && HAVE_AVX512BW
    if constexpr (has_avx512_support()) {
        return find_string_end_8x_avx512(data, size, start_pos);
    }
#endif
#if HAVE_AVX2
    if constexpr (has_avx2_support()) {
        return find_string_end_4x_avx2(data, size, start_pos);
    }
#endif
    // Fallback to scalar implementation
    bool escaped = false;
    for (size_t pos = start_pos; pos < size; ++pos) {
        char c = data[pos];
        if (escaped) {
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return pos;
        }
    }
    return size;
}

auto validate_number_chars_multi(const char* data, size_t start_pos, size_t end_pos) -> bool {
#if HAVE_AVX512F && HAVE_AVX512BW
    if constexpr (has_avx512_support()) {
        return validate_number_chars_8x_avx512(data, start_pos, end_pos);
    }
#endif
#if HAVE_AVX2
    if constexpr (has_avx2_support()) {
        return validate_number_chars_4x_avx2(data, start_pos, end_pos);
    }
#endif
    // Fallback to scalar implementation
    for (size_t pos = start_pos; pos < end_pos; ++pos) {
        char c = data[pos];
        if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || 
              c == 'e' || c == 'E')) {
            return false;
        }
    }
    return true;
}

}  // namespace fastjson::simd::multi