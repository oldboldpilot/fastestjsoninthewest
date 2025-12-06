# Binary Compatibility Guide

## Overview

This guide explains the binary compatibility requirements for fastjson's dependencies: **std.pcm** (C++23 modules) vs **Intel TBB** (shared library).

**TL;DR**:
- ✅ **std.pcm**: MUST use exact same compilation flags
- ❌ **TBB**: Does NOT need flag matching (stable ABI)

---

## Understanding Binary Compatibility

### What is Binary Compatibility?

**Binary compatibility** means that separately compiled code can work together without recompilation. It requires:
- Compatible **Application Binary Interface (ABI)**
- Matching **data structure layouts**
- Consistent **calling conventions**

---

## std.pcm (C++23 Precompiled Modules)

### What is std.pcm?

`std.pcm` is a **precompiled module** containing the C++23 standard library interface.

### Why Flag Matching is REQUIRED

❌ **Precompiled modules are ABI-sensitive**:

```cpp
// std.pcm built with:  -std=c++23 -stdlib=libc++ -O3 -march=native

// Your code compiled with different flags:
import std;  // ❌ ERROR: Binary incompatibility!

// Error message:
// fatal error: module file 'std.pcm' was compiled with different settings
```

### How Modules Store ABI Information

Precompiled modules embed:
1. **Compiler flags** - `-std`, `-stdlib`, optimization level
2. **Target architecture** - `-march=native`, `-mavx512f`
3. **Type layouts** - struct padding, alignment
4. **Template instantiations** - Specific to compilation flags

### Ensuring Compatibility

✅ **Always use the exported flags**:

```cmake
# In parent project CMakeLists.txt
add_dependencies(myapp build_fastjson_std_modules)
target_compile_options(myapp PRIVATE
    ${FASTJSON_STD_PCM_FLAGS}  # MUST match exactly
    -fprebuilt-module-path=${FASTJSON_STD_PCM_DIR}
)
```

### What Happens Without Flag Matching

```
Compilation Failure:
  → PCM file rejected
  → Error: "compiled with different settings"
  → Build fails

Runtime Issues (if forced):
  → Undefined behavior
  → Memory corruption
  → Segmentation faults
```

---

## Intel TBB (Shared Library)

### What is TBB?

Intel TBB is a **regular shared library** (`.so` on Linux, `.dylib` on macOS, `.dll` on Windows) providing parallel algorithms.

### Why Flag Matching is NOT Required

✅ **Shared libraries have stable ABIs**:

```cpp
// TBB built with its own internal flags
// Your code compiled with different flags:
#include <tbb/tbb.h>  // ✅ Works fine!

std::transform(std::execution::par, ...);  // ✅ No problems
```

### How Shared Libraries Work

1. **Compilation**: TBB is compiled once with its own flags
2. **Linking**: Your code links against the compiled `.so` file
3. **Runtime**: Dynamic linker loads TBB at runtime
4. **ABI**: Stable interface, doesn't depend on your compiler flags

### ABI Stability Guarantees

TBB provides:
- ✅ **Stable function signatures** - Same across compilation flags
- ✅ **Opaque data structures** - Internal layout hidden
- ✅ **Standard C++ types** - `std::vector`, `std::function`, etc.
- ✅ **Versioned symbols** - ABI breakage causes linker errors, not runtime crashes

### Using TBB in Parent Projects

```cmake
# No flag matching needed
if(FASTJSON_HAS_PARALLEL_STL)
    target_link_libraries(myapp PRIVATE ${FASTJSON_TBB_TARGET})
    # Just link and use - no special flags required
endif()
```

---

## Comparison Table

| Aspect | std.pcm (Precompiled Module) | TBB (Shared Library) |
|--------|------------------------------|----------------------|
| **Type** | Precompiled C++23 module | Shared library (.so/.dll) |
| **ABI Sensitivity** | ❌ Very high | ✅ Stable ABI |
| **Flag Matching** | ✅ Required | ❌ Not required |
| **Compilation** | Per-consumer (with flags) | Once (independent) |
| **Binary Compatibility** | Exact flags match | Version-based only |
| **Error Detection** | Compile-time | Link-time (versioned) |
| **Usage Complexity** | High (must export flags) | Low (just link) |

---

## Technical Deep Dive

### Why Modules are ABI-Sensitive

C++23 modules embed:

```cpp
// std.cppm compiled to std.pcm
export module std;

export namespace std {
    // These type layouts depend on compiler flags:
    template<typename T>
    class vector {
        T* data;           // Alignment depends on -march
        size_t size;       // Layout depends on optimization
        size_t capacity;   // Padding depends on -std version
    };
}
```

If your code is compiled with different flags:
- Different alignment → data corruption
- Different padding → wrong offsets
- Different optimization → inconsistent codegen

### Why Shared Libraries are ABI-Stable

TBB uses opaque types and stable interfaces:

