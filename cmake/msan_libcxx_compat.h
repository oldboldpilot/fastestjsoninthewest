// MemorySanitizer compatibility header for prebuilt (uninstrumented) libc++.so.
// Disables extern template declarations for std::basic_string so Clang
// instantiates and MSan-instruments all std::string methods inline.
#ifndef FASTJSON_MSAN_LIBCXX_COMPAT_H
#define FASTJSON_MSAN_LIBCXX_COMPAT_H

#define _LIBCPP___STRING_EXTERN_TEMPLATE_LISTS_H
#define _LIBCPP_STRING_COMMON_EXTERN_TEMPLATE_LIST(Func, CharT)
#define _LIBCPP_STRING_V1_EXTERN_TEMPLATE_LIST(Func, CharT)
#define _LIBCPP_STRING_UNSTABLE_EXTERN_TEMPLATE_LIST(Func, CharT)

#endif // FASTJSON_MSAN_LIBCXX_COMPAT_H
