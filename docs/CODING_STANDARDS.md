# FastestJSONInTheWest - Coding Standards

## ⚠️ CRITICAL: Compiler and Toolchain Configuration (READ THIS FIRST!)

### Primary Toolchain Locations
**All AI agents and developers MUST use these exact paths:**

```bash
# C++ Compiler (Clang 21.1.5 - built from source)
COMPILER: /opt/clang-21/bin/clang++

# C Compiler
C_COMPILER: /opt/clang-21/bin/clang

# Build System (Ninja 1.13.1 - built from source, REQUIRED for C++23 modules)
NINJA: /usr/local/bin/ninja

# CMake (System installation)
CMAKE: /usr/bin/cmake
```

### ❌ DO NOT USE These Paths:
- `/home/linuxbrew/.linuxbrew/bin/clang++` (broken/incomplete)
- Any Homebrew or Linuxbrew compilers
- System package manager clang installations  
- Default `clang++` or `clang` in PATH without full path

### Required CMake Configuration Template
```cmake
cmake_minimum_required(VERSION 3.20)
project(FastestJSONInTheWest CXX)

# MANDATORY: Use exact compiler paths
set(CMAKE_CXX_COMPILER /opt/clang-21/bin/clang++)
set(CMAKE_C_COMPILER /opt/clang-21/bin/clang)
set(CMAKE_MAKE_PROGRAM /usr/local/bin/ninja)

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MUST use Ninja generator for C++23 modules
# Command: cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ...
```

### Compiler Capabilities
- **Version:** Clang 21.1.5 (built from source with full features)
- **C++ Standard:** C++23 with full modules support
- **Standard Library:** Currently libstdc++ (GCC 13), switching to libc++ after full build
- **Resource Directory:** `/opt/clang-21/lib/clang/21/include/` (builtin headers)
- **SIMD:** Full support for SSE/AVX/AVX2/AVX-512/AMX/FMA
- **OpenMP:** Will be available after full LLVM build completes
- **Tools:** clang-tidy, clang-format, clang-uml (after full build)

