#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/variant.h>

#include <iostream>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <expected>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <omp.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <cpuid.h>
#endif

namespace nb = nanobind;
using namespace nb::literals;

import std;
import fastjson;
import fastjson_parallel;
import json_linq;
import json_template;

using json_value = fastjson_parallel::json_value;

// ============================================================================
// SIMD Level Enumeration - Ordered by Performance (Highest to Lowest)
// ============================================================================
enum class SIMDLevel : int {
    AUTO = 0,       // Automatic detection - use best available
    AVX512 = 1,     // 512-bit, 8x registers (512 bytes/iter)
    AMX = 2,        // Advanced Matrix Extensions (for future use)
    AVX2 = 3,       // 256-bit, 4x registers (128 bytes/iter)
    AVX = 4,        // 256-bit legacy (64 bytes/iter)
    SSE4 = 5,       // 128-bit SSE4.2 (32 bytes/iter)
    SSE2 = 6,       // 128-bit SSE2 (16 bytes/iter)
    SCALAR = 7      // No SIMD (1 byte/iter)
};

// ============================================================================
// SIMD Capabilities Detection
// ============================================================================
struct SIMDCapabilities {
    bool sse2 = false;
    bool sse42 = false;
    bool avx = false;
    bool avx2 = false;
    bool avx512f = false;
    bool avx512bw = false;
    bool fma = false;
    bool vnni = false;
    bool amx = false;
    bool neon = false;  // ARM
    
    SIMDLevel best_level = SIMDLevel::SCALAR;
    std::string optimal_path = "Scalar";
    size_t bytes_per_iteration = 1;
    size_t register_count = 1;
};

inline SIMDCapabilities detect_simd_capabilities() {
    SIMDCapabilities caps;
    
#if defined(__x86_64__) || defined(_M_X64)
    unsigned int eax, ebx, ecx, edx;
    
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        caps.sse2 = (edx & bit_SSE2) != 0;
        caps.sse42 = (ecx & bit_SSE4_2) != 0;
        caps.avx = (ecx & bit_AVX) != 0;
        caps.fma = (ecx & bit_FMA) != 0;
        
        if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
            caps.avx2 = (ebx & bit_AVX2) != 0;
            caps.avx512f = (ebx & bit_AVX512F) != 0;
            caps.avx512bw = (ebx & bit_AVX512BW) != 0;
            caps.vnni = (ecx & 0x00000800) != 0;  // AVX512VNNI
            caps.amx = (edx & (1 << 24)) != 0;    // AMX-TILE
        }
    }
    
    // Determine best level (waterfall order)
    if (caps.avx512f && caps.avx512bw) {
        caps.best_level = SIMDLevel::AVX512;
        caps.optimal_path = "AVX-512 (8x registers)";
        caps.bytes_per_iteration = 512;
        caps.register_count = 8;
    } else if (caps.avx2) {
        caps.best_level = SIMDLevel::AVX2;
        caps.optimal_path = "AVX2 (4x registers)";
        caps.bytes_per_iteration = 128;
        caps.register_count = 4;
    } else if (caps.avx) {
        caps.best_level = SIMDLevel::AVX;
        caps.optimal_path = "AVX (2x registers)";
        caps.bytes_per_iteration = 64;
        caps.register_count = 2;
    } else if (caps.sse42) {
        caps.best_level = SIMDLevel::SSE4;
        caps.optimal_path = "SSE4.2";
        caps.bytes_per_iteration = 32;
        caps.register_count = 2;
    } else if (caps.sse2) {
        caps.best_level = SIMDLevel::SSE2;
        caps.optimal_path = "SSE2";
        caps.bytes_per_iteration = 16;
        caps.register_count = 1;
    }
    
#elif defined(__aarch64__) || defined(_M_ARM64)
    caps.neon = true;  // NEON is mandatory on AArch64
    caps.best_level = SIMDLevel::AVX2;  // NEON is roughly equivalent to AVX2 performance
    caps.optimal_path = "ARM NEON";
    caps.bytes_per_iteration = 128;
    caps.register_count = 4;
#endif
    
    return caps;
}

// Global cached capabilities
static const SIMDCapabilities g_simd_caps = detect_simd_capabilities();

