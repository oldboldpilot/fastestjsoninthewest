/**
 * Nanobind Python Bindings for FastJSON
 *
 * Drop-in replacement for Python stdlib json module:
 *   import fastjson_py as json
 *   data = json.loads('{"key": "value"}')
 *   text = json.dumps(data)
 *
 * Uses std::expected / std::unexpected for railway-oriented error handling.
 *
 * Author: Olumuyiwa Oluwasanmi
 * Date: 2026-02-22
 */

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>

#include <expected>
#include <fstream>
#include <sstream>
#include <string>

import fastjson;

namespace nb = nanobind;
using namespace nb::literals;

// ============================================================================
// json_value -> Python object conversion
// ============================================================================

static auto to_python(const fastjson::json_value& val) -> nb::object {
    if (val.is_null()) {
        return nb::none();
    }
    if (val.is_boolean()) {
        return nb::bool_(val.as_boolean());
    }
    if (val.is_int_128() || val.is_uint_128()) {
        try {
            return nb::int_(val.as_int64());
        } catch (...) {
            try {
                return nb::int_(val.as_uint64());
            } catch (...) {
                return nb::float_(val.as_number());
            }
        }
    }
    if (val.is_number()) {
        double d = val.as_number();
        if (d == static_cast<double>(static_cast<int64_t>(d)) &&
            d >= -9007199254740992.0 && d <= 9007199254740992.0) {
            return nb::int_(static_cast<int64_t>(d));
        }
        return nb::float_(d);
    }
    if (val.is_string()) {
        return nb::cast(val.as_std_string());
    }
    if (val.is_array()) {
        const auto& arr = val.as_array();
        nb::list py_list;
        for (const auto& item : arr) {
            py_list.append(to_python(item));
        }
        return py_list;
    }
    if (val.is_object()) {
        const auto& obj = val.as_object();
        nb::dict py_dict;
        for (const auto& [key, value] : obj) {
            py_dict[nb::cast(key)] = to_python(value);
        }
        return py_dict;
    }
    return nb::none();
}

// ============================================================================
// Python object -> json_value conversion (std::expected)
// ============================================================================

static auto from_python(nb::handle obj) -> std::expected<fastjson::json_value, std::string> {
    if (obj.is_none()) {
        return fastjson::json_value{nullptr};
    }
    if (nb::isinstance<nb::bool_>(obj)) {
        return fastjson::json_value{nb::cast<bool>(obj)};
    }
    if (nb::isinstance<nb::int_>(obj)) {
        return fastjson::json_value{nb::cast<int64_t>(obj)};
    }
    if (nb::isinstance<nb::float_>(obj)) {
        return fastjson::json_value{nb::cast<double>(obj)};
    }
    if (nb::isinstance<nb::str>(obj)) {
        return fastjson::json_value{nb::cast<std::string>(obj)};
    }
    if (nb::isinstance<nb::list>(obj) || nb::isinstance<nb::tuple>(obj)) {
        fastjson::json_array arr;
        for (auto item : obj) {
            auto result = from_python(item);
            if (!result.has_value()) {
                return std::unexpected(result.error());
            }
            arr.push_back(std::move(result.value()));
        }
        return fastjson::json_value{std::move(arr)};
    }
    if (nb::isinstance<nb::dict>(obj)) {
        fastjson::json_object map;
        nb::dict d = nb::borrow<nb::dict>(obj);
        for (auto [key, value] : d) {
            std::string k = nb::cast<std::string>(nb::str(key));
            auto result = from_python(value);
            if (!result.has_value()) {
                return std::unexpected(result.error());
            }
            map[k] = std::move(result.value());
        }
        return fastjson::json_value{std::move(map)};
    }
    return std::unexpected(std::string("Unsupported type for JSON serialization: ") +
                           nb::cast<std::string>(nb::str(obj.type())));
}

// ============================================================================
// File I/O helpers (std::expected)
// ============================================================================

static auto read_file(const std::string& path) -> std::expected<std::string, std::string> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::unexpected("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static auto write_file(const std::string& path, const std::string& content) -> std::expected<void, std::string> {
    std::ofstream file(path);
    if (!file.is_open()) {
        return std::unexpected("Cannot open file for writing: " + path);
    }
    file << content;
    return {};
}

// ============================================================================
// Module Definition
// ============================================================================