### Why These Specific Paths Matter
1. Clang 21 was compiled from source (`/home/muyiwa/toolchain-build/llvm-project`)
2. Ninja was built from source (system/brew versions don't support C++23 modules properly)
3. Linuxbrew installation at `/home/linuxbrew/` is incomplete/broken
4. Using wrong compiler causes missing header errors (stddef.h, etc.)

### Verification Before Building
```bash
# 1. Verify Clang 21
/opt/clang-21/bin/clang++ --version
# Expected: clang version 21.1.5

# 2. Verify Ninja
/usr/local/bin/ninja --version  
# Expected: 1.13.1

# 3. Verify builtin headers exist
ls /opt/clang-21/lib/clang/21/include/stddef.h
# Should exist without errors

# 4. Check LLVM full build status (if needed)
tail /home/muyiwa/toolchain-build/llvm-full-build/build.log
ps aux | grep ninja  # Check if still building
```

### LLVM Full Rebuild Status
**Current:** Full feature LLVM/Clang build in progress (Process 238689)
**Directory:** `/home/muyiwa/toolchain-build/llvm-full-build`
**Features:** clang, clang-tools-extra, lld, mlir, openmp, libcxx, libcxxabi, libunwind
**Post-Install:** After completion, run `sudo ninja install` to update `/opt/clang-21`

---

## Overview
This document defines the mandatory coding standards for FastestJSONInTheWest development. All code must adhere to these standards to ensure consistency, maintainability, and performance.

## C++23 Standards Compliance

### Language Features
- **Modules**: Use C++23 modules exclusively (no legacy headers in module interface)
- **Concepts**: Leverage concepts for template constraints
- **Ranges**: Prefer ranges over traditional iterators
- **Coroutines**: Use for async operations where applicable
- **Auto**: Use auto for type deduction, but be explicit for clarity

### Modern C++ Practices
```cpp
// ✅ GOOD: Modern C++23 style
auto parse_value() -> json_result<json_value> {
    auto result = some_operation();
    return result.transform([](auto&& val) {
        return json_value{std::forward<decltype(val)>(val)};
    });
}

// ❌ BAD: Old C++ style
json_value* parse_value() {
    json_value* result = new json_value();
    // Manual memory management
    return result;
}
```

## Memory Management Rules

### Smart Pointer Policy (MANDATORY)
1. **No Raw Pointers**: Raw pointers are PROHIBITED in all code
2. **Prefer unique_ptr**: For exclusive ownership
3. **Use shared_ptr**: Only when shared ownership is required
4. **RAII Everywhere**: All resources must follow RAII principles

```cpp
// ✅ GOOD: Smart pointer usage
auto create_parser(std::string_view input) -> std::unique_ptr<parser> {
    return std::make_unique<parser>(input);
}

// ❌ BAD: Raw pointer usage
parser* create_parser(std::string_view input) {
    return new parser(input); // FORBIDDEN
}
```

### Memory Safety
- **Bounds Checking**: Always validate array/string access
- **Initialize Variables**: No uninitialized variables
- **Move Semantics**: Prefer move over copy for expensive operations

```cpp
// ✅ GOOD: Safe and efficient
auto process_string(std::string input) -> json_result<std::string> {
    if (input.empty()) {
        return json_error{json_error_code::invalid_input, "Empty input"};
    }
    return std::move(input); // Efficient move
}
```

## Naming Conventions

### Functions and Variables
- **snake_case**: All functions, variables, and members
- **Descriptive Names**: Clear purpose indication
- **No Abbreviations**: Use full words

```cpp
// ✅ GOOD: Clear naming
auto parse_json_string(std::string_view input) -> json_result<std::string>;
const auto max_recursion_depth = 1000;

// ❌ BAD: Unclear naming
auto pjs(std::string_view inp) -> json_result<std::string>;
const auto mrd = 1000;
```

### Types and Classes
- **snake_case**: For all types (matching STL convention)
- **Clear Purpose**: Type name should indicate its role

```cpp
// ✅ GOOD: Clear type names
class json_parser;
struct parse_result;
enum class json_error_code;

// ❌ BAD: Unclear type names
class JP;
struct result;
enum class error;
```

### Constants and Enums
- **snake_case**: All constants and enum values
- **Scoped Enums**: Always use enum class

```cpp
// ✅ GOOD: Clear constants
constexpr auto default_buffer_size = 8192;
enum class json_type {
    null_value,
    boolean_value,
    number_value,
    string_value,
    array_value,
    object_value
};
```

## Function Design Principles

### Return Types
- **Explicit Types**: Use trailing return type syntax
- **Error Handling**: Use expected<T, Error> pattern
- **Move-Friendly**: Design for efficient moves

```cpp
// ✅ GOOD: Modern function design
auto parse_number(std::string_view input) -> json_result<double> {
    // Implementation
}

// ❌ BAD: Old-style function
double parse_number(const char* input, bool* success) {
    // Error-prone design
}
```

### Parameter Guidelines
- **string_view**: For read-only string parameters
- **const&**: For large objects that won't be moved
- **&&**: For objects that will be moved
- **by-value**: For small, copyable types

```cpp
// ✅ GOOD: Appropriate parameter types
auto create_value(std::string_view text) -> json_value;        // Read-only
auto merge_objects(json_object&& obj) -> json_value;           // Move
auto compare_values(const json_value& lhs, const json_value& rhs) -> bool; // Compare
```

## Error Handling Standards

### Error Types
- **Expected Pattern**: Use std::expected or custom json_result
- **Detailed Context**: Provide position and context information
- **No Exceptions**: Avoid exceptions in performance-critical paths

```cpp
// ✅ GOOD: Structured error handling
template<typename T>
using json_result = std::expected<T, json_error>;

auto parse_string() -> json_result<std::string> {
    if (at_end()) {
        return std::unexpected(make_error(
            json_error_code::unexpected_end,
            "Unexpected end of input while parsing string"
        ));
    }
    // Success path
}
```

### Error Context
```cpp
struct json_error {
    json_error_code code;
    std::string message;
    size_t position;
    size_t line;
    size_t column;
    
    auto what() const -> std::string {
        return std::format("JSON Error at {}:{}: {}", line, column, message);
    }
};
```

## Performance Standards

### SIMD Guidelines (COMPREHENSIVE)
- **Graceful Fallback**: Always provide scalar implementations
- **Runtime Detection**: Detect SIMD capabilities at runtime
- **Alignment**: Ensure proper memory alignment for SIMD operations
- **Instruction Set Waterfall**: Support full instruction hierarchy

#### Supported SIMD Instruction Sets (MANDATORY)
1. **SSE**: Basic 128-bit vector operations
2. **SSE2**: 128-bit integer and double precision
3. **SSE3**: Additional 128-bit operations
4. **SSE4.1/4.2**: Enhanced string/text processing
5. **AVX**: 256-bit floating point operations
6. **AVX2**: 256-bit integer operations
7. **AVX-512**: 512-bit vector operations (F, BW, CD, DQ, VL)
8. **FMA**: Fused multiply-add operations
9. **VNNI**: Vector Neural Network Instructions
10. **TMUL**: Tile matrix multiply (AMX)
11. **AMX**: Advanced Matrix Extensions
12. **ARM NEON**: ARM SIMD support

```cpp
// ✅ GOOD: Complete SIMD waterfall with benchmarking
auto find_next_delimiter(const char* data, size_t length) -> const char* {
    const auto caps = get_simd_capabilities();
    
    if (caps & SIMD_AVX512F) {
        return find_next_delimiter_avx512(data, length);
    }
    if (caps & SIMD_AVX2) {
        return find_next_delimiter_avx2(data, length);
    }
    if (caps & SIMD_AVX) {
        return find_next_delimiter_avx(data, length);
    }
    if (caps & SIMD_SSE42) {
        return find_next_delimiter_sse42(data, length);
    }
    return find_next_delimiter_scalar(data, length);
}

// ✅ REQUIRED: Performance validation for each SIMD level
struct simd_performance_results {
    double scalar_time;
    double sse_time;
    double sse2_time;
    double sse3_time;
    double sse4_time;
    double avx_time;
    double avx2_time;
    double avx512_time;
    double fma_time;
    double vnni_time;
    double openmp_time;
    double speedup_factor;
};
```

### Thread Safety
- **Immutable by Default**: Prefer immutable designs
- **Lock-Free**: Use atomic operations where possible
- **OpenMP**: Use OpenMP for parallel algorithms

```cpp
// ✅ GOOD: Thread-safe design
class json_value {
public:
    // Const methods are thread-safe
    auto is_string() const noexcept -> bool;
    auto as_string() const -> const std::string&;
    
    // Mutating methods require exclusive access
    auto push_back(json_value value) -> json_value&;
};
```

## Code Organization

### File Structure
```cpp
// Module file structure
module;                    // Global module fragment
#include <system_headers> // System includes only

export module fastjson;   // Module declaration

export namespace fastjson {
    // Public interface declarations
}

namespace fastjson {
    // Private implementation
}
```

### Documentation
- **Doxygen Comments**: For public APIs
- **Implementation Comments**: Explain complex algorithms
- **Performance Notes**: Document optimization decisions

```cpp
/// \brief Parses a JSON string from the input
/// \param input JSON string to parse
/// \return Parsed JSON value or error
/// \note This function is thread-safe and SIMD-optimized
auto parse(std::string_view input) -> json_result<json_value>;
```

## Testing Requirements (MANDATORY COMPLIANCE)

### Test Organization (STRICTLY ENFORCED)
```
tests/                           # ALL tests must be in this directory
├── unit/                       # Individual component tests
│   ├── json_value_test.cpp     # Type system validation
│   ├── parser_test.cpp         # Core parsing logic
│   └── simd_test.cpp          # SIMD operations validation
├── integration/                # End-to-end workflow tests
│   ├── module_test.cpp        # C++23 module integration
│   ├── feature_test.cpp       # Complete feature validation
│   └── workflow_test.cpp      # Real-world usage scenarios
├── performance/                # Benchmarks and profiling
│   ├── simd_benchmark.cpp     # SIMD performance validation
│   ├── large_file_benchmark.cpp # 2GB+ file processing
│   ├── instruction_set_benchmark.cpp # Per-instruction benchmarking
│   └── openmp_benchmark.cpp   # Parallel processing validation
└── compatibility/              # Cross-platform validation
    ├── compiler_test.cpp      # GCC/Clang/MSVC compatibility
    ├── platform_test.cpp     # Linux/Windows/macOS support
    └── cpp23_test.cpp        # C++23 standard compliance
```

### SIMD Benchmarking Standards (MANDATORY)
```cpp
// ✅ REQUIRED: Comprehensive SIMD performance validation
class SIMDBenchmark {
public:
    struct BenchmarkResults {
        std::string instruction_set;
        double parse_time_ms;
        double throughput_mbps;
        size_t operations_per_second;
        double speedup_vs_scalar;
        bool openmp_enabled;
        int thread_count;
    };
    
    // Test each SIMD instruction set individually
    auto benchmark_instruction_sets() -> std::vector<BenchmarkResults> {
        std::vector<BenchmarkResults> results;
        
        // Scalar baseline
        results.push_back(benchmark_scalar());
        
        // SSE family
        if (has_sse())   results.push_back(benchmark_sse());
        if (has_sse2())  results.push_back(benchmark_sse2());
        if (has_sse3())  results.push_back(benchmark_sse3());
        if (has_sse41()) results.push_back(benchmark_sse41());
        if (has_sse42()) results.push_back(benchmark_sse42());
        
        // AVX family
        if (has_avx())    results.push_back(benchmark_avx());
        if (has_avx2())   results.push_back(benchmark_avx2());
        if (has_avx512f()) results.push_back(benchmark_avx512f());
        if (has_avx512bw()) results.push_back(benchmark_avx512bw());
        
        // Advanced instructions
        if (has_fma())   results.push_back(benchmark_fma());
        if (has_vnni())  results.push_back(benchmark_vnni());
        if (has_amx())   results.push_back(benchmark_amx());
        
        // OpenMP variants
        for (int threads : {1, 2, 4, 8, 16}) {
            results.push_back(benchmark_openmp(threads));
        }
        
        return results;
    }
};

// ✅ REQUIRED: Performance validation test
TEST(Performance, SIMDSpeedupValidation) {
    SIMDBenchmark benchmark;
    auto results = benchmark.benchmark_instruction_sets();
    
    auto scalar_time = results[0].parse_time_ms; // First result is scalar
    
    for (const auto& result : results) {
        if (result.instruction_set != "scalar") {
            // Each SIMD implementation must be faster than scalar
            EXPECT_LT(result.parse_time_ms, scalar_time) 
                << "SIMD implementation " << result.instruction_set 
                << " is slower than scalar";
            
            // Document speedup factor
            EXPECT_GT(result.speedup_vs_scalar, 1.0)
                << "No speedup achieved with " << result.instruction_set;
        }
    }
}
```

### Test Standards (COMPREHENSIVE)
```cpp
// ✅ GOOD: Complete test validation
TEST(JsonParser, ParsesSimpleObject) {
    const auto input = R"({"key": "value"})";
    const auto result = fastjson::parse(input);
    
    ASSERT_TRUE(result.has_value()) << "Parse failed: " << result.error().message;
    EXPECT_TRUE(result->is_object());
    EXPECT_EQ(result->as_object().size(), 1);
    
    // Performance validation
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        auto perf_result = fastjson::parse(input);
        ASSERT_TRUE(perf_result.has_value());
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto avg_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    
    EXPECT_LT(avg_time, 100.0) << "Parse time exceeded 100μs: " << avg_time << "μs";
}
```

## Performance Expectations

### Benchmarks
- **Small JSON (<1KB)**: <0.1ms
- **Medium JSON (1MB)**: <10ms  
- **Large JSON (100MB)**: <1s
- **Massive JSON (2GB+)**: <30s

### Memory Usage
- **Minimal Overhead**: <10% memory overhead
- **No Leaks**: Zero memory leaks tolerated
- **Efficient Copying**: Minimize unnecessary copies

---
*These standards are mandatory for all FastestJSONInTheWest development. Non-compliance will result in code rejection.*