/**
 * Comprehensive test suite for functional template features
 *
 * Tests:
 * - Section iteration ({{#name}}...{{/name}})
 * - Context variables (@index, @first, @last, @key, @value)
 * - Filter operations (reverse, slice, zip)
 * - Functional operations (all, none, not)
 * - JSON construction helpers
 * - Nested sections
 */

import logger;

#include <cassert>
#include <iostream>
#include <string>

using namespace logger;

// Test 1: Basic array iteration
auto test_array_iteration() -> void {
    std::cout << "Testing basic array iteration...\n";

    TemplateValue::NestedArray colors;
    colors.push_back(TemplateValue{"red"});
    colors.push_back(TemplateValue{"green"});
    colors.push_back(TemplateValue{"blue"});

    TemplateParams params;
    params["colors"] = TemplateValue{std::move(colors)};

    auto const result = Logger::processTemplate("Colors: {{#colors}}{@value} {{/colors}}", params);
    std::cout << "  Result: " << result << "\n";
    assert(result == "Colors: red green blue ");

    std::cout << "  ✓ Basic array iteration test passed\n";
}

// Test 2: Array iteration with @index
auto test_array_iteration_with_index() -> void {
    std::cout << "Testing array iteration with @index...\n";

    TemplateValue::NestedArray items;
    items.push_back(TemplateValue{"apple"});
    items.push_back(TemplateValue{"banana"});
    items.push_back(TemplateValue{"cherry"});

    TemplateParams params;
    params["items"] = TemplateValue{std::move(items)};

    auto const result = Logger::processTemplate("{{#items}}[{@index}]={@value} {{/items}}", params);
    std::cout << "  Result: " << result << "\n";
    assert(result == "[0]=apple [1]=banana [2]=cherry ");

    std::cout << "  ✓ Array iteration with @index test passed\n";
}

// Test 3: Array iteration with @first and @last
auto test_array_iteration_with_first_last() -> void {
    std::cout << "Testing array iteration with @first and @last...\n";

    TemplateValue::NestedArray items;
    items.push_back(TemplateValue{"A"});
    items.push_back(TemplateValue{"B"});
    items.push_back(TemplateValue{"C"});

    TemplateParams params;
    params["items"] = TemplateValue{std::move(items)};

    auto const result = Logger::processTemplate(
        "{{#items}}{@value}(first={@first},last={@last}) {{/items}}", params);
    std::cout << "  Result: " << result << "\n";
    assert(result ==
           "A(first=true,last=false) B(first=false,last=false) C(first=false,last=true) ");

    std::cout << "  ✓ Array iteration with @first/@last test passed\n";
}

// Test 4: Map iteration with @key and @value
auto test_map_iteration() -> void {
    std::cout << "Testing map iteration with @key and @value...\n";

    TemplateValue::NestedMap config;
    config["host"] = TemplateValue{"localhost"};
    config["port"] = TemplateValue{"8080"};
    config["timeout"] = TemplateValue{"30"};

    TemplateParams params;
    params["config"] = TemplateValue{std::move(config)};

    auto const result = Logger::processTemplate("{{#config}}{@key}={@value} {{/config}}", params);
    std::cout << "  Result: " << result << "\n";
    // Note: map iteration order may vary, so we just check it contains expected pairs
    assert(result.find("host=localhost") != std::string::npos);
    assert(result.find("port=8080") != std::string::npos);
    assert(result.find("timeout=30") != std::string::npos);

    std::cout << "  ✓ Map iteration test passed\n";
}

// Test 5: Nested sections
auto test_nested_sections() -> void {
    std::cout << "Testing nested sections...\n";

    // Create outer array of maps
    TemplateValue::NestedArray users;

    TemplateValue::NestedMap user1;
    user1["name"] = TemplateValue{"Alice"};
    TemplateValue::NestedArray skills1;
    skills1.push_back(TemplateValue{"C++"});
    skills1.push_back(TemplateValue{"Python"});
    user1["skills"] = TemplateValue{std::move(skills1)};
    users.push_back(TemplateValue{std::move(user1)});

    TemplateValue::NestedMap user2;
    user2["name"] = TemplateValue{"Bob"};
    TemplateValue::NestedArray skills2;
    skills2.push_back(TemplateValue{"Java"});
    skills2.push_back(TemplateValue{"Rust"});
    user2["skills"] = TemplateValue{std::move(skills2)};
    users.push_back(TemplateValue{std::move(user2)});

    TemplateParams params;
    params["users"] = TemplateValue{std::move(users)};

    auto const result = Logger::processTemplate(
        "{{#users}}{@value.name}: {{#@value.skills}}{@value} {{/@value.skills}}\n{{/users}}",
        params);
    std::cout << "  Result:\n" << result;
    assert(result.find("Alice: C++ Python") != std::string::npos);
    assert(result.find("Bob: Java Rust") != std::string::npos);

    std::cout << "  ✓ Nested sections test passed\n";
}

