# FastJSON Examples

This directory contains example programs demonstrating various features of fastjson.

## Examples

### 1. parallel_stl_example.cpp
**Parallel STL with Intel TBB for High-Performance JSON Processing**

Demonstrates using C++23 Parallel STL (std::execution policies) with fastjson for parallel JSON processing.

**Features**:
- Parallel batch JSON parsing with `std::execution::par` and `par_unseq`
- Parallel data extraction using `std::transform`
- Parallel aggregation with `std::transform_reduce`
- Performance comparison between sequential, parallel, and parallel+SIMD execution

**Build Requirements**:
- ENABLE_PARALLEL_STL=ON (default in CMake)
- Intel TBB (automatically fetched)
- Clang 21+ with libc++

**Build**:
```bash
cd /path/to/fastestjsoninthewest
mkdir build && cd build

# Build with Parallel STL (TBB)
CC=/usr/local/bin/clang CXX=/usr/local/bin/clang++ cmake .. \
  -DENABLE_PARALLEL_STL=ON \
  -DENABLE_NATIVE_OPTIMIZATIONS=ON \
  -G Ninja
ninja

# Compile the example
clang++ -std=c++23 -stdlib=libc++ \
  -fprebuilt-module-path=./modules \
  -march=native \
  ../examples/parallel_stl_example.cpp \
  -L./lib -lfastjson -ltbb \
  -o parallel_stl_example

# Run
./parallel_stl_example
```

**Expected Output**:
```
=== Example 1: Parallel Batch JSON Parsing ===
Parsed 8 documents in 245 µs
Results:
  ID: 1, Name: Alice, Score: 95
  ...

=== Example 5: Execution Policy Comparison ===
Performance comparison (1000 documents):
  Sequential (seq):        12450 µs (1.00x)
  Parallel (par):          3210 µs (3.88x speedup)
  Parallel+SIMD (par_unseq): 2150 µs (5.79x speedup)

✅ All examples completed successfully!
```

---

### 2. fluent_api_examples.cpp
**Fluent API and Method Chaining**

Demonstrates fluent API patterns for JSON construction and manipulation.

**Build**:
```bash
clang++ -std=c++23 -stdlib=libc++ \
  -fprebuilt-module-path=./modules \
  ../examples/fluent_api_examples.cpp \
  -L./lib -lfastjson \
  -o fluent_api_example
```

---

### 3. unicode_examples.cpp
**Unicode and UTF-8 Handling**

Shows proper UTF-8 encoding/decoding, emoji support, and Unicode normalization.

**Build**:
```bash
clang++ -std=c++23 -stdlib=libc++ \
  -fprebuilt-module-path=./modules \
  ../examples/unicode_examples.cpp \
  -L./lib -lfastjson \
  -o unicode_example
```

---

### 4. type_mappings_demo.cpp
**Type System and Conversions**

Demonstrates JSON type conversions, null handling, and type safety features.

**Build**:
```bash
clang++ -std=c++23 -stdlib=libc++ \
  -fprebuilt-module-path=./modules \
  ../examples/type_mappings_demo.cpp \
  -L./lib -lfastjson \
  -o type_mappings_demo
```

---

## Quick Start

### Build All Examples at Once

Add this to the main CMakeLists.txt to build examples automatically:

```cmake
# In fastestjsoninthewest/CMakeLists.txt
option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

Then create `examples/CMakeLists.txt`:

```cmake
# Build parallel_stl_example (requires ENABLE_PARALLEL_STL=ON)
if(ENABLE_PARALLEL_STL AND TARGET TBB::tbb)
    add_executable(parallel_stl_example parallel_stl_example.cpp)
    target_link_libraries(parallel_stl_example PRIVATE fastjson TBB::tbb)

    # Use same std module as fastjson
    if(TARGET std_module)
        add_dependencies(parallel_stl_example std_module)
        target_compile_options(parallel_stl_example PRIVATE
            ${FASTJSON_STD_PCM_FLAGS}
            -fprebuilt-module-path=${FASTJSON_STD_PCM_DIR}
        )
    endif()

    message(STATUS "Added example: parallel_stl_example")
endif()

# Build other examples
add_executable(fluent_api_example fluent_api_examples.cpp)
target_link_libraries(fluent_api_example PRIVATE fastjson)

add_executable(unicode_example unicode_examples.cpp)
target_link_libraries(unicode_example PRIVATE fastjson)

add_executable(type_mappings_demo type_mappings_demo.cpp)
target_link_libraries(type_mappings_demo PRIVATE fastjson)
```

Then build:
```bash
mkdir build && cd build
cmake .. -DBUILD_EXAMPLES=ON -DENABLE_PARALLEL_STL=ON -G Ninja
ninja
./parallel_stl_example
./fluent_api_example
./unicode_example
./type_mappings_demo
```

## Performance Tips

### Parallel STL Best Practices

1. **Use `par_unseq` for data-parallel operations**:
   ```cpp
   std::transform(std::execution::par_unseq,  // Parallel + SIMD
       data.begin(), data.end(), result.begin(),
       [](auto& x) { return process(x); });
   ```

2. **Use `par` when operations can't be vectorized**:
   ```cpp
   std::for_each(std::execution::par,  // Just parallel
       data.begin(), data.end(),
       [&](auto& x) {
           mutex.lock();  // Can't vectorize
           shared_state.update(x);
           mutex.unlock();
       });
   ```

3. **Use `seq` for small datasets**:
   ```cpp
   if (data.size() < 100) {
       std::transform(std::execution::seq, ...);  // Sequential
   }
   ```

4. **Minimize allocations in parallel sections**:
   ```cpp
   // Good: Pre-allocate
   std::vector<Result> results(data.size());
   std::transform(std::execution::par,
       data.begin(), data.end(), results.begin(), process);

   // Bad: Dynamic allocation
   std::vector<Result> results;
   std::for_each(std::execution::par,
       data.begin(), data.end(),
       [&](auto& x) { results.push_back(process(x)); });  // Race condition!
   ```

## Troubleshooting

### TBB Not Found
```
Error: TBB::tbb target not found
```
**Solution**: Enable Parallel STL in CMake:
```bash
cmake .. -DENABLE_PARALLEL_STL=ON
```

### Module Not Found
```
error: module 'std' not found
```
**Solution**: Ensure std module is built first:
```bash
ninja std_module
```

### Illegal Instruction
```
Illegal instruction (core dumped)
```
**Solution**: Your CPU doesn't support AVX-512. Rebuild without native optimizations:
```bash
cmake .. -DENABLE_NATIVE_OPTIMIZATIONS=OFF
```

## Additional Resources

- [PARALLEL_STL_MIGRATION.md](../PARALLEL_STL_MIGRATION.md) - Detailed migration guide from OpenMP to Parallel STL
- [QUICKSTART.md](../QUICKSTART.md) - Complete build and configuration guide
- [C++23 Parallel Algorithms Reference](https://en.cppreference.com/w/cpp/algorithm/execution_policy_tag)
- [Intel TBB Documentation](https://www.intel.com/content/www/us/en/docs/onetbb/get-started-guide/2021-11/overview.html)
