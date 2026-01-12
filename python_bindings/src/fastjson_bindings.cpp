// FastestJSONInTheWest Python Bindings v2.0
// Uses nanobind, C++23 modules, and UV
// Copyright (c) 2026 Olumuyiwa Oluwasanmi

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/function.h>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <format>

/* 
   Mixed Mode: Imports + Includes
   We use 'import std;' for C++23 compliance handling.
   Nanobind requires traditional headers.
*/
import std;
import fastjson;
import json_template;
// Import linq/parallel explicitly if they are separate modules, though they might be exported by fastjson logic
// In this case, fastjson exports linq/parallel namespaces, or we import them if they are module partitions.
// Re-check: json_template is separate.
// fastjson core exports the fastjson namespace.

namespace nb = nanobind;
using namespace nb::literals;
using namespace fastjson;

// --- Helper Functions ---

// Convert C++ JSONValue to Python Object (Recursive)
nb::object to_python(const json_value& v) {
    if (v.is_null()) {
        return nb::none();
    } else if (v.is_boolean()) {
        return nb::bool_(v.as_boolean());
    } else if (v.is_number()) {
        double d = v.as_number();
        // Heuristic: Check if integer-like for cleaner python types
        double int_part;
        if (std::modf(d, &int_part) == 0.0) {
            // Check bounds for basic int64
             return nb::int_((int64_t)d);
        }
        return nb::float_(d);
    } else if (v.is_string()) {
        return nb::str(v.as_string().c_str());
    } else if (v.is_array()) {
        nb::list l;
        const auto& arr = v.as_array();
        for (const auto& item : arr) {
            l.append(to_python(item));
        }
        return l;
    } else if (v.is_object()) {
        nb::dict d;
        const auto& obj = v.as_object();
        for (const auto& [key, val] : obj) {
            d[key.c_str()] = to_python(val);
        }
        return d;
    } else if (v.is_int_128()) {
        // 128-bit integer support
        // Convert to string, then let Python parse it into arbitrary precision int
        return nb::int_(fastjson::to_string(v).c_str());
    } else if (v.is_uint_128()) {
         return nb::int_(fastjson::to_string(v).c_str());
    } else if (v.is_number_128()) {
        // float128 -> python float (64-bit) 
        // Python doesn't support 128-bit float natively.
        return nb::float_((double)v.as_float128());
    }
    return nb::none();
}

namespace fastjson::bindings {

template <typename Func>
auto with_gil_release(Func&& f) {
    nb::gil_scoped_release release;
    return f();
}

template<typename T>
void bind_numeric_vector(nb::module_& m, const char* name) {
    nb::class_<std::vector<T>>(m, name)
        .def(nb::init<>())
        .def("push_back", [](std::vector<T>& v, T val) { v.push_back(val); })
        .def("append", [](std::vector<T>& v, T val) { v.push_back(val); })
        .def("__len__", [](const std::vector<T>& v) { return v.size(); })
        .def("__getitem__", [](const std::vector<T>& v, size_t i) {
            if (i >= v.size()) throw nb::index_error();
            return v[i];
        })
        .def("to_list", [](const std::vector<T>& v) {
            nb::list l;
            for (auto x : v) l.append(x);
            return l;
        });
}

} // namespace fastjson::bindings

// --- Module Definition ---

