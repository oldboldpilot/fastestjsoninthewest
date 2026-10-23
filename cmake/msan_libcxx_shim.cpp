// MemorySanitizer shim for prebuilt (uninstrumented) libc++.so.1.
// Compiled only when -DENABLE_MSAN=ON. Instantiates stream/string templates
// with MSan instrumentation and unpoisons outputs of non-template ABI calls
// into libc++.so.1 so MSan can verify all project and test code without
// false positives from the uninstrumented system libc++ shared library.

#if defined(__has_feature)
#  if __has_feature(memory_sanitizer)
#    define FJ_HAS_MSAN 1
#  endif
#endif

#ifdef FJ_HAS_MSAN

#include <sanitizer/msan_interface.h>
#include <dlfcn.h>
#include <charconv>
#include <ios>
#include <locale>
#include <ostream>
#include <streambuf>
#include <string>

namespace std { inline namespace __1 {

// Explicitly instantiate stream and string templates with MSan instrumentation
// (excluding basic_ostream<char>::put(char), which is defined below via __asm__
// to route through write(&c, 1) and avoid __stdoutbuf<char>::overflow's uninstrumented stack local).
template basic_ostream<char>::sentry::sentry(basic_ostream<char>&);
template basic_ostream<char>::sentry::~sentry();
template basic_ostream<char>& basic_ostream<char>::flush();
template basic_ostream<char>& basic_ostream<char>::write(const char*, streamsize);
template basic_ostream<char>& basic_ostream<char>::operator<<(bool);
template basic_ostream<char>& basic_ostream<char>::operator<<(short);
template basic_ostream<char>& basic_ostream<char>::operator<<(unsigned short);
template basic_ostream<char>& basic_ostream<char>::operator<<(int);
template basic_ostream<char>& basic_ostream<char>::operator<<(unsigned int);
template basic_ostream<char>& basic_ostream<char>::operator<<(long);
template basic_ostream<char>& basic_ostream<char>::operator<<(unsigned long);
template basic_ostream<char>& basic_ostream<char>::operator<<(long long);
template basic_ostream<char>& basic_ostream<char>::operator<<(unsigned long long);
template basic_ostream<char>& basic_ostream<char>::operator<<(float);
template basic_ostream<char>& basic_ostream<char>::operator<<(double);
template basic_ostream<char>& basic_ostream<char>::operator<<(long double);
template basic_ostream<char>& basic_ostream<char>::operator<<(const void*);
template basic_ostream<char>& basic_ostream<char>::operator<<(basic_streambuf<char>*);
template class basic_streambuf<char>;
template class basic_ios<char>;
template class num_put<char, ostreambuf_iterator<char, char_traits<char>>>;
template ostreambuf_iterator<char, char_traits<char>> __pad_and_output<char, char_traits<char>>(
    ostreambuf_iterator<char, char_traits<char>>, const char*, const char*, const char*, ios_base&, char);
template basic_string<char> operator+<char, char_traits<char>, allocator<char>>(
    const char*, const basic_string<char>&);

// Wrap floating-point std::to_chars overloads in libc++.so.1 and unpoison written characters.
#define FJ_WRAP_TO_CHARS_1(T, MANGLED) \
    to_chars_result to_chars(char* first, char* last, T value) { \
        using fn_t = to_chars_result(*)(char*, char*, T); \
        static const auto real_fn = reinterpret_cast<fn_t>(dlsym(RTLD_NEXT, MANGLED)); \
        auto res = real_fn(first, last, value); \
        __msan_unpoison(&res, sizeof(res)); \
        if (res.ec == errc{} && res.ptr > first) __msan_unpoison(first, static_cast<size_t>(res.ptr - first)); \
        return res; \
    }
#define FJ_WRAP_TO_CHARS_2(T, MANGLED) \
    to_chars_result to_chars(char* first, char* last, T value, chars_format fmt) { \
        using fn_t = to_chars_result(*)(char*, char*, T, chars_format); \
        static const auto real_fn = reinterpret_cast<fn_t>(dlsym(RTLD_NEXT, MANGLED)); \
        auto res = real_fn(first, last, value, fmt); \
        __msan_unpoison(&res, sizeof(res)); \
        if (res.ec == errc{} && res.ptr > first) __msan_unpoison(first, static_cast<size_t>(res.ptr - first)); \
        return res; \
    }
#define FJ_WRAP_TO_CHARS_3(T, MANGLED) \
    to_chars_result to_chars(char* first, char* last, T value, chars_format fmt, int prec) { \
        using fn_t = to_chars_result(*)(char*, char*, T, chars_format, int); \
        static const auto real_fn = reinterpret_cast<fn_t>(dlsym(RTLD_NEXT, MANGLED)); \
        auto res = real_fn(first, last, value, fmt, prec); \
        __msan_unpoison(&res, sizeof(res)); \
        if (res.ec == errc{} && res.ptr > first) __msan_unpoison(first, static_cast<size_t>(res.ptr - first)); \
        return res; \
    }

FJ_WRAP_TO_CHARS_1(float,       "_ZNSt3__18to_charsEPcS0_f")
FJ_WRAP_TO_CHARS_2(float,       "_ZNSt3__18to_charsEPcS0_fNS_12chars_formatE")
FJ_WRAP_TO_CHARS_3(float,       "_ZNSt3__18to_charsEPcS0_fNS_12chars_formatEi")
FJ_WRAP_TO_CHARS_1(double,      "_ZNSt3__18to_charsEPcS0_d")
FJ_WRAP_TO_CHARS_2(double,      "_ZNSt3__18to_charsEPcS0_dNS_12chars_formatE")
FJ_WRAP_TO_CHARS_3(double,      "_ZNSt3__18to_charsEPcS0_dNS_12chars_formatEi")
FJ_WRAP_TO_CHARS_1(long double, "_ZNSt3__18to_charsEPcS0_e")
FJ_WRAP_TO_CHARS_2(long double, "_ZNSt3__18to_charsEPcS0_eNS_12chars_formatE")
FJ_WRAP_TO_CHARS_3(long double, "_ZNSt3__18to_charsEPcS0_eNS_12chars_formatEi")

// MSan-instrumented std::to_string overloads using header-only integer to_chars and wrapped float to_chars.
namespace {
template <typename IntT>
inline string fj_int_to_string(IntT v) {
    char b[32];
    auto r = to_chars(b, b + sizeof(b), v);
    return (r.ec == errc{}) ? string(b, static_cast<size_t>(r.ptr - b)) : string{};
}
template <typename FloatT>
inline string fj_float_to_string(FloatT v) {
    char b[128];
    auto r = to_chars(b, b + sizeof(b), v, chars_format::fixed, 6);
    return (r.ec == errc{}) ? string(b, static_cast<size_t>(r.ptr - b)) : string{};
}
} // namespace

string to_string(int v)                { return fj_int_to_string(v); }
string to_string(long v)               { return fj_int_to_string(v); }
string to_string(long long v)          { return fj_int_to_string(v); }
string to_string(unsigned v)           { return fj_int_to_string(v); }
string to_string(unsigned long v)      { return fj_int_to_string(v); }
string to_string(unsigned long long v) { return fj_int_to_string(v); }
string to_string(float v)              { return fj_float_to_string(v); }
string to_string(double v)             { return fj_float_to_string(v); }
string to_string(long double v)        { return fj_float_to_string(v); }

// Wrap ios_base::getloc() so returned locale on caller stack is unpoisoned.
locale ios_base::getloc() const {
    using fn_t = void(*)(locale*, const ios_base*);
    static const auto real_fn = reinterpret_cast<fn_t>(dlsym(RTLD_NEXT, "_ZNKSt3__18ios_base6getlocEv"));
    alignas(locale) unsigned char buf[sizeof(locale)];
    real_fn(reinterpret_cast<locale*>(buf), this);
    __msan_unpoison(buf, sizeof(buf));
    locale out = *reinterpret_cast<locale*>(buf);
    reinterpret_cast<locale*>(buf)->~locale();
    return out;
}

} } // namespace std::__1

