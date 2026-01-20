# FastestJSONInTheWest - Gemini AI Instructions

> **SYNC POLICY**: This file MUST be kept in sync with `ai/CLAUDE.md`, `ai/copilot-instructions.md`, and `.github/copilot-instructions.md`. Last sync: January 19, 2026.

## Project Context
High-performance C++23 JSON parser with **4x multi-register SIMD** acceleration (128 bytes/iteration), 128-bit precision support, LINQ-style queries, and Mustache templating.

## v2.0 Performance Update
- Default parser now uses **4x AVX2 multi-register SIMD** (2-6x faster)
- String-heavy JSON: 4.7-5.9x faster
- Large arrays: 2.8-4.3x faster
- Zero API changes - existing code gets faster automatically

## v2.1 Features
- **Mustache Templating**: Logic-less templates with `fastjson::mustache::render` (C++23 module).
- **Nanobind Python Bindings**: Next-gen GIL-free bindings using `uv` and `nanobind`.

## v2.2 Features (January 2026)
- **SIMD Waterfall API**: Python bindings expose full SIMD level selection with automatic waterfall
- **SIMD Capabilities Detection**: `get_simd_capabilities()` returns detected CPU features
- **Levels**: AVX-512 → AMX → AVX2 → AVX → SSE4 → SSE2 → SCALAR
- **Parallelism Control**: `to_python(threads=N, simd_level=...)` for conversion

## Build System
- **Compiler**: `/opt/clang-21/bin/clang++` (Clang 21.1.5)
- **Build Tool**: Ninja (`/usr/local/bin/ninja`), UV (Python)
- **Standard**: C++23 with modules
- **OpenMP**: Enabled for parallel processing

## Modules (C++23)
- **Core**: `import fastjson;` (modules/fastjson.cppm)
- **LINQ**: `import json_linq;` (modules/json_linq.cppm)
- **Templating**: `import json_template;` (modules/json_template.cppm)

### Templating usage
```cpp
import json_template;
std::string output = fastjson::mustache::render("Hello {{name}}", json_data);
```

## Key Implementation Details

### 128-bit Precision Numbers
- **Adaptive Precision**: Auto-upgrades from `double` to `__float128` or `__int128`.
- **Python Support**: Bindings convert 128-bit ints to Python arbitrary precision ints via string.

### Python Bindings (Nanobind + UV)
- **Directory**: `python_bindings/`
- **Manager**: `uv` (scikit-build-core + cmake)
- **Library**: Nanobind (replaces pybind11)
- **Configuration**: `pyproject.toml`
- **GIL**: released for `parse`, `parse_file`.

```bash
cd python_bindings
uv sync
CXX=/opt/clang-21/bin/clang++ CC=/opt/clang-21/bin/clang uv pip install -e .
uv run pytest
```

```python
import fastjson

# GIL-free parsing
data = fastjson.parse(large_json_str)

# SIMD capabilities detection
caps = fastjson.get_simd_capabilities()
print(caps.avx2, caps.avx512f, caps.optimal_path)

# SIMD level selection with waterfall
py_obj = data.to_python(threads=4, simd_level=fastjson.SIMDLevel.AVX2.value)

# LINQ
res = fastjson.query(data).where(lambda x: x['val'].as_int() > 5).to_list()

# Mustache
html = fastjson.render_template("<h1>{{title}}</h1>", data)
```

### SIMD Levels (Python)
```python
fastjson.SIMDLevel.AUTO     # Automatic detection (default)
fastjson.SIMDLevel.AVX512   # 512-bit, 8x registers, 512 bytes/iter
fastjson.SIMDLevel.AMX      # Advanced Matrix Extensions
fastjson.SIMDLevel.AVX2     # 256-bit, 4x registers, 128 bytes/iter
fastjson.SIMDLevel.AVX      # 256-bit legacy
fastjson.SIMDLevel.SSE4     # 128-bit SSE4.2
fastjson.SIMDLevel.SSE2     # 128-bit SSE2
fastjson.SIMDLevel.SCALAR   # No SIMD fallback
```

## File Locations
- **Core**: `modules/fastjson.cppm`
- **Template**: `modules/json_template.cppm`
- **Bindings**: `python_bindings/src/fastjson_bindings.cpp`
- **Tests**: `tests/` (C++), `python_bindings/tests/` (Python)

## Guidelines
1. **Single Author**: All commits must credit **Olumuyiwa Oluwasanmi**.
2. **C++ Standard**: Use `import std;` where possible.
3. **Docs**: Keep `README.md` and `documents/API_REFERENCE.md` updated.
