# FastestJSONInTheWest - Claude AI Context

## Project Overview
Ultra-high-performance JSON parsing library for C++23 with SIMD acceleration, parallel processing, and comprehensive LINQ-style query interface.

## Critical Build Configuration

### Compiler Toolchain (MEMORIZE)
```bash
CXX:   /opt/clang-21/bin/clang++  # Clang 21.1.5 (built from source)
CC:    /opt/clang-21/bin/clang
BUILD: /usr/local/bin/ninja       # Ninja 1.13.1
CMAKE: /usr/bin/cmake             # CMake 3.28.3
```

### Standard Build Command
```bash
cd build
cmake -G Ninja \
  -DCMAKE_CXX_COMPILER=/opt/clang-21/bin/clang++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_OPENMP=ON \
  ..
ninja -j$(nproc)
```

### Testing
```bash
./test_simple           # 3 basic tests
./test_comprehensive    # 65 comprehensive tests  
./test_128bit          # 50 precision tests
./test_conversion_helpers  # 36 conversion tests
./comparative_benchmark    # Performance vs simdjson
```

## Key Features

### 128-bit Adaptive Precision Parsing ✨
- **Fast path**: 64-bit `double` for common numbers (~15 digit precision)
- **Auto-upgrade**: `__float128`, `__int128`, `unsigned __int128` for:
  - Decimal places > 15 digits
  - Exponent magnitude > 308
  - Integers exceeding int64_t/uint64_t range
- **Exception-free**: All numeric accessors return NaN/0 instead of throwing
- **Type conversions**: Safe cross-type conversions (64↔128 bit)

### Exception-Free Numeric API
```cpp
// All methods return NaN for floats, 0 for integers (no exceptions)
double as_number() const;              // 64-bit float
__float128 as_number_128() const;      // 128-bit float
__int128 as_int_128() const;           // 128-bit signed int
unsigned __int128 as_uint_128() const; // 128-bit unsigned int

// Conversion helpers (auto-convert between types)
int64_t as_int64() const;        // To 64-bit int
double as_float64() const;       // To 64-bit float  
__int128 as_int128() const;      // To 128-bit int
__float128 as_float128() const;  // To 128-bit float
```

### SIMD Acceleration
- **x86/x64**: SSE2 → SSE4.2 → AVX → AVX2 (AVX-512 disabled for WSL2/VM)
- **ARM**: NEON 128-bit support
- **Automatic detection**: Runtime instruction set selection

### LINQ & Functional Programming
- 40+ LINQ operations with sequential and parallel execution
- Functional primitives: map, filter, reduce, zip, scan
- Method chaining with fluent API

### Unicode Support
- UTF-8 (native), UTF-16 (surrogate pairs), UTF-32 (code points)
- 39/39 Unicode compliance tests passing

## Project Structure

```
FastestJSONInTheWest/
├── modules/
│   ├── fastjson.cppm         # Core parser module (C++23)
│   ├── fastjson.cpp          # Implementation
│   ├── json_linq.h           # LINQ query interface
│   └── fastjson_parallel.cpp # Parallel processing
├── ai/
│   ├── AGENT_GUIDELINES.md   # Comprehensive dev guidelines
│   ├── AGENT_TEST_POLICY.md  # Testing requirements
│   ├── CLAUDE.md             # This file
│   └── coding_standards.md   # Code style rules
├── docs/
│   └── ARCHITECTURE.md       # System architecture
├── documents/
│   └── API_REFERENCE.md      # Complete API docs
├── tests/                    # Test suite
├── benchmarks/              # Performance benchmarks
└── python_bindings/         # Python wrapper (pybind11)
```

## Common Tasks

### Adding New Features
1. Read `ai/AGENT_GUIDELINES.md` for coding standards
2. Implement in `modules/fastjson.cppm` (for core features)
3. Add tests in appropriate test file
4. Update `docs/ARCHITECTURE.md` if architecture changes
5. Update `documents/API_REFERENCE.md` for public APIs
6. Update `README.md` examples if user-facing

### Running Tests
```bash
cd build
ninja
./test_simple && ./test_comprehensive && ./test_128bit && ./test_conversion_helpers
```

### Performance Benchmarking
```bash
cd build
LD_LIBRARY_PATH=/opt/clang-21/lib:$LD_LIBRARY_PATH ./comparative_benchmark
```

### Formatting Code
```bash
cd /home/muyiwa/Development/FastestJSONInTheWest
find . -name "*.cpp" -o -name "*.cppm" | grep -v "build/" | grep -v "external/" | xargs clang-format -i
```

## Current Test Results (Nov 26, 2025)

- **test_simple**: 3/3 passed ✅
- **test_comprehensive**: 65/65 passed ✅
- **test_128bit**: 47/50 passed (3 edge case failures on extreme doubles)
- **test_conversion_helpers**: 36/36 passed ✅
- **Total**: 151/154 tests passing (98% success rate)

## Performance

- **1.13x-1.51x faster** than simdjson on benchmarks
- **Throughput**: >100 MB/s for typical workloads
- **NUMA-aware**: 69% speedup on multi-socket systems
- **UTF-8 validation**: <1% overhead with SIMD

## Git Workflow

### Author Policy (CRITICAL)
```bash
git config user.name "Olumuyiwa Oluwasanmi"
git config --unset user.email  # Privacy - NO email
```

### Commit Messages
- NO "Co-authored-by" lines
- NO AI assistant attribution  
- NO email addresses
- Single author: Olumuyiwa Oluwasanmi

### Syncing to Public Repo
```bash
./sync-to-public.sh  # Syncs to public GitHub repository
```

## Dependencies

- **Clang 21+**: C++23 modules support
- **OpenMP**: Parallel processing
- **CMake 3.28+**: Build system
- **Ninja 1.13+**: Build tool
- **cpp23-logger**: Logging (submodule at `external/cpp23-logger`)

## Important Notes

1. **Always use absolute paths** for compiler in CMake
2. **OpenMP must be consistent**: Enable with `-DENABLE_OPENMP=ON`
3. **Module precompilation**: Modules use `.pcm` files, must match compile flags
4. **No GCC dependencies**: Uses Clang's native `__float128` (not libquadmath)
5. **Exception-free numeric API**: Always check NaN/0 after conversion

## Recent Changes (Nov 26, 2025)

- ✅ Implemented 128-bit adaptive precision parsing
- ✅ Added `__float128`, `__int128`, `unsigned __int128` support
- ✅ Made all numeric accessors exception-free (return NaN/0)
- ✅ Added type conversion helpers (as_int64, as_float64, as_int128, as_float128)
- ✅ Updated cpp23-logger submodule to latest (std::format with filesystem::path)
- ✅ Comprehensive test suite: 151/154 tests passing

## Quick Reference

### Parse JSON
```cpp
import fastjson;
auto result = fastjson::parse(json_string);
if (result.has_value()) {
    // Use result.value()
}
```

### Access Values (Exception-Free)
```cpp
auto val = result.value();
double d = val.as_number();      // Returns NaN if not numeric
int64_t i = val.as_int64();      // Returns 0 if not numeric

if (!std::isnan(d)) {
    // Safe to use d
}
```

### 128-bit Precision
```cpp
auto big = parse("3.14159265358979323846264338327950288");
if (big.value().is_number_128()) {
    __float128 precise = big.value().as_number_128();
}
```

## Resources

- **Guidelines**: `ai/AGENT_GUIDELINES.md` - Complete development rules
- **Architecture**: `docs/ARCHITECTURE.md` - System design  
- **API**: `documents/API_REFERENCE.md` - Full API documentation
- **Tests**: All test files demonstrate correct usage patterns
