# Logger Migration Guide

## Overview
This project has migrated from `std::cout`/`std::cerr` to the cpp23-logger library for structured, thread-safe logging.

## Conversion Pattern

### Before (Old Style)
```cpp
#include <iostream>

int main() {
    std::cout << "Starting application\n";
    std::cout << "Processing " << count << " items\n";
    std::cerr << "Error: " << error_message << "\n";
}
```

### After (New Style with Logger)
```cpp
import logger;

int main() {
    auto& log = logger::Logger::getInstance();
    
    log.info("Starting application");
    log.info("Processing {} items", count);
    log.error("Error: {}", error_message);
}
```

## Log Levels
- `log.debug()` - Detailed diagnostic information
- `log.info()` - General informational messages (replaces std::cout)
- `log.warn()` - Warning messages
- `log.error()` - Error messages (replaces std::cerr)
- `log.critical()` - Critical errors

## Advanced Features

### Named Arguments
```cpp
log.debug("Processing {filename} with {size} bytes", 
    logger::Logger::named_args("filename", file, "size", file_size));
```

### Mustache-style Templates
```cpp
log.info("Array with {elements} elements, using {mode} mode",
    logger::Logger::named_args("elements", arr.size(), "mode", "PARALLEL"));
```

## Files Converted
- ✅ modules/fastjson_parallel.cpp (2 locations)
- ✅ tests/utf8_validation_test.cpp (complete)
- ⏳ tests/performance/*.cpp (20+ files)
- ⏳ benchmarks/*.cpp (10+ files)
- ⏳ tools/*.cpp (3+ files)
- ⏳ examples/*.cpp (5+ files)

## How to Convert Remaining Files

1. Add logger import at the top after includes:
   ```cpp
   import logger;
   ```

2. Add logger instance at the start of main() or test functions:
   ```cpp
   auto& log = logger::Logger::getInstance();
   ```

3. Replace iostream patterns:
   - `std::cout << "text\n";` → `log.info("text");`
   - `std::cout << "Value: " << val << "\n";` → `log.info("Value: {}", val);`
   - `std::cerr << "Error\n";` → `log.error("Error");`

4. Complex stream chains need manual conversion:
   ```cpp
   // Before:
   std::cout << std::setw(10) << value << " " << std::fixed << std::setprecision(2) << ratio << "\n";
   
   // After:
   log.info("{:10} {:.2f}", value, ratio);  // Using fmt-style formatting
   ```

## Build Configuration
- CMake: Logger integrated via submodule by default
- CMake with FetchContent: `cmake -DUSE_FETCHCONTENT_LOGGER=ON -B build -G Ninja`
- Bazel: Now uses Bzlmod (MODULE.bazel) instead of WORKSPACE

## Status
Partial conversion completed. Core modules and sample test files converted.
Remaining files marked with ⏳ require manual conversion following the patterns above.
