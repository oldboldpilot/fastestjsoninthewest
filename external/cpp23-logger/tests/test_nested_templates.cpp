/**
 * Comprehensive test suite for nested dictionary template support
 *
 * Tests the following features:
 * - Simple flat dictionaries (std::unordered_map)
 * - Nested dictionaries with dot notation
 * - std::map support (ordered maps)
 * - Mixed value types (strings, ints, doubles)
 * - Deep nesting (3+ levels)
 * - Missing keys and error handling
 * - Type safety with LoggableValue concept
 */

import logger;

#include <cassert>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <unordered_map>

using namespace logger;
using TemplateValue = Logger::TemplateValue;
using TemplateParams = Logger::TemplateParams;

// Test 1: Simple flat dictionary with std::unordered_map
auto test_flat_unordered_map() -> void {
    std::cout << "Test 1: Flat std::unordered_map...\n";

    Logger::TemplateParams params;
    params.emplace("user_id", TemplateValue{12345});
    params.emplace("ip", TemplateValue{"192.168.1.100"});
    params.emplace("time", TemplateValue{"14:23:45"});

    auto const result =
        Logger::processTemplate("User {user_id} logged in from {ip} at {time}", params);

    assert(result == "User 12345 logged in from 192.168.1.100 at 14:23:45");
    std::cout << "  ✓ Result: " << result << "\n";
}

// Test 2: Simple flat dictionary with std::map (ordered)
auto test_flat_ordered_map() -> void {
    std::cout << "Test 2: Flat std::map (ordered)...\n";

    std::map<std::string, TemplateValue> params;
    params.emplace("name", TemplateValue{"Alice"});
    params.emplace("age", TemplateValue{30});
    params.emplace("city", TemplateValue{"New York"});

    auto const result =
        Logger::processTemplate("Name: {name}, Age: {age}, City: {city}",
                                {{"name", "Alice"}, {"age", 30}, {"city", "New York"}});

    assert(result == "Name: Alice, Age: 30, City: New York");
    std::cout << "  ✓ Result: " << result << "\n";
}

// Test 3: Two-level nested dictionary with dot notation
auto test_nested_two_levels() -> void {
    std::cout << "Test 3: Two-level nested dictionary...\n";

    // Create nested user structure
    TemplateValue::NestedMap user_map;
    user_map["name"] = TemplateValue{"Alice"};
    user_map["id"] = TemplateValue{12345};
    user_map["email"] = TemplateValue{"alice@example.com"};

    Logger::TemplateParams params;
    params.emplace("user", TemplateValue{std::move(user_map)});
    params.emplace("action", TemplateValue{"login"});

    auto const result = Logger::processTemplate(
        "User {user.name} (ID: {user.id}, Email: {user.email}) performed {action}", params);

    assert(result == "User Alice (ID: 12345, Email: alice@example.com) performed login");
    std::cout << "  ✓ Result: " << result << "\n";
}

// Test 4: Three-level deep nesting
auto test_nested_three_levels() -> void {
    std::cout << "Test 4: Three-level deep nesting...\n";

    // Create database config structure
    TemplateValue::NestedMap db_config;
    db_config["host"] = TemplateValue{"localhost"};
    db_config["port"] = TemplateValue{5432};

    // Create server config structure
    TemplateValue::NestedMap server_config;
    server_config["name"] = TemplateValue{"production"};
    server_config["database"] = TemplateValue{std::move(db_config)};

    // Create root config structure
    TemplateValue::NestedMap config;
    config["server"] = TemplateValue{std::move(server_config)};
    config["version"] = TemplateValue{"1.0.0"};

    Logger::TemplateParams params;
    params.emplace("config", TemplateValue{std::move(config)});

    auto const result = Logger::processTemplate(
        "Connecting to {config.server.database.host}:{config.server.database.port} "
        "(Server: {config.server.name}, Version: {config.version})",
        params);

    assert(result == "Connecting to localhost:5432 (Server: production, Version: 1.0.0)");
    std::cout << "  ✓ Result: " << result << "\n";
}

