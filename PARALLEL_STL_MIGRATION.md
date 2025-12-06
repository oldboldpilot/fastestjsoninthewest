# Parallel STL Migration with Intel TBB

## Overview

Migrated fastjson from OpenMP to C++23 Parallel STL with Intel TBB for better integration with Clang + libc++.

## Why This Change?

### Problem with Clang + libc++
- **Clang with libc++ does NOT natively support Parallel STL**
- `std::execution::par` and `std::execution::par_unseq` require external implementation
- OpenMP is not the standard way to implement Parallel STL

### Solution: Intel TBB (Threading Building Blocks)
- **Industry standard** Parallel STL backend for Clang
- **Modern oneTBB** (v2021+) supports C++23
- **Better integration** with standard library algorithms
- **MIT licensed** - no restrictive dependencies

## What Changed

### 1. CMake Configuration

Added FetchContent to automatically download and build oneTBB:

```cmake
option(ENABLE_PARALLEL_STL "Enable Parallel STL with Intel TBB (recommended for Clang)" ON)
option(ENABLE_OPENMP "Enable OpenMP parallel processing support (legacy)" OFF)

if(ENABLE_PARALLEL_STL)
    # Fetch Intel oneTBB
    FetchContent_Declare(
        TBB
        GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
        GIT_TAG        v2022.3.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(TBB)
endif()
```

### 2. Library Linking

fastjson_parallel now links with TBB:

```cmake
if(ENABLE_PARALLEL_STL)
    target_link_libraries(fastjson_parallel PUBLIC TBB::tbb)
    target_compile_definitions(fastjson_parallel PUBLIC FASTJSON_USE_PARALLEL_STL)
elseif(ENABLE_OPENMP)
    # Legacy OpenMP still available
    target_link_libraries(fastjson_parallel PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(fastjson_parallel PUBLIC FASTJSON_USE_OPENMP)
endif()
```

## Usage

### Building with Parallel STL (Default)

```bash
mkdir build && cd build
cmake .. -G Ninja
ninja
```

This automatically:
1. Downloads Intel TBB via FetchContent
2. Builds TBB (one-time, cached)
3. Links fastjson_parallel with TBB
4. Enables `std::execution::par` and `std::execution::par_unseq`

### Using Parallel Algorithms

```cpp
import std;
import fastjson_parallel;

// Parse multiple JSON documents in parallel
std::vector<std::string> json_documents = load_many_files();

// Use Parallel STL with execution policies
std::vector<json_doc> results;
std::transform(std::execution::par_unseq,  // Parallel + Vectorized
    json_documents.begin(),
    json_documents.end(),
    std::back_inserter(results),
    [](const auto& json_str) {
        return fastjson::parse(json_str);
    }
);

// Parallel for_each with TBB backend
std::for_each(std::execution::par,
    json_array.begin(),
    json_array.end(),
    [](auto& element) {
        process_element(element);
    }
);
```

## Execution Policies

With TBB, fastjson now supports all standard execution policies:

### 1. `std::execution::seq` - Sequential
```cpp
std::transform(std::execution::seq, ...);  // Single-threaded
```

### 2. `std::execution::par` - Parallel
```cpp
std::transform(std::execution::par, ...);  // Multi-threaded with TBB
```

### 3. `std::execution::par_unseq` - Parallel + Vectorized
```cpp
std::transform(std::execution::par_unseq, ...);  // Multi-threaded + SIMD
```

## Performance Comparison

### Before (OpenMP)
- ✅ Good parallel performance
- ❌ Non-standard implementation
- ❌ Manual `#pragma omp parallel for`
- ❌ Doesn't work with standard algorithms

### After (Parallel STL + TBB)
- ✅ Standard C++23 Parallel STL
- ✅ Works with all `<algorithm>` functions
- ✅ Composable with ranges
- ✅ Better with libc++
- ✅ Modern, maintained by Intel

## Configuration Options

### Enable Parallel STL (Recommended)
```bash
cmake .. -DENABLE_PARALLEL_STL=ON -DENABLE_OPENMP=ON -G Ninja
```

**Why both flags?**
- `ENABLE_PARALLEL_STL=ON` - fastjson uses Parallel STL with TBB
- `ENABLE_OPENMP=ON` - Required for sensen and other dependencies that use OpenMP

### Use Legacy OpenMP for fastjson
```bash
cmake .. -DENABLE_PARALLEL_STL=OFF -DENABLE_OPENMP=ON -G Ninja
```

This keeps fastjson using OpenMP pragmas instead of Parallel STL (for fine-grained NUMA control).

### Disable All Parallelism (Not Recommended)
```bash
cmake .. -DENABLE_PARALLEL_STL=OFF -DENABLE_OPENMP=OFF -G Ninja
```

**Warning**: This will fail if sensen or other dependencies require OpenMP.

## TBB Version

We use **oneTBB v2022.3.0** (latest stable):
- Full C++23 support
- Modern threading model
- NUMA-aware task scheduler
- Lock-free concurrent containers
- Scalable memory allocator

## Migration for Existing Code

### Old OpenMP Code
```cpp
#pragma omp parallel for
for (size_t i = 0; i < elements.size(); ++i) {
    process(elements[i]);
}
```

### New Parallel STL Code
```cpp
std::for_each(std::execution::par,
    elements.begin(),
    elements.end(),
    [](auto& elem) { process(elem); }
);
```

## Benefits

1. **Standard C++23** - Uses official parallel algorithms
2. **Better Composability** - Works with ranges and views
3. **Cleaner Code** - No pragmas, just execution policies
4. **Portable** - Works across Clang, GCC (future)
5. **Modern** - Integrates with C++23 modules
6. **SIMD + Parallel** - `par_unseq` allows both

## Dependencies

### fastjson Dependencies
- **Parallel STL Mode** (default):
  - Intel TBB (fetched automatically via FetchContent)
  - Standard backend for Parallel STL on Clang
  - No manual installation required

- **OpenMP Mode** (legacy):
  - OpenMP (system-provided)
  - Used for custom NUMA thread binding

### Other Library Dependencies
- **sensen library**: Still uses OpenMP (required)
- **cpp23-logger**: No parallelism dependencies

**Important**: Even when using Parallel STL for fastjson, you still need OpenMP enabled (`-DENABLE_OPENMP=ON`) because the sensen dependency requires it. The two parallelization approaches coexist:
- fastjson uses Parallel STL (standard algorithms)
- sensen uses OpenMP (neural network operations)

## Compatibility

- ✅ **Clang 21+** with libc++ (recommended)
- ✅ **GCC 13+** with libstdc++ (future support)
- ✅ **CMake 3.28+**
- ✅ **C++23** required

## Next Steps

Once this is stable, we'll:
1. Update all parallel algorithms to use execution policies
2. Remove OpenMP as default
3. Add benchmarks comparing TBB vs OpenMP
4. Document performance characteristics

## References

- [Intel oneTBB GitHub](https://github.com/oneapi-src/oneTBB)
- [C++23 Parallel Algorithms](https://en.cppreference.com/w/cpp/algorithm/execution_policy_tag)
- [PSTL Documentation](https://www.intel.com/content/www/us/en/docs/oneapi/programming-guide/2024-0/parallel-stl.html)