// Route basic_ostream<char>::put(char) through write(&c, 1) so single-character writes
// (such as std::endl) pass an MSan-initialized buffer directly to xsputn/fwrite instead of
// spilling an uninstrumented stack local inside libc++.so.1's __stdoutbuf<char>::overflow(int).
std::__1::basic_ostream<char>&
fj_msan_ostream_put(std::__1::basic_ostream<char>* self, char c)
    __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEE3putEc");

std::__1::basic_ostream<char>&
fj_msan_ostream_put(std::__1::basic_ostream<char>* self, char c) {
    return self->write(&c, 1);
}

// Wrap __num_put<char>::__widen_and_group_int / __widen_and_group_float so output buffers
// and pointers written by libc++.so.1 are unpoisoned before __pad_and_output / fwrite.
void fj_msan_widen_and_group_int(
    char* nb, char* np, char* ne, char* ob, char*& op, char*& oe, const std::__1::locale& loc)
    __asm__("_ZNSt3__19__num_putIcE21__widen_and_group_intEPcS2_S2_S2_RS2_S3_RKNS_6localeE");

void fj_msan_widen_and_group_int(
    char* nb, char* np, char* ne, char* ob, char*& op, char*& oe, const std::__1::locale& loc) {
    using fn_t = void(*)(char*, char*, char*, char*, char*&, char*&, const std::__1::locale&);
    static const auto real_fn = reinterpret_cast<fn_t>(
        dlsym(RTLD_NEXT, "_ZNSt3__19__num_putIcE21__widen_and_group_intEPcS2_S2_S2_RS2_S3_RKNS_6localeE"));
    real_fn(nb, np, ne, ob, op, oe, loc);
    __msan_unpoison(&op, sizeof(op));
    __msan_unpoison(&oe, sizeof(oe));
    if (oe > ob) __msan_unpoison(ob, static_cast<size_t>(oe - ob));
}

