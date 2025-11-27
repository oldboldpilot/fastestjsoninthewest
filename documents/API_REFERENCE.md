# FastestJSONInTheWest API Reference

**Version:** 1.0.0  
**Last Updated:** November 26, 2025  
**License:** MIT

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Core API](#core-api)
3. [LINQ Query Interface](#linq-query-interface)
4. [Functional Programming API](#functional-programming-api)
5. [Unicode Support](#unicode-support)
6. [Parallel Operations](#parallel-operations)
7. [Error Handling](#error-handling)
8. [Performance Tips](#performance-tips)

---

## Quick Start

### Installation

```cpp
// Include the main header
#include "modules/json_linq.h"

// Or individual headers
#include "modules/fastjson.h"         // Core JSON parsing
#include "modules/json_linq.h"         // LINQ queries
#include "modules/unicode.h"           // Unicode support
#include "modules/json_parallel.h"     // Parallel processing
```

### Basic Usage

```cpp
#include "modules/json_linq.h"
using namespace fastjson::linq;

int main() {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    // Create a LINQ query
    auto result = from(data)
        .where([](int x) { return x > 2; })
        .select([](int x) { return x * 2; })
        .to_vector();
    
    // Result: {6, 8, 10}
    return 0;
}
```

---

## Core API

### JSON Parsing

#### `fastjson::Parser`

```cpp
class Parser {
    // Parse JSON string
    static std::optional<fastjson::json> parse(const std::string& json_str);
    
    // Parse with error details
    static std::pair<std::optional<fastjson::json>, std::string> 
    parse_with_error(const std::string& json_str);
    
    // Parse from file
    static std::optional<fastjson::json> parse_file(const std::string& path);
};
```

**Example:**
```cpp
auto json = fastjson::Parser::parse(R"({"name": "Alice", "age": 30})");
if (json) {
    std::cout << "Parsed successfully\n";
}
```

### JSON Types

#### `fastjson::json`

```cpp
struct json {
    // Type checking
    bool is_null() const;
    bool is_bool() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;
    
    // 128-bit type checking
    bool is_number_128() const;     // Checks if stored as __float128
    bool is_int_128() const;        // Checks if stored as __int128
    bool is_uint_128() const;       // Checks if stored as unsigned __int128
    
    // Type conversion (exception-free, returns NaN/0 for non-numeric)
    bool as_bool() const;
    double as_number() const;              // Returns NaN if not numeric
    std::string as_string() const;
    std::vector<json> as_array() const;
    std::map<std::string, json> as_object() const;
    
    // 128-bit type accessors (exception-free)
    __float128 as_number_128() const;      // Returns NaN if not numeric
    __int128 as_int_128() const;           // Returns 0 if not numeric
    unsigned __int128 as_uint_128() const; // Returns 0 if not numeric
    
    // Type conversion helpers (auto-convert between numeric types)
    int64_t as_int64() const;        // Returns 0 for non-numeric
    double as_float64() const;       // Returns NaN for non-numeric  
    __int128 as_int128() const;      // Auto-converts from any numeric type
    __float128 as_float128() const;  // Auto-converts from any numeric type
    
    // Access
    json operator[](size_t index) const;        // Array access
    json operator[](const std::string& key) const;  // Object access
    json at(const std::string& key) const;     // Object access with error
};
```

**Example:**
```cpp
auto obj = json::object({
    {"name", json::string("Bob")},
    {"age", json::number(25)},
    {"active", json::boolean(true)}
});

std::string name = obj["name"].as_string();  // "Bob"
double age = obj["age"].as_number();         // 25.0
```

**128-bit Precision Example:**
```cpp
// Parse high-precision number
auto result = fastjson::parse("3.14159265358979323846264338327950288");
if (result.has_value()) {
    auto& val = result.value();
    
    // Check precision used
    if (val.is_number_128()) {
        __float128 precise = val.as_number_128();  // Full precision
    } else {
        double approx = val.as_number();  // 64-bit precision was sufficient
    }
    
    // Safe type conversions (exception-free)
    double d = val.as_float64();        // Returns NaN for non-numeric
    int64_t i = val.as_int64();         // Returns 0 for non-numeric
    __float128 f128 = val.as_float128();  // Auto-converts from any numeric
    __int128 i128 = val.as_int128();      // Auto-converts from any numeric
    
    // Handle NaN/zero safely
    if (!std::isnan(d)) {
        // Value is numeric and convertible
    }
}

// Large integer support
auto big_int = fastjson::parse("170141183460469231731687303715884105727");
if (big_int.has_value() && big_int.value().is_int_128()) {
    __int128 value = big_int.value().as_int_128();
}
```

---

## LINQ Query Interface

### Sequential Queries

#### Creating Queries

```cpp
// From std::vector
auto query = from(std::vector<int>{1, 2, 3, 4, 5});

// From initializer list
auto query = from({1, 2, 3, 4, 5});

// From existing query result
auto query2 = query.select([](int x) { return x * 2; });
```

### Filtering Operations

#### `where(predicate)`

Filters elements matching the predicate.

```cpp
auto result = from({1, 2, 3, 4, 5})
    .where([](int x) { return x > 2; })
    .to_vector();
// Result: {3, 4, 5}
```

#### `distinct()`

Removes duplicate elements.

```cpp
auto result = from({1, 1, 2, 2, 3, 3})
    .distinct()
    .to_vector();
// Result: {1, 2, 3}
```

#### `distinct_by(key_selector)`

Removes duplicates based on a key function.

```cpp
struct Person { std::string name; int age; };

auto people = std::vector<Person>{
    {"Alice", 30}, {"Bob", 30}, {"Charlie", 25}
};

auto result = from(people)
    .distinct_by([](const Person& p) { return p.age; })
    .to_vector();
// Result: {{"Alice", 30}, {"Charlie", 25}}
```

#### `take(n)`, `skip(n)`

Take/skip first n elements.

```cpp
auto first_three = from({1, 2, 3, 4, 5})
    .take(3)
    .to_vector();
// Result: {1, 2, 3}

auto skip_two = from({1, 2, 3, 4, 5})
    .skip(2)
    .to_vector();
// Result: {3, 4, 5}
```

### Transformation Operations

#### `select(func)`

Transforms each element (map operation).

```cpp
auto doubled = from({1, 2, 3})
    .select([](int x) { return x * 2; })
    .to_vector();
// Result: {2, 4, 6}
```

#### `order_by(key_selector)`, `order_by_descending(key_selector)`

Sorts elements.

```cpp
struct Item { std::string name; int price; };

auto items = std::vector<Item>{
    {"Apple", 50}, {"Banana", 30}, {"Cherry", 40}
};

// Sort by price ascending
auto by_price = from(items)
    .order_by([](const Item& i) { return i.price; })
    .to_vector();
// Result: Banana(30), Cherry(40), Apple(50)

// Sort by price descending
auto by_price_desc = from(items)
    .order_by_descending([](const Item& i) { return i.price; })
    .to_vector();
// Result: Apple(50), Cherry(40), Banana(30)
```

### Aggregation Operations

#### `sum(selector)`, `min(selector)`, `max(selector)`, `average(selector)`

Aggregate operations.

```cpp
auto numbers = std::vector<int>{1, 2, 3, 4, 5};

auto total = from(numbers)
    .sum([](int x) { return x; });
// Result: 15

auto minimum = from(numbers)
    .min([](int x) { return x; });
// Result: 1

auto maximum = from(numbers)
    .max([](int x) { return x; });
// Result: 5

auto avg = from(numbers)
    .average([](int x) { return x; });
// Result: 3.0
```

#### `count()`, `count(predicate)`

Count elements.

```cpp
auto total_count = from({1, 2, 3, 4, 5})
    .count();
// Result: 5

auto count_evens = from({1, 2, 3, 4, 5})
    .count([](int x) { return x % 2 == 0; });
// Result: 2
```

#### `aggregate(func)`

Custom fold/reduce operation.

```cpp
auto sum = from({1, 2, 3, 4, 5})
    .aggregate([](int acc, int x) { return acc + x; });
// Result: 15

auto product = from({1, 2, 3, 4, 5})
    .aggregate([](int acc, int x) { return acc * x; });
// Result: 120
```

### Element Access

#### `first()`, `last()`, `single()`

Get specific elements.

```cpp
auto first = from({1, 2, 3})
    .first();           // Returns optional<int>, value = 1

auto last = from({1, 2, 3})
    .last();            // Returns optional<int>, value = 3

auto single = from({42})
    .single();          // Returns optional<int>, value = 42
                        // Throws if length != 1
```

#### `first_or_default(default_value)`

Get first element or default.

```cpp
auto value = from({})
    .first_or_default(99);  // Returns 99
```

### Predicate Operations

#### `any()`, `any(predicate)`, `all(predicate)`

Check conditions.

```cpp
auto has_elements = from({1, 2, 3})
    .any();             // Returns true

auto has_even = from({1, 2, 3})
    .any([](int x) { return x % 2 == 0; });  // Returns true

auto all_positive = from({1, 2, 3})
    .all([](int x) { return x > 0; });       // Returns true
```

### Set Operations

#### `concat(other)`, `union_with(other)`, `intersect(other)`, `except(other)`

Set operations on sequences.

```cpp
auto a = std::vector<int>{1, 2, 3};
auto b = std::vector<int>{3, 4, 5};

auto combined = from(a)
    .concat(b)
    .to_vector();
// Result: {1, 2, 3, 3, 4, 5}

auto unique = from(a)
    .union_with(b)
    .to_vector();
// Result: {1, 2, 3, 4, 5}

auto common = from(a)
    .intersect(b)
    .to_vector();
// Result: {3}

auto diff = from(a)
    .except(b)
    .to_vector();
// Result: {1, 2}
```

### Join Operations

#### `join(other, key1, key2, result_selector)`

Inner join two sequences.

```cpp
struct Order { int customer_id; std::string product; };
struct Customer { int id; std::string name; };

auto customers = std::vector<Customer>{
    {1, "Alice"}, {2, "Bob"}
};

auto orders = std::vector<Order>{
    {1, "Laptop"}, {1, "Mouse"}, {2, "Keyboard"}
};

struct Result { std::string customer; std::string product; };

auto joined = from(orders)
    .join(customers,
        [](const Order& o) { return o.customer_id; },
        [](const Customer& c) { return c.id; },
        [](const Order& o, const Customer& c) {
            return Result{c.name, o.product};
        })
    .to_vector();
// Result: {"Alice", "Laptop"}, {"Alice", "Mouse"}, {"Bob", "Keyboard"}
```

#### `group_by(key_selector)`

Group elements by key.

```cpp
struct Item { int category; std::string name; };

auto items = std::vector<Item>{
    {1, "Apple"}, {2, "Bread"}, {1, "Apricot"}, {2, "Bagel"}
};

auto grouped = from(items)
    .group_by([](const Item& i) { return i.category; });
// Group 1: {"Apple", "Apricot"}
// Group 2: {"Bread", "Bagel"}
```

### Conversion Operations

#### `to_vector()`, `to<Container>()`

Convert query results to containers.

```cpp
// To vector
auto vec = from({1, 2, 3})
    .select([](int x) { return x * 2; })
    .to_vector();

// To any container
auto set = from({1, 2, 3, 2, 1})
    .distinct()
    .to<std::set<int>>();
```

---

## Functional Programming API

### Functional Primitives

#### `map(func)`

Transform elements.

```cpp
auto doubled = map({1, 2, 3}, [](int x) { return x * 2; });
// Result: {2, 4, 6}
```

#### `filter(predicate)`

Filter elements.

```cpp
auto evens = filter({1, 2, 3, 4, 5}, [](int x) { return x % 2 == 0; });
// Result: {2, 4}
```

#### `reduce(func)`, `reduce(initial, func)`

Fold operation.

```cpp
auto sum = reduce({1, 2, 3, 4, 5}, [](int a, int b) { return a + b; });
// Result: 15

auto product = reduce({1, 2, 3}, 1, [](int a, int b) { return a * b; });
// Result: 6
```

#### `zip(other, func)`

Combine two sequences.

```cpp
auto a = std::vector<int>{1, 2, 3};
auto b = std::vector<int>{10, 20, 30};

auto zipped = zip(a, b, [](int x, int y) { return x + y; });
// Result: {11, 22, 33}
```

#### `find(predicate)`

Find first matching element.

```cpp
auto found = find({1, 2, 3, 4, 5}, [](int x) { return x > 3; });
// Result: optional<int> with value 4
```

#### `forEach(func)`

Execute function for each element.

```cpp
forEach({1, 2, 3}, [](int x) { 
    std::cout << x << " "; 
});
// Output: 1 2 3
```

#### `scan(func)`, `scan(initial, func)` / `prefix_sum(func)`

Cumulative/scan operations.

```cpp
auto cumsum = scan({1, 2, 3, 4, 5}, [](int a, int b) { return a + b; });
// Result: {1, 3, 6, 10, 15}

auto cumprod = scan({1, 2, 3, 4}, 1, [](int a, int b) { return a * b; });
// Result: {1, 2, 6, 24}
```

---

## Unicode Support

### UTF-8 (Native)

```cpp
std::string json_utf8 = R"({"emoji": "😀", "text": "Hello"})";
auto parsed = fastjson::Parser::parse(json_utf8);
```

### UTF-16

```cpp
// Convert to UTF-16
std::string utf16_str = fastjson::unicode::to_utf16("Hello");

// Validate UTF-16
bool valid = fastjson::unicode::is_valid_utf16(utf16_str);
```

### UTF-32

```cpp
// Convert to UTF-32
std::string utf32_str = fastjson::unicode::to_utf32("Hello");

// Get Unicode code point from UTF-32
uint32_t codepoint = fastjson::unicode::get_codepoint(utf32_str);
```

### Unicode Validation

```cpp
// Check for valid UTF-8
bool valid_utf8 = fastjson::unicode::is_valid_utf8(str);

// Validate surrogate pairs (UTF-16)
bool valid_surrogates = fastjson::unicode::is_valid_surrogate_pair(high, low);

// Check for valid code point
bool valid_codepoint = fastjson::unicode::is_valid_codepoint(0x1F600);  // 😀
```

---

## Parallel Operations

### Parallel LINQ Queries

All sequential LINQ operations have parallel equivalents:

```cpp
#include "modules/json_linq.h"
using namespace fastjson::linq;

auto result = parallel_from({1, 2, 3, 4, 5})
    .where([](int x) { return x > 2; })
    .select([](int x) { return x * 2; })
    .sum([](int x) { return x; });
```

### Parallel Query Methods

#### `parallel_where(predicate)`

Filter in parallel.

```cpp
auto result = parallel_from(large_dataset)
    .parallel_where([](int x) { return x % 2 == 0; })
    .as_sequential()
    .to_vector();
```

#### `parallel_select(func)`

Transform in parallel.

```cpp
auto result = parallel_from(data)
    .parallel_select([](int x) { return expensive_computation(x); });
```

#### `parallel_sum(selector)`, `parallel_min()`, `parallel_max()`, `parallel_count()`

Aggregate in parallel.

```cpp
auto total = parallel_from(large_numbers)
    .parallel_sum([](int x) { return x; });
```

### Thread Control

```cpp
// Set number of threads
#pragma omp parallel num_threads(8)
{
    auto result = parallel_from(data)
        .parallel_where(predicate)
        .to_vector();
}
```

---

## Error Handling

### Exception Handling

```cpp
try {
    auto json = fastjson::Parser::parse("invalid json");
    if (!json) {
        std::cerr << "Parse failed\n";
    }
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

### Error Details

```cpp
auto [json, error] = fastjson::Parser::parse_with_error(json_str);
if (!json) {
    std::cerr << "Error: " << error << "\n";
}
```

### Query Validation

```cpp
// These operations return optional<T>
auto first = from({1, 2, 3}).first();
if (first) {
    std::cout << *first << "\n";
}

// Some operations throw on invalid conditions
try {
    auto single = from({1, 2, 3}).single();  // Throws - not exactly 1 element
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

---

## Performance Tips

### 1. Use Parallel Queries for Large Datasets

```cpp
// Good for >10K elements
auto result = parallel_from(large_data)
    .parallel_where(predicate)
    .to_vector();

// Overkill for small datasets
auto result = from(small_data)
    .where(predicate)
    .to_vector();
```

### 2. Chain Operations to Avoid Intermediate Copies

```cpp
// ✓ Efficient - single pipeline
auto result = from(data)
    .where(predicate1)
    .select(transform)
    .order_by(sorter)
    .take(10)
    .to_vector();

// ✗ Inefficient - multiple intermediate vectors
auto temp1 = from(data).where(predicate1).to_vector();
auto temp2 = from(temp1).select(transform).to_vector();
auto temp3 = from(temp2).order_by(sorter).to_vector();
auto result = from(temp3).take(10).to_vector();
```

### 3. Use Appropriate Container Types

```cpp
// For lookups - use set
auto unique_ids = from(data)
    .select([](const Item& i) { return i.id; })
    .distinct()
    .to<std::set<int>>();

// For iteration - use vector
auto items = from(data)
    .where(predicate)
    .to_vector();
```

### 4. Leverage SIMD Operations

```cpp
// SIMD operations are automatic in Release builds
// Use -O3 optimization for best performance
```

### 5. Memory-Conscious Processing

```cpp
// Process in batches instead of all at once
for (size_t i = 0; i < total; i += batch_size) {
    auto batch = from(data)
        .skip(i)
        .take(batch_size)
        .to_vector();
    process(batch);
}
```

---

## Compilation

### Required Compiler

- **Clang 21+** (with C++23 support)
- **C++ Standard**: C++23 or later

### Compilation Flags

```bash
# Recommended compilation
clang++ -std=c++23 -O3 -fopenmp -ffast-math -march=native \
    -o program program.cpp

# With sanitizers (debug)
clang++ -std=c++23 -g -fsanitize=address -fopenmp \
    -o program program.cpp
```

### CMake Integration

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyProject)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE OpenMP::OpenMP_CXX)
```

---

## Testing

Run the comprehensive test suite:

```bash
# Build tests
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make tests

# Run tests
./test_linq
./test_unicode
./test_parallel_operations
./comparative_benchmark
```

---

## Examples

See `examples/` directory for comprehensive usage examples:

- `fluent_api_examples.cpp` - LINQ usage patterns
- `functional_api_examples.cpp` - Functional programming
- Data transformation pipelines
- Real-world JSON processing scenarios

---

## License

MIT License - See LICENSE file for details

---

**For more information:**
- 📖 [LINQ Implementation Guide](./LINQ_IMPLEMENTATION.md)
- 🚀 [Performance Benchmarks](./BENCHMARK_RESULTS.md)
- 🏗️ [Architecture Guide](./ARCHITECTURE.md)
- 📝 [Contributor Guidelines](./CONTRIBUTION_GUIDE.md)

