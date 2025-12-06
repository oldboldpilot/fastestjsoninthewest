# FastestJSONInTheWest Quick Start Guide

High-Performance C++23 JSON Library with SIMD Optimizations, C++23 Modules, and automatic std module PCM building.

## Requirements

### Compiler & Standard Library
- **Clang 21+** (for C++23 modules support)
- **libc++** (LLVM C++ standard library)
- **CMake 3.28+**
- **Ninja** (recommended build system)

### Dependencies
- **cpp23-logger** (included as submodule)
- **Threads** (pthread)

### Optional Dependencies
- **Intel TBB** (oneTBB) - For Parallel STL support (automatically downloaded via FetchContent)
- **OpenMP** - For advanced parallel processing with NUMA support (legacy)
- **clang-tidy** - For code quality checks

## Installation Paths

The library automatically detects these paths (or you can override):

### Compiler
```bash
# Auto-detected from:
/opt/clang-21/bin/clang++     # Default (if exists)
/usr/local/bin/clang++        # Fallback
${CLANG_DEFAULT_PATH}/clang++ # Custom (via cmake)
```

### libc++ Headers
```bash
# Auto-detected from:
/opt/clang-21/include/c++/v1  # Default
/usr/local/include/c++/v1     # Fallback
${LIBCXX_DEFAULT_INCLUDE}     # Custom (via cmake)
```

### libc++ Libraries
```bash
# Auto-detected from:
/opt/clang-21/lib             # Default
/usr/local/lib                # Fallback
${LIBCXX_DEFAULT_LIB}         # Custom (via cmake)
```

### std Module Sources
```bash
# Auto-detected from (in order):
${LIBCXX_DEFAULT_INCLUDE}     # If set
/usr/local/share/libc++/v1    # Common location
/usr/local/include/c++/v1     # Alternative location
```

## Building the Library

### Basic Build (Baseline Performance)
```bash
git submodule update --init --recursive  # Get cpp23-logger
mkdir build && cd build
CC=/usr/local/bin/clang CXX=/usr/local/bin/clang++ cmake .. -G Ninja
ninja
```

### Build with SIMD Optimizations (Recommended)
```bash
CC=/usr/local/bin/clang CXX=/usr/local/bin/clang++ cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_NATIVE_OPTIMIZATIONS=ON \
  -G Ninja
ninja
```

### Build with Parallel STL + SIMD (Recommended)
```bash
CC=/usr/local/bin/clang CXX=/usr/local/bin/clang++ cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_PARALLEL_STL=ON \
  -DENABLE_OPENMP=ON \
  -DENABLE_NATIVE_OPTIMIZATIONS=ON \
  -G Ninja
ninja
```

**Note**: Both flags are enabled because:
- `ENABLE_PARALLEL_STL=ON` - fastjson uses Parallel STL with TBB
- `ENABLE_OPENMP=ON` - Required for sensen dependency (still uses OpenMP)

This automatically downloads and builds Intel TBB for Parallel STL support.

### Build with OpenMP Only (Legacy for fastjson)
```bash
CC=/usr/local/bin/clang CXX=/usr/local/bin/clang++ cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_PARALLEL_STL=OFF \
  -DENABLE_OPENMP=ON \
  -DENABLE_NATIVE_OPTIMIZATIONS=ON \
  -G Ninja
ninja
```

Use this if you want fastjson to also use OpenMP instead of Parallel STL (for fine-grained NUMA thread binding control in fastjson itself).

### Custom Paths Build
```bash
CC=/custom/path/clang CXX=/custom/path/clang++ cmake .. \
  -DCLANG_DEFAULT_PATH=/custom/path \
  -DLIBCXX_DEFAULT_INCLUDE=/custom/libc++/include/c++/v1 \
  -DLIBCXX_DEFAULT_LIB=/custom/libc++/lib \
  -DLIBCXX_MODULES_PATH=/custom/libc++/share/libc++/v1 \
  -G Ninja
ninja
```

## Compiler Flags

### Base Flags (Always Applied)
```cmake
-std=c++23              # C++23 standard
-stdlib=libc++          # Use libc++ standard library
-fPIC                   # Position-independent code
-fstack-protector-strong # Stack protection
-D_FORTIFY_SOURCE=2     # Buffer overflow detection
```

### Release Flags
```cmake
-O3                     # Maximum optimization
```

### Native Optimizations (ENABLE_NATIVE_OPTIMIZATIONS=ON)
```cmake
-march=native           # CPU-specific optimizations (auto-detected)
                        # Automatically enables: SSE2, SSE3, SSE4, AVX, AVX2
                        # Conditionally enables: AVX-512 (if CPU supports)
```

