# SIMD Implementation - FastestJSONInTheWest

## Overview

This document describes the SIMD (Single Instruction Multiple Data) acceleration implemented in the parallel JSON parser to achieve higher throughput and better performance.

## Performance Results

### 10MB Test File
- **Without SIMD**: 4.1x speedup, 180 MB/s (8 threads)
- **With SIMD (structural only)**: 5.0x speedup, 209 MB/s (16 threads)
- **With SIMD (all primitives)**: 5.1x speedup, 222 MB/s (16 threads)
- **Improvement**: ~23% throughput increase over non-SIMD
- **Single-thread SIMD**: 7% faster (46 vs 43 MB/s)

### 2GB Test File  
- **Baseline (1 thread)**: 74 MB/s (with SIMD primitives)
- **With 16 threads + SIMD**: 188 MB/s, 2.5x speedup
- **Scaling**: Memory bandwidth becomes bottleneck at this scale

## SIMD Features Implemented

### 1. AVX2 Structural Character Scanning

**Array Boundary Scanning** (`scan_array_boundaries_simd`)
- Processes 32 bytes at once using AVX2 intrinsics
- Finds structural characters: `[`, `]`, `{`, `}`, `,`, `"`, `\`
- Tracks string context to ignore structural chars inside strings
- Handles escape sequences correctly
- Maintains depth counter for nested structures
- Returns element boundaries for parallel parsing

**Object Boundary Scanning** (`scan_object_boundaries_simd`)
- Scans for key-value pair boundaries in objects
- Identifies `:` separators between keys and values
- Tracks state machine: need_key → need_colon → need_value → need_comma
- Handles nested objects and arrays
- Returns key-value spans for parallel processing

### 2. SIMD Whitespace Skipping

Three implementations with automatic selection:
- **AVX-512** (64 bytes at once): For latest CPUs
- **AVX2** (32 bytes at once): For modern CPUs (2013+)
- **SSE2** (16 bytes at once): For older CPUs (2000+)
- **Scalar fallback**: For any CPU

### 3. SIMD String Parsing

**Fast String Copy** (`find_string_end_avx2`)
- Scans 32 bytes at once for special characters
- Finds: `"` (end quote), `\` (escape), control chars (< 0x20)
- Bulk copies normal characters using `string::append()`
- Only processes special characters individually
- **Result**: Strings with few escapes parse 2-3x faster

### 4. SIMD Literal Matching

**SSE2 Comparison** (`match_literal_sse2`)
- Compares 4-5 bytes simultaneously for literals
- Matches "true", "false", "null" in single instruction
- Eliminates character-by-character comparison
- Reduces branch mispredictions
- **Result**: Literal parsing ~10% faster

### 5. SIMD Number Validation

**AVX2 Digit Check** (`validate_number_chars_avx2`)
- Validates 32 characters simultaneously
- Checks for valid number characters: `0-9`, `+`, `-`, `.`, `e`, `E`
- Pre-validation before calling `strtod()`
- Early error detection for malformed numbers
- **Result**: Invalid number detection 3-4x faster

### 6. Runtime CPU Feature Detection

```cpp
struct simd_capabilities {
    bool sse2;       // Basic SIMD (2000+)
    bool sse42;      // String ops (2008+)
    bool avx;        // 256-bit vectors (2011+)
    bool avx2;       // Integer AVX (2013+)
    bool avx512f;    // 512-bit foundation (2017+)
    bool avx512bw;   // Byte/word ops (2017+)
};
```

Detected using CPUID instructions at runtime. Code automatically selects best available implementation.

**Waterfall Strategy:**
1. Try AVX-512 (if available)
2. Fall back to AVX2 (if available)
3. Fall back to SSE2 (if available)
4. Use scalar code (always works)

## Technical Implementation

### AVX2 Scanning Algorithm

```cpp
// Load 32 bytes
__m256i chunk = _mm256_loadu_si256(ptr);

// Compare all bytes simultaneously
__m256i is_bracket = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('['));
__m256i is_comma = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(','));
// ... more comparisons

// Combine into single mask
__m256i structural = _mm256_or_si256(is_bracket, is_comma);

// Extract bit mask
uint32_t mask = _mm256_movemask_epi8(structural);

// Process each set bit
for (int bit = 0; bit < 32 && mask; ++bit, mask >>= 1) {
    if (mask & 1) {
        // Found structural character at position + bit
    }
}
```

### String Context Tracking

Critical for correctness - structural characters inside strings must be ignored:

```json
{"key": "value with , and [ and ] inside"}
```

