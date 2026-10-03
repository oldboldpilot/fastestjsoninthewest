# SIMD Implementation - FastestJSONInTheWest

## v3.0 SIMD Architecture (February 2026)

### Global Module Fragment (GMF)

All SIMD implementations (`__m256i`, `__m512i`, `__m128i` intrinsics) now reside in `namespace fastjson::detail` inside the **Global Module Fragment** (before `export module fastjson;`). This is required because Clang 21's `TemplateInstantiator` segfaults when serializing SIMD intrinsic types into the Binary Module Interface (BMI).

```
module;  // Global Module Fragment starts here

// All SIMD code goes here in namespace fastjson::detail
namespace fastjson::detail { ... }

export module fastjson;  // Module purview starts here

// Thin wrappers delegate to detail:: functions
inline auto skip_whitespace_simd(...) { return detail::skip_whitespace_simd_impl(...); }
```

### Multi-Register SIMD (v3.0)

| Function | Registers | Bytes/Iteration |
|----------|-----------|-----------------|
| `skip_whitespace_avx512()` | 4x ZMM (512-bit) | 256 bytes |
| `skip_whitespace_avx2()` | 8x YMM (256-bit) | 256 bytes |
| `skip_whitespace_sse42()` | 4x XMM (128-bit) | 64 bytes |
| `skip_whitespace_sse2()` | 4x XMM (128-bit) | 64 bytes |
| `find_string_end_simd` (AVX2) | 8x YMM | 256 bytes |

### Structural SIMD Tape (`fastjson_simd_index.h`)

The ondemand parser uses a two-stage approach:
1. **Stage 1**: SIMD structural indexing builds a tape of structural characters
2. **Stage 2**: Lazy navigation through the tape without parsing values

The tape indexes: `{ } [ ] , : "` plus primitive value starts (`t`, `f`, `n`, `0-9`, `-`).

**AVX2 scanner fixes (v3.0):**
- Fixed mid-chunk string-state tracking: `in_string` now toggles on each `"` within the SIMD mask loop
- Fixed post-closing-quote skip: scalar fallback continues processing structural chars after closing quote

### Runtime CPUID Dispatch

```cpp
auto detect_simd_capabilities() noexcept -> uint32_t;

// Constants: SIMD_SSE2, SIMD_SSE42, SIMD_AVX2, SIMD_AVX512F, SIMD_AVX512BW, etc.
// Waterfall: AVX-512 → AVX2 → SSE4.2 → SSE2 → scalar
```

---

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

## 2026-10-03 — AVX-512 string scan, runtime-dispatched AVX-512, and four scan/lifetime defects

**AVX-512 is now compiled in wherever the compiler can target it and chosen at run time.** The portable
x86-64-v3 baseline (no `-mavx512f`) left `HAVE_AVX512F` undefined, so every AVX-512 kernel was compiled OUT
and an AVX-512 host ran the AVX2 path. `FASTJSON_TARGET_AVX512` (clang/gcc on x86-64) compiles them under a
per-function `target("avx512f,avx512bw")`; `detect_simd_capabilities()` decides at run time. cl.exe has no
target attribute and keeps the scalar path. The kernels use no lambdas: a lambda does not inherit its
enclosing function's target attribute, so an AVX-512 intrinsic inside one fails to compile at x86-64-v3.

- **New: `find_string_end_avx512`** — 8× zmm (512 B/iteration), 1× zmm loop, masked final load (never reads
  past the end). Dispatch: AVX-512F+BW → AVX2 8× ymm → scalar. Exported as `fastjson::find_string_end_simd`.
- **Fixed: the AVX2 string scan stopped at the first byte of every string.** The unsigned `< 0x20` test
  flipped each byte's sign bit but not the limit's; with the limit `0x20` instead of `0x20 ^ 0x80`, every
  printable byte read as a control character, so `parse_string_simd`'s fast path never ran.
- **Fixed: AVX-512 control tests compared SIGNED** (`_mm512_cmplt_epi8_mask`), flagging every UTF-8 byte;
  now `_mm512_cmplt_epu8_mask` (also in `fastjson_simd_multiregister_complex.cpp`).
- **Fixed: parsed strings dangled.** The fast path stored a `string_view` into the caller's input, so
  `auto v = parse(readFile(p));` left every unescaped string pointing at freed memory. Live on every scalar
  build (cl.exe, ARM) all along; on x86 only hidden by the AVX2 defect above. The fast path now copies once
  (still a bulk copy, not the per-character loop); zero-copy views stay with the explicit ondemand API.
- **Fixed: CPU detection ignored the OS.** CPUID says what the CPU has, not what the OS saves; a VM or kernel
  that masks AVX/AVX-512 state would SIGILL. Now OSXSAVE + XGETBV gate AVX (XCR0 & 0x6), AVX-512
  (XCR0 & 0xE6) and AMX (bits 17–18). The cache's plain `static bool initialized` (a data race) is a ready bit
  in the one atomic.
- **New knob: `FASTJSON_SIMD=scalar|avx2`** lowers the tier so one host exercises every path, and
  **`fastjson::string_scan_tier()`** reports the kernel that actually runs (it mirrors the dispatcher, including
  what was compiled in, so a compiled-out tier shows even when CPUID reports the feature).
- **Fixed: the serializer's escape scan** (`find_escape_position_simd_impl`, used by `stringify`) compared
  signed and its AVX-512 branch was dead; it now delegates to the string-end dispatcher (same predicate).
- **Fixed: loop bounds formed pointers past one-past-the-end** (`ptr + 512 <= end` on a short input is undefined
  behaviour even unread); every SIMD loop in `fastjson.cppm` now compares remaining bytes (`end - ptr >= N`).
- Reviewed adversarially (agy Gemini 3.8 Flash High, cursor GPT-5.6 Sol High); false positives dropped after
  checking (e.g. `read_xcr0` is inside the x86-only block).

Gate: sensen's `test_fastjson_simd` (every tier; mutation-checked against each defect). Measured on a 5 MB
tool-result-shaped document (Xeon, AVX-512): scalar 134, AVX2 146, AVX-512 150 MB/s. Parse time here is
dominated by building the DOM (allocation, copies, map inserts), not by the scan; larger wins need the tape /
structural-index design (`fastjson_turbo`).