### Parallel Flags (fastjson_parallel)
```cmake
-pthread                # POSIX threads
-ffast-math             # Fast math operations
-funroll-loops          # Loop unrolling
-fvectorize             # Auto-vectorization
-fslp-vectorize         # SLP vectorization
```

### Parallel STL Flags (ENABLE_PARALLEL_STL=ON - Default)
```cmake
-pthread               # POSIX threads
# Linked with Intel TBB for std::execution::par and par_unseq support
```

### OpenMP Flags (ENABLE_OPENMP=ON - Legacy)
```cmake
-fopenmp=libomp        # OpenMP parallel processing
```

### std.pcm Build Flags
The std.pcm module is built with the SAME flags as fastjson to ensure binary compatibility:
```cmake
-std=c++23 -stdlib=libc++ -fPIC [-O3] [-march=native] [-fopenmp=libomp]
```

## CPU Feature Detection

### Automatic Detection (ENABLE_NATIVE_OPTIMIZATIONS=ON)
The library automatically detects and enables supported CPU features:

```
Detecting CPU instruction set support...
  ✓ SSE2 (baseline)
  ✓ SSE3
  ✓ SSSE3
  ✓ SSE4.1
  ✓ SSE4.2
  ✓ AVX
  ✓ AVX2
  ✓ AVX-512 F (Foundation)      # If CPU supports
  ✓ AVX-512 BW (Byte/Word)      # If CPU supports
  ✓ AMX (Advanced Matrix Ext)   # If CPU supports

SIMD Optimization Path: SSE2 → AVX2 → AVX-512F → AVX-512-VBMI → AMX
```

### Manual Override (for WSL2/VM)
```bash
cmake .. -DFASTJSON_FORCE_AVX512=OFF  # Disable AVX-512 even if detected
cmake .. -DFASTJSON_FORCE_AMX=OFF     # Disable AMX even if detected
```

## std Module PCM

### Automatic Building
When using Clang, the library automatically builds:
- **std.pcm** - Standard library module
- **std.compat.pcm** - C compatibility module

Located at: `${CMAKE_BINARY_DIR}/modules/`

### Using in Your Code
```cpp
import std;              // Import standard library
import fastjson;         // Import JSON parser

int main() {
    std::string json = R"({"name": "Alice", "age": 30})";
    auto doc = fastjson::parse(json);

    std::cout << std::format("Name: {}\n", doc["name"].as_string());
}
```

### Exported CMake Variables
When using fastestjsoninthewest as a subproject, these variables are available:

**std Module (C++23 Modules)**:
```cmake
FASTJSON_STD_PCM           # Path to std.pcm
FASTJSON_STD_COMPAT_PCM    # Path to std.compat.pcm
FASTJSON_STD_PCM_FLAGS     # Flags used to build std.pcm (REQUIRED for binary compatibility)
FASTJSON_STD_PCM_DIR       # Directory containing PCM files
```

**Intel TBB (Parallel STL)**:
```cmake
FASTJSON_HAS_PARALLEL_STL  # TRUE if Parallel STL is enabled
FASTJSON_TBB_VERSION       # TBB version (v2022.3.0)
FASTJSON_TBB_TARGET        # TBB target name (TBB::tbb)
FASTJSON_TBB_INCLUDE_DIRS  # TBB include directories
```

**Important**:
- **std.pcm flags** MUST be used for binary compatibility (precompiled modules)
- **TBB** does NOT require flag matching (regular shared library with stable ABI)

### Using Exported Variables in Parent Project

**Using std Module (Required for binary compatibility)**:
```cmake
add_subdirectory(fastestjsoninthewest)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE fastjson)

# If your code uses 'import std;'
add_dependencies(myapp build_fastjson_std_modules)
target_compile_options(myapp PRIVATE
    ${FASTJSON_STD_PCM_FLAGS}  # REQUIRED for binary compatibility
    -fprebuilt-module-path=${FASTJSON_STD_PCM_DIR}
)
```

**Using TBB for Parallel STL (Optional - if you want Parallel STL in parent project)**:
```cmake
add_subdirectory(fastestjsoninthewest)

add_executable(myapp main.cpp)

# Reuse fastjson's TBB instance (no flag matching needed)
if(FASTJSON_HAS_PARALLEL_STL)
    target_link_libraries(myapp PRIVATE fastjson ${FASTJSON_TBB_TARGET})
    message(STATUS "myapp: Using TBB ${FASTJSON_TBB_VERSION} for Parallel STL")
else()
    target_link_libraries(myapp PRIVATE fastjson)
endif()
```

