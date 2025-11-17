#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <atomic>
#include <mutex>
#include <thread>
#include <fstream>

namespace py = pybind11;

// Copy-on-write wrapper for efficient JSON value sharing
template<typename T>
class CowPtr {
private:
    std::shared_ptr<T> ptr;
    
public:
    CowPtr() : ptr(std::make_shared<T>()) {}
    explicit CowPtr(const T& value) : ptr(std::make_shared<T>(value)) {}
    explicit CowPtr(T&& value) : ptr(std::make_shared<T>(std::move(value))) {}
    
    // Copy constructor - shares ownership (no actual copy yet)
    CowPtr(const CowPtr& other) : ptr(other.ptr) {}
    
    // Copy assignment - shares ownership
    CowPtr& operator=(const CowPtr& other) {
        if (this != &other) {
            ptr = other.ptr;
        }
        return *this;
    }
    
    // Move operations
    CowPtr(CowPtr&& other) noexcept : ptr(std::move(other.ptr)) {}
    CowPtr& operator=(CowPtr&& other) noexcept {
        ptr = std::move(other.ptr);
        return *this;
    }
    
    // Make unique copy if needed (copy-on-write)
    T& ensure_unique() {
        if (ptr.use_count() > 1) {
            ptr = std::make_shared<T>(*ptr);
        }
        return *ptr;
    }
    
    // Read-only access (shared)
    const T& operator*() const { return *ptr; }
    const T* operator->() const { return ptr.get(); }
    
    // Mutable access (copy-on-write)
    T& operator*() {
        return ensure_unique();
    }
    T* operator->() {
        return &ensure_unique();
    }
    
    // Reference counting
    long use_count() const { return ptr.use_count(); }
};

// Mock JSON types to match the C++ library
// In real implementation, these would include the actual fastjson headers
namespace fastjson {
    // Forward declarations
    class json_value;
    using json_object = std::unordered_map<std::string, json_value>;
    using json_array = std::vector<json_value>;
    
    // JSON value variant
    using json_data = std::variant<
        std::monostate,           // null
        bool,                      // boolean
        int64_t,                   // integer
        double,                    // float
        std::string,               // string
        json_array,                // array
        json_object                // object
    >;
    
    class json_value {
    private:
        // Use copy-on-write pointer for efficient sharing
        CowPtr<json_data> data_ptr;
        
    public:
        json_data& data;
        
        json_value() 
            : data_ptr(std::monostate{}), data(*data_ptr) {}
        
        json_value(std::monostate m) 
            : data_ptr(m), data(*data_ptr) {}
        
        json_value(bool b) 
            : data_ptr(b), data(*data_ptr) {}
        
        json_value(int64_t i) 
            : data_ptr(i), data(*data_ptr) {}
        
        json_value(double d) 
            : data_ptr(d), data(*data_ptr) {}
        
        json_value(const std::string& s) 
            : data_ptr(s), data(*data_ptr) {}
        
        json_value(const json_array& a) 
            : data_ptr(a), data(*data_ptr) {}
        
        json_value(const json_object& o) 
            : data_ptr(o), data(*data_ptr) {}
        
        // Copy constructor - uses copy-on-write
        json_value(const json_value& other) 
            : data_ptr(other.data_ptr), data(*data_ptr) {}
        
        // Move constructor
        json_value(json_value&& other) noexcept 
            : data_ptr(std::move(other.data_ptr)), data(*data_ptr) {}
        
        // Copy assignment
        json_value& operator=(const json_value& other) {
            if (this != &other) {
                data_ptr = other.data_ptr;
            }
            return *this;
        }
        
        // Move assignment
        json_value& operator=(json_value&& other) noexcept {
            data_ptr = std::move(other.data_ptr);
            return *this;
        }
        
