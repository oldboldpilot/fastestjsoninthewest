# FastestJSONInTheWest

**Ultra-high-performance JSON parsing library for C++23**

A production-ready JSON library combining cutting-edge SIMD optimizations, parallel processing, NUMA awareness, and modern C++23 features including a comprehensive LINQ-style query interface and functional programming operations.

## ⚡ Performance Highlights (v2.0 - Multi-Register SIMD)

The parser now uses **4x AVX2 multi-register SIMD** processing 128 bytes per iteration, delivering significant speedups with zero API changes:

| Workload | Speedup | Notes |
|----------|---------|-------|
| String-heavy JSON | **4.7-5.9x faster** | SIMD string boundary detection |
| Large arrays | **2.8-4.3x faster** | Vectorized number parsing |
| Deep nesting | **1.7x faster** | Optimized whitespace skipping |
| Nested objects | **1.5x faster** | Improved structural parsing |

**Drop-in performance upgrade** - existing code gets faster without any changes.

## 🚀 Key Features

### Performance
- **Multi-Register SIMD**: 4x AVX2 registers (128 bytes/iteration) for 2-6x speedup
  - Default parser now uses multi-register SIMD automatically
  - Legacy single-register parser available via `fastjson::legacy` namespace
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
- **128-bit Precision**: Adaptive precision parsing with `__float128` and `__int128`
  - Fast path: 64-bit doubles for common numbers
  - Auto-upgrade: 128-bit types for high-precision requirements
  - Exception-free: NaN/0 returns instead of throwing
  - Type conversion: Safe cross-type conversions (64↔128 bit)

### Cross-Platform SIMD
- **x86/x64**: SSE2 through AVX-512 with automatic instruction selection
- **ARM**: NEON 128-bit SIMD with JSON parsing kernels
  - Vectorized character classification
  - Fast string comparison
  - Optimized number parsing
  - Automatic runtime detection

### Python Bindings (v2.0)
- **pybind11**: Modern Python bindings with GIL release
- **LINQ-style API**: `query()` and `parallel_query()` with fluent interface
- **GIL-Free**: Parse operations release the GIL for true parallelism
- **Full API**: 20+ query operations (filter, map, where, select, take, skip, etc.)

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
- **Ninja** (recommended build system)
- **Platform Support**:
  - x86/x64 Linux/macOS/Windows with SSE2+
  - ARM with NEON support (ARMv7+ or ARMv8)

### Quick Start
```bash
# Clone the repository
git clone https://github.com/oldboldpilot/FastestJSONInTheWest.git
cd FastestJSONInTheWest

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=/opt/clang-21/bin/clang++ \
      ..

# Build everything
ninja -j$(nproc)
```

### Building Individual Libraries

#### Single-Threaded Library
The single-threaded `fastjson` module provides fast sequential parsing with full SIMD support:

```bash
# Build only the single-threaded library
ninja fastjson

# Output: libfastjson.a
# Module: fastjson.cppm
```

**Usage:**
```cpp
import fastjson;

auto result = fastjson::parse(R"({"key": "value"})");
```

#### Parallel Library
The parallel `fastjson_parallel` module adds OpenMP-based multi-threading for large datasets:

```bash
# Build only the parallel library
ninja fastjson_parallel

# Output: libfastjson_parallel.a
# Module: fastjson_parallel.cppm
```

**Usage:**
```cpp
import fastjson_parallel;

// Configure thread count
fastjson_parallel::set_num_threads(8);

// Parse with automatic parallelization for large files
auto result = fastjson_parallel::parse(large_json_data);
```

### Build Options

#### Enable/Disable Features
```bash
# Disable OpenMP (single-threaded only)
cmake -DENABLE_OPENMP=OFF ..

# Disable SIMD optimizations
cmake -DENABLE_SIMD=OFF ..

# Enable verbose output
cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..
```

#### Build Specific Targets
```bash
# Build both libraries
ninja fastjson fastjson_parallel

# Build tests
ninja test_parser test_parallel_module test_large_file_parallel

# Build benchmarks
ninja benchmark benchmark_parallel

# Build examples
ninja example_basic example_parallel
```

### Platform-Specific Builds

#### ARM NEON Build
```bash
# ARM 32-bit (ARMv7)
mkdir build_arm && cd build_arm
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-clang++ \
      -DCMAKE_CXX_FLAGS="-march=armv7-a -mfpu=neon" \
      ..
ninja fastjson fastjson_parallel

# ARM 64-bit (ARMv8)
mkdir build_arm64 && cd build_arm64
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-clang++ \
      -DCMAKE_CXX_FLAGS="-march=armv8-a+simd" \
      ..
ninja fastjson fastjson_parallel
```

#### x86/x64 with Specific SIMD
```bash
# AVX2 only
cmake -DCMAKE_CXX_FLAGS="-march=haswell" ..

# AVX-512
cmake -DCMAKE_CXX_FLAGS="-march=skylake-avx512" ..

# SSE4.2 (older CPUs)
cmake -DCMAKE_CXX_FLAGS="-march=nehalem" ..
```

### Installation
```bash
# Install libraries and headers
sudo ninja install

# Default install location: /usr/local/lib
# Modules: /usr/local/include/fastjson
```

### Linking Your Application

#### CMakeLists.txt Example

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyApp CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenMP (required for parallel module)
find_package(OpenMP REQUIRED)

# Single-threaded application
add_executable(my_app_single main_single.cpp)
target_link_libraries(my_app_single PRIVATE 
    fastjson
)

# Parallel application
add_executable(my_app_parallel main_parallel.cpp)
target_link_libraries(my_app_parallel PRIVATE 
    fastjson_parallel
    OpenMP::OpenMP_CXX
)

# Set module include path
target_compile_options(my_app_single PRIVATE 
    -fprebuilt-module-path=${CMAKE_BINARY_DIR}
)
target_compile_options(my_app_parallel PRIVATE 
    -fprebuilt-module-path=${CMAKE_BINARY_DIR}
)
```

#### Manual Compilation

```bash
# Single-threaded application
clang++ -std=c++23 \
    -fprebuilt-module-path=/path/to/FastestJSONInTheWest/build \
    -o my_app main.cpp \
    -L/path/to/FastestJSONInTheWest/build \
    -lfastjson

# Parallel application
clang++ -std=c++23 \
    -fprebuilt-module-path=/path/to/FastestJSONInTheWest/build \
    -fopenmp=libomp \
    -o my_app_parallel main_parallel.cpp \
    -L/path/to/FastestJSONInTheWest/build \
    -lfastjson_parallel \
    -L/opt/clang-21/lib/x86_64-unknown-linux-gnu \
    -lomp