// ============================================================================
// Waterfall SIMD Level Selection
// ============================================================================
inline SIMDLevel get_effective_simd_level(SIMDLevel requested) {
    if (requested == SIMDLevel::AUTO) {
        return g_simd_caps.best_level;
    }
    
    // Waterfall: if requested level isn't available, fall through to lower levels
    switch (requested) {
        case SIMDLevel::AVX512:
            if (g_simd_caps.avx512f && g_simd_caps.avx512bw) return SIMDLevel::AVX512;
            [[fallthrough]];
        case SIMDLevel::AMX:
            // AMX fallthrough to AVX2 (AMX is for matrix ops, not general parsing)
            [[fallthrough]];
        case SIMDLevel::AVX2:
            if (g_simd_caps.avx2) return SIMDLevel::AVX2;
            [[fallthrough]];
        case SIMDLevel::AVX:
            if (g_simd_caps.avx) return SIMDLevel::AVX;
            [[fallthrough]];
        case SIMDLevel::SSE4:
            if (g_simd_caps.sse42) return SIMDLevel::SSE4;
            [[fallthrough]];
        case SIMDLevel::SSE2:
            if (g_simd_caps.sse2) return SIMDLevel::SSE2;
            [[fallthrough]];
        case SIMDLevel::SCALAR:
        default:
            return SIMDLevel::SCALAR;
    }
}

inline std::string simd_level_to_string(SIMDLevel level) {
    switch (level) {
        case SIMDLevel::AUTO: return "AUTO";
        case SIMDLevel::AVX512: return "AVX-512";
        case SIMDLevel::AMX: return "AMX";
        case SIMDLevel::AVX2: return "AVX2";
        case SIMDLevel::AVX: return "AVX";
        case SIMDLevel::SSE4: return "SSE4.2";
        case SIMDLevel::SSE2: return "SSE2";
        case SIMDLevel::SCALAR: return "SCALAR";
    }
    return "UNKNOWN";
}

struct ConversionConfig {
    bool convert_128bit_to_str = false;
    int num_threads = 0;
    SIMDLevel simd_level = SIMDLevel::AUTO;
};

std::string int128_to_string(__int128 n) {
    if (n == 0) return "0";
    std::string s;
    bool neg = n < 0;
    unsigned __int128 un = neg ? (unsigned __int128)-(unsigned __int128)n : (unsigned __int128)n;
    while (un > 0) {
        s += (char)('0' + (un % 10));
        un /= 10;
    }
    if (neg) s += '-';
    std::reverse(s.begin(), s.end());
    return s;
}

std::string uint128_to_string(unsigned __int128 n) {
    if (n == 0) return "0";
    std::string s;
    while (n > 0) {
        s += (char)('0' + (n % 10));
        n /= 10;
    }
    std::reverse(s.begin(), s.end());
    return s;
}

// Optimized conversion from C++ json_value to Python object
template<bool IsParallel = false>
inline nb::object to_python_optimized(const json_value& v, const ConversionConfig& config) {
    if (v.is_null()) return nb::none();
    if (v.is_bool()) return nb::cast(v.as_bool());
    if (v.is_number()) return nb::cast(v.as_number());
    if (v.is_string()) return nb::cast(v.as_string());
    
    if (v.is_int_128()) {
        std::string s = int128_to_string(v.as_int_128());
        return nb::module_::import_("builtins").attr("int")(s);
    }
    if (v.is_uint_128()) {
        std::string s = uint128_to_string(v.as_uint_128());
        return nb::module_::import_("builtins").attr("int")(s);
    }
    
    if (v.is_number_128()) {
        return nb::cast((double)v.as_number_128());
    }

    if (v.is_array()) {
        const auto& arr = v.as_array();
        size_t n = arr.size();
        PyObject* py_list = PyList_New(n);
        for (size_t i = 0; i < n; ++i) {
            nb::object item = to_python_optimized<false>(arr[i], config);
            PyList_SET_ITEM(py_list, i, item.release().ptr());
        }
        return nb::steal<nb::object>(py_list);
    }

    if (v.is_object()) {
        const auto& obj = v.as_object();
        nb::dict py_dict;
        for (const auto& [key, val] : obj) {
            py_dict[nb::cast(key)] = to_python_optimized<false>(val, config);
        }
        return py_dict;
    }

    return nb::none();
}

