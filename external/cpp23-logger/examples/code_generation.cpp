/**
 * Code Generation Examples for cpp23-logger
 *
 * Demonstrates:
 * - C++ class/function generation
 * - Python module generation
 * - SQL schema generation
 * - JSON/YAML configuration generation
 * - String filters and transformations
 * - Code formatters and indentation
 */

import logger.code_generator;

#include <iostream>
#include <string>

using namespace logger::codegen;

auto main() -> int {
    CodeGenerator gen;

    std::cout << "=============================================================================\n";
    std::cout << "                  Code Generation Examples\n";
    std::cout << "=============================================================================\n\n";

    // =========================================================================
    // Example 1: C++ Class Generation
    // =========================================================================

    std::cout << "Example 1: C++ Class Generation\n";
    std::cout << "--------------------------------\n";

    auto cpp_class_template = R"(
class {{class_name | PascalCase}} {
public:
    // Constructor
    {{class_name | PascalCase}}();

    // Destructor
    ~{{class_name | PascalCase}}() = default;

    // Getters
    {{#each getters}}
    [[nodiscard]] auto get{{name | PascalCase}}() const noexcept -> {{type}} { return {{name | camelCase}}_; }
    {{/each}}

    // Setters
    {{#each setters}}
    auto set{{name | PascalCase}}({{type}} value) -> void { {{name | camelCase}}_ = value; }
    {{/each}}

private:
    {{#each members}}
    {{type}} {{name | camelCase}}_;
    {{/each}}
};
)";

    TemplateObject cpp_data{
        {"class_name", TemplateValue{"user_account"}},
        {"getters", TemplateValue{TemplateArray{
            TemplateValue{TemplateObject{{"name", "user_id"}, {"type", "int"}}},
            TemplateValue{TemplateObject{{"name", "username"}, {"type", "std::string"}}},
            TemplateValue{TemplateObject{{"name", "balance"}, {"type", "double"}}}
        }}},
        {"setters", TemplateValue{TemplateArray{
            TemplateValue{TemplateObject{{"name", "username"}, {"type", "std::string const&"}}},
            TemplateValue{TemplateObject{{"name", "balance"}, {"type", "double"}}}
        }}},
        {"members", TemplateValue{TemplateArray{
            TemplateValue{TemplateObject{{"type", "int"}, {"name", "user_id"}}},
            TemplateValue{TemplateObject{{"type", "std::string"}, {"name", "username"}}},
            TemplateValue{TemplateObject{{"type", "double"}, {"name", "balance"}}}
        }}}
    };

    // NOTE: Full conditional/loop support not yet implemented
    // This example shows the template structure

    std::cout << "Template structure created (full rendering requires loop implementation)\n\n";

    // =========================================================================
    // Example 2: String Filter Demonstrations
    // =========================================================================

    std::cout << "Example 2: String Filter Demonstrations\n";
    std::cout << "----------------------------------------\n";

    TemplateObject filter_data{
        {"original", TemplateValue{"hello_world"}},
        {"camelCase", TemplateValue{toCamelCase("hello_world")}},
        {"PascalCase", TemplateValue{toPascalCase("hello_world")}},
        {"snake_case", TemplateValue{toSnakeCase("HelloWorld")}},
        {"kebab-case", TemplateValue{toKebabCase("HelloWorld")}},
        {"SCREAMING", TemplateValue{toScreamingSnakeCase("HelloWorld")}},
        {"uppercase", TemplateValue{toUpperCase("hello")}},
        {"lowercase", TemplateValue{toLowerCase("WORLD")}}
    };

    std::cout << "Original: " << filter_data["original"].asString() << "\n";
    std::cout << "camelCase: " << filter_data["camelCase"].asString() << "\n";
    std::cout << "PascalCase: " << filter_data["PascalCase"].asString() << "\n";
    std::cout << "snake_case: " << filter_data["snake_case"].asString() << "\n";
    std::cout << "kebab-case: " << filter_data["kebab-case"].asString() << "\n";
    std::cout << "SCREAMING_SNAKE_CASE: " << filter_data["SCREAMING"].asString() << "\n";
    std::cout << "UPPERCASE: " << filter_data["uppercase"].asString() << "\n";
    std::cout << "lowercase: " << filter_data["lowercase"].asString() << "\n\n";

    // =========================================================================
    // Example 3: Code Formatters (indent, wrap, escape)
    // =========================================================================

    std::cout << "Example 3: Code Formatters\n";
    std::cout << "--------------------------\n";

    std::string code_block = "def hello():\nprint('world')\nreturn 42";
    std::cout << "Original code:\n" << code_block << "\n\n";

    std::cout << "Indented 4 spaces:\n" << indent(code_block, 4) << "\n\n";

    std::string long_text = "This is a very long line of text that should be wrapped to fit within a specific width constraint for better readability in code comments or documentation.";
    std::cout << "Original text:\n" << long_text << "\n\n";
    std::cout << "Wrapped to 40 chars:\n" << wrap(long_text, 40) << "\n\n";

    std::string special_chars = "Line 1\nLine 2\tTabbed\n\"Quoted\"\\Backslash";
    std::cout << "Original string:\n" << special_chars << "\n\n";
    std::cout << "Escaped:\n" << escape(special_chars) << "\n\n";

    // =========================================================================
    // Example 4: Python Function Generation Template
    // =========================================================================

    std::cout << "Example 4: Python Function Template Structure\n";
    std::cout << "----------------------------------------------\n";

    auto py_function_template = R"(
def {{name | snake_case}}({{params}}):
    """{{description | wrap80}}"""
    {{#if validate}}
    # Input validation
    {{validation_code | indent4}}
    {{/if}}

    {{body | indent4}}

    return {{return_value}}
)";

    std::cout << "Python function template created\n";
    std::cout << "(Full rendering requires conditional/loop implementation)\n\n";

    // =========================================================================
    // Example 5: SQL Table Generation Template
    // =========================================================================

    std::cout << "Example 5: SQL Schema Template Structure\n";
    std::cout << "-----------------------------------------\n";

    auto sql_template = R"(
CREATE TABLE {{table_name | snake_case}} (
    {{#each columns}}
    {{name | snake_case}} {{type}}{{#if not_null}} NOT NULL{{/if}}{{#if primary_key}} PRIMARY KEY{{/if}}{{#unless @last}},{{/unless}}
    {{/each}}
);

CREATE INDEX idx_{{table_name}}_{{index_column}} ON {{table_name | snake_case}}({{index_column | snake_case}});
)";

    std::cout << "SQL table template created\n\n";

    // =========================================================================
    // Example 6: Comment Style Transformations
    // =========================================================================

    std::cout << "Example 6: Comment Style Transformations\n";
    std::cout << "-----------------------------------------\n";

    std::string comment_text = "This is a comment";

    std::cout << "Original: " << comment_text << "\n";
    std::cout << "C++ style: " << cppComment(comment_text) << "\n";
    std::cout << "C style: " << cComment(comment_text) << "\n";
    std::cout << "Python style: " << pythonComment(comment_text) << "\n\n";

    // =========================================================================
    // Example 7: Simple Template Rendering (No loops/conditionals yet)
    // =========================================================================

    std::cout << "Example 7: Simple Template Rendering\n";
    std::cout << "-------------------------------------\n";

    auto simple_template = "Class name: {{class_name | PascalCase}}, Method: {{method_name | camelCase}}";

    TemplateObject simple_data{
        {"class_name", TemplateValue{"database_connection"}},
        {"method_name", TemplateValue{"execute_query"}}
    };

    auto rendered = gen.render(simple_template, simple_data);
    std::cout << "Template: " << simple_template << "\n";
    std::cout << "Rendered: " << rendered << "\n\n";

    // =========================================================================
    // Example 8: Nested Property Access
    // =========================================================================

    std::cout << "Example 8: Nested Property Access\n";
    std::cout << "----------------------------------\n";

    TemplateObject user_data{
        {"user", TemplateValue{TemplateObject{
            {"name", TemplateValue{"Alice"}},
            {"id", TemplateValue{12345}},
            {"profile", TemplateValue{TemplateObject{
                {"email", TemplateValue{"alice@example.com"}},
                {"role", TemplateValue{"admin"}}
            }}}
        }}}
    };

    auto nested_template = "User: {{user.name}}, Email: {{user.profile.email}}, Role: {{user.profile.role | uppercase}}";
    auto nested_result = gen.render(nested_template, user_data);

    std::cout << "Template: " << nested_template << "\n";
    std::cout << "Rendered: " << nested_result << "\n\n";

    // =========================================================================
    // Example 9: Custom Filter Registration
    // =========================================================================

    std::cout << "Example 9: Custom Filter Registration\n";
    std::cout << "--------------------------------------\n";

    // Register custom filter: reverse string
    gen.registerFilter("reverse", [](std::string_view s) -> std::string {
        std::string result{s};
        std::reverse(result.begin(), result.end());
        return result;
    });

    // Register custom filter: first N characters
    gen.registerFilter("first5", [](std::string_view s) -> std::string {
        if (s.length() <= 5) return std::string(s);
        return std::string(s.substr(0, 5)) + "...";
    });

    TemplateObject custom_data{
        {"word", TemplateValue{"hello"}},
        {"long_text", TemplateValue{"This is a very long text"}}
    };

    auto custom_template1 = "Reversed: {{word | reverse}}";
    auto custom_template2 = "Truncated: {{long_text | first5}}";

    std::cout << "Custom filter 'reverse': " << gen.render(custom_template1, custom_data) << "\n";
    std::cout << "Custom filter 'first5': " << gen.render(custom_template2, custom_data) << "\n\n";

    // =========================================================================
    // Example 10: JSON Configuration Template
    // =========================================================================

    std::cout << "Example 10: JSON Configuration Template Structure\n";
    std::cout << "--------------------------------------------------\n";

    auto json_template = R"({
    "name": "{{name | escape}}",
    "version": "{{version}}",
    "description": "{{description | escape}}",
    "author": "{{author}}",
    "dependencies": [
        {{#each deps}}
        "{{name}}@{{version}}"{{#unless @last}},{{/unless}}
        {{/each}}
    ],
    "config": {
        "debug": {{debug}},
        "port": {{port}},
        "host": "{{host}}"
    }
})";

    std::cout << "JSON configuration template created\n";
    std::cout << "(Full array rendering requires loop implementation)\n\n";

    std::cout << "=============================================================================\n";
    std::cout << "All examples completed successfully!\n";
    std::cout << "=============================================================================\n";

    return 0;
}
