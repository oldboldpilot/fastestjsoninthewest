# FastestJSONInTheWest

**Ultra-high-performance JSON parsing library for C++23**

A production-ready JSON library combining cutting-edge SIMD optimizations, parallel processing, NUMA awareness, and modern C++23 features including a comprehensive LINQ-style query interface and functional programming operations.

## 🚀 Key Features

### Performance
- **SIMD Acceleration**: Multi-platform support
  - x86/x64: SSE2 → SSE3 → SSSE3 → SSE4.1 → SSE4.2 → AVX → AVX2 → AVX-512
  - ARM: NEON (128-bit) with optimized JSON parsing kernels
- **Parallel Parsing**: OpenMP-based multi-threaded parsing
- **NUMA-Aware**: 69% speedup on multi-socket systems
- **UTF-8 Validation**: <1% overhead with SIMD (12/12 tests passing)
- **Prefetching**: 3-5% improvement on large datasets

### Unicode Support
- **UTF-8**: Native support (default)
- **UTF-16**: Full surrogate pair handling
- **UTF-32**: Direct code point support
- **Validation**: 39/39 Unicode compliance tests passing

### LINQ & Functional Programming
- **40+ LINQ Operations**: Complete .NET-style query interface
- **Sequential & Parallel**: Automatic parallelization with OpenMP
- **Functional Primitives**: map, filter, reduce, zip, find, forEach
- **Scan Operations**: prefix_sum with custom binary operations
- **Lazy Evaluation**: Efficient query composition
- **Method Chaining**: Fluent API design

### Modern C++23
- **Modules**: Full C++23 module support
- **Thread-Safe**: All operations safe for concurrent use
- **Zero-Copy**: Efficient memory handling
- **Type-Safe**: Strong type system with `std::variant`

### Cross-Platform SIMD
- **x86/x64**: SSE2 through AVX-512 with automatic instruction selection
- **ARM**: NEON 128-bit SIMD with JSON parsing kernels
  - Vectorized character classification
  - Fast string comparison
  - Optimized number parsing
  - Automatic runtime detection

## 📊 Performance Targets

| JSON Size | Target Time | Throughput |
|-----------|-------------|------------|
| <1KB | <0.1ms | - |
| 1MB | <10ms | >100 MB/s |
| 100MB | <1s | >100 MB/s |
| 2GB+ | <30s | >68 MB/s |

## 🛠️ Building

### Requirements
- **Clang 21+** (C++23 modules support)
- **OpenMP** (parallel processing)
- **CMake 3.28+**
- **Platform Support**:
  - x86/x64 Linux/macOS/Windows with SSE2+
  - ARM with NEON support (ARMv7+ or ARMv8)

### Build Steps
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=/opt/clang-21/bin/clang++ \
      ..
cmake --build . -j$(nproc)
```

### ARM NEON Build
```bash
# ARM 32-bit (ARMv7)
mkdir build_arm && cd build_arm
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-clang++ \
      -DCMAKE_CXX_FLAGS="-march=armv7-a -mfpu=neon" \
      ..
cmake --build . -j$(nproc)

# ARM 64-bit (ARMv8)
mkdir build_arm64 && cd build_arm64
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-clang++ \
      -DCMAKE_CXX_FLAGS="-march=armv8-a+simd" \
      ..
cmake --build . -j$(nproc)
```

## 📝 Usage Examples

### Basic Parsing
```cpp
import fastjson;

std::string json = R"({"name":"Alice","age":30})";
auto result = fastjson::parse(json);

if (result.has_value()) {
    auto& obj = std::get<fastjson::json_object>(result.value());
    auto name = std::get<std::string>(obj["name"]);
    // name == "Alice"
}
```

### LINQ Queries
```cpp
#include "modules/json_linq.h"
using namespace fastjson::linq;

std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// Sequential query
auto evens = from(numbers)
    .where([](int n) { return n % 2 == 0; })
    .select([](int n) { return n * n; })
    .to_vector();
// Result: [4, 16, 36, 64, 100]

// Parallel query
auto sum = from_parallel(numbers)
    .where([](int n) { return n > 5; })
    .sum([](int n) { return n; });
// Result: 40 (6+7+8+9+10)
```

### Functional Programming
```cpp
// Map-Filter-Reduce
auto result = from(numbers)
    .map([](int x) { return x * 2; })        // [2,4,6,8,10]
    .filter([](int x) { return x > 5; })     // [6,8,10]
    .reduce([](int a, int b) { return a + b; }); // 24

// Prefix Sum (Scan)
auto cumulative = from(numbers)
    .prefix_sum()  // or .scan()
    .to_vector();
// [1, 3, 6, 10, 15, 21, 28, 36, 45, 55]

// Zip operations
std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
std::vector<int> scores = {95, 87, 92};

auto report = from(names)
    .zip(scores, [](const std::string& name, int score) {
        return name + ": " + std::to_string(score);
    })
    .to_vector();
// ["Alice: 95", "Bob: 87", "Charlie: 92"]
```

### Parallel Processing
```cpp
// Automatic parallelization
auto result = from_parallel(large_dataset)
    .where(predicate)
    .select(transform)
    .order_by(key_selector)
    .to_vector();

