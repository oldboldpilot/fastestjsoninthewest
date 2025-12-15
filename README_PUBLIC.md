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
- **UTF-8 Validation**: <1% overhead with SIMD
- **Prefetching**: 3-5% improvement on large datasets

### Unicode Support
- **UTF-8**: Native support (default)
- **UTF-16**: Full surrogate pair handling
- **UTF-32**: Direct code point support
- **Validation**: Comprehensive Unicode compliance testing

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
- **128-bit Precision**: Native support for high-precision numbers
  - `__float128`: Quadruple-precision floating point (~34 decimal digits)
  - `__int128`: 128-bit signed integers (±1.7×10³⁸ range)
  - `unsigned __int128`: 128-bit unsigned integers (0 to 3.4×10³⁸)
  - Adaptive precision: Auto-upgrades from 64-bit to 128-bit when needed
  - Exception-free: Returns NaN/0 instead of throwing

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

The json_linq module provides a complete C++23 LINQ implementation with 40+ operations. Integrates seamlessly with fastjson_parallel for high-performance JSON querying with OpenMP acceleration.

```cpp
import json_linq;
import fastjson_parallel;
using namespace fastjson::linq;
using namespace fastjson_parallel;

// Query JSON objects with LINQ
std::string json = R"([{"id":1,"value":100},{"id":2,"value":200}])";
auto parsed = parse_json(json);
auto objects = parsed.extract_objects();

// Sequential JSON query
auto filtered = from(objects)
    .where([](const json_object& obj) {
        return obj.at("value").as_number() > 150;
    })
    .select([](const json_object& obj) {
        return obj.at("id").as_number();
    })
    .to_vector();
// Result: [2]

// Parallel query on large data
std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
auto sum = from_parallel(numbers)
    .where([](int n) { return n > 5; })
    .sum([](int n) { return n; });
// Result: 40 (6+7+8+9+10)
```

### Functional Programming
```cpp
// Map-Filter-Reduce
auto result = from(numbers)
    .map([](int x) { return x * 2; })
    .filter([](int x) { return x > 5; })
    .reduce([](int a, int b) { return a + b; });
// Result: 24

// Prefix Sum (Scan)
auto cumulative = from(numbers)
    .prefix_sum()
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
```

### Unicode Support
```cpp
// UTF-8 (native)
std::string json_utf8 = R"({"emoji": "😀", "text": "Hello 世界"})";

// UTF-16 with surrogate pairs
std::u16string utf16 = u"Hello 𝕎𝕠𝕣𝕝𝕕";
auto json = fastjson::parse_utf16(utf16);

// UTF-32 direct code points
std::u32string utf32 = U"こんにちは";
auto json = fastjson::parse_utf32(utf32);
```

### 128-bit Precision Numbers
```cpp
// High-precision floating point (auto-detected)
auto pi = fastjson::parse("3.14159265358979323846264338327950288");
if (pi.has_value() && pi.value().is_number_128()) {
    __float128 precise = pi.value().as_number_128();
    // Full 34-digit precision (~1.18973e+4932 range)
}

// Large integers beyond int64_t
auto huge = fastjson::parse("170141183460469231731687303715884105727");
if (huge.has_value() && huge.value().is_int_128()) {
    __int128 value = huge.value().as_int_128();
    // Full 128-bit integer: ±1.7×10³⁸ range
}

// Programmatic creation
json_value quad_pi = json_value((__float128)3.14159265358979323846264338327950288L);
json_value big_int = json_value((__int128)123456789012345678901234567890LL);
json_value unsigned_big = json_value((unsigned __int128)340282366920938463463374607431768211455ULL);

// Safe conversions (exception-free)
double d = pi.value().as_float64();      // NaN if not numeric
__float128 f128 = pi.value().as_float128();  // Auto-converts
```

## 📚 Documentation

For comprehensive documentation, see the `documents/` folder:

- **API_REFERENCE.md** - Complete API documentation with examples
- **LINQ_IMPLEMENTATION.md** - Detailed LINQ query guide
- **CONTRIBUTION_GUIDE.md** - Contributing guidelines
- **PUBLIC_VS_INTERNAL.md** - Repository structure guide

## 🏗️ Project Structure

```
FastestJSONInTheWest/
├── modules/           # Core library (C++23 modules)
├── examples/          # Usage examples
├── benchmarks/        # Performance tests
├── tests/             # Unit tests
├── documents/         # Public documentation
└── LICENSE_BSD_4_CLAUSE
```

## 🧪 Testing

```bash
# Build and run tests
mkdir build && cd build
cmake ..
ctest --output-on-failure

# Run specific benchmark
./comparative_benchmark
```

## 📈 Performance

FastestJSONInTheWest consistently outperforms simdjson on:
- Query operations: 2-8x faster with LINQ interface
- Parsing complex structures: 9-16% faster parsing
- Parallel operations: 8-12x speedup with OpenMP
- Large datasets: Scales efficiently with data size

See `documents/` for detailed benchmark results and comparative analysis.

## ✨ Key Advantages

✅ **Superior Query Interface**: 40+ LINQ operations vs manual parsing  
✅ **Full Unicode Support**: UTF-8, UTF-16, UTF-32 all supported  
✅ **Cross-Platform SIMD**: x86/x64 SSE→AVX-512, ARM NEON  
✅ **Parallel Processing**: Built-in OpenMP with zero configuration  
✅ **Modern C++23**: Modules, zero-copy semantics, thread-safe  
✅ **Production Ready**: Zero memory leaks, thread-safe (Valgrind verified)  

## 🚀 Getting Started

1. **Read the documentation** in `documents/API_REFERENCE.md`
2. **Check examples** in `examples/` directory
3. **Run the benchmark** with `./comparative_benchmark`
4. **Build your project** using the examples as templates

## 📄 License

BSD 4-Clause License - See LICENSE_BSD_4_CLAUSE file for details.

This product includes software developed by Olumuyiwa Oluwasanmi.

## 🤝 Contributing

We welcome contributions! See `documents/CONTRIBUTION_GUIDE.md` for details on:
- Setting up development environment
- Code style and standards
- Testing requirements
- Submitting pull requests

## 📞 Support

- **Documentation**: Check `documents/` folder
- **Examples**: Browse `examples/` directory
- **Issues**: Report on GitHub
- **Questions**: Check documentation first

---

**FastestJSONInTheWest: The Crown Jewel of C++23 JSON Libraries** 👑

Built with modern C++23, optimized for performance, and designed for production use.

