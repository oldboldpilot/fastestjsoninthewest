#pragma once

// Multi-Register SIMD Optimizations for JSON Parsing
// Uses multiple SIMD registers in parallel for maximum throughput
// Conditional compilation based on detected CPU features

#include <cstdint>
#include <span>

// Feature detection - these are set by CMake based on runtime CPU detection
#ifndef HAVE_AVX2
    #define HAVE_AVX2 0
#endif

#ifndef HAVE_AVX512F
    #define HAVE_AVX512F 0
#endif

#ifndef HAVE_AVX512BW
    #define HAVE_AVX512BW 0
#endif

#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
#endif

#ifdef __ARM_NEON
    #include <arm_neon.h>
#endif

namespace fastjson::simd::multi {

// ============================================================================
// CPU Capability Detection
// ============================================================================

// Check what SIMD level this build supports
constexpr bool has_avx2_support() { return HAVE_AVX2; }
constexpr bool has_avx512_support() { return HAVE_AVX512F && HAVE_AVX512BW; }

// ============================================================================
// Multi-Register Whitespace Skipping
// ============================================================================

#if HAVE_AVX2
// Use 4 AVX2 registers (128 bytes) for parallel whitespace detection
__attribute__((target("avx2"))) 
auto skip_whitespace_4x_avx2(const char* data, size_t size, size_t start_pos) -> size_t;
#endif

#if HAVE_AVX512F && HAVE_AVX512BW
// Use 8 AVX-512 registers (512 bytes) for maximum throughput
__attribute__((target("avx512f,avx512bw"))) 
auto skip_whitespace_8x_avx512(const char* data, size_t size, size_t start_pos) -> size_t;
#endif

#ifdef __ARM_NEON
auto skip_whitespace_4x_neon(const char* data, size_t size, size_t start_pos) -> size_t;
#endif

// ============================================================================
// Multi-Register String Scanning
// ============================================================================

#if HAVE_AVX2
// Parallel quote/escape/control character detection using multiple registers
__attribute__((target("avx2"))) 
auto find_string_end_4x_avx2(const char* data, size_t size, size_t start_pos) -> size_t;
#endif

#if HAVE_AVX512F && HAVE_AVX512BW
__attribute__((target("avx512f,avx512bw"))) 
auto find_string_end_8x_avx512(const char* data, size_t size, size_t start_pos) -> size_t;
#endif

#ifdef __ARM_NEON
auto find_string_end_4x_neon(const char* data, size_t size, size_t start_pos) -> size_t;
#endif

// ============================================================================
// Multi-Register Number Validation
// ============================================================================

#if HAVE_AVX2
// Validate multiple 64-byte chunks in parallel for number characters
__attribute__((target("avx2"))) 
auto validate_number_chars_4x_avx2(const char* data, size_t start_pos, size_t end_pos) -> bool;
#endif

#if HAVE_AVX512F && HAVE_AVX512BW
__attribute__((target("avx512f,avx512bw"))) 
auto validate_number_chars_8x_avx512(const char* data, size_t start_pos, size_t end_pos) -> bool;
#endif

// ============================================================================
// Multi-Register Structural Character Detection
// ============================================================================

// Find multiple JSON structural characters ({}[],:) using parallel SIMD
struct StructuralChars {
    uint64_t positions[64];  // Up to 64 structural char positions per chunk
    uint8_t types[64];       // Character types (1='{', 2='}', 3='[', etc.)
    size_t count;
};

#if HAVE_AVX2
__attribute__((target("avx2"))) 
auto find_structural_chars_4x_avx2(const char* data, size_t size, size_t start_pos, 
                                   StructuralChars& result) -> size_t;
#endif

#if HAVE_AVX512F && HAVE_AVX512BW
__attribute__((target("avx512f,avx512bw"))) 
auto find_structural_chars_8x_avx512(const char* data, size_t size, size_t start_pos,
                                     StructuralChars& result) -> size_t;
#endif

// ============================================================================
// Multi-Register Unicode Validation
// ============================================================================

#if HAVE_AVX2
// Validate UTF-8 sequences using multiple registers for parallel processing
__attribute__((target("avx2"))) 
auto validate_utf8_4x_avx2(const char* data, size_t size, size_t start_pos) -> bool;
#endif

#if HAVE_AVX512F && HAVE_AVX512BW && HAVE_AVX512VBMI
__attribute__((target("avx512f,avx512bw,avx512vbmi"))) 
auto validate_utf8_8x_avx512(const char* data, size_t size, size_t start_pos) -> bool;
#endif

// ============================================================================
// Auto-Dispatch Functions
// ============================================================================

// Automatically choose the best implementation based on CPU features
auto skip_whitespace_multi(const char* data, size_t size, size_t start_pos) -> size_t;
auto find_string_end_multi(const char* data, size_t size, size_t start_pos) -> size_t;
auto validate_number_chars_multi(const char* data, size_t start_pos, size_t end_pos) -> bool;

} // namespace fastjson::simd::multi

// ============================================================================
// Multi-Register Structural Character Detection
// ============================================================================

// Find multiple JSON structural characters ({}[],:) using parallel SIMD
struct StructuralChars {
    uint64_t positions[64];  // Up to 64 structural char positions per chunk
    uint8_t types[64];       // Character types (1='{', 2='}', 3='[', etc.)
    size_t count;
};

__attribute__((target("avx2"))) 
auto find_structural_chars_4x_avx2(const char* data, size_t size, size_t start_pos, 
                                   StructuralChars& result) -> size_t;

__attribute__((target("avx512f,avx512bw"))) 
auto find_structural_chars_8x_avx512(const char* data, size_t size, size_t start_pos,
                                     StructuralChars& result) -> size_t;

// ============================================================================
// Multi-Register Unicode Validation
// ============================================================================

// Validate UTF-8 sequences using multiple registers for parallel processing
__attribute__((target("avx2"))) 
auto validate_utf8_4x_avx2(const char* data, size_t size, size_t start_pos) -> bool;

__attribute__((target("avx512f,avx512bw,avx512vbmi"))) 
auto validate_utf8_8x_avx512(const char* data, size_t size, size_t start_pos) -> bool;

// ============================================================================
// Multi-Register Digit Parsing
// ============================================================================

// Parse multiple 8-digit integer chunks in parallel using FMA
struct MultiDigitResult {
    uint64_t values[8];  // Up to 8 parsed integers per call
    size_t count;
    size_t bytes_consumed;
};

__attribute__((target("fma,avx2"))) 
auto parse_digits_4x_fma(const char* data, size_t size, size_t start_pos,
                         MultiDigitResult& result) -> bool;

// ============================================================================
// Dispatch Functions
// ============================================================================

// Automatically choose the best implementation based on CPU features
auto skip_whitespace_multi(const char* data, size_t size, size_t start_pos) -> size_t;
auto find_string_end_multi(const char* data, size_t size, size_t start_pos) -> size_t;
auto validate_number_chars_multi(const char* data, size_t start_pos, size_t end_pos) -> bool;
auto find_structural_chars_multi(const char* data, size_t size, size_t start_pos, 
                                 StructuralChars& result) -> size_t;

// Always available fallback implementations  
auto skip_whitespace_4x_avx2(const char* data, size_t size, size_t start_pos) -> size_t;
auto find_string_end_4x_avx2(const char* data, size_t size, size_t start_pos) -> size_t;
auto validate_number_chars_4x_avx2(const char* data, size_t start_pos, size_t end_pos) -> bool;
auto find_structural_chars_4x_avx2(const char* data, size_t size, size_t start_pos, 
                                   StructuralChars& result) -> size_t;

} // namespace fastjson::simd::multi