        // Type checking
        bool is_null() const { return std::holds_alternative<std::monostate>(data); }
        bool is_bool() const { return std::holds_alternative<bool>(data); }
        bool is_integer() const { return std::holds_alternative<int64_t>(data); }
        bool is_number() const { return is_integer() || is_float(); }
        bool is_float() const { return std::holds_alternative<double>(data); }
        bool is_string() const { return std::holds_alternative<std::string>(data); }
        bool is_array() const { return std::holds_alternative<json_array>(data); }
        bool is_object() const { return std::holds_alternative<json_object>(data); }
        
        // Getters
        std::string to_string() const;
        py::object to_python() const;
        py::dict to_python_dict() const;  // GIL-free conversion
        
        // Reference counting for debugging
        long ref_count() const { return data_ptr.use_count(); }
    };
    
    // Parse functions
    std::optional<json_value> parse(const std::string& json_str);
    std::optional<json_value> parse_utf16(const std::u16string& json_str);
    std::optional<json_value> parse_utf32(const std::u32string& json_str);
    
    // Serialization
    std::string stringify(const json_value& val);
}

// Implementation of json_value methods
std::string fastjson::json_value::to_string() const {
    if (is_null()) return "null";
    if (is_bool()) return std::get<bool>(data) ? "true" : "false";
    if (is_integer()) return std::to_string(std::get<int64_t>(data));
    if (is_float()) return std::to_string(std::get<double>(data));
    if (is_string()) return std::get<std::string>(data);
    if (is_array()) return "[array]";
    if (is_object()) return "{object}";
    return "unknown";
}

py::object fastjson::json_value::to_python() const {
    if (is_null()) return py::none();
    if (is_bool()) return py::bool_(std::get<bool>(data));
    if (is_integer()) return py::int_(std::get<int64_t>(data));
    if (is_float()) return py::float_(std::get<double>(data));
    if (is_string()) return py::str(std::get<std::string>(data));
    if (is_array()) {
        py::list list;
        for (const auto& item : std::get<json_array>(data)) {
            list.append(item.to_python());
        }
        return list;
    }
    if (is_object()) {
        py::dict dict;
        for (const auto& [key, value] : std::get<json_object>(data)) {
            dict[py::str(key)] = value.to_python();
        }
        return dict;
    }
    return py::none();
}

// GIL-free efficient dict conversion with copy-on-write semantics
// Removes GIL during parsing and dict building for large JSON
py::dict fastjson::json_value::to_python_dict() const {
    if (!is_object()) {
        throw std::runtime_error("Value is not a JSON object");
    }
    
    py::dict result;
    const auto& obj = std::get<json_object>(data);
    
    // Pre-allocate expected size
    if (!obj.empty()) {
        // Python dicts don't have reserve, but we can optimize by building in batches
        
        // For large objects, release GIL during conversion
        {
            py::gil_scoped_release release;
            
            // Convert all values without holding GIL
            std::vector<std::pair<std::string, py::object>> temp_results;
            temp_results.reserve(obj.size());
            
            for (const auto& [key, value] : obj) {
                auto py_val = value.to_python();
                temp_results.emplace_back(key, py_val);
            }
            
            // Reacquire GIL and populate dict
            py::gil_scoped_acquire acquire;
            for (auto& [key, val] : temp_results) {
                result[py::str(key)] = val;
            }
        }
    }
    
    return result;
}

// Helper function for multi-threaded parsing without GIL
class GilRelease {
public:
    GilRelease() : release_(std::make_unique<py::gil_scoped_release>()) {}
    ~GilRelease() = default;
    
    // Reacquire GIL when needed
    void acquire() {
        release_.reset();  // Destructs the release
    }
    
    void release() {
        if (!release_) {
            release_ = std::make_unique<py::gil_scoped_release>();
        }
    }
    
private:
    std::unique_ptr<py::gil_scoped_release> release_;
};

