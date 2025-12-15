// FastestJSONInTheWest - Multi-Register SIMD Module
// Copyright (c) 2025 - High-performance JSON parsing with multi-register SIMD
// ============================================================================

module;

// Global module fragment - all includes must be here
#include <cstdint>
#include <span>

#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
#endif

#ifdef __ARM_NEON
    #include <arm_neon.h>
#endif

export module fastjson_simd_multiregister;

namespace fastjson::simd::multi {

// ============================================================================
// Structural Characters Result 
// ============================================================================

export struct StructuralChars {
    uint64_t positions[64];  // Up to 64 structural char positions per chunk
    uint8_t types[64];       // Character types (1='{', 2='}', 3='[', etc.)
    size_t count;
};

// ============================================================================
// CPU Capability Detection
// ============================================================================

export auto has_avx2_support() -> bool;
export auto has_avx512_support() -> bool;

// ============================================================================
// Multi-Register SIMD Functions with Auto-Dispatch
// ============================================================================

// Whitespace skipping with automatic CPU feature selection
export auto skip_whitespace_multiregister(const char* data, size_t size, size_t start_pos) -> size_t;

// String scanning with automatic CPU feature selection  
export auto find_string_end_multiregister(const char* data, size_t size, size_t start_pos) -> size_t;

// Number validation with automatic CPU feature selection
export auto validate_number_chars_multiregister(const char* data, size_t start_pos, size_t end_pos) -> bool;

// Structural character detection with automatic CPU feature selection
export auto find_structural_chars_multiregister(const char* data, size_t size, size_t start_pos) -> StructuralChars;

// ============================================================================
// Specific SIMD Implementation Functions (Advanced Use)
// ============================================================================

// AVX2 implementations (4x parallel registers, 128 bytes)
export auto skip_whitespace_4x_avx2(const char* data, size_t size, size_t start_pos) -> size_t;
export auto find_string_end_4x_avx2(const char* data, size_t size, size_t start_pos) -> size_t;
export auto validate_number_chars_4x_avx2(const char* data, size_t start_pos, size_t end_pos) -> bool;
export auto find_structural_chars_4x_avx2(const char* data, size_t size, size_t start_pos, StructuralChars& result) -> size_t;

// AVX-512 implementations (8x parallel registers, 512 bytes)  
export auto skip_whitespace_8x_avx512(const char* data, size_t size, size_t start_pos) -> size_t;
export auto find_string_end_8x_avx512(const char* data, size_t size, size_t start_pos) -> size_t;
export auto validate_number_chars_8x_avx512(const char* data, size_t start_pos, size_t end_pos) -> bool;
export auto find_structural_chars_8x_avx512(const char* data, size_t size, size_t start_pos, StructuralChars& result) -> size_t;

#ifdef __ARM_NEON
// NEON implementations (4x parallel registers, 64 bytes)
export auto skip_whitespace_4x_neon(const char* data, size_t size, size_t start_pos) -> size_t;
export auto find_string_end_4x_neon(const char* data, size_t size, size_t start_pos) -> size_t;
export auto validate_number_chars_4x_neon(const char* data, size_t size, size_t start_pos) -> size_t;
export auto find_structural_chars_4x_neon(const char* data, size_t size, size_t start_pos) -> StructuralChars;
#endif

} // namespace fastjson::simd::multi