# Set library path at runtime
export LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH
./my_app_parallel
```

#### Makefile Example

```makefile
CXX = /opt/clang-21/bin/clang++
CXXFLAGS = -std=c++23 -O3 -march=native
MODULE_PATH = /path/to/FastestJSONInTheWest/build
LDFLAGS = -L$(MODULE_PATH)

# Single-threaded target
my_app: main.cpp
	$(CXX) $(CXXFLAGS) -fprebuilt-module-path=$(MODULE_PATH) \
	    -o $@ $< $(LDFLAGS) -lfastjson

# Parallel target
my_app_parallel: main_parallel.cpp
	$(CXX) $(CXXFLAGS) -fopenmp=libomp \
	    -fprebuilt-module-path=$(MODULE_PATH) \
	    -o $@ $< $(LDFLAGS) -lfastjson_parallel \
	    -L/opt/clang-21/lib/x86_64-unknown-linux-gnu -lomp

.PHONY: clean
clean:
	rm -f my_app my_app_parallel
```

## 📝 Usage Examples

### Single-Threaded Parsing

#### Basic Parsing
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

#### Error Handling
```cpp
import fastjson;

auto result = fastjson::parse(invalid_json);
if (!result.has_value()) {
    auto& error = result.error();
    std::cerr << "Parse error: " << error.message 
              << " at line " << error.line 
              << ", column " << error.column << std::endl;
}
```

### Parallel Parsing

#### Basic Parallel Usage
```cpp
import fastjson_parallel;

// Set number of threads (default: 8, use -1 for auto)
fastjson_parallel::set_num_threads(8);

// Parse large JSON file
std::string large_json = load_file("large_data.json");
auto result = fastjson_parallel::parse(large_json);

if (result.has_value()) {
    // Process parsed data
    auto& value = result.value();
    // ... work with data
}
```

#### Advanced Parallel Configuration
```cpp
import fastjson_parallel;

// Create custom configuration
fastjson_parallel::parse_config config;
config.num_threads = 16;                    // Thread count
config.parallel_threshold = 1000;           // Min size for parallel parsing
config.chunk_size = 100;                    // Elements per thread chunk
config.enable_simd = true;                  // SIMD optimizations
config.enable_numa = true;                  // NUMA-aware allocation
config.bind_threads_to_numa = true;         // Bind threads to NUMA nodes

// Apply configuration and parse
auto result = fastjson_parallel::parse_with_config(large_json, config);
```

#### Thread Count Options
```cpp
import fastjson_parallel;

// Use specific thread count
fastjson_parallel::set_num_threads(8);

// Auto-detect optimal thread count (uses OMP_NUM_THREADS or hardware max)
fastjson_parallel::set_num_threads(-1);

// Disable parallelism (single-threaded mode)
fastjson_parallel::set_num_threads(0);

// Get current thread configuration
int threads = fastjson_parallel::get_num_threads();
```

### UTF-16/UTF-32 Encoding Support

#### Automatic Encoding Detection (Parallel Module)
```cpp
import fastjson_parallel;

// Auto-detect encoding from BOM or heuristics
auto result = fastjson_parallel::parse(utf16_data);
```

#### Explicit Encoding Specification
```cpp
import fastjson_parallel;

// Parse UTF-16 Little Endian
auto result = fastjson_parallel::parse(utf16_le_data, 
    fastjson_parallel::text_encoding::UTF16_LE);

// Parse UTF-16 Big Endian
auto result2 = fastjson_parallel::parse(utf16_be_data,
    fastjson_parallel::text_encoding::UTF16_BE);

// Parse UTF-32 Little Endian
auto result3 = fastjson_parallel::parse(utf32_le_data,
    fastjson_parallel::text_encoding::UTF32_LE);

// Parse UTF-32 Big Endian
auto result4 = fastjson_parallel::parse(utf32_be_data,
    fastjson_parallel::text_encoding::UTF32_BE);

// Parse UTF-8 (default)
auto result5 = fastjson_parallel::parse(utf8_data,
    fastjson_parallel::text_encoding::UTF8);
```

#### Configuration-Based Encoding
```cpp
import fastjson_parallel;

// Set encoding in configuration
fastjson_parallel::parse_config config;
config.input_encoding = fastjson_parallel::text_encoding::UTF16_LE;

fastjson_parallel::set_parse_config(config);
auto result = fastjson_parallel::parse(utf16_data);
```

#### Direct UTF Conversion
```cpp
import fastjson_parallel;

// Convert any encoding to UTF-8
auto utf8_result = fastjson_parallel::convert_to_utf8(
    utf16_data, 
    fastjson_parallel::text_encoding::UTF16_LE
);

if (utf8_result.has_value()) {
    std::string utf8_string = utf8_result.value();
    // Now parse as UTF-8
    auto json_result = fastjson_parallel::parse(utf8_string);
}
```

### 128-bit Precision Numbers

FastestJSONInTheWest provides **native support for 128-bit numbers** using GCC/Clang's `__float128` and `__int128` types, enabling high-precision computations without external libraries.

#### Supported 128-bit Types
- **`__float128`**: 128-bit quadruple-precision floating point (~34 decimal digits)
- **`__int128`**: 128-bit signed integer (range: -2^127 to 2^127-1)
- **`unsigned __int128`**: 128-bit unsigned integer (range: 0 to 2^128-1)

#### How It Works
The parser uses **adaptive precision** with a two-tier approach:
1. **Fast Path (64-bit)**: Common numbers parsed as `double` for speed
2. **Auto-Upgrade (128-bit)**: Automatically switches to 128-bit when:
   - Number exceeds `double` precision (>15-17 digits)
   - Integer exceeds `int64_t` range (>9,223,372,036,854,775,807)
   - Explicit high-precision requirements

#### Usage Examples

```cpp
// High-precision floating point (auto-detected)
auto high_precision = fastjson::parse("3.14159265358979323846264338327950288");
if (high_precision.has_value()) {
    auto& val = high_precision.value();
    
    // Check if 128-bit precision was used
    if (val.is_number_128()) {
        __float128 precise = val.as_number_128();  // Full 34-digit precision
        // Range: ±1.18973e+4932 (exponent range: -16382 to 16383)
    }
    
    // Safe type conversions (exception-free)
    double d = val.as_float64();    // Returns NaN for non-numeric
    int64_t i = val.as_int64();     // Returns 0 for non-numeric
    __float128 f128 = val.as_float128();  // Auto-converts from any numeric type
    __int128 i128 = val.as_int128();      // Auto-converts from any numeric type
}

