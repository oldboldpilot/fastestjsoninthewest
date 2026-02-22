# FastestJSONInTheWest - Claude AI Instructions

> **SYNC POLICY**: This file MUST be kept in sync with `ai/copilot-instructions.md`, `ai/gemini.md`, and `.github/copilot-instructions.md`. Last sync: February 22, 2026.

## Project Context
High-performance C++23 JSON parser with **8x multi-register SIMD** acceleration (256 bytes/iteration), ondemand lazy parsing, arena allocation, zero-copy string_view, 128-bit precision support, LINQ-style queries, and Mustache templating.

## v3.0 Performance Refactor (February 2026)
- **SIMD GMF Fix**: All SIMD implementations moved to Global Module Fragment — fixes Clang 21 BMI serialization segfault
- **8x AVX2 registers** (256 bytes/iteration), **4x AVX-512** (256B/iter), **4x SSE4.2/SSE2** (64B/iter)
- **Runtime CPUID dispatch**: AVX-512 → AVX2 → SSE4.2 → SSE2 → scalar
- **Ondemand parsing**: Two-stage lazy parsing (structural SIMD tape + lazy value materialization)
- **Arena allocation**: `parse_arena()` with `pmr::monotonic_buffer_resource` for reduced heap pressure
- **Zero-copy strings**: `json_string_data` with `variant<string_view, string>` COW semantics
- **AVX2 scanner fix**: Corrected string-state tracking within 32-byte chunks

### Key Benchmark Results (v3.0)
| Benchmark | FastJSON | simdjson | Notes |
|-----------|----------|----------|-------|
| Field access | **52 ns** | 66 ns | FastJSON 1.3x faster |
| Array iteration (10K) | **198 us** | 444 us | FastJSON 2.2x faster |
| Ondemand single field (1MB) | **3.0 ms** | — | 7.8x vs full parse |
| Serialization (medium) | **571 MB/s** | — | FastJSON exclusive |

## v2.1 Features
- **Mustache Templating**: Logic-less templates with `fastjson::mustache::render` (C++23 module).
- **Nanobind Python Bindings**: Next-gen GIL-free bindings using `uv` and `nanobind`.

## Build System
- **Compiler**: Clang 21+ (C++23 modules)
- **Build Tool**: Ninja, CMake 3.28+
- **Standard**: C++23 with modules
- **OpenMP**: Enabled for parallel processing

## Modules (C++23)
- **Core**: `import fastjson;` (modules/fastjson.cppm)
- **LINQ**: `import json_linq;` (modules/json_linq.cppm)
- **Templating**: `import json_template;` (modules/json_template.cppm)

## Core API (v3.0)

### Parsing Modes
```cpp
import fastjson;

// Full DOM parse (eager)
auto result = fastjson::parse(json_string);

// Arena-allocated parse (reduced heap pressure)
auto doc = fastjson::parse_arena(std::move(json_string));

// Ondemand/lazy parse (structural tape only, no DOM)
auto doc = fastjson::parse_ondemand(std::move(json_string));
auto root = doc->root();
auto obj = root.get_object().value();
double price = obj["price"].value().get_double().value();
```

### Key Types
- `json_value` — DOM node (eager parse result)
- `json_document` — owns input + arena + root value (arena parse)
- `ondemand_document` — owns input + structural tape (lazy parse)
- `ondemand_value` — lazy accessor for type/string/double/int64/bool/null
- `ondemand_object` — lazy object with `find_field()` / `operator[]`
- `ondemand_array` — lazy array with `at()` / `count_elements()`
- `json_string_data` — COW string with `variant<string_view, string>`

### Error Handling
All parse functions return `json_result<T>` which is `std::expected<T, json_error>`.

### Python Bindings (Nanobind)
```python
import fastjson_py as json

data = json.loads('{"key": "value"}')   # Parse string
text = json.dumps(data)                  # Serialize
data = json.load("file.json")           # Parse file
json.dump(data, "out.json", indent=2)   # Write file
info = json.simd_info()                 # SIMD capabilities
```

## File Locations
- **Core module**: `modules/fastjson.cppm`
- **SIMD structural index**: `modules/fastjson_simd_index.h`
- **Template module**: `modules/json_template.cppm`
- **Nanobind bindings**: `python/fastjson_bindings.cpp`
- **Benchmark**: `benchmarks/fastjson_vs_simdjson.cpp`
- **Tests**: `tests/test_fastjson_nanobind.py` (85 tests)

## Architecture Notes
- SIMD implementations live in `namespace fastjson::detail` inside the Global Module Fragment (before `export module fastjson;`)
- Thin inline wrappers in the module purview delegate to `detail::` functions
- The structural tape (`fastjson_simd_index.h`) indexes `{ } [ ] , : "` plus primitive starts (`t`, `f`, `n`, digits)
- The AVX2 scanner tracks `in_string` state within 32-byte chunks to avoid false structural indexing

## Guidelines
1. **Single Author**: All commits must credit **Olumuyiwa Oluwasanmi**.
2. **C++ Standard**: Use `import std;` where possible.
3. **Error Handling**: Use `std::expected` / `std::unexpected` (railway-oriented).
4. **Docs**: Keep `README.md` and `documents/API_REFERENCE.md` updated.
