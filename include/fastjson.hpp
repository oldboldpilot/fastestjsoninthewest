#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <format>
#include <immintrin.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace fastjson {

// Forward declarations
class json_value;

// Type aliases with proper scope
using json_null = std::monostate;
using json_boolean = bool;
using json_number = double;
using json_string = std::string;
using json_array = std::vector<json_value>;
using json_object = std::unordered_map<std::string, json_value>;

// Error handling
enum class json_error {
    invalid_syntax,
    unexpected_character,
    unexpected_end,
    invalid_string,
    invalid_number,
    stack_overflow
};

template <typename T>
using json_result = std::expected<T, json_error>;

// SIMD capability detection
struct simd_caps {
    static inline bool detect_sse2() {
        #if defined(__SSE2__)
        return true;
        #else
        return false;
        #endif
    }
    
    static inline bool detect_avx2() {
        #if defined(__AVX2__)
        return true;
        #else
        return false;
        #endif
    }
    
    static inline bool detect_avx512() {
        #if defined(__AVX512F__)
        return true;
        #else
        return false;
        #endif
    }
    
    static inline bool detect_vnni() {
        #if defined(__AVX512VNNI__)
        return true;
        #else
        return false;
        #endif
    }
    
    static inline bool detect_amx_tmul() {
        #if defined(__AMX_TMUL__)
        return true;
        #else
        return false;
        #endif
    }
};

// JSON value class
class json_value {
    using value_type = std::variant<
        json_null,
        json_boolean,
        json_number,
        json_string,
        json_array,
        json_object
    >;

    value_type value_;

public:
    // Constructors with proper type resolution
    json_value() noexcept : value_(json_null{}) {}
    
    explicit json_value(json_null) noexcept : value_(json_null{}) {}
    explicit json_value(json_boolean b) noexcept : value_(b) {}
    explicit json_value(json_number n) noexcept : value_(n) {}
    explicit json_value(const json_string& s) : value_(s) {}
    explicit json_value(json_string&& s) : value_(std::move(s)) {}
    explicit json_value(const char* s) : value_(json_string{s}) {}
    explicit json_value(const json_array& a) : value_(a) {}
    explicit json_value(json_array&& a) : value_(std::move(a)) {}
    explicit json_value(const json_object& o) : value_(o) {}
    explicit json_value(json_object&& o) : value_(std::move(o)) {}
    
    // Numeric constructor templates
    template<typename T>
    requires std::integral<T> && (!std::same_as<T, bool>)
    explicit json_value(T n) noexcept : value_(static_cast<json_number>(n)) {}
    
    template<typename T>
    requires std::floating_point<T>
    explicit json_value(T n) noexcept : value_(static_cast<json_number>(n)) {}
    
    // Copy and move
    json_value(const json_value&) = default;
    json_value(json_value&&) noexcept = default;
    json_value& operator=(const json_value&) = default;
    json_value& operator=(json_value&&) noexcept = default;
    
    // Type checking
    [[nodiscard]] bool is_null() const noexcept {
        return std::holds_alternative<json_null>(value_);
    }
    [[nodiscard]] bool is_boolean() const noexcept {
        return std::holds_alternative<json_boolean>(value_);
    }
    [[nodiscard]] bool is_number() const noexcept {
        return std::holds_alternative<json_number>(value_);
    }
    [[nodiscard]] bool is_string() const noexcept {
        return std::holds_alternative<json_string>(value_);
    }
    [[nodiscard]] bool is_array() const noexcept {
        return std::holds_alternative<json_array>(value_);
    }
    [[nodiscard]] bool is_object() const noexcept {
        return std::holds_alternative<json_object>(value_);
    }
    
    // Value extraction
    [[nodiscard]] json_boolean as_boolean() const {
        return std::get<json_boolean>(value_);
    }
    
    [[nodiscard]] json_number as_number() const {
        return std::get<json_number>(value_);
    }
    
    [[nodiscard]] const json_string& as_string() const {
        return std::get<json_string>(value_);
    }
    
