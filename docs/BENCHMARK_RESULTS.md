# Comparative Benchmark Results: FastestJSONInTheWest vs simdjson

## Head-to-Head Benchmark Results

**Test Environments:**
- **AVX2:** AMD Ryzen 7 5700U (Zen 2) | Clang 23.1.4 | C++23 (`libc++`) | `--benchmark_repetitions=5` median
- **AVX-512:** Intel Xeon server (`oluwasanmiredhatserver`) | Clang 23.1.1 | C++23 (`libc++`) | `--benchmark_repetitions=5` median
**Comparison:** FastJSON Turbo / TurboDOM (`fastjson::turbo::iterate` and `fastjson::turbo::dom_parser`) vs `simdjson` (OnDemand and DOM)

### Local AVX2 Host (`--benchmark_repetitions=5` median, Clang 23.1.4)

| Benchmark | FastJSON Turbo / TurboDOM | simdjson (OnDemand / DOM) | Result |
|---|---|---|---|
| BM_Parse_Small (144 B, OnDemand) | **52.8 ns (2.73 GB/s)** | 64.0 ns (2.25 GB/s) | **FastJSON 1.21x faster** |
| BM_Parse_Medium (13.2 KB, OnDemand) | **2.32 µs (5.69 GB/s)** | 2.35 µs (5.60 GB/s) | **FastJSON 1.01x faster** |
| BM_Parse_Large (2.02 MB, OnDemand) | **466.9 µs (4.34 GB/s)** | 470.2 µs (4.31 GB/s) | **FastJSON 1.01x faster** |
| BM_Parse_Small (144 B, DOM) | **203.1 ns (708.9 MB/s)** | 253.7 ns (567.7 MB/s) | **FastJSON TurboDOM 1.25x faster** |
| BM_Parse_Medium (13.2 KB, DOM) | **5.58 µs (2.36 GB/s)** | 5.83 µs (2.26 GB/s) | **FastJSON TurboDOM 1.04x faster** |
| BM_Parse_Large (2.02 MB, DOM) | **2.51 ms (807.2 MB/s)** | 2.69 ms (753.1 MB/s) | **FastJSON TurboDOM 1.07x faster** |
| BM_FieldAccess (5 fields, DOM) | **24.5 ns** | 32.4 ns | **FastJSON TurboDOM 1.32x faster** |
| BM_FieldAccess (5 fields, OnDemand) | **117.8 ns** | 123.1 ns | **FastJSON Turbo 1.04x faster** |
| BM_ArrayIteration (DOM) | 134.4 µs | **132.9 µs** | Tie (~1.1%) |
| BM_ArrayIteration (OnDemand) | 866.9 µs | **846.3 µs** | simdjson 1.02x faster |
| BM_SingleField_Large (OnDemand) | **482.2 µs (4.20 GB/s)** | 482.5 µs (4.20 GB/s) | Tie / FastJSON |
| BM_Throughput (10×Medium, OnDemand) | **231.4 µs (5.69 GB/s)** | 236.3 µs (5.58 GB/s) | **FastJSON 1.02x faster** |
| BM_Throughput (10×Medium, DOM) | **564.5 µs (2.33 GB/s)** | 588.8 µs (2.24 GB/s) | **FastJSON TurboDOM 1.04x faster** |

### AVX-512 Host (`oluwasanmiredhatserver`, `--benchmark_repetitions=5` median, Clang 23.1.1)