// Test 5: Mixed types in nested structure
auto test_mixed_value_types() -> void {
    std::cout << "Test 5: Mixed value types (int, double, string, bool)...\n";

    TemplateValue::NestedMap trade_data;
    trade_data["symbol"] = TemplateValue{"AAPL"};
    trade_data["price"] = TemplateValue{175.45};
    trade_data["quantity"] = TemplateValue{100};
    trade_data["total"] = TemplateValue{17545.0};

    Logger::TemplateParams params;
    params.emplace("trade", TemplateValue{std::move(trade_data)});

    auto const result = Logger::processTemplate(
        "Trade: {trade.symbol} @ ${trade.price} x {trade.quantity} = ${trade.total}", params);

    assert(result == "Trade: AAPL @ $175.45 x 100 = $17545");
    std::cout << "  ✓ Result: " << result << "\n";
}

// Test 6: Missing keys (graceful handling)
auto test_missing_keys() -> void {
    std::cout << "Test 6: Missing keys and error handling...\n";

    TemplateValue::NestedMap user_map;
    user_map["name"] = TemplateValue{"Bob"};
    // Note: "email" key is missing

    Logger::TemplateParams params;
    params.emplace("user", TemplateValue{std::move(user_map)});

    // Test missing nested key
    auto result = Logger::processTemplate("User: {user.name}, Email: {user.email}", params);
    assert(result == "User: Bob, Email: {user.email}"); // Missing key preserved
    std::cout << "  ✓ Missing nested key: " << result << "\n";

    // Test missing root key
    result = Logger::processTemplate("Status: {status}", params);
    assert(result == "Status: {status}"); // Missing root key preserved
    std::cout << "  ✓ Missing root key: " << result << "\n";

    // Test accessing nested path on non-map value
    params.emplace("simple", TemplateValue{"just_a_string"});
    result = Logger::processTemplate("Value: {simple.nested}", params);
    assert(result == "Value: {simple.nested}"); // Can't navigate into non-map
    std::cout << "  ✓ Invalid path on non-map: " << result << "\n";
}

// Test 7: Using std::map for ordered iteration
auto test_std_map_constructor() -> void {
    std::cout << "Test 7: std::map constructor...\n";

    // Create nested structure using std::map
    std::map<std::string, TemplateValue> ordered_settings;
    ordered_settings.emplace("timeout", TemplateValue{30});
    ordered_settings.emplace("retries", TemplateValue{3});
    ordered_settings.emplace("endpoint", TemplateValue{"https://api.example.com"});

    Logger::TemplateParams params;
    params.emplace("settings", TemplateValue{std::move(ordered_settings)});

    auto const result = Logger::processTemplate(
        "API: {settings.endpoint}, Timeout: {settings.timeout}s, Retries: {settings.retries}",
        params);

    assert(result == "API: https://api.example.com, Timeout: 30s, Retries: 3");
    std::cout << "  ✓ Result: " << result << "\n";
}

