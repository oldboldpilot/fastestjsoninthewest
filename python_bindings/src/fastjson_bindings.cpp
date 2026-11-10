#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>
#include <nanobind/ndarray.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

// Opaque C++ vector types exposed as zero-copy Python sequence classes
NB_MAKE_OPAQUE(std::vector<int64_t>);
NB_MAKE_OPAQUE(std::vector<uint64_t>);
NB_MAKE_OPAQUE(std::vector<double>);
NB_MAKE_OPAQUE(std::vector<std::string>);

#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/vector.h>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

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
#if defined(FASTJSON_PYTHON_HAS_TURBO)
import fastjson_turbo;
#endif

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
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;

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
            caps.amx = (edx & (1u << 24)) != 0;   // AMX-TILE
        }
    }

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
    caps.neon = true;
    caps.best_level = SIMDLevel::AVX2;
    caps.optimal_path = "ARM NEON";
    caps.bytes_per_iteration = 128;
    caps.register_count = 4;
#endif

    return caps;
}

static const SIMDCapabilities g_simd_caps = detect_simd_capabilities();

// ============================================================================
// Waterfall SIMD Level Selection
// ============================================================================
inline SIMDLevel get_effective_simd_level(SIMDLevel requested) {
    if (requested == SIMDLevel::AUTO) {
        return g_simd_caps.best_level;
    }

    switch (requested) {
        case SIMDLevel::AVX512:
            if (g_simd_caps.avx512f && g_simd_caps.avx512bw) return SIMDLevel::AVX512;
            [[fallthrough]];
        case SIMDLevel::AMX:
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

// ============================================================================
// Safe 128-bit Integer Formatting (UB-free for INT128_MIN)
// ============================================================================
inline std::string uint128_to_string(fastjson_parallel::json_uint_128 n) {
    if (n == 0) return "0";
    std::string s;
    s.reserve(40);
    fastjson_parallel::json_uint_128 un = n;
    while (un > 0) {
        s.push_back(static_cast<char>('0' + static_cast<int>(un % 10)));
        un /= 10;
    }
    std::reverse(s.begin(), s.end());
    return s;
}

inline std::string int128_to_string(fastjson_parallel::json_int_128 n) {
    if (n == 0) return "0";
    const bool neg = n < 0;
    const auto un_raw = static_cast<fastjson_parallel::json_uint_128>(n);
    fastjson_parallel::json_uint_128 un =
        neg ? (fastjson_parallel::json_uint_128{0} - un_raw) : un_raw;
    std::string s;
    s.reserve(41);
    while (un > 0) {
        s.push_back(static_cast<char>('0' + static_cast<int>(un % 10)));
        un /= 10;
    }
    if (neg) s.push_back('-');
    std::reverse(s.begin(), s.end());
    return s;
}

inline PyObject* py_long_from_int128(fastjson_parallel::json_int_128 n) {
    std::string s = int128_to_string(n);
    return PyLong_FromString(s.c_str(), nullptr, 10);
}

inline PyObject* py_long_from_uint128(fastjson_parallel::json_uint_128 n) {
    std::string s = uint128_to_string(n);
    return PyLong_FromString(s.c_str(), nullptr, 10);
}

// ============================================================================
// Optimized Conversion from C++ json_value to Python Object
// ============================================================================
template <bool IsParallel = false>
inline nb::object to_python_optimized(const json_value& v, const ConversionConfig& config) {
    if (v.is_null()) return nb::none();
    if (v.is_bool()) return nb::cast(v.as_bool());
    if (v.is_number()) return nb::cast(v.as_number());
    if (v.is_string()) return nb::cast(v.as_string());

    if (v.is_int_128()) {
        if (config.convert_128bit_to_str) {
            return nb::cast(int128_to_string(v.as_int_128()));
        }
        PyObject* obj = py_long_from_int128(v.as_int_128());
        if (!obj) throw nb::python_error();
        return nb::steal<nb::object>(obj);
    }
    if (v.is_uint_128()) {
        if (config.convert_128bit_to_str) {
            return nb::cast(uint128_to_string(v.as_uint_128()));
        }
        PyObject* obj = py_long_from_uint128(v.as_uint_128());
        if (!obj) throw nb::python_error();
        return nb::steal<nb::object>(obj);
    }

    if (v.is_number_128()) {
        return nb::cast(static_cast<double>(v.as_number_128()));
    }

    if (v.is_array()) {
        const auto& arr = v.as_array();
        const size_t n = arr.size();
        PyObject* py_list = PyList_New(static_cast<Py_ssize_t>(n));
        if (!py_list) throw nb::python_error();
        for (size_t i = 0; i < n; ++i) {
            try {
                nb::object item = to_python_optimized<false>(arr[i], config);
                PyList_SET_ITEM(py_list, static_cast<Py_ssize_t>(i), item.release().ptr());
            } catch (...) {
                Py_DECREF(py_list);
                throw;
            }
        }
        return nb::steal<nb::object>(py_list);
    }

    if (v.is_object()) {
        const auto& obj = v.as_object();
        nb::dict py_dict;
        for (const auto& [key, val] : obj) {
            py_dict[nb::str(key.data(), key.size())] = to_python_optimized<false>(val, config);
        }
        return py_dict;
    }

    return nb::none();
}

nb::object to_python_parallel(const json_value& v, int threads, SIMDLevel simd_level) {
    ConversionConfig config;
    config.num_threads = threads;
    config.simd_level = get_effective_simd_level(simd_level);

    if (threads <= 1 || (!v.is_array() && !v.is_object())) {
        return to_python_optimized<false>(v, config);
    }

    if (v.is_array()) {
        const auto& arr = v.as_array();
        const size_t n = arr.size();
        std::vector<nb::object> items(n);

        {
            nb::gil_scoped_release release;
            tbb::parallel_for(tbb::blocked_range<size_t>(0, n), [&](const tbb::blocked_range<size_t>& r) {
                for (size_t i = r.begin(); i < r.end(); ++i) {
                    nb::gil_scoped_acquire acquire;
                    items[i] = to_python_optimized<false>(arr[i], config);
                }
            });
        }

        PyObject* py_list = PyList_New(static_cast<Py_ssize_t>(n));
        if (!py_list) throw nb::python_error();
        for (size_t i = 0; i < n; ++i) {
            PyList_SET_ITEM(py_list, static_cast<Py_ssize_t>(i), items[i].release().ptr());
        }
        return nb::steal<nb::object>(py_list);
    } else {
        const auto& obj = v.as_object();
        std::vector<std::string> keys;
        keys.reserve(obj.size());
        for (const auto& [k, _] : obj) keys.push_back(k);

        const size_t n = keys.size();
        std::vector<nb::object> values(n);

        {
            nb::gil_scoped_release release;
            tbb::parallel_for(tbb::blocked_range<size_t>(0, n), [&](const tbb::blocked_range<size_t>& r) {
                for (size_t i = r.begin(); i < r.end(); ++i) {
                    nb::gil_scoped_acquire acquire;
                    values[i] = to_python_optimized<false>(obj.at(keys[i]), config);
                }
            });
        }

        nb::dict py_dict;
        for (size_t i = 0; i < n; ++i) {
            py_dict[nb::str(keys[i].data(), keys[i].size())] = std::move(values[i]);
        }
        return std::move(py_dict);
    }
}

// ============================================================================
// Convert Native Python Object -> C++ json_value
// ============================================================================
inline json_value py_to_json_value(nb::handle obj, int depth = 0) {
    if (depth > 1000) {
        throw nb::value_error("Maximum recursion depth exceeded converting Python object to JSONValue");
    }
    PyObject* p = obj.ptr();
    if (p == Py_None) {
        return json_value(nullptr);
    }
    if (PyBool_Check(p)) {
        return json_value(p == Py_True);
    }
    if (nb::isinstance<json_value>(obj)) {
        return nb::cast<json_value>(obj);
    }
    if (PyLong_Check(p)) {
        int overflow = 0;
        long long v = PyLong_AsLongLongAndOverflow(p, &overflow);
        if (!overflow && !PyErr_Occurred()) {
            return json_value(static_cast<double>(v));
        }
        PyErr_Clear();
        unsigned long long uv = PyLong_AsUnsignedLongLong(p);
        if (!PyErr_Occurred()) {
            return json_value(static_cast<fastjson_parallel::json_uint_128>(uv));
        }
        PyErr_Clear();
        PyObject* s_obj = PyObject_Str(p);
        if (!s_obj) throw nb::python_error();
        Py_ssize_t slen = 0;
        const char* s_ptr = PyUnicode_AsUTF8AndSize(s_obj, &slen);
        if (!s_ptr) {
            Py_DECREF(s_obj);
            throw nb::python_error();
        }
        std::string num_str(s_ptr, static_cast<size_t>(slen));
        Py_DECREF(s_obj);
        auto parsed = fastjson_parallel::parse(num_str);
        if (!parsed) throw nb::value_error("Integer out of supported JSON range");
        return std::move(*parsed);
    }
    if (PyFloat_Check(p)) {
        return json_value(PyFloat_AS_DOUBLE(p));
    }
    if (PyUnicode_Check(p)) {
        Py_ssize_t slen = 0;
        const char* s_ptr = PyUnicode_AsUTF8AndSize(p, &slen);
        if (!s_ptr) throw nb::python_error();
        return json_value(std::string(s_ptr, static_cast<size_t>(slen)));
    }
    if (PyList_Check(p)) {
        const Py_ssize_t n = PyList_GET_SIZE(p);
        fastjson_parallel::json_array arr;
        arr.reserve(static_cast<size_t>(n));
        for (Py_ssize_t i = 0; i < n; ++i) {
            arr.push_back(py_to_json_value(PyList_GET_ITEM(p, i), depth + 1));
        }
        return json_value(std::move(arr));
    }
    if (PyTuple_Check(p)) {
        const Py_ssize_t n = PyTuple_GET_SIZE(p);
        fastjson_parallel::json_array arr;
        arr.reserve(static_cast<size_t>(n));
        for (Py_ssize_t i = 0; i < n; ++i) {
            arr.push_back(py_to_json_value(PyTuple_GET_ITEM(p, i), depth + 1));
        }
        return json_value(std::move(arr));
    }
    if (PyDict_Check(p)) {
        fastjson_parallel::json_object map;
        PyObject* k = nullptr;
        PyObject* v = nullptr;
        Py_ssize_t pos = 0;
        while (PyDict_Next(p, &pos, &k, &v)) {
            if (!PyUnicode_Check(k)) {
                throw nb::type_error("JSON object keys must be strings");
            }
            Py_ssize_t klen = 0;
            const char* kptr = PyUnicode_AsUTF8AndSize(k, &klen);
            if (!kptr) throw nb::python_error();
            map.emplace(std::string(kptr, static_cast<size_t>(klen)), py_to_json_value(v, depth + 1));
        }
        return json_value(std::move(map));
    }
    if (nb::isinstance<std::vector<int64_t>>(obj)) {
        const auto& vec = nb::cast<const std::vector<int64_t>&>(obj);
        fastjson_parallel::json_array arr;
        arr.reserve(vec.size());
        for (int64_t x : vec) arr.emplace_back(static_cast<double>(x));
        return json_value(std::move(arr));
    }
    if (nb::isinstance<std::vector<uint64_t>>(obj)) {
        const auto& vec = nb::cast<const std::vector<uint64_t>&>(obj);
        fastjson_parallel::json_array arr;
        arr.reserve(vec.size());
        for (uint64_t x : vec) arr.emplace_back(static_cast<fastjson_parallel::json_uint_128>(x));
        return json_value(std::move(arr));
    }
    if (nb::isinstance<std::vector<double>>(obj)) {
        const auto& vec = nb::cast<const std::vector<double>&>(obj);
        fastjson_parallel::json_array arr;
        arr.reserve(vec.size());
        for (double x : vec) arr.emplace_back(x);
        return json_value(std::move(arr));
    }
    if (nb::isinstance<std::vector<std::string>>(obj)) {
        const auto& vec = nb::cast<const std::vector<std::string>&>(obj);
        fastjson_parallel::json_array arr;
        arr.reserve(vec.size());
        for (const auto& x : vec) arr.emplace_back(x);
        return json_value(std::move(arr));
    }
    throw nb::type_error("Object of unsupported type is not JSON serializable");
}

// ============================================================================
// Buffer Extraction Helper (str | bytes | bytearray | buffer)
// ============================================================================
struct ExtractedInput {
    std::string owned;
    std::string_view view;
};

inline ExtractedInput extract_input_bytes(nb::handle h, bool allow_str = true) {
    PyObject* p = h.ptr();
    if (allow_str && PyUnicode_Check(p)) {
        Py_ssize_t size = 0;
        const char* data = PyUnicode_AsUTF8AndSize(p, &size);
        if (!data) throw nb::python_error();
        return ExtractedInput{{}, std::string_view(data, static_cast<size_t>(size))};
    }
    if (PyBytes_Check(p)) {
        const char* data = PyBytes_AS_STRING(p);
        const Py_ssize_t size = PyBytes_GET_SIZE(p);
        return ExtractedInput{{}, std::string_view(data, static_cast<size_t>(size))};
    }
    if (PyByteArray_Check(p)) {
        const char* data = PyByteArray_AS_STRING(p);
        const Py_ssize_t size = PyByteArray_GET_SIZE(p);
        ExtractedInput out;
        out.owned.assign(data, static_cast<size_t>(size));
        out.view = out.owned;
        return out;
    }
    if (PyObject_CheckBuffer(p)) {
        Py_buffer view{};
        if (PyObject_GetBuffer(p, &view, PyBUF_SIMPLE) != 0) {
            throw nb::python_error();
        }
        ExtractedInput out;
        if (view.buf && view.len > 0) {
            out.owned.assign(static_cast<const char*>(view.buf), static_cast<size_t>(view.len));
        }
        PyBuffer_Release(&view);
        out.view = out.owned;
        return out;
    }
    if (allow_str) {
        throw nb::type_error("Expected str, bytes, bytearray, or buffer object");
    }
    throw nb::type_error("Expected bytes, bytearray, or buffer object (PostgreSQL binary jsonb wire data)");
}

// ============================================================================
// TurboDOM -> Native Python Object Conversion (Direct CPython C API)
// ============================================================================
#if defined(FASTJSON_PYTHON_HAS_TURBO)
inline PyObject* dom_element_to_pyobject(const fastjson::turbo::dom_element& el) {
    using fastjson::turbo::dom_type;
    switch (el.type()) {
        case dom_type::null_value:
            Py_INCREF(Py_None);
            return Py_None;
        case dom_type::boolean: {
            const bool b = *el.get_bool();
            PyObject* res = b ? Py_True : Py_False;
            Py_INCREF(res);
            return res;
        }
        case dom_type::int64:
            return PyLong_FromLongLong(static_cast<long long>(*el.get_int64()));
        case dom_type::uint64:
            return PyLong_FromUnsignedLongLong(static_cast<unsigned long long>(*el.get_uint64()));
        case dom_type::int128: {
            std::string s = int128_to_string(*el.get_int128());
            return PyLong_FromString(s.c_str(), nullptr, 10);
        }
        case dom_type::uint128: {
            std::string s = uint128_to_string(*el.get_uint128());
            return PyLong_FromString(s.c_str(), nullptr, 10);
        }
        case dom_type::double_value:
            return PyFloat_FromDouble(*el.get_double());
        case dom_type::string: {
            const std::string_view sv = *el.get_string();
            return PyUnicode_FromStringAndSize(sv.data(), static_cast<Py_ssize_t>(sv.size()));
        }
        case dom_type::array: {
            const auto arr = *el.get_array();
            PyObject* list = PyList_New(static_cast<Py_ssize_t>(arr.size()));
            if (!list) return nullptr;
            Py_ssize_t idx = 0;
            for (const auto item : arr) {
                PyObject* py_item = dom_element_to_pyobject(item);
                if (!py_item) {
                    Py_DECREF(list);
                    return nullptr;
                }
                PyList_SET_ITEM(list, idx++, py_item);
            }
            return list;
        }
        case dom_type::object: {
            const auto obj = *el.get_object();
            PyObject* dict = PyDict_New();
            if (!dict) return nullptr;
            for (const auto field : obj) {
                PyObject* py_key = PyUnicode_FromStringAndSize(
                    field.key.data(), static_cast<Py_ssize_t>(field.key.size()));
                if (!py_key) {
                    Py_DECREF(dict);
                    return nullptr;
                }
                PyObject* py_val = dom_element_to_pyobject(field.value);
                if (!py_val) {
                    Py_DECREF(py_key);
                    Py_DECREF(dict);
                    return nullptr;
                }
                const int rc = PyDict_SetItem(dict, py_key, py_val);
                Py_DECREF(py_key);
                Py_DECREF(py_val);
                if (rc < 0) {
                    Py_DECREF(dict);
                    return nullptr;
                }
            }
            return dict;
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

inline nb::object turbo_loads_sv(std::string_view sv, fastjson::turbo::dom_parser& parser) {
    fastjson::turbo::result<fastjson::turbo::dom_element> res;
    {
        nb::gil_scoped_release release;
        res = parser.parse(sv);
    }
    if (!res) {
        std::string msg("JSON parse error: ");
        msg.append(res.error().message);
        throw nb::value_error(msg.c_str());
    }
    PyObject* obj = dom_element_to_pyobject(*res);
    if (!obj) throw nb::python_error();
    return nb::steal<nb::object>(obj);
}

inline nb::object turbo_loads_default(std::string_view sv) {
    // Reuse a thread-local parser arena for typical inputs (<= 16 MiB)
    constexpr size_t kMaxThreadLocalArenaInput = 16u * 1024u * 1024u;
    if (sv.size() <= kMaxThreadLocalArenaInput) {
        thread_local fastjson::turbo::dom_parser tl_parser;
        return turbo_loads_sv(sv, tl_parser);
    }
    fastjson::turbo::dom_parser large_parser;
    return turbo_loads_sv(sv, large_parser);
}

class PyTurboParser {
    fastjson::turbo::dom_parser parser_;

public:
    explicit PyTurboParser(size_t max_depth = fastjson::turbo::dom_parser::default_max_depth)
        : parser_(max_depth) {}

    size_t max_depth() const noexcept { return parser_.max_depth(); }

    nb::object loads(nb::handle data) {
        ExtractedInput in = extract_input_bytes(data, true);
        return turbo_loads_sv(in.view, parser_);
    }

    nb::object loads_file(const std::string& filename) {
        std::string content;
        {
            nb::gil_scoped_release release;
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file) {
                throw std::runtime_error("Could not open file: " + filename);
            }
            const auto sz = file.tellg();
            if (sz > 0) {
                content.resize(static_cast<size_t>(sz));
                file.seekg(0, std::ios::beg);
                file.read(content.data(), sz);
            }
        }
        return turbo_loads_sv(content, parser_);
    }
};
#else
// Non-x86 fallback (e.g. AArch64 / Apple Silicon): uses RFC-8259-strict fastjson::parse
inline PyObject* core_json_to_pyobject(const fastjson::json_value& v) {
    if (v.is_null()) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    if (v.is_bool()) {
        PyObject* res = v.as_bool() ? Py_True : Py_False;
        Py_INCREF(res);
        return res;
    }
    if (v.is_int_128()) {
        std::string s = fastjson::stringify(v);
        return PyLong_FromString(s.c_str(), nullptr, 10);
    }
    if (v.is_uint_128()) {
        std::string s = fastjson::stringify(v);
        return PyLong_FromString(s.c_str(), nullptr, 10);
    }
    if (v.is_number()) {
        const double d = v.as_number();
        double ipart = 0.0;
        if (!std::signbit(d) && std::isfinite(d) && std::modf(d, &ipart) == 0.0 &&
            d <= static_cast<double>(1ULL << 53)) {
            return PyLong_FromLongLong(static_cast<long long>(d));
        }
        return PyFloat_FromDouble(d);
    }
    if (v.is_number_128()) {
        return PyFloat_FromDouble(v.as_float64());
    }
    if (v.is_string()) {
        const auto& s = v.as_string();
        return PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size()));
    }
    if (v.is_array()) {
        const auto& arr = v.as_array();
        PyObject* list = PyList_New(static_cast<Py_ssize_t>(arr.size()));
        if (!list) return nullptr;
        for (size_t i = 0; i < arr.size(); ++i) {
            PyObject* item = core_json_to_pyobject(arr[i]);
            if (!item) {
                Py_DECREF(list);
                return nullptr;
            }
            PyList_SET_ITEM(list, static_cast<Py_ssize_t>(i), item);
        }
        return list;
    }
    if (v.is_object()) {
        const auto& obj = v.as_object();
        PyObject* dict = PyDict_New();
        if (!dict) return nullptr;
        for (const auto& [k, val] : obj) {
            PyObject* pk = PyUnicode_FromStringAndSize(k.data(), static_cast<Py_ssize_t>(k.size()));
            if (!pk) {
                Py_DECREF(dict);
                return nullptr;
            }
            PyObject* pv = core_json_to_pyobject(val);
            if (!pv) {
                Py_DECREF(pk);
                Py_DECREF(dict);
                return nullptr;
            }
            const int rc = PyDict_SetItem(dict, pk, pv);
            Py_DECREF(pk);
            Py_DECREF(pv);
            if (rc < 0) {
                Py_DECREF(dict);
                return nullptr;
            }
        }
        return dict;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

inline nb::object turbo_loads_default(std::string_view sv) {
    std::expected<fastjson::json_value, fastjson::json_error> res;
    {
        nb::gil_scoped_release release;
        res = fastjson::parse(sv);
    }
    if (!res) {
        throw nb::value_error(res.error().message.c_str());
    }
    PyObject* obj = core_json_to_pyobject(*res);
    if (!obj) throw nb::python_error();
    return nb::steal<nb::object>(obj);
}

class PyTurboParser {
    size_t max_depth_;

public:
    explicit PyTurboParser(size_t max_depth = 1024) : max_depth_(max_depth ? max_depth : 1) {}
    size_t max_depth() const noexcept { return max_depth_; }

    nb::object loads(nb::handle data) {
        ExtractedInput in = extract_input_bytes(data, true);
        return turbo_loads_default(in.view);
    }

    nb::object loads_file(const std::string& filename) {
        std::string content;
        {
            nb::gil_scoped_release release;
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file) throw std::runtime_error("Could not open file: " + filename);
            const auto sz = file.tellg();
            if (sz > 0) {
                content.resize(static_cast<size_t>(sz));
                file.seekg(0, std::ios::beg);
                file.read(content.data(), sz);
            }
        }
        return turbo_loads_default(content);
    }
};
#endif

// ============================================================================
// Fast Native Python -> JSON String Serializer (dumps)
// ============================================================================
inline void append_escaped_utf8(std::string& out, std::string_view s, bool ensure_ascii) {
    static constexpr char kHex[] = "0123456789abcdef";
    out.push_back('"');
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        size_t run_start = i;
        while (i < n) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if (c < 0x20u || c == '"' || c == '\\' || (ensure_ascii && c >= 0x80u)) {
                break;
            }
            ++i;
        }
        if (i > run_start) {
            out.append(s.data() + run_start, i - run_start);
        }
        if (i >= n) break;

        const unsigned char c = static_cast<unsigned char>(s[i]);
        switch (c) {
            case '"':  out.append("\\\"", 2); ++i; break;
            case '\\': out.append("\\\\", 2); ++i; break;
            case '\b': out.append("\\b", 2);  ++i; break;
            case '\f': out.append("\\f", 2);  ++i; break;
            case '\n': out.append("\\n", 2);  ++i; break;
            case '\r': out.append("\\r", 2);  ++i; break;
            case '\t': out.append("\\t", 2);  ++i; break;
            default:
                if (c < 0x20u) {
                    char esc[6] = {'\\', 'u', '0', '0', kHex[(c >> 4) & 0xFu], kHex[c & 0xFu]};
                    out.append(esc, 6);
                    ++i;
                } else {
                    // Non-ASCII UTF-8 sequence when ensure_ascii == true
                    uint32_t cp = 0;
                    size_t seq_len = 1;
                    if ((c & 0xE0u) == 0xC0u && i + 1 < n) {
                        cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
                        seq_len = 2;
                    } else if ((c & 0xF0u) == 0xE0u && i + 2 < n) {
                        cp = ((c & 0x0Fu) << 12) |
                             ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
                             (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
                        seq_len = 3;
                    } else if ((c & 0xF8u) == 0xF0u && i + 3 < n) {
                        cp = ((c & 0x07u) << 18) |
                             ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) |
                             ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) |
                             (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
                        seq_len = 4;
                    }
                    i += seq_len;
                    if (cp <= 0xFFFFu) {
                        char esc[6] = {
                            '\\', 'u',
                            kHex[(cp >> 12) & 0xFu], kHex[(cp >> 8) & 0xFu],
                            kHex[(cp >> 4) & 0xFu],  kHex[cp & 0xFu]
                        };
                        out.append(esc, 6);
                    } else {
                        cp -= 0x10000u;
                        const uint32_t hi = 0xD800u + ((cp >> 10) & 0x3FFu);
                        const uint32_t lo = 0xDC00u + (cp & 0x3FFu);
                        char esc[12] = {
                            '\\', 'u',
                            kHex[(hi >> 12) & 0xFu], kHex[(hi >> 8) & 0xFu],
                            kHex[(hi >> 4) & 0xFu],  kHex[hi & 0xFu],
                            '\\', 'u',
                            kHex[(lo >> 12) & 0xFu], kHex[(lo >> 8) & 0xFu],
                            kHex[(lo >> 4) & 0xFu],  kHex[lo & 0xFu]
                        };
                        out.append(esc, 12);
                    }
                }
                break;
        }
    }
    out.push_back('"');
}

inline void append_double_json(std::string& out, double d, bool allow_nan) {
    if (!std::isfinite(d)) {
        if (!allow_nan) {
            throw nb::value_error("Out of range float values (NaN, Infinity) are not JSON compliant");
        }
        if (std::isnan(d)) {
            out.append("NaN", 3);
        } else if (d < 0) {
            out.append("-Infinity", 9);
        } else {
            out.append("Infinity", 8);
        }
        return;
    }
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), d);
    if (ec != std::errc{}) {
        out.append("0.0", 3);
        return;
    }
    const std::string_view sv(buf, static_cast<size_t>(ptr - buf));
    out.append(sv);
    if (sv.find_first_of(".eE") == std::string_view::npos) {
        out.append(".0", 2);
    }
}