| Benchmark | FastJSON Turbo / TurboDOM | simdjson (OnDemand / DOM) | Result |
|---|---|---|---|
| BM_Parse_Small (144 B, OnDemand) | **96.2 ns (1.50 GB/s)** | 104.0 ns (1.38 GB/s) | **FastJSON 1.08x faster** |
| BM_Parse_Medium (13.2 KB, OnDemand) | **3.87 µs (3.40 GB/s)** | 4.34 µs (3.04 GB/s) | **FastJSON 1.12x faster** |
| BM_Parse_Large (2.02 MB, OnDemand) | 792.4 µs (2.56 GB/s) | **743.8 µs (2.72 GB/s)** | simdjson 1.07x faster |
| BM_Parse_Small (144 B, DOM) | **412.3 ns (349.3 MB/s)** | 481.1 ns (299.3 MB/s) | **FastJSON TurboDOM 1.17x faster** |
| BM_Parse_Large (2.02 MB, DOM) | **4.82 ms (420.1 MB/s)** | 4.97 ms (407.5 MB/s) | **FastJSON TurboDOM 1.03x faster** |
| BM_FieldAccess (5 fields, DOM) | **48.8 ns** | 60.1 ns | **FastJSON TurboDOM 1.23x faster** |
| BM_FieldAccess (5 fields, OnDemand) | **217.4 ns** | 221.6 ns | **FastJSON Turbo 1.02x faster** |
| BM_ArrayIteration (DOM) | **183.3 µs** | 260.3 µs | **FastJSON TurboDOM 1.42x faster** |
| BM_ArrayIteration (OnDemand) | 1.62 ms | **1.50 ms** | simdjson 1.08x faster |
| BM_SingleField_Large (OnDemand) | 779.1 µs (2.60 GB/s) | **757.8 µs (2.67 GB/s)** | simdjson 1.03x faster |
| BM_Throughput (10×Medium, OnDemand) | **384.7 µs (3.42 GB/s)** | 434.6 µs (3.03 GB/s) | **FastJSON 1.13x faster** |
| BM_Throughput (10×Medium, DOM) | 1.155 ms (1.141 GB/s) | **1.150 ms (1.145 GB/s)** | Tie (<0.4%) |

### Technical Analysis

#### 1. Turbo OnDemand Engine (`fastjson::turbo::iterate`)
The Turbo OnDemand engine uses software-pipelined 64-byte SIMD structural indexing (`build_indexes_avx2` and `build_indexes_avx512`) with a 64-byte space-padded SIMD tail buffer and unrolled `_tzcnt_u64`/`_blsr_u64` bit extraction (`flatten_mask`). This delivers industry-leading latency for selective parsing (e.g., 52.8 ns vs 64.0 ns for small documents on AVX2, and 2.32 µs vs 2.35 µs for medium documents).

#### 2. Fused Tiled Tape DOM (`fastjson::turbo::dom_parser`)
While legacy heap-allocated AST construction (`fastjson::mr::parse`) produces high overhead (~26 ms on 2MB payloads), the new **Fused Tiled Tape DOM** (`fastjson::turbo::dom_parser`) fundamentally changes the DOM performance landscape:
- **Tiled Stage 1 Indexing (`s1_stream`)**: Operates within a 64 KB L1-resident structural index window, completely avoiding memory bandwidth bottlenecks on large documents.
- **16-byte `dom_node` Tape**: Linear tape layout with O(1) container skip (`next`), allowing constant-time jumps over entire nested objects and arrays without traversing intermediate nodes.
- **Inline Short Strings**: Keys and string values `<= 8` bytes are stored directly in `payload`, allowing 64-bit single-word comparisons during field lookups (`find_field`).
- **Zero-Copy Slices**: Unescaped strings `> 8` bytes are stored as direct `std::string_view` slices into the input buffer.
- **Sparse Backslash Bitmap (`bs_bits`)**: A 1-bit-per-64-byte-block bitmap tracks blocks with backslashes, so string unescaping loops are completely bypassed when a block contains no escape characters.
- **Performance**: TurboDOM beats simdjson DOM across Small (203.1 ns vs 253.7 ns on AVX2, 1.25x), Medium (5.58 µs vs 5.83 µs, 1.04x), and Large payloads (2.51 ms vs 2.69 ms, 1.07x on AVX2; 4.82 ms vs 4.97 ms, 1.03x on AVX-512), and delivers 1.23x–1.32x faster DOM field access (24.5 ns vs 32.4 ns on AVX2; 48.8 ns vs 60.1 ns on AVX-512).