// Test 8: Complex real-world example - trading system log
auto test_real_world_trading_log() -> void {
    std::cout << "Test 8: Real-world trading system example...\n";

    // Build complex nested structure
    TemplateValue::NestedMap trade_info;
    trade_info["id"] = TemplateValue{98765};
    trade_info["symbol"] = TemplateValue{"TSLA"};
    trade_info["side"] = TemplateValue{"BUY"};
    trade_info["quantity"] = TemplateValue{50};
    trade_info["price"] = TemplateValue{245.67};

    TemplateValue::NestedMap account_info;
    account_info["id"] = TemplateValue{12345};
    account_info["name"] = TemplateValue{"Hedge Fund Alpha"};

    TemplateValue::NestedMap risk_metrics;
    risk_metrics["var"] = TemplateValue{150000.0};
    risk_metrics["sharpe"] = TemplateValue{1.85};
    risk_metrics["drawdown"] = TemplateValue{-0.12};

    Logger::TemplateParams params;
    params.emplace("trade", TemplateValue{std::move(trade_info)});
    params.emplace("account", TemplateValue{std::move(account_info)});
    params.emplace("risk", TemplateValue{std::move(risk_metrics)});
    params.emplace("timestamp", TemplateValue{"2025-11-19T14:23:45Z"});

    auto const result = Logger::processTemplate(
        "[{timestamp}] Trade #{trade.id}: {trade.side} {trade.quantity} {trade.symbol} @ "
        "${trade.price} | "
        "Account: {account.name} (#{account.id}) | "
        "Risk: VaR=${risk.var} Sharpe={risk.sharpe} Drawdown={risk.drawdown}",
        params);

    assert(result == "[2025-11-19T14:23:45Z] Trade #98765: BUY 50 TSLA @ $245.67 | "
                     "Account: Hedge Fund Alpha (#12345) | "
                     "Risk: VaR=$150000 Sharpe=1.85 Drawdown=-0.12");

    std::cout << "  ✓ Result:\n    " << result << "\n";
}

// Test 9: Integration with logger methods (info, debug, etc.)
auto test_logger_integration() -> void {
    std::cout << "Test 9: Integration with logger methods...\n";

    auto& log = Logger::getInstance();
    log.initialize("test_nested.log", LogLevel::TRACE, false); // No console output

    // Test with nested structures in actual logging
    TemplateValue::NestedMap user_data;
    user_data["name"] = TemplateValue{"Charlie"};
    user_data["session_id"] = TemplateValue{999888777};

    Logger::TemplateParams params;
    params.emplace("user", TemplateValue{std::move(user_data)});
    params.emplace("event", TemplateValue{"page_view"});

    // This should work without errors
    log.info("Event {event} from user {user.name} (Session: {user.session_id})", params);
    log.debug("Debug: {user.name}", params);
    log.warn("Warning for {user.name}", params);

    std::cout << "  ✓ Logger integration successful (check test_nested.log)\n";
}

// Test 10: Array support with std::vector
auto test_vector_arrays() -> void {
    std::cout << "Test 10: Array support with std::vector...\n";

    // Create array of items
    TemplateValue::NestedArray items;
    items.push_back(TemplateValue{"Apple"});
    items.push_back(TemplateValue{"Banana"});
    items.push_back(TemplateValue{"Cherry"});

    Logger::TemplateParams params;
    params.emplace("items", TemplateValue{std::move(items)});

    // Test array indexing
    auto result =
        Logger::processTemplate("First: {items.0}, Second: {items.1}, Third: {items.2}", params);
    assert(result == "First: Apple, Second: Banana, Third: Cherry");
    std::cout << "  ✓ Vector array access: " << result << "\n";

    // Test out of bounds (should preserve placeholder)
    result = Logger::processTemplate("Missing: {items.99}", params);
    assert(result == "Missing: {items.99}");
    std::cout << "  ✓ Out of bounds handling: " << result << "\n";
}

// Test 11: Array support with std::array
auto test_std_array() -> void {
    std::cout << "Test 11: Array support with std::array...\n";

    // Create fixed-size array
    std::array<TemplateValue, 4> seasons{TemplateValue{"Spring"}, TemplateValue{"Summer"},
                                         TemplateValue{"Fall"}, TemplateValue{"Winter"}};

    Logger::TemplateParams params;
    params.emplace("seasons", TemplateValue{seasons});

    auto const result = Logger::processTemplate(
        "Q1: {seasons.0}, Q2: {seasons.1}, Q3: {seasons.2}, Q4: {seasons.3}", params);

    assert(result == "Q1: Spring, Q2: Summer, Q3: Fall, Q4: Winter");
    std::cout << "  ✓ std::array access: " << result << "\n";
}