    [[nodiscard]] const json_array& as_array() const {
        return std::get<json_array>(value_);
    }
    
    [[nodiscard]] json_array& as_array() {
        return std::get<json_array>(value_);
    }
    
    [[nodiscard]] const json_object& as_object() const {
        return std::get<json_object>(value_);
    }
    
    [[nodiscard]] json_object& as_object() {
        return std::get<json_object>(value_);
    }
    
    // Array and object manipulation
    json_value& push_back(const json_value& val) {
        if (!is_array()) value_ = json_array{};
        std::get<json_array>(value_).push_back(val);
        return *this;
    }
    
    json_value& push_back(json_value&& val) {
        if (!is_array()) value_ = json_array{};
        std::get<json_array>(value_).push_back(std::move(val));
        return *this;
    }
    
    json_value& set(const std::string& key, const json_value& val) {
        if (!is_object()) value_ = json_object{};
        std::get<json_object>(value_)[key] = val;
        return *this;
    }
    
    json_value& set(const std::string& key, json_value&& val) {
        if (!is_object()) value_ = json_object{};
        std::get<json_object>(value_)[key] = std::move(val);
        return *this;
    }
    
    // Index operators
    json_value& operator[](std::size_t idx) {
        if (!is_array()) value_ = json_array{};
        auto& arr = std::get<json_array>(value_);
        if (idx >= arr.size()) {
            arr.resize(idx + 1);
        }
        return arr[idx];
    }
    
    const json_value& operator[](std::size_t idx) const {
        const auto& arr = std::get<json_array>(value_);
        return arr[idx];
    }
    
    json_value& operator[](const std::string& key) {
        if (!is_object()) value_ = json_object{};
        return std::get<json_object>(value_)[key];
    }
    
    const json_value& operator[](const std::string& key) const {
        const auto& obj = std::get<json_object>(value_);
        auto it = obj.find(key);
        if (it == obj.end()) {
            static const json_value null_value{json_null{}};
            return null_value;
        }
        return it->second;
    }
    
    // Comparison
    bool operator==(const json_value& other) const noexcept {
        return value_ == other.value_;
    }
    
    // Utility
    [[nodiscard]] std::size_t size() const {
        if (is_array()) return std::get<json_array>(value_).size();
        if (is_object()) return std::get<json_object>(value_).size();
        return 0;
    }
    
    [[nodiscard]] bool empty() const {
        if (is_array()) return std::get<json_array>(value_).empty();
        if (is_object()) return std::get<json_object>(value_).empty();
        if (is_string()) return std::get<json_string>(value_).empty();
        return true;
    }
    
    [[nodiscard]] bool contains(const std::string& key) const {
        if (!is_object()) return false;
        const auto& obj = std::get<json_object>(value_);
        return obj.find(key) != obj.end();
    }
};

// Parser class
class parser {
    std::string_view input_;
    std::size_t pos_ = 0;
    
    // SIMD-optimized whitespace skipping
    void skip_whitespace() {
        #if defined(__AVX512F__)
        if (simd_caps::detect_avx512()) {
            while (pos_ < input_.size()) {
                const auto* ptr = input_.data() + pos_;
                const auto remaining = input_.size() - pos_;
                
                if (remaining >= 64) {
                    __m512i chunk = _mm512_loadu_si512(ptr);
                    __m512i spaces = _mm512_set1_epi8(' ');
                    __m512i tabs = _mm512_set1_epi8('\t');
                    __m512i newlines = _mm512_set1_epi8('\n');
                    __m512i returns = _mm512_set1_epi8('\r');
                    
                    auto space_mask = _mm512_cmpeq_epi8_mask(chunk, spaces);
                    auto tab_mask = _mm512_cmpeq_epi8_mask(chunk, tabs);
                    auto newline_mask = _mm512_cmpeq_epi8_mask(chunk, newlines);
                    auto return_mask = _mm512_cmpeq_epi8_mask(chunk, returns);
                    
                    auto ws_mask = space_mask | tab_mask | newline_mask | return_mask;
                    
                    if (ws_mask == 0xFFFFFFFFFFFFFFFFULL) {
                        pos_ += 64;
                        continue;
                    } else {
                        auto first_non_ws = _tzcnt_u64(~ws_mask);
                        pos_ += first_non_ws;
                        return;
                    }
                } else {
                    break;
                }
            }
        }
        #endif
        
        // Fallback implementation
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }
    