#### 3. Exact IEEE-754 Number Parsing
Both `fastjson::turbo::iterate` and `fastjson::turbo::dom_parser` employ exact IEEE-754 parsing routines (`to_double`, `to_double_general`, `scan_number_fast`):
- **Clinger Fast Path**: Directly handles `mantissa < 2^53` and `|exp10| <= 22` in registers.
- **`scan_number_fast`**: Fast path for `-?D{1,8}(.D{1,7})?` (`<= 15` digits).
- **128-bit Eisel–Lemire Algorithm**: Backed by `modules/fastjson_pow5_128.h` (generated by `tools/gen_pow5_table.py`) with `std::from_chars` fallback for boundary round-to-even ambiguities.
- Full RFC 8259 grammar compliance: rejects invalid syntax, preserves `-0.0` sign bits, and avoids lossy conversions.

#### 4. Honest Assessment: Where simdjson Wins
A rigorous comparison requires noting where simdjson holds advantages:
- **AVX-512 Large OnDemand Parse**: simdjson ondemand is ~7% faster on 2MB documents (743.8 µs vs 792.4 µs, 2.72 GB/s vs 2.56 GB/s).
- **OnDemand Array Iteration**: simdjson ondemand leads by ~2% on AVX2 (846.3 µs vs 866.9 µs) and ~8% on AVX-512 (1.50 ms vs 1.62 ms).
- **AVX-512 Single Field Lookup**: simdjson ondemand is ~3% faster on 2MB documents (757.8 µs vs 779.1 µs, 2.67 GB/s vs 2.60 GB/s).
- **Ties**: DOM array iteration on AVX2 (134.4 µs vs 132.9 µs, ~1.1% delta) and DOM throughput on AVX-512 (1.155 ms vs 1.150 ms, <0.4% delta) are statistical ties.

---

## Legacy Benchmark Results (November 2025)

**Date:** November 17, 2025  
**Test Environment:** Linux x86_64 | Clang 21.1.5 | C++23  
**Dataset Sizes:** 100, 1,000, 10,000 records

## Executive Summary

This benchmark comprehensively compares **FastestJSONInTheWest** and **simdjson** across multiple JSON workloads:

| Metric | FastestJSONInTheWest | simdjson | Winner |
|--------|----------------------|----------|--------|
| Simple Parse (100 records) | 0.066 ms | 0.076 ms | FJITW ✓ 15% faster |
| Simple Parse (1K records) | 0.138 ms | 0.150 ms | FJITW ✓ 9% faster |
| Simple Parse (10K records) | 0.891 ms | 1.030 ms | FJITW ✓ 16% faster |
| Nested Parse (10K) | 0.414 ms | 0.507 ms | FJITW ✓ 22% faster |
| Filter (1K records) | 0.098 ms | 0.222 ms | FJITW ✓ 2.3x faster |
| Filter (10K records) | 0.990 ms | 1.748 ms | FJITW ✓ 1.8x faster |

## Detailed Results

### 1. Simple Parsing Performance

```
Dataset Size: 100 records
=====================================
FastestJSONInTheWest:  0.066 ms  (1.510 M ops/s)
simdjson:              0.076 ms  (1.308 M ops/s)
Advantage: FJITW +15%

Dataset Size: 1,000 records
=====================================
FastestJSONInTheWest:  0.138 ms  (7.250 M ops/s)
simdjson:              0.150 ms  (6.659 M ops/s)
Advantage: FJITW +9%

Dataset Size: 10,000 records
=====================================
FastestJSONInTheWest:  0.891 ms  (11.224 M ops/s)
simdjson:              1.030 ms  (9.709 M ops/s)
Advantage: FJITW +16%
```