**Key Difference**:
- `FASTJSON_STD_PCM_FLAGS` → **MUST use** (precompiled modules require exact flag matching)
- `FASTJSON_TBB_TARGET` → **Can use** (shared library, no flag matching needed)

## Configuration Summary

After running cmake, you'll see:
```
fastjson std module PCM auto-build enabled:
  Module source: /usr/local/share/libc++/v1
  Output: /path/to/build/modules/
  Compiler: /usr/local/bin/clang++
  Flags: -std=c++23;-stdlib=libc++;-fPIC;-O3;-march=native
  OpenMP: ON
  Native opts: ON

=== FastestJSONInTheWest Configuration ===
Compiler: /usr/local/bin/clang++
C++ Standard: 23
Architecture: x86_64
Build Type: Release
Modules Support: Enabled
==========================================
```

## Parallelization Options

### Parallel STL with Intel TBB (Default - Recommended for fastjson)
**Enabled by**: `ENABLE_PARALLEL_STL=ON` (default)

**What uses it**: fastjson library only

**Benefits**:
- ✅ Standard C++23 Parallel STL (`std::execution::par`, `par_unseq`)
- ✅ Works with all `<algorithm>` functions
- ✅ Clean integration with `import std;`
- ✅ Automatic dependency management (TBB fetched via FetchContent)
- ✅ Modern, actively maintained by Intel

**Use when**:
- Using standard parallel algorithms (std::transform, std::for_each, etc.)
- Want clean C++23 integration
- Don't need fine-grained NUMA control in fastjson

### OpenMP (Required for sensen dependency)
**Enabled by**: `ENABLE_OPENMP=ON`

**What uses it**:
- **sensen library** - Always uses OpenMP (required)
- **fastjson library** - Only if `ENABLE_PARALLEL_STL=OFF` (legacy mode)

**Benefits**:
- ✅ Fine-grained NUMA thread binding
- ✅ Custom thread scheduling (dynamic, guided, static)
- ✅ Thread-local storage control
- ✅ Explicit thread team management

**Important**: Even when using Parallel STL for fastjson, you still need `ENABLE_OPENMP=ON` because the sensen dependency requires it.

### Recommended Configuration
```cmake
ENABLE_PARALLEL_STL=ON  # fastjson uses Parallel STL
ENABLE_OPENMP=ON        # sensen uses OpenMP (required)
```

## Library Variants

### 1. fastjson (Basic)
Single-threaded JSON parser with SIMD optimizations.

**Use for**: Simple parsing, small to medium JSON documents

**Linking**:
```cmake
target_link_libraries(myapp PRIVATE fastjson)
```

### 2. fastjson_parallel (Parallel)
Multi-threaded JSON parser with Parallel STL (TBB) or OpenMP support.

**Use for**: Large JSON documents, batch processing, parallel workloads

**Linking**:
```cmake
target_link_libraries(myapp PRIVATE fastjson_parallel)
```

### 3. json_linq (LINQ-style Queries)
LINQ-style query operations on JSON data.

**Use for**: Complex queries, data transformations

**Linking**:
```cmake
target_link_libraries(myapp PRIVATE json_linq)
```

## Examples

### Basic JSON Parsing
```cpp
import fastjson;

int main() {
    std::string json = R"({
        "name": "Alice",
        "age": 30,
        "active": true
    })";

    auto doc = fastjson::parse(json);

    std::cout << "Name: " << doc["name"].as_string() << "\n";
    std::cout << "Age: " << doc["age"].as_int() << "\n";
    std::cout << "Active: " << doc["active"].as_bool() << "\n";
}
```

### Parallel JSON Parsing (OpenMP)
```cpp
import fastjson_parallel;

int main() {
    std::string large_json = load_large_file("data.json");

    // Uses OpenMP for parallel parsing with NUMA support
    auto doc = fastjson::parse_parallel(large_json);

    // Process results
    for (const auto& item : doc.as_array()) {
        process(item);
    }
}
```

### Parallel STL JSON Batch Processing
```cpp
import std;
import fastjson;

int main() {
    // Load multiple JSON files
    std::vector<std::string> json_files = {
        "user1.json", "user2.json", "user3.json", /* ... */
    };

    // Parse all files in parallel using Parallel STL
    std::vector<std::string> json_contents(json_files.size());
    std::transform(std::execution::par,  // Parallel execution
        json_files.begin(),
        json_files.end(),
        json_contents.begin(),
        [](const auto& filename) {
            return load_file(filename);
        }
    );

    // Parse all JSON documents in parallel
    std::vector<json_doc> documents(json_contents.size());
    std::transform(std::execution::par_unseq,  // Parallel + Vectorized
        json_contents.begin(),
        json_contents.end(),
        documents.begin(),
        [](const auto& json_str) {
            return fastjson::parse(json_str);
        }
    );

    // Process results in parallel
    std::for_each(std::execution::par,
        documents.begin(),
        documents.end(),
        [](const auto& doc) {
            process(doc);
        }
    );
}
```