```cpp
// tbb.h (public API)
namespace tbb {
    class task_scheduler_init {
        // Opaque pointer - internal details hidden
        void* impl;  // Always sizeof(void*) regardless of flags
    public:
        task_scheduler_init(int threads);  // Stable signature
    };
}

// tbb.so (compiled implementation)
// Internal details can change, but API stays stable
```

Your code only sees:
- Function signatures (stable)
- Opaque pointers (always same size)
- Standard C++ types (ABI guaranteed by compiler)

---

## Best Practices

### For std.pcm Users

1. **Always use exported flags**:
   ```cmake
   target_compile_options(myapp PRIVATE ${FASTJSON_STD_PCM_FLAGS})
   ```

2. **Verify flag consistency**:
   ```bash
   # Check what flags were used
   echo "${FASTJSON_STD_PCM_FLAGS}"
   ```

3. **Rebuild if flags change**:
   ```bash
   # Clean and rebuild if fastjson flags changed
   rm -rf build && mkdir build && cd build && cmake ..
   ```

### For TBB Users

1. **Just link the target**:
   ```cmake
   target_link_libraries(myapp PRIVATE ${FASTJSON_TBB_TARGET})
   ```

2. **Optionally use version info**:
   ```cmake
   message(STATUS "Using TBB ${FASTJSON_TBB_VERSION}")
   ```

3. **No special flags needed**:
   - Compile your code with any flags you want
   - TBB's stable ABI handles the rest

---

## Common Questions

### Q: Can I compile my code with -O2 if std.pcm was built with -O3?

❌ **No**. You must use the exact same optimization level:
```cmake
target_compile_options(myapp PRIVATE ${FASTJSON_STD_PCM_FLAGS})
```

### Q: Can I compile my code with -O2 if TBB was built with -O3?

✅ **Yes**. TBB's ABI is stable regardless of optimization:
```cmake
target_compile_options(myapp PRIVATE -O2)  # Different from TBB's -O3
target_link_libraries(myapp PRIVATE ${FASTJSON_TBB_TARGET})
```

### Q: Why doesn't TBB need flag matching?

Because TBB:
- Uses opaque types (implementation details hidden)
- Provides stable C interfaces
- Guarantees ABI stability across versions
- Doesn't expose template instantiations

### Q: Can I use a different TBB version?

⚠️ **Use the same major version**:
- TBB 2021.x → Compatible with all 2021.x versions
- TBB 2020.x → May have ABI differences
- Best practice: Use `FASTJSON_TBB_TARGET` to ensure consistency

### Q: What about mixing std.pcm from different sources?

❌ **Never mix std.pcm from different builds**:
```cmake
# BAD: Using different std.pcm files
add_subdirectory(fastjson)      # Builds std.pcm with -O3
add_subdirectory(other_lib)     # Builds std.pcm with -O2
# ❌ Undefined behavior if both are used
```

✅ **Always use one source**:
```cmake
# GOOD: Single std.pcm source
add_subdirectory(fastjson)
add_dependencies(myapp build_fastjson_std_modules)
add_dependencies(other_target build_fastjson_std_modules)
```

---

## Debugging Binary Compatibility Issues

### std.pcm Issues

**Symptoms**:
```
error: module file 'std.pcm' was compiled with different settings
```

**Solution**:
```bash
# 1. Check flags used to build std.pcm
cmake -LA | grep FASTJSON_STD_PCM_FLAGS

# 2. Ensure your target uses those exact flags
target_compile_options(myapp PRIVATE ${FASTJSON_STD_PCM_FLAGS})

# 3. Clean and rebuild
rm -rf build && mkdir build && cd build && cmake .. && ninja
```

### TBB Issues

**Symptoms**:
```
undefined reference to `tbb::task_scheduler_init::initialize(int)'
```

**Solution**:
```cmake
# 1. Ensure TBB is linked
target_link_libraries(myapp PRIVATE ${FASTJSON_TBB_TARGET})

# 2. Verify TBB target exists
if(NOT TARGET ${FASTJSON_TBB_TARGET})
    message(FATAL_ERROR "TBB target not found. Enable ENABLE_PARALLEL_STL=ON")
endif()
```

---

## Summary

### std.pcm (Precompiled Modules)
- ❌ **Requires exact flag matching**
- ✅ Use `${FASTJSON_STD_PCM_FLAGS}` always
- ⚠️ Rebuild if fastjson flags change
- 🎯 Modern but complex

### TBB (Shared Library)
- ✅ **No flag matching needed**
- ✅ Just link `${FASTJSON_TBB_TARGET}`
- ✅ Compile with any flags you want
- 🎯 Traditional and simple

**Key Takeaway**: Modules are new and ABI-sensitive. Shared libraries are traditional and ABI-stable. Understand the difference to avoid compatibility issues.

---

## References

- [C++23 Modules](https://en.cppreference.com/w/cpp/language/modules)
- [Intel TBB Documentation](https://www.intel.com/content/www/us/en/docs/onetbb/get-started-guide/2021-11/overview.html)
- [ABI Stability](https://isocpp.org/wiki/faq/cpp14-language#abi-stability)
- [CMake Module Support](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html)
