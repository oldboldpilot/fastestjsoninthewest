# cpp23-logger

A production-grade, **thread-safe** C++23 logging library with Mustache-style templates and fluent API.

## Features

- **Thread-Safe**: Lock-free log level checking with atomic operations
- **Modern C++23**: Uses `import std;` for faster compilation
- **Fluent API**: Method chaining for easy configuration
- **Mustache Templates**: Named parameter formatting for structured logging
- **Multiple Log Levels**: TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL
- **ANSI Colors**: Color-coded console output (optional)
- **UTF-8 Support**: Full international character support
- **Timezone-Aware**: ISO 8601 timestamps with UTC offset
- **Zero Dependencies**: Self-contained library (requires only libc++)

## Thread-Safety Guarantees

### Lock-Free Log Level Checking
The logger uses `std::atomic<LogLevel>` for thread-safe log level filtering **without** mutex contention:

```cpp
// Thread-safe level check happens BEFORE acquiring mutex
if (level < current_level.load(std::memory_order_relaxed)) {
    return;  // Fast path: no lock needed
}

// Only acquire lock for actual logging
std::lock_guard<std::mutex> lock(mutex_);
// ... write to file/console
```

### Benefits
- **Lock-free read path**: `getLevel()` doesn't acquire mutex
- **Lock-free write path**: `setLevel()` uses atomic store
- **Reduced contention**: Mutex only held during actual I/O
- **Safe concurrency**: Multiple threads can log simultaneously

## Quick Start

### Basic Usage

```cpp
#include <logger/logger.cppm>  // C++23 module
// OR
import logger;  // If your project uses modules

using logger::Logger;
using logger::LogLevel;

int main() {
    auto& log = Logger::getInstance();

    // Initialize with file and log level
    log.initialize("logs/app.log", LogLevel::INFO)
       .enableColors(true);

    // Simple logging
    log.info("Application started");
    log.debug("Processing {} items", 100);
    log.error("Failed to connect: {}", error_msg);

    // Named parameter logging (Mustache-style)
    log.info("User {user_id} logged in from {ip}",
        {{"user_id", 12345}, {"ip", "192.168.1.1"}});

    return 0;
}
```

### Named Templates (Advanced)

```cpp
// Method 1: Direct map initialization
log.warn("Trade rejected: Symbol={symbol} Size={size}",
    {{"symbol", "AAPL"}, {"size", 1000}});

// Method 2: Using named_args helper
log.debug("Request {request_id}: {method} {path}",
    Logger::named_args("request_id", "abc123", "method", "GET", "path", "/api/users"));

// Method 3: Nested structures
Logger::TemplateParams::NestedMap user;
user["name"] = Logger::TemplateValue{"Alice"};
user["id"] = Logger::TemplateValue{12345};

log.info("Welcome {user.name}!", {{"user", Logger::TemplateValue{std::move(user)}}});
```

## CMake Integration

### Option 1: As a Subdirectory (Recommended)

```cmake
# In your CMakeLists.txt
add_subdirectory(external/cpp23-logger)

# Link your target
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE logger)
```

### Option 2: With Custom std.pcm (Advanced)

If your project builds its own `std.pcm` with specific compiler flags:

```cmake
# Set these BEFORE adding cpp23-logger subdirectory
set(BIGBROTHER_STD_PCM_DIR "${CMAKE_SOURCE_DIR}/modules" CACHE PATH "std.pcm directory")
set(LIBCXX_MODULES_PATH "/usr/local/share/libc++/v1" CACHE STRING "libc++ module sources")

add_subdirectory(external/cpp23-logger)
```

The logger will automatically use your prebuilt `std.pcm` if these variables are set.

### Option 3: Standalone Build

```bash
cmake -B build -G Ninja
ninja -C build
```

CMake 3.30+ will automatically build `std.pcm` when `CMAKE_CXX_MODULE_STD=1` is enabled.

## CMake Variables Reference

### Parent Project Variables (Optional)

These variables allow parent projects to share their precompiled `std.pcm`:

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `STD_PCM_DIR` | PATH | `${CMAKE_BINARY_DIR}/modules` | Directory containing `std.pcm` |
| `BIGBROTHER_STD_PCM_DIR` | PATH | - | Alternative name (auto-detected) |
| `LIBCXX_MODULES_PATH` | STRING | `/usr/local/share/libc++/v1` | Path to libc++ module sources |

### Logger-Specific Options

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `BUILD_TESTS` | BOOL | ON | Build test suite |
| `BUILD_EXAMPLES` | BOOL | ON | Build example programs |
| `ENABLE_CLANG_TIDY` | BOOL | OFF | Enable clang-tidy checks |
| `ENABLE_OPENMP` | BOOL | OFF | Enable OpenMP support |

### Example: Integrating with Custom Build

```cmake
# Your project's CMakeLists.txt

# Build std.pcm with your project's flags
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/modules/std.pcm
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++23 -stdlib=libc++
            -fPIC -O3 -march=native
            --precompile /usr/local/share/libc++/v1/std.cppm
            -o ${CMAKE_SOURCE_DIR}/modules/std.pcm
    COMMENT "Building std.pcm"
)

add_custom_target(build_std_modules
    DEPENDS ${CMAKE_SOURCE_DIR}/modules/std.pcm
)

# Share std.pcm with cpp23-logger
set(BIGBROTHER_STD_PCM_DIR "${CMAKE_SOURCE_DIR}/modules" CACHE PATH "std.pcm directory" FORCE)

# Now add logger - it will use your std.pcm
add_subdirectory(external/cpp23-logger)
```

## Performance Characteristics

### Log Level Filtering
- **Lock-free check**: ~5-10ns (atomic load)
- **Mutex acquisition**: ~20-50ns (uncontended)
- **File write**: ~100-500μs (depends on disk I/O)
- **Total per log call**: ~5-30μs average

### Named Template Processing
- **Named lookup**: O(1) average via `std::unordered_map`
- **Single-pass processing**: Linear in template string length
- **Comparable to positional format()** for small parameter counts

## Requirements

- **Compiler**: Clang 17+, GCC 14+, or MSVC 19.34+
- **CMake**: 3.28+ (3.30+ recommended for `import std;`)
- **C++ Standard**: C++23
- **Standard Library**: libc++ (LLVM's C++ standard library)

## Thread-Safety Details

### Atomic Operations
```cpp
// Member variable
std::atomic<LogLevel> current_level;

// Thread-safe operations
current_level.store(LogLevel::DEBUG, std::memory_order_relaxed);
auto level = current_level.load(std::memory_order_relaxed);
```

### Mutex-Protected Operations
The following operations are protected by `std::mutex`:
- File I/O (writes to log file)
- Console output
- Logger initialization

### Memory Ordering
We use `std::memory_order_relaxed` because:
- Log level changes are independent operations
- No need for synchronization with other variables
- Provides best performance for high-frequency logging

## License

Proprietary - suitable for extraction to private repository.

## Author

Olumuyiwa Oluwasanmi

## See Also

- [QUICKSTART.md](QUICKSTART.md) - Quick start guide
- [examples/](examples/) - Usage examples
- [tests/](tests/) - Test suite
