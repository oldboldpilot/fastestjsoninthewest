// FastestJSONInTheWest Serializer Module Implementation
// Author: Olumuyiwa Oluwasanmi  
// C++23 module for high-performance JSON serialization

export module fastjson.serializer;

import fastjson.core;

import <string>;
import <string_view>;
import <sstream>;
import <format>;
import <concepts>;
import <ranges>;

export namespace fastjson::serializer {

using namespace fastjson::core;

// ============================================================================
// Serialization Configuration
// ============================================================================

export struct serialize_config {
    bool pretty_print = false;
    std::string indent = "  ";
    bool escape_unicode = false;
    bool ensure_ascii = false;
    size_t max_depth = 256;
};

// ============================================================================
// Fast String Escaping
// ============================================================================

export std::string escape_string(std::string_view str) {
    std::string result;
    result.reserve(str.size() + str.size() / 8); // Estimate
    
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b':  result += "\\b"; break;
            case '\f':  result += "\\f"; break;
            case '\n':  result += "\\n"; break;
            case '\r':  result += "\\r"; break;
            case '\t':  result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    result += std::format("\\u{:04x}", static_cast<unsigned char>(c));
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

// ============================================================================
// High-Performance JSON Serializer
// ============================================================================

export class json_serializer {
private:
    serialize_config config_;
    std::string result_;
    size_t depth_ = 0;
    
public:
    explicit json_serializer(serialize_config config = {}) : config_(config) {
        result_.reserve(4096); // Initial capacity
    }
    
    [[nodiscard]] std::string serialize(const json_value& value) {
        result_.clear();
        depth_ = 0;
        serialize_value(value);
        return std::move(result_);
    }
    
private:
    void serialize_value(const json_value& value) {
        if (depth_++ > config_.max_depth) {
            throw std::runtime_error("Maximum serialization depth exceeded");
        }
        
        if (value.is_null()) {
            result_ += "null";
        } else if (value.is_boolean()) {
            result_ += value.as<bool>() ? "true" : "false";
        } else if (value.is_number()) {
            serialize_number(value.as<double>());
        } else if (value.is_string()) {
            serialize_string(value.as<std::string>());
        } else if (value.is_array()) {
            serialize_array(value.as<json_array>());
        } else if (value.is_object()) {
            serialize_object(value.as<json_object>());
        }
        
        --depth_;
    }
    
    void serialize_number(double num) {
        if (std::isnan(num) || std::isinf(num)) {
            result_ += "null";
        } else if (num == static_cast<long long>(num)) {
            // Integer representation
            result_ += std::format("{}", static_cast<long long>(num));
        } else {
            // Floating point representation
            result_ += std::format("{}", num);
        }
    }
    
    void serialize_string(const std::string& str) {
        result_ += '"';
        result_ += escape_string(str);
        result_ += '"';
    }
    
    void serialize_array(const json_array& arr) {
        result_ += '[';
        
        if (config_.pretty_print && !arr.empty()) {
            result_ += '\n';
        }
        
        for (size_t i = 0; i < arr.size(); ++i) {
            if (config_.pretty_print) {
                add_indent();
            }
            
            serialize_value(arr[i]);
            
            if (i < arr.size() - 1) {
                result_ += ',';
            }
            
            if (config_.pretty_print) {
                result_ += '\n';
            }
        }
        
        if (config_.pretty_print && !arr.empty()) {
            add_indent_prev_level();
        }
        
        result_ += ']';
    }
    
    void serialize_object(const json_object& obj) {
        result_ += '{';
        
        if (config_.pretty_print && !obj.empty()) {
            result_ += '\n';
        }
        
        size_t count = 0;
        for (const auto& [key, value] : obj) {
            if (config_.pretty_print) {
                add_indent();
            }
            
            serialize_string(key);
            result_ += ':';
            
            if (config_.pretty_print) {
                result_ += ' ';
            }
            
            serialize_value(value);
            
            if (count < obj.size() - 1) {
                result_ += ',';
            }
            
            if (config_.pretty_print) {
                result_ += '\n';
            }
            
            ++count;
        }
        
        if (config_.pretty_print && !obj.empty()) {
            add_indent_prev_level();
        }
        
        result_ += '}';
    }
    
    void add_indent() {
        for (size_t i = 0; i < depth_; ++i) {
            result_ += config_.indent;
        }
    }
    
    void add_indent_prev_level() {
        for (size_t i = 0; i < depth_ - 1; ++i) {
            result_ += config_.indent;
        }
    }
};

// ============================================================================
// Convenience Functions
// ============================================================================

export [[nodiscard]] std::string to_string(const json_value& value) {
    json_serializer serializer;
    return serializer.serialize(value);
}

export [[nodiscard]] std::string to_pretty_string(const json_value& value) {
    serialize_config config{.pretty_print = true};
    json_serializer serializer{config};
    return serializer.serialize(value);
}

export [[nodiscard]] std::string to_string(const json_value& value, const serialize_config& config) {
    json_serializer serializer{config};
    return serializer.serialize(value);
}

} // namespace fastjson::serializer
