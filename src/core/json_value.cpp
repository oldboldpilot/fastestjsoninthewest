// FastestJSONInTheWest - JSON Value Implementation
module;

#include <stdexcept>
#include <utility>
#include <algorithm>

module fastjson.core;

namespace fastjson {

// =============================================================================
// JsonValue Implementation
// =============================================================================

JsonValue::JsonValue(const JsonValue& other) : type_(other.type_) {
    switch (type_) {
        case JsonType::Null:
            break;
        case JsonType::Boolean:
            data_.boolean = other.data_.boolean;
            break;
        case JsonType::Integer:
            data_.integer = other.data_.integer;
            break;
        case JsonType::Double:
            data_.floating_point = other.data_.floating_point;
            break;
        case JsonType::String:
            data_.string = new std::string(*other.data_.string);
            break;
        case JsonType::Array:
            data_.array = new JsonArray(*other.data_.array);
            break;
        case JsonType::Object:
            data_.object = new JsonObject(*other.data_.object);
            break;
    }
}

JsonValue::JsonValue(JsonValue&& other) noexcept : type_(other.type_), data_(other.data_) {
    other.type_ = JsonType::Null;
    other.data_.integer = 0;
}

JsonValue& JsonValue::operator=(const JsonValue& other) {
    if (this != &other) {
        this->~JsonValue();
        new (this) JsonValue(other);
    }
    return *this;
}

JsonValue& JsonValue::operator=(JsonValue&& other) noexcept {
    if (this != &other) {
        this->~JsonValue();
        type_ = other.type_;
        data_ = other.data_;
        other.type_ = JsonType::Null;
        other.data_.integer = 0;
    }
    return *this;
}

JsonValue::~JsonValue() noexcept {
    switch (type_) {
        case JsonType::String:
            delete data_.string;
            break;
        case JsonType::Array:
            delete data_.array;
            break;
        case JsonType::Object:
            delete data_.object;
            break;
        default:
            break;
    }
}

bool JsonValue::as_boolean() const {
    if (type_ != JsonType::Boolean) {
        throw std::runtime_error("JsonValue is not a boolean");
    }
    return data_.boolean;
}

std::int64_t JsonValue::as_integer() const {
    if (type_ != JsonType::Integer) {
        throw std::runtime_error("JsonValue is not an integer");
    }
    return data_.integer;
}

double JsonValue::as_double() const {
    if (type_ == JsonType::Double) {
        return data_.floating_point;
    } else if (type_ == JsonType::Integer) {
        return static_cast<double>(data_.integer);
    }
    throw std::runtime_error("JsonValue is not a number");
}

const std::string& JsonValue::as_string() const {
    if (type_ != JsonType::String) {
        throw std::runtime_error("JsonValue is not a string");
    }
    return *data_.string;
}

const JsonArray& JsonValue::as_array() const {
    if (type_ != JsonType::Array) {
        throw std::runtime_error("JsonValue is not an array");
    }
    return *data_.array;
}

const JsonObject& JsonValue::as_object() const {
    if (type_ != JsonType::Object) {
        throw std::runtime_error("JsonValue is not an object");
    }
    return *data_.object;
}

bool& JsonValue::as_boolean() {
    if (type_ != JsonType::Boolean) {
        throw std::runtime_error("JsonValue is not a boolean");
    }
    return data_.boolean;
}

std::int64_t& JsonValue::as_integer() {
    if (type_ != JsonType::Integer) {
        throw std::runtime_error("JsonValue is not an integer");
    }
    return data_.integer;
}

double& JsonValue::as_double() {
    if (type_ != JsonType::Double) {
        throw std::runtime_error("JsonValue is not a double");
    }
    return data_.floating_point;
}

std::string& JsonValue::as_string() {
    if (type_ != JsonType::String) {
        throw std::runtime_error("JsonValue is not a string");
    }
    return *data_.string;
}

JsonArray& JsonValue::as_array() {
    if (type_ != JsonType::Array) {
        throw std::runtime_error("JsonValue is not an array");
    }
    return *data_.array;
}

JsonObject& JsonValue::as_object() {
    if (type_ != JsonType::Object) {
        throw std::runtime_error("JsonValue is not an object");
    }
    return *data_.object;
}

bool JsonValue::operator==(const JsonValue& other) const noexcept {
    if (type_ != other.type_) return false;
    
    switch (type_) {
        case JsonType::Null:
            return true;
        case JsonType::Boolean:
            return data_.boolean == other.data_.boolean;
        case JsonType::Integer:
            return data_.integer == other.data_.integer;
        case JsonType::Double:
            return data_.floating_point == other.data_.floating_point;
        case JsonType::String:
            return *data_.string == *other.data_.string;
        case JsonType::Array:
            return *data_.array == *other.data_.array;
        case JsonType::Object:
            return *data_.object == *other.data_.object;
    }
    return false;
}

const JsonValue& JsonValue::operator[](std::size_t index) const {
    return as_array()[index];
}

JsonValue& JsonValue::operator[](std::size_t index) {
    return as_array()[index];
}

const JsonValue& JsonValue::operator[](std::string_view key) const {
    return as_object()[key];
}

JsonValue& JsonValue::operator[](std::string_view key) {
    return as_object()[key];
}

// =============================================================================
// JsonObject Implementation
// =============================================================================

auto JsonObject::find_key(std::string_view key) const noexcept {
    return std::find_if(pairs_.begin(), pairs_.end(),
        [key](const auto& pair) { return pair.first == key; });
}

auto JsonObject::find_key(std::string_view key) noexcept {
    return std::find_if(pairs_.begin(), pairs_.end(),
        [key](const auto& pair) { return pair.first == key; });
}

const JsonValue& JsonObject::operator[](std::string_view key) const {
    auto it = find_key(key);
    if (it == pairs_.end()) {
        throw std::out_of_range("Key not found in JSON object");
    }
    return it->second;
}

JsonValue& JsonObject::operator[](std::string_view key) {
    auto it = find_key(key);
    if (it == pairs_.end()) {
        // Insert new key with null value
        pairs_.emplace_back(std::string(key), JsonValue());
        return pairs_.back().second;
    }
    return it->second;
}

const JsonValue& JsonObject::at(std::string_view key) const {
    auto it = find_key(key);
    if (it == pairs_.end()) {
        throw std::out_of_range("Key not found in JSON object");
    }
    return it->second;
}

JsonValue& JsonObject::at(std::string_view key) {
    auto it = find_key(key);
    if (it == pairs_.end()) {
        throw std::out_of_range("Key not found in JSON object");
    }
    return it->second;
}

bool JsonObject::contains(std::string_view key) const noexcept {
    return find_key(key) != pairs_.end();
}

auto JsonObject::find(std::string_view key) const noexcept -> const_iterator {
    return find_key(key);
}

auto JsonObject::find(std::string_view key) noexcept -> iterator {
    return find_key(key);
}

std::pair<JsonObject::iterator, bool> JsonObject::insert(const value_type& pair) {
    auto it = find_key(pair.first);
    if (it != pairs_.end()) {
        return {it, false};
    }
    pairs_.push_back(pair);
    return {pairs_.end() - 1, true};
}

std::pair<JsonObject::iterator, bool> JsonObject::insert(value_type&& pair) {
    auto it = find_key(pair.first);
    if (it != pairs_.end()) {
        return {it, false};
    }
    pairs_.push_back(std::move(pair));
    return {pairs_.end() - 1, true};
}

JsonObject::size_type JsonObject::erase(std::string_view key) {
    auto it = find_key(key);
    if (it == pairs_.end()) {
        return 0;
    }
    pairs_.erase(it);
    return 1;
}

JsonObject::iterator JsonObject::erase(const_iterator pos) {
    return pairs_.erase(pos);
}

// =============================================================================
// JsonDocument Implementation
// =============================================================================

JsonDocument::JsonDocument() : root_(JsonValue()) {
}

JsonDocument::JsonDocument(JsonValue root) : root_(std::move(root)) {
}

JsonDocument::~JsonDocument() noexcept = default;

std::size_t JsonDocument::memory_usage() const noexcept {
    // TODO: Implement memory tracking
    return sizeof(JsonDocument);
}

void JsonDocument::compact() {
    // TODO: Implement memory compaction
}

// =============================================================================
// Utility Functions
// =============================================================================

std::string escape_string(std::string_view str) {
    std::string result;
    result.reserve(str.size() + 10); // Reserve some extra space for escapes
    
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control character - use \uXXXX encoding
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }
    
