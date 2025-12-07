# cpp23-logger Quick Start Guide

Get up and running with cpp23-logger in 5 minutes.

## Installation

### Method 1: Git Submodule (Recommended)

```bash
# Add as submodule
git submodule add https://github.com/yourusername/cpp23-logger external/cpp23-logger

# Update submodules
git submodule update --init --recursive
```

### Method 2: Direct Clone

```bash
git clone https://github.com/yourusername/cpp23-logger external/cpp23-logger
```

## Basic Setup

### 1. Update Your CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyApp LANGUAGES CXX)

# C++23 required
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add cpp23-logger
add_subdirectory(external/cpp23-logger)

# Create your executable
add_executable(myapp src/main.cpp)

# Link with logger
target_link_libraries(myapp PRIVATE logger)
```

### 2. Write Your First Program

**src/main.cpp**:
```cpp
import logger;

int main() {
    auto& log = logger::Logger::getInstance();

    // Initialize logger
    log.initialize("logs/app.log", logger::LogLevel::INFO)
       .enableColors(true);

    // Log messages
    log.info("Application started successfully!");
    log.debug("This won't appear (below INFO level)");

    return 0;
}
```

### 3. Build and Run

```bash
# Configure
cmake -B build -G Ninja

# Build
ninja -C build

# Run
./build/myapp
```

## Common Usage Patterns

### Pattern 1: Application Initialization

```cpp
import logger;
using logger::Logger;
using logger::LogLevel;

int main() {
    auto& log = Logger::getInstance();

    // Production config
    log.initialize("logs/production.log", LogLevel::INFO)
       .enableColors(false)  // Disable for log files
       .setConsoleOutput(true);

    log.info("Starting production server");
    // ... your app code
}
```

### Pattern 2: Development/Debug Mode

```cpp
int main() {
    auto& log = Logger::getInstance();

    #ifdef DEBUG
        log.initialize("logs/debug.log", LogLevel::DEBUG)
           .enableColors(true);  // Pretty colors for terminal
    #else
        log.initialize("logs/app.log", LogLevel::INFO)
           .enableColors(false);
    #endif

    log.debug("Debug info: value = {}", 42);
    log.info("Normal operation");
}
```

### Pattern 3: Multi-Threaded Application

```cpp
#include <thread>
import logger;

void worker_thread(int id) {
    auto& log = logger::Logger::getInstance();

    // Thread-safe logging - no setup needed!
    log.info("Worker {} started", id);

    for (int i = 0; i < 100; ++i) {
        log.debug("Worker {} processing item {}", id, i);
    }

    log.info("Worker {} finished", id);
}

int main() {
    auto& log = logger::Logger::getInstance();
    log.initialize("logs/multithread.log", logger::LogLevel::INFO);

    // Spawn multiple threads - all logging is thread-safe
    std::vector<std::thread> workers;
    for (int i = 0; i < 10; ++i) {
        workers.emplace_back(worker_thread, i);
    }

    for (auto& t : workers) {
        t.join();
    }

    return 0;
}
```

### Pattern 4: Named Parameters (Advanced)

```cpp
// Web server logging
log.info("Request {method} {path} from {ip}",
    logger::Logger::named_args(
        "method", "GET",
        "path", "/api/users",
        "ip", "192.168.1.100"
    ));

// Database queries
log.debug("Query executed: {query} in {time_ms}ms, rows={rows}",
    {
        {"query", "SELECT * FROM users"},
        {"time_ms", 45},
        {"rows", 1234}
    });

// Structured logging for monitoring systems
log.info("metric.request_latency value={latency} tags={tags}",
    {
        {"latency", 123.45},
        {"tags", "endpoint=/api,region=us-west"}
    });
```

## Advanced: Using Parent Project's std.pcm

If your project already builds `std.pcm` with specific compiler flags (OpenMP, optimization, etc.), share it with the logger:

### Parent Project CMakeLists.txt

```cmake
# Build your std.pcm
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/modules/std.pcm
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++23 -stdlib=libc++
            -fPIC -O3 -march=native -fopenmp
            --precompile /usr/local/share/libc++/v1/std.cppm
            -o ${CMAKE_SOURCE_DIR}/modules/std.pcm
    COMMENT "Building std.pcm with project flags"
)

add_custom_target(build_std_modules
    DEPENDS ${CMAKE_SOURCE_DIR}/modules/std.pcm
)