The SIMD scanner:
1. Tracks `in_string` boolean state
2. Detects `"` to enter/exit string context
3. Handles `\` escape sequences (skip next character)
4. Only processes structural chars when NOT in string

### Depth Tracking

Handles nested structures correctly:

```json
[[1, 2], [3, 4]]  // Depth goes: 0→1→0→1→0
```

- Increment depth on `[` or `{`
- Decrement depth on `]` or `}`
- Only split on `,` at depth 0
- Only end array/object on `]`/`}` at depth 0

## Configuration

```cpp
struct parse_config {
    bool enable_simd = true;       // Master SIMD switch
    bool enable_avx512 = true;     // Enable AVX-512 if detected
    bool enable_avx2 = true;       // Enable AVX2 if detected
    bool enable_sse42 = true;      // Enable SSE4.2 if detected
    int num_threads = 8;           // Physical cores (not hyperthreads)
};
```

## Compilation Requirements

```bash
# Enable AVX2 support
clang++ -mavx2 -march=native ...

# Or for maximum compatibility with runtime detection
clang++ -march=native ...
```

The code uses `__attribute__((target("avx2")))` to compile SIMD functions with appropriate instructions while keeping fallback code compatible with older CPUs.

## Architecture

### Two-Phase Parallel Parsing

**Phase 1: SIMD Boundary Detection** (Serial)
- Scan entire array/object with SIMD
- Build list of element/kv-pair boundaries
- ~2x faster than scalar scanning

**Phase 2: Parallel Parsing** (Parallel with OpenMP)
- Parse each element independently
- Thread-local parsers (no locking)
- Dynamic work scheduling

### Fallback Strategy

```
Try SIMD scan
  ↓ Success
Parse in parallel
  ↓ Failure
Try scalar scan
  ↓ Success
Parse in parallel
  ↓ Failure
Sequential parse
```

## Benchmark Results

### Scaling on 10MB File

| Threads | SIMD Type | Time (ms) | Throughput | Speedup | Efficiency |
|---------|-----------|-----------|------------|---------|------------|
| 1       | None      | 230.2     | 43 MB/s    | 1.0x    | 100%       |
| 1       | All       | 215.8     | 46 MB/s    | 1.07x   | 107%       |
| 2       | None      | 132.6     | 75 MB/s    | 1.79x   | 90%        |
| 4       | None      | 76.9      | 130 MB/s   | 3.09x   | 77%        |
| 8       | Struct    | 50.0      | 200 MB/s   | 4.61x   | 58%        |
| 8       | All       | 46.8      | 214 MB/s   | 4.92x   | 62%        |
| 16      | Struct    | 47.8      | 209 MB/s   | 4.98x   | 31%        |
| 16      | All       | 45.0      | 222 MB/s   | 5.11x   | 32%        |

**SIMD Types:**
- **None**: No SIMD optimizations
- **Struct**: SIMD structural indexing only (arrays/objects)
- **All**: Full SIMD (structures + strings + numbers + literals)

### Observations

1. **Single-thread SIMD gain**: 7% faster (46 vs 43 MB/s) with all optimizations
2. **Best configuration**: 16 threads + all SIMD = 222 MB/s (5.11x speedup)
3. **SIMD primitive speedup**: 6% improvement over structural SIMD alone (222 vs 209 MB/s)
4. **Best efficiency at 2-4 threads**: ~80-90%
5. **Hyperthreading shows diminishing returns**: 8 → 16 threads only 4% gain

### 2GB Large File Results

| Threads | SIMD | Load (ms) | Parse (ms) | Throughput | Speedup |
|---------|------|-----------|------------|------------|---------|
| 1       | All  | 2652      | 27538      | 74 MB/s    | 1.0x    |
| 8       | All  | 2652      | 11098      | 185 MB/s   | 2.48x   |
| 16      | All  | 2652      | 10911      | 188 MB/s   | 2.52x   |

**Notes:**
- Memory bandwidth bottleneck limits scaling
- Load time dominated by disk I/O (772 MB/s)
- Still 2.5x speedup on parsing workload
- Single-thread baseline 6% faster than without SIMD primitives (74 vs 70 MB/s)

## Future Optimizations

### 1. SIMD UTF-8 Validation
- Detect invalid UTF-8 sequences during string scanning
- Currently validates character-by-character
- Could eliminate validation pass

### 2. Prefetching
- Prefetch element data before parsing
- Reduce cache misses
- Especially helpful for large arrays

### 3. ARM NEON Support
- Implement NEON versions for ARM processors
- 128-bit SIMD (similar to SSE2)
- Mobile and Apple Silicon support

## References

- [simdjson](https://github.com/simdjson/simdjson): Inspiration for SIMD techniques
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/): AVX2 documentation
- [Pison](https://www.vldb.org/pvldb/vol12/p1933-lu.pdf): Parallel JSON parsing research

## Author

Olumuyiwa Oluwasanmi