// Custom thread count
from_parallel(data)
    .with_threads(8)
    .aggregate(accumulator);
```

### Unicode Support
```cpp
// UTF-16 with surrogate pairs
std::u16string utf16 = u"Hello 𝕎𝕠𝕣𝕝𝕕";  // Contains 4-byte emoji
auto json = fastjson::parse_utf16(utf16);

// UTF-32 direct code points
std::u32string utf32 = U"こんにちは";
auto json = fastjson::parse_utf32(utf32);
```

## 📚 Documentation

- [Architecture](docs/ARCHITECTURE.md) - System design and components
- [LINQ Implementation](docs/LINQ_IMPLEMENTATION.md) - Query interface details
- [Unicode Support](docs/UTF16_UTF32_IMPLEMENTATION.md) - UTF-16/32 handling
- [NUMA Implementation](docs/NUMA_IMPLEMENTATION.md) - Multi-socket optimization
- [Future Work Status](docs/FUTURE_WORK_STATUS.md) - Roadmap and completion status
- [Coding Standards](docs/CODING_STANDARDS.md) - Development guidelines

## 🧪 Testing

```bash
# Run all tests
ctest --output-on-failure

# LINQ tests
./tests/linq_test

# Unicode tests
./tests/unicode_compliance_test

# Functional API tests
./tests/functional_api_test

# Prefix sum tests
./tests/prefix_sum_test

# Valgrind memory checks
./tools/run_valgrind_tests.sh
```

## 📈 Benchmarks

```bash
# LINQ performance
./benchmarks/linq_benchmark

# Parallel scaling
./benchmarks/parallel_scaling_benchmark

# SIMD instruction sets
./benchmarks/instruction_set_benchmark
```

## 🏗️ Project Structure

```
FastestJSONInTheWest/
├── modules/           # Core library
│   ├── fastjson.cppm           # Main module
│   ├── fastjson_parallel.cpp   # Parallel parser
│   ├── json_linq.h             # LINQ interface
│   └── unicode.h               # UTF-16/32 support
├── tests/             # Test suites
│   ├── linq_test.cpp
│   ├── unicode_compliance_test.cpp
│   ├── functional_api_test.cpp
│   └── prefix_sum_test.cpp
├── benchmarks/        # Performance tests
├── examples/          # Usage examples
├── docs/              # Documentation
└── tools/             # Build & test utilities
```

## 🔬 LINQ Operations Reference

### Filtering & Projection
- `where(predicate)` / `filter(predicate)` - Filter elements
- `select(transform)` / `map(transform)` - Transform elements
- `distinct()` - Remove duplicates
- `take(n)` / `skip(n)` - Pagination
- `take_while(pred)` / `skip_while(pred)` - Conditional pagination

### Aggregation
- `aggregate(func)` / `reduce(func)` - Custom aggregation
- `sum()`, `min()`, `max()`, `average()` - Numeric aggregates
- `count()`, `any()`, `all()` - Boolean aggregates

### Ordering
- `order_by(key)` / `order_by_descending(key)` - Sorting

### Set Operations
- `concat()`, `union()`, `intersect()`, `except()` - Set operations

### Functional Operations
- `map(transform)` - Transform each element
- `filter(predicate)` - Keep matching elements
- `reduce(func)` - Fold operation
- `forEach(action)` - Side effects
- `find(predicate)` - Find first match
- `find_index(predicate)` - Find index
- `zip(other, combiner)` - Combine sequences
- `prefix_sum(func)` / `scan(func)` - Cumulative operations

### Grouping & Joining
- `group_by(key)` - Group by key
- `join(other, key1, key2, result)` - Inner join

### Element Access
- `first()`, `last()`, `single()` - Element access
- `to_vector()` - Materialize results

## 🌟 Features in Detail

### Parallel LINQ
All operations available in parallel variants using OpenMP:
```cpp
from_parallel(data)
    .where(pred)
    .select(transform)
    .reduce(accumulator);
```

### Lazy Evaluation
LINQ operations are lazily evaluated and composable:
```cpp
auto query = from(data)
    .where(expensive_predicate)
    .select(transform);
// No execution yet

auto materialized = query.to_vector();  // Executes now
```

### Thread Safety
All LINQ operations are thread-safe and use OpenMP pragmas:
- `#pragma omp parallel for` - Parallel loops
- `#pragma omp reduction` - Safe accumulation
- `#pragma omp critical` - Mutual exclusion

## 📜 License

MIT License - See LICENSE file for details

## 👥 Author

**Olumuyiwa Oluwasanmi**

## 🔗 Related Projects

- [simdjson](https://github.com/simdjson/simdjson) - SIMD-based JSON parsing
- [RapidJSON](https://github.com/Tencent/rapidjson) - Fast JSON parser/generator
- [nlohmann/json](https://github.com/nlohmann/json) - Modern C++ JSON library

## 🙏 Acknowledgments

Built with:
- **LLVM/Clang 21** - C++23 modules support
- **OpenMP** - Parallel processing
- **Intel Intrinsics Guide** - SIMD optimizations
- **NUMA Library** - Multi-socket awareness