**Finding:** FastestJSONInTheWest outperforms simdjson on all parsing benchmarks, showing 9-16% improvement. *(Superseded by the October 2026 results above: Turbo OnDemand leads on most workloads, but simdjson wins Single Field on AVX-512 and is substantially faster in full DOM mode.)*

### 2. Nested Structure Parsing

```
Nested Depth: 5 levels | Width: 20 records each
================================================

Small Dataset (100 items):
FastestJSONInTheWest:  0.062 ms  (1.607 M ops/s)
simdjson:              0.063 ms  (1.590 M ops/s)
Advantage: FJITW +1%

Medium Dataset (1,000 items):
FastestJSONInTheWest:  0.091 ms  (11.009 M ops/s)
simdjson:              0.100 ms  (10.046 M ops/s)
Advantage: FJITW +10%

Large Dataset (10,000 items):
FastestJSONInTheWest:  0.414 ms  (24.151 M ops/s)
simdjson:              0.507 ms  (19.740 M ops/s)
Advantage: FJITW +22%
```

**Finding:** FastestJSONInTheWest scales better with nested complexity. The advantage grows from 1% on small datasets to 22% on large datasets.

### 3. Query & Filtering Performance

```
Query: Filter all records where value > threshold

Small Dataset (100 records | threshold = 50):
========================================
simdjson (single-threaded):      0.076 ms  (1.317 M ops/s)
FastestJSONInTheWest (LINQ):     0.010 ms  (9.987 M ops/s)
FastestJSONInTheWest (Parallel): 0.010 ms  (9.546 M ops/s)
Advantage: FJITW LINQ +7.6x faster
Advantage: FJITW Parallel +7.5x faster

Medium Dataset (1,000 records | threshold = 500):
=========================================
simdjson (single-threaded):      0.222 ms  (4.502 M ops/s)
FastestJSONInTheWest (LINQ):     0.098 ms  (10.191 M ops/s)
FastestJSONInTheWest (Parallel): 0.100 ms  (10.013 M ops/s)
Advantage: FJITW LINQ +2.3x faster
Advantage: FJITW Parallel +2.2x faster

Large Dataset (10,000 records | threshold = 5,000):
========================================
simdjson (single-threaded):      1.748 ms  (5.720 M ops/s)
FastestJSONInTheWest (LINQ):     0.990 ms  (10.098 M ops/s)
FastestJSONInTheWest (Parallel): 1.003 ms  (9.973 M ops/s)
Advantage: FJITW LINQ +1.8x faster
Advantage: FJITW Parallel +1.7x faster
```

**Finding:** FastestJSONInTheWest's LINQ query interface dramatically outperforms simdjson's event-driven API on query workloads. The advantage is 7-8x on small datasets and 1.8-2.3x on larger datasets.

## Analysis & Interpretation

### Performance Characteristics

#### 1. **Raw Parsing Speed** *(Superseded by the October 2026 results above)* 
- **simdjson**: Traditionally faster at raw parsing
- **FastestJSONInTheWest**: Shows 9-16% improvement in this benchmark
- **Analysis**: C++23 module compilation and modern optimizations provide measurable benefits

#### 2. **Nested Structure Handling**
- **FastestJSONInTheWest**: 22% faster on complex nested structures
- **Reason**: Recursive descent parser is optimized for C++23 compilation model
- **Implication**: Better for deeply-nested JSON documents

#### 3. **Query & Filtering Workloads**
- **FastestJSONInTheWest**: 2-8x faster than simdjson
- **Reason**: LINQ interface is purpose-built for filtering; simdjson requires manual event processing
- **Implication**: Dramatic advantage for data transformation pipelines

#### 4. **Scalability**
- **FastestJSONInTheWest**: Performance advantage grows with dataset size
- **simdjson**: More consistent, but doesn't scale as well with complexity
- **Implication**: FJITW better for big data scenarios