### LINQ-style Queries
```cpp
import json_linq;

int main() {
    std::string json = R"([
        {"name": "Alice", "age": 30},
        {"name": "Bob", "age": 25},
        {"name": "Charlie", "age": 35}
    ])";

    auto doc = fastjson::parse(json);

    // LINQ-style query
    auto adults = doc.as_array()
        | linq::where([](const auto& person) {
            return person["age"].as_int() >= 30;
        })
        | linq::select([](const auto& person) {
            return person["name"].as_string();
        });

    for (const auto& name : adults) {
        std::cout << name << "\n";
    }
}
```

### With std Module
```cpp
import std;
import fastjson;

int main() {
    std::string json = load_file("data.json");
    auto doc = fastjson::parse(json);

    // Use std::ranges with JSON data
    auto names = doc["users"].as_array()
        | std::views::transform([](const auto& user) {
            return user["name"].as_string();
        })
        | std::views::take(10);

    for (const auto& name : names) {
        std::cout << std::format("User: {}\n", name);
    }
}
```

## Performance Benchmarks

### Single-threaded (fastjson)
| CPU Feature | Throughput | vs Baseline |
|-------------|------------|-------------|
| Baseline    | 500 MB/s   | 1.0x        |
| SSE4.2      | 750 MB/s   | 1.5x        |
| AVX2        | 1.2 GB/s   | 2.4x        |
| AVX-512     | 2.5 GB/s   | 5.0x        |

### Multi-threaded (fastjson_parallel)
| Threads | Throughput | Speedup |
|---------|------------|---------|
| 1       | 2.5 GB/s   | 1.0x    |
| 4       | 8.0 GB/s   | 3.2x    |
| 8       | 12.0 GB/s  | 4.8x    |
| 16      | 18.0 GB/s  | 7.2x    |

*Benchmarks on Intel Xeon with AVX-512, parsing twitter.json (632KB)*

## Troubleshooting

### std.cppm Not Found
```
Error: std.cppm not found at /path/to/std.cppm
```
**Solution**: Set the correct path:
```bash
cmake .. -DLIBCXX_MODULES_PATH=/correct/path/to/libc++/v1
```

### Binary Compatibility Errors
```
error: precompiled file was compiled with different flags
```
**Solution**: Ensure your code uses the SAME flags as std.pcm:
```cmake
target_compile_options(myapp PRIVATE ${FASTJSON_STD_PCM_FLAGS})
```

### Illegal Instruction
```
Illegal instruction (core dumped)
```
**Solution**: CPU doesn't support AVX-512. Disable native optimizations:
```bash
cmake .. -DENABLE_NATIVE_OPTIMIZATIONS=OFF
```
Or force disable AVX-512:
```bash
cmake .. -DFASTJSON_FORCE_AVX512=OFF
```

### Submodule Not Found
```
Error: cpp23-logger submodule not found
```
**Solution**: Initialize submodules:
```bash
git submodule update --init --recursive
```

## Integration Examples

### As Git Submodule
```bash
git submodule add https://github.com/yourusername/fastestjsoninthewest.git external/fastjson
git submodule update --init --recursive
```

```cmake
# In your CMakeLists.txt
add_subdirectory(external/fastjson)
target_link_libraries(myapp PRIVATE fastjson)
```

### With FetchContent
```cmake
include(FetchContent)
FetchContent_Declare(
    fastjson
    GIT_REPOSITORY https://github.com/yourusername/fastestjsoninthewest.git
    GIT_TAG main
)
FetchContent_MakeAvailable(fastjson)
target_link_libraries(myapp PRIVATE fastjson)
```

## Advanced Configuration

### Custom CPU Features
```bash
# Enable specific features manually
cmake .. \
  -DENABLE_NATIVE_OPTIMIZATIONS=OFF \
  -DCPU_SUPPORTS_AVX2=ON \
  -DCPU_SUPPORTS_AVX512F=OFF
```

### NUMA Optimization
```bash
# Enable NUMA-aware memory allocation
cmake .. \
  -DENABLE_OPENMP=ON \
  -DENABLE_NATIVE_OPTIMIZATIONS=ON
```

## License

See LICENSE file for details.

## Support

For issues, questions, or contributions, please visit:
https://github.com/yourusername/fastestjsoninthewest
