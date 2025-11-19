// FastestJSONInTheWest - SIMD Structural Character Indexing
// Phase 1 of parallel JSON parsing: Build structural character tape
// Based on simdjson's approach but optimized for our use case
// ============================================================================

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <immintrin.h>

namespace fastjson {

// ============================================================================
// Structural Character Types
// ============================================================================

enum class structural_type : uint8_t {
    none = 0,
    left_brace = '{',     // 0x7B
    right_brace = '}',    // 0x7D
    left_bracket = '[',   // 0x5B
    right_bracket = ']',  // 0x5D
    comma = ',',          // 0x2C
    colon = ':',          // 0x3A
    quote = '"',          // 0x22
    // Primitives
    null_start = 'n',
    true_start = 't',
    false_start = 'f',
    number_start = '0'  // or '-'
};

struct structural_index {
    size_t position;       // Position in original string
    structural_type type;  // Type of structural character
    uint8_t padding[7];    // Align to 16 bytes for cache
};

// ============================================================================
// SIMD Structural Scanner - AVX2 Version
// ============================================================================

#if defined(__AVX2__)
__attribute__((target("avx2"))) inline auto
find_structural_chars_avx2(std::span<const char> input, std::vector<structural_index>& indices)
    -> void {
    const size_t len = input.size();
    const char* data = input.data();

    // Reserve space (heuristic: ~10% of input size for structural chars)
    indices.reserve(len / 10);

    // SIMD constants for structural characters
    const __m256i left_brace = _mm256_set1_epi8('{');
    const __m256i right_brace = _mm256_set1_epi8('}');
    const __m256i left_bracket = _mm256_set1_epi8('[');
    const __m256i right_bracket = _mm256_set1_epi8(']');
    const __m256i comma = _mm256_set1_epi8(',');
    const __m256i colon = _mm256_set1_epi8(':');
    const __m256i quote = _mm256_set1_epi8('"');
    const __m256i backslash = _mm256_set1_epi8('\\');

    bool in_string = false;
    bool prev_escape = false;

    size_t pos = 0;

    // Process 32 bytes at a time with AVX2
    while (pos + 32 <= len) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));

        if (!in_string) {
            // Find all structural characters
            __m256i is_left_brace = _mm256_cmpeq_epi8(chunk, left_brace);
            __m256i is_right_brace = _mm256_cmpeq_epi8(chunk, right_brace);
            __m256i is_left_bracket = _mm256_cmpeq_epi8(chunk, left_bracket);
            __m256i is_right_bracket = _mm256_cmpeq_epi8(chunk, right_bracket);
            __m256i is_comma = _mm256_cmpeq_epi8(chunk, comma);
            __m256i is_colon = _mm256_cmpeq_epi8(chunk, colon);
            __m256i is_quote = _mm256_cmpeq_epi8(chunk, quote);

            // Combine all structural characters
            __m256i structural =
                _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(is_left_brace, is_right_brace),
                                                _mm256_or_si256(is_left_bracket, is_right_bracket)),
                                _mm256_or_si256(_mm256_or_si256(is_comma, is_colon), is_quote));

            uint32_t mask = _mm256_movemask_epi8(structural);

            // Process each structural character found
            while (mask != 0) {
                int bit_pos = __builtin_ctz(mask);  // Count trailing zeros
                size_t char_pos = pos + bit_pos;
                char c = data[char_pos];

                if (c == '"') {
                    in_string = true;
                    indices.push_back({char_pos, structural_type::quote, {}});
                } else {
                    indices.push_back({char_pos, static_cast<structural_type>(c), {}});
                }

                mask &= mask - 1;  // Clear lowest set bit
            }
        } else {
            // In string: look for closing quote (handle escapes)
            __m256i is_quote = _mm256_cmpeq_epi8(chunk, quote);
            __m256i is_backslash = _mm256_cmpeq_epi8(chunk, backslash);

            uint32_t quote_mask = _mm256_movemask_epi8(is_quote);
            uint32_t backslash_mask = _mm256_movemask_epi8(is_backslash);

            // Simple escape handling (TODO: optimize with carry propagation)
            for (int i = 0; i < 32 && pos + i < len; i++) {
                if (prev_escape) {
                    prev_escape = false;
                    continue;
                }

                if (backslash_mask & (1u << i)) {
                    prev_escape = true;
                } else if (quote_mask & (1u << i)) {
                    in_string = false;
                    indices.push_back({pos + i, structural_type::quote, {}});
                    break;
                }
            }
        }

        pos += 32;
    }

    // Handle remaining bytes sequentially
    while (pos < len) {
        char c = data[pos];

        if (in_string) {
            if (prev_escape) {
                prev_escape = false;
            } else if (c == '\\') {
                prev_escape = true;
            } else if (c == '"') {
                in_string = false;
                indices.push_back({pos, structural_type::quote, {}});
            }
        } else {
            switch (c) {
                case '{':
                case '}':
                case '[':
                case ']':
                case ',':
                case ':':
                    indices.push_back({pos, static_cast<structural_type>(c), {}});
                    break;
                case '"':
                    in_string = true;
                    indices.push_back({pos, structural_type::quote, {}});
                    break;
            }
        }

        pos++;
    }
}
#endif  // __AVX2__