NB_MODULE(fastjson_py, m) {
    m.doc() = "FastJSON - High-performance JSON parser with SIMD acceleration";

    // loads(s: str) -> object
    m.def("loads", [](std::string_view input) -> nb::object {
        auto result = fastjson::parse(input);
        if (!result.has_value()) {
            throw nb::value_error(result.error().to_string().c_str());
        }
        return to_python(result.value());
    }, "input"_a,
       "Parse a JSON string and return a Python object.\n\n"
       "Args:\n    input: JSON string to parse\n\n"
       "Returns:\n    Parsed Python object (dict, list, str, int, float, bool, or None)\n\n"
       "Raises:\n    ValueError: If the input is not valid JSON");

    // dumps(obj, indent=-1) -> str  (-1 means compact, no indent)
    m.def("dumps", [](nb::object obj, int indent) -> std::string {
        if (obj.is_none()) {
            return "null";
        }
        auto result = from_python(obj);
        if (!result.has_value()) {
            throw nb::type_error(result.error().c_str());
        }
        if (indent >= 0) {
            return result.value().to_pretty_string(indent);
        }
        return result.value().to_string();
    }, "obj"_a.none(), "indent"_a = -1,
       "Serialize a Python object to a JSON string.\n\n"
       "Args:\n    obj: Python object to serialize\n    indent: Indentation level for pretty-printing (default: compact)\n\n"
       "Returns:\n    JSON string");

    // load(path: str) -> object
    m.def("load", [](const std::string& path) -> nb::object {
        auto content = read_file(path);
        if (!content.has_value()) {
            throw nb::value_error(content.error().c_str());
        }
        auto result = fastjson::parse(content.value());
        if (!result.has_value()) {
            throw nb::value_error(result.error().to_string().c_str());
        }
        return to_python(result.value());
    }, "path"_a,
       "Read and parse a JSON file.\n\n"
       "Args:\n    path: Path to the JSON file\n\n"
       "Returns:\n    Parsed Python object");

    // dump(obj, path: str, indent=-1)
    m.def("dump", [](nb::object obj, const std::string& path, int indent) {
        if (obj.is_none()) {
            auto wr = write_file(path, "null");
            if (!wr.has_value()) {
                throw nb::value_error(wr.error().c_str());
            }
            return;
        }
        auto conv = from_python(obj);
        if (!conv.has_value()) {
            throw nb::type_error(conv.error().c_str());
        }
        std::string json_str = (indent >= 0) ?
            conv.value().to_pretty_string(indent) :
            conv.value().to_string();

        auto write_result = write_file(path, json_str);
        if (!write_result.has_value()) {
            throw nb::value_error(write_result.error().c_str());
        }
    }, "obj"_a.none(), "path"_a, "indent"_a = -1,
       "Serialize a Python object and write to a JSON file.\n\n"
       "Args:\n    obj: Python object to serialize\n    path: Output file path\n    indent: Indentation level for pretty-printing (default: compact)");

    // prettify(s: str, indent=4) -> str
    m.def("prettify", [](std::string_view input, int indent) -> std::string {
        auto result = fastjson::parse(input);
        if (!result.has_value()) {
            throw nb::value_error(result.error().to_string().c_str());
        }
        return result.value().to_pretty_string(indent);
    }, "input"_a, "indent"_a = 4,
       "Parse JSON and return a pretty-printed string.\n\n"
       "Args:\n    input: JSON string\n    indent: Indentation level (default: 4)\n\n"
       "Returns:\n    Pretty-printed JSON string");

    // simd_info() -> str — reports runtime SIMD capabilities via CPUID
    m.def("simd_info", []() -> std::string {
        uint32_t caps = fastjson::detect_simd_capabilities();
        std::string result = "FastJSON C++23 module (SIMD GMF: ";
        if (caps == 0) {
            result += "scalar-only";
        } else {
            bool first = true;
            auto append = [&](uint32_t flag, const char* name) {
                if (caps & flag) {
                    if (!first) result += " + ";
                    result += name;
                    first = false;
                }
            };
            append(fastjson::SIMD_AVX512BW, "AVX-512BW");
            append(fastjson::SIMD_AVX512F,  "AVX-512F");
            append(fastjson::SIMD_AVX2,     "AVX2");
            append(fastjson::SIMD_SSE42,    "SSE4.2");
            append(fastjson::SIMD_SSE2,     "SSE2");
        }
        result += ")";
        return result;
    }, "Return information about the SIMD capabilities in use.");
}