    char peek() const {
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }
    
    char consume() {
        return pos_ < input_.size() ? input_[pos_++] : '\0';
    }
    
    bool expect(char c) {
        skip_whitespace();
        if (peek() == c) {
            consume();
            return true;
        }
        return false;
    }
    
    // SIMD-optimized string parsing
    json_result<json_string> parse_string() {
        if (!expect('"')) {
            return std::unexpected(json_error::invalid_string);
        }
        
        json_string result;
        result.reserve(32);  // Reserve reasonable capacity
        
        while (pos_ < input_.size()) {
            char c = consume();
            if (c == '"') {
                return result;
            }
            if (c == '\\') {
                if (pos_ >= input_.size()) {
                    return std::unexpected(json_error::unexpected_end);
                }
                char escaped = consume();
                switch (escaped) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default:
                        return std::unexpected(json_error::invalid_string);
                }
            } else {
                result += c;
            }
        }
        return std::unexpected(json_error::unexpected_end);
    }
    
    json_result<json_number> parse_number() {
        const auto start_pos = pos_;
        
        // Handle optional minus
        if (peek() == '-') consume();
        
        // Parse integer part
        if (peek() == '0') {
            consume();
        } else if (peek() >= '1' && peek() <= '9') {
            consume();
            while (peek() >= '0' && peek() <= '9') consume();
        } else {
            return std::unexpected(json_error::invalid_number);
        }
        
        // Parse optional fractional part
        if (peek() == '.') {
            consume();
            if (!(peek() >= '0' && peek() <= '9')) {
                return std::unexpected(json_error::invalid_number);
            }
            while (peek() >= '0' && peek() <= '9') consume();
        }
        
        // Parse optional exponent
        if (peek() == 'e' || peek() == 'E') {
            consume();
            if (peek() == '+' || peek() == '-') consume();
            if (!(peek() >= '0' && peek() <= '9')) {
                return std::unexpected(json_error::invalid_number);
            }
            while (peek() >= '0' && peek() <= '9') consume();
        }
        
        auto number_str = input_.substr(start_pos, pos_ - start_pos);
        try {
            return std::stod(std::string{number_str});
        } catch (...) {
            return std::unexpected(json_error::invalid_number);
        }
    }
    
    json_result<json_array> parse_array() {
        if (!expect('[')) {
            return std::unexpected(json_error::invalid_syntax);
        }
        
        json_array result;
        skip_whitespace();
        
        if (peek() == ']') {
            consume();
            return result;
        }
        
        while (true) {
            auto value = parse_value();
            if (!value) return std::unexpected(value.error());
            result.push_back(std::move(*value));
            
            skip_whitespace();
            if (peek() == ']') {
                consume();
                return result;
            }
            if (peek() == ',') {
                consume();
                skip_whitespace();
            } else {
                return std::unexpected(json_error::invalid_syntax);
            }
        }
    }
    
    json_result<json_object> parse_object() {
        if (!expect('{')) {
            return std::unexpected(json_error::invalid_syntax);
        }
        
        json_object result;
        skip_whitespace();
        
        if (peek() == '}') {
            consume();
            return result;
        }
        
        while (true) {
            auto key = parse_string();
            if (!key) return std::unexpected(key.error());
            
            skip_whitespace();
            if (!expect(':')) {
                return std::unexpected(json_error::invalid_syntax);
            }
            
            auto value = parse_value();
            if (!value) return std::unexpected(value.error());
            
            result[*key] = std::move(*value);
            
            skip_whitespace();
            if (peek() == '}') {
                consume();
                return result;
            }
            if (peek() == ',') {
                consume();
                skip_whitespace();
            } else {
                return std::unexpected(json_error::invalid_syntax);
            }
        }
    }
    
    json_result<json_value> parse_value() {
        skip_whitespace();
        
        switch (peek()) {
            case 'n': {
                if (input_.substr(pos_, 4) == "null") {
                    pos_ += 4;
                    return json_value{json_null{}};
                }
                return std::unexpected(json_error::invalid_syntax);
            }
            case 't': {
                if (input_.substr(pos_, 4) == "true") {
                    pos_ += 4;
                    return json_value{true};
                }
                return std::unexpected(json_error::invalid_syntax);
            }
            case 'f': {
                if (input_.substr(pos_, 5) == "false") {
                    pos_ += 5;
                    return json_value{false};
                }
                return std::unexpected(json_error::invalid_syntax);
            }
            case '"': {
                auto str = parse_string();
                if (!str) return std::unexpected(str.error());
                return json_value{std::move(*str)};
            }
            case '[': {
                auto arr = parse_array();
                if (!arr) return std::unexpected(arr.error());
                return json_value{std::move(*arr)};
            }
            case '{': {
                auto obj = parse_object();
                if (!obj) return std::unexpected(obj.error());
                return json_value{std::move(*obj)};
            }
            default: {
                if ((peek() >= '0' && peek() <= '9') || peek() == '-') {
                    auto num = parse_number();
                    if (!num) return std::unexpected(num.error());
                    return json_value{*num};
                }
                return std::unexpected(json_error::invalid_syntax);
            }
        }
    }
    