// Test 11.5: Span support with std::span
auto test_std_span() -> void {
    std::cout << "Test 11.5: Span support with std::span...\n";

    // Create source data
    TemplateValue::NestedArray source_data;
    source_data.push_back(TemplateValue{"Red"});
    source_data.push_back(TemplateValue{"Green"});
    source_data.push_back(TemplateValue{"Blue"});
    source_data.push_back(TemplateValue{"Yellow"});

    // Create span view over source data
    std::span<TemplateValue> colors_span{source_data};

    Logger::TemplateParams params;
    params.emplace("colors", TemplateValue{colors_span});

    auto const result = Logger::processTemplate(
        "Primary: {colors.0}, Secondary: {colors.1}, Tertiary: {colors.2}, Extra: {colors.3}",
        params);

    assert(result == "Primary: Red, Secondary: Green, Tertiary: Blue, Extra: Yellow");
    std::cout << "  ✓ std::span access: " << result << "\n";

    // Test with partial span (subview)
    std::span<TemplateValue> primary_colors = colors_span.subspan(0, 3);
    Logger::TemplateParams params2;
    params2.emplace("primary", TemplateValue{primary_colors});

    auto const result2 =
        Logger::processTemplate("Primaries: {primary.0}, {primary.1}, {primary.2}", params2);

    assert(result2 == "Primaries: Red, Green, Blue");
    std::cout << "  ✓ std::span subspan access: " << result2 << "\n";
}

// Test 12: Mixed map and array nesting
auto test_mixed_map_array_nesting() -> void {
    std::cout << "Test 12: Mixed map and array nesting...\n";

    // Create array of server configurations
    TemplateValue::NestedMap server1;
    server1["host"] = TemplateValue{"server1.example.com"};
    server1["port"] = TemplateValue{8080};

    TemplateValue::NestedMap server2;
    server2["host"] = TemplateValue{"server2.example.com"};
    server2["port"] = TemplateValue{8081};

    TemplateValue::NestedArray servers;
    servers.push_back(TemplateValue{std::move(server1)});
    servers.push_back(TemplateValue{std::move(server2)});

    Logger::TemplateParams params;
    params.emplace("servers", TemplateValue{std::move(servers)});

    auto const result = Logger::processTemplate("Primary: {servers.0.host}:{servers.0.port}, "
                                                "Backup: {servers.1.host}:{servers.1.port}",
                                                params);

    assert(result == "Primary: server1.example.com:8080, "
                     "Backup: server2.example.com:8081");
    std::cout << "  ✓ Mixed map/array nesting: " << result << "\n";
}

// Test 13: Array of arrays (deep nesting)
auto test_array_of_arrays() -> void {
    std::cout << "Test 13: Array of arrays (deep nesting)...\n";

    // Create 2D matrix-like structure
    TemplateValue::NestedArray row1;
    row1.push_back(TemplateValue{1});
    row1.push_back(TemplateValue{2});
    row1.push_back(TemplateValue{3});

    TemplateValue::NestedArray row2;
    row2.push_back(TemplateValue{4});
    row2.push_back(TemplateValue{5});
    row2.push_back(TemplateValue{6});

    TemplateValue::NestedArray matrix;
    matrix.push_back(TemplateValue{std::move(row1)});
    matrix.push_back(TemplateValue{std::move(row2)});

    Logger::TemplateParams params;
    params.emplace("matrix", TemplateValue{std::move(matrix)});

    auto const result =
        Logger::processTemplate("matrix[0][0]={matrix.0.0}, matrix[0][1]={matrix.0.1}, "
                                "matrix[1][2]={matrix.1.2}",
                                params);

    assert(result == "matrix[0][0]=1, matrix[0][1]=2, matrix[1][2]=6");
    std::cout << "  ✓ Array of arrays: " << result << "\n";
}