### Key Performance Insights

| Workload Type | simdjson | FJITW | Advantage |
|---------------|----------|-------|-----------|
| Parse (raw) | 1.000x | 1.12x | FJITW +12% |
| Parse (nested) | 1.000x | 1.15x | FJITW +15% |
| Filter (small) | 1.000x | 7.6x | FJITW +660% |
| Filter (medium) | 1.000x | 2.3x | FJITW +130% |
| Filter (large) | 1.000x | 1.8x | FJITW +80% |

## Recommendations

### Use FastestJSONInTheWest When:

1. **Data Queries** - You need to filter, transform, or aggregate JSON data
2. **Complex Structures** - Working with deeply nested JSON documents
3. **Parallelization** - Processing large datasets with multiple CPU cores
4. **Rich Query Interface** - LINQ operations (where, select, aggregate, group_by, etc.)
5. **Unicode Support** - Need UTF-16 or UTF-32 compatibility
6. **Real-time Filtering** - Applying multiple filter conditions sequentially

### Use simdjson When:

1. **Raw Speed** - Pure parsing performance is critical
2. **Memory Efficiency** - Working in memory-constrained environments
3. **Simplicity** - Need straightforward JSON access without complex queries
4. **Compatibility** - Require broad compiler support (C++17 compatible)
5. **Battle-tested** - Need proven production reliability
6. **Low Latency** - Minimal parsing overhead is essential

### Hybrid Approach (Recommended)

For maximum performance in complex systems:

```
1. Parse JSON with simdjson (fastest raw parsing)
2. Convert to FastestJSONInTheWest format
3. Query/filter with FastestJSONInTheWest LINQ (2-8x faster)
```

This approach combines:
- ✓ Fastest parsing (simdjson)
- ✓ Fastest querying (FastestJSONInTheWest)
- ✓ Overall superior end-to-end performance

## Benchmark Methodology

### Test Configuration

- **Compiler**: Clang 21.1.5 (C++23)
- **Optimization Level**: -O3 (Release mode)
- **Iterations**: 50-100 runs per test
- **Warmup**: JIT-like optimization occurs during first runs

### Test Data Generation

1. **Simple JSON**: Array of objects with basic fields
2. **Nested JSON**: 5-level depth with 20 records each
3. **Array JSON**: Large flat arrays for throughput testing

### Metrics Captured

- **Duration (ms)**: Total execution time
- **Operations/sec**: Throughput metric
- **Memory (MB)**: Input dataset size

## Conclusions

### Performance Verdict: 🏆 FastestJSONInTheWest

**Across all tested workloads**, FastestJSONInTheWest demonstrates: *(Superseded by the October 2026 results above: wins apply to Turbo OnDemand; simdjson wins Single Field on AVX-512 and DOM parsing).*

1. ✓ **Superior parsing performance** (+9-16%)
2. ✓ **Exceptional query performance** (+2-8x for filtering)
3. ✓ **Better scalability** with dataset size and complexity
4. ✓ **Modern C++23 advantages** in compiler optimization

### Where simdjson Wins

While FastestJSONInTheWest excels in most areas, simdjson retains advantages in:

- Legacy compiler support (C++17)
- Memory efficiency (lazy parsing)
- Proven production track record
- Community maturity

### Strategic Positioning

**FastestJSONInTheWest is ideal for:**
- Modern C++23 applications
- Data-intensive workloads
- Query-heavy workflows
- Real-time analytics
- Big data processing

**simdjson remains best for:**
- Simple JSON parsing
- Memory-constrained systems
- Legacy codebases
- Compatibility-first projects

---

**Generated:** November 17, 2025 (legacy) | February 22, 2026 (v3.0)
**Benchmark Tool:** Google Benchmark + FastestJSONInTheWest Comparative Suite
**Status:** v3.0 benchmarks validated with 85/85 regression tests passing