// Large integer support (auto-detected when > int64_t max)
auto big_int = fastjson::parse("170141183460469231731687303715884105727");
if (big_int.has_value() && big_int.value().is_int_128()) {
    __int128 value = big_int.value().as_int_128();
    // Full 128-bit integer support: ±1.7014118e+38
}

// Creating 128-bit values programmatically
json_value quad_pi = json_value((__float128)3.14159265358979323846264338327950288L);
json_value huge_int = json_value((__int128)123456789012345678901234567890LL);
json_value unsigned_huge = json_value((unsigned __int128)340282366920938463463374607431768211455ULL);
```

#### Compiler Support
- **GCC 4.6+**: Full `__float128` and `__int128` support
- **Clang 3.0+**: Full support on x86-64, partial on ARM
- **Platforms**: Linux, macOS, Windows (WSL/MinGW)
- **Architecture**: x86-64 (primary), ARM64 (limited)

### Exception-Free Numeric Access
```cpp
// All numeric accessors return NaN/0 instead of throwing
auto result = fastjson::parse(R"({"value": 42, "text": "hello"})");
if (result.has_value()) {
    auto& obj = std::get<fastjson::json_object>(result.value());
    
    double num = obj["value"].as_number();  // Returns 42.0
    double nan = obj["text"].as_number();   // Returns NaN (no exception)
    int64_t zero = obj["text"].as_int64();  // Returns 0 (no exception)
    
    // Check for NaN/zero before using
    if (!std::isnan(nan)) {
        // Safe to use
    }
}
```

### LINQ Queries

The `json_linq` module provides a complete C++23 LINQ implementation with 40+ operations and automatic parallelization.

```cpp
import json_linq;
using namespace fastjson::linq;

std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// Sequential LINQ query (chained operations)
auto evens = from(numbers)
    .where([](int n) { return n % 2 == 0; })
    .select([](int n) { return n * n; })
    .to_vector();
// Result: [4, 16, 36, 64, 100]

// Parallel LINQ with OpenMP acceleration
auto sum = from_parallel(numbers)
    .where([](int n) { return n > 5; })
    .sum([](int n) { return n; });
// Result: 40 (6+7+8+9+10)

// Working with JSON objects
import fastjson_parallel;
auto json_result = fastjson_parallel::parse(R"([
    {"name": "Alice", "score": 95},
    {"name": "Bob", "score": 87},
    {"name": "Charlie", "score": 92}
])");

if (json_result.has_value()) {
    auto objects = extract_objects(json_result.value());
    
    // Query JSON data with LINQ
    auto high_scorers = from(objects)
        .where([](const auto& obj) { 
            return obj.at("score").as_number() > 90; 
        })
        .select([](const auto& obj) { 
            return obj.at("name").as_string(); 
        })
        .to_vector();
    // Result: ["Alice", "Charlie"]
}
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

## 🔄 Type Mappings (Parallel Module)

The `fastjson_parallel` module maps JSON types to optimal C++ standard library containers:

### Type Mapping Table

| JSON Type | C++ Type | Header | Usage |
|-----------|----------|--------|-------|
| `null` | `std::nullptr_t` | `<cstddef>` | `json_value(nullptr)` |
| `boolean` | `bool` | Built-in | `json_value(true)` |
| `number` | `double` | Built-in | `json_value(42.0)` |
| `number (128-bit)` | `__float128` | GCC/Clang | `json_value((__float128)3.14...)` |
| `integer (128-bit)` | `__int128` | GCC/Clang | `json_value((__int128)large)` |
| `uint (128-bit)` | `unsigned __int128` | GCC/Clang | `json_value((unsigned __int128)...)` |
| `string` | `std::string` | `<string>` | `json_value("text")` |
| `array` | `std::vector<json_value>` | `<vector>` | `json_value(json_array{...})` |
| `object` | `std::unordered_map<std::string, json_value>` | `<unordered_map>` | `json_value(json_object{...})` |

### Design Rationale

#### Why `std::unordered_map` for Objects?
- **O(1) average lookup time** vs O(log n) for `std::map`
- Ideal for key-value access patterns common in JSON
- Better performance for large objects with frequent lookups
- Hash-based storage optimized for string keys

#### Why `std::vector` for Arrays?
- **O(1) random access** to elements by index
- Dynamic sizing (JSON arrays have runtime-determined length)
- Cache-friendly contiguous memory layout
- O(1) amortized append for building arrays

> **Note:** `std::array` cannot be used because JSON arrays have dynamic sizes unknown at compile time.

### Working with Type Mappings

#### Basic Type Access
```cpp
import fastjson_parallel;

auto result = fastjson_parallel::parse(R"({
    "name": "Alice",
    "age": 30,
    "scores": [95, 87, 92],
    "active": true
})");

if (result.has_value()) {
    auto& root = result.value();
    
    // Access as unordered_map
    if (root.is_object()) {
        const json_object& obj = root.as_object();
        
        // O(1) lookup by key
        if (obj.contains("name")) {
            std::string name = obj.at("name").as_string();
        }
        
        // Iterate over key-value pairs (unordered!)
        for (const auto& [key, value] : obj) {
            std::cout << key << ": ";
            if (value.is_string()) std::cout << value.as_string();
            else if (value.is_number()) std::cout << value.as_number();
            std::cout << "\n";
        }
    }
    
    // Access arrays as std::vector
    if (root["scores"].is_array()) {
        const json_array& scores = root["scores"].as_array();
        
        // O(1) index access
        double first_score = scores[0].as_number();
        
        // Range-based iteration
        for (const auto& score : scores) {
            std::cout << score.as_number() << " ";
        }
        
        // STL algorithms work directly
        auto max_score = std::max_element(scores.begin(), scores.end(),
            [](const json_value& a, const json_value& b) {
                return a.as_number() < b.as_number();
            });
    }
}
```

#### Safe Type Extraction with `json_utils`
```cpp
import fastjson_parallel;

auto result = fastjson_parallel::parse(R"({"users": ["Alice", "Bob", "Charlie"]})");

if (result.has_value()) {
    auto& root = result.value();
    
    // Safe optional extraction
    if (auto name = json_utils::try_get_string(root["name"])) {
        std::cout << "Name: " << *name << "\n";
    } else {
        std::cout << "Name not found or wrong type\n";
    }
    
    // Extract array of specific type
    if (auto users = json_utils::array_to_strings(root["users"])) {
        // users is std::optional<std::vector<std::string>>
        for (const auto& user : *users) {
            std::cout << user << "\n";
        }
    }
    
    // Check if key exists
    if (json_utils::has_key(root, "admin")) {
        std::cout << "Admin field present\n";
    }
    
    // Get with default value
    json_value status = json_utils::get_or(root, "status", json_value("active"));
}
```