inline void serialize_pyobject(
    PyObject* p,
    std::string& out,
    int indent,
    int current_indent,
    bool ensure_ascii,
    bool allow_nan,
    int depth) {
    if (depth > 1000) {
        throw nb::value_error("Maximum recursion depth exceeded during JSON serialization");
    }

    if (p == Py_None) {
        out.append("null", 4);
        return;
    }
    if (PyBool_Check(p)) {
        if (p == Py_True) out.append("true", 4);
        else out.append("false", 5);
        return;
    }
    if (PyLong_Check(p)) {
        int overflow = 0;
        const long long v = PyLong_AsLongLongAndOverflow(p, &overflow);
        if (!overflow && !PyErr_Occurred()) {
            char buf[32];
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
            if (ec == std::errc{}) {
                out.append(buf, static_cast<size_t>(ptr - buf));
                return;
            }
        }
        PyErr_Clear();
        PyObject* s_obj = PyObject_Str(p);
        if (!s_obj) throw nb::python_error();
        Py_ssize_t slen = 0;
        const char* s_ptr = PyUnicode_AsUTF8AndSize(s_obj, &slen);
        if (!s_ptr) {
            Py_DECREF(s_obj);
            throw nb::python_error();
        }
        out.append(s_ptr, static_cast<size_t>(slen));
        Py_DECREF(s_obj);
        return;
    }
    if (PyFloat_Check(p)) {
        append_double_json(out, PyFloat_AS_DOUBLE(p), allow_nan);
        return;
    }
    if (PyUnicode_Check(p)) {
        Py_ssize_t slen = 0;
        const char* s_ptr = PyUnicode_AsUTF8AndSize(p, &slen);
        if (!s_ptr) throw nb::python_error();
        append_escaped_utf8(out, std::string_view(s_ptr, static_cast<size_t>(slen)), ensure_ascii);
        return;
    }
    if (PyList_Check(p) || PyTuple_Check(p)) {
        const bool is_list = PyList_Check(p);
        const Py_ssize_t n = is_list ? PyList_GET_SIZE(p) : PyTuple_GET_SIZE(p);
        if (n == 0) {
            out.append("[]", 2);
            return;
        }
        out.push_back('[');
        const int next_indent = (indent >= 0) ? (current_indent + indent) : -1;
        for (Py_ssize_t i = 0; i < n; ++i) {
            if (i > 0) out.push_back(',');
            if (indent >= 0) {
                out.push_back('\n');
                out.append(static_cast<size_t>(next_indent), ' ');
            }
            PyObject* item = is_list ? PyList_GET_ITEM(p, i) : PyTuple_GET_ITEM(p, i);
            serialize_pyobject(item, out, indent, next_indent, ensure_ascii, allow_nan, depth + 1);
        }
        if (indent >= 0) {
            out.push_back('\n');
            out.append(static_cast<size_t>(current_indent), ' ');
        }
        out.push_back(']');
        return;
    }
    if (PyDict_Check(p)) {
        const Py_ssize_t n = PyDict_Size(p);
        if (n == 0) {
            out.append("{}", 2);
            return;
        }
        out.push_back('{');
        const int next_indent = (indent >= 0) ? (current_indent + indent) : -1;
        PyObject* k = nullptr;
        PyObject* v = nullptr;
        Py_ssize_t pos = 0;
        bool first = true;
        while (PyDict_Next(p, &pos, &k, &v)) {
            if (!PyUnicode_Check(k)) {
                throw nb::type_error("JSON object keys must be strings");
            }
            if (!first) out.push_back(',');
            first = false;
            if (indent >= 0) {
                out.push_back('\n');
                out.append(static_cast<size_t>(next_indent), ' ');
            }
            Py_ssize_t klen = 0;
            const char* kptr = PyUnicode_AsUTF8AndSize(k, &klen);
            if (!kptr) throw nb::python_error();
            append_escaped_utf8(out, std::string_view(kptr, static_cast<size_t>(klen)), ensure_ascii);
            if (indent >= 0) out.append(": ", 2);
            else out.push_back(':');
            serialize_pyobject(v, out, indent, next_indent, ensure_ascii, allow_nan, depth + 1);
        }
        if (indent >= 0) {
            out.push_back('\n');
            out.append(static_cast<size_t>(current_indent), ' ');
        }
        out.push_back('}');
        return;
    }

    nb::handle h(p);
    if (nb::isinstance<json_value>(h)) {
        const auto& jv = nb::cast<const json_value&>(h);
        if (indent >= 0) out.append(fastjson_parallel::prettify(jv, indent));
        else out.append(fastjson_parallel::stringify(jv));
        return;
    }
    if (nb::isinstance<std::vector<int64_t>>(h)) {
        const auto& vec = nb::cast<const std::vector<int64_t>&>(h);
        out.push_back('[');
        char buf[32];
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) out.push_back(',');
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), vec[i]);
            if (ec == std::errc{}) out.append(buf, static_cast<size_t>(ptr - buf));
        }
        out.push_back(']');
        return;
    }
    if (nb::isinstance<std::vector<uint64_t>>(h)) {
        const auto& vec = nb::cast<const std::vector<uint64_t>&>(h);
        out.push_back('[');
        char buf[32];
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) out.push_back(',');
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), vec[i]);
            if (ec == std::errc{}) out.append(buf, static_cast<size_t>(ptr - buf));
        }
        out.push_back(']');
        return;
    }
    if (nb::isinstance<std::vector<double>>(h)) {
        const auto& vec = nb::cast<const std::vector<double>&>(h);
        out.push_back('[');
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) out.push_back(',');
            append_double_json(out, vec[i], allow_nan);
        }
        out.push_back(']');
        return;
    }
    if (nb::isinstance<std::vector<std::string>>(h)) {
        const auto& vec = nb::cast<const std::vector<std::string>&>(h);
        out.push_back('[');
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) out.push_back(',');
            append_escaped_utf8(out, vec[i], ensure_ascii);
        }
        out.push_back(']');
        return;
    }

    throw nb::type_error("Object of unsupported type is not JSON serializable");
}

