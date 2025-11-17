# JSON Object Mapping Guide

Complete reference for mapping JSON structures to C++ types, including std::unordered_map and custom classes.

## Table of Contents

1. [Basic Type Mappings](#basic-type-mappings)
2. [JSON Objects to Containers](#json-objects-to-containers)
3. [Custom Class Mapping](#custom-class-mapping)
4. [Advanced Patterns](#advanced-patterns)
5. [Error Handling](#error-handling)
6. [Performance Considerations](#performance-considerations)

## Basic Type Mappings

### JSON ↔ C++ Type Correspondence

| JSON Type | C++ Type | Notes |
|-----------|----------|-------|
| `null` | `std::monostate` (variant) | Represents absence |
| `true`/`false` | `bool` | Direct boolean |
| `42` | `int64_t` | Integer numbers |
| `3.14` | `double` | Floating-point numbers |
| `"text"` | `std::string` | UTF-8 encoded strings |
| `[...]` | `std::vector<T>` or `fastjson::json_array` | Homogeneous/heterogeneous |
| `{...}` | `std::unordered_map<std::string, T>` or `fastjson::json_object` | Key-value pairs |

### Basic Example

```cpp
#include "modules/json_linq.h"
import fastjson;

using namespace fastjson;

// JSON: {"id": 42, "active": true, "score": 3.14, "name": "Alice"}
json_object obj = {
    {"id", int64_t(42)},
    {"active", true},
    {"score", 3.14},
    {"name", std::string("Alice")}
};

// Access individual fields
auto id = std::get<int64_t>(obj["id"]);           // 42
auto active = std::get<bool>(obj["active"]);      // true
auto score = std::get<double>(obj["score"]);      // 3.14
auto name = std::get<std::string>(obj["name"]);   // "Alice"
```

## JSON Objects to Containers

### std::unordered_map Mapping

The primary container for JSON objects is `std::unordered_map<std::string, T>`, where `T` is the value type.

#### Simple Objects to unordered_map

```cpp
import fastjson;
using namespace fastjson;

// JSON: {"x": 1, "y": 2, "z": 3}
std::string json_str = R"({"x": 1, "y": 2, "z": 3})";
auto parsed = parse(json_str);

if (parsed.has_value()) {
    auto& coords = std::get<json_object>(parsed.value());
    
    // Convert to std::unordered_map<std::string, int64_t>
    std::unordered_map<std::string, int64_t> coord_map;
    for (const auto& [key, value] : coords) {
        coord_map[key] = std::get<int64_t>(value);
    }
    
    // Access: coord_map["x"] == 1
}
```

#### Nested Objects with unordered_map

```cpp
// JSON: {"user": {"name": "Alice", "age": 30}, "status": "active"}
std::string json_str = R"({
    "user": {"name": "Alice", "age": 30},
    "status": "active"
})";

auto parsed = parse(json_str);
if (parsed.has_value()) {
    auto& root = std::get<json_object>(parsed.value());
    
    // Access nested object
    auto& user_obj = std::get<json_object>(root["user"]);
    auto name = std::get<std::string>(user_obj["name"]);
    auto age = std::get<int64_t>(user_obj["age"]);
    
    // Convert to nested unordered_map
    std::unordered_map<std::string, 
        std::unordered_map<std::string, fastjson::json_value>> nested_map;
    
    for (const auto& [key, value] : root) {
        if (std::holds_alternative<json_object>(value)) {
            nested_map[key] = std::get<json_object>(value);
        }
    }
}
```

#### Heterogeneous Objects

For objects with mixed value types, use `std::unordered_map<std::string, json_value>`:

```cpp
// JSON: {"id": 1, "name": "test", "tags": ["a", "b"], "active": true}
std::string json_str = R"({
    "id": 1,
    "name": "test",
    "tags": ["a", "b"],
    "active": true
})";

auto parsed = parse(json_str);
if (parsed.has_value()) {
    // Direct access - already a map with mixed value types
    auto& obj = std::get<json_object>(parsed.value());
    
    // Visit each value
    for (const auto& [key, value] : obj) {
        if (std::holds_alternative<int64_t>(value)) {
            auto num = std::get<int64_t>(value);
            // Handle integer
        } else if (std::holds_alternative<std::string>(value)) {
            auto str = std::get<std::string>(value);
            // Handle string
        } else if (std::holds_alternative<json_array>(value)) {
            auto arr = std::get<json_array>(value);
            // Handle array
        } else if (std::holds_alternative<bool>(value)) {
            auto flag = std::get<bool>(value);
            // Handle boolean
        } else if (std::holds_alternative<double>(value)) {
            auto num = std::get<double>(value);
            // Handle float
        } else if (std::holds_alternative<std::monostate>(value)) {
            // null value
        }
    }
}
```

### fastjson::json_object (Alias for unordered_map)

`fastjson::json_object` is defined as:

```cpp
using json_object = std::unordered_map<std::string, json_value>;
using json_array = std::vector<json_value>;
using json_value = std::variant<
    std::monostate,      // null
    bool,                // boolean
    int64_t,             // integer
    double,              // floating-point
    std::string,         // string
    json_array,          // array
    json_object          // object
>;
```

So all `json_object` operations are standard `unordered_map` operations:

```cpp
json_object obj = ...;

// Insert/update
obj["new_key"] = 42;

// Find/access
if (obj.count("key") > 0) {
    auto value = obj["key"];
}

// Iterate
for (const auto& [key, value] : obj) {
    // Process
}

// Delete
obj.erase("key");

// Size
size_t count = obj.size();
```

## Custom Class Mapping

### Mapping to Struct

Create structures that mirror JSON object structure:

```cpp
#include <string>
#include <vector>

struct User {
    int64_t id;
    std::string name;
    std::string email;
    int32_t age;
    bool active;
    
    // Factory method from json_object
    static User from_json(const fastjson::json_object& obj) {
        return User{
            std::get<int64_t>(obj.at("id")),
            std::get<std::string>(obj.at("name")),
            std::get<std::string>(obj.at("email")),
            static_cast<int32_t>(std::get<int64_t>(obj.at("age"))),
            std::get<bool>(obj.at("active"))
        };
    }
    
    // Convert back to json_object
    fastjson::json_object to_json() const {
        return {
            {"id", id},
            {"name", name},
            {"email", email},
            {"age", int64_t(age)},
            {"active", active}
        };
    }
};

// Usage
std::string json_str = R"({
    "id": 1,
    "name": "Alice",
    "email": "alice@example.com",
    "age": 30,
    "active": true
})";

auto parsed = fastjson::parse(json_str);
if (parsed.has_value()) {
    auto& obj = std::get<fastjson::json_object>(parsed.value());
    User user = User::from_json(obj);
    
    // user.name == "Alice"
    // user.age == 30
    
    // Convert back
    auto json_back = user.to_json();
}
```

### Mapping Collections of Objects

```cpp
struct Product {
    int64_t id;
    std::string title;
    double price;
    std::vector<std::string> tags;
    
    static Product from_json(const fastjson::json_object& obj) {
        const auto& tags_arr = std::get<fastjson::json_array>(obj.at("tags"));
        std::vector<std::string> tags;
        for (const auto& tag_val : tags_arr) {
            tags.push_back(std::get<std::string>(tag_val));
        }
        
        return Product{
            std::get<int64_t>(obj.at("id")),
            std::get<std::string>(obj.at("title")),
            std::get<double>(obj.at("price")),
            tags
        };
    }
};

// JSON: [{"id": 1, "title": "Item 1", "price": 9.99, "tags": ["tag1", "tag2"]}, ...]
std::string json_str = R"([
    {"id": 1, "title": "Item 1", "price": 9.99, "tags": ["tag1", "tag2"]},
    {"id": 2, "title": "Item 2", "price": 19.99, "tags": ["tag2", "tag3"]}
])";

auto parsed = fastjson::parse(json_str);
if (parsed.has_value()) {
    auto& arr = std::get<fastjson::json_array>(parsed.value());
    std::vector<Product> products;
    
    for (const auto& item : arr) {
        auto& obj = std::get<fastjson::json_object>(item);
        products.push_back(Product::from_json(obj));
    }
}
```

### Template-Based Mapping

For reusable type conversion:

```cpp
template<typename T>
class JsonMapper {
public:
    static std::vector<T> map_array(const fastjson::json_array& arr) {
        std::vector<T> result;
        for (const auto& item : arr) {
            result.push_back(T::from_json(std::get<fastjson::json_object>(item)));
        }
        return result;
    }
    
    static fastjson::json_object map_to_json(const T& obj) {
        return obj.to_json();
    }
};

// Usage
auto products = JsonMapper<Product>::map_array(arr);
```

## Advanced Patterns

### Optional Fields

Handle missing JSON fields gracefully:

```cpp
struct Config {
    std::string host;
    int32_t port;
    bool debug = false;  // Default value
    std::optional<std::string> api_key = std::nullopt;
    
    static Config from_json(const fastjson::json_object& obj) {
        Config cfg;
        cfg.host = std::get<std::string>(obj.at("host"));
        cfg.port = static_cast<int32_t>(std::get<int64_t>(obj.at("port")));
        
        // Optional fields
        if (obj.count("debug") > 0) {
            cfg.debug = std::get<bool>(obj.at("debug"));
        }
        if (obj.count("api_key") > 0) {
            auto& val = obj.at("api_key");
            if (!std::holds_alternative<std::monostate>(val)) {
                cfg.api_key = std::get<std::string>(val);
            }
        }
        
        return cfg;
    }
};
```

### Polymorphic Objects

Handle different object types based on discriminator field:

```cpp
struct Message {
    std::string type;
    
    static std::variant<std::monostate, fastjson::json_object> 
    parse_message(const fastjson::json_object& obj) {
        auto type = std::get<std::string>(obj.at("type"));
        
        if (type == "user_message") {
            // Validate user message fields
            if (obj.count("user_id") && obj.count("content")) {
                return obj;
            }
        } else if (type == "system_message") {
            // Validate system message fields
            if (obj.count("code") && obj.count("description")) {
                return obj;
            }
        }
        
        return std::monostate{};  // Invalid
    }
};
```

### Array of Mixed Types to Tuple

```cpp
// JSON: ["Alice", 30, true]
std::tuple<std::string, int64_t, bool> 
extract_tuple(const fastjson::json_array& arr) {
    return std::make_tuple(
        std::get<std::string>(arr[0]),
        std::get<int64_t>(arr[1]),
        std::get<bool>(arr[2])
    );
}
```

### Flatten Nested Objects

```cpp
std::unordered_map<std::string, fastjson::json_value>
flatten_object(
    const fastjson::json_object& obj,
    const std::string& prefix = ""
) {
    std::unordered_map<std::string, fastjson::json_value> result;
    
    for (const auto& [key, value] : obj) {
        std::string full_key = prefix.empty() ? key : prefix + "." + key;
        
        if (std::holds_alternative<fastjson::json_object>(value)) {
            auto nested = std::get<fastjson::json_object>(value);
            auto flattened = flatten_object(nested, full_key);
            result.insert(flattened.begin(), flattened.end());
        } else {
            result[full_key] = value;
        }
    }
    
    return result;
}

// JSON: {"user": {"name": "Alice", "age": 30}}
// Result: {"user.name": "Alice", "user.age": 30}
```

## Error Handling

### Safe Access Patterns

```cpp
// Pattern 1: Using at() with try-catch
try {
    auto name = std::get<std::string>(obj.at("name"));
} catch (const std::out_of_range& e) {
    std::cerr << "Key not found: " << e.what() << std::endl;
} catch (const std::bad_variant_access& e) {
    std::cerr << "Type mismatch: " << e.what() << std::endl;
}

// Pattern 2: Check before access
if (obj.count("name") > 0 && 
    std::holds_alternative<std::string>(obj["name"])) {
    auto name = std::get<std::string>(obj["name"]);
}

// Pattern 3: Using find
auto it = obj.find("name");
if (it != obj.end() && 
    std::holds_alternative<std::string>(it->second)) {
    auto name = std::get<std::string>(it->second);
}

// Pattern 4: Get with default
std::string get_string_or_default(
    const fastjson::json_object& obj,
    const std::string& key,
    const std::string& default_value = ""
) {
    auto it = obj.find(key);
    if (it != obj.end() && 
        std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
    }
    return default_value;
}
```

### Type Validation Helper

```cpp
class TypeValidator {
public:
    static bool is_integer(const fastjson::json_value& val) {
        return std::holds_alternative<int64_t>(val);
    }
    
    static bool is_string(const fastjson::json_value& val) {
        return std::holds_alternative<std::string>(val);
    }
    
    static bool is_array(const fastjson::json_value& val) {
        return std::holds_alternative<fastjson::json_array>(val);
    }
    
    static bool is_object(const fastjson::json_value& val) {
        return std::holds_alternative<fastjson::json_object>(val);
    }
    
    static bool is_bool(const fastjson::json_value& val) {
        return std::holds_alternative<bool>(val);
    }
    
    static bool is_null(const fastjson::json_value& val) {
        return std::holds_alternative<std::monostate>(val);
    }
};
```

## Performance Considerations

### Memory Efficiency

1. **Use References**: Avoid copying large objects
   ```cpp
   // Good
   const auto& obj = std::get<fastjson::json_object>(value);
   for (const auto& [key, val] : obj) { ... }
   
   // Avoid (creates copy)
   auto obj = std::get<fastjson::json_object>(value);
   ```

2. **Reserve Space**: Pre-allocate for known sizes
   ```cpp
   std::vector<User> users;
   users.reserve(expected_count);
   ```

3. **Move Semantics**: Use std::move for large objects
   ```cpp
   return std::move(users);
   ```

### Parsing Performance

1. **Parallel Parsing**: For large JSON arrays
   ```cpp
   #include "modules/json_linq.h"
   using namespace fastjson::linq;
   
   auto products = from_parallel(json_array)
       .where([](const auto& item) { /* filter */ })
       .select([](const auto& item) { 
           return Product::from_json(
               std::get<fastjson::json_object>(item)
           );
       })
       .to_vector();
   ```

2. **Lazy Evaluation**: Process data as needed
   ```cpp
   auto query = from(json_array)
       .where(predicate)
       .select(transform);
   
   // Evaluation deferred until iteration
   for (auto item : query) {
       // Process one at a time
   }
   ```

### Query Optimization

1. **Filter Early**: Apply restrictions before transformations
   ```cpp
   // Good - filters before transform
   from(data).where(filter).select(transform)
   
   // Less efficient - transforms then filters
   from(data).select(transform).where(filter)
   ```

2. **Short-Circuit Operations**: Use find() or any()
   ```cpp
   // Stops at first match
   auto first = from(data).where(predicate).first_or_default();
   
   // Stops at first true
   bool has_any = from(data).any(predicate);
   ```

## Complete Example

```cpp
#include "modules/json_linq.h"
import fastjson;
using namespace fastjson;

// Define structure
struct TeamMember {
    int64_t id;
    std::string name;
    std::string role;
    std::vector<std::string> skills;
    
    static TeamMember from_json(const json_object& obj) {
        const auto& skills_arr = std::get<json_array>(obj.at("skills"));
        std::vector<std::string> skills;
        for (const auto& skill : skills_arr) {
            skills.push_back(std::get<std::string>(skill));
        }
        return TeamMember{
            std::get<int64_t>(obj.at("id")),
            std::get<std::string>(obj.at("name")),
            std::get<std::string>(obj.at("role")),
            skills
        };
    }
};

// Parse and process
std::string json = R"([
    {"id": 1, "name": "Alice", "role": "lead", "skills": ["C++", "SIMD"]},
    {"id": 2, "name": "Bob", "role": "dev", "skills": ["C++", "Python"]},
    {"id": 3, "name": "Carol", "role": "dev", "skills": ["Rust", "Go"]}
])";

auto parsed = parse(json);
if (parsed.has_value()) {
    auto& arr = std::get<json_array>(parsed.value());
    
    // Map to objects
    std::vector<TeamMember> team;
    for (const auto& item : arr) {
        team.push_back(TeamMember::from_json(
            std::get<json_object>(item)
        ));
    }
    
    // Query: Find all C++ developers
    auto cpp_devs = from(team)
        .where([](const auto& member) {
            return std::any_of(
                member.skills.begin(), 
                member.skills.end(),
                [](const auto& skill) { return skill == "C++"; }
            );
        })
        .select([](const auto& member) { return member.name; })
        .to_vector();
    // Result: ["Alice", "Bob"]
}
```

---

**Key Takeaway**: Use `fastjson::json_object` (alias for `std::unordered_map<std::string, json_value>`) as your primary container, then map to custom structures using factory methods (`from_json`) for type-safe, ergonomic JSON handling.

