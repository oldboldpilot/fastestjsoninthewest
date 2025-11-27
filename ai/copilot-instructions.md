# FastestJSONInTheWest - GitHub Copilot Instructions

## Project Context
High-performance C++23 JSON parser with SIMD acceleration, 128-bit precision support, and LINQ-style queries.

## Build System
- **Compiler**: `/opt/clang-21/bin/clang++` (Clang 21.1.5)
- **Build Tool**: Ninja (`/usr/local/bin/ninja`)
- **Standard**: C++23 with modules
- **OpenMP**: Enabled for parallel processing

## Code Style

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

// Exception-free accessors
double as_number() const;          // Returns NaN for non-numeric
__float128 as_number_128() const;  // Returns NaN for non-numeric
__int128 as_int_128() const;       // Returns 0 for non-numeric
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
- Always provide scalar fallback
- Use runtime detection for instruction set
- Optimize for SSE2 → AVX2 path (AVX-512 disabled in WSL2/VM)

### Thread Safety
- Public APIs must be thread-safe
- Parsing operations are independent
- Immutable values after parsing

## Testing Requirements
- All tests in `tests/` directory
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
- **Tests**: `test_*.cpp` (root level for now)
- **Docs**: `docs/`, `documents/`, `ai/`

## Build Commands
```bash
# Configure
cmake -G Ninja -DCMAKE_CXX_COMPILER=/opt/clang-21/bin/clang++ -DENABLE_OPENMP=ON ..

# Build
ninja -j$(nproc)

# Test
./test_simple && ./test_comprehensive && ./test_128bit && ./test_conversion_helpers

# Benchmark
LD_LIBRARY_PATH=/opt/clang-21/lib:$LD_LIBRARY_PATH ./comparative_benchmark
```

## Current Features
- ✅ 128-bit adaptive precision parsing
- ✅ Exception-free numeric API
- ✅ Type conversion helpers
- ✅ SIMD acceleration (SSE2-AVX2)
- ✅ LINQ query interface
- ✅ Unicode support (UTF-8/16/32)
- ✅ Parallel processing with OpenMP

## Performance Metrics
- 1.13x-1.51x faster than simdjson
- >100 MB/s throughput
- <1% UTF-8 validation overhead
- 151/154 tests passing (98%)

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
- Context: `ai/CLAUDE.md`