// Test 6: Reverse filter
auto test_reverse_filter() -> void {
    std::cout << "Testing reverse filter...\n";

    TemplateValue::NestedArray numbers;
    numbers.push_back(TemplateValue{"1"});
    numbers.push_back(TemplateValue{"2"});
    numbers.push_back(TemplateValue{"3"});

    auto const original = TemplateValue{numbers};
    auto const reversed = Logger::reverse(original);

    TemplateParams params;
    params["numbers"] = reversed;

    auto const result = Logger::processTemplate("{{#numbers}}{@value} {{/numbers}}", params);
    std::cout << "  Result: " << result << "\n";
    assert(result == "3 2 1 ");

    std::cout << "  ✓ Reverse filter test passed\n";
}

// Test 7: Slice filter
auto test_slice_filter() -> void {
    std::cout << "Testing slice filter...\n";

    TemplateValue::NestedArray items;
    items.push_back(TemplateValue{"A"});
    items.push_back(TemplateValue{"B"});
    items.push_back(TemplateValue{"C"});
    items.push_back(TemplateValue{"D"});
    items.push_back(TemplateValue{"E"});

    auto const original = TemplateValue{items};
    auto const sliced = Logger::slice(original, 1, 4); // Extract B, C, D

    TemplateParams params;
    params["items"] = sliced;

    auto const result = Logger::processTemplate("{{#items}}{@value} {{/items}}", params);
    std::cout << "  Result: " << result << "\n";
    assert(result == "B C D ");

    std::cout << "  ✓ Slice filter test passed\n";
}

// Test 8: Zip operation
auto test_zip_operation() -> void {
    std::cout << "Testing zip operation...\n";

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

    TemplateParams params;
    params["pairs"] = zipped;

    auto const result =
        Logger::processTemplate("{{#pairs}}{@value.first}:{@value.second} {{/pairs}}", params);
    std::cout << "  Result: " << result << "\n";
    assert(result == "Alice:25 Bob:30 Charlie:35 ");

    std::cout << "  ✓ Zip operation test passed\n";
}

// Test 9: All operation
auto test_all_operation() -> void {
    std::cout << "Testing all operation...\n";

    // Test with all truthy values
    TemplateValue::NestedArray truthy;
    truthy.push_back(TemplateValue{"yes"});
    truthy.push_back(TemplateValue{"true"});
    truthy.push_back(TemplateValue{"1"});

    auto const truthy_val = TemplateValue{truthy};
    auto const all_truthy = Logger::all(truthy_val);
    assert(all_truthy.toString() == "true");

    // Test with one falsy value
    TemplateValue::NestedArray mixed;
    mixed.push_back(TemplateValue{"yes"});
    mixed.push_back(TemplateValue{""});
    mixed.push_back(TemplateValue{"true"});

    auto const mixed_val = TemplateValue{mixed};
    auto const all_mixed = Logger::all(mixed_val);
    assert(all_mixed.toString() == "false");

    std::cout << "  ✓ All operation test passed\n";
}

// Test 10: None operation
auto test_none_operation() -> void {
    std::cout << "Testing none operation...\n";

    // Test with all falsy values
    TemplateValue::NestedArray falsy;
    falsy.push_back(TemplateValue{""});
    falsy.push_back(TemplateValue{"false"});
    falsy.push_back(TemplateValue{"0"});

    auto const falsy_val = TemplateValue{falsy};
    auto const none_falsy = Logger::none(falsy_val);
    assert(none_falsy.toString() == "true");

    // Test with one truthy value
    TemplateValue::NestedArray mixed;
    mixed.push_back(TemplateValue{""});
    mixed.push_back(TemplateValue{"yes"});
    mixed.push_back(TemplateValue{"false"});

    auto const mixed_val = TemplateValue{mixed};
    auto const none_mixed = Logger::none(mixed_val);
    assert(none_mixed.toString() == "false");

    std::cout << "  ✓ None operation test passed\n";
}

// Test 11: Not operation
auto test_not_operation() -> void {
    std::cout << "Testing not operation...\n";

    auto const true_val = TemplateValue{"true"};
    auto const not_true = Logger::not_(true_val);
    assert(not_true.toString() == "false");

    auto const false_val = TemplateValue{"false"};
    auto const not_false = Logger::not_(false_val);
    assert(not_false.toString() == "true");

    auto const empty_val = TemplateValue{""};
    auto const not_empty = Logger::not_(empty_val);
    assert(not_empty.toString() == "true");

    std::cout << "  ✓ Not operation test passed\n";
}

// Test 12: JSON construction helpers
auto test_json_helpers() -> void {
    std::cout << "Testing JSON construction helpers...\n";

    // Create JSON object using helper
    auto const user = Logger::json({{"name", TemplateValue{"Alice"}},
                                    {"age", TemplateValue{"30"}},
                                    {"city", TemplateValue{"NYC"}}});

    TemplateParams params;
    params["user"] = user;

    auto const result =
        Logger::processTemplate("Name: {user.name}, Age: {user.age}, City: {user.city}", params);
    std::cout << "  Result: " << result << "\n";
    assert(result == "Name: Alice, Age: 30, City: NYC");

    std::cout << "  ✓ JSON helpers test passed\n";
}