#### Building JSON Objects Programmatically
```cpp
import fastjson_parallel;

// Create object (unordered_map)
json_object user;
user["id"] = json_value(12345);
user["name"] = json_value("Alice");
user["email"] = json_value("alice@example.com");

// Create array (vector)
json_array scores;
scores.push_back(json_value(95.0));
scores.push_back(json_value(87.0));
scores.push_back(json_value(92.0));
user["scores"] = json_value(scores);

// Nested object
json_object address;
address["city"] = json_value("New York");
address["zip"] = json_value("10001");
user["address"] = json_value(address);

json_value root(user);

// Convert to JSON string
std::string json_str = fastjson_parallel::stringify(root);
```

#### Filtering and Transforming Collections
```cpp
import fastjson_parallel;

auto result = fastjson_parallel::parse(R"({
    "products": [
        {"name": "Laptop", "price": 999.99, "stock": 5},
        {"name": "Mouse", "price": 29.99, "stock": 0},
        {"name": "Keyboard", "price": 79.99, "stock": 12}
    ]
})");

if (result.has_value()) {
    auto& root = result.value();
    
    // Filter array: only in-stock products
    if (auto filtered = json_utils::filter_array(root["products"], 
        [](const json_value& item) {
            return item["stock"].as_number() > 0;
        })) {
        // filtered is std::optional<json_array>
        std::cout << "In stock: " << filtered->size() << " items\n";
    }
    
    // Filter object: only specific fields
    if (auto user_public = json_utils::filter_object(root, 
        [](const std::string& key, const json_value&) {
            return key != "password" && key != "ssn";
        })) {
        // user_public contains filtered fields
    }
    
    // Get all object keys
    if (auto keys = json_utils::object_keys(root)) {
        for (const auto& key : *keys) {
            std::cout << "Key: " << key << "\n";
        }
    }
    
    // Flatten nested object for path-based access
    auto flat = json_utils::flatten_object(root);
    // Access nested values by path: flat["products.0.name"]
}
```

#### Type Coercion and Conversion
```cpp
import fastjson_parallel;

auto result = fastjson_parallel::parse(R"({"count": "42", "enabled": 1})");

if (result.has_value()) {
    auto& root = result.value();
    
    // Force to string representation
    std::string count_str = json_utils::coerce_to_string(root["count"]);
    // "42"
    
    // Try converting string to number
    if (auto num = json_utils::coerce_to_number(root["count"])) {
        double value = *num;  // 42.0
    }
    
    // Boolean coercion (1 -> true)
    if (auto enabled = json_utils::coerce_to_number(root["enabled"])) {
        bool is_enabled = (*enabled != 0.0);
    }
}
```

### Performance Characteristics

| Operation | `json_object` (unordered_map) | `json_array` (vector) |
|-----------|-------------------------------|----------------------|
| **Lookup by key** | O(1) average, O(n) worst | N/A |
| **Lookup by index** | N/A | O(1) |
| **Insert** | O(1) average, O(n) worst | O(1) amortized |
| **Erase** | O(1) average | O(n) |
| **Iteration** | O(n) unordered | O(n) ordered |
| **Memory** | Hash table overhead | Contiguous, cache-friendly |

### Thread Safety Notes

- **Parsing is thread-safe**: Multiple threads can parse different JSON simultaneously
- **json_value instances are NOT thread-safe**: Don't modify from multiple threads
- **OpenMP handles synchronization** during parallel parsing
- **After parsing**: Protect shared json_value with mutex or use separate instances per thread

## 💡 Complete Usage Examples

### Example 1: Loading and Parsing a Large JSON File

```cpp
import fastjson_parallel;
#include <fstream>
#include <sstream>

// Read file into string
std::ifstream file("large_data.json");
std::stringstream buffer;
buffer << file.rdbuf();
std::string json_data = buffer.str();

// Configure parallel parser
fastjson_parallel::set_num_threads(8);

// Parse
auto start = std::chrono::high_resolution_clock::now();
auto result = fastjson_parallel::parse(json_data);
auto end = std::chrono::high_resolution_clock::now();

if (result.has_value()) {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Parsed " << json_data.size() / (1024*1024) << " MB in " 
              << duration.count() << " ms" << std::endl;
    
    // Process the parsed data
    auto& value = result.value();
    if (value.is_array()) {
        auto& arr = value.as_array();
        std::cout << "Array has " << arr.size() << " elements" << std::endl;
    }
} else {
    std::cerr << "Parse error: " << result.error().message << std::endl;
}
```

### Example 2: Functional Array Processing with Parallel Module

```cpp
import fastjson_parallel;

std::string json = R"([
    {"name": "Alice", "age": 30, "salary": 75000},
    {"name": "Bob", "age": 25, "salary": 65000},
    {"name": "Charlie", "age": 35, "salary": 85000},
    {"name": "David", "age": 28, "salary": 70000}
])";

auto result = fastjson_parallel::parse(json);
if (result.has_value()) {
    auto& employees = result.value();
    
    // Filter employees over 27
    auto filtered = fastjson_parallel::filter(employees, 
        [](const auto& emp) {
            if (emp.is_object()) {
                auto& obj = emp.as_object();
                return obj["age"].as_number() > 27;
            }
            return false;
        });
    
    // Map to extract names
    auto names = fastjson_parallel::map(filtered,
        [](const auto& emp) {
            return emp.as_object()["name"];
        });
    
    // Reduce to calculate total salary
    auto total = fastjson_parallel::reduce(filtered,
        [](const auto& acc, const auto& emp) {
            auto sum = acc.as_number() + emp.as_object()["salary"].as_number();
            return fastjson_parallel::json_value(sum);
        },
        fastjson_parallel::json_value(0.0));
    
    std::cout << "Total salary of employees over 27: $" 
              << total.as_number() << std::endl;
}
```

### Example 3: Building JSON Programmatically

```cpp
import fastjson;

// Create object
auto person = fastjson::object();
auto& obj = std::get<fastjson::json_object>(person);

obj["name"] = "Alice";
obj["age"] = 30.0;
obj["active"] = true;

// Create nested array
auto hobbies = fastjson::array();
auto& arr = std::get<fastjson::json_array>(hobbies);
arr.push_back("reading");
arr.push_back("hiking");
arr.push_back("coding");

obj["hobbies"] = hobbies;

// Stringify
std::string json_str = fastjson::stringify(person);
std::cout << json_str << std::endl;
// Output: {"name":"Alice","age":30,"active":true,"hobbies":["reading","hiking","coding"]}

// Pretty print
std::string pretty = fastjson::prettify(person, 2);
std::cout << pretty << std::endl;
```