inline std::string py_dumps_impl(nb::handle obj, int indent, bool ensure_ascii, bool allow_nan) {
    std::string out;
    out.reserve(256);
    serialize_pyobject(obj.ptr(), out, indent, 0, ensure_ascii, allow_nan, 0);
    return out;
}

// ============================================================================
// PostgreSQL Binary jsonb Wire Format Helpers
// ============================================================================
inline void validate_pg_jsonb_wire_header(std::string_view wire) {
    if (wire.empty()) {
        throw nb::value_error("jsonb: empty wire value");
    }
    const auto ver = static_cast<uint8_t>(wire.front());
    if (ver != fastjson::pg_jsonb_wire_version) {
        std::string msg = "jsonb: unsupported wire version " + std::to_string(ver);
        throw nb::value_error(msg.c_str());
    }
}

// ============================================================================
// Python Key Wrapper for LINQ Ordering / Grouping / Distinct
// ============================================================================
struct PySortKey {
    nb::object key;

    bool operator<(const PySortKey& other) const {
        nb::gil_scoped_acquire acquire;
        const int rc = PyObject_RichCompareBool(key.ptr(), other.key.ptr(), Py_LT);
        if (rc < 0) throw nb::python_error();
        return rc == 1;
    }

    bool operator>(const PySortKey& other) const {
        nb::gil_scoped_acquire acquire;
        const int rc = PyObject_RichCompareBool(key.ptr(), other.key.ptr(), Py_GT);
        if (rc < 0) throw nb::python_error();
        return rc == 1;
    }

