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
import fastjson;
import fastjson_parallel;
import json_linq;
import json_template;

namespace nb = nanobind;
using namespace nb::literals;
using namespace fastjson_parallel;

NB_MAKE_OPAQUE(std::vector<int64_t>);
NB_MAKE_OPAQUE(std::vector<double>);

// --- Helper Functions ---

// Convert C++ JSONValue to Python Object (Recursive)
nb::object to_python(const json_value& v) {
    if (v.is_null()) {
        return nb::none();
    } else if (v.is_bool()) {
        return nb::bool_(v.as_bool());
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
        std::string s = fastjson_parallel::stringify(v);
        return nb::module_::import_("builtins").attr("int")(s.c_str());
    } else if (v.is_uint_128()) {
         std::string s = fastjson_parallel::stringify(v);
         return nb::module_::import_("builtins").attr("int")(s.c_str());
    } else if (v.is_number_128()) {
        // float128 -> python float (64-bit) 
        // Python doesn't support 128-bit float natively.
        return nb::float_((double)v.as_float128());
    }
    return nb::none();
}

namespace bindings {

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

} // namespace bindings

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
            return fastjson_parallel::stringify(self);
        })
        .def("__repr__", [](const json_value& self) {
            return std::format("<JSONValue: {}>", fastjson_parallel::stringify(self));
        })
        // Accessors
        .def("__getitem__", [](const json_value& self, std::string key) {
             if (!self.is_object()) throw std::runtime_error("Not an object");
             // json_object is unordered_map, has contains
             // json_value has contains? No, but operator[] on map throws if not found?
             // fastjson_parallel::json_value operator[] calls map::at
             // But we should check contains first to throw KeyError
             const auto& obj = self.as_object();
             if (obj.find(key) == obj.end()) throw nb::key_error(key.c_str());
             return obj.at(key);
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
        .def_prop_ro("is_bool", &json_value::is_bool)
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
        return bindings::with_gil_release([&] {
            // Using fastjson_parallel::parse
            auto result = fastjson_parallel::parse(json_str);
            if (!result) {
                throw std::runtime_error(result.error().message);
            }
            return *result;
        });
    }, "json_str"_a, "Parse JSON string (SIMD accelerated, 128-bit support)");

    m.def("parse_file", [](std::string filename) -> json_value {
        return bindings::with_gil_release([&] {
            std::ifstream f(filename, std::ios::binary | std::ios::ate);
            if (!f) throw std::runtime_error("Cannot open file: " + filename);
            
            auto size = f.tellg();
            std::string str(size, '\0');
            f.seekg(0);
            if (!f.read(str.data(), size)) throw std::runtime_error("Read error");
            
            auto result = fastjson_parallel::parse(str);
             if (!result) {
                throw std::runtime_error(result.error().message);
            }
            return *result;
        });
    }, "filename"_a, "Parse JSON from file path");

    // --- Parallel Parser API ---

    m.def("parse_parallel", [](std::string_view json_str) -> json_value {
        return bindings::with_gil_release([&] {
             // fastjson_parallel is parallel enabled by default config
             auto result = fastjson_parallel::parse(json_str);
            if (!result) throw std::runtime_error(result.error().message);
            return *result;
        });
    }, "json_str"_a, "Parse large JSON string using OpenMP");
    
    // --- LINQ API ---
    
    // json_linq is in fastjson::linq namespace (exported by json_linq module)
    using Query = fastjson::linq::query_result<json_value>; 

    nb::class_<Query>(m, "Query")
        .def("where", [](Query& self, std::function<bool(const json_value&)> pred) {
            return self.where([pred](const json_value& v) { return pred(v); });
        })
        .def("select", [](Query& self, std::function<json_value(const json_value&)> transform) {
            return self.select([transform](const json_value& v) { return transform(v); });
        })
        .def("to_list", [](Query& self) {
            // to_vector() returns std::vector<json_value>
            // to_python will handle json_value conversion if we mapped it, 
            // but we need to return list of JSONValue objects (wrappers)
            // Nanobind should handle std::vector<json_value> -> list[JSONValue] 
            // if we didn't make vector opaque?
            // Actually we only made vector<int64/double> opaque.
            // So vector<json_value> should be convertible to list.
            return self.to_vector();
        });

    m.def("query", [](const json_value& source) {
        if (!source.is_array()) {
            throw std::runtime_error("Query source must be an array");
        }
        return fastjson::linq::from(source.as_array());
    }, "Create a LINQ query from a JSONValue (must be array)");

    // --- Mustache Templating API ---
    m.def("render_template", [](std::string_view tmpl, const json_value& data) {
        return fastjson_parallel::mustache::render(tmpl, data);
    }, "template"_a, "data"_a, "Render a connection-less Mustache template using the JSON data model");

    // --- Template Instantiations (Vectors) ---
    bindings::bind_numeric_vector<int64_t>(m, "Int64Vector");
    bindings::bind_numeric_vector<double>(m, "DoubleVector");
}