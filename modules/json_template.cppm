// json_template.cppm - FastestJSONInTheWest Mustache Logic
// Pure C++23 Module for high-performance string templating
// Copyright (c) 2026 Olumuyiwa Oluwasanmi

module;

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <functional>
#include <unordered_map>
#include <variant>

export module json_template;

// Import fastjson_parallel to access json_value
import fastjson_parallel;

export namespace fastjson_parallel {
namespace mustache {

// ============================================================================
// Tokenizer & Parser
// ============================================================================

enum class token_type {
    text,
    variable,
    section_start,
    inverted_section_start,
    section_end,
    comment
};

struct token {
    token_type type;
    std::string content;
    std::string_view raw;
};

// Optimized tokenizer using string_view to avoid copies during parsing
inline auto tokenize(std::string_view template_str) -> std::vector<token> {
    std::vector<token> tokens;
    size_t pos = 0;
    const std::string_view open_tag = "{{";
    const std::string_view close_tag = "}}";

    while (pos < template_str.length()) {
        size_t start = template_str.find(open_tag, pos);
        if (start == std::string_view::npos) {
            // Remaining text
            tokens.push_back({token_type::text, std::string(template_str.substr(pos)), template_str.substr(pos)});
            break;
        }

        // Add text before tag
        if (start > pos) {
            tokens.push_back({token_type::text, std::string(template_str.substr(pos, start - pos)), template_str.substr(pos, start - pos)});
        }

        size_t end = template_str.find(close_tag, start);
        if (end == std::string_view::npos) {
            // Unclosed tag, treat rest as text
            tokens.push_back({token_type::text, std::string(template_str.substr(start)), template_str.substr(start)});
            break;
        }

        // Parse tag content
        // {{  content  }}
        //   ^ start     ^ end
        std::string_view tag_content = template_str.substr(start + 2, end - (start + 2));
        
        // Trim whitespace
        size_t first = tag_content.find_first_not_of(" \t");
        size_t last = tag_content.find_last_not_of(" \t");
        
        if (first == std::string_view::npos) {
             token_type type = token_type::variable; // empty tag? treat as var
             tokens.push_back({type, "", tag_content});
        } else {
            tag_content = tag_content.substr(first, (last - first + 1));
            
            token_type type = token_type::variable;
            std::string content = std::string(tag_content);

            if (tag_content.starts_with('#')) {
                type = token_type::section_start;
                content = tag_content.substr(1);
            } else if (tag_content.starts_with('^')) {
                type = token_type::inverted_section_start;
                content = tag_content.substr(1);
            } else if (tag_content.starts_with('/')) {
                type = token_type::section_end;
                content = tag_content.substr(1);
            } else if (tag_content.starts_with('!')) {
                type = token_type::comment;
            }

            // Trim content again if needed (e.g. after #)
            size_t c_first = content.find_first_not_of(" \t");
            if (c_first != std::string::npos) {
                 size_t c_last = content.find_last_not_of(" \t");
                 content = content.substr(c_first, c_last - c_first + 1);
            }

            tokens.push_back({type, content, template_str.substr(start, end + 2 - start)});
        }

        pos = end + 2;
    }
    return tokens;
}

// ============================================================================
// Renderer
// ============================================================================

// Helper to look up variable in context stack
// Context is a vector of json_value references (stack)
using context_stack = std::vector<const json_value*>;

inline auto lookup(const context_stack& ctx, const std::string& key) -> const json_value* {
    // Search from top of stack down
    for (auto it = ctx.rbegin(); it != ctx.rend(); ++it) {
        const json_value* current = *it;
        if (current->is_object() && current->as_object().contains(key)) {
            return &((*current)[key]);
        }
        // Implicit dot iterator
        if (key == "." || key == "this") {
            return current;
        }
    }
    return nullptr;
}

// Optimized renderer with string buffer pre-allocation strategy
inline auto render_tokens(const std::vector<token>& tokens, context_stack& ctx, size_t& current_idx, std::string& output) -> void {
    while (current_idx < tokens.size()) {
        const auto& tok = tokens[current_idx];
        current_idx++;

        switch (tok.type) {
            case token_type::text:
                output.append(tok.content);
                break;
            case token_type::variable: {
                const json_value* val = lookup(ctx, tok.content);
                if (val) {
                    if (val->is_string()) {
                        output.append(val->as_string());
                    } else if (val->is_number()) {
                         // Double
                         double d = val->as_number();
                         // Simple int check for cleaner output
                         double int_part;
                         if (std::modf(d, &int_part) == 0.0) {
                             output.append(std::to_string((long long)d));
                         } else {
                             output.append(std::to_string(d));
                         }
                    } else if (val->is_bool()) {
                        output.append(val->as_bool() ? "true" : "false");
                    } else if (val->is_int_128()) {
                        // TODO: Implement 128-bit to string in fastjson core or here
                        // For now, fallback or cast
                        output.append(" <int128> "); 
                    } 
                    // Arrays/Objects usually not printed directly in simple mustache
                }
                break;
            }
            case token_type::section_start: {
                std::string section_name = tok.content;
                const json_value* val = lookup(ctx, section_name);
                
                // Find matching end tag to know range
                size_t section_start_idx = current_idx;
                int nesting = 1;
                size_t section_end_idx = current_idx;
                
                // Scan ahead for matching close tag
                while (section_end_idx < tokens.size()) {
                    if (tokens[section_end_idx].type == token_type::section_start && tokens[section_end_idx].content == section_name) nesting++;
                    else if (tokens[section_end_idx].type == token_type::section_end && tokens[section_end_idx].content == section_name) {
                        nesting--;
                        if (nesting == 0) break;
                    }
                    section_end_idx++;
                }

                if (nesting != 0) {
                    // Unclosed section, abort or ignore?
                    // Just print raw? 
                    break; 
                }

                bool is_truthy = false;
                bool is_list = false;

                if (val) {
                    if (val->is_bool()) is_truthy = val->as_bool();
                    else if (val->is_array()) { is_truthy = !val->empty(); is_list = true; }
                    else if (val->is_object()) is_truthy = true; // Object is truthy?
                    else if (val->is_string()) is_truthy = !val->as_string().empty();
                    else if (val->is_number()) is_truthy = (val->as_number() != 0.0);
                }

                if (is_truthy) {
                    if (is_list) {
                        // Loop over array
                        const auto& arr = val->as_array();
                        for (const auto& item : arr) {
                            ctx.push_back(&item);
                            size_t temp_idx = section_start_idx;
                            std::string loop_output; // Use temp buffer or direct?
                            // Recursive call is easiest but stack depth concern
                            // Let's modify current_idx only in the outer loop
                            // We need to re-run tokens [start...end) for each item
                            
                            // To avoid recursion depth issues with huge templates, efficient way:
                            // Create sub-vector of tokens? Or pass indices.
                            // Let's optimize: passing indices
                            size_t runner = section_start_idx;
                            render_tokens(tokens, ctx, runner, output);
                            ctx.pop_back();
                        }
                    } else {
                        // Object or simple truthy
                        if (val->is_object()) ctx.push_back(val);
                        size_t runner = section_start_idx;
                        render_tokens(tokens, ctx, runner, output);
                        if (val->is_object()) ctx.pop_back();
                    }
                }
                
                // Move actual index past the section
                current_idx = section_end_idx + 1;
                break;
            }
            case token_type::inverted_section_start: {
                 std::string section_name = tok.content;
                const json_value* val = lookup(ctx, section_name);
                
                // Find matching end tag
                size_t section_start_idx = current_idx;
                int nesting = 1;
                size_t section_end_idx = current_idx;
                 while (section_end_idx < tokens.size()) {
                    if ((tokens[section_end_idx].type == token_type::section_start || tokens[section_end_idx].type == token_type::inverted_section_start) && tokens[section_end_idx].content == section_name) nesting++;
                    else if (tokens[section_end_idx].type == token_type::section_end && tokens[section_end_idx].content == section_name) {
                        nesting--;
                        if (nesting == 0) break;
                    }
                    section_end_idx++;
                }

                bool is_falsy = true;
                 if (val) {
                    if (val->is_bool()) is_falsy = !val->as_bool();
                    else if (val->is_array()) is_falsy = val->empty();
                    else if (val->is_string()) is_falsy = val->as_string().empty();
                    else if (val->is_number()) is_falsy = (val->as_number() == 0.0);
                    else is_falsy = false; // Object is truthy
                }

                if (is_falsy) {
                     size_t runner = section_start_idx;
                     render_tokens(tokens, ctx, runner, output);
                }
                
                current_idx = section_end_idx + 1;
                break;
            }
            case token_type::section_end:
                // Should be handled by caller if recursing, or we just hit an end doing linear scan
                // If we hit this normally, it means we finished a block
                return; 
            case token_type::comment:
                break;
        }
    }
}

auto render(std::string_view template_str, const json_value& data) -> std::string {
    auto tokens = tokenize(template_str);
    std::string output;
    output.reserve(template_str.size() * 2); // Heuristic
    context_stack ctx;
    ctx.push_back(&data);
    
    size_t start_idx = 0;
    render_tokens(tokens, ctx, start_idx, output);
    
    return output;
}

} // namespace mustache
} // namespace fastjson
