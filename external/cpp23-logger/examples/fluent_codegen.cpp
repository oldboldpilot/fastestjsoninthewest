/**
 * Fluent API Code Generation Example
 *
 * Demonstrates the powerful fluent API with method chaining for code generation
 */

import logger.code_generator;

#include <iostream>
#include <string>

using namespace logger::codegen;

auto main() -> int {
    std::cout << "=============================================================================\n";
    std::cout << "          Fluent API Code Generation Demonstration\n";
    std::cout << "=============================================================================\n\n";

    // =========================================================================
    // Example 1: Method Chaining with setTemplate().setData().render()
    // =========================================================================

    std::cout << "Example 1: Basic Fluent API\n";
    std::cout << "----------------------------\n";

    CodeGenerator gen;

    auto result1 = gen
        .setTemplate("class {{name | PascalCase}} { /* {{comment}} */ };")
        .setData(TemplateObject{
            {"name", TemplateValue{"user_manager"}},
            {"comment", TemplateValue{"Auto-generated class"}}
        })
        .render();

    std::cout << "Generated: " << result1 << "\n\n";

    // =========================================================================
    // Example 2: Incremental Data Building with addData()
    // =========================================================================

    std::cout << "Example 2: Incremental Data Building\n";
    std::cout << "-------------------------------------\n";

    auto result2 = gen
        .reset()  // Clear previous template and data
        .setTemplate("Function: {{return_type}} {{name | camelCase}}({{param_type}} {{param_name}});")
        .addData("return_type", TemplateValue{"void"})
        .addData("name", TemplateValue{"update_user"})
        .addData("param_type", TemplateValue{"std::string const&"})
        .addData("param_name", TemplateValue{"username"})
        .render();

    std::cout << "Generated: " << result2 << "\n\n";

    // =========================================================================
    // Example 3: Custom Filter Registration with Chaining
    // =========================================================================

    std::cout << "Example 3: Custom Filter Chaining\n";
    std::cout << "----------------------------------\n";

    auto result3 = gen
        .reset()
        .registerFilter("reverse", [](std::string_view s) -> std::string {
            std::string result{s};
            std::reverse(result.begin(), result.end());
            return result;
        })
        .registerFilter("addEmoji", [](std::string_view s) -> std::string {
            return std::string(s) + " 🚀";
        })
        .setTemplate("Original: {{text}}, Reversed: {{text | reverse}}, With Emoji: {{text | addEmoji}}")
        .addData("text", TemplateValue{"Hello"})
        .render();

    std::cout << "Generated: " << result3 << "\n\n";

    // =========================================================================
    // Example 4: SQL Generation with Fluent API
    // =========================================================================

    std::cout << "Example 4: SQL Table Generation\n";
    std::cout << "--------------------------------\n";

    auto sql_result = gen
        .reset()
        .setTemplate(R"(CREATE TABLE {{table_name | snake_case}} (
    id INTEGER PRIMARY KEY,
    {{field1_name | snake_case}} {{field1_type}},
    {{field2_name | snake_case}} {{field2_type}}
);)")
        .addData("table_name", TemplateValue{"UserAccount"})
        .addData("field1_name", TemplateValue{"userName"})
        .addData("field1_type", TemplateValue{"VARCHAR(255)"})
        .addData("field2_name", TemplateValue{"accountBalance"})
        .addData("field2_type", TemplateValue{"DECIMAL(10, 2)"})
        .render();

    std::cout << "Generated SQL:\n" << sql_result << "\n\n";

    // =========================================================================
    // Example 5: Python Function Generation
    // =========================================================================

    std::cout << "Example 5: Python Function Generation\n";
    std::cout << "--------------------------------------\n";

    auto py_result = gen
        .reset()
        .setTemplate(R"(def {{func_name | snake_case}}({{param1}}: {{type1}}, {{param2}}: {{type2}}) -> {{return_type}}:
    {{docstring | py_comment}}
    return {{param1}} + {{param2}})")
        .addData("func_name", TemplateValue{"AddNumbers"})
        .addData("param1", TemplateValue{"x"})
        .addData("type1", TemplateValue{"int"})
        .addData("param2", TemplateValue{"y"})
        .addData("type2", TemplateValue{"int"})
        .addData("return_type", TemplateValue{"int"})
        .addData("docstring", TemplateValue{"Add two numbers together"})
        .render();

    std::cout << "Generated Python:\n" << py_result << "\n\n";

    // =========================================================================
    // Example 6: JSON Configuration Generation
    // =========================================================================

    std::cout << "Example 6: JSON Configuration\n";
    std::cout << "------------------------------\n";

    auto json_result = gen
        .reset()
        .setTemplate(R"({
    "name": "{{name | escape}}",
    "version": "{{version}}",
    "author": "{{author | escape}}",
    "description": "{{description | escape}}"
})")
        .addData("name", TemplateValue{"MyProject"})
        .addData("version", TemplateValue{"1.0.0"})
        .addData("author", TemplateValue{"Olumuyiwa Oluwasanmi"})
        .addData("description", TemplateValue{"A powerful code generation library"})
        .render();

    std::cout << "Generated JSON:\n" << json_result << "\n\n";

    // =========================================================================
    // Example 7: C++ Header Guard Generation
    // =========================================================================

    std::cout << "Example 7: C++ Header Guard\n";
    std::cout << "----------------------------\n";

    auto header_result = gen
        .reset()
        .setTemplate(R"(#ifndef {{guard_name | SCREAMING_SNAKE_CASE}}_H
#define {{guard_name | SCREAMING_SNAKE_CASE}}_H

namespace {{namespace_name | snake_case}} {

class {{class_name | PascalCase}} {
    // Implementation
};

} // namespace {{namespace_name | snake_case}}

#endif // {{guard_name | SCREAMING_SNAKE_CASE}}_H)")
        .addData("guard_name", TemplateValue{"UserManager"})
        .addData("namespace_name", TemplateValue{"myProject"})
        .addData("class_name", TemplateValue{"user_manager"})
        .render();

    std::cout << "Generated Header:\n" << header_result << "\n\n";

    // =========================================================================
    // Example 8: Nested Object Access
    // =========================================================================

    std::cout << "Example 8: Nested Object Access\n";
    std::cout << "--------------------------------\n";

    TemplateObject nested_data{
        {"project", TemplateValue{TemplateObject{
            {"name", TemplateValue{"BigBrother"}},
            {"config", TemplateValue{TemplateObject{
                {"version", TemplateValue{"2.0"}},
                {"author", TemplateValue{"Olumuyiwa"}}
            }}}
        }}}
    };

    auto nested_result = gen
        .reset()
        .setTemplate("Project: {{project.name}}, Version: {{project.config.version}}, Author: {{project.config.author}}")
        .setData(nested_data)
        .render();

    std::cout << "Generated: " << nested_result << "\n\n";

    // =========================================================================
    // Example 9: Multiple Filter Application
    // =========================================================================

    std::cout << "Example 9: String Transformation Showcase\n";
    std::cout << "------------------------------------------\n";

    std::string base_string = "hello_world";

    std::cout << "Original: " << base_string << "\n";
    std::cout << "camelCase: " << toCamelCase(base_string) << "\n";
    std::cout << "PascalCase: " << toPascalCase(base_string) << "\n";
    std::cout << "kebab-case: " << toKebabCase(base_string) << "\n";
    std::cout << "SCREAMING_SNAKE_CASE: " << toScreamingSnakeCase(base_string) << "\n\n";

    // =========================================================================
    // Example 10: Stateless vs Stateful API
    // =========================================================================

    std::cout << "Example 10: Stateless vs Stateful API\n";
    std::cout << "--------------------------------------\n";

    // Stateless API (single call)
    TemplateObject stateless_data{{"name", TemplateValue{"stateless"}}};
    auto stateless_result = gen.render("Name: {{name | uppercase}}", stateless_data);
    std::cout << "Stateless: " << stateless_result << "\n";

    // Stateful API (fluent chaining)
    auto stateful_result = gen
        .reset()
        .setTemplate("Name: {{name | uppercase}}")
        .addData("name", TemplateValue{"stateful"})
        .render();
    std::cout << "Stateful: " << stateful_result << "\n\n";

    std::cout << "=============================================================================\n";
    std::cout << "All fluent API examples completed successfully!\n";
    std::cout << "=============================================================================\n";

    return 0;
}