    return result;
}

std::string unescape_string(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
                case '"':  result += '"'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case '/':  result += '/'; ++i; break;
                case 'b':  result += '\b'; ++i; break;
                case 'f':  result += '\f'; ++i; break;
                case 'n':  result += '\n'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                case 'u':
                    // TODO: Handle \uXXXX Unicode escapes
                    ++i;
                    break;
                default:
                    result += str[i];
                    break;
            }
        } else {
            result += str[i];
        }
    }
    
    return result;
}

bool validate_utf8(std::string_view str) noexcept {
    std::size_t i = 0;
    while (i < str.size()) {
        unsigned char c = str[i];
        
        if (c <= 0x7F) {
            // Single-byte character
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            // Two-byte character
            if (i + 1 >= str.size() || (str[i + 1] & 0xC0) != 0x80) {
                return false;
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // Three-byte character
            if (i + 2 >= str.size() || (str[i + 1] & 0xC0) != 0x80 || (str[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // Four-byte character
            if (i + 3 >= str.size() || (str[i + 1] & 0xC0) != 0x80 || 
                (str[i + 2] & 0xC0) != 0x80 || (str[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            i += 4;
        } else {
            return false;
        }
    }
    
    return true;
}

Result<std::int64_t> parse_integer(std::string_view str) noexcept {
    if (str.empty()) {
        return std::unexpected(FASTJSON_ERROR(InvalidNumber, "Empty string"));
    }
    
    std::int64_t result = 0;
    std::size_t i = 0;
    bool negative = false;
    
    if (str[0] == '-') {
        negative = true;
        ++i;
    }
    
    if (i >= str.size()) {
        return std::unexpected(FASTJSON_ERROR(InvalidNumber, "No digits after sign"));
    }
    
    for (; i < str.size(); ++i) {
        if (str[i] < '0' || str[i] > '9') {
            return std::unexpected(FASTJSON_ERROR(InvalidNumber, "Invalid digit"));
        }
        result = result * 10 + (str[i] - '0');
    }
    
    return negative ? -result : result;
}

Result<double> parse_double(std::string_view str) noexcept {
    // Simple implementation - production code would use faster parsing
    try {
        std::size_t pos;
        double result = std::stod(std::string(str), &pos);
        if (pos != str.size()) {
            return std::unexpected(FASTJSON_ERROR(InvalidNumber, "Extra characters after number"));
        }
        return result;
    } catch (...) {
        return std::unexpected(FASTJSON_ERROR(InvalidNumber, "Failed to parse double"));
    }
}

const char* type_name(JsonType type) noexcept {
    switch (type) {
        case JsonType::Null: return "null";
        case JsonType::Boolean: return "boolean";
        case JsonType::Integer: return "integer";
        case JsonType::Double: return "double";
        case JsonType::String: return "string";
        case JsonType::Array: return "array";
        case JsonType::Object: return "object";
    }
    return "unknown";
}

} // namespace fastjson
