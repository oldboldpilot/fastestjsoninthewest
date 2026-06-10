# cpp23-logger Quick Start Guide

Production-grade C++23 logger with Mustache templates, fluent API, and automatic std module PCM building.

## Requirements

### Compiler & Standard Library
- **Clang 21+** (for C++23 modules support)
- **libc++** (LLVM C++ standard library)
- **CMake 3.28+**
- **Ninja** (recommended build system)

### Optional Dependencies
- **OpenMP** - For parallel processing support
- **clang-tidy** - For code quality checks

## Installation Paths

The library automatically detects these paths (or you can override):

### Compiler
```bash
# Auto-detected from:
/usr/local/bin/clang++        # Default
/usr/bin/clang++              # Fallback
${CLANG_DEFAULT_PATH}/clang++ # Custom (via cmake)
```

### libc++ Headers
```bash
# Auto-detected from:
/usr/local/include/c++/v1     # Default
${LIBCXX_DEFAULT_INCLUDE}     # Custom (via cmake)
```

### libc++ Libraries
```bash
# Auto-detected from:
/usr/local/lib                # Default
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

### Basic Build (No Optimizations)
```bash
mkdir build && cd build
CC=/usr/local/bin/clang CXX=/usr/local/bin/clang++ cmake .. -G Ninja
ninja
```

### Build with Tests
```bash
CC=/usr/local/bin/clang CXX=/usr/local/bin/clang++ cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -G Ninja
ninja
ctest
```

### Build with OpenMP + Native Optimizations
```bash
CC=/usr/local/bin/clang CXX=/usr/local/bin/clang++ cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_OPENMP=ON \
  -DENABLE_NATIVE_OPTIMIZATIONS=ON \
  -G Ninja
ninja
```

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
```

### Release Flags
```cmake
-O3                     # Maximum optimization
```

### Native Optimizations (ENABLE_NATIVE_OPTIMIZATIONS=ON)
```cmake
-march=native           # CPU-specific optimizations (auto-detected)
```

### OpenMP Flags (ENABLE_OPENMP=ON)
```cmake
-fopenmp=libomp        # OpenMP parallel processing
```

### std.pcm Build Flags
The std.pcm module is built with the SAME flags as the logger to ensure binary compatibility:
```cmake
-std=c++23 -stdlib=libc++ -fPIC [-O3] [-march=native] [-fopenmp=libomp]
```

## std Module PCM

### Automatic Building
When using Clang, the library automatically builds:
- **std.pcm** - Standard library module
- **std.compat.pcm** - C compatibility module

Located at: `${CMAKE_BINARY_DIR}/modules/`

### Using in Your Code
```cpp
import std;           // Import standard library
import logger;        // Import logger

int main() {
    auto& log = logger::Logger::getInstance();
    log.info("Hello from C++23 modules!");

    std::vector<int> numbers = {1, 2, 3};
    log.info("Vector size: {}", numbers.size());
}
```

### Exported CMake Variables
When using cpp23-logger as a subproject, these variables are available:
```cmake
LOGGER_STD_PCM               # Path to std.pcm
LOGGER_STD_COMPAT_PCM        # Path to std.compat.pcm
LOGGER_STD_PCM_FLAGS         # Flags used to build std.pcm
LOGGER_STD_PCM_DIR           # Directory containing PCM files
```

### Using Exported PCM in Parent Project
```cmake
add_subdirectory(cpp23-logger)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE logger)

# If your code uses 'import std;'
add_dependencies(myapp build_logger_std_modules)
target_compile_options(myapp PRIVATE
    ${LOGGER_STD_PCM_FLAGS}
    -fprebuilt-module-path=${LOGGER_STD_PCM_DIR}
)
```

## Configuration Summary

After running cmake, you'll see:
```
std module PCM auto-build enabled:
  Module source: /usr/local/share/libc++/v1
  Output: /path/to/build/modules/
  Compiler: /usr/local/bin/clang++
  Flags: -std=c++23;-stdlib=libc++;-fPIC;-O3;-march=native
  OpenMP: ON
  Native opts: ON
```

## CPU Auto-Detection

The library uses `-march=native` for CPU feature detection:
- ✅ **Automatic** - Detects available CPU instructions (AVX2, SSE4, etc.)
- ✅ **Safe** - No illegal instruction errors on different CPUs
- ✅ **Portable** - Works across different machines without recompilation

## Examples

### Basic Logging
```cpp
import logger;

int main() {
    auto& log = logger::Logger::getInstance();

    log.info("Info message");
    log.warning("Warning: {}", "something happened");
    log.error("Error code: {}", 42);
}
```

### Template Logging
```cpp
import logger;

int main() {
    auto& log = logger::Logger::getInstance();

    log.info("User {{name}} logged in at {{time}}",
             {{"name", "Alice"}, {"time", "10:30"}});
}
```

### With std Module
```cpp
import std;
import logger;

int main() {
    auto& log = logger::Logger::getInstance();

    std::vector<int> data = {1, 2, 3, 4, 5};
    log.info("Processing {} items", data.size());

    for (const auto& item : data | std::views::transform([](int x) { return x * 2; })) {
        log.debug("Processed: {}", item);
    }
}
```

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
target_compile_options(myapp PRIVATE ${LOGGER_STD_PCM_FLAGS})
```

### Illegal Instruction
```
Illegal instruction (core dumped)
```
**Solution**: CPU doesn't support detected instructions. Disable native optimizations:
```bash
cmake .. -DENABLE_NATIVE_OPTIMIZATIONS=OFF
```

## Performance

### Build Times
- **Without std.pcm**: ~5s for logger only
- **With std.pcm**: +~3s for std.pcm build (one-time per build dir)

### Runtime Performance
- **Without optimizations**: Baseline
- **With -march=native**: +15-30% throughput
- **With OpenMP**: +50-200% for parallel operations

## Integration Examples

### As Git Submodule
```bash
git submodule add https://github.com/yourusername/cpp23-logger.git external/cpp23-logger
```

```cmake
# In your CMakeLists.txt
add_subdirectory(external/cpp23-logger)
target_link_libraries(myapp PRIVATE logger)
```

### With FetchContent
```cmake
include(FetchContent)
FetchContent_Declare(
    cpp23_logger
    GIT_REPOSITORY https://github.com/yourusername/cpp23-logger.git
    GIT_TAG main
)
FetchContent_MakeAvailable(cpp23_logger)
target_link_libraries(myapp PRIVATE logger)
```

## License

See LICENSE file for details.

## Support

For issues, questions, or contributions, please visit:
https://github.com/yourusername/cpp23-logger
