// FastestJSONInTheWest Core Module Implementation
// Author: Olumuyiwa Oluwasanmi
// C++23 module for core JSON value representation

export module fastjson.core;

import <string>;
import <variant>;
import <vector>;
import <unordered_map>;
import <memory>;
import <type_traits>;
import <concepts>;
import <expected>;
import <span>;

export namespace fastjson::core {

// ============================================================================
// Core JSON Value Types
// ============================================================================

class json_value;
using json_null = std::monostate;
using json_boolean = bool;
using json_number = double;
using json_string = std::string;
using json_array = std::vector<json_value>;
using json_object = std::unordered_map<std::string, json_value>;

using json_variant = std::variant<
    json_null,
    json_boolean, 
    json_number,
    json_string,
    json_array,
    json_object
>;

// ============================================================================
// JSON Value Class 
// ============================================================================

export class json_value {
private:
    json_variant value_;
    
public:
    // Constructors
    json_value() : value_{json_null{}} {}
    json_value(std::nullptr_t) : value_{json_null{}} {}
    json_value(bool b) : value_{json_boolean{b}} {}
    json_value(int i) : value_{json_number{static_cast<double>(i)}} {}
    json_value(double d) : value_{json_number{d}} {}
    json_value(std::string_view s) : value_{json_string{s}} {}
    json_value(const char* s) : value_{json_string{s}} {}
    json_value(json_array arr) : value_{std::move(arr)} {}
    json_value(json_object obj) : value_{std::move(obj)} {}
    
    // Copy and move
    json_value(const json_value&) = default;
    json_value(json_value&&) = default;
    json_value& operator=(const json_value&) = default;
    json_value& operator=(json_value&&) = default;
    
    // Type checking
    [[nodiscard]] constexpr bool is_null() const noexcept {
        return std::holds_alternative<json_null>(value_);
    }
    
    [[nodiscard]] constexpr bool is_boolean() const noexcept {
        return std::holds_alternative<json_boolean>(value_);
    }
    
    [[nodiscard]] constexpr bool is_number() const noexcept {
        return std::holds_alternative<json_number>(value_);
    }
    
    [[nodiscard]] constexpr bool is_string() const noexcept {
        return std::holds_alternative<json_string>(value_);
    }
    
    [[nodiscard]] constexpr bool is_array() const noexcept {
        return std::holds_alternative<json_array>(value_);
    }
    
    [[nodiscard]] constexpr bool is_object() const noexcept {
        return std::holds_alternative<json_object>(value_);
    }
    
    // Value access with concepts
    template<typename T>
    requires std::same_as<T, bool>
    [[nodiscard]] T& as() {
        return std::get<json_boolean>(value_);
    }
    
    template<typename T>
    requires std::floating_point<T>
    [[nodiscard]] T as() const {
        return static_cast<T>(std::get<json_number>(value_));
    }
    
    template<typename T>
    requires std::same_as<T, std::string>
    [[nodiscard]] const T& as() const {
        return std::get<json_string>(value_);
    }
    
    template<typename T>
    requires std::same_as<T, json_array>
    [[nodiscard]] T& as() {
        return std::get<json_array>(value_);
    }
    
    template<typename T>
    requires std::same_as<T, json_object>
    [[nodiscard]] T& as() {
        return std::get<json_object>(value_);
    }
    
    // Array operations
    [[nodiscard]] size_t size() const {
        if (is_array()) {
            return std::get<json_array>(value_).size();
        } else if (is_object()) {
            return std::get<json_object>(value_).size();
        }
        return 0;
    }
    
    json_value& operator[](size_t index) {
        return std::get<json_array>(value_)[index];
    }
    
    const json_value& operator[](size_t index) const {
        return std::get<json_array>(value_)[index];
    }
    
    // Object operations
    json_value& operator[](const std::string& key) {
        return std::get<json_object>(value_)[key];
    }
    
    json_value& operator[](std::string_view key) {
        return std::get<json_object>(value_)[std::string{key}];
    }
    
    [[nodiscard]] bool contains(const std::string& key) const {
        if (!is_object()) return false;
        return std::get<json_object>(value_).contains(key);
    }
    
    // Utility methods
    [[nodiscard]] std::string type_name() const {
        return std::visit([](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::same_as<T, json_null>) return "null";
            else if constexpr (std::same_as<T, json_boolean>) return "boolean";
            else if constexpr (std::same_as<T, json_number>) return "number";
            else if constexpr (std::same_as<T, json_string>) return "string";
            else if constexpr (std::same_as<T, json_array>) return "array";
            else if constexpr (std::same_as<T, json_object>) return "object";
            else return "unknown";
        }, value_);
    }
    
    // Equality comparison
    bool operator==(const json_value& other) const = default;
};

// ============================================================================
// Error Handling
// ============================================================================

export enum class json_error {
    none = 0,
    invalid_json,
    type_mismatch,
    out_of_range,
    key_not_found,
    memory_error
};

export using json_result = std::expected<json_value, json_error>;

// ============================================================================
// Utility Functions
// ============================================================================

export [[nodiscard]] constexpr std::string_view error_message(json_error err) noexcept {
    switch (err) {
        case json_error::none: return "no error";
        case json_error::invalid_json: return "invalid JSON";
        case json_error::type_mismatch: return "type mismatch";
        case json_error::out_of_range: return "out of range";
        case json_error::key_not_found: return "key not found";
        case json_error::memory_error: return "memory error";
        default: return "unknown error";
    }
}

} // namespace fastjson::core