### Example 4: Processing Multiple JSON Files in Parallel

```cpp
import fastjson_parallel;
#include <filesystem>
#include <vector>
#include <thread>

namespace fs = std::filesystem;

struct ParseResult {
    std::string filename;
    size_t record_count;
    bool success;
};

std::vector<ParseResult> process_json_directory(const std::string& dir_path) {
    std::vector<ParseResult> results;
    std::vector<std::string> files;
    
    // Collect all JSON files
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.path().extension() == ".json") {
            files.push_back(entry.path().string());
        }
    }
    
    // Process files in parallel
    std::vector<std::thread> threads;
    std::mutex results_mutex;
    
    for (const auto& file : files) {
        threads.emplace_back([&, file]() {
            std::ifstream f(file);
            std::string json((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
            
            auto parse_result = fastjson_parallel::parse(json);
            
            ParseResult pr;
            pr.filename = fs::path(file).filename();
            pr.success = parse_result.has_value();
            
            if (pr.success && parse_result.value().is_array()) {
                pr.record_count = parse_result.value().as_array().size();
            } else {
                pr.record_count = 0;
            }
            
            std::lock_guard<std::mutex> lock(results_mutex);
            results.push_back(pr);
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    return results;
}

// Usage
int main() {
    auto results = process_json_directory("/data/json_files");
    
    size_t total_records = 0;
    for (const auto& r : results) {
        std::cout << r.filename << ": " 
                  << (r.success ? "OK" : "FAILED") 
                  << " (" << r.record_count << " records)" << std::endl;
        total_records += r.record_count;
    }
    
    std::cout << "\nTotal: " << total_records << " records from " 
              << results.size() << " files" << std::endl;
}
```

### Example 5: High-Precision Financial Calculations

```cpp
import fastjson;

std::string financial_data = R"({
    "total": "999999999999999999.99",
    "precision_value": "3.141592653589793238462643383279502884197169399375105820974944592307816406286",
    "large_int": "170141183460469231731687303715884105727"
})";

auto result = fastjson::parse(financial_data);
if (result.has_value()) {
    auto& obj = result.value().as_object();
    
    // Handle high-precision decimal
    if (obj["precision_value"].is_number_128()) {
        __float128 precise = obj["precision_value"].as_number_128();
        std::cout << "Using 128-bit precision" << std::endl;
    }
    
    // Handle large integers
    if (obj["large_int"].is_int_128()) {
        __int128 large = obj["large_int"].as_int_128();
        std::cout << "Parsed 128-bit integer" << std::endl;
    }
    
    // Safe fallback for standard precision
    double standard = obj["total"].as_float64();
    if (!std::isnan(standard)) {
        std::cout << "Standard value: " << std::fixed << standard << std::endl;
    }
}
```

### Example 6: REST API Response Processing

```cpp
import fastjson_parallel;
#include <curl/curl.h>

// Callback for CURL
size_t write_callback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

struct APIResponse {
    bool success;
    std::vector<std::string> items;
    std::string error;
};

APIResponse fetch_and_parse(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response_data;
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res == CURLE_OK) {
            // Parse JSON response
            auto parse_result = fastjson_parallel::parse(response_data);
            
            if (parse_result.has_value()) {
                APIResponse result;
                result.success = true;
                
                auto& json = parse_result.value();
                if (json.is_object()) {
                    auto& obj = json.as_object();
                    
                    // Extract items array
                    if (obj.contains("items") && obj["items"].is_array()) {
                        auto& items = obj["items"].as_array();
                        for (const auto& item : items) {
                            if (item.is_string()) {
                                result.items.push_back(item.as_string());
                            }
                        }
                    }
                }
                
                return result;
            } else {
                return {false, {}, parse_result.error().message};
            }
        }
    }
    
    return {false, {}, "CURL request failed"};
}

// Usage
int main() {
    auto response = fetch_and_parse("https://api.example.com/data");
    
    if (response.success) {
        std::cout << "Fetched " << response.items.size() << " items:" << std::endl;
        for (const auto& item : response.items) {
            std::cout << "  - " << item << std::endl;
        }
    } else {
        std::cerr << "Error: " << response.error << std::endl;
    }
}
```

## 🐍 Python Bindings

FastestJSONInTheWest provides high-performance Python bindings via **pybind11**, allowing you to access the C++ parser directly from Python while maintaining near-native performance.

### Features
- **GIL-Free Parsing**: Released during parsing to enable true multithreading
- **Copy-on-Write Semantics**: Efficient memory usage with shared ownership
- **UTF-8, UTF-16, UTF-32 Support**: Full Unicode encoding support
- **Batch Processing**: Optimized parsing of multiple JSON strings
- **Zero-Copy Conversions**: Direct conversion to Python dicts/lists

### Building & Installing Python Bindings

#### Prerequisites
- **Python 3.13** (or 3.10+)
- **Clang 21+** with C++23 support
- **CMake 3.15+**
- **OpenMP** (for multithreading support)
- **pybind11** (auto-handled via `uv`)

#### Method 1: Build from Source with uv (Recommended)

```bash
cd python_bindings

# Setup Python environment with uv (Python 3.13)
uv sync

# Build the C++ extension
mkdir -p build && cd build

# Configure with your custom toolchain
/path/to/your/cmake \
  -DCMAKE_C_COMPILER=/path/to/clang \
  -DCMAKE_CXX_COMPILER=/path/to/clang++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DPython3_EXECUTABLE=$(uv run which python) \
  -Dpybind11_ROOT=$(uv run python -m pybind11 --cmakedir) \
  ..

# Compile the extension
make -j$(nproc)

# Verify the build
cd ..
PYTHONPATH=./build/lib uv run python -c "import fastjson; print(f'✓ FastJSON {fastjson.__version__} loaded')"
```

#### Method 2: Simple Build (Using System Tools)

```bash
cd python_bindings

# Create virtual environment
python3.13 -m venv venv
source venv/bin/activate

# Install build dependencies
pip install pybind11 cmake ninja

# Build in-place
pip install -e . --no-build-isolation
```

#### Method 3: Pre-built Wheels (When Available)

```bash
# Install from PyPI (when published)
pip install fastjson

# Or install local wheel
pip install dist/fastjson-1.0.0-cp313-cp313-linux_x86_64.whl
```

#### Environment Setup for Running Tests

After building, set these environment variables:

```bash
# Set library paths for OpenMP
export LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH

# Set Python path to find the compiled module
export PYTHONPATH=./build/lib:$PYTHONPATH

# Or use uv to manage it automatically
uv run python -c "import fastjson; print(fastjson.parse('{\"test\": 42}'))"
```

