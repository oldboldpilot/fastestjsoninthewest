# FastestJSONInTheWest - GitHub Copilot Instructions

> **SYNC POLICY**: This file MUST be kept in sync with `ai/CLAUDE.md`, `ai/gemini.md`, and `.github/copilot-instructions.md`. Last sync: January 19, 2026.

## Project Context
High-performance C++23 JSON parser with **4x multi-register SIMD** acceleration (128 bytes/iteration), 128-bit precision support, LINQ-style queries, and Mustache templating.

## v2.0 Performance Update
- Default parser now uses **4x AVX2 multi-register SIMD** (2-6x faster)
- String-heavy JSON: 4.7-5.9x faster
- Large arrays: 2.8-4.3x faster
- Zero API changes - existing code gets faster automatically

## v2.1 Features
- **Mustache Templating**: Logic-less templates with `fastjson::mustache::render` (C++23 module).
- **Nanobind Python Bindings**: Next-gen GIL-free bindings using `uv` and `nanobind`.

## v2.2 Features (January 2026)
- **SIMD Waterfall API**: Python bindings expose full SIMD level selection with automatic waterfall
- **SIMD Capabilities Detection**: `get_simd_capabilities()` returns detected CPU features
- **Levels**: AVX-512 → AMX → AVX2 → AVX → SSE4 → SSE2 → SCALAR
- **Parallelism Control**: `to_python(threads=N, simd_level=...)` for conversion

## Build System
- **Compiler**: `/opt/clang-21/bin/clang++` (Clang 21.1.5)
- **Build Tool**: Ninja (`/usr/local/bin/ninja`)
- **Standard**: C++23 with modules
- **OpenMP**: Enabled for parallel processing

## LINQ Module (C++23)
- **File**: `modules/json_linq.cppm` (not `.h`)
- **Import syntax**: `import json_linq;` (not `#include`)
- **Namespace**: `fastjson::linq`
- **Classes**: `query_result<T>` (sequential), `parallel_query_result<T>` (OpenMP)
- **Containers**: `std::unordered_map`, `std::unordered_set` for consistency with fastjson
- **40+ operations**: where, select, order_by, group_by, sum, min, max, aggregate, etc.
- **Helper functions**: `from()`, `from_parallel()`

Example:
```cpp
import json_linq;
using namespace fastjson::linq;

auto result = from(data)
    .where([](auto& x) { return x > 5; })
    .select([](auto& x) { return x * 2; })
    .to_vector();
```

### Naming Conventions
- **Functions**: `snake_case` with trailing syntax (e.g., `auto parse() -> result`)
- **Variables**: `snake_case`
- **Types/Classes**: `snake_case` (STL style)
- **Constants**: `snake_case`

### Modern C++ Features
- Use `auto` with trailing return types
- Prefer `std::variant` for type-safe unions
- Use `std::expected` for error handling
- No raw pointers - use smart pointers only
- Exception-free numeric APIs (return NaN/0)

## Key Implementation Details

### 128-bit Precision Numbers
```cpp
// Adaptive precision: 64-bit fast path, auto-upgrade to 128-bit
using json_number = double;           // Default
using json_number_128 = __float128;   // High precision
using json_int_128 = __int128;        // Large integers
using json_uint_128 = unsigned __int128;

// Exception-free accessors - THREE categories:

// 1. PRIMARY (JSON standard - always double)
double as_number() const;          // Converts any numeric to double

// 2. STRICT (return default if wrong type)
__float128 as_number_128() const;  // Returns NaN if not __float128
__int128 as_int_128() const;       // Returns 0 if not __int128
unsigned __int128 as_uint_128() const; // Returns 0 if not unsigned __int128

// 3. CONVERTING (auto-convert between types)
int64_t as_int64() const;          // Any numeric → int64_t
uint64_t as_uint64() const;        // Any numeric → uint64_t
double as_float64() const;         // Any numeric → double
__int128 as_int128() const;        // Any numeric → __int128
unsigned __int128 as_uint128() const; // Any numeric → unsigned __int128
__float128 as_float128() const;    // Any numeric → __float128

// IMPORTANT: Storage preserves exact type, use converting accessors
// to recover values with appropriate conversions.
```

### Type System
```cpp
using json_data = std::variant<
    std::nullptr_t,
    bool,
    double,
    std::string,
    std::vector<json_value>,
    std::unordered_map<std::string, json_value>,
    __float128,
    __int128,
    unsigned __int128
>;
```

