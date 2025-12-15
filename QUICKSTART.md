# FastestJSONInTheWest - Quick Start Guide

Get up and running with FastestJSONInTheWest in minutes.

## Table of Contents
- [Python Installation](#python-installation)
- [Python Usage](#python-usage)
- [C++ Installation](#c-installation)
- [C++ Usage](#c-usage)
- [Performance Tips](#performance-tips)

---

## Python Installation

### Using uv (Recommended)
```bash
# Install from source
git clone https://github.com/oldboldpilot/FastestJSONInTheWest.git
cd FastestJSONInTheWest/python_bindings

# Create virtual environment and install
uv venv
source .venv/bin/activate
uv pip install -e .
```

### Using pip
```bash
cd FastestJSONInTheWest/python_bindings
pip install -e .
```

### Requirements
- Python 3.11+
- C++23 compiler (Clang 21+ recommended)
- CMake 3.15+
- Ninja (optional but recommended)

---

## Python Usage

### Basic Parsing

```python
import fastjson

# Parse JSON string
data = fastjson.parse('{"name": "Alice", "age": 30}')
print(data["name"])  # Alice
print(data["age"])   # 30

# Parse file
data = fastjson.parse_file("data.json")

# Batch parsing (multiple JSON strings)
results = fastjson.parse_batch([
    '{"id": 1}',
    '{"id": 2}',
    '{"id": 3}'
])
```

### LINQ-Style Queries

The library provides a fluent, LINQ-style API for querying JSON data:

```python
import fastjson

# Sample data
data = fastjson.parse('''
{
    "users": [
        {"name": "Alice", "age": 30, "active": true},
        {"name": "Bob", "age": 25, "active": false},
        {"name": "Carol", "age": 35, "active": true}
    ]
}
''')

# Create a query
query = fastjson.query(data["users"])

# Filter and transform
active_names = (query
    .where(lambda u: u["active"])
    .select(lambda u: u["name"])
    .to_list())
# Result: ["Alice", "Carol"]

# Chained operations
result = (fastjson.query(data["users"])
    .where(lambda u: u["age"] >= 30)
    .order_by(lambda u: u["age"])
    .take(2)
    .to_list())
```

### Available Query Operations

| Operation | Description |
|-----------|-------------|
| `where(pred)` / `filter(pred)` | Filter elements matching predicate |
| `select(fn)` / `map(fn)` | Transform each element |
| `take(n)` | Take first n elements |
| `skip(n)` | Skip first n elements |
| `first()` / `last()` | Get first/last element |
| `order_by(key)` | Sort ascending |
| `order_by_descending(key)` | Sort descending |
| `distinct()` | Remove duplicates |
| `reverse()` | Reverse order |
| `concat(other)` | Concatenate with another sequence |
| `any(pred)` / `all(pred)` | Check if any/all match |
| `count()` / `count_if(pred)` | Count elements |
| `reduce(fn, init)` | Aggregate to single value |
| `to_list()` / `to_vector()` | Convert to list |

### Parallel Processing (GIL-Free)

For large datasets, use parallel queries for multi-threaded processing:

```python
import fastjson

# Create parallel query (releases GIL)
large_data = fastjson.parse_file("large_dataset.json")

# Parallel operations use OpenMP
results = (fastjson.parallel_query(large_data["items"])
    .filter(lambda x: x["value"] > 100)
    .to_list())

# Control thread count
fastjson.set_thread_count(8)
print(f"Using {fastjson.get_thread_count()} threads")
```

### SIMD Information

```python
import fastjson

# Check SIMD capabilities
info = fastjson.get_simd_info()
print(f"AVX2: {info.has_avx2}")
print(f"AVX512: {info.has_avx512}")
print(f"Mode: {fastjson.__simd_mode__}")
```

### Serialization

```python
import fastjson

data = {"name": "test", "values": [1, 2, 3]}

# Compact output
json_str = fastjson.stringify(data)
# {"name":"test","values":[1,2,3]}

# Pretty print (if available)
# json_str = fastjson.stringify(data, pretty=True)
```

---

## C++ Installation

### Requirements
- Clang 21+ (C++23 modules support)
- CMake 3.28+
- Ninja (recommended)
- OpenMP (for parallel features)

### Build from Source

```bash
git clone https://github.com/oldboldpilot/FastestJSONInTheWest.git
cd FastestJSONInTheWest

mkdir build && cd build

cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/opt/clang-21/bin/clang++ \
    ..

ninja -j$(nproc)
```

---

## C++ Usage

### Basic Parsing

```cpp
import fastjson;

int main() {
    // Parse JSON string
    auto result = fastjson::parse(R"({"name": "Alice", "age": 30})");
    
    if (result.has_value()) {
        auto& doc = *result;
        std::cout << doc["name"].as_string() << std::endl;  // Alice
        std::cout << doc["age"].as_number() << std::endl;   // 30
    }
    
    return 0;
}
```

### LINQ Queries (C++)

```cpp
import json_linq;
using namespace fastjson::linq;

int main() {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    auto result = from(data)
        .where([](auto& x) { return x % 2 == 0; })  // Even numbers
        .select([](auto& x) { return x * x; })      // Square them
        .to_vector();
    // Result: {4, 16, 36, 64, 100}
    
    return 0;
}
```

### Parallel Processing (C++)

```cpp
import fastjson_parallel;

int main() {
    // Set thread count
    fastjson_parallel::set_num_threads(8);
    
    // Parse large file with parallelization
    auto result = fastjson_parallel::parse_file("large.json");
    
    return 0;
}
```

---

## Performance Tips

### 1. Use Parallel Queries for Large Data
```python
# For datasets > 10,000 items, use parallel_query
results = fastjson.parallel_query(large_list).filter(pred).to_list()
```

### 2. Avoid Repeated Parsing
```python
# Bad: Parse multiple times
for item in items:
    data = fastjson.parse(json_str)  # Repeated!

# Good: Parse once, query multiple times
data = fastjson.parse(json_str)
for item in items:
    query = fastjson.query(data)
```

### 3. Use Batch Parsing for Multiple Strings
```python
# Process multiple JSON strings efficiently
results = fastjson.parse_batch(json_strings)
```

### 4. Check SIMD Support
```python
# Verify SIMD acceleration is enabled
info = fastjson.get_simd_info()
if not info.has_avx2:
    print("Warning: AVX2 not available, using scalar fallback")
```

---

## Troubleshooting

### Import Error: libomp.so not found
```bash
# Add OpenMP library to path
export LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH
```

### Build Error: pybind11 not found
```bash
# Install pybind11 in your environment
pip install pybind11
```

### Module Import Error
```bash
# Ensure you're using the correct Python
which python3
# Should point to your virtual environment
```

---

## Next Steps

- [Full API Reference](docs/API_REFERENCE.md)
- [Architecture Overview](docs/ARCHITECTURE.md)
- [Contributing Guide](docs/CONTRIBUTING.md)
- [Benchmarks](benchmarks/README.md)

---

**Author**: Olumuyiwa Oluwasanmi  
**License**: BSD-4-Clause