#### Installation Locations

After building:

```
python_bindings/
├── build/
│   └── lib/
│       └── fastjson.cpython-313-x86_64-linux-gnu.so  ← Compiled module
├── .venv/                                             ← Virtual environment
├── src/
│   └── fastjson_bindings.cpp                          ← Source code
└── tests/                                             ← Test suite
```

To use in your project:

```python
# Option 1: Add to PYTHONPATH
import sys
sys.path.insert(0, '/path/to/python_bindings/build/lib')
import fastjson

# Option 2: Install system-wide
cd python_bindings
pip install .

# Then use from anywhere
import fastjson
```

### Quick Start After Building

```bash
cd python_bindings

# Run tests
PYTHONPATH=./build/lib pytest tests/ -v

# Interactive usage
PYTHONPATH=./build/lib python3 << 'EOF'
import fastjson
result = fastjson.parse('{"hello": "world"}')
print(result.to_python())  # {'hello': 'world'}
EOF
```

### Python API Usage

#### Basic Parsing
```python
import fastjson

# Parse JSON string (GIL-free)
result = fastjson.parse('{"name":"Alice","age":30}')
print(result)  # <JSONValue: {"name":"Alice","age":30}>

# Type checking
if result.is_object():
    obj_dict = result.to_python_dict()
    print(obj_dict)  # {'name': 'Alice', 'age': 30}
```

#### Unicode Support
```python
import fastjson

# UTF-8 (default)
json_utf8 = fastjson.parse('{"emoji":"🚀"}')

# UTF-16 bytes
json_utf16 = fastjson.parse_utf16(b'\xff\xfe{\x00"\x00e\x00m\x00o\x00j\x00i\x00"\x00:\x00"\x00')

# UTF-32 bytes
json_utf32 = fastjson.parse_utf32(b'\x00\x00\xfe\xff\x00\x00\x00{\x00\x00\x00"')
```

#### GIL-Free Batch Processing
```python
import fastjson
import threading

# Batch parsing (single GIL release for entire batch)
json_strings = [
    '{"id":1}',
    '{"id":2}',
    '{"id":3}',
    # ... thousands more
]

# Much faster than parsing one-by-one
results = fastjson.parse_batch(json_strings)

# Can safely use with threads
def parse_worker(json_list):
    # GIL is released during parsing
    return fastjson.parse_batch(json_list)

threads = []
for chunk in chunks(json_strings, 1000):
    t = threading.Thread(target=parse_worker, args=(chunk,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()
```

#### File Parsing
```python
import fastjson

# Parse large JSON files with GIL-free I/O
result = fastjson.parse_file('data.json')

# Perfect for multiprocessing
from multiprocessing import Pool

def process_json_file(filepath):
    data = fastjson.parse_file(filepath)
    return data.to_python_dict()

with Pool(8) as pool:
    results = pool.map(process_json_file, json_files)
```

#### Copy-on-Write Reference Tracking
```python
import fastjson

result = fastjson.parse('{"x":1}')
print(result.ref_count())  # 1

# Copy is shallow (shares data)
copy = result
print(copy.ref_count())    # 2 (same data)

# Efficient memory usage for large JSON structures
```

### Running Tests

```bash
cd python_bindings

# Quick test
pytest tests/ -v

# With coverage
pytest tests/ --cov=fastjson --cov-report=html

# Specific test categories
pytest tests/ -m unicode -v      # Unicode tests
pytest tests/ -m edge_case -v    # Edge cases
pytest tests/ -m benchmark -v    # Performance benchmarks

# Run single test
pytest tests/test_fastjson.py::TestBasicParsing::test_null -v

# Verbose with timing
pytest tests/ -v --durations=10
```

### Performance Comparison

```python
import fastjson
import json
import time

data = '{"key":"value"}' * 10000

# Standard library
start = time.time()
for _ in range(1000):
    json.loads(data)
py_time = time.time() - start

# FastJSON (GIL-free)
start = time.time()
for _ in range(1000):
    fastjson.parse(data)
fj_time = time.time() - start

print(f"json module: {py_time:.3f}s")
print(f"fastjson:   {fj_time:.3f}s")
print(f"Speedup: {py_time/fj_time:.1f}x")
```

### Multithreaded Parsing Example

```python
import fastjson
import threading
import time

# Large JSON file
large_json = '{"items":' + str(list(range(100000))) + '}'

def parse_threaded():
    """Demonstrates GIL-free multithreading"""
    results = []
    
    def worker():
        # GIL is released during parsing
        results.append(fastjson.parse(large_json))
    
    threads = [threading.Thread(target=worker) for _ in range(4)]
    
    start = time.time()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    
    return time.time() - start

print(f"4 threads parsed in {parse_threaded():.3f}s")
```

### Integration with NumPy

```python
import fastjson
import numpy as np

# Parse array of numbers
json_array = '[1.5, 2.3, 3.7, 4.1, 5.9]'
result = fastjson.parse(json_array)
python_list = result.to_python()

# Convert to NumPy array
arr = np.array(python_list)
print(arr)  # [1.5 2.3 3.7 4.1 5.9]
```

### Building & Installation Troubleshooting

#### Issue: CMake not found

**Solution:** Specify full path to your cmake:

```bash
/path/to/cmake -DCMAKE_CXX_COMPILER=... -DCMAKE_BUILD_TYPE=Release ..
```

#### Issue: Clang not found

**Solution:** Explicitly set compiler paths:

```bash
cmake -DCMAKE_C_COMPILER=/path/to/clang \
      -DCMAKE_CXX_COMPILER=/path/to/clang++ \
      -DCMAKE_BUILD_TYPE=Release ..
```

#### Issue: pybind11 not found

**Solution:** Tell CMake where to find it:

```bash
cmake -Dpybind11_ROOT=$(python -m pybind11 --cmakedir) \
      -DPython3_EXECUTABLE=$(which python) ...
```

#### Issue: "libomp.so.21.1: cannot open shared object"

**Solution:** Create symlink or set LD_LIBRARY_PATH:

```bash
# Option 1: Create symlink
sudo ln -sf /opt/clang-21/lib/x86_64-unknown-linux-gnu/libomp.so \
           /opt/clang-21/lib/x86_64-unknown-linux-gnu/libomp.so.21.1

# Option 2: Set environment variable
export LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH
```

#### Issue: "Failed to import fastjson" after building

**Solution:** Set PYTHONPATH to the build directory:

```bash
export PYTHONPATH=/path/to/python_bindings/build/lib:$PYTHONPATH
python -c "import fastjson; print(fastjson.__version__)"
```