    bool operator==(const PySortKey& other) const {
        nb::gil_scoped_acquire acquire;
        const int rc = PyObject_RichCompareBool(key.ptr(), other.key.ptr(), Py_EQ);
        if (rc < 0) throw nb::python_error();
        return rc == 1;
    }
};

// ============================================================================
// Nanobind Module Definition
// ============================================================================
NB_MODULE(fastjson, m) {
    m.doc() = "FastestJSONInTheWest Python Bindings (TurboDOM SIMD, COW, Parallel, LINQ, Mustache, PG jsonb, GPU)";
    m.attr("__version__") = "2.0.0";
    m.attr("PG_JSONB_WIRE_VERSION") = static_cast<int>(fastjson::pg_jsonb_wire_version);

    // ========================================================================
    // Zero-Copy Typed C++ Vectors
    // ========================================================================
    nb::bind_vector<std::vector<int64_t>>(m, "Int64Vector")
        .def("to_list", [](const std::vector<int64_t>& self) {
            PyObject* lst = PyList_New(static_cast<Py_ssize_t>(self.size()));
            if (!lst) throw nb::python_error();
            for (size_t i = 0; i < self.size(); ++i) {
                PyObject* item = PyLong_FromLongLong(static_cast<long long>(self[i]));
                if (!item) { Py_DECREF(lst); throw nb::python_error(); }
                PyList_SET_ITEM(lst, static_cast<Py_ssize_t>(i), item);
            }
            return nb::steal<nb::list>(lst);
        }, "Convert Int64Vector to a Python list")
        .def("sum", [](const std::vector<int64_t>& self) {
            int64_t acc = 0;
            for (int64_t v : self) acc += v;
            return acc;
        }, "Sum all elements in C++")
        .def("to_json", [](const std::vector<int64_t>& self) {
            std::string out;
            out.push_back('[');
            char buf[32];
            for (size_t i = 0; i < self.size(); ++i) {
                if (i > 0) out.push_back(',');
                auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), self[i]);
                if (ec == std::errc{}) out.append(buf, static_cast<size_t>(ptr - buf));
            }
            out.push_back(']');
            return out;
        }, "Serialize Int64Vector directly to JSON array string");

    nb::bind_vector<std::vector<uint64_t>>(m, "UInt64Vector")
        .def("to_list", [](const std::vector<uint64_t>& self) {
            PyObject* lst = PyList_New(static_cast<Py_ssize_t>(self.size()));
            if (!lst) throw nb::python_error();
            for (size_t i = 0; i < self.size(); ++i) {
                PyObject* item = PyLong_FromUnsignedLongLong(static_cast<unsigned long long>(self[i]));
                if (!item) { Py_DECREF(lst); throw nb::python_error(); }
                PyList_SET_ITEM(lst, static_cast<Py_ssize_t>(i), item);
            }
            return nb::steal<nb::list>(lst);
        }, "Convert UInt64Vector to a Python list")
        .def("sum", [](const std::vector<uint64_t>& self) {
            uint64_t acc = 0;
            for (uint64_t v : self) acc += v;
            return acc;
        }, "Sum all elements in C++")
        .def("to_json", [](const std::vector<uint64_t>& self) {
            std::string out;
            out.push_back('[');
            char buf[32];
            for (size_t i = 0; i < self.size(); ++i) {
                if (i > 0) out.push_back(',');
                auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), self[i]);
                if (ec == std::errc{}) out.append(buf, static_cast<size_t>(ptr - buf));
            }
            out.push_back(']');
            return out;
        }, "Serialize UInt64Vector directly to JSON array string");

    nb::bind_vector<std::vector<double>>(m, "Float64Vector")
        .def("to_list", [](const std::vector<double>& self) {
            PyObject* lst = PyList_New(static_cast<Py_ssize_t>(self.size()));
            if (!lst) throw nb::python_error();
            for (size_t i = 0; i < self.size(); ++i) {
                PyObject* item = PyFloat_FromDouble(self[i]);
                if (!item) { Py_DECREF(lst); throw nb::python_error(); }
                PyList_SET_ITEM(lst, static_cast<Py_ssize_t>(i), item);
            }
            return nb::steal<nb::list>(lst);
        }, "Convert Float64Vector to a Python list")
        .def("sum", [](const std::vector<double>& self) {
            double acc = 0.0;
            for (double v : self) acc += v;
            return acc;
        }, "Sum all elements in C++")
        .def("to_json", [](const std::vector<double>& self) {
            std::string out;
            out.push_back('[');
            for (size_t i = 0; i < self.size(); ++i) {
                if (i > 0) out.push_back(',');
                append_double_json(out, self[i], false);
            }
            out.push_back(']');
            return out;
        }, "Serialize Float64Vector directly to JSON array string");

    nb::bind_vector<std::vector<std::string>>(m, "StringVector")
        .def("to_list", [](const std::vector<std::string>& self) {
            PyObject* lst = PyList_New(static_cast<Py_ssize_t>(self.size()));
            if (!lst) throw nb::python_error();
            for (size_t i = 0; i < self.size(); ++i) {
                PyObject* item = PyUnicode_FromStringAndSize(
                    self[i].data(), static_cast<Py_ssize_t>(self[i].size()));
                if (!item) { Py_DECREF(lst); throw nb::python_error(); }
                PyList_SET_ITEM(lst, static_cast<Py_ssize_t>(i), item);
            }
            return nb::steal<nb::list>(lst);
        }, "Convert StringVector to a Python list")
        .def("to_json", [](const std::vector<std::string>& self) {
            std::string out;
            out.push_back('[');
            for (size_t i = 0; i < self.size(); ++i) {
                if (i > 0) out.push_back(',');
                append_escaped_utf8(out, self[i], false);
            }
            out.push_back(']');
            return out;
        }, "Serialize StringVector directly to JSON array string");

    // ========================================================================
    // JSONValue (Copy-On-Write DOM Value)
    // ========================================================================
    nb::class_<json_value>(m, "JSONValue")
        .def(nb::init<>())
        .def(nb::init<bool>())
        .def(nb::init<int64_t>())
        .def(nb::init<double>())
        .def(nb::init<const std::string&>())
        .def_static("object", []() { return json_value(fastjson_parallel::json_object{}); },
                    "Create an empty JSON object JSONValue")
        .def_static("array", []() { return json_value(fastjson_parallel::json_array{}); },
                    "Create an empty JSON array JSONValue")
        .def_static("from_python", [](nb::handle obj) { return py_to_json_value(obj); },
                    "Convert a native Python value into a JSONValue")
        .def("is_null", &json_value::is_null)
        .def("is_bool", &json_value::is_bool)
        .def("is_number", &json_value::is_number)
        .def("is_int_128", &json_value::is_int_128)
        .def("is_uint_128", &json_value::is_uint_128)
        .def("is_number_128", &json_value::is_number_128)
        .def("is_string", &json_value::is_string)
        .def("is_array", &json_value::is_array)
        .def("is_object", &json_value::is_object)
        .def("empty", &json_value::empty)
        .def("as_bool", [](const json_value& v) {
            if (!v.is_bool()) throw nb::type_error("Not a boolean");
            return v.as_bool();
        })
        .def("as_number", [](const json_value& v) {
            if (v.is_number()) return v.as_number();
            if (v.is_int_128()) return static_cast<double>(v.as_int_128());
            if (v.is_uint_128()) return static_cast<double>(v.as_uint_128());
            if (v.is_number_128()) return static_cast<double>(v.as_number_128());
            throw nb::type_error("Not a number");
        })
        .def("as_float", [](const json_value& v) {
            if (v.is_number()) return v.as_number();
            if (v.is_int_128()) return static_cast<double>(v.as_int_128());
            if (v.is_uint_128()) return static_cast<double>(v.as_uint_128());
            if (v.is_number_128()) return static_cast<double>(v.as_number_128());
            throw nb::type_error("Not a number");
        })
        .def("as_int", [](const json_value& v) {
            if (v.is_number()) return static_cast<int64_t>(v.as_number());
            if (v.is_int_128()) return static_cast<int64_t>(v.as_int_128());
            if (v.is_uint_128()) return static_cast<int64_t>(v.as_uint_128());
            throw nb::type_error("Not a number or 128-bit integer");
        })
        .def("as_string", [](const json_value& v) {
            if (!v.is_string()) throw nb::type_error("Not a string");
            return v.as_string();
        })
        .def("contains", [](const json_value& v, const std::string& key) {
            if (!v.is_object()) return false;
            return v.as_object().contains(key);
        })
        .def("__contains__", [](const json_value& v, const std::string& key) {
            if (!v.is_object()) throw nb::type_error("'in' with string key requires a JSON object");
            return v.as_object().contains(key);
        })
        .def("get", [](const json_value& v, const std::string& key, nb::object default_val) -> nb::object {
            if (!v.is_object()) throw nb::type_error("get() requires a JSON object");
            const auto& obj = v.as_object();
            const auto it = obj.find(key);
            if (it == obj.end()) return default_val;
            return nb::cast(it->second);
        }, "key"_a, "default"_a = nb::none(), "Safe key lookup returning JSONValue or default")
        .def("keys", [](const json_value& v) {
            if (!v.is_object()) throw nb::type_error("keys() requires a JSON object");
            nb::list out;
            for (const auto& [k, _] : v.as_object()) out.append(nb::str(k.data(), k.size()));
            return out;
        })
        .def("values", [](const json_value& v) {
            if (!v.is_object()) throw nb::type_error("values() requires a JSON object");
            nb::list out;
            for (const auto& [_, val] : v.as_object()) out.append(nb::cast(val));
            return out;
        })
        .def("items", [](const json_value& v) {
            if (!v.is_object()) throw nb::type_error("items() requires a JSON object");
            nb::list out;
            for (const auto& [k, val] : v.as_object()) {
                out.append(nb::make_tuple(nb::str(k.data(), k.size()), nb::cast(val)));
            }
            return out;
        })
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
            v.as_object()[key] = val;
        })
        .def("__setitem__", [](json_value& v, size_t index, const json_value& val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = val;
        })
        .def("__setitem__", [](json_value& v, size_t index, bool val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, size_t index, int64_t val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, size_t index, double val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, size_t index, const std::string& val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, size_t index, nb::handle val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            v.as_array()[index] = py_to_json_value(val);
        })
        .def("__setitem__", [](json_value& v, const std::string& key, bool val) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            v.as_object()[key] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, const std::string& key, int64_t val) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            v.as_object()[key] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, const std::string& key, double val) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            v.as_object()[key] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, const std::string& key, const std::string& val) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            v.as_object()[key] = json_value(val);
        })
        .def("__setitem__", [](json_value& v, const std::string& key, nb::handle val) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            v.as_object()[key] = py_to_json_value(val);
        })
        .def("__delitem__", [](json_value& v, const std::string& key) {
            if (!v.is_object()) throw nb::type_error("Not an object");
            auto& obj = v.as_object();
            const auto it = obj.find(key);
            if (it == obj.end()) throw nb::key_error(key.c_str());
            obj.erase(it);
        })
        .def("__delitem__", [](json_value& v, size_t index) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            if (index >= v.size()) throw nb::index_error();
            auto& arr = v.as_array();
            arr.erase(arr.begin() + static_cast<std::ptrdiff_t>(index));
        })
        .def("append", [](json_value& v, const json_value& val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            v.as_array().push_back(val);
        })
        .def("append", [](json_value& v, nb::handle val) {
            if (!v.is_array()) throw nb::type_error("Not an array");
            v.as_array().push_back(py_to_json_value(val));
        })
        .def("copy", [](const json_value& v) {
            return json_value(v);  // O(1) Copy-On-Write shared_ptr copy
        })
        .def("clear", [](json_value& v) {
            v.clear();
        })
        .def("__len__", [](const json_value& v) {
            return v.size();
        })
        .def("stringify", [](const json_value& v) {
            return fastjson_parallel::stringify(v);
        }, "Serialize JSONValue to compact JSON string")
        .def("prettify", [](const json_value& v, int indent) {
            return fastjson_parallel::prettify(v, indent);
        }, "indent"_a = 2, "Serialize JSONValue to pretty-printed JSON string")
        .def("to_json", [](const json_value& v, int indent) {
            if (indent >= 0) return fastjson_parallel::prettify(v, indent);
            return fastjson_parallel::stringify(v);
        }, "indent"_a = -1, "Serialize JSONValue to JSON string (compact when indent < 0)")
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

    // ========================================================================
    // Threading & Core Parsing APIs
    // ========================================================================
    m.def("set_num_threads", [](int threads) {
        fastjson_parallel::set_num_threads(threads);
    }, "threads"_a, "Set the number of threads for parallel operations");

    m.def("get_num_threads", []() {
        return fastjson_parallel::get_num_threads();
    }, "Get the number of threads for parallel operations");

    m.def("parse", [](const std::string& s) {
        auto result = fastjson_parallel::parse(s);
        if (!result) throw std::runtime_error("Parse error: " + result.error().message);
        return result.value();
    }, "s"_a, "Parse JSON from a string into a COW JSONValue (GIL-free)", nb::call_guard<nb::gil_scoped_release>());

    m.def("parse", [](nb::bytes b) {
        const std::string_view sv(b.c_str(), b.size());
        nb::gil_scoped_release release;
        auto result = fastjson_parallel::parse(sv);
        if (!result) throw std::runtime_error("Parse error: " + result.error().message);
        return result.value();
    }, "b"_a, "Parse JSON from bytes into a COW JSONValue (GIL-free)");

    m.def("parse_parallel", [](const std::string& s, int num_threads) {
        if (num_threads >= 0) {
            fastjson_parallel::set_num_threads(num_threads);
        }
        auto result = fastjson_parallel::parse(s);
        if (!result) throw std::runtime_error("Parse error: " + result.error().message);
        return result.value();
    }, "s"_a, "num_threads"_a = -1,
       "Parse large JSON string in parallel with TBB/SIMD (GIL-free)",
       nb::call_guard<nb::gil_scoped_release>());

    m.def("parse_file", [](const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Could not open file: " + filename);
        const auto sz = file.tellg();
        std::string content;
        if (sz > 0) {
            content.resize(static_cast<size_t>(sz));
            file.seekg(0, std::ios::beg);
            file.read(content.data(), sz);
        }

        auto result = fastjson_parallel::parse(content);
        if (!result) throw std::runtime_error("Parse error: " + result.error().message);
        return result.value();
    }, "filename"_a, "Parse JSON from a file into a COW JSONValue (GIL-free)",
       nb::call_guard<nb::gil_scoped_release>());

    // ========================================================================
    // High-Speed TurboDOM Native Python API (loads / dumps / TurboParser)
    // ========================================================================
    nb::class_<PyTurboParser>(m, "TurboParser",
        "Reusable zero-allocation SIMD TurboDOM parser returning native Python objects")
        .def(nb::init<size_t>(), "max_depth"_a = 1024)
        .def_prop_ro("max_depth", &PyTurboParser::max_depth)
        .def("loads", &PyTurboParser::loads, "data"_a,
             "Parse JSON (str, bytes, bytearray, or memoryview) into native Python objects")
        .def("loads_file", &PyTurboParser::loads_file, "filename"_a,
             "Parse a JSON file directly into native Python objects");

    m.def("loads", [](nb::handle data) {
        ExtractedInput in = extract_input_bytes(data, true);
        return turbo_loads_default(in.view);
    }, "data"_a,
       "Parse JSON (str, bytes, bytearray, or memoryview) directly into native Python objects "
       "using the fused tiled SIMD TurboDOM parser with full RFC 8259 validation");

    m.def("parse_turbo", [](nb::handle data) {
        ExtractedInput in = extract_input_bytes(data, true);
        return turbo_loads_default(in.view);
    }, "data"_a, "Alias for loads(): parse JSON via SIMD TurboDOM into native Python objects");

    m.def("loads_file", [](const std::string& filename) {
        std::string content;
        {
            nb::gil_scoped_release release;
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file) throw std::runtime_error("Could not open file: " + filename);
            const auto sz = file.tellg();
            if (sz > 0) {
                content.resize(static_cast<size_t>(sz));
                file.seekg(0, std::ios::beg);
                file.read(content.data(), sz);
            }
        }
        return turbo_loads_default(content);
    }, "filename"_a, "Parse a JSON file directly into native Python objects via SIMD TurboDOM");

    m.def("dumps", [](nb::handle obj, int indent, bool ensure_ascii, bool allow_nan) {
        return py_dumps_impl(obj, indent, ensure_ascii, allow_nan);
    }, "obj"_a, "indent"_a = -1, "ensure_ascii"_a = false, "allow_nan"_a = false,
       "Serialize Python objects or JSONValue to a JSON str");

    m.def("dumps_bytes", [](nb::handle obj, int indent, bool ensure_ascii, bool allow_nan) {
        const std::string s = py_dumps_impl(obj, indent, ensure_ascii, allow_nan);
        return nb::bytes(s.data(), s.size());
    }, "obj"_a, "indent"_a = -1, "ensure_ascii"_a = false, "allow_nan"_a = false,
       "Serialize Python objects or JSONValue to UTF-8 JSON bytes");

    m.def("stringify", [](const json_value& v) {
        return fastjson_parallel::stringify(v);
    }, "value"_a, "Serialize a JSONValue to compact JSON text",
       nb::call_guard<nb::gil_scoped_release>());

    m.def("stringify", [](nb::handle obj) {
        return py_dumps_impl(obj, -1, false, false);
    }, "obj"_a, "Serialize a Python object to compact JSON text");

    m.def("prettify", [](const json_value& v, int indent) {
        return fastjson_parallel::prettify(v, indent);
    }, "value"_a, "indent"_a = 2, "Serialize a JSONValue to indented JSON text",
       nb::call_guard<nb::gil_scoped_release>());

    m.def("prettify", [](nb::handle obj, int indent) {
        return py_dumps_impl(obj, indent, false, false);
    }, "obj"_a, "indent"_a = 2, "Serialize a Python object to indented JSON text");

    // ========================================================================
    // PostgreSQL jsonb Binary Wire Adapter (version byte 0x01 + JSON text)
    // ========================================================================
    m.def("parse_pg_jsonb", [](nb::handle wire_obj) {
        ExtractedInput in = extract_input_bytes(wire_obj, false);
        validate_pg_jsonb_wire_header(in.view);
        const std::string_view payload = in.view.substr(1);
        nb::gil_scoped_release release;
        // Validate against RFC-8259-strict fastjson::parse_pg_jsonb first
        auto strict_check = fastjson::parse_pg_jsonb(in.view);
        if (!strict_check) {
            throw nb::value_error(strict_check.error().message.c_str());
        }
        auto par_res = fastjson_parallel::parse(payload);
        if (!par_res) {
            throw nb::value_error(par_res.error().message.c_str());
        }
        return std::move(*par_res);
    }, "wire"_a,
       "Parse PostgreSQL binary-mode jsonb wire bytes (0x01 + JSON text) into a JSONValue");

    m.def("loads_pg_jsonb", [](nb::handle wire_obj) {
        ExtractedInput in = extract_input_bytes(wire_obj, false);
        validate_pg_jsonb_wire_header(in.view);
        return turbo_loads_default(in.view.substr(1));
    }, "wire"_a,
       "Parse PostgreSQL binary-mode jsonb wire bytes (0x01 + JSON text) directly into native Python objects");

    m.def("dumps_pg_jsonb", [](nb::handle obj, bool ensure_ascii, bool allow_nan) {
        std::string wire;
        wire.reserve(256);
        wire.push_back(static_cast<char>(fastjson::pg_jsonb_wire_version));
        serialize_pyobject(obj.ptr(), wire, -1, 0, ensure_ascii, allow_nan, 0);
        return nb::bytes(wire.data(), wire.size());
    }, "obj"_a, "ensure_ascii"_a = false, "allow_nan"_a = false,
       "Serialize Python objects or JSONValue into PostgreSQL binary-mode jsonb wire bytes (0x01 + JSON)");

    // ========================================================================
    // Complete Lazy Fluent LINQ Bindings (JSONQuery)
    // ========================================================================
    using Query = fastjson::linq::query_result<json_value>;

    auto wrap_predicate = [](nb::callable pred) {
        return [pred](const json_value& v) -> bool {
            nb::gil_scoped_acquire acquire;
            return nb::cast<bool>(pred(v));
        };
    };

    auto wrap_transform = [](nb::callable fn) {
        return [fn](const json_value& v) -> json_value {
            nb::gil_scoped_acquire acquire;
            return py_to_json_value(fn(v));
        };
    };

    auto wrap_numeric_selector = [](nb::object selector) {
        return [selector](const json_value& v) -> double {
            nb::gil_scoped_acquire acquire;
            if (selector.is_none()) {
                if (v.is_number()) return v.as_number();
                if (v.is_int_128()) return static_cast<double>(v.as_int_128());
                if (v.is_uint_128()) return static_cast<double>(v.as_uint_128());
                if (v.is_number_128()) return static_cast<double>(v.as_number_128());
                throw nb::type_error("JSONValue element is not numeric; pass a selector function");
            }
            nb::object res = selector(v);
            if (nb::isinstance<json_value>(res)) {
                const auto& jv = nb::cast<const json_value&>(res);
                if (jv.is_number()) return jv.as_number();
                if (jv.is_int_128()) return static_cast<double>(jv.as_int_128());
                if (jv.is_uint_128()) return static_cast<double>(jv.as_uint_128());
                throw nb::type_error("Selected JSONValue is not numeric");
            }
            return nb::cast<double>(res);
        };
    };

    nb::class_<Query>(m, "JSONQuery", "Lazy push-transducer LINQ pipeline over JSONValue sequences")
        .def("is_lazy", &Query::is_lazy)
        .def("where", [wrap_predicate](const Query& self, nb::callable predicate) {
            return self.where(wrap_predicate(std::move(predicate)));
        }, "predicate"_a)
        .def("filter", [wrap_predicate](const Query& self, nb::callable predicate) {
            return self.filter(wrap_predicate(std::move(predicate)));
        }, "predicate"_a)
        .def("select", [wrap_transform](const Query& self, nb::callable transform) {
            return self.select(wrap_transform(std::move(transform)));
        }, "transform"_a)
        .def("map", [wrap_transform](const Query& self, nb::callable transform) {
            return self.map(wrap_transform(std::move(transform)));
        }, "transform"_a)
        .def("take", &Query::take, "n"_a)
        .def("skip", &Query::skip, "n"_a)
        .def("take_while", [wrap_predicate](const Query& self, nb::callable predicate) {
            return self.take_while(wrap_predicate(std::move(predicate)));
        }, "predicate"_a)
        .def("skip_while", [wrap_predicate](const Query& self, nb::callable predicate) {
            return self.skip_while(wrap_predicate(std::move(predicate)));
        }, "predicate"_a)
        .def("reverse", &Query::reverse)
        .def("order_by", [](const Query& self, nb::callable key_selector) {
            return self.order_by([key_selector](const json_value& v) -> PySortKey {
                nb::gil_scoped_acquire acquire;
                nb::object k = key_selector(v);
                if (nb::isinstance<json_value>(k)) {
                    ConversionConfig cfg;
                    k = to_python_optimized<false>(nb::cast<const json_value&>(k), cfg);
                }
                return PySortKey{std::move(k)};
            });
        }, "key_selector"_a)
        .def("order_by_descending", [](const Query& self, nb::callable key_selector) {
            return self.order_by_descending([key_selector](const json_value& v) -> PySortKey {
                nb::gil_scoped_acquire acquire;
                nb::object k = key_selector(v);
                if (nb::isinstance<json_value>(k)) {
                    ConversionConfig cfg;
                    k = to_python_optimized<false>(nb::cast<const json_value&>(k), cfg);
                }
                return PySortKey{std::move(k)};
            });
        }, "key_selector"_a)
        .def("distinct", [](const Query& self) {
            return self.distinct_by([](const json_value& v) {
                return fastjson_parallel::stringify(v);
            });
        })
        .def("distinct_by", [](const Query& self, nb::callable key_selector) {
            return self.distinct_by([key_selector](const json_value& v) -> PySortKey {
                nb::gil_scoped_acquire acquire;
                nb::object k = key_selector(v);
                if (nb::isinstance<json_value>(k)) {
                    ConversionConfig cfg;
                    k = to_python_optimized<false>(nb::cast<const json_value&>(k), cfg);
                }
                return PySortKey{std::move(k)};
            });
        }, "key_selector"_a)
        .def("prefix_sum", [](const Query& self, nb::object func) {
            if (func.is_none()) {
                return self.prefix_sum([](const json_value& a, const json_value& b) -> json_value {
                    return json_value(a.as_float64() + b.as_float64());
                });
            }
            nb::callable fn = nb::borrow<nb::callable>(func);
            return self.prefix_sum([fn](const json_value& a, const json_value& b) -> json_value {
                nb::gil_scoped_acquire acquire;
                return py_to_json_value(fn(a, b));
            });
        }, "func"_a = nb::none())
        .def("scan", [](const Query& self, nb::object func) {
            if (func.is_none()) {
                return self.scan([](const json_value& a, const json_value& b) -> json_value {
                    return json_value(a.as_float64() + b.as_float64());
                });
            }
            nb::callable fn = nb::borrow<nb::callable>(func);
            return self.scan([fn](const json_value& a, const json_value& b) -> json_value {
                nb::gil_scoped_acquire acquire;
                return py_to_json_value(fn(a, b));
            });
        }, "func"_a = nb::none())
        .def("concat", &Query::concat, "other"_a)
        .def("union_with", [](const Query& self, const Query& other) {
            return self.concat(other).distinct_by([](const json_value& v) {
                return fastjson_parallel::stringify(v);
            });
        }, "other"_a)
        .def("zip", [](const Query& self, const Query& other, nb::object result_selector) {
            if (result_selector.is_none()) {
                return self.zip(other, [](const json_value& a, const json_value& b) -> json_value {
                    fastjson_parallel::json_array pair;
                    pair.reserve(2);
                    pair.push_back(a);
                    pair.push_back(b);
                    return json_value(std::move(pair));
                });
            }
            nb::callable fn = nb::borrow<nb::callable>(result_selector);
            return self.zip(other, [fn](const json_value& a, const json_value& b) -> json_value {
                nb::gil_scoped_acquire acquire;
                return py_to_json_value(fn(a, b));
            });
        }, "other"_a, "result_selector"_a = nb::none())
        .def("first", [wrap_predicate](const Query& self, nb::object predicate) -> std::optional<json_value> {
            if (predicate.is_none()) return self.first();
            return self.first(wrap_predicate(nb::borrow<nb::callable>(predicate)));
        }, "predicate"_a = nb::none())
        .def("last", [wrap_predicate](const Query& self, nb::object predicate) -> std::optional<json_value> {
            if (predicate.is_none()) return self.last();
            return self.last(wrap_predicate(nb::borrow<nb::callable>(predicate)));
        }, "predicate"_a = nb::none())
        .def("single", &Query::single)
        .def("find", [wrap_predicate](const Query& self, nb::callable predicate) {
            return self.find(wrap_predicate(std::move(predicate)));
        }, "predicate"_a)
        .def("find_index", [wrap_predicate](const Query& self, nb::callable predicate) {
            return self.find_index(wrap_predicate(std::move(predicate)));
        }, "predicate"_a)
        .def("any", [wrap_predicate](const Query& self, nb::object predicate) -> bool {
            if (predicate.is_none()) return self.any();
            return self.any(wrap_predicate(nb::borrow<nb::callable>(predicate)));
        }, "predicate"_a = nb::none())
        .def("all", [wrap_predicate](const Query& self, nb::callable predicate) -> bool {
            return self.all(wrap_predicate(std::move(predicate)));
        }, "predicate"_a)
        .def("count", [wrap_predicate](const Query& self, nb::object predicate) -> size_t {
            if (predicate.is_none()) return self.count();
            return self.count(wrap_predicate(nb::borrow<nb::callable>(predicate)));
        }, "predicate"_a = nb::none())
        .def("empty", &Query::empty)
        .def("reduce", [](const Query& self, nb::callable func, nb::object seed) -> nb::object {
            if (seed.is_none()) {
                auto res = self.reduce([func](const json_value& acc, const json_value& item) -> json_value {
                    nb::gil_scoped_acquire acquire;
                    return py_to_json_value(func(acc, item));
                });
                if (!res) return nb::none();
                return nb::cast(*res);
            }
            return self.reduce(seed, [func](nb::object acc, const json_value& item) -> nb::object {
                nb::gil_scoped_acquire acquire;
                return func(acc, item);
            });
        }, "func"_a, "seed"_a = nb::none())
        .def("aggregate", [](const Query& self, nb::callable func, nb::object seed) -> nb::object {
            if (seed.is_none()) {
                auto res = self.aggregate([func](const json_value& acc, const json_value& item) -> json_value {
                    nb::gil_scoped_acquire acquire;
                    return py_to_json_value(func(acc, item));
                });
                if (!res) return nb::none();
                return nb::cast(*res);
            }
            return self.aggregate(seed, [func](nb::object acc, const json_value& item) -> nb::object {
                nb::gil_scoped_acquire acquire;
                return func(acc, item);
            });
        }, "func"_a, "seed"_a = nb::none())
        .def("sum", [wrap_numeric_selector](const Query& self, nb::object selector) {
            return self.sum(wrap_numeric_selector(std::move(selector)));
        }, "selector"_a = nb::none())
        .def("min", [wrap_numeric_selector](const Query& self, nb::object selector) {
            return self.min(wrap_numeric_selector(std::move(selector)));
        }, "selector"_a = nb::none())
        .def("max", [wrap_numeric_selector](const Query& self, nb::object selector) {
            return self.max(wrap_numeric_selector(std::move(selector)));
        }, "selector"_a = nb::none())
        .def("average", [wrap_numeric_selector](const Query& self, nb::object selector) {
            return self.average(wrap_numeric_selector(std::move(selector)));
        }, "selector"_a = nb::none())
        .def("group_by", [](const Query& self, nb::callable key_selector) {
            nb::dict groups;
            ConversionConfig cfg;
            self.consume([&](const json_value& item) -> bool {
                nb::object k = key_selector(item);
                if (nb::isinstance<json_value>(k)) {
                    k = to_python_optimized<false>(nb::cast<const json_value&>(k), cfg);
                }
                if (!groups.contains(k)) {
                    groups[k] = nb::list();
                }
                nb::cast<nb::list>(groups[k]).append(to_python_optimized<false>(item, cfg));
                return true;
            });
            return groups;
        }, "key_selector"_a)
        .def("for_each", [](const Query& self, nb::callable action) {
            self.for_each([action](const json_value& v) {
                nb::gil_scoped_acquire acquire;
                action(v);
            });
        }, "action"_a)
        .def("to_values", [](const Query& self) {
            nb::list out;
            self.consume([&out](const json_value& v) -> bool {
                out.append(nb::cast(v));
                return true;
            });
            return out;
        }, "Materialize query into a Python list of JSONValue objects")
        .def("to_list", [](const Query& self) {
            nb::list result;
            ConversionConfig config;
            self.consume([&result, &config](const json_value& v) -> bool {
                result.append(to_python_optimized<false>(v, config));
                return true;
            });
            return result;
        }, "Materialize query into a Python list of native Python objects")
        .def("__len__", [](const Query& self) { return self.count(); })
        .def("__getitem__", [](const Query& self, size_t idx) {
            const auto& vec = self.ensure_materialized();
            if (idx >= vec.size()) throw nb::index_error();
            return vec[idx];
        })
        .def("__iter__", [](const Query& self) {
            const auto& vec = self.ensure_materialized();
            return nb::make_iterator(nb::type<Query>(), "JSONQueryIterator", vec.begin(), vec.end());
        }, nb::keep_alive<0, 1>());

    m.def("query", [](const json_value& v) {
        if (!v.is_array()) throw nb::type_error("query() requires a JSON array");
        std::vector<json_value> data = v.as_array();
        return Query(std::move(data));
    }, "array"_a, "Create a lazy JSONQuery over a JSONValue array");

    m.def("query", [](nb::list py_list) {
        std::vector<json_value> data;
        data.reserve(py_list.size());
        for (nb::handle item : py_list) {
            data.push_back(py_to_json_value(item));
        }
        return Query(std::move(data));
    }, "items"_a, "Create a lazy JSONQuery over a Python list");

    // ========================================================================
    // Mustache Templating
    // ========================================================================
    m.def("render_template", [](const std::string& tmpl, const json_value& data) {
        return fastjson_parallel::mustache::render(tmpl, data);
    }, "tmpl"_a, "data"_a, "Render a Mustache template with JSONValue data",
       nb::call_guard<nb::gil_scoped_release>());

    m.def("render_template", [](const std::string& tmpl, nb::dict data) {
        json_value jv = py_to_json_value(data);
        nb::gil_scoped_release release;
        return fastjson_parallel::mustache::render(tmpl, jv);
    }, "tmpl"_a, "data"_a, "Render a Mustache template with a Python dict");

    // ========================================================================
    // SIMD Capabilities and Level Selection API
    // ========================================================================
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

    m.def("get_simd_capabilities", []() {
        return g_simd_caps;
    }, "Get the SIMD capabilities detected on this CPU");

    m.def("get_effective_simd_level", [](int requested_level) {
        SIMDLevel level = static_cast<SIMDLevel>(requested_level);
        return get_effective_simd_level(level);
    }, "requested_level"_a = 0,
       "Get the effective SIMD level that will be used (after waterfall fallback)");

    m.def("simd_level_name", [](int level) {
        return simd_level_to_string(static_cast<SIMDLevel>(level));
    }, "level"_a, "Get the name of a SIMD level");

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

    nb::list waterfall_order;
    waterfall_order.append("AVX-512 (512 bytes/iter, 8 registers)");
    waterfall_order.append("AMX (Advanced Matrix Extensions)");
    waterfall_order.append("AVX2 (128 bytes/iter, 4 registers)");
    waterfall_order.append("AVX (64 bytes/iter, 2 registers)");
    waterfall_order.append("SSE4.2 (32 bytes/iter)");
    waterfall_order.append("SSE2 (16 bytes/iter)");
    waterfall_order.append("SCALAR (1 byte/iter)");
    m.attr("SIMD_WATERFALL_ORDER") = waterfall_order;

    // ========================================================================
    // GPU Acceleration API
    // ========================================================================
    nb::enum_<fastjson::gpu::gpu_backend>(m, "GPUBackend")
        .value("NONE", fastjson::gpu::gpu_backend::none)
        .value("CUDA", fastjson::gpu::gpu_backend::cuda)
        .value("CUDA_GRAPH", fastjson::gpu::gpu_backend::cuda_graph)
        .value("CUBLAS", fastjson::gpu::gpu_backend::cublas)
        .value("CUTILE", fastjson::gpu::gpu_backend::cutile)
        .value("TRITON", fastjson::gpu::gpu_backend::triton)
        .value("ROCM", fastjson::gpu::gpu_backend::rocm)
        .value("SYCL", fastjson::gpu::gpu_backend::sycl)
        .value("AUTO_DETECT", fastjson::gpu::gpu_backend::auto_detect)
        .export_values();

    nb::class_<fastjson::gpu::gpu_info>(m, "GPUInfo")
        .def_ro("backend", &fastjson::gpu::gpu_info::backend)
        .def_ro("device_name", &fastjson::gpu::gpu_info::device_name)
        .def_ro("total_memory", &fastjson::gpu::gpu_info::total_memory)
        .def_ro("available_memory", &fastjson::gpu::gpu_info::available_memory)
        .def_ro("compute_units", &fastjson::gpu::gpu_info::compute_units)
        .def_ro("max_threads_per_block", &fastjson::gpu::gpu_info::max_threads_per_block)
        .def_ro("supports_concurrent_kernels", &fastjson::gpu::gpu_info::supports_concurrent_kernels);

    nb::class_<fastjson::gpu::gpu_parse_config>(m, "GPUParseConfig")
        .def(nb::init<>())
        .def_rw("backend", &fastjson::gpu::gpu_parse_config::backend)
        .def_rw("min_size_for_gpu", &fastjson::gpu::gpu_parse_config::min_size_for_gpu)
        .def_rw("block_size", &fastjson::gpu::gpu_parse_config::block_size)
        .def_rw("grid_size", &fastjson::gpu::gpu_parse_config::grid_size)
        .def_rw("async_execution", &fastjson::gpu::gpu_parse_config::async_execution)
        .def_rw("pin_host_memory", &fastjson::gpu::gpu_parse_config::pin_host_memory)
        .def_rw("use_cuda_graph", &fastjson::gpu::gpu_parse_config::use_cuda_graph);

    nb::class_<fastjson::gpu::gpu_parse_result>(m, "GPUParseResult")
        .def_ro("success", &fastjson::gpu::gpu_parse_result::success)
        .def_ro("error_message", &fastjson::gpu::gpu_parse_result::error_message)
        .def_ro("token_positions", &fastjson::gpu::gpu_parse_result::token_positions)
        .def_ro("token_types", &fastjson::gpu::gpu_parse_result::token_types)
        .def_ro("token_lengths", &fastjson::gpu::gpu_parse_result::token_lengths)
        .def_ro("transfer_to_gpu_ms", &fastjson::gpu::gpu_parse_result::transfer_to_gpu_ms)
        .def_ro("kernel_execution_ms", &fastjson::gpu::gpu_parse_result::kernel_execution_ms)
        .def_ro("transfer_from_gpu_ms", &fastjson::gpu::gpu_parse_result::transfer_from_gpu_ms)
        .def_ro("total_ms", &fastjson::gpu::gpu_parse_result::total_ms);

    m.def("detect_gpu_backend", &fastjson::gpu::detect_gpu_backend, "Detect available GPU backend");
    m.def("get_gpu_info", &fastjson::gpu::get_gpu_info,
          "backend"_a = fastjson::gpu::gpu_backend::auto_detect, "Get GPU device info");
    m.def("is_gpu_available", &fastjson::gpu::is_gpu_available, "Check if GPU is available");
    m.def("parse_on_gpu", &fastjson::gpu::parse_on_gpu,
          "input"_a, "config"_a = fastjson::gpu::gpu_parse_config{}, "Parse JSON on GPU");
    m.def("gpu_matrix_multiply",
          [](nb::ndarray<float, nb::c_contig> A, nb::ndarray<float, nb::c_contig> B, int backend_val) {
              if (A.ndim() != 2 || B.ndim() != 2) {
                  throw std::runtime_error("Matrices must be 2D");
              }
              const int M = static_cast<int>(A.shape(0));
              const int K = static_cast<int>(A.shape(1));
              const int K2 = static_cast<int>(B.shape(0));
              const int N = static_cast<int>(B.shape(1));
              if (K != K2) {
                  throw std::runtime_error("Inner dimensions must match");
              }

              std::vector<float> C_vec(static_cast<size_t>(M) * static_cast<size_t>(N));
              const auto backend = static_cast<fastjson::gpu::gpu_backend>(backend_val);

              bool ok = false;
              {
                  nb::gil_scoped_release release;
                  ok = fastjson::gpu::gpu_matrix_multiply(
                      static_cast<const float*>(A.data()),
                      static_cast<const float*>(B.data()),
                      C_vec.data(), M, N, K, backend);
              }
              if (!ok) {
                  throw std::runtime_error("GPU matrix multiplication failed");
              }

              nb::list result;
              for (int i = 0; i < M; ++i) {
                  nb::list row;
                  for (int j = 0; j < N; ++j) {
                      row.append(C_vec[static_cast<size_t>(i) * static_cast<size_t>(N) + static_cast<size_t>(j)]);
                  }
                  result.append(row);
              }
              return result;
          },
          "A"_a, "B"_a, "backend"_a = static_cast<int>(fastjson::gpu::gpu_backend::cublas),
          "Multiply matrices on GPU");

    m.def("gpu_launch_triton_ptx",
          [](const std::string& ptx_code, const std::string& kernel_name, nb::list args_list,
             int grid_size, int block_size) -> bool {
              std::vector<std::variant<uint64_t, int64_t, double, float, uint32_t, int32_t>> storage;
              storage.reserve(args_list.size());

              for (size_t i = 0; i < args_list.size(); ++i) {
                  nb::handle item = args_list[i];
                  if (nb::isinstance<nb::int_>(item)) {
                      storage.push_back(nb::cast<int64_t>(item));
                  } else if (nb::isinstance<nb::float_>(item)) {
                      storage.push_back(static_cast<float>(nb::cast<double>(item)));
                  } else if (nb::hasattr(item, "data_ptr")) {
                      storage.push_back(nb::cast<uint64_t>(item.attr("data_ptr")()));
                  } else {
                      throw nb::type_error("Unsupported argument type in Triton kernel launch");
                  }
              }

              std::vector<void*> arg_ptrs;
              arg_ptrs.reserve(storage.size());
              for (size_t i = 0; i < storage.size(); ++i) {
                  arg_ptrs.push_back(std::visit(
                      [](auto& v) -> void* { return const_cast<void*>(static_cast<const void*>(&v)); },
                      storage[i]));
              }

              bool ok = false;
              {
                  nb::gil_scoped_release release;
                  ok = fastjson::gpu::gpu_launch_triton_ptx(
                      ptx_code.c_str(), kernel_name.c_str(), arg_ptrs.data(), grid_size, block_size);
              }
              return ok;
          },
          "ptx_code"_a, "kernel_name"_a, "args"_a, "grid_size"_a, "block_size"_a,
          "Launch Triton JIT PTX code dynamically");
}