// ============================================================================
// SIMD Structural Scanner - SSE4.2 Version
// ============================================================================

#if defined(__SSE4_2__)
__attribute__((target("sse4.2"))) inline auto
find_structural_chars_sse42(std::span<const char> input, std::vector<structural_index>& indices)
    -> void {
    const size_t len = input.size();
    const char* data = input.data();

    indices.reserve(len / 10);

    // SSE version processes 16 bytes at a time
    // Similar to AVX2 but with 128-bit registers
    // Implementation similar to above but with _mm_ intrinsics

    bool in_string = false;
    bool prev_escape = false;

    size_t pos = 0;

    // For now, fall back to scalar (TODO: implement SSE4.2 version)
    while (pos < len) {
        char c = data[pos];

        if (in_string) {
            if (prev_escape) {
                prev_escape = false;
            } else if (c == '\\') {
                prev_escape = true;
            } else if (c == '"') {
                in_string = false;
                indices.push_back({pos, structural_type::quote, {}});
            }
        } else {
            switch (c) {
                case '{':
                case '}':
                case '[':
                case ']':
                case ',':
                case ':':
                    indices.push_back({pos, static_cast<structural_type>(c), {}});
                    break;
                case '"':
                    in_string = true;
                    indices.push_back({pos, structural_type::quote, {}});
                    break;
            }
        }

        pos++;
    }
}
#endif  // __SSE4_2__

// ============================================================================
// Scalar Fallback Version
// ============================================================================

inline auto find_structural_chars_scalar(std::span<const char> input,
                                         std::vector<structural_index>& indices) -> void {
    const size_t len = input.size();
    const char* data = input.data();

    indices.reserve(len / 10);

    bool in_string = false;
    bool prev_escape = false;

    for (size_t pos = 0; pos < len; pos++) {
        char c = data[pos];

        if (in_string) {
            if (prev_escape) {
                prev_escape = false;
            } else if (c == '\\') {
                prev_escape = true;
            } else if (c == '"') {
                in_string = false;
                indices.push_back({pos, structural_type::quote, {}});
            }
        } else {
            switch (c) {
                case '{':
                case '}':
                case '[':
                case ']':
                case ',':
                case ':':
                    indices.push_back({pos, static_cast<structural_type>(c), {}});
                    break;
                case '"':
                    in_string = true;
                    indices.push_back({pos, structural_type::quote, {}});
                    break;
            }
        }
    }
}

// ============================================================================
// Dispatcher - Selects Best SIMD Version
// ============================================================================

inline auto build_structural_index(std::span<const char> input) -> std::vector<structural_index> {
    std::vector<structural_index> indices;

#if defined(__AVX2__)
    find_structural_chars_avx2(input, indices);
#elif defined(__SSE4_2__)
    find_structural_chars_sse42(input, indices);
#else
    find_structural_chars_scalar(input, indices);
#endif

    return indices;
}

}  // namespace fastjson