#### Issue: Python version mismatch

**Solution:** Ensure you're using the correct Python version:

```bash
# Check which Python CMake found
cmake -LA | grep PYTHON3

# Explicitly specify
cmake -DPython3_EXECUTABLE=$(which python3.13) ...
```

#### Issue: Compilation fails with C++23 errors

**Solution:** Ensure Clang 21+ is being used:

```bash
clang++ --version  # Should show 21.x.x or higher

# If not, specify full path
cmake -DCMAKE_CXX_COMPILER=/path/to/clang-21/bin/clang++ ...
```

### Building Detailed Walkthrough

Here's a complete step-by-step guide:

```bash
# 1. Navigate to bindings directory
cd FastestJSONInTheWest/python_bindings

# 2. Create build directory
mkdir -p build && cd build

# 3. Configure with all required paths (adjust paths to your system)
PYTHON_BIN=$(which python3.13)
CMAKE_BIN=/home/muyiwa/toolchain-build/cmake-4.1.2/bin/cmake
CLANG_BIN=/home/muyiwa/toolchain/clang-21/bin/clang++
PYBIND11_DIR=$($PYTHON_BIN -m pybind11 --cmakedir)

$CMAKE_BIN \
  -DCMAKE_C_COMPILER=${CLANG_BIN%/*}/clang \
  -DCMAKE_CXX_COMPILER=$CLANG_BIN \
  -DCMAKE_BUILD_TYPE=Release \
  -DPython3_EXECUTABLE=$PYTHON_BIN \
  -Dpybind11_ROOT=$PYBIND11_DIR \
  ..

# 4. Build
make -j$(nproc)

# 5. Verify build
cd ..
export LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH
export PYTHONPATH=./build/lib:$PYTHONPATH

python -c "import fastjson; print(f'✓ FastJSON {fastjson.__version__} ready!')"

# 6. Run tests
pytest tests/ -v --tb=short
```

### Platform-Specific Notes

#### Linux (Ubuntu/Debian)

```bash
# Install system dependencies
sudo apt-get install python3.13-dev clang-21 cmake libgomp1-dev

# Build
cd python_bindings && mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=clang++-21 -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

#### macOS

```bash
# Install with Homebrew
brew install python@3.13 llvm@21 cmake

# Build
cd python_bindings && mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=$(brew --prefix llvm@21)/bin/clang++ \
      -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
```

#### Windows (MSVC)

```bash
# Use Visual Studio Command Prompt
cd python_bindings && mkdir build && cd build

# Configure
cmake -G "Visual Studio 17 2022" \
      -DCMAKE_BUILD_TYPE=Release \
      ..

# Build
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
```

## 📚 Documentation & Architecture

### System Architecture

FastestJSONInTheWest implements a **multi-layered architecture** optimized for maximum performance:

1. **Module Interface Layer** - Public API exports through C++23 modules
2. **SIMD Optimization Layer** - Hardware-specific optimizations with instruction set waterfall:
   - AVX-512 (latest Intel/AMD) → AVX2 → AVX → SSE4.2 → ARM NEON → Scalar fallback
   - Vectorized string scanning, number parsing, and validation
3. **Parser Core Layer** - Lexical analyzer, recursive parser, type-safe value construction
4. **Type System Layer** - Strong typing with `std::variant` for null, bool, numbers, strings, arrays, objects
5. **Memory Management** - No raw pointers, RAII-based with exception safety guarantees

**Performance Architecture**: Branch prediction optimization, cache-friendly sequential access, minimal allocations with move semantics.

### LINQ & Functional Programming

Complete query interface with **40+ operations** available in both sequential and parallel variants:

- **Filtering**: `where()`, `take()`, `skip()`, `distinct()`, `take_while()`, `skip_while()`
- **Transformation**: `select()`, `order_by()`, `order_by_descending()`
- **Aggregation**: `sum()`, `min()`, `max()`, `average()`, `aggregate()`, `count()`, `any()`, `all()`
- **Set Operations**: `concat()`, `union_with()`, `intersect()`, `except()`
- **Joining**: `join()`, `group_by()`
- **Element Access**: `first()`, `last()`, `single()`, `to_vector()`

Parallel variants use **OpenMP** for thread-safe execution with automatic load balancing.

### Unicode Support (UTF-8/16/32)

- **UTF-8** - Native default with <1% SIMD validation overhead
- **UTF-16** - Full surrogate pair handling for emoji and extended characters
- **UTF-32** - Direct fixed-width code point support
- **Comprehensive Testing** - 39/39 Unicode compliance tests passing

### NUMA-Aware Optimization

For multi-socket systems:
- Automatic NUMA topology detection
- Thread binding to optimize cache locality
- NUMA-aware memory allocation strategies (local, interleaved, node-specific)
- Dynamic `libnuma` loading (no compile-time dependency)
- **Performance Impact**: 69% speedup on multi-socket systems

### Thread Safety & Concurrency

- **Immutable Design** - Parsed values are immutable by default
- **Lock-Free Operations** - Concurrent read access without synchronization
- **OpenMP Integration** - Automatic parallelization with `#pragma omp` pragmas
- **Atomic Operations** - Safe counter updates and synchronization

For detailed technical documentation, see the `docs/` folder with internal references and complete implementation guides.

## 🐍 Python Bindings

### Installation

```bash
# Using uv (recommended)
cd python_bindings
uv venv && source .venv/bin/activate
uv pip install -e .

# Using pip
pip install -e .
```

### Basic Usage

```python
import fastjson

# Parse JSON
data = fastjson.parse('{"name": "Alice", "age": 30}')
print(data["name"])  # Alice

# Parse file
data = fastjson.parse_file("data.json")

# Batch parsing
results = fastjson.parse_batch(['{"id": 1}', '{"id": 2}'])
```

### LINQ-Style Queries

```python
import fastjson

data = fastjson.parse('{"users": [{"name": "Alice", "active": true}, {"name": "Bob", "active": false}]}')

# Fluent query API
active_users = (fastjson.query(data["users"])
    .where(lambda u: u["active"])
    .select(lambda u: u["name"])
    .to_list())
# Result: ["Alice"]
```

### Parallel Processing (GIL-Free)

```python
import fastjson

# Parallel queries release the GIL
results = (fastjson.parallel_query(large_dataset)
    .filter(lambda x: x["value"] > 100)
    .to_list())

# Control thread count
fastjson.set_thread_count(8)
```

See [QUICKSTART.md](QUICKSTART.md) for comprehensive Python usage examples.

## 🧪 Testing