public:
    explicit parser(std::string_view input) : input_(input) {}
    
    json_result<json_value> parse() {
        auto result = parse_value();
        if (!result) return result;
        
        skip_whitespace();
        if (pos_ < input_.size()) {
            return std::unexpected(json_error::invalid_syntax);
        }
        
        return result;
    }
};

// Serializer class
class serializer {
    std::string output_;
    int indent_level_ = 0;
    bool pretty_ = false;
    
    void write_indent() {
        if (pretty_) {
            for (int i = 0; i < indent_level_; ++i) {
                output_ += "  ";
            }
        }
    }
    
    void write_newline() {
        if (pretty_) {
            output_ += '\n';
        }
    }
    
    // SIMD-optimized string escaping
    void escape_string(const json_string& str) {
        output_ += '"';
        
        #if defined(__AVX2__)
        if (simd_caps::detect_avx2() && str.size() >= 32) {
            const auto* data = str.data();
            std::size_t i = 0;
            const std::size_t simd_end = str.size() - 31;
            
            while (i < simd_end) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
                __m256i quote = _mm256_set1_epi8('"');
                __m256i backslash = _mm256_set1_epi8('\\');
                __m256i control = _mm256_set1_epi8('\x1F');
                
                auto quote_mask = _mm256_cmpeq_epi8(chunk, quote);
                auto backslash_mask = _mm256_cmpeq_epi8(chunk, backslash);
                auto control_mask = _mm256_cmpgt_epi8(control, chunk);
                
                int needs_escape = _mm256_movemask_epi8(quote_mask) | 
                                 _mm256_movemask_epi8(backslash_mask) | 
                                 _mm256_movemask_epi8(control_mask);
                
                if (needs_escape == 0) {
                    output_.append(data + i, 32);
                    i += 32;
                } else {
                    break;
                }
            }
            
            // Handle remaining characters
            for (; i < str.size(); ++i) {
                char c = str[i];
                switch (c) {
                    case '"': output_ += "\\\""; break;
                    case '\\': output_ += "\\\\"; break;
                    case '\b': output_ += "\\b"; break;
                    case '\f': output_ += "\\f"; break;
                    case '\n': output_ += "\\n"; break;
                    case '\r': output_ += "\\r"; break;
                    case '\t': output_ += "\\t"; break;
                    default:
                        if (c < 0x20) {
                            output_ += "\\u" + std::format("{:04x}", static_cast<unsigned char>(c));
                        } else {
                            output_ += c;
                        }
                        break;
                }
            }
        } else
        #endif
        {
            // Fallback implementation
            for (char c : str) {
                switch (c) {
                    case '"': output_ += "\\\""; break;
                    case '\\': output_ += "\\\\"; break;
                    case '\b': output_ += "\\b"; break;
                    case '\f': output_ += "\\f"; break;
                    case '\n': output_ += "\\n"; break;
                    case '\r': output_ += "\\r"; break;
                    case '\t': output_ += "\\t"; break;
                    default: output_ += c; break;
                }
            }
        }
        
