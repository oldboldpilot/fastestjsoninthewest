# FastestJSONInTheWest Python Bindings (nanobind + uv)

High-performance Python bindings for the FastestJSONInTheWest C++23 JSON parser, powered by [nanobind](https://github.com/wjakob/nanobind) for efficient C++/Python interoperability and built with modern C++23 standards.

## 🚀 Speed & Features

- **Blazing Fast**: Uses SIMD (AVX2/AVX-512) and efficient binding logic (nanobind is ~2-4x faster than pybind11 for calls).
- **GIL-Free**: Heavy parsing tasks release the Global Interpreter Lock (GIL), allowing true parallelism in Python threads.
- **Fluent API**: LINQ-style querying directly from Python.
- **C++23 Native**: Built with `import std` modules and modern C++ features.
- **Thread-safe**: Designed for concurrent usage.

## 📦 Requirements

- **Python**: 3.11+
- **Compiler**: Clang 21+ (Required for C++23 modules)
- **Tooling**: `uv` (recommended package manager), CMake 3.28+

## 🛠️ Quick Start

### Installation

We recommend using **uv** for a fast, reliable build environment.

1. **Install uv**:
   See [official installation guide](https://docs.astral.sh/uv/getting-started/installation/) for Windows/macOS/Linux.
   
   Unix/macOS helper:
   ```bash
   curl -LsSf https://astral.sh/uv/install.sh | sh
   ```

2. **Sync Dependencies**:
   ```bash
   cd python_bindings
   uv sync
   ```

3. **Build and Install**:
   ```bash
   # Build the extension in the virtual environment
   uv pip install .
   
   # For development (editable install)
   uv pip install -e . --no-build-isolation
   ```

### Verification

```bash
# Run tests
uv run pytest tests/ -v
```

## 📚 Usage Guide

### Basic Parsing

```python
import fastjson

# Parse a JSON string (releases GIL)
data = fastjson.parse('{"name": "Speedy", "value": 42}')

print(data)  
# Output: <JSONValue: {"name":"Speedy","value":42}>

# Convert to Python dict/list recursively
py_data = data.to_python()
print(py_data['name'])  # Speedy
```

### Reading Files

```python
# Efficient file reading and parsing in C++
data = fastjson.parse_file("large_dataset.json")
```

### Parallel Parsing (Thread-Safe)

```python
import threading

def worker():
    # This call releases the GIL, so other threads run concurrently!
    # Ideal for parsing multiple large files/strings in parallel
    fastjson.parse_parallel(very_large_json_string)

threads = [threading.Thread(target=worker) for _ in range(4)]
for t in threads: t.start()
for t in threads: t.join()
```

### Fluent LINQ API

Query JSON data without converting to Python objects first (faster!).

```python
# Assume 'data' is a JSON array of objects
query = fastjson.query(data)

# Filter and transform using Python lambdas (or C++ ops if exposed)
results = (query
    .where(lambda x: x['age'].as_int() > 30)
    .select(lambda x: x['name'])
    .to_list())
```

### Mustache Templating

Render text templates using the parsed JSON data at C++ speed.

```python
template = """
Hello {{name}}!
{{#items}}
  - {{.}}
{{/items}}
"""

data = fastjson.parse('{"name": "World", "items": ["Apple", "Banana"]}')
output = fastjson.render_template(template, data)
print(output)
```

### 128-bit Numeric Support

Fastjson automatically upgrades to 128-bit precision when needed.

```python
# Parsing a huge integer
huge_json = '{"id": 123456789012345678901234567890123456789}'
val = fastjson.parse(huge_json)

# .to_python() converts it to a standard Python arbitrarily large int
print(val["id"].to_python()) 
# Output: 123456789012345678901234567890123456789
```

### Typed Vectors (Templates)

We expose optimized C++ vectors for numeric types to avoid list overhead.

```python
# Create a typed vector of int64 (zero-copy if passed to C++)
nums = fastjson.Int64Vector()
nums.append(100)
nums.append(200)

# Pass to C++ functions expecting std::vector<int64_t>
# (This is just an example, specific API usage depends on exposed methods)
```

## 🏗️ Architecture

- **Backend**: Nanobind (C++ bindings)
- **Build System**: scikit-build-core + CMake
- **Std Lib**: Uses C++23 `import std` where available.

## ⚠️ Troubleshooting

- **Compiler Errors**: Ensure you are using Clang 21+. Set `export CXX=/opt/clang-21/bin/clang++` and `export CC=/opt/clang-21/bin/clang`.
- **Module Errors**: If C++ modules fail to scan, ensure your CMake version is >= 3.28.