// Parallel entry point for conversion with SIMD level selection
nb::object to_python_parallel(const json_value& v, int threads, SIMDLevel simd_level) {
    ConversionConfig config;
    config.num_threads = threads;
    config.simd_level = get_effective_simd_level(simd_level);
    
    if (threads <= 1 || (!v.is_array() && !v.is_object())) {
        return to_python_optimized<false>(v, config);
    }

    if (v.is_array()) {
        const auto& arr = v.as_array();
        size_t n = arr.size();
        std::vector<nb::object> items(n);
        
        // Release GIL before entering parallel region to avoid deadlock
        {
            nb::gil_scoped_release release;
            #pragma omp parallel for num_threads(threads)
            for (size_t i = 0; i < n; ++i) {
                nb::gil_scoped_acquire acquire;
                items[i] = to_python_optimized<false>(arr[i], config);
            }
        }
        
        nb::list py_list;
        for (auto& item : items) py_list.append(std::move(item));
        return std::move(py_list);
    } else {
        const auto& obj = v.as_object();
        std::vector<std::string> keys;
        keys.reserve(obj.size());
        for (auto const& [k, _] : obj) keys.push_back(k);
        
        size_t n = keys.size();
        std::vector<nb::object> values(n);
        
        // Release GIL before entering parallel region to avoid deadlock
        {
            nb::gil_scoped_release release;
            #pragma omp parallel for num_threads(threads)
            for (size_t i = 0; i < n; ++i) {
                nb::gil_scoped_acquire acquire;
                values[i] = to_python_optimized<false>(obj.at(keys[i]), config);
            }
        }
        
        nb::dict py_dict;
        for (size_t i = 0; i < n; ++i) {
            py_dict[nb::cast(keys[i])] = std::move(values[i]);
        }
        return std::move(py_dict);
    }
}

