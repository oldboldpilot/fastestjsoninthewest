# Changelog

All notable changes to cpp23-logger will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2025-12-06

### Added
- `import std;` support for modern C++23 module usage
- CMake variable `STD_PCM_DIR` for sharing precompiled `std.pcm` between projects
- CMake variable `LIBCXX_MODULES_PATH` for custom libc++ module source paths
- Automatic detection of parent project's `std.pcm` via `BIGBROTHER_STD_PCM_DIR`
- `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` support for CMake 3.30+
- Dependency on parent's `build_std_modules` target when available
- Comprehensive README.md documentation
- QUICKSTART.md guide for new users
- This CHANGELOG.md file

### Changed
- **BREAKING**: Replaced 24 individual `#include` directives with single `import std;`
- Log level storage from `LogLevel` to `std::atomic<LogLevel>` for thread-safety
- `setLevel()` now uses `std::atomic::store()` instead of mutex-protected write
- `getLevel()` now uses `std::atomic::load()` instead of mutex-protected read
- `log()` method performs lock-free level check before mutex acquisition
- Global module fragment now empty (no standard library includes)
- CMakeLists.txt enhanced with module scanning and std.pcm configuration

### Performance
- **Lock-free log level filtering**: ~5-10ns (down from ~20-50ns with mutex)
- Reduced mutex contention for `setLevel()`/`getLevel()` operations
- Faster compilation with `import std;` (single module import vs. 24 headers)

### Thread-Safety
- Fixed race condition in log level checking (read before mutex acquisition)
- All log level operations now use atomic operations with `memory_order_relaxed`
- Maintained mutex protection for file I/O and console output
- Safe for concurrent access from multiple threads

### Documentation
- Added comprehensive CMake integration guide
- Documented thread-safety guarantees and performance characteristics
- Added examples for multi-threaded usage
- Explained `std.pcm` sharing between parent and child projects

## [1.0.0] - 2025-11-01

### Added
- Initial release
- Thread-safe logging with mutex protection
- Mustache-style named parameter formatting
- Fluent API with method chaining
- ANSI color support for console output
- UTF-8 support with automatic locale detection
- ISO 8601 timestamps with timezone offset
- Multiple log levels (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL)
- Singleton pattern for global access
- Structured logging with template parameters
- File and console output support
- Custom formatters for `std::filesystem::path`

### Supported Compilers
- Clang 17+ with libc++
- GCC 14+ with libstdc++
- MSVC 19.34+ (Visual Studio 2022 17.4)

## Notes

### Version 1.1.0 Thread-Safety Improvements

The atomic log level implementation provides significant performance benefits:

**Before (mutex-protected)**:
```cpp
auto getLevel() const -> LogLevel {
    std::lock_guard<std::mutex> lock(mutex_);  // ~20-50ns
    return current_level;
}
```

**After (atomic)**:
```cpp
auto getLevel() const -> LogLevel {
    return current_level.load(std::memory_order_relaxed);  // ~5-10ns
}
```

This is especially beneficial for high-frequency logging where level checks dominate the execution time.

### Binary Compatibility Note

When using `import std;`, the precompiled `std.pcm` **must** be built with the same compiler flags as the consuming code. This is why the logger detects and uses the parent project's `std.pcm` when available via `BIGBROTHER_STD_PCM_DIR`.

**Safe**: Parent and logger use same `std.pcm` → ✓ Binary compatible
**Unsafe**: Different `std.pcm` with different flags → ✗ Undefined behavior

Always ensure consistency when sharing `std.pcm` across targets!
