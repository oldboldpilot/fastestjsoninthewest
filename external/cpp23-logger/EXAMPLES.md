# cpp23-logger - Comprehensive Examples

This document provides detailed examples demonstrating all template features available in the cpp23-logger library.

## Table of Contents

1. [Basic Logging](#basic-logging)
2. [Positional Formatting](#positional-formatting)
3. [Named Templates](#named-templates)
4. [Arrays and Collections](#arrays-and-collections)
5. [Nested Dictionaries](#nested-dictionaries)
6. [std::map and std::unordered_map](#stdmap-and-stdunordered_map)
7. [std::span Support](#stdspan-support)
8. [Section Iteration](#section-iteration)
9. [Context Variables](#context-variables)
10. [Filter Operations](#filter-operations)
11. [Functional Operations](#functional-operations)
12. [JSON Construction Helpers](#json-construction-helpers)
13. [Real-World Trading System Examples](#real-world-examples)

---

## Basic Logging

Simple logging without any templating:

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log", LogLevel::DEBUG, true);

    log.trace("Trace level message");
    log.debug("Debug level message");
    log.info("Info level message");
    log.warn("Warning level message");
    log.error("Error level message");
    log.critical("Critical level message");

    return 0;
}
```

---

## Positional Formatting

Use `{}` placeholders for positional arguments:

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Simple positional formatting
    log.info("Processing {} items", 100);
    // Output: Processing 100 items

    // Multiple arguments
    log.info("User {}: logged in from {}", "alice", "192.168.1.1");
    // Output: User alice: logged in from 192.168.1.1

    // Mixed types
    log.debug("Price: ${}, Volume: {}, Change: {}%", 150.25, 1000, 2.5);
    // Output: Price: $150.25, Volume: 1000, Change: 2.5%

    return 0;
}
```

---

## Named Templates

Use `{name}` placeholders for self-documenting logs:

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Method 1: Direct map initialization
    log.info("User {user_id} logged in from {ip}",
        {{"user_id", 12345}, {"ip", "192.168.1.1"}});

    // Method 2: Using named_args helper
    log.warn("Trade rejected: Symbol={symbol} Size={size}",
        Logger::named_args("symbol", "AAPL", "size", 1000));

    // Method 3: Complex multi-parameter
    int simulation_id = 42;
    int current_step = 100;
    double temperature = 273.15;

    log.debug(
        "Simulation[{sim_id}] Step[{step}]: Temp={temp}°C",
        {
            {"sim_id", simulation_id},
            {"step", current_step},
            {"temp", temperature}
        }
    );

    return 0;
}
```

---

## Arrays and Collections

### std::vector

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Create array of items
    TemplateValue::NestedArray items;
    items.push_back(TemplateValue{"Apple"});
    items.push_back(TemplateValue{"Banana"});
    items.push_back(TemplateValue{"Cherry"});

    Logger::TemplateParams params;
    params["items"] = TemplateValue{std::move(items)};

    // Access by index
    log.info("First: {items.0}, Second: {items.1}, Third: {items.2}", params);
    // Output: First: Apple, Second: Banana, Third: Cherry

    return 0;
}
```

### std::array

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Fixed-size array
    std::array<TemplateValue, 4> seasons{
        TemplateValue{"Spring"},
        TemplateValue{"Summer"},
        TemplateValue{"Fall"},
        TemplateValue{"Winter"}
    };

    Logger::TemplateParams params;
    params["seasons"] = TemplateValue{seasons};

    log.info("Q1: {seasons.0}, Q2: {seasons.1}, Q3: {seasons.2}, Q4: {seasons.3}", params);
    // Output: Q1: Spring, Q2: Summer, Q3: Fall, Q4: Winter

    return 0;
}
```

---

## Nested Dictionaries

### Two-Level Nesting

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Create nested user structure
    TemplateValue::NestedMap user;
    user["name"] = TemplateValue{"Alice"};
    user["id"] = TemplateValue{12345};
    user["email"] = TemplateValue{"alice@example.com"};

    Logger::TemplateParams params;
    params["user"] = TemplateValue{std::move(user)};

    // Access nested values with dot notation
    log.info("User: {user.name} (ID: {user.id}, Email: {user.email})", params);
    // Output: User: Alice (ID: 12345, Email: alice@example.com)

    return 0;
}
```

### Three-Level Nesting

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Create deeply nested config structure
    TemplateValue::NestedMap database;
    database["host"] = TemplateValue{"localhost"};
    database["port"] = TemplateValue{5432};

    TemplateValue::NestedMap server;
    server["name"] = TemplateValue{"production"};
    server["database"] = TemplateValue{std::move(database)};

    TemplateValue::NestedMap config;
    config["version"] = TemplateValue{"1.0.0"};
    config["server"] = TemplateValue{std::move(server)};

    Logger::TemplateParams params;
    params["config"] = TemplateValue{std::move(config)};

    log.info("Connecting to {config.server.database.host}:{config.server.database.port}", params);
    // Output: Connecting to localhost:5432

    return 0;
}
```

---

## std::map and std::unordered_map

### std::unordered_map

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    Logger::TemplateParams params;
    params.emplace("user_id", TemplateValue{12345});
    params.emplace("ip", TemplateValue{"192.168.1.100"});
    params.emplace("time", TemplateValue{"14:23:45"});

    log.info("User {user_id} logged in from {ip} at {time}", params);
    // Output: User 12345 logged in from 192.168.1.100 at 14:23:45

    return 0;
}
```

### std::map (Ordered)

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    std::map<std::string, TemplateValue> ordered_config{
        {"endpoint", TemplateValue{"https://api.example.com"}},
        {"retries", TemplateValue{3}},
        {"timeout", TemplateValue{30}}
    };

    Logger::TemplateParams params;
    params.emplace("settings", TemplateValue{std::move(ordered_config)});

    log.info("API: {settings.endpoint}, Timeout: {settings.timeout}s, Retries: {settings.retries}", params);
    // Output: API: https://api.example.com, Timeout: 30s, Retries: 3

    return 0;
}
```

---

## std::span Support

```cpp
import logger;
#include <span>

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Create source data
    TemplateValue::NestedArray source_data;
    source_data.push_back(TemplateValue{"Red"});
    source_data.push_back(TemplateValue{"Green"});
    source_data.push_back(TemplateValue{"Blue"});
    source_data.push_back(TemplateValue{"Yellow"});

    // Create span view
    std::span<TemplateValue> colors_span{source_data};

    Logger::TemplateParams params;
    params.emplace("colors", TemplateValue{colors_span});

    log.info("Primary: {colors.0}, Secondary: {colors.1}, Tertiary: {colors.2}", params);
    // Output: Primary: Red, Secondary: Green, Tertiary: Blue

    // Test with subspan
    std::span<TemplateValue> primary_colors = colors_span.subspan(0, 3);
    Logger::TemplateParams params2;
    params2.emplace("primary", TemplateValue{primary_colors});

    log.info("Primaries: {primary.0}, {primary.1}, {primary.2}", params2);
    // Output: Primaries: Red, Green, Blue

    return 0;
}
```

---

## Section Iteration

Iterate over arrays or maps using `{{#name}}...{{/name}}` syntax:

### Array Iteration

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    TemplateValue::NestedArray colors;
    colors.push_back(TemplateValue{"red"});
    colors.push_back(TemplateValue{"green"});
    colors.push_back(TemplateValue{"blue"});

    Logger::TemplateParams params;
    params["colors"] = TemplateValue{std::move(colors)};

    // Simple iteration
    log.info("Colors: {{#colors}}{@value} {{/colors}}", params);
    // Output: Colors: red green blue

    return 0;
}
```

### Map Iteration

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    TemplateValue::NestedMap config;
    config["host"] = TemplateValue{"localhost"};
    config["port"] = TemplateValue{"8080"};
    config["timeout"] = TemplateValue{"30"};

    Logger::TemplateParams params;
    params["config"] = TemplateValue{std::move(config)};

    // Iterate over map entries
    log.info("Config: {{#config}}{@key}={@value} {{/config}}", params);
    // Output: Config: host=localhost port=8080 timeout=30 (order may vary)

    return 0;
}
```

---

## Context Variables

Special variables available during section iteration:

### @index, @first, @last

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    TemplateValue::NestedArray items;
    items.push_back(TemplateValue{"apple"});
    items.push_back(TemplateValue{"banana"});
    items.push_back(TemplateValue{"cherry"});

    Logger::TemplateParams params;
    params["items"] = TemplateValue{std::move(items)};

    // Use @index
    log.info("Items: {{#items}}[{@index}]={@value} {{/items}}", params);
    // Output: Items: [0]=apple [1]=banana [2]=cherry

    // Use @first and @last
    log.info("Items: {{#items}}{@value}(first={@first},last={@last}) {{/items}}", params);
    // Output: Items: apple(first=true,last=false) banana(first=false,last=false) cherry(first=false,last=true)

    return 0;
}
```

### @key and @value (for maps)

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    TemplateValue::NestedMap settings;
    settings["debug"] = TemplateValue{"true"};
    settings["log_level"] = TemplateValue{"INFO"};
    settings["max_connections"] = TemplateValue{"100"};

    Logger::TemplateParams params;
    params["settings"] = TemplateValue{std::move(settings)};

    log.info("Settings: {{#settings}}{@key}={@value}; {{/settings}}", params);
    // Output: Settings: debug=true; log_level=INFO; max_connections=100;

    return 0;
}
```

---

## Filter Operations

### reverse()

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    TemplateValue::NestedArray numbers;
    numbers.push_back(TemplateValue{"1"});
    numbers.push_back(TemplateValue{"2"});
    numbers.push_back(TemplateValue{"3"});

    auto const original = TemplateValue{numbers};
    auto const reversed = Logger::reverse(original);

    Logger::TemplateParams params;
    params["numbers"] = reversed;

    log.info("Reversed: {{#numbers}}{@value} {{/numbers}}", params);
    // Output: Reversed: 3 2 1

    return 0;
}
```

### slice()

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    TemplateValue::NestedArray items;
    items.push_back(TemplateValue{"A"});
    items.push_back(TemplateValue{"B"});
    items.push_back(TemplateValue{"C"});
    items.push_back(TemplateValue{"D"});
    items.push_back(TemplateValue{"E"});

    auto const original = TemplateValue{items};
    auto const sliced = Logger::slice(original, 1, 4); // Extract B, C, D

    Logger::TemplateParams params;
    params["items"] = sliced;

    log.info("Sliced: {{#items}}{@value} {{/items}}", params);
    // Output: Sliced: B C D

    return 0;
}
```

### split()

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Split CSV string
    auto const csv = TemplateValue{"red,green,blue,yellow"};
    auto const colors = Logger::split(csv, ",");

    Logger::TemplateParams params;
    params["colors"] = colors;

    log.info("Colors: {{#colors}}{@value} {{/colors}}", params);
    // Output: Colors: red green blue yellow

    // Split path
    auto const path = TemplateValue{"/usr/local/bin"};
    auto const parts = Logger::split(path, "/");

    Logger::TemplateParams params2;
    params2["parts"] = parts;

    log.info("Path parts: {{#parts}}[{@value}]{{/parts}}", params2);
    // Output: Path parts: [][usr][local][bin]

    // Split with multi-character delimiter
    auto const sentence = TemplateValue{"hello::world::test"};
    auto const words = Logger::split(sentence, "::");

    Logger::TemplateParams params3;
    params3["words"] = words;

    log.info("Words: {{#words}}{@value} {{/words}}", params3);
    // Output: Words: hello world test

    return 0;
}
```

### zip()

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    TemplateValue::NestedArray names;
    names.push_back(TemplateValue{"Alice"});
    names.push_back(TemplateValue{"Bob"});
    names.push_back(TemplateValue{"Charlie"});

    TemplateValue::NestedArray ages;
    ages.push_back(TemplateValue{"25"});
    ages.push_back(TemplateValue{"30"});
    ages.push_back(TemplateValue{"35"});

    auto const names_val = TemplateValue{names};
    auto const ages_val = TemplateValue{ages};
    auto const zipped = Logger::zip(names_val, ages_val);

    Logger::TemplateParams params;
    params["pairs"] = zipped;

    log.info("People: {{#pairs}}{@value.first}:{@value.second} {{/pairs}}", params);
    // Output: People: Alice:25 Bob:30 Charlie:35

    return 0;
}
```

---

## Functional Operations

### all()

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // All truthy values
    TemplateValue::NestedArray truthy;
    truthy.push_back(TemplateValue{"yes"});
    truthy.push_back(TemplateValue{"true"});
    truthy.push_back(TemplateValue{"1"});

    auto const truthy_val = TemplateValue{truthy};
    auto const all_truthy = Logger::all(truthy_val);

    Logger::TemplateParams params;
    params["result"] = all_truthy;

    log.info("All truthy: {result}", params);
    // Output: All truthy: true

    // With one falsy value
    TemplateValue::NestedArray mixed;
    mixed.push_back(TemplateValue{"yes"});
    mixed.push_back(TemplateValue{""});
    mixed.push_back(TemplateValue{"true"});

    auto const mixed_val = TemplateValue{mixed};
    auto const all_mixed = Logger::all(mixed_val);

    Logger::TemplateParams params2;
    params2["result"] = all_mixed;

    log.info("All mixed: {result}", params2);
    // Output: All mixed: false

    return 0;
}
```

### none()

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // All falsy values
    TemplateValue::NestedArray falsy;
    falsy.push_back(TemplateValue{""});
    falsy.push_back(TemplateValue{"false"});
    falsy.push_back(TemplateValue{"0"});

    auto const falsy_val = TemplateValue{falsy};
    auto const none_falsy = Logger::none(falsy_val);

    Logger::TemplateParams params;
    params["result"] = none_falsy;

    log.info("None falsy: {result}", params);
    // Output: None falsy: true

    return 0;
}
```

### not_()

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    auto const true_val = TemplateValue{"true"};
    auto const not_true = Logger::not_(true_val);

    Logger::TemplateParams params;
    params["result"] = not_true;

    log.info("Not true: {result}", params);
    // Output: Not true: false

    auto const false_val = TemplateValue{"false"};
    auto const not_false = Logger::not_(false_val);

    Logger::TemplateParams params2;
    params2["result"] = not_false;

    log.info("Not false: {result}", params2);
    // Output: Not false: true

    return 0;
}
```

---

## JSON Construction Helpers

### json()

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Create JSON-like object
    auto const user = Logger::json({
        {"name", TemplateValue{"Alice"}},
        {"age", TemplateValue{"30"}},
        {"city", TemplateValue{"NYC"}}
    });

    Logger::TemplateParams params;
    params["user"] = user;

    log.info("User: {user.name}, Age: {user.age}, City: {user.city}", params);
    // Output: User: Alice, Age: 30, City: NYC

    return 0;
}
```

### jsonArray()

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Create JSON-like array
    auto const colors = Logger::jsonArray({
        TemplateValue{"red"},
        TemplateValue{"green"},
        TemplateValue{"blue"}
    });

    Logger::TemplateParams params;
    params["colors"] = colors;

    log.info("Colors: {{#colors}}{@value} {{/colors}}", params);
    // Output: Colors: red green blue

    return 0;
}
```

---

## Real-World Trading System Examples

### Market Data Feed Logging

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("trading.log");

    // Create array of market ticks
    auto const ticks = Logger::jsonArray({
        Logger::json({
            {"symbol", TemplateValue{"AAPL"}},
            {"price", TemplateValue{175.50}},
            {"volume", TemplateValue{1000}}
        }),
        Logger::json({
            {"symbol", TemplateValue{"GOOGL"}},
            {"price", TemplateValue{140.25}},
            {"volume", TemplateValue{500}}
        }),
        Logger::json({
            {"symbol", TemplateValue{"MSFT"}},
            {"price", TemplateValue{380.75}},
            {"volume", TemplateValue{750}}
        })
    });

    Logger::TemplateParams params;
    params["ticks"] = ticks;
    params["count"] = TemplateValue{3};

    log.info("Processed {count} ticks: "
             "{{#ticks}}[{@value.symbol}@${@value.price}x{@value.volume}] {{/ticks}}", params);
    // Output: Processed 3 ticks: [AAPL@$175.5x1000] [GOOGL@$140.25x500] [MSFT@$380.75x750]

    return 0;
}
```

### Trade Execution Logging

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("trades.log");

    // Build complex trade structure
    auto const trade = Logger::json({
        {"id", TemplateValue{98765}},
        {"symbol", TemplateValue{"TSLA"}},
        {"side", TemplateValue{"BUY"}},
        {"quantity", TemplateValue{50}},
        {"price", TemplateValue{245.67}}
    });

    auto const account = Logger::json({
        {"id", TemplateValue{12345}},
        {"name", TemplateValue{"Hedge Fund Alpha"}}
    });

    auto const risk = Logger::json({
        {"var", TemplateValue{150000}},
        {"sharpe", TemplateValue{1.85}},
        {"drawdown", TemplateValue{-0.12}}
    });

    Logger::TemplateParams params;
    params["trade"] = trade;
    params["account"] = account;
    params["risk"] = risk;
    params["timestamp"] = TemplateValue{"2025-11-19T14:23:45Z"};

    log.info("[{timestamp}] Trade #{trade.id}: {trade.side} {trade.quantity} {trade.symbol} @ "
             "${trade.price} | "
             "Account: {account.name} (#{account.id}) | "
             "Risk: VaR=${risk.var} Sharpe={risk.sharpe} Drawdown={risk.drawdown}",
             params);
    // Output: [2025-11-19T14:23:45Z] Trade #98765: BUY 50 TSLA @ $245.67 |
    //         Account: Hedge Fund Alpha (#12345) |
    //         Risk: VaR=$150000 Sharpe=1.85 Drawdown=-0.12

    return 0;
}
```

### Server Status Monitoring

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("monitoring.log");

    // Create server status array
    auto const servers = Logger::jsonArray({
        Logger::json({
            {"name", TemplateValue{"web-01"}},
            {"status", TemplateValue{"healthy"}},
            {"cpu", TemplateValue{"45%"}}
        }),
        Logger::json({
            {"name", TemplateValue{"web-02"}},
            {"status", TemplateValue{"warning"}},
            {"cpu", TemplateValue{"85%"}}
        }),
        Logger::json({
            {"name", TemplateValue{"db-01"}},
            {"status", TemplateValue{"healthy"}},
            {"cpu", TemplateValue{"35%"}}
        })
    });

    Logger::TemplateParams params;
    params["servers"] = servers;

    log.info("Server Status Report:\n"
             "{{#servers}}- {@value.name}: {@value.status} (CPU: {@value.cpu})\n{{/servers}}",
             params);
    // Output: Server Status Report:
    //         - web-01: healthy (CPU: 45%)
    //         - web-02: warning (CPU: 85%)
    //         - db-01: healthy (CPU: 35%)

    return 0;
}
```

### Combined Filters Example

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("app.log");

    // Create array, reverse it, then slice it
    TemplateValue::NestedArray numbers;
    for (int i = 1; i <= 10; ++i) {
        numbers.push_back(TemplateValue{std::to_string(i)});
    }

    auto const original = TemplateValue{numbers};
    auto const reversed = Logger::reverse(original);
    auto const sliced = Logger::slice(reversed, 2, 7); // Get middle 5 elements

    Logger::TemplateParams params;
    params["processed"] = sliced;

    log.info("Processed: {{#processed}}{@value} {{/processed}}", params);
    // Output: Processed: 8 7 6 5 4

    return 0;
}
```

---

## Thread-Safe Logging

All logging operations are thread-safe. Multiple threads can log concurrently:

```cpp
import logger;
#include <thread>
#include <vector>