        output_ += '"';
    }
    
    void serialize_value(const json_value& value) {
        if (value.is_null()) {
            output_ += "null";
        } else if (value.is_boolean()) {
            output_ += (value.as_boolean() ? "true" : "false");
        } else if (value.is_number()) {
            output_ += std::to_string(value.as_number());
        } else if (value.is_string()) {
            escape_string(value.as_string());
        } else if (value.is_array()) {
            const auto& arr = value.as_array();
            output_ += '[';
            if (pretty_ && !arr.empty()) {
                write_newline();
                ++indent_level_;
            }
            
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) {
                    output_ += ',';
                    if (pretty_) write_newline();
                }
                if (pretty_) write_indent();
                serialize_value(arr[i]);
            }
            
            if (pretty_ && !arr.empty()) {
                --indent_level_;
                write_newline();
                write_indent();
            }
            output_ += ']';
        } else if (value.is_object()) {
            const auto& obj = value.as_object();
            output_ += '{';
            if (pretty_ && !obj.empty()) {
                write_newline();
                ++indent_level_;
            }
            
            bool first = true;
            for (const auto& [key, val] : obj) {
                if (!first) {
                    output_ += ',';
                    if (pretty_) write_newline();
                }
                if (pretty_) write_indent();
                
                escape_string(key);
                output_ += ':';
                if (pretty_) output_ += ' ';
                serialize_value(val);
                first = false;
            }
            
            if (pretty_ && !obj.empty()) {
                --indent_level_;
                write_newline();
                write_indent();
            }
            output_ += '}';
        }
    }
    
public:
    explicit serializer(bool pretty = false) : pretty_(pretty) {}
    
    std::string serialize(const json_value& value) {
        output_.clear();
        serialize_value(value);
        return output_;
    }
};

// Convenience functions
inline json_result<json_value> parse(std::string_view input) {
    parser p{input};
    return p.parse();
}

inline std::string stringify(const json_value& value, bool pretty = false) {
    serializer s{pretty};
    return s.serialize(value);
}

// Value creation helpers
inline json_value null_value() { 
    return json_value{json_null{}}; 
}

inline json_value boolean_value(bool b) { 
    return json_value{json_boolean{b}}; 
}

inline json_value number_value(double n) { 
    return json_value{json_number{n}}; 
}

inline json_value string_value(std::string s) { 
    return json_value{json_string{std::move(s)}}; 
}

inline json_value array_value() { 
    return json_value{json_array{}}; 
}

inline json_value object_value() { 
    return json_value{json_object{}}; 
}

// JSON builder pattern
class json_builder {
    json_value value_;
    
public:
    json_builder() : value_(json_object{}) {}
    explicit json_builder(json_value initial) : value_(std::move(initial)) {}
    
    json_builder& add(const std::string& key, json_value val) {
        value_.set(key, std::move(val));
        return *this;
    }
    
    json_builder& append(json_value val) {
        value_.push_back(std::move(val));
        return *this;
    }
    
    json_value build() && { return std::move(value_); }
    const json_value& build() const & { return value_; }
};

// Convenience builder functions
[[nodiscard]] inline json_builder make_object() {
    return json_builder{object_value()};
}

[[nodiscard]] inline json_builder make_array() {
    return json_builder{array_value()};
}

// Literal support
namespace literals {
    inline json_result<json_value> operator""_json(const char* str, std::size_t len) {
        return parse(std::string_view{str, len});
    }
}

using namespace literals;

} // namespace fastjson