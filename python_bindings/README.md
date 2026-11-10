# FastestJSONInTheWest Python Bindings (nanobind + uv)

High-performance Python bindings for the FastestJSONInTheWest C++23 JSON parser, powered by [nanobind](https://github.com/wjakob/nanobind) for efficient C++/Python interoperability and built with modern C++23 standards.

## 🚀 Speed & Features

- **Blazing Fast**: Uses fused-tiled SIMD (AVX-512, AMX, AVX2, SSE4.2, NEON) via `fastjson::turbo::dom_parser` on x86-64 with automatic fallback on AArch64.
- **GIL-Free**: Heavy parsing tasks release the Global Interpreter Lock (GIL), enabling true multi-core throughput across Python threads.
- **Fluent LINQ API**: Lazy single-pass transducer querying directly from Python with zero intermediate allocations.
- **PostgreSQL jsonb Zero-Copy Wire Adapter**: Direct parsing and serialization of PostgreSQL binary wire format (0x01 header + UTF-8 JSON).
- **C++23 Native**: Built with `import std` modules and modern C++ features using the official Clang 23+ compiler.
- **Thread-safe**: Designed for concurrent execution with configurable worker thread pools.

## 📦 Requirements

- **Python**: 3.11+ (tested on Python 3.13 and Python 3.14)
- **Compiler**: Clang 23+ (official C++23 modules compiler, auto-detected by `python_bindings/CMakeLists.txt`; e.g., `clang++-23` or `/home/linuxbrew/.linuxbrew/bin/clang++`)
- **Tooling**: `uv` (recommended package manager), CMake 3.28+

## 🛠️ Quick Start

### Installation

We recommend using **uv** for a fast, reliable build environment.

1. **Install uv**:
   See official installation guide for Windows/macOS/Linux.

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

   # For development (editable install with no build isolation)
   uv pip install -e . --no-build-isolation
   ```

4. **Shell Build Scripts**:
   You can also build and test using the provided helper scripts:
   - `python_bindings/BUILD.sh` — compiles the extension with automatic Clang 23+ detection.
   - `python_bindings/BUILD_AND_TEST.sh` — compiles and runs the complete test suite.

5. **Sanitizer Build Support**:
   Enable runtime sanitizer instrumentation by passing CMake flags:
   - AddressSanitizer + UndefinedBehaviorSanitizer: `-DENABLE_ASAN=ON`
   - MemorySanitizer: `-DENABLE_MSAN=ON`
   - ThreadSanitizer: `-DENABLE_TSAN=ON`

### Verification

Run the test suite out-of-the-box (75 tests passing, 0 skipped):

```bash
cd python_bindings
uv run pytest tests/ -v
```

All 75 tests pass cleanly under both standard Release builds and AddressSanitizer (`-DENABLE_ASAN=ON`).

## 📚 API Reference & Usage Guide

### 1. High-Speed SIMD TurboDOM Native Python Parser

The fused-tiled SIMD native Python parser (`fastjson::turbo::dom_parser` on x86-64 with automatic fallback on AArch64) parses directly into native Python `dict`, `list`, `int`, `float`, `str`, `bool`, and `None` while releasing the GIL during SIMD parsing. It enforces strict RFC 8259 validation (raising `ValueError` on malformed syntax, unescaped control characters, invalid surrogates, and IEEE-754 overflow) and provides exact 64-bit and 128-bit `int` fidelity without precision loss.

- `loads()`: Parse JSON directly into native Python objects. Accepts `str`, `bytes`, `bytearray`, or `memoryview`.
- `parse_turbo()`: Alias for `loads()`.
- `loads_file()`: Read and parse a JSON file directly into native Python objects with released GIL.
- `TurboParser`: Reusable zero-allocation SIMD TurboDOM parser class accepting `max_depth` parameter. Reuses internal buffers across parses for maximum throughput, exposing property `max_depth` and methods `loads()` and `loads_file()`.

```python
import fastjson

# Parse string, bytes, bytearray, or memoryview
doc = fastjson.loads('{"project": "FastestJSONInTheWest", "active": true, "count": 42}')
print(doc["project"])  # FastestJSONInTheWest

# Reusable parser instance with custom depth limit
parser = fastjson.TurboParser(max_depth=64)
result = parser.loads(b'{"temperatures": [21.5, 22.0, 19.8]}')

# Read directly from a file
config = fastjson.loads_file("config.json")
```

### 2. High-Speed C++ Serializer

High-throughput C++ serialization for Python objects and `JSONValue` instances:

- `dumps()`: Serialize Python objects or `JSONValue` to a JSON string. Parameters: obj, indent=-1, ensure_ascii=False, allow_nan=False.
- `dumps_bytes()`: Serialize to UTF-8 JSON bytes. Parameters: obj, indent=-1, ensure_ascii=False, allow_nan=False.
- `stringify()`: Serialize `JSONValue` or Python objects to compact JSON text (GIL-free for `JSONValue`).
- `prettify()`: Serialize `JSONValue` or Python objects to indented JSON text (default indent=2).

```python
payload = {"status": "ok", "codes": [200, 201], "unicode": "\U0001F680"}

# Compact serialization
compact_str = fastjson.dumps(payload)
compact_bytes = fastjson.dumps_bytes(payload)

# Indented formatting
pretty_str = fastjson.prettify(payload, indent=2)
```

### 3. Copy-on-Write DOM Parser & JSONValue

The Copy-on-Write DOM parser preserves structural integrity with O(1) shallow copies and thread safety.

- `parse()`: Parse JSON from `str` or `bytes` into a `JSONValue` (GIL-free).
- `parse_parallel()`: Parse large JSON strings using parallel multi-threading (num_threads parameter, GIL-free).
- `parse_file()`: Parse a JSON file directly into a `JSONValue` (GIL-free).

`JSONValue` methods:
- Constructors and factories: `object()`, `array()`, `from_python()`
- Type inspection: `is_null()`, `is_bool()`, `is_number()`, `is_int_128()`, `is_uint_128()`, `is_number_128()`, `is_string()`, `is_array()`, `is_object()`, `empty()`
- Accessors: `as_bool()`, `as_float()`, `as_int()`, `as_string()`
- Container protocol: `contains()`, `get()`, `keys()`, `values()`, `items()`, `append()`, `copy()`, `clear()`, `__len__`, `__getitem__`, `__setitem__`, `__delitem__`
- Python conversion: `to_python()`, `to_python_parallel()`
- Serialization: `stringify()`, `prettify()`, `to_json()`

```python
# Parse into COW JSONValue
val = fastjson.parse('{"sensor": "temp-01", "readings": [18.2, 19.5, 21.0]}')
assert val.is_object()
assert val["sensor"].as_string() == "temp-01"

# Safe access with get() and contains()
val.contains("missing")  # False
val.get("missing", 0)    # 0

# Convert to native Python dict
py_dict = val.to_python()
```

### 4. PostgreSQL Binary jsonb Wire-Format Adapters

Zero-copy binary adapters for PostgreSQL jsonb network wire format (version header byte 0x01 followed by UTF-8 encoded JSON text):

- `PG_JSONB_WIRE_VERSION`: Constant integer 1 (0x01).
- `parse_pg_jsonb()`: Parse wire bytes into a `JSONValue` with strict RFC 8259 header and payload validation.
- `loads_pg_jsonb()`: Parse wire bytes directly into native Python objects via TurboDOM.
- `dumps_pg_jsonb()`: Serialize Python objects or `JSONValue` into PostgreSQL jsonb wire bytes.

```python
# Create wire payload (0x01 header + JSON)
wire_bytes = fastjson.dumps_pg_jsonb({"user_id": 42, "role": "admin"})

# Parse directly into Python objects or JSONValue
data = fastjson.loads_pg_jsonb(wire_bytes)
jv = fastjson.parse_pg_jsonb(wire_bytes)
```

### 5. Zero-Copy C++ Typed Vectors

Bound C++ opaque vectors (`bind_vector`) eliminating list boxing overhead for numerical arrays:

- `Int64Vector`: Vector of signed 64-bit integers. Methods: `append()`, `to_list()`, `to_json()`, `sum()`.
- `UInt64Vector`: Vector of unsigned 64-bit integers. Methods: `append()`, `to_list()`, `to_json()`, `sum()`.
- `Float64Vector`: Vector of 64-bit floating-point numbers. Methods: `append()`, `to_list()`, `to_json()`, `sum()`.
- `StringVector`: Vector of UTF-8 strings. Methods: `append()`, `to_list()`, `to_json()`.

```python
v = fastjson.Int64Vector()
for i in range(1000):
    v.append(i)

# Sum directly in C++ without Python iteration overhead
total = v.sum()

# Serialize directly to JSON array string
json_array = v.to_json()

# Convert to Python list
py_list = v.to_list()
```

### 6. Lazy Single-Pass Transducer LINQ (JSONQuery)

Query JSON sequences with fluent, push-transducer pipelines via `query()` on a `JSONValue` array or Python list:

- Introspection: `is_lazy()`
- Transforms and filtering: `where()`, `filter()`, `select()`, `map()`, `take()`, `skip()`, `take_while()`, `skip_while()`, `reverse()`, `order_by()`, `order_by_descending()`, `distinct()`, `distinct_by()`, `prefix_sum()`, `scan()`, `concat()`, `union_with()`, `zip()`
- Aggregations and materialization: `first()`, `last()`, `single()`, `find()`, `find_index()`, `any()`, `all()`, `count()`, `empty()`, `reduce()`, `aggregate()`, `sum()`, `min()`, `max()`, `average()`, `group_by()`, `for_each()`, `to_values()`, `to_list()`

```python
items = fastjson.parse('[{"id": 1, "score": 85}, {"id": 2, "score": 92}, {"id": 3, "score": 78}]')

q = (fastjson.query(items)
     .where(lambda x: x["score"].as_int() >= 80)
     .order_by_descending(lambda x: x["score"].as_int())
     .select(lambda x: x["id"].as_int()))

top_ids = q.to_list()  # [2, 1]
```

### 7. Mustache Templating

Render text templates using parsed data at C++ speed with `render_template()`:
- Accepts either a `JSONValue` DOM object or a native Python `dict`.
- Supports variables, sections, inverted sections, and array iteration.

```python
template = "Hello {{name}}! Active: {{#active}}yes{{/active}}{{^active}}no{{/active}}"
data = {"name": "Alice", "active": True}

output = fastjson.render_template(template, data)
print(output)  # Hello Alice! Active: yes
```

### 8. Runtime SIMD & Concurrency Control

Inspect hardware vector capabilities and configure execution paths dynamically:

- `SIMDLevel`: Enum for SIMD tiers (`AUTO`, `AVX512`, `AMX`, `AVX2`, `AVX`, `SSE4`, `SSE2`, `SCALAR`).
- `SIMDCapabilities`: Struct with CPU vector flags (`avx512f`, `avx2`, `sse42`, `neon`), `bytes_per_iteration`, and `register_count`.
- `get_simd_capabilities()`: Returns hardware detection results.
- `is_simd_level_available()`: Test if a specific level is supported by hardware.
- `get_effective_simd_level()`: Determine the active level after fallback waterfall.
- `simd_level_name()`: Get the descriptive string name for a SIMD level.
- `SIMD_WATERFALL_ORDER`: Ordered list of fallback priority stages.
- `set_num_threads()` and `get_num_threads()`: Configure worker thread pool size.

### 9. GPU Detection & Graceful Fallback

GPU acceleration and fallback dispatch interface:

- `GPUBackend`: Enum (`NONE`, `CUDA`, `CUDA_GRAPH`, `CUBLAS`, `CUTILE`, `TRITON`, `ROCM`, `SYCL`, `AUTO_DETECT`).
- `GPUInfo`, `GPUParseConfig`, `GPUParseResult`.
- `detect_gpu_backend()`: Detect available hardware acceleration backends.
- `is_gpu_available()`: Check if GPU acceleration is operational.
- `get_gpu_info()`: Retrieve device properties and memory limits.
- `parse_on_gpu()`: Parse JSON data on GPU with fallback to CPU.
- `gpu_matrix_multiply()`: GPU matrix multiplication interface.

## 🏗️ Architecture

- **Backend**: Nanobind for modern C++23 and Python interoperability.
- **Build System**: scikit-build-core + CMake 3.28+.
- **Compiler**: Clang 23+ with C++23 `import std` modules support.

## ⚠️ Troubleshooting

- **Compiler Version**: Ensure Clang 23+ is used (e.g. `/home/linuxbrew/.linuxbrew/bin/clang++` or `clang++-23`). C++23 modules require Clang 23+ with libc++.
- **Environment Variables**: To explicitly set the compiler, configure:
  ```bash
  export CXX=/home/linuxbrew/.linuxbrew/bin/clang++
  export CC=/home/linuxbrew/.linuxbrew/bin/clang
  ```
- **CMake Version**: CMake 3.28+ is required for C++23 module scanning.
