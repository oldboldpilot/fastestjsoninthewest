# FastestJSONInTheWest - GitHub Copilot Instructions

> **SYNC POLICY**: This file MUST be kept in sync with `.github/copilot-instructions.md`. When updating either file, copy the changes to both. Last sync: January 12, 2026.

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
- **GIL**: released for `parse`, `parse_file`, `parse_parallel`.

```bash
cd python_bindings
uv sync
uv pip install -e .
uv run pytest
```

```python
import fastjson
# GIL-free parsing
data = fastjson.parse(large_json_str)

# LINQ
res = fastjson.query(data).where(lambda x: x['val'].as_int() > 5).to_list()

# Mustache
html = fastjson.render_template("<h1>{{title}}</h1>", data)
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