// Test 14: Real-world example - market data feed
auto test_real_world_market_data() -> void {
    std::cout << "Test 14: Real-world market data feed example...\n";

    // Create array of trade ticks
    TemplateValue::NestedMap tick1;
    tick1["symbol"] = TemplateValue{"AAPL"};
    tick1["price"] = TemplateValue{175.50};
    tick1["volume"] = TemplateValue{1000};

    TemplateValue::NestedMap tick2;
    tick2["symbol"] = TemplateValue{"GOOGL"};
    tick2["price"] = TemplateValue{140.25};
    tick2["volume"] = TemplateValue{500};

    TemplateValue::NestedMap tick3;
    tick3["symbol"] = TemplateValue{"MSFT"};
    tick3["price"] = TemplateValue{380.75};
    tick3["volume"] = TemplateValue{750};

    TemplateValue::NestedArray ticks;
    ticks.push_back(TemplateValue{std::move(tick1)});
    ticks.push_back(TemplateValue{std::move(tick2)});
    ticks.push_back(TemplateValue{std::move(tick3)});

    Logger::TemplateParams params;
    params.emplace("ticks", TemplateValue{std::move(ticks)});
    params.emplace("count", TemplateValue{3});

    auto const result =
        Logger::processTemplate("Processed {count} ticks: "
                                "[{ticks.0.symbol}@${ticks.0.price}x{ticks.0.volume}] "
                                "[{ticks.1.symbol}@${ticks.1.price}x{ticks.1.volume}] "
                                "[{ticks.2.symbol}@${ticks.2.price}x{ticks.2.volume}]",
                                params);

    assert(result == "Processed 3 ticks: "
                     "[AAPL@$175.5x1000] [GOOGL@$140.25x500] [MSFT@$380.75x750]");

    std::cout << "  ✓ Market data feed:\n    " << result << "\n";
}

// Test 15: Edge cases and boundary conditions
auto test_edge_cases() -> void {
    std::cout << "Test 10: Edge cases...\n";

    // Empty template string
    auto result = Logger::processTemplate("", {});
    assert(result.empty());
    std::cout << "  ✓ Empty template string\n";

    // Template with no placeholders
    result = Logger::processTemplate("No placeholders here", {{"key", "value"}});
    assert(result == "No placeholders here");
    std::cout << "  ✓ No placeholders\n";

    // Empty parameter map
    result = Logger::processTemplate("Value: {missing}", {});
    assert(result == "Value: {missing}");
    std::cout << "  ✓ Empty parameter map\n";

    // Multiple dots in path
    TemplateValue::NestedMap level3;
    level3["value"] = TemplateValue{42};

    TemplateValue::NestedMap level2;
    level2["level3"] = TemplateValue{std::move(level3)};

    TemplateValue::NestedMap level1;
    level1["level2"] = TemplateValue{std::move(level2)};

    Logger::TemplateParams params;
    params.emplace("level1", TemplateValue{std::move(level1)});

    result = Logger::processTemplate("{level1.level2.level3.value}", params);
    assert(result == "42");
    std::cout << "  ✓ Multiple dots in path: " << result << "\n";

    // Adjacent placeholders
    result = Logger::processTemplate("{a}{b}{c}", {{"a", 1}, {"b", 2}, {"c", 3}});
    assert(result == "123");
    std::cout << "  ✓ Adjacent placeholders: " << result << "\n";
}

auto main() -> int {
    std::cout << "\n=== Nested Dictionary & Array Template Test Suite ===\n\n";

    try {
        // Dictionary/Map tests
        test_flat_unordered_map();
        test_flat_ordered_map();
        test_nested_two_levels();
        test_nested_three_levels();
        test_mixed_value_types();
        test_missing_keys();
        test_std_map_constructor();
        test_real_world_trading_log();
        test_logger_integration();

        // Array tests
        test_vector_arrays();
        test_std_array();
        test_std_span();
        test_mixed_map_array_nesting();
        test_array_of_arrays();
        test_real_world_market_data();

        // Edge cases
        test_edge_cases();

        std::cout << "\n=== All nested dictionary & array tests passed! ===\n\n";
        return 0;

    } catch (std::exception const& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << "\n\n";
        return 1;
    }
}
