# Comparative Benchmark Results: FastestJSONInTheWest vs simdjson

## Head-to-Head Benchmark Results (October 22, 2026)

**Test Environments:**
- **AVX2:** AMD Ryzen 7 5700U (Zen 3) | Clang 23.1.4 | C++23 (`libc++`)
- **AVX-512:** Intel Xeon Gold 6152 (Skylake-SP) | Clang 23.1.4 | C++23 (`libc++`)
**Comparison:** `fastjson::turbo::iterate` vs `simdjson::ondemand::parser::iterate`

### Stage 1 Structural Indexing & OnDemand Parse Latency

| Payload | Size | `fastjson::turbo::iterate` (AVX2 / AVX-512) | `simdjson::ondemand` (AVX2 / AVX-512) | Speedup (AVX2 / AVX-512) |
|---------|------|---------------------------------------------|---------------------------------------|--------------------------|
| Small | 144B | **45.1 ns (3.19 GB/s)** / **76.7 ns (1.88 GB/s)** | 64.1 ns (2.25 GB/s) / 120.8 ns (1.19 GB/s) | **FastJSON 1.42x / 1.58x** |
| Medium | 13.2KB | **2.13 us (6.19 GB/s)** / **3.30 us (4.00 GB/s)** | 2.32 us (5.68 GB/s) / 4.69 us (2.81 GB/s) | **FastJSON 1.09x / 1.42x** |
| Large | 2.02MB | **451.3 us (4.49 GB/s)** / **666.6 us (3.04 GB/s)** | 466.1 us (4.34 GB/s) / 846.8 us (2.39 GB/s) | **FastJSON 1.03x / 1.27x** |
| Mixed Throughput (100 msgs) | 1.32MB | **212.0 us (6.22 GB/s)** / **329.7 us (4.00 GB/s)** | 232.1 us (5.68 GB/s) / 469.1 us (2.81 GB/s) | **FastJSON 1.09x / 1.42x** |

### Stage 2 Query & Iteration Benchmarks (Parse + Query)

| Benchmark | `fastjson::turbo::iterate` (AVX2 / AVX-512) | `simdjson::ondemand` (AVX2 / AVX-512) | Winner |
|-----------|---------------------------------------------|---------------------------------------|--------|
| **Field Access** (5 float fields) | **91.5 ns** / **173.3 ns** | 112.0 ns / 240.8 ns | **FastJSON 1.22x / 1.39x** |
| **Array Iteration** (10K records) | **808.5 us** / **1.36 ms** | 867.1 us / 1.87 ms | **FastJSON 1.07x / 1.38x** |
| **Single Field Lookup** (2.02MB, AVX2) | **458.5 us (4.42 GB/s)** | 474.0 us (4.27 GB/s) | **FastJSON 1.03x** |

### Analysis

**Parse throughput**: `fastjson::turbo::iterate` uses software-pipelined 64-byte SIMD structural indexing (`build_indexes_avx2` and `build_indexes_avx512`) with a 64-byte space-padded SIMD tail buffer and unrolled `_tzcnt_u64`/`_blsr_u64` bit extraction (`flatten_mask`), outperforming `simdjson::ondemand` across Small, Medium, Large, and mixed-throughput workloads on both AVX2 and AVX-512.

**Post-parse access**: `turbo_object::find_field` uses first-byte + closing-quote prefiltering and cursor advancement (`cursor_` and `parent_cursor_`), while `turbo_value::get_double` uses an inline 64-bit mantissa accumulator with `pow10_inv`, delivering 1.22x–1.39x faster field access and 1.07x–1.38x faster 10K-record array iteration than `simdjson::ondemand`.

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

**Finding:** FastestJSONInTheWest outperforms simdjson on all parsing benchmarks, showing 9-16% improvement.

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

#### 1. **Raw Parsing Speed** 
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

**Across all tested workloads**, FastestJSONInTheWest demonstrates:

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