### SIMD Integration
- **Default: 4x multi-register AVX2** (128 bytes per iteration)
- Always provide scalar fallback
- Use runtime detection for instruction set
- Optimize for SSE2 → AVX2 path (AVX-512 disabled in WSL2/VM)
- Multi-register pattern: Load 4 chunks → Process in parallel → Early exit on match

### Thread Safety
- Public APIs must be thread-safe
- Parsing operations are independent
- Immutable values after parsing

## Testing Requirements
- All tests in `tests/` directory
- **Local test framework**: `tests/test_framework.h` - zero-dependency C++23 framework
  - Uses `std::expected<void, std::string>` for test results
  - No C libraries (no cassert) - pure modern C++
  - Macros: TEST(suite, name), EXPECT_*, ASSERT_*, RUN_ALL_TESTS()
- Categorize: unit/, integration/, performance/
- Aim for >95% code coverage
- Test 128-bit precision edge cases
- Verify exception-free behavior

## Common Patterns

### Error Handling
```cpp
auto parse(std::string_view input) -> json_result<json_value> {
    if (input.empty()) {
        return std::unexpected(make_error(
            json_error_code::empty_input,
            "Empty input"
        ));
    }
    // ...
}
```

### Safe Numeric Access
```cpp
// Always check for NaN/0 after conversion
double d = value.as_number();
if (!std::isnan(d)) {
    // Safe to use
}

int64_t i = value.as_int64();
if (i != 0 || value.is_number()) {
    // Valid integer
}
```

### SIMD with Fallback
```cpp
auto process_data(const char* data, size_t len) -> result {
    #ifdef HAVE_AVX2
    if (cpu_supports_avx2()) {
        return process_avx2(data, len);
    }
    #endif
    return process_scalar(data, len);
}
```

## File Locations
- **Core**: `modules/fastjson.cppm`, `modules/fastjson.cpp`
- **LINQ**: `modules/json_linq.h`
- **Python Bindings**: `python_bindings/src/fastjson_bindings.cpp`
- **Tests**: `tests/unit/`, `tests/integration/`, `tests/performance/`
- **Docs**: `docs/`, `documents/`, `ai/`
- **Scripts**: `scripts/`

## Python Bindings (v2.0)
- **pybind11**: Full Python bindings with GIL release
- **LINQ API**: `query()` and `parallel_query()` with fluent interface
- **Installation**: `cd python_bindings && uv pip install -e .`
- **OpenMP**: Set `LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu`

```python
import fastjson

# Parse with GIL release
data = fastjson.parse('{"name": "Alice"}')

# LINQ-style queries
result = (fastjson.query(data["users"])
    .where(lambda u: u["active"])
    .select(lambda u: u["name"])
    .to_list())

# Parallel queries (OpenMP)
result = fastjson.parallel_query(large_list).filter(pred).to_list()
```

## Build Commands
```bash
# Configure
cmake -G Ninja -DCMAKE_CXX_COMPILER=/opt/clang-21/bin/clang++ -DENABLE_OPENMP=ON ..

# Build
ninja -j$(nproc)

# Test C++
cd tests && ctest --output-on-failure

# Test Python
cd python_bindings && source .venv/bin/activate
LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH pytest tests/ -v
```

## Current Features
- ✅ 128-bit adaptive precision parsing
- ✅ Exception-free numeric API
- ✅ Type conversion helpers
- ✅ SIMD acceleration (SSE2-AVX2)
- ✅ LINQ query interface (C++ and Python)
- ✅ Unicode support (UTF-8/16/32)
- ✅ Parallel processing with OpenMP
- ✅ Python bindings with GIL release

## Performance Metrics
- 1.13x-1.51x faster than simdjson
- >100 MB/s throughput
- <1% UTF-8 validation overhead
- 193/193 tests passing (100%)
  - C++ test_framework_validation: 14/14
  - C++ test_multiregister_parser: 140/140
  - Python bindings: 38/39 (1 intentionally skipped)

## Important Notes
1. No libquadmath - use Clang native `__float128`
2. OpenMP must be consistent across all modules
3. All numeric accessors are exception-free
4. Always format with clang-format before commit
5. Single author: Olumuyiwa Oluwasanmi (no email)

## Resources
- Guidelines: `ai/AGENT_GUIDELINES.md`
- Architecture: `docs/ARCHITECTURE.md`  
- API Reference: `documents/API_REFERENCE.md`
- Quick Start: `QUICKSTART.md`
