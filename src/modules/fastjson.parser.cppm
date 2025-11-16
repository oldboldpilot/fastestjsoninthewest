// FastestJSONInTheWest Parser Module Implementation  
// Author: Olumuyiwa Oluwasanmi
// C++23 module for high-performance JSON parsing

export module fastjson.parser;

import fastjson.core;
import fastjson.simd;

import <string_view>;
import <expected>;
import <span>;
import <concepts>;
import <bit>;
import <cstring>;

export namespace fastjson::parser {

using namespace fastjson::core;

// ============================================================================
// Parser Configuration
// ============================================================================

export struct parser_config {
    bool allow_comments = false;
    bool allow_trailing_commas = false;
    bool allow_single_quotes = false;
    size_t max_depth = 256;
    size_t max_string_length = 16 * 1024 * 1024; // 16MB
};

// ============================================================================
// Fast Character Classification
// ============================================================================

export constexpr bool is_whitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

export constexpr bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

export constexpr bool is_hex_digit(char c) noexcept {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// ============================================================================
// SIMD-Optimized Skip Functions
// ============================================================================

export size_t skip_whitespace_simd(std::string_view json, size_t pos) noexcept {
    if (pos >= json.size()) return pos;
    
#if defined(__AVX2__) && defined(FASTJSON_USE_SIMD)
    // Use SIMD for bulk whitespace skipping
    return fastjson::simd::skip_whitespace_avx2(json.data() + pos, json.size() - pos) + pos;
#else
    // Fallback scalar implementation
    while (pos < json.size() && is_whitespace(json[pos])) {
        ++pos;
    }
    return pos;
#endif
}

// ============================================================================
// High-Performance JSON Parser
// ============================================================================

export class json_parser {
private:
    std::string_view json_;
    size_t pos_ = 0;
    size_t depth_ = 0;
    parser_config config_;
    
    // Error handling
    json_error last_error_ = json_error::none;
    std::string error_context_;
    
public:
    explicit json_parser(std::string_view json, parser_config config = {})
        : json_(json), config_(config) {}
    
    [[nodiscard]] json_result parse() {
        pos_ = 0;
        depth_ = 0;
        last_error_ = json_error::none;
        
        if (json_.empty()) {
            return std::unexpected(json_error::invalid_json);
        }
        
        pos_ = skip_whitespace_simd(json_, pos_);
        
        auto result = parse_value();
        if (!result) return result;
        
        pos_ = skip_whitespace_simd(json_, pos_);
        if (pos_ < json_.size()) {
            return std::unexpected(json_error::invalid_json);
        }
        
        return result;
    }
    
private:
    [[nodiscard]] json_result parse_value() {
        if (pos_ >= json_.size()) {
            return std::unexpected(json_error::invalid_json);
        }
        
        if (++depth_ > config_.max_depth) {
            return std::unexpected(json_error::invalid_json);
        }
        
        char c = json_[pos_];
        json_result result;
        
        switch (c) {
            case 'n':
                result = parse_null();
                break;
            case 't':
            case 'f':
                result = parse_boolean();
                break;
            case '"':
                result = parse_string();
                break;
            case '[':
                result = parse_array();
                break;
            case '{':
                result = parse_object();
                break;
            default:
                if (c == '-' || is_digit(c)) {
                    result = parse_number();
                } else {
                    result = std::unexpected(json_error::invalid_json);
                }
                break;
        }
        
        --depth_;
        return result;
    }
    
    [[nodiscard]] json_result parse_null() {
        if (pos_ + 4 > json_.size() || json_.substr(pos_, 4) != "null") {
            return std::unexpected(json_error::invalid_json);
        }
        pos_ += 4;
        return json_value{nullptr};
    }
    
    [[nodiscard]] json_result parse_boolean() {
        if (pos_ + 4 <= json_.size() && json_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return json_value{true};
        } else if (pos_ + 5 <= json_.size() && json_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return json_value{false};
        }
        return std::unexpected(json_error::invalid_json);
    }
    
    [[nodiscard]] json_result parse_string() {
        if (json_[pos_] != '"') {
            return std::unexpected(json_error::invalid_json);
        }
        
        ++pos_; // Skip opening quote
        size_t start = pos_;
        
        // Fast path for strings without escapes
        while (pos_ < json_.size() && json_[pos_] != '"' && json_[pos_] != '\\') {
            if (static_cast<unsigned char>(json_[pos_]) < 0x20) {
                return std::unexpected(json_error::invalid_json);
            }
            ++pos_;
        }
        
        if (pos_ >= json_.size()) {
            return std::unexpected(json_error::invalid_json);
        }
        
        if (json_[pos_] == '"') {
            // No escapes, fast path
            std::string result{json_.substr(start, pos_ - start)};
            ++pos_; // Skip closing quote
            return json_value{std::move(result)};
        }
        
        // Slow path with escape handling
        return parse_string_with_escapes(start);
    }
    
    [[nodiscard]] json_result parse_string_with_escapes(size_t start) {
        std::string result;
        result.reserve(json_.size() - start);
        
        // Add characters before first escape
        result += json_.substr(start, pos_ - start);
        
        while (pos_ < json_.size() && json_[pos_] != '"') {
            if (json_[pos_] == '\\') {
                ++pos_;
                if (pos_ >= json_.size()) break;
                
                switch (json_[pos_]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // Unicode escape
                        if (pos_ + 4 >= json_.size()) {
                            return std::unexpected(json_error::invalid_json);
                        }
                        // Simplified: just copy the \uXXXX for now
                        result += "\\u";
                        result += json_.substr(pos_ + 1, 4);
                        pos_ += 4;
                        break;
                    }
                    default:
                        return std::unexpected(json_error::invalid_json);
                }
            } else {
                if (static_cast<unsigned char>(json_[pos_]) < 0x20) {
                    return std::unexpected(json_error::invalid_json);
                }
                result += json_[pos_];
            }
            ++pos_;
        }
        
        if (pos_ >= json_.size() || json_[pos_] != '"') {
            return std::unexpected(json_error::invalid_json);
        }
        
        ++pos_; // Skip closing quote
        return json_value{std::move(result)};
    }
    
    [[nodiscard]] json_result parse_number() {
        size_t start = pos_;
        
        // Handle optional minus
        if (json_[pos_] == '-') ++pos_;
        
        // Parse integer part
        if (pos_ >= json_.size() || !is_digit(json_[pos_])) {
            return std::unexpected(json_error::invalid_json);
        }
        
        if (json_[pos_] == '0') {
            ++pos_;
        } else {
            while (pos_ < json_.size() && is_digit(json_[pos_])) {
                ++pos_;
            }
        }
        
        // Parse fractional part
        if (pos_ < json_.size() && json_[pos_] == '.') {
            ++pos_;
            if (pos_ >= json_.size() || !is_digit(json_[pos_])) {
                return std::unexpected(json_error::invalid_json);
            }
            while (pos_ < json_.size() && is_digit(json_[pos_])) {
                ++pos_;
            }
        }
        
        // Parse exponent part
        if (pos_ < json_.size() && (json_[pos_] == 'e' || json_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < json_.size() && (json_[pos_] == '+' || json_[pos_] == '-')) {
                ++pos_;
            }
            if (pos_ >= json_.size() || !is_digit(json_[pos_])) {
                return std::unexpected(json_error::invalid_json);
            }
            while (pos_ < json_.size() && is_digit(json_[pos_])) {
                ++pos_;
            }
        }
        
        // Convert to double
        std::string num_str{json_.substr(start, pos_ - start)};
        double value = std::strtod(num_str.c_str(), nullptr);
        return json_value{value};
    }
    
    [[nodiscard]] json_result parse_array() {
        if (json_[pos_] != '[') {
            return std::unexpected(json_error::invalid_json);
        }
        ++pos_;
        
        json_array result;
        pos_ = skip_whitespace_simd(json_, pos_);
        
        // Handle empty array
        if (pos_ < json_.size() && json_[pos_] == ']') {
            ++pos_;
            return json_value{std::move(result)};
        }
        
        while (pos_ < json_.size()) {
            auto value = parse_value();
            if (!value) return value;
            
            result.push_back(std::move(*value));
            
            pos_ = skip_whitespace_simd(json_, pos_);
            if (pos_ >= json_.size()) break;
            
            if (json_[pos_] == ']') {
                ++pos_;
                return json_value{std::move(result)};
            } else if (json_[pos_] == ',') {
                ++pos_;
                pos_ = skip_whitespace_simd(json_, pos_);
            } else {
                return std::unexpected(json_error::invalid_json);
            }
        }
        
        return std::unexpected(json_error::invalid_json);
    }
    
    [[nodiscard]] json_result parse_object() {
        if (json_[pos_] != '{') {
            return std::unexpected(json_error::invalid_json);
        }
        ++pos_;
        
        json_object result;
        pos_ = skip_whitespace_simd(json_, pos_);
        
        // Handle empty object
        if (pos_ < json_.size() && json_[pos_] == '}') {
            ++pos_;
            return json_value{std::move(result)};
        }
        
        while (pos_ < json_.size()) {
            // Parse key
            pos_ = skip_whitespace_simd(json_, pos_);
            if (pos_ >= json_.size() || json_[pos_] != '"') {
                return std::unexpected(json_error::invalid_json);
            }
            
            auto key = parse_string();
            if (!key) return key;
            
            std::string key_str = key->as<std::string>();
            
            // Parse colon
            pos_ = skip_whitespace_simd(json_, pos_);
            if (pos_ >= json_.size() || json_[pos_] != ':') {
                return std::unexpected(json_error::invalid_json);
            }
            ++pos_;
            pos_ = skip_whitespace_simd(json_, pos_);
            
            // Parse value
            auto value = parse_value();
            if (!value) return value;
            
            result[key_str] = std::move(*value);
            
            pos_ = skip_whitespace_simd(json_, pos_);
            if (pos_ >= json_.size()) break;
            
            if (json_[pos_] == '}') {
                ++pos_;
                return json_value{std::move(result)};
            } else if (json_[pos_] == ',') {
                ++pos_;
                pos_ = skip_whitespace_simd(json_, pos_);
            } else {
                return std::unexpected(json_error::invalid_json);
            }
        }
        
        return std::unexpected(json_error::invalid_json);
    }
};

// ============================================================================
// Convenience Functions
// ============================================================================

export [[nodiscard]] json_result parse(std::string_view json) {
    json_parser parser{json};
    return parser.parse();
}

export [[nodiscard]] json_result parse(std::string_view json, const parser_config& config) {
    json_parser parser{json, config};
    return parser.parse();
}

} // namespace fastjson::parser