# Share with cpp23-logger (BEFORE add_subdirectory)
set(BIGBROTHER_STD_PCM_DIR "${CMAKE_SOURCE_DIR}/modules"
    CACHE PATH "Shared std.pcm directory" FORCE)

# Now add logger - it will use your std.pcm
add_subdirectory(external/cpp23-logger)
```

### Why Share std.pcm?

1. **Binary Compatibility**: `std.pcm` must be built with **identical** compiler flags
2. **Build Performance**: Reuse existing `std.pcm` instead of rebuilding
3. **Consistency**: Ensure logger uses same standard library configuration

## CMake Variables Quick Reference

Set these **before** `add_subdirectory(external/cpp23-logger)`:

```cmake
# Optional: Custom std.pcm directory
set(BIGBROTHER_STD_PCM_DIR "${CMAKE_SOURCE_DIR}/modules"
    CACHE PATH "std.pcm directory" FORCE)

# Optional: Custom libc++ module sources path
set(LIBCXX_MODULES_PATH "/usr/local/share/libc++/v1"
    CACHE STRING "libc++ module sources" FORCE)

# Optional: Disable tests/examples
set(BUILD_TESTS OFF CACHE BOOL "Don't build logger tests" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Don't build logger examples" FORCE)
```

## Log Levels

From most to least verbose:

| Level | Use Case | Example |
|-------|----------|---------|
| **TRACE** | Extremely detailed debugging | Variable dumps, loop iterations |
| **DEBUG** | Detailed diagnostic info | Algorithm steps, state transitions |
| **INFO** | General informational messages | App lifecycle, config changes |
| **WARN** | Warnings for potentially harmful situations | Retry attempts, deprecated APIs |
| **ERROR** | Error messages for serious issues | Failed operations, caught exceptions |
| **CRITICAL** | Critical system failures | Unrecoverable errors, system shutdown |

## Formatting Options

### Positional Arguments

```cpp
log.info("User {} logged in at {}", username, timestamp);
log.debug("Processing {} of {} items", current, total);
log.error("Error {}: {}", error_code, error_message);
```

### Format Specifiers

```cpp
log.info("Price: ${:.2f}", 123.456);        // Price: $123.46
log.debug("Hex: {:#x}", 255);               // Hex: 0xff
log.info("Percent: {:.1f}%", 95.5);         // Percent: 95.5%
```

### Named Parameters

```cpp
// Basic named parameters
log.info("User {user_id} from {country}",
    {{"user_id", 12345}, {"country", "USA"}});

// Using helper function
log.warn("Retry {attempt} for {operation}",
    logger::Logger::named_args("attempt", 3, "operation", "database_connect"));
```

## Troubleshooting

### Build Error: "import std not found"

**Solution**: Ensure CMake 3.30+ or provide `STD_PCM_DIR`:

```cmake
# Option 1: Upgrade CMake to 3.30+

# Option 2: Set std.pcm directory
set(BIGBROTHER_STD_PCM_DIR "/path/to/modules" CACHE PATH "" FORCE)
```

### Build Error: "No matching precompiled module"

**Problem**: Parent's `std.pcm` built with different compiler flags

**Solution**: Either:
1. Let logger build its own `std.pcm` (remove `BIGBROTHER_STD_PCM_DIR`)
2. Ensure parent uses same compiler flags for `std.pcm`

### Runtime: No log output

**Check**:
```cpp
// 1. Is log level high enough?
log.setLevel(logger::LogLevel::DEBUG);  // Lower threshold

// 2. Is file path writable?
log.initialize("logs/app.log");  // Will create logs/ directory

// 3. Is console output enabled?
log.setConsoleOutput(true);
```

## Next Steps

- Read [README.md](README.md) for complete API documentation
- Check [examples/](examples/) for more usage examples
- Review [tests/](tests/) for comprehensive test coverage

## Getting Help

- GitHub Issues: https://github.com/yourusername/cpp23-logger/issues
- Documentation: See [README.md](README.md)

## Performance Tips

1. **Use appropriate log levels** - DEBUG/TRACE have overhead
2. **Disable colors for file output** - `.enableColors(false)`
3. **Flush manually if needed** - `log.flush()`
4. **Leverage lock-free level check** - Filter happens before mutex

Example optimized configuration:
```cpp
log.initialize("logs/production.log", logger::LogLevel::INFO)
   .enableColors(false)      // No ANSI codes in file
   .setConsoleOutput(false); // File only, no console overhead
```