void fj_msan_widen_and_group_float(
    char* nb, char* np, char* ne, char* ob, char*& op, char*& oe, const std::__1::locale& loc)
    __asm__("_ZNSt3__19__num_putIcE23__widen_and_group_floatEPcS2_S2_S2_RS2_S3_RKNS_6localeE");

void fj_msan_widen_and_group_float(
    char* nb, char* np, char* ne, char* ob, char*& op, char*& oe, const std::__1::locale& loc) {
    using fn_t = void(*)(char*, char*, char*, char*, char*&, char*&, const std::__1::locale&);
    static const auto real_fn = reinterpret_cast<fn_t>(
        dlsym(RTLD_NEXT, "_ZNSt3__19__num_putIcE23__widen_and_group_floatEPcS2_S2_S2_RS2_S3_RKNS_6localeE"));
    real_fn(nb, np, ne, ob, op, oe, loc);
    __msan_unpoison(&op, sizeof(op));
    __msan_unpoison(&oe, sizeof(oe));
    if (oe > ob) __msan_unpoison(ob, static_cast<size_t>(oe - ob));
}

// Intercept __from_chars_floating_point<double>/<float> via mangled symbol names
// (avoids explicit-specialization-after-instantiation conflict with <charconv> extern template).
std::__1::__from_chars_result<double>
fj_msan_from_chars_double(const char* first, const char* last, std::__1::chars_format fmt)
    __asm__("_ZNSt3__127__from_chars_floating_pointIdEENS_19__from_chars_resultIT_EEPKcS5_NS_12chars_formatE");

std::__1::__from_chars_result<double>
fj_msan_from_chars_double(const char* first, const char* last, std::__1::chars_format fmt) {
    using fn_t = std::__1::__from_chars_result<double>(*)(const char*, const char*, std::__1::chars_format);
    static const auto real_fn = reinterpret_cast<fn_t>(
        dlsym(RTLD_NEXT, "_ZNSt3__127__from_chars_floating_pointIdEENS_19__from_chars_resultIT_EEPKcS5_NS_12chars_formatE"));
    __msan_check_mem_is_initialized(first, static_cast<size_t>(last - first));
    auto res = real_fn(first, last, fmt);
    __msan_unpoison(&res, sizeof(res));
    return res;
}

std::__1::__from_chars_result<float>
fj_msan_from_chars_float(const char* first, const char* last, std::__1::chars_format fmt)
    __asm__("_ZNSt3__127__from_chars_floating_pointIfEENS_19__from_chars_resultIT_EEPKcS5_NS_12chars_formatE");

std::__1::__from_chars_result<float>
fj_msan_from_chars_float(const char* first, const char* last, std::__1::chars_format fmt) {
    using fn_t = std::__1::__from_chars_result<float>(*)(const char*, const char*, std::__1::chars_format);
    static const auto real_fn = reinterpret_cast<fn_t>(
        dlsym(RTLD_NEXT, "_ZNSt3__127__from_chars_floating_pointIfEENS_19__from_chars_resultIT_EEPKcS5_NS_12chars_formatE"));
    __msan_check_mem_is_initialized(first, static_cast<size_t>(last - first));
    auto res = real_fn(first, last, fmt);
    __msan_unpoison(&res, sizeof(res));
    return res;
}

#endif // FJ_HAS_MSAN
