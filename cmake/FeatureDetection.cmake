# Feature Detection for FastestJSONInTheWest
include(CheckCXXSourceCompiles)
include(CheckIncludeFileCXX)

# =============================================================================
# Function: detect_simd_features
# Detects available SIMD instruction sets
# =============================================================================

function(detect_simd_features)
    message(STATUS "Detecting SIMD capabilities...")
    
    # Test for SSE2 support (baseline for x86_64)
    check_cxx_source_compiles("
        #include <emmintrin.h>
        int main() {
            __m128i v = _mm_setzero_si128();
            return _mm_cvtsi128_si32(v);
        }" FASTJSON_HAVE_SSE2)
    
    if(FASTJSON_HAVE_SSE2)
        message(STATUS "  SSE2: Available")
    endif()
    
    # Test for SSE4.1 support
    check_cxx_source_compiles("
        #include <smmintrin.h>
        int main() {
            __m128i v = _mm_setzero_si128();
            return _mm_testz_si128(v, v);
        }" FASTJSON_HAVE_SSE4_1)
    
    if(FASTJSON_HAVE_SSE4_1)
        message(STATUS "  SSE4.1: Available")
    endif()
    
    # Test for AVX support
    check_cxx_source_compiles("
        #include <immintrin.h>
        int main() {
            __m256 v = _mm256_setzero_ps();
            return _mm256_testz_ps(v, v);
        }" FASTJSON_HAVE_AVX)
    
    if(FASTJSON_HAVE_AVX)
        message(STATUS "  AVX: Available")
    endif()
    
    # Test for AVX2 support
    check_cxx_source_compiles("
        #include <immintrin.h>
        int main() {
            __m256i v = _mm256_setzero_si256();
            return _mm256_testz_si256(v, v);
        }" FASTJSON_HAVE_AVX2)
    
    if(FASTJSON_HAVE_AVX2)
        message(STATUS "  AVX2: Available")
    endif()
    
    # Test for AVX-512 support
    check_cxx_source_compiles("
        #include <immintrin.h>
        int main() {
            __m512i v = _mm512_setzero_si512();
            return _mm512_test_epi32_mask(v, v);
        }" FASTJSON_HAVE_AVX512)
    
    if(FASTJSON_HAVE_AVX512)
        message(STATUS "  AVX-512: Available")
    endif()
    
    # Test for NEON support (ARM)
    check_cxx_source_compiles("
        #include <arm_neon.h>
        int main() {
            uint8x16_t v = vdupq_n_u8(0);
            return vgetq_lane_u8(v, 0);
        }" FASTJSON_HAVE_NEON)
    
    if(FASTJSON_HAVE_NEON)
        message(STATUS "  NEON: Available")
    endif()
    
    # Test for SVE support (ARM)
    check_cxx_source_compiles("
        #include <arm_sve.h>
        int main() {
            svbool_t p = svptrue_b8();
            return svptest_any(p, p);
        }" FASTJSON_HAVE_SVE)
    
    if(FASTJSON_HAVE_SVE)
        message(STATUS "  SVE: Available")
    endif()
    
    # Export results to parent scope
    set(FASTJSON_HAVE_SSE2 ${FASTJSON_HAVE_SSE2} PARENT_SCOPE)
    set(FASTJSON_HAVE_SSE4_1 ${FASTJSON_HAVE_SSE4_1} PARENT_SCOPE)
    set(FASTJSON_HAVE_AVX ${FASTJSON_HAVE_AVX} PARENT_SCOPE)
    set(FASTJSON_HAVE_AVX2 ${FASTJSON_HAVE_AVX2} PARENT_SCOPE)
    set(FASTJSON_HAVE_AVX512 ${FASTJSON_HAVE_AVX512} PARENT_SCOPE)
    set(FASTJSON_HAVE_NEON ${FASTJSON_HAVE_NEON} PARENT_SCOPE)
    set(FASTJSON_HAVE_SVE ${FASTJSON_HAVE_SVE} PARENT_SCOPE)
endfunction()

# =============================================================================
# Function: detect_cxx23_features
# Detects availability of required C++23 features
# =============================================================================

function(detect_cxx23_features)
    message(STATUS "Detecting C++23 features...")
    
    # Test for std::expected
    check_cxx_source_compiles("
        #include <expected>
        int main() {
            std::expected<int, std::string> e = 42;
            return e.has_value() ? 0 : 1;
        }" FASTJSON_HAVE_EXPECTED)
    
    if(NOT FASTJSON_HAVE_EXPECTED)
        message(FATAL_ERROR "std::expected not available - C++23 compiler required")
    endif()
    
    # Test for std::span
    check_cxx_source_compiles("
        #include <span>
        #include <array>
        int main() {
            std::array<int, 5> arr{};
            std::span<int> s{arr};
            return s.size() > 0 ? 0 : 1;
        }" FASTJSON_HAVE_SPAN)
    
    if(NOT FASTJSON_HAVE_SPAN)
        message(FATAL_ERROR "std::span not available - C++20 or later required")
    endif()
    
    # Test for ranges
    check_cxx_source_compiles("
        #include <ranges>
        #include <vector>
        int main() {
            std::vector<int> v{1, 2, 3, 4, 5};
            auto r = v | std::views::take(3);
            return std::ranges::distance(r);
        }" FASTJSON_HAVE_RANGES)
    
    if(NOT FASTJSON_HAVE_RANGES)
        message(FATAL_ERROR "std::ranges not available - C++20 or later required")
    endif()
    
    # Test for concepts
    check_cxx_source_compiles("
        #include <concepts>
        template<typename T>
        concept Integral = std::integral<T>;
        template<Integral T>
        T add(T a, T b) { return a + b; }
        int main() {
            return add(1, 2);
        }" FASTJSON_HAVE_CONCEPTS)
    
    if(NOT FASTJSON_HAVE_CONCEPTS)
        message(FATAL_ERROR "Concepts not available - C++20 or later required")
    endif()
    
    # Test for std::format (C++20, but may not be available on all compilers)
    check_cxx_source_compiles("
        #include <format>
        #include <string>
        int main() {
            std::string s = std::format(\"Hello, {}!\", \"World\");
            return s.empty() ? 1 : 0;
        }" FASTJSON_HAVE_FORMAT)
    
    if(FASTJSON_HAVE_FORMAT)
        message(STATUS "  std::format: Available")
    else()
        message(STATUS "  std::format: Not available (will use alternative)")
    endif()
    
    # Test for source_location
    check_cxx_source_compiles("
        #include <source_location>
        int main() {
            auto loc = std::source_location::current();
            return loc.line() > 0 ? 0 : 1;
        }" FASTJSON_HAVE_SOURCE_LOCATION)
    
    if(FASTJSON_HAVE_SOURCE_LOCATION)
        message(STATUS "  std::source_location: Available")
    else()
        message(STATUS "  std::source_location: Not available (will use alternative)")
    endif()
    
    # Test for coroutines
    check_cxx_source_compiles("
        #include <coroutine>
        struct task {
            struct promise_type {
                task get_return_object() { return {}; }
                std::suspend_never initial_suspend() { return {}; }
                std::suspend_never final_suspend() noexcept { return {}; }
                void return_void() {}
                void unhandled_exception() {}
            };
        };
        task example() { co_return; }
        int main() { return 0; }
    " FASTJSON_HAVE_COROUTINES)
    
    if(FASTJSON_HAVE_COROUTINES)
        message(STATUS "  Coroutines: Available")
    else()
        message(STATUS "  Coroutines: Not available")
    endif()
    
    # Export results to parent scope
    set(FASTJSON_HAVE_EXPECTED ${FASTJSON_HAVE_EXPECTED} PARENT_SCOPE)
    set(FASTJSON_HAVE_SPAN ${FASTJSON_HAVE_SPAN} PARENT_SCOPE)
    set(FASTJSON_HAVE_RANGES ${FASTJSON_HAVE_RANGES} PARENT_SCOPE)
    set(FASTJSON_HAVE_CONCEPTS ${FASTJSON_HAVE_CONCEPTS} PARENT_SCOPE)
    set(FASTJSON_HAVE_FORMAT ${FASTJSON_HAVE_FORMAT} PARENT_SCOPE)
    set(FASTJSON_HAVE_SOURCE_LOCATION ${FASTJSON_HAVE_SOURCE_LOCATION} PARENT_SCOPE)
    set(FASTJSON_HAVE_COROUTINES ${FASTJSON_HAVE_COROUTINES} PARENT_SCOPE)
endfunction()

# =============================================================================
# Function: detect_platform_features
# Detects platform-specific capabilities
# =============================================================================

function(detect_platform_features)
    message(STATUS "Detecting platform features...")
    
    # Test for memory mapping support
    if(WIN32)
        check_cxx_source_compiles("
            #include <windows.h>
            int main() {
                HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, NULL);
                if (h) CloseHandle(h);
                return 0;
            }" FASTJSON_HAVE_MMAP)
    else()
        check_cxx_source_compiles("
            #include <sys/mman.h>
            #include <unistd.h>
            int main() {
                void* ptr = mmap(nullptr, 1024, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (ptr != MAP_FAILED) munmap(ptr, 1024);
                return 0;
            }" FASTJSON_HAVE_MMAP)
    endif()
    
    if(FASTJSON_HAVE_MMAP)
        message(STATUS "  Memory mapping: Available")
    endif()
    
    # Test for large file support
    check_cxx_source_compiles("
        #include <cstdint>
        #include <sys/types.h>
        int main() {
            off_t offset = static_cast<off_t>(1ULL << 32);
            return sizeof(offset) >= 8 ? 0 : 1;
        }" FASTJSON_HAVE_LARGE_FILE_SUPPORT)
    
    if(FASTJSON_HAVE_LARGE_FILE_SUPPORT)
        message(STATUS "  Large file support: Available")
    endif()
    
    # Test for thread-local storage
    check_cxx_source_compiles("
        thread_local int tls_var = 42;
        int main() {
            return tls_var;
        }" FASTJSON_HAVE_THREAD_LOCAL)
    
    if(FASTJSON_HAVE_THREAD_LOCAL)
        message(STATUS "  Thread-local storage: Available")
    endif()
    
    # Export results to parent scope
    set(FASTJSON_HAVE_MMAP ${FASTJSON_HAVE_MMAP} PARENT_SCOPE)
    set(FASTJSON_HAVE_LARGE_FILE_SUPPORT ${FASTJSON_HAVE_LARGE_FILE_SUPPORT} PARENT_SCOPE)
    set(FASTJSON_HAVE_THREAD_LOCAL ${FASTJSON_HAVE_THREAD_LOCAL} PARENT_SCOPE)
endfunction()

# =============================================================================
# Function: detect_compiler_optimizations
# Detects available compiler optimization features
# =============================================================================

function(detect_compiler_optimizations)
    message(STATUS "Detecting compiler optimizations...")
    
    # Test for link-time optimization
    check_cxx_source_compiles("
        int add(int a, int b) { return a + b; }
        int main() { return add(1, 2); }
    " FASTJSON_HAVE_LTO)
    
    if(FASTJSON_HAVE_LTO)
        message(STATUS "  Link-time optimization: Available")
    endif()
    
    # Test for profile-guided optimization support
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(FASTJSON_HAVE_PGO TRUE)
        message(STATUS "  Profile-guided optimization: Available")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(FASTJSON_HAVE_PGO TRUE)
        message(STATUS "  Profile-guided optimization: Available")
    else()
        set(FASTJSON_HAVE_PGO FALSE)
    endif()
    
    # Test for function multiversioning (GCC/Clang feature)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        check_cxx_source_compiles("
            __attribute__((target(\"default\")))
            int process_data() { return 1; }
            
            __attribute__((target(\"avx2\")))
            int process_data() { return 2; }
            
            int main() { return process_data(); }
        " FASTJSON_HAVE_FUNCTION_MULTIVERSIONING)
        
        if(FASTJSON_HAVE_FUNCTION_MULTIVERSIONING)
            message(STATUS "  Function multiversioning: Available")
        endif()
    endif()
    
    # Export results to parent scope
    set(FASTJSON_HAVE_LTO ${FASTJSON_HAVE_LTO} PARENT_SCOPE)
    set(FASTJSON_HAVE_PGO ${FASTJSON_HAVE_PGO} PARENT_SCOPE)
    set(FASTJSON_HAVE_FUNCTION_MULTIVERSIONING ${FASTJSON_HAVE_FUNCTION_MULTIVERSIONING} PARENT_SCOPE)
endfunction()

# =============================================================================
# Main detection functions - called from main CMakeLists.txt
# =============================================================================

macro(run_all_feature_detection)
    detect_simd_features()
    detect_cxx23_features()
    detect_platform_features()
    detect_compiler_optimizations()
endmacro()