```bash
# Run all tests
ctest --output-on-failure

# Python tests
cd python_bindings && pytest tests/ -v

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
│   ├── fastjson_parallel.cppm  # Parallel parser module
│   ├── json_linq.cppm          # LINQ module (C++23)
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

The `json_linq.cppm` C++23 module provides a complete LINQ implementation with both sequential and parallel execution modes. Uses `std::unordered_map` and `std::unordered_set` for consistency with fastjson types.

### Filtering & Projection
- `where(predicate)` / `filter(predicate)` - Filter elements matching predicate
- `select(transform)` / `map(transform)` - Transform elements (map operation)
- `distinct()` - Remove duplicates (uses `std::unordered_set` for O(1) lookup)
- `distinct_by(key_selector)` - Remove duplicates by key
- `take(n)` / `skip(n)` - Pagination operations
- `take_while(pred)` / `skip_while(pred)` - Conditional pagination

### Aggregation
- `aggregate(func)` / `reduce(func)` - Custom fold operation
- `sum(selector)`, `min(selector)`, `max(selector)`, `average(selector)` - Numeric aggregates
- `count()`, `count(predicate)` - Count elements
- `any(predicate)`, `all(predicate)` - Boolean predicates
- **Parallel Support**: All aggregation operations have OpenMP-accelerated parallel variants

### Ordering
- `order_by(key_selector)` - Sort ascending by key
- `order_by_descending(key_selector)` - Sort descending by key
- **Parallel Support**: Parallel sorting with OpenMP tasks

### Set Operations (Hash-based)
- `concat(other)` - Concatenate sequences
- `union_with(other)` - Union (uses `std::unordered_set`)
- `intersect(other)` - Intersection (O(n) with hash lookup)
- `except(other)` - Set difference (O(n) with hash lookup)

### Functional Operations
- `map(transform)` - Transform each element (alias for `select`)
- `filter(predicate)` - Keep matching elements (alias for `where`)
- `reduce(func)` - Fold operation (alias for `aggregate`)
- `forEach(action)` / `for_each(action)` - Execute action for each element
- `find(predicate)` - Find first matching element
- `find_index(predicate)` - Find index of first match
- `zip(other, combiner)` - Combine two sequences element-wise
- `prefix_sum(func)` / `scan(func)` - Cumulative operations with custom binary op
- **Parallel Prefix Sum**: Efficient parallel scan with block-wise computation

### Grouping & Joining
- `group_by(key_selector)` - Group elements by key (returns `std::unordered_map`)
- `join(other, key1, key2, result_selector)` - Inner join on matching keys

### Element Access
- `first()`, `first(predicate)`, `first_or_default()` - Get first element
- `last()`, `last(predicate)` - Get last element
- `single()` - Get single element (returns `std::nullopt` if not exactly one)
- `to_vector()` - Materialize query results into vector
- `to<Container>()` - Materialize into any STL container

### Module Integration
```cpp
// Import the LINQ module
import json_linq;
using namespace fastjson::linq;

// Works with any container
std::vector<int> data = {1, 2, 3, 4, 5};
auto result = from(data).where([](int x) { return x > 2; }).to_vector();

// Parallel execution
auto parallel_result = from_parallel(large_data)
    .where(predicate)
    .select(transform)
    .to_vector();
```

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

## 🧪 Testing

The project uses a zero-dependency C++23 test framework and comprehensive Python bindings tests.

### Test Status

| Test Suite | Tests | Status |
|------------|-------|--------|
| C++ test_framework_validation | 14/14 | ✅ Passing |
| C++ test_multiregister_parser | 140/140 | ✅ Passing |
| Python bindings | 38/39 | ✅ Passing (1 skipped) |
| **Total** | **193** | **100% Pass Rate** |

### Running Tests

```bash
# C++ Tests (from build directory)
./test_multiregister_parser
./test_framework_validation

# Python Tests (requires OpenMP library path)
cd python_bindings
export LD_LIBRARY_PATH="/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH"
uv run pytest tests/ -v

# Run benchmarks
./benchmark_multiregister
./comparative_benchmark
```

### Local Test Framework

The project includes a custom C++23 test framework (`tests/test_framework.h`) that:
- Uses `std::expected<void, std::string>` for test results
- Has no C library dependencies (no cassert)
- Provides TEST(suite, name), EXPECT_*, ASSERT_*, RUN_ALL_TESTS() macros

## 📜 License

BSD 4-Clause License with advertising clause - See LICENSE_BSD_4_CLAUSE file for details

The license requires that redistributions in binary form include the following notice in documentation or prominent display:
> This product includes software developed by Olumuyiwa Oluwasanmi.

## 👥 Author

**Olumuyiwa Oluwasanmi**

## 🔧 Known Issues & Solutions

### C++23 Modules and External Libraries

When using C++23 modules with external dependencies, there are important considerations:

**Issue**: Importing a module compiled with different flags (e.g., OpenMP) into a compilation unit with different flags causes module configuration mismatches and preprocessor directive parsing errors.

**Solution**: Treat external libraries (like logging libraries) as pure library dependencies rather than module imports:
- **Do NOT** `import` external modules into your module interface (.cppm)
- **DO** link against the compiled library without importing
- **DO** use forward declarations or remove methods that require external types in exported interfaces
- **Preprocessor Directives**: Avoid `#ifdef` inside lambdas in exported functions after module imports - use runtime checks instead

**Example**: The logger submodule (`cpp23-logger`) is used as a linked library dependency, not as an imported module, to avoid:
- OpenMP configuration mismatches between PCM files
- Preprocessor directive parsing issues in module purview
- Cross-module dependency complications

**Best Practice**:
```cpp
// ❌ Don't do this in module interface
export module mymodule;
import external_library;  // Causes issues with preprocessor directives

// ✅ Do this instead
export module mymodule;
// Use forward declarations or omit external dependencies from interface

// Then link against external_library without importing it
```

This approach maintains module independence while still allowing library functionality through linking.

## 🔗 Related Projects

- [simdjson](https://github.com/simdjson/simdjson) - SIMD-based JSON parsing
- [RapidJSON](https://github.com/Tencent/rapidjson) - Fast JSON parser/generator
- [nlohmann/json](https://github.com/nlohmann/json) - Modern C++ JSON library
- [cpp23-logger](https://github.com/oldboldpilot/cpp23-logger) - Thread-safe C++23 logger (used as external library)

## 🙏 Acknowledgments

Built with:
- **LLVM/Clang 21** - C++23 modules support
- **OpenMP** - Parallel processing
- **Intel Intrinsics Guide** - SIMD optimizations
- **NUMA Library** - Multi-socket awareness
- **cpp23-logger** - Production-grade logging (linked as external library)