auto main() -> int {
    auto& log = Logger::getInstance();
    log.initialize("multithreaded.log");

    auto worker = [&log](int thread_id) {
        for (int i = 0; i < 100; ++i) {
            log.info("Thread {thread} iteration {iter}",
                Logger::named_args("thread", thread_id, "iter", i));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}
```

---

## Fluent API

Chain configuration methods:

```cpp
import logger;

auto main() -> int {
    auto& log = Logger::getInstance()
        .initialize("app.log")
        .setLevel(LogLevel::DEBUG)
        .enableColors(true)
        .setConsoleOutput(true);

    log.info("Logger initialized with fluent API");

    return 0;
}
```

---

## Summary

The cpp23-logger provides a comprehensive set of features for modern C++23 applications:

- **Basic logging**: Simple trace/debug/info/warn/error/critical levels
- **Positional formatting**: `{}` placeholders
- **Named templates**: `{name}` placeholders with nested access
- **Collections**: Support for std::vector, std::array, std::span
- **Dictionaries**: Support for std::map, std::unordered_map with nesting
- **Section iteration**: `{{#array}}...{{/array}}` with context variables
- **Filters**: reverse(), slice(), split(), zip()
- **Functional operations**: all(), none(), not_()
- **JSON helpers**: json(), jsonArray()
- **Thread-safe**: All operations are mutex-protected
- **UTF-8 support**: International characters work correctly
- **ANSI colors**: Optional colored console output

For more information, see the test files in the `tests/` directory.
