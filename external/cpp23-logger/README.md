# C++23 Logger

A production-grade, thread-safe logging system with Mustache-style named templates and fluent API.

## Features

- **Mustache Templates**: Named placeholders `{param_name}` for context-rich logging
- **Fluent API**: Method chaining for ergonomic configuration
- **Thread-Safe**: Concurrent logging without mutex contention
- **C++23 Modules**: Modern module system with clean namespace separation
- **ANSI Colors**: Color-coded console output (optional)
- **Timezone-Aware**: ISO 8601 timestamps with UTC offset
- **Zero Dependencies**: Self-contained, no external dependencies
- **Smart Pointers**: RAII-compliant resource management

## Requirements

- **Compiler**: Clang 17+, GCC 14+, or MSVC 19.34+ (VS 2022 17.4)
- **Build System**: CMake 3.28+ or Bazel 7.0+
- **C++ Standard**: C++23 with modules support

## Quick Start

### Installation via CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    logger
    GIT_REPOSITORY https://github.com/YOUR_USERNAME/cpp23-logger.git
    GIT_TAG        v1.0.0
)

FetchContent_MakeAvailable(logger)

# Link to your target
target_link_libraries(your_target PRIVATE logger)
```

### Installation via Bazel

In your `WORKSPACE` file:

```python
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "cpp23_logger",
    remote = "https://github.com/YOUR_USERNAME/cpp23-logger.git",
    tag = "v1.0.0",
)
```

In your `BUILD.bazel` file:

```python
cc_binary(
    name = "my_app",
    srcs = ["main.cpp"],
    deps = ["@cpp23_logger//:logger"],
)
```

Build your project:

```bash
bazel build //:my_app
```

### Basic Usage

```cpp
import logger;

int main() {
    auto& log = logger::Logger::getInstance()
        .initialize("logs/app.log")
        .setLevel(logger::LogLevel::DEBUG)
        .enableColors(true);

    // Positional formatting
    log.info("Application started");
    log.error("Connection failed: {}, retrying in {}s", error_msg, 5);

    // Named template formatting
    log.info("User {user_id} logged in from {ip}",
        {{"user_id", 12345}, {"ip", "192.168.1.1"}});

    log.warn("Trade rejected: Symbol={symbol} Size={size}",
        logger::Logger::named_args("symbol", "AAPL", "size", 1000));

    return 0;
}
```

### Building from Source

```bash
git clone https://github.com/YOUR_USERNAME/cpp23-logger.git
cd cpp23-logger
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

## API Reference

### Log Levels

```cpp
enum class LogLevel {
    TRACE,     // Extremely detailed debugging
    DEBUG,     // Detailed diagnostic information
    INFO,      // General informational messages
    WARN,      // Warning messages
    ERROR,     // Error messages
    CRITICAL   // Critical system failures
};
```

### Fluent API

```cpp
auto& logger = Logger::getInstance()
    .initialize("logs/app.log")
    .setLevel(LogLevel::DEBUG)
    .enableColors(true)
    .setConsoleOutput(true);
```

### Positional Formatting

```cpp
logger.info("Value: {}, Count: {}", 42, 100);
logger.debug("Processing {}/{} items", current, total);
```

### Named Template Formatting

```cpp
// Method 1: Initializer list
logger.info("User {user_id} at {time}",
    {{"user_id", 123}, {"time", "14:23"}});

// Method 2: named_args helper
logger.warn("Error {code}: {message}",
    Logger::named_args("code", 404, "message", "Not found"));

// Method 3: Complex multi-parameter
logger.debug("Simulation[{sim_id}] Step[{step}]: Temp={temp}°C",
    {
        {"sim_id", simulation_id},
        {"step", current_step},
        {"temp", temperature}
    });
```

## Thread Safety

The logger is fully thread-safe:

- `Logger::format()` is static with no shared state
- `Logger::processTemplate()` is static with no shared state
- All logging methods are mutex-protected
- Multiple threads can log concurrently without corruption
- Zero contention for formatting operations

## Performance

- ~5-30μs per log call (buffered writes)
- Stack-allocated formatting (minimal heap allocations)
- Named template lookup: O(1) average via hash map
- Immediate flush to disk for reliability

## Examples

See the [examples/](examples/) directory for complete working examples:

- `basic_usage.cpp` - Simple positional formatting
- `named_templates.cpp` - Mustache-style templates

## Testing

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## License

BSD 4-Clause License with Author's Special Rights. See [LICENSE](LICENSE) for details.

## Author

Olumuyiwa Oluwasanmi

## Version

1.0.0 (November 2025)