NB_MODULE(fastjson, m) {
    m.doc() = "FastestJSONInTheWest Python Bindings (C++23/Nanobind/SIMD)";

    // --- Core JSON Value ---

    nb::class_<json_value>(m, "JSONValue")
        .def(nb::init<>())
        .def("to_python", [](const json_value& self) {
            return to_python(self);
        }, "Convert to native Python objects (recursive, handles 128-bit ints)")
        .def("__str__", [](const json_value& self) {
            return fastjson::stringify(self);
        })
        .def("__repr__", [](const json_value& self) {
            return std::format("<JSONValue: {}>", fastjson::stringify(self));
        })
        // Accessors
        .def("__getitem__", [](const json_value& self, std::string key) {
             if (!self.is_object()) throw std::runtime_error("Not an object");
             if (!self.contains(key)) throw nb::key_error(key.c_str());
             return self[key];
        })
        .def("__getitem__", [](const json_value& self, size_t index) {
             if (!self.is_array()) throw std::runtime_error("Not an array");
             if (index >= self.size()) throw nb::index_error();
             return self[index];
        })
        .def("__len__", [](const json_value& self) {
            return self.size();
        })
        // Type checks
        .def_prop_ro("is_null", &json_value::is_null)
        .def_prop_ro("is_bool", &json_value::is_boolean)
        .def_prop_ro("is_number", &json_value::is_number)
        .def_prop_ro("is_string", &json_value::is_string)
        .def_prop_ro("is_array", &json_value::is_array)
        .def_prop_ro("is_object", &json_value::is_object)
        // Value access
        .def("as_int", &json_value::as_int64)
        .def("as_float", &json_value::as_float64)
        .def("as_str", &json_value::as_string);

    // --- Main Parser API ---

    m.def("parse", [](std::string_view json_str) -> json_value {
        return fastjson::bindings::with_gil_release([&] {
            auto result = fastjson::parse(json_str);
            if (!result) {
                throw std::runtime_error(result.error().message);
            }
            return *result;
        });
    }, "json_str"_a, "Parse JSON string (SIMD accelerated, 128-bit support)");

    m.def("parse_file", [](std::string filename) -> json_value {
        return fastjson::bindings::with_gil_release([&] {
            std::ifstream f(filename, std::ios::binary | std::ios::ate);
            if (!f) throw std::runtime_error("Cannot open file: " + filename);
            
            auto size = f.tellg();
            std::string str(size, '\0');
            f.seekg(0);
            if (!f.read(str.data(), size)) throw std::runtime_error("Read error");
            
            auto result = fastjson::parse(str);
             if (!result) {
                throw std::runtime_error(result.error().message);
            }
            return *result;
        });
    }, "filename"_a, "Parse JSON from file path");

    // --- Parallel Parser API ---

    m.def("parse_parallel", [](std::string_view json_str) -> json_value {
        return fastjson::bindings::with_gil_release([&] {
             #ifdef _OPENMP
                auto result = fastjson_parallel::parse(json_str); 
             #else
                auto result = fastjson::parse(json_str);
             #endif
            if (!result) throw std::runtime_error(result.error().message);
            return *result;
        });
    }, "json_str"_a, "Parse large JSON string using OpenMP");
    
    // --- LINQ API ---
    // Exposing the Query Wrapper
    
    using Query = fastjson::linq::query_wrapper<json_value>; 

    nb::class_<Query>(m, "Query")
        .def("where", [](Query& self, std::function<bool(const json_value&)> pred) {
            return self.where([pred](const json_value& v) { return pred(v); });
        })
        .def("select", [](Query& self, std::function<json_value(const json_value&)> transform) {
            return self.select([transform](const json_value& v) { return transform(v); });
        })
        .def("to_list", [](Query& self) {
            std::vector<json_value> vec = self.to_vector();
            return vec;
        });

    m.def("query", [](const json_value& source) {
        return fastjson::linq::from(source);
    }, "Create a LINQ query from a JSONValue");

    // --- Mustache Templating API ---
    m.def("render_template", [](std::string_view tmpl, const json_value& data) {
        return fastjson::mustache::render(tmpl, data);
    }, "template"_a, "data"_a, "Render a connection-less Mustache template using the JSON data model");

    // --- Template Instantiations (Vectors) ---
    fastjson::bindings::bind_numeric_vector<int64_t>(m, "Int64Vector");
    fastjson::bindings::bind_numeric_vector<double>(m, "DoubleVector");
}