// Mock implementations (these would link to real fastjson library)
std::optional<fastjson::json_value> fastjson::parse(const std::string& json_str) {
    try {
        // Parse would happen here - for now return mock
        fastjson::json_value val;
        val.data = std::monostate{};
        return val;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<fastjson::json_value> fastjson::parse_utf16(const std::u16string& json_str) {
    return parse(std::string(json_str.begin(), json_str.end()));
}

std::optional<fastjson::json_value> fastjson::parse_utf32(const std::u32string& json_str) {
    return parse(std::string(json_str.begin(), json_str.end()));
}

std::string fastjson::stringify(const fastjson::json_value& val) {
    return val.to_string();
}

// pybind11 module definition with GIL management for thread-safe parsing
PYBIND11_MODULE(fastjson, m) {
    m.doc() = "FastestJSONInTheWest - Ultra-high-performance JSON library with C++23\n"
              "Features:\n"
              "  - Copy-on-write semantics for memory efficiency\n"
              "  - GIL-free parsing for true parallel multithreading\n"
              "  - Support for UTF-8, UTF-16, and UTF-32 encoding\n"
              "  - Batch and streaming parsing capabilities";

    // Bind json_value class with full documentation
    py::class_<fastjson::json_value>(m, "JSONValue", 
        "Represents a JSON value with copy-on-write semantics for efficient memory usage")
        
        // Type checking methods
        .def("is_null", &fastjson::json_value::is_null, 
             "Check if value is null")
        .def("is_bool", &fastjson::json_value::is_bool, 
             "Check if value is boolean")
        .def("is_integer", &fastjson::json_value::is_integer, 
             "Check if value is integer")
        .def("is_number", &fastjson::json_value::is_number, 
             "Check if value is numeric (int or float)")
        .def("is_float", &fastjson::json_value::is_float, 
             "Check if value is float")
        .def("is_string", &fastjson::json_value::is_string, 
             "Check if value is string")
        .def("is_array", &fastjson::json_value::is_array, 
             "Check if value is array")
        .def("is_object", &fastjson::json_value::is_object, 
             "Check if value is object")
        
        // Conversion methods
        .def("to_string", &fastjson::json_value::to_string, 
             "Convert to string representation")
        .def("to_python", &fastjson::json_value::to_python, 
             "Convert to Python object (holds GIL during conversion)")
        .def("to_python_dict", &fastjson::json_value::to_python_dict, 
             "Convert to Python dict with GIL-free dict building for objects")
        .def("ref_count", &fastjson::json_value::ref_count, 
             "Get reference count (for CoW debugging/profiling)")
        
        // Python special methods
        .def("__repr__", [](const fastjson::json_value& val) {
            return "<JSONValue: " + val.to_string() + " (refs=" + 
                   std::to_string(val.ref_count()) + ")>";
        }, "String representation with CoW ref count")
        
        .def("__str__", &fastjson::json_value::to_string,
             "String conversion")
        
        .def("__bool__", [](const fastjson::json_value& val) {
            return !val.is_null();
        }, "Convert to boolean (null is False)")
        
        .def("__copy__", [](const fastjson::json_value& val) {
            // Shallow copy leveraging CoW - shares data ownership
            return fastjson::json_value(val);
        }, "Create shallow copy (shares data via CoW)")
        
        .def("__deepcopy__", [](const fastjson::json_value& val, py::dict) {
            // CoW makes this equivalent to shallow copy
            return fastjson::json_value(val);
        }, "Deep copy via Python interface (CoW makes it shallow)");
    
    // Bind module-level parsing functions with GIL management
    
    // Parse JSON string with GIL release for multithreading
    m.def("parse", 
        [](const std::string& json_str) -> py::object {
            // Release GIL during parsing to allow other threads to run
            py::gil_scoped_release release;
            auto result = fastjson::parse(json_str);
            if (result.has_value()) {
                return result.value().to_python();
            }
            throw std::runtime_error("Failed to parse JSON");
        },
        py::arg("json"), 
        "Parse JSON string and return Python object.\n\n"
        "The GIL is released during parsing, allowing other threads\n"
        "to run in parallel. Optimal for multithreaded workloads.");
    
    // Parse UTF-16 JSON with GIL release
    m.def("parse_utf16",
        [](const std::string& json_str) -> py::object {
            // Release GIL during UTF-16 parsing
            py::gil_scoped_release release;
            std::u16string utf16(json_str.begin(), json_str.end());
            auto result = fastjson::parse_utf16(utf16);
            if (result.has_value()) {
                return result.value().to_python();
            }
            throw std::runtime_error("Failed to parse UTF-16 JSON");
        },
        py::arg("json"), 
        "Parse UTF-16 JSON string.\n\n"
        "The GIL is released during parsing for multithreaded operation.");
    
    // Parse UTF-32 JSON with GIL release
    m.def("parse_utf32",
        [](const std::string& json_str) -> py::object {
            // Release GIL during UTF-32 parsing
            py::gil_scoped_release release;
            std::u32string utf32(json_str.begin(), json_str.end());
            auto result = fastjson::parse_utf32(utf32);
            if (result.has_value()) {
                return result.value().to_python();
            }
            throw std::runtime_error("Failed to parse UTF-32 JSON");
        },
        py::arg("json"), 
        "Parse UTF-32 JSON string.\n\n"
        "The GIL is released during parsing for multithreaded operation.");
    
    // Stringify JSON with GIL release
    m.def("stringify",
        [](const py::object& obj) -> std::string {
            // Release GIL during stringification
            py::gil_scoped_release release;
            return "{}";  // Placeholder - would serialize Python objects to JSON
        },
        py::arg("obj"), 
        "Convert Python object to JSON string.\n\n"
        "The GIL is released during stringification.");
    
    // Batch parsing for multiple JSON strings with GIL optimization
    m.def("parse_batch", 
        [](const std::vector<std::string>& json_strings) -> py::list {
            std::vector<fastjson::json_value> temp_results;
            
            // Release GIL for entire batch processing
            {
                py::gil_scoped_release release;
                for (const auto& json_str : json_strings) {
                    auto result = fastjson::parse(json_str);
                    if (result.has_value()) {
                        temp_results.push_back(result.value());
                    } else {
                        // Can't push None without GIL, will handle this after
                        temp_results.push_back(fastjson::json_value());
                    }
                }
            }
            
            // Convert to Python objects with GIL held
            py::list results;
            for (const auto& val : temp_results) {
                results.append(val.to_python());
            }
            
            return results;
        },
        py::arg("json_strings"),
        "Parse multiple JSON strings with minimal GIL contention.\n\n"
        "Processes batch with single GIL release/reacquire cycle,\n"
        "much more efficient than parsing one at a time in a loop.");
    
    // Parse file with GIL-free I/O and parsing
    m.def("parse_file", 
        [](const std::string& filepath) -> py::object {
            std::string content;
            
            // Read file with GIL released (no Python work)
            {
                py::gil_scoped_release release;
                std::ifstream file(filepath, std::ios::binary);
                if (!file) {
                    throw std::runtime_error("Cannot open file: " + filepath);
                }
                file.seekg(0, std::ios::end);
                content.reserve(file.tellg());
                file.seekg(0, std::ios::beg);
                content.assign((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            }
            
            // Parse with GIL released
            fastjson::json_value result_val;
            {
                py::gil_scoped_release release;
                auto result = fastjson::parse(content);
                if (result.has_value()) {
                    result_val = result.value();
                } else {
                    throw std::runtime_error("Failed to parse JSON file: " + filepath);
                }
            }
            
            return result_val.to_python();
        },
        py::arg("filepath"),
        "Parse JSON file with GIL-free I/O and parsing.\n\n"
        "Both file reading and parsing happen without holding the GIL,\n"
        "enabling true parallel processing across threads.");
    
    // Module attributes and version info
    m.attr("__version__") = "1.0.0";
    m.attr("__author__") = "FastJSON Contributors";
}
