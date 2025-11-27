# FastestJSONInTheWest - Architecture Documentation

## System Architecture Overview

FastestJSONInTheWest implements a multi-layered architecture designed for maximum performance while maintaining thread safety and memory efficiency. The system includes full Unicode support (UTF-8/16/32), NUMA-aware memory allocation, and a comprehensive LINQ-style query interface with functional programming primitives.

## Core Architecture Layers

### 1. Module Interface Layer (fastjson.cppm)
**Responsibility**: Public API and module exports

```cpp
export module fastjson;
export namespace fastjson {
    // Public APIs
    auto parse(std::string_view input) -> json_result<json_value>;
    // Type system exports
    // Utility functions
}
```

### 2. SIMD Optimization Layer
**Responsibility**: Hardware-specific optimizations

#### Instruction Set Waterfall
1. **AVX-512**: Latest Intel/AMD processors
2. **AVX2**: Modern Intel/AMD processors  
3. **AVX**: Older Intel/AMD processors
4. **SSE4.2**: Legacy Intel/AMD support
5. **NEON**: ARM processors
6. **Scalar**: Universal fallback

#### SIMD Operations
- **String Scanning**: Vectorized character classification
- **Number Parsing**: SIMD-accelerated digit processing
- **Validation**: Parallel JSON structure validation

### 3. Parser Core Layer
**Responsibility**: JSON parsing logic

#### Components
- **Lexical Analyzer**: Token recognition and classification
- **Recursive Parser**: JSON structure parsing
- **Value Constructor**: Type-safe value creation
- **Error Handler**: Detailed error reporting

### 4. Type System Layer
**Responsibility**: JSON value representation

#### Type Hierarchy
```cpp
using json_data = std::variant<
    std::nullptr_t,                                    // null
    bool,                                              // boolean
    double,                                            // number (64-bit)
    std::string,                                       // string
    std::vector<json_value>,                          // array
    std::unordered_map<std::string, json_value>,      // object
    __float128,                                        // high-precision float (128-bit)
    __int128,                                          // large signed integer (128-bit)
    unsigned __int128                                  // large unsigned integer (128-bit)
>;
```

### Adaptive Precision Number Parsing

#### Strategy
FastestJSONInTheWest implements adaptive precision parsing that automatically selects the appropriate numeric type based on the number's characteristics:

1. **Fast Path (64-bit)**: Most numbers are parsed as `double` using optimized `std::strtod`
2. **Precision Analysis**: Analyzes decimal places, exponent, and integer size
3. **Auto-Upgrade (128-bit)**: Upgrades to `__float128`/`__int128` when:
   - Decimal places exceed 15 digits
   - Exponent magnitude > 308
   - Integer value exceeds int64_t/uint64_t range
4. **Precision Verification**: For integers, converts back and compares to ensure no data loss

#### 128-bit Type System

```cpp
struct number_precision_info {
    bool needs_128bit;          // Requires 128-bit precision
    bool is_integer;            // Integer vs floating-point
    bool is_negative;           // Sign information
    size_t integer_digits;      // Integer part digit count
    size_t decimal_digits;      // Fractional part digit count  
    int exponent;               // Scientific notation exponent
};
```

#### Exception-Free Numeric API

All numeric accessors use a safe, exception-free design:
- **Float accessors**: Return `NaN` for non-numeric values
- **Integer accessors**: Return `0` for non-numeric values
- **Type conversions**: Auto-convert between 64-bit and 128-bit types
- **Safety**: No exceptions thrown, easy to check with `std::isnan()` or comparison to zero

## Memory Management Strategy

### Smart Pointer Policy
- **No Raw Pointers**: Strict prohibition of raw pointer usage
- **Automatic Management**: RAII-based resource management
- **Exception Safety**: Strong exception safety guarantees

### Memory Layout Optimization
- **Contiguous Storage**: Vector-based array storage
- **Hash Map Objects**: Fast key-value lookups
- **String Interning**: Optional string deduplication
- **Move Semantics**: Efficient value transfers

## Thread Safety Architecture

### Immutable Design
- **Read-Only Values**: Parsed values are immutable by default
- **Copy-on-Write**: Efficient value sharing
- **Lock-Free Reads**: Concurrent read operations

### Synchronization Strategy
- **Parser Isolation**: Each parse operation is independent
- **Atomic Operations**: Lock-free where possible
- **OpenMP Integration**: Parallel parsing for large structures

## Performance Architecture

### SIMD Integration Points

#### 1. String Processing
```cpp
// Vectorized character classification
auto classify_chars_simd(const char* data, size_t length) -> simd_result;

// Fast string scanning
auto find_string_end_simd(const char* start) -> const char*;
```

#### 2. Number Parsing
```cpp
// SIMD digit recognition
auto parse_digits_simd(const char* digits, size_t count) -> double;

// Vectorized decimal parsing
auto parse_decimal_simd(std::string_view input) -> parse_result<double>;
```

#### 3. Validation
```cpp
// Parallel structure validation
auto validate_json_structure(const char* data, size_t length) -> bool;
```

### Optimization Techniques

#### Branch Prediction
- **Likely/Unlikely**: Compiler hints for common paths
- **Jump Tables**: Fast dispatch for character classification
- **Prefetching**: Cache-friendly memory access patterns

#### Cache Optimization
- **Sequential Access**: Linear memory traversal
- **Locality Preservation**: Keep related data together
- **Minimal Allocations**: Reduce memory fragmentation

## Error Handling Architecture

### Error Types
```cpp
enum class json_error_code {
    none,
    invalid_syntax,
    unexpected_end,
    invalid_number,
    invalid_string,
    invalid_escape,
    stack_overflow,
    out_of_memory
};
```

### Error Context
```cpp
struct json_error {
    json_error_code code;
    std::string message;
    size_t position;
    size_t line;
    size_t column;
};
```

## Build System Architecture

### CMake Integration
- **Module Support**: C++23 module compilation
- **SIMD Detection**: Runtime capability detection
- **OpenMP Integration**: Parallel compilation flags
- **Testing Framework**: Comprehensive test suite

### Compiler Support
- **Clang 21+**: Primary development compiler (C++23 modules)
- **GCC 13+**: Alternative compiler support
- **MSVC 19.35+**: Windows platform support

### Key Components
- **Unicode Module** (`modules/unicode.h`): UTF-16/32 support with surrogate pairs
- **LINQ Interface** (`modules/json_linq.h`): Query operations (40+ operations)
- **Parallel LINQ**: OpenMP-based parallel query execution
- **Functional API**: map, filter, reduce, zip, find, forEach, scan operations

## Testing Architecture

### Test Organization
```
tests/
├── unit/           # Component-level tests
├── integration/    # End-to-end tests
├── performance/    # Benchmarks and stress tests
└── compatibility/  # Cross-platform validation
```

### Test Categories
1. **Unit Tests**: Individual function validation
2. **Integration Tests**: Full parsing workflows
3. **Performance Tests**: Speed and memory benchmarks
4. **Compatibility Tests**: Platform and compiler validation

## Deployment Architecture

### Distribution Methods
1. **Header-Only**: Single-file distribution
2. **Module Package**: C++23 module distribution
3. **Static Library**: Traditional linking
4. **Dynamic Library**: Runtime loading

### Platform Support
- **Linux**: Primary development platform
- **Windows**: Full compatibility
- **macOS**: Apple Silicon and Intel support
- **Embedded**: ARM and RISC-V targets

---
*This architecture enables FastestJSONInTheWest to achieve industry-leading performance while maintaining code quality and safety.*