// Test 13: JSON array helper
auto test_json_array_helper() -> void {
    std::cout << "Testing JSON array helper...\n";

    auto const colors =
        Logger::jsonArray({TemplateValue{"red"}, TemplateValue{"green"}, TemplateValue{"blue"}});

    TemplateParams params;
    params["colors"] = colors;

    auto const result = Logger::processTemplate("Colors: {{#colors}}{@value} {{/colors}}", params);
    std::cout << "  Result: " << result << "\n";
    assert(result == "Colors: red green blue ");

    std::cout << "  ✓ JSON array helper test passed\n";
}

// Test 14: Complex real-world example
auto test_complex_real_world() -> void {
    std::cout << "Testing complex real-world example...\n";

    // Trading log with multiple trades
    auto const trades = Logger::jsonArray({
        Logger::json({{"symbol", TemplateValue{"AAPL"}},
                      {"action", TemplateValue{"BUY"}},
                      {"quantity", TemplateValue{"100"}},
                      {"price", TemplateValue{"150.25"}}}),
        Logger::json({{"symbol", TemplateValue{"GOOGL"}},
                      {"action", TemplateValue{"SELL"}},
                      {"quantity", TemplateValue{"50"}},
                      {"price", TemplateValue{"2800.50"}}}),
        Logger::json({{"symbol", TemplateValue{"MSFT"}},
                      {"action", TemplateValue{"BUY"}},
                      {"quantity", TemplateValue{"75"}},
                      {"price", TemplateValue{"380.75"}}}),
    });

    TemplateParams params;
    params["trades"] = trades;

    auto const result = Logger::processTemplate(
        "Trading Activity:\n"
        "{{#trades}}[{@index}] {@value.action} {@value.quantity} {@value.symbol} @ "
        "${@value.price}\n{{/trades}}",
        params);

    std::cout << "  Result:\n" << result;
    assert(result.find("[0] BUY 100 AAPL @ $150.25") != std::string::npos);
    assert(result.find("[1] SELL 50 GOOGL @ $2800.50") != std::string::npos);
    assert(result.find("[2] BUY 75 MSFT @ $380.75") != std::string::npos);

    std::cout << "  ✓ Complex real-world test passed\n";
}

// Test 15: Combined filters and operations
auto test_combined_operations() -> void {
    std::cout << "Testing combined filters and operations...\n";

    // Create array, reverse it, slice it
    TemplateValue::NestedArray numbers;
    for (int i = 1; i <= 10; ++i) {
        numbers.push_back(TemplateValue{std::to_string(i)});
    }

    auto const original = TemplateValue{numbers};
    auto const reversed = Logger::reverse(original);
    auto const sliced = Logger::slice(reversed, 2, 7); // Get middle 5 elements of reversed

    TemplateParams params;
    params["processed"] = sliced;

    auto const result = Logger::processTemplate("{{#processed}}{@value} {{/processed}}", params);
    std::cout << "  Result: " << result << "\n";
    assert(result == "8 7 6 5 4 ");

    std::cout << "  ✓ Combined operations test passed\n";
}

// Test 16: Logger integration with sections
auto test_logger_integration_with_sections() -> void {
    std::cout << "Testing logger integration with sections...\n";

    auto& log = Logger::getInstance();
    log.initialize("test_functional.log", LogLevel::INFO, false);

    // Create structured log data
    auto const servers = Logger::jsonArray({
        Logger::json({{"name", TemplateValue{"web-01"}},
                      {"status", TemplateValue{"healthy"}},
                      {"cpu", TemplateValue{"45%"}}}),
        Logger::json({{"name", TemplateValue{"web-02"}},
                      {"status", TemplateValue{"warning"}},
                      {"cpu", TemplateValue{"85%"}}}),
    });

    TemplateParams params;
    params["servers"] = servers;

    log.info("Server Status Report:\n{{#servers}}- {@value.name}: {@value.status} (CPU: "
             "{@value.cpu})\n{{/servers}}",
             params);

    std::cout << "  ✓ Logger integration with sections test passed\n";
}

auto main() -> int {
    std::cout << "\n=== Functional Template System Test Suite ===\n\n";

    try {
        test_array_iteration();
        test_array_iteration_with_index();
        test_array_iteration_with_first_last();
        test_map_iteration();
        test_nested_sections();
        test_reverse_filter();
        test_slice_filter();
        test_zip_operation();
        test_all_operation();
        test_none_operation();
        test_not_operation();
        test_json_helpers();
        test_json_array_helper();
        test_complex_real_world();
        test_combined_operations();
        test_logger_integration_with_sections();

        std::cout << "\n=== All functional template tests passed! ===\n\n";
        return 0;

    } catch (std::exception const& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << "\n\n";
        return 1;
    }
}