NB_MODULE(fastjson, m) {
    m.doc() = "FastestJSONInTheWest Python Bindings (SIMD, COW, Parallel)";

    nb::class_<json_value>(m, "JSONValue")
        .def(nb::init<>())
        .def("is_null", &json_value::is_null)
        .def("is_bool", &json_value::is_bool)
        .def("is_number", &json_value::is_number)
        .def("is_string", &json_value::is_string)
        .def("is_array", &json_value::is_array)
        .def("is_object", &json_value::is_object)
        .def("as_bool", &json_value::as_bool)
        .def("as_number", &json_value::as_number)
        .def("as_int", [](const json_value& v) {
            if (v.is_number()) return (int64_t)v.as_number();
            if (v.is_int_128()) return (int64_t)v.as_int_128();
            if (v.is_uint_128()) return (int64_t)v.as_uint_128();
            throw nb::type_error("Not a number or 128-bit integer");
        })
        .def("as_string", &json_value::as_string)
        .def("__getitem__", [](const json_value& v, const std::string& key) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            const auto& obj = v.as_object();
            if (obj.find(key) == obj.end()) throw nb::key_error(key.c_str());
            return obj.at(key);
        })
        .def("__getitem__", [](const json_value& v, size_t index) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            return v[index];
        })
        .def("__setitem__", [](json_value& v, const std::string& key, const json_value& val) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            v.as_object()[key] = val; // Triggers COW
        })
        .def("__setitem__", [](json_value& v, size_t index, const json_value& val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = val; // Triggers COW
        })
        // Overloads for Python native types
        .def("__setitem__", [](json_value& v, size_t index, double val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, size_t index, int64_t val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, size_t index, const std::string& val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, size_t index, bool val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, const std::string& key, double val) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            v.as_object()[key] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, const std::string& key, int64_t val) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            v.as_object()[key] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, const std::string& key, const std::string& val) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            v.as_object()[key] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, const std::string& key, bool val) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            v.as_object()[key] = json_value(val);
        })
        .def("append", [](json_value& v, const json_value& val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            v.as_array().push_back(val); // Triggers COW
        })
        .def("copy", [](const json_value& v) {
            return json_value(v); // Uses bitwise copy of shared_ptr = O(1)
        })
        .def("clear", [](json_value& v) {
            v.clear();
        })
        .def("__len__", [](const json_value& v) {
            return v.size();
        })
        .def("__str__", [](const json_value& v) {
            return fastjson_parallel::stringify(v);
        })
        .def("__repr__", [](const json_value& v) {
            return "<JSONValue: " + fastjson_parallel::stringify(v) + ">";
        })
        .def("to_python", [](const json_value& v, int threads, int simd_level) {
            SIMDLevel level = static_cast<SIMDLevel>(simd_level);
            if (threads > 1) return to_python_parallel(v, threads, level);
            ConversionConfig config;
            config.simd_level = get_effective_simd_level(level);
            return to_python_optimized<false>(v, config);
        }, "threads"_a = 0, "simd_level"_a = 0,
           "Convert to native Python objects. simd_level: 0=AUTO, 1=AVX512, 2=AMX, 3=AVX2, 4=AVX, 5=SSE4, 6=SSE2, 7=SCALAR")
        .def("to_python_parallel", [](const json_value& v, int threads, int simd_level) {
            SIMDLevel level = static_cast<SIMDLevel>(simd_level);
            return to_python_parallel(v, threads, level);
        }, "threads"_a = 0, "simd_level"_a = 0,
           "Convert to native Python objects (parallel) with SIMD level selection");

    m.def("set_num_threads", [](int threads) {
        fastjson_parallel::set_num_threads(threads);
    }, "Set the number of threads for parallel operations");

    m.def("get_num_threads", []() {
        return fastjson_parallel::get_num_threads();
    }, "Get the number of threads for parallel operations");

    m.def("parse", [](const std::string& s) {
        auto result = fastjson_parallel::parse(s);
        if (!result) throw std::runtime_error("Parse error");
        return result.value();
    }, "Parse JSON from a string (GIL-free)", nb::call_guard<nb::gil_scoped_release>());

    m.def("parse_file", [](const std::string& filename) {
        std::ifstream file(filename);
        if (!file) throw std::runtime_error("Could not open file: " + filename);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        auto result = fastjson_parallel::parse(content);
        if (!result) throw std::runtime_error("Parse error");
        return result.value();
    }, "Parse JSON from a file (GIL-free)", nb::call_guard<nb::gil_scoped_release>());

    // LINQ Bindings
    using Query = fastjson::linq::query_result<json_value>;
    nb::class_<Query>(m, "JSONQuery")
        .def("where", [](Query& self, nb::callable predicate) {
            return self.where([predicate](const json_value& v) {
                nb::gil_scoped_acquire acquire;
                return nb::cast<bool>(predicate(v));
            });
        })
        .def("select", [](Query& self, nb::callable transform) {
            return self.select([transform](const json_value& v) -> json_value {
                nb::gil_scoped_acquire acquire;
                return nb::cast<json_value>(transform(v));
            });
        })
        .def("to_list", [](Query& self) {
            nb::list result;
            ConversionConfig config;
            for (const auto& v : self) {
                result.append(to_python_optimized<false>(v, config));
            }
            return result;
        });

    m.def("query", [](const json_value& v) {
        if (!v.is_array()) throw nb::type_error("query() requires a JSON array");
        std::vector<json_value> data = v.as_array(); // Copy it
        return Query(std::move(data));
    });

    m.def("render_template", [](const std::string& tmpl, const json_value& data) {
        return fastjson_parallel::mustache::render(tmpl, data);
    }, "Render a Mustache template with JSON data");

    // ========================================================================
    // SIMD Capabilities and Level Selection API
    // ========================================================================
    
    // SIMD Level enum exposed to Python
    nb::enum_<SIMDLevel>(m, "SIMDLevel")
        .value("AUTO", SIMDLevel::AUTO, "Automatic detection - use best available")
        .value("AVX512", SIMDLevel::AVX512, "512-bit AVX-512 (8x registers, 512 bytes/iter)")
        .value("AMX", SIMDLevel::AMX, "Advanced Matrix Extensions")
        .value("AVX2", SIMDLevel::AVX2, "256-bit AVX2 (4x registers, 128 bytes/iter)")
        .value("AVX", SIMDLevel::AVX, "256-bit AVX legacy")
        .value("SSE4", SIMDLevel::SSE4, "128-bit SSE4.2")
        .value("SSE2", SIMDLevel::SSE2, "128-bit SSE2")
        .value("SCALAR", SIMDLevel::SCALAR, "Scalar fallback (no SIMD)")
        .export_values();
    
    // SIMD Capabilities struct exposed to Python
    nb::class_<SIMDCapabilities>(m, "SIMDCapabilities")
        .def_ro("sse2", &SIMDCapabilities::sse2)
        .def_ro("sse42", &SIMDCapabilities::sse42)
        .def_ro("avx", &SIMDCapabilities::avx)
        .def_ro("avx2", &SIMDCapabilities::avx2)
        .def_ro("avx512f", &SIMDCapabilities::avx512f)
        .def_ro("avx512bw", &SIMDCapabilities::avx512bw)
        .def_ro("fma", &SIMDCapabilities::fma)
        .def_ro("vnni", &SIMDCapabilities::vnni)
        .def_ro("amx", &SIMDCapabilities::amx)
        .def_ro("neon", &SIMDCapabilities::neon)
        .def_ro("best_level", &SIMDCapabilities::best_level)
        .def_ro("optimal_path", &SIMDCapabilities::optimal_path)
        .def_ro("bytes_per_iteration", &SIMDCapabilities::bytes_per_iteration)
        .def_ro("register_count", &SIMDCapabilities::register_count)
        .def("__repr__", [](const SIMDCapabilities& caps) {
            std::ostringstream oss;
            oss << "<SIMDCapabilities: " << caps.optimal_path 
                << ", " << caps.bytes_per_iteration << " bytes/iter"
                << ", " << caps.register_count << " registers"
                << ", avx512=" << (caps.avx512f ? "yes" : "no")
                << ", avx2=" << (caps.avx2 ? "yes" : "no")
                << ", sse42=" << (caps.sse42 ? "yes" : "no")
                << ">";
            return oss.str();
        });
    
    // Get current SIMD capabilities
    m.def("get_simd_capabilities", []() {
        return g_simd_caps;
    }, "Get the SIMD capabilities detected on this CPU");
    
    // Get effective SIMD level after waterfall
    m.def("get_effective_simd_level", [](int requested_level) {
        SIMDLevel level = static_cast<SIMDLevel>(requested_level);
        return get_effective_simd_level(level);
    }, "requested_level"_a = 0,
       "Get the effective SIMD level that will be used (after waterfall fallback)");
    
    // Get SIMD level name
    m.def("simd_level_name", [](int level) {
        return simd_level_to_string(static_cast<SIMDLevel>(level));
    }, "level"_a, "Get the name of a SIMD level");
    
    // Check if specific SIMD level is available
    m.def("is_simd_level_available", [](int level) {
        SIMDLevel requested = static_cast<SIMDLevel>(level);
        switch (requested) {
            case SIMDLevel::AVX512: return g_simd_caps.avx512f && g_simd_caps.avx512bw;
            case SIMDLevel::AMX: return g_simd_caps.amx;
            case SIMDLevel::AVX2: return g_simd_caps.avx2;
            case SIMDLevel::AVX: return g_simd_caps.avx;
            case SIMDLevel::SSE4: return g_simd_caps.sse42;
            case SIMDLevel::SSE2: return g_simd_caps.sse2;
            case SIMDLevel::SCALAR: return true;
            case SIMDLevel::AUTO: return true;
            default: return false;
        }
    }, "level"_a, "Check if a specific SIMD level is available on this CPU");
    
    // SIMD waterfall order documentation
    m.attr("SIMD_WATERFALL_ORDER") = nb::cast(std::vector<std::string>{
        "AVX-512 (512 bytes/iter, 8 registers)",
        "AMX (Advanced Matrix Extensions)",
        "AVX2 (128 bytes/iter, 4 registers)",
        "AVX (64 bytes/iter, 2 registers)", 
        "SSE4.2 (32 bytes/iter)",
        "SSE2 (16 bytes/iter)",
        "SCALAR (1 byte/iter)"
    });
}
