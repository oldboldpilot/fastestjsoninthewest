# Python Bindings - Testing Guide

Comprehensive testing guide for FastestJSONInTheWest Python bindings.

## Table of Contents

1. [Test Structure](#test-structure)
2. [Running Tests](#running-tests)
3. [Test Coverage](#test-coverage)
4. [Fixtures and Utilities](#fixtures-and-utilities)
5. [Building and Testing](#building-and-testing)
6. [Performance Testing](#performance-testing)
7. [Troubleshooting](#troubleshooting)

## Test Structure

### File Organization

```
python_bindings/
├── tests/
│   ├── conftest.py                  # Pytest configuration & fixtures
│   ├── test_128bit_minimal.py       # 128-bit integer minimal tests (4 tests)
│   ├── test_complete_bindings.py    # Complete API verification suite (21 tests)
│   ├── test_comprehensive.py        # Comprehensive DOM and GIL tests (8 tests)
│   ├── test_fast_conversion.py      # COW, parallel conversion & escaping (4 tests)
│   ├── test_fastjson.py             # Core API integration suite (7 tests)
│   ├── test_parallelism.py          # Parallelism limit tests (1 test)
│   ├── test_simd_features.py        # SIMD levels & capabilities suite (24 tests)
│   └── test_thorough_128bit.py      # Thorough 128-bit boundary suite (6 tests)
├── src/
│   └── fastjson_bindings.cpp        # Nanobind C++23 binding implementation
├── BUILD.sh                         # Shell build script with Clang 23+ detection
├── BUILD_AND_TEST.sh                # Shell build and test automation script
├── CMakeLists.txt                   # CMake build configuration with Clang 23+
└── pyproject.toml                   # Scikit-build-core project metadata
```

### Test Suite Overview

The test suite consists of 8 test modules comprising **75 individual test items** (all passing, 0 skipped, 0 failed):

| Test Module | Tests | Primary Focus |
|---|---|---|
| `test_128bit_minimal.py` | 4 | Basic 128-bit unsigned/signed integer conversions |
| `test_complete_bindings.py` | 21 | TurboDOM SIMD loads/dumps, PG jsonb, typed vectors, COW DOM, LINQ, Mustache, GPU fallback |
| `test_comprehensive.py` | 8 | Deep nesting, Unicode, file parsing, error handling, GIL release |
| `test_fast_conversion.py` | 4 | Copy-on-Write semantics, parallel conversions, string escaping, 128-bit fidelity |
| `test_fastjson.py` | 7 | Core integration (`test_parse_basic`, `test_parse_array`, `test_linq_query`, `test_vectors`, `test_mustache_rendering`, `test_128bit_integers`, `test_parallel_parse`) |
| `test_parallelism.py` | 1 | Multi-threading limits and thread safety |
| `test_simd_features.py` | 24 | SIMD capabilities hierarchy, levels, waterfall order, conversions, concurrency |
| `test_thorough_128bit.py` | 6 | 64-bit and 128-bit boundary transitions, overflow handling, mixed types |

**Total: 75 passed, 0 skipped, 0 failed** across all test suites under Python 3.11+ (verified on Python 3.13 and Python 3.14).

## Running Tests

### Quick Start

```bash
cd python_bindings

# Sync dependencies with uv
uv sync

# Run all 75 tests
uv run pytest tests/ -v
```

### Running Specific Test Modules

```bash
# Run complete bindings regression suite (21 tests)
uv run pytest tests/test_complete_bindings.py -v

# Run SIMD capability and waterfall tests (24 tests)
uv run pytest tests/test_simd_features.py -v

# Run core integration tests (7 tests, including test_vectors)
uv run pytest tests/test_fastjson.py -v

# Run 128-bit integer validation tests
uv run pytest tests/test_thorough_128bit.py tests/test_128bit_minimal.py -v
```

### Running Specific Test Classes and Functions

```bash
# Run TurboDOM loads and dumps tests
uv run pytest tests/test_complete_bindings.py::TestTurboLoadsAndDumps -v

# Run PostgreSQL jsonb adapter tests
uv run pytest tests/test_complete_bindings.py::TestPostgreSQLJsonbAdapter -v

# Run Typed Vectors tests (Int64Vector, UInt64Vector, Float64Vector, StringVector)
uv run pytest tests/test_complete_bindings.py::TestTypedVectors -v

# Run JSONQuery LINQ transducer tests
uv run pytest tests/test_complete_bindings.py::TestJSONQueryCompleteLINQ -v

# Run specific single test
uv run pytest tests/test_complete_bindings.py::TestTurboLoadsAndDumps::test_loads_primitives_exact_types -v
```

### Advanced Test Execution

```bash
# Run with verbose tracebacks
uv run pytest tests/ -vv --tb=long

# Stop immediately on first failure
uv run pytest tests/ -x

# Filter tests by keyword pattern
uv run pytest tests/ -k "turbo or vector or linq" -v

# Run under AddressSanitizer build
uv run pytest tests/ -v
```

## Test Coverage

### Comprehensive Test Categories

#### 1. SIMD TurboDOM API (`test_complete_bindings.py`)

- `test_loads_primitives_exact_types`: Exact type fidelity for null, booleans, integers, floats, negative zero (-0 vs -0.0), and strings.
- `test_loads_input_buffer_types`: Support for `str`, `bytes`, `bytearray`, and `memoryview`.
- `test_loads_64bit_and_128bit_exact_integers`: Exact preservation across int64, uint64, int128, and uint128 boundaries.
- `test_loads_escaped_unicode_and_surrogate_pairs`: Surrogate pairs (UTF-16 high/low) and international UTF-8.
- `test_loads_rfc8259_strict_rejection`: Strict rejection of trailing commas, unescaped control chars, lone surrogates, and IEEE-754 overflow.
- `test_turbo_parser_reusable_and_depth_limit`: Zero-allocation reuse of `TurboParser` with enforced `max_depth`.
- `test_loads_file_and_dumps_roundtrip`: File reading and serializer round-trip fidelity.
- `test_dumps_nan_handling_and_key_validation`: Strict validation during serialization.

#### 2. PostgreSQL Binary jsonb Adapter (`test_complete_bindings.py`)

- `test_wire_version_constant`: Validates `PG_JSONB_WIRE_VERSION == 1`.
- `test_pg_jsonb_roundtrip`: End-to-end wire format round-trip via `parse_pg_jsonb()`, `loads_pg_jsonb()`, and `dumps_pg_jsonb()`.
- `test_pg_jsonb_error_cases`: Rejection of invalid wire header versions and malformed payloads.

#### 3. Zero-Copy Typed Vectors (`test_complete_bindings.py`, `test_fastjson.py`)

- `test_int64_and_uint64_vectors`: Validates `Int64Vector` and `UInt64Vector` operations: `append()`, `sum()`, `to_list()`, `to_json()`.
- `test_float64_and_string_vectors`: Validates `Float64Vector` and `StringVector` operations: `append()`, `sum()`, `to_list()`, `to_json()`.
- `test_vectors`: Core integration test verifying zero-copy vector instantiation and summation.

#### 4. Copy-on-Write DOM JSONValue (`test_complete_bindings.py`, `test_comprehensive.py`, `test_fast_conversion.py`)

- `test_constructors_and_factories`: `JSONValue.object()`, `JSONValue.array()`, type queries (`is_null()`, `is_bool()`, `is_number()`, `is_string()`, etc.).
- `test_from_python_and_bytes_parse`: Interoperability between native Python structures and `JSONValue`.
- `test_cow_behavior`: Verifies O(1) Copy-on-Write shallow copies and mutation independence.

#### 5. Lazy Transducer LINQ Pipeline (`test_complete_bindings.py`, `test_fastjson.py`)

- `test_lazy_pipeline_and_early_termination`: Verifies lazy evaluation, short-circuiting with `take()`, `first()`, `any()`, `all()`.
- `test_linq_ordering_grouping_scan_and_aggregations`: Verifies `where()`, `select()`, `order_by()`, `distinct()`, `prefix_sum()`, `group_by()`, and aggregations.

#### 6. SIMD Capabilities & Waterfall Hierarchy (`test_simd_features.py`)

- Capability detection: `SIMDCapabilities` structure and feature flags (`avx512f`, `avx2`, `sse42`, `neon`).
- Waterfall fallback: Tests fallback from AVX-512 to AVX2, SSE4.2, and SCALAR.
- Concurrency: Validates multithreaded conversions and thread safety under high contention.

#### 7. 128-bit Numeric Fidelity (`test_thorough_128bit.py`, `test_128bit_minimal.py`)

- Boundary checks: Exact transitions across signed 64-bit, unsigned 64-bit, signed 128-bit, and unsigned 128-bit integers.
- Overflow rejection: Rejection of numbers exceeding 128 bits without scientific exponent notation.

## Fixtures and Utilities

### Available Fixtures in `python_bindings/tests/conftest.py`

- `sample_json_simple`: Standard dictionary with primitives.
- `sample_json_complex`: Multi-tier nested objects with metadata and lists.
- `sample_json_array`: Homogeneous and heterogeneous array structures.
- `sample_json_unicode`: Multilingual strings covering emoji and various non-Latin scripts.
- `sample_json_edge_cases`: Boundary representations (empty strings, zero values, escaped quotes).
- `sample_json_deeply_nested`: Deeply nested JSON tree.
- `sample_json_large_array`: Large array for stress testing.

## Building and Testing

### Build with uv (Recommended)

```bash
cd python_bindings
uv sync
uv pip install -e . --no-build-isolation
uv run pytest tests/ -v
```

### Build with Provided Shell Scripts

```bash
# Standard compilation
bash python_bindings/BUILD.sh

# Complete build and test run
bash python_bindings/BUILD_AND_TEST.sh
```

### Sanitizer Verification

All 75 tests are verified clean under AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
cd python_bindings
mkdir -p build && cd build
cmake .. -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
cd ..
uv run pytest tests/ -v
```

## Performance Testing

Benchmark parsing and conversion performance using pytest-benchmark:

```bash
# Run benchmarks
uv run pytest tests/ -m benchmark -v
```

## Troubleshooting

### Common Issues and Solutions

#### 1. Compiler Selection

If CMake cannot locate the appropriate compiler, ensure Clang 23+ is installed and configured:

```bash
# Verify Clang 23+ is available
clang++-23 --version
# or
/home/linuxbrew/.linuxbrew/bin/clang++ --version

# Set compiler environment variables
export CXX=/home/linuxbrew/.linuxbrew/bin/clang++
export CC=/home/linuxbrew/.linuxbrew/bin/clang
```

#### 2. Rebuilding the Extension

To perform a clean rebuild:

```bash
cd python_bindings
rm -rf build
uv pip install -e . --no-build-isolation
uv run pytest tests/ -v
```

### Verification Checklist

- Python 3.11+ installed (tested with Python 3.13 and Python 3.14)
- uv package manager installed
- Clang 23+ compiler available (`clang++-23` or `/home/linuxbrew/.linuxbrew/bin/clang++`)
- CMake 3.28+ available
- nanobind dependencies synced (`uv sync`)
- Extension built and importable
- All 75 tests passing (`uv run pytest tests/ -v`, 0 skipped, 0 failed)
