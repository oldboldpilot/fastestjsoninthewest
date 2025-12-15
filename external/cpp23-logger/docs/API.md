# API Reference

## Namespace

All logger components are in the `logger` namespace:

```cpp
import logger;

auto& log = logger::Logger::getInstance();
```

## Classes

### Logger

Thread-safe singleton logger class.

#### Static Methods

```cpp
// Get singleton instance
[[nodiscard]] static auto getInstance() -> Logger&;

// Positional format string (static, thread-safe)
template <typename... Args>
[[nodiscard]] static auto format(std::string_view fmt_str, Args&&... args) -> std::string;

// Process named template (static, thread-safe)
[[nodiscard]] static auto processTemplate(std::string_view template_str,
                                           TemplateParams const& params) -> std::string;

// Create template parameters from variadic args
template <typename... Args>
[[nodiscard]] static auto named_args(Args&&... args) -> TemplateParams;
```

#### Configuration Methods

```cpp
// Initialize logger (fluent API)
auto initialize(LoggerConfig const& config = {}) -> Logger&;
auto initialize(std::string const& log_file_path,
                LogLevel level = LogLevel::INFO,
                bool console_output = true) -> Logger&;

// Set log level (fluent API)
auto setLevel(LogLevel level) -> Logger&;

// Enable/disable colors (fluent API)
auto enableColors(bool enable) -> Logger&;

// Enable/disable console output (fluent API)
auto setConsoleOutput(bool enable) -> Logger&;

// Get current log level
[[nodiscard]] auto getLevel() const -> LogLevel;
```

#### Logging Methods - Positional Formatting

```cpp
// All methods support variadic arguments with {} placeholders
template <typename... Args>
auto trace(std::string_view fmt_str, Args&&... args) -> void;

template <typename... Args>
auto debug(std::string_view fmt_str, Args&&... args) -> void;

template <typename... Args>
auto info(std::string_view fmt_str, Args&&... args) -> void;

template <typename... Args>
auto warn(std::string_view fmt_str, Args&&... args) -> void;

template <typename... Args>
auto error(std::string_view fmt_str, Args&&... args) -> void;

template <typename... Args>
auto critical(std::string_view fmt_str, Args&&... args) -> void;
```

#### Logging Methods - Named Templates

```cpp
// All methods support Mustache-style {param_name} placeholders
auto trace(std::string_view template_str, TemplateParams const& params) -> void;
auto debug(std::string_view template_str, TemplateParams const& params) -> void;
auto info(std::string_view template_str, TemplateParams const& params) -> void;
auto warn(std::string_view template_str, TemplateParams const& params) -> void;
auto error(std::string_view template_str, TemplateParams const& params) -> void;
auto critical(std::string_view template_str, TemplateParams const& params) -> void;
```

#### Utility Methods

```cpp
// Flush log buffer to disk
auto flush() -> void;

// Rotate log file (future feature)
auto rotate() -> void;
```

### TemplateValue

Thread-safe wrapper for template parameter values.

```cpp
class TemplateValue {
  public:
    template <typename T>
    explicit TemplateValue(T&& value);

    [[nodiscard]] auto toString() const -> std::string const&;
};
```

## Enumerations

### LogLevel

```cpp
enum class LogLevel : std::uint8_t {
    TRACE = 0,      // Most verbose
    DEBUG = 1,
    INFO = 2,       // Default
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5    // Least verbose
};
```

### LogFormat

```cpp
enum class LogFormat {
    TEXT,    // Human-readable (default, implemented)
    JSON,    // JSON structured (future)
    COMPACT  // Minimal format (future)
};
```

## Structures

### LoggerConfig

```cpp
struct LoggerConfig {
    std::string log_file_path{"logs/app.log"};
    LogLevel min_level{LogLevel::INFO};
    LogFormat format{LogFormat::TEXT};
    bool console_output{true};
    bool console_color{true};
    std::size_t max_file_size_mb{100};      // Future
    std::size_t max_backup_files{5};        // Future
    bool async_logging{false};              // Future
};
```

## Type Aliases

```cpp
using TemplateParams = std::unordered_map<std::string, TemplateValue>;
```

## Usage Examples

### Basic Initialization

```cpp
import logger;

auto& log = logger::Logger::getInstance();
log.initialize("logs/app.log", logger::LogLevel::DEBUG, true);
```

### Fluent API Initialization

```cpp
auto& log = logger::Logger::getInstance()
    .initialize("logs/app.log")
    .setLevel(logger::LogLevel::DEBUG)
    .enableColors(true)
    .setConsoleOutput(true);
```

### Positional Formatting

```cpp
log.info("Value: {}, Count: {}", 42, 100);
log.debug("Processing {}/{} records", current, total);
log.error("HTTP Error {}: {}", 404, "Not found");
```

### Named Template Formatting

```cpp
// Method 1: Initializer list
log.info("User {user_id} logged in from {ip}",
    {{"user_id", 12345}, {"ip", "192.168.1.1"}});

// Method 2: named_args helper
log.warn("Error {code}: {message}",
    logger::Logger::named_args("code", 404, "message", "Not found"));

// Method 3: Complex parameters
log.debug("Sim[{sim_id}] Step[{step}]: Temp={temp}°C",
    {
        {"sim_id", simulation_id},
        {"step", current_step},
        {"temp", temperature}
    });
```

### Configuration Changes

```cpp
// Change log level dynamically
log.setLevel(logger::LogLevel::WARN)
   .enableColors(false);

// Flush logs immediately
log.flush();
```

## Thread Safety Guarantees

- **Logger::getInstance()**: Thread-safe singleton (C++11 guarantee)
- **Logger::format()**: Fully thread-safe (static, no shared state)
- **Logger::processTemplate()**: Fully thread-safe (static, no shared state)
- **All logging methods**: Mutex-protected, safe for concurrent use
- **TemplateValue**: Uses std::shared_ptr for thread-safe value sharing

## Performance Characteristics

- **Log call overhead**: ~5-30μs per call (buffered writes)
- **Template processing**: Single-pass, O(n) where n is string length
- **Named lookup**: O(1) average via std::unordered_map
- **Memory**: Minimal heap allocations, stack-allocated formatting
- **Contention**: Zero contention for format() and processTemplate()

## Best Practices

1. **Initialize once**: Call `initialize()` once at application startup
2. **Use appropriate levels**: TRACE/DEBUG for development, INFO+ for production
3. **Disable colors for files**: Colors are for console only
4. **Flush before exit**: Call `flush()` before application termination
5. **Use named templates**: For complex logging with many parameters
6. **Thread safety**: Logger is thread-safe, no external synchronization needed
