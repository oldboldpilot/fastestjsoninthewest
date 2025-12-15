# FastestJSONInTheWest C++23 Modules CMakeLists.txt
# Author: Olumuyiwa Oluwasanmi
# High-Performance JSON Library with C++23 Modules and Trail Calling Syntax

cmake_minimum_required(VERSION 3.28)

# ============================================================================
# Toolchain Configuration (before project())
# ============================================================================
# Configurable toolchain paths with fallback detection
# Can be overridden via: cmake -DCLANG_DEFAULT_PATH=/custom/path ...
# Or via environment: CC=/custom/clang CXX=/custom/clang++ cmake ...

# Only configure compiler paths when building standalone (not as submodule)
# and when the toolchain exists
if(NOT CMAKE_CXX_COMPILER AND EXISTS "/opt/clang-21/bin/clang++")
    # Default toolchain paths (can be overridden)
    set(CLANG_DEFAULT_PATH "/opt/clang-21/bin" CACHE STRING "Default path for Clang binaries")
    set(LIBCXX_DEFAULT_INCLUDE "/opt/clang-21/include/c++/v1" CACHE STRING "Default path for libc++ headers")
    set(LIBCXX_DEFAULT_LIB "/opt/clang-21/lib" CACHE STRING "Default path for libc++ libraries")

    # Set compilers using the configurable paths
    set(CMAKE_CXX_COMPILER "${CLANG_DEFAULT_PATH}/clang++")
    set(CMAKE_C_COMPILER "${CLANG_DEFAULT_PATH}/clang")
endif()

project(FastestJSONInTheWest
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "High-Performance C++23 JSON Library with Modules"
)

# ============================================================================
# Project Options
# ============================================================================

option(USE_FETCHCONTENT_LOGGER "Use FetchContent to fetch cpp23-logger instead of submodule" OFF)
option(ENABLE_NATIVE_OPTIMIZATIONS "Enable native CPU optimizations (AVX-512, AVX2, etc) to match parent project" OFF)
option(ENABLE_OPENMP "Enable OpenMP parallel processing support (legacy)" OFF)
option(ENABLE_PARALLEL_STL "Enable Parallel STL with Intel TBB (recommended for Clang)" ON)

# Parent project CPU feature overrides (for WSL2/VM compatibility)
# These allow parent projects to override auto-detection
option(FASTJSON_FORCE_AVX512 "Force AVX-512 on/off (overrides detection)" "")
option(FASTJSON_FORCE_AMX "Force AMX on/off (overrides detection)" "")

# ============================================================================
# Compiler Verification
# ============================================================================

# Verify we're using the correct compiler
execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} --version
    OUTPUT_VARIABLE COMPILER_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX MATCH "clang version ([0-9]+\.[0-9]+\.[0-9]+)" CLANG_VERSION_MATCH "${COMPILER_VERSION}")
if(CLANG_VERSION_MATCH)
    message(STATUS "✓ Using Clang ${CMAKE_MATCH_1} (Required: 21.x)")
else()
    message(FATAL_ERROR "❌ Failed to detect Clang version from ${CMAKE_CXX_COMPILER}")
endif()

message(STATUS "Using Clang 21 from source: ${CMAKE_CXX_COMPILER}")

# Set C++23 standard with modules support
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required libraries
find_package(Threads REQUIRED)

# Optional OpenMP support for parallel JSON parsing (legacy)
if(ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "Enabling OpenMP support for parallel JSON parsing (legacy)")
    else()
        message(WARNING "OpenMP requested but not found")
    endif()
endif()

# ============================================================================
# Intel TBB for Parallel STL Support (Clang + libc++)
# ============================================================================
# Clang with libc++ requires TBB to enable std::execution parallel policies
# We use FetchContent to get oneTBB (Intel's modern TBB implementation)

if(ENABLE_PARALLEL_STL)
    message(STATUS "Enabling Parallel STL with Intel TBB...")

    include(FetchContent)

    # Fetch Intel oneTBB (latest stable version)
    FetchContent_Declare(
        TBB
        GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
        GIT_TAG        v2022.3.0  # Latest stable release
        GIT_SHALLOW    TRUE
    )

    # Configure TBB options
    set(TBB_TEST OFF CACHE BOOL "Don't build TBB tests" FORCE)
    set(TBB_EXAMPLES OFF CACHE BOOL "Don't build TBB examples" FORCE)
    set(TBB_STRICT OFF CACHE BOOL "Disable strict warnings" FORCE)

    # Make TBB available
    FetchContent_MakeAvailable(TBB)

    # Export TBB configuration for parent projects
    # NOTE: Unlike std.pcm, TBB doesn't require flag matching for binary compatibility
    # because it's a regular shared library with a stable ABI. However, we export
    # these variables so parent projects can reuse the same TBB instance.
    get_target_property(TBB_INCLUDE_DIRS TBB::tbb INTERFACE_INCLUDE_DIRECTORIES)

    set(FASTJSON_TBB_VERSION "v2022.3.0" CACHE STRING "TBB version used by fastjson" FORCE)
    set(FASTJSON_TBB_TARGET "TBB::tbb" CACHE STRING "TBB target name" FORCE)
    set(FASTJSON_TBB_INCLUDE_DIRS "${TBB_INCLUDE_DIRS}" CACHE STRING "TBB include directories" FORCE)
    set(FASTJSON_HAS_PARALLEL_STL ON CACHE BOOL "fastjson has Parallel STL support" FORCE)

    mark_as_advanced(FASTJSON_TBB_VERSION FASTJSON_TBB_TARGET FASTJSON_TBB_INCLUDE_DIRS FASTJSON_HAS_PARALLEL_STL)

    message(STATUS "✓ Intel TBB fetched and configured")
    message(STATUS "  TBB version: 2022.3.0")
    message(STATUS "  TBB target: TBB::tbb (available to parent projects)")
    message(STATUS "  Parallel STL execution policies available: std::execution::par, par_unseq")
else()
    set(FASTJSON_HAS_PARALLEL_STL OFF CACHE BOOL "fastjson has Parallel STL support" FORCE)
endif()

# Add Clang library paths using configurable variables
link_directories(
    ${LIBCXX_DEFAULT_LIB}
    ${LIBCXX_DEFAULT_LIB}/x86_64-unknown-linux-gnu
)

# ============================================================================
# Integrate cpp23-logger dependency
# ============================================================================

# Pass toolchain configuration to logger submodule
# These ensure the submodule uses the same compiler and library paths
set(CLANG_DEFAULT_PATH "${CLANG_DEFAULT_PATH}" CACHE STRING "Clang binary path for submodules" FORCE)
set(LIBCXX_DEFAULT_INCLUDE "${LIBCXX_DEFAULT_INCLUDE}" CACHE STRING "libc++ include path for submodules" FORCE)
set(LIBCXX_DEFAULT_LIB "${LIBCXX_DEFAULT_LIB}" CACHE STRING "libc++ library path for submodules" FORCE)
set(CLANG_TIDY_SEARCH_PATHS "${CLANG_DEFAULT_PATH}" CACHE STRING "clang-tidy search paths for submodules" FORCE)

if(USE_FETCHCONTENT_LOGGER)
    message(STATUS "Using FetchContent to fetch cpp23-logger")
    include(FetchContent)
    FetchContent_Declare(
        cpp23_logger
        GIT_REPOSITORY https://github.com/oldboldpilot/cpp23-logger.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(cpp23_logger)
else()
    message(STATUS "Using cpp23-logger from submodule")
    # Guard against duplicate inclusion (in case parent project already includes logger)
    if(NOT TARGET logger)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cpp23-logger/CMakeLists.txt")
            # Disable tests and examples for cpp23-logger (directories may not exist in sync)
            set(BUILD_TESTS_LOGGER_BACKUP ${BUILD_TESTS})
            set(BUILD_EXAMPLES_LOGGER_BACKUP ${BUILD_EXAMPLES})
            set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            add_subdirectory(external/cpp23-logger EXCLUDE_FROM_ALL)
            set(BUILD_TESTS ${BUILD_TESTS_LOGGER_BACKUP} CACHE BOOL "" FORCE)
            set(BUILD_EXAMPLES ${BUILD_EXAMPLES_LOGGER_BACKUP} CACHE BOOL "" FORCE)
        else()
            message(FATAL_ERROR "cpp23-logger submodule not found. Run: git submodule update --init --recursive")
        endif()
    else()
        message(STATUS "  cpp23-logger already included by parent project, skipping")
    endif()
endif()

# Find clang-tidy using configurable path
find_program(CLANG_TIDY_EXE NAMES clang-tidy-21 clang-tidy
    PATHS ${CLANG_DEFAULT_PATH} /usr/local/bin /usr/bin
    DOC "Path to clang-tidy executable")

# ============================================================================
# Build C++23 std modules from locally-built libc++
# ============================================================================
message(STATUS "Building C++23 std modules from locally-built libc++...")
add_subdirectory(external/std)
message(STATUS "✓ std modules will be built at: ${CMAKE_BINARY_DIR}/external/std")

# Create FastJSON C++23 module library
add_library(fastjson)
target_sources(fastjson
    PUBLIC
        FILE_SET CXX_MODULES FILES
        modules/fastjson.cppm
    PRIVATE
        modules/fastjson.cpp
)

# Link with std_module for import std; support
target_link_libraries(fastjson PUBLIC $<TARGET_NAME_IF_EXISTS:std_module>)

# Add std module support for import std;
if(TARGET std_module)
    target_compile_options(fastjson PRIVATE
        -fprebuilt-module-path=${CMAKE_BINARY_DIR}/external/std/CMakeFiles/std_module.dir
    )
endif()

# Enable SIMD support with thread safety - defines are added after CPU detection
target_compile_definitions(fastjson PRIVATE 
    FASTJSON_ENABLE_SIMD
    FASTJSON_THREAD_SAFE
)

# Add thread-safe compilation options - flags are added after CPU detection
target_compile_options(fastjson PRIVATE
    -fPIC                   # Position Independent Code for shared libraries
    -fstack-protector-strong # Stack protection
    -D_FORTIFY_SOURCE=2     # Buffer overflow detection
    -stdlib=libc++          # Use custom libc++
)
target_link_options(fastjson PRIVATE 
    -stdlib=libc++
    -L/opt/clang-21/lib
)

# Set compile features and properties
target_compile_features(fastjson PUBLIC cxx_std_23)
set_target_properties(fastjson PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Create FastJSON Parallel C++23 module library with OpenMP support
add_library(fastjson_parallel)
target_sources(fastjson_parallel
    PUBLIC
        FILE_SET CXX_MODULES FILES
        modules/fastjson_parallel.cppm
    PRIVATE
        modules/numa_allocator.cpp
        modules/fastjson_simd_multiregister.cpp
        modules/fastjson_simd_api.cpp
)

# Link with std_module for import std; support
target_link_libraries(fastjson_parallel PUBLIC $<TARGET_NAME_IF_EXISTS:std_module>)

# Add std module support for import std;
if(TARGET std_module)
    target_compile_options(fastjson_parallel PRIVATE
        -fprebuilt-module-path=${CMAKE_BINARY_DIR}/external/std/CMakeFiles/std_module.dir
    )
endif()

# Create standalone SIMD Multi-Register library for high-performance JSON primitives
add_library(fastjson_simd_multiregister STATIC)
target_sources(fastjson_simd_multiregister
    PUBLIC
        modules/fastjson_simd_api.h
    PRIVATE
        modules/fastjson_simd_multiregister.cpp
        modules/fastjson_simd_api.cpp
    PUBLIC
        FILE_SET CXX_MODULES 
        BASE_DIRS modules
        FILES
            modules/fastjson_simd_multiregister.cppm
)

# Include directories for helper headers
target_include_directories(fastjson_parallel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
)

target_include_directories(fastjson_simd_multiregister PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
)

# Enable SIMD support with thread safety and OpenMP
target_compile_definitions(fastjson_parallel PRIVATE 
    FASTJSON_ENABLE_SIMD
    FASTJSON_THREAD_SAFE
)

# Configure SIMD multi-register library with aggressive optimizations
target_compile_definitions(fastjson_simd_multiregister PUBLIC
    FASTJSON_ENABLE_SIMD
    FASTJSON_MULTIREGISTER_SIMD
)

# Set compile features for both libraries
target_compile_features(fastjson_parallel PUBLIC cxx_std_23)
target_compile_features(fastjson_simd_multiregister PUBLIC cxx_std_23)

# Add thread-safe compilation options
target_compile_options(fastjson_parallel PRIVATE
    -fPIC                   # Position Independent Code for shared libraries
    -fstack-protector-strong # Stack protection
    -D_FORTIFY_SOURCE=2     # Buffer overflow detection
    -pthread                # POSIX thread support
    -ffast-math             # Fast math for performance
    -funroll-loops          # Loop unrolling
    -fvectorize             # Vectorization
    -fslp-vectorize         # SLP vectorization
    -stdlib=libc++          # Use custom libc++
)
target_link_options(fastjson_parallel PRIVATE 
    -stdlib=libc++
    -L/opt/clang-21/lib
)

# Aggressive optimization for SIMD multi-register library
target_compile_options(fastjson_simd_multiregister PRIVATE
    -O3                     # Maximum optimization
    -fPIC                   # Position Independent Code
    -ffast-math             # Aggressive fast math
    -funroll-loops          # Loop unrolling
    -fvectorize             # Auto-vectorization
    -fslp-vectorize         # SLP vectorization
    -stdlib=libc++          # Use custom libc++
    -march=native           # Native CPU optimizations (if enabled)
    -mtune=native           # Tune for native CPU
    $<$<BOOL:${FASTJSON_FORCE_AVX512}>:-mavx512f -mavx512bw>
    $<$<BOOL:${ENABLE_NATIVE_OPTIMIZATIONS}>:-mavx2 -mfma>
)
target_link_options(fastjson_simd_multiregister PRIVATE 
    -stdlib=libc++
    -L/opt/clang-21/lib
)

# Set compile features and properties
target_compile_features(fastjson_parallel PUBLIC cxx_std_23)
target_compile_features(fastjson_simd_multiregister PUBLIC cxx_std_23)

set_target_properties(fastjson_parallel PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

set_target_properties(fastjson_simd_multiregister PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    ARCHIVE_OUTPUT_NAME fastjson_simd_multiregister
)

# Create Multi-Register SIMD Parser library with complete fluent API
# Self-contained module with SIMD primitives and fluent parser API
add_library(fastjson_multiregister_parser STATIC)
target_sources(fastjson_multiregister_parser
    PUBLIC
        FILE_SET CXX_MODULES 
        BASE_DIRS modules
        FILES
            modules/fastjson_multiregister_parser.cppm
)

target_include_directories(fastjson_multiregister_parser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
)

target_compile_definitions(fastjson_multiregister_parser PUBLIC
    ${SIMD_DEFINES}
    FASTJSON_ENABLE_SIMD
    FASTJSON_MULTIREGISTER_SIMD
)

target_compile_features(fastjson_multiregister_parser PUBLIC cxx_std_23)

# IMPORTANT: Use explicit SIMD flags to avoid C++23 module configuration mismatch
# The flags MUST match exactly between the parser module and any consuming modules
# Using AVX2 only - this CPU doesn't support AVX-512 in hardware
set(MULTIREGISTER_SIMD_FLAGS 
    -stdlib=libc++
    -march=native 
    -mtune=native 
    -mavx2 
    -mfma
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
)

target_compile_options(fastjson_multiregister_parser PRIVATE
    -O3
    ${MULTIREGISTER_SIMD_FLAGS}
    -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}
    -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/external/std/CMakeFiles/std_module.dir
)

target_link_options(fastjson_multiregister_parser PRIVATE 
    -stdlib=libc++
    -L/opt/clang-21/lib
)

set_target_properties(fastjson_multiregister_parser PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    ARCHIVE_OUTPUT_NAME fastjson_multiregister_parser
)

# Link dependencies for fastjson_multiregister_parser
add_dependencies(fastjson_multiregister_parser 
    std_module 
)

# Link parallel processing libraries
if(ENABLE_PARALLEL_STL)
    # Link Intel TBB for Parallel STL support (std::execution::par, par_unseq)
    target_link_libraries(fastjson_parallel PUBLIC TBB::tbb)
    target_compile_definitions(fastjson_parallel PUBLIC FASTJSON_USE_PARALLEL_STL)
    message(STATUS "fastjson_parallel: Linked with Intel TBB for Parallel STL")
elseif(ENABLE_OPENMP)
    # Legacy OpenMP support
    target_link_libraries(fastjson_parallel PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(fastjson_parallel PUBLIC FASTJSON_USE_OPENMP)
    message(STATUS "fastjson_parallel: Linked with OpenMP (legacy)")
endif()

# Create JSON LINQ C++23 module library
add_library(json_linq)
target_sources(json_linq
    PUBLIC
        FILE_SET CXX_MODULES FILES
        modules/json_linq.cppm
)

# Enable OpenMP for parallel LINQ operations
target_compile_definitions(json_linq PRIVATE 
    FASTJSON_THREAD_SAFE
)

# Add thread-safe compilation options
target_compile_options(json_linq PRIVATE
    -pthread                # POSIX thread support
    -ffast-math             # Fast math for performance
    -funroll-loops          # Loop unrolling
    -fvectorize             # Vectorization
    -fslp-vectorize         # SLP vectorization
    -fstack-protector-strong # Stack protection
    -D_FORTIFY_SOURCE=2     # Buffer overflow detection
)

# Set compile features and properties
target_compile_features(json_linq PUBLIC cxx_std_23)
set_target_properties(json_linq PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Link OpenMP if enabled
if(ENABLE_OPENMP)
    target_link_libraries(json_linq PUBLIC OpenMP::OpenMP_CXX)
endif()

# SIMD Optimizations - CPU-specific conditional compilation
# Use -march=native to let compiler detect and enable all supported features
include(CheckCXXCompilerFlag)

message(STATUS "Detecting CPU instruction set support...")

# Runtime CPU feature detection from /proc/cpuinfo (Linux)
if(EXISTS "/proc/cpuinfo")
    file(READ "/proc/cpuinfo" CPUINFO_CONTENT)
    
    # Look for flags line containing actual CPU features
    string(REGEX MATCH "flags[ \t]*:([^\n]*)" FLAGS_MATCH "${CPUINFO_CONTENT}")
    if(FLAGS_MATCH)
        string(REGEX REPLACE "flags[ \t]*:" "" CPU_FLAGS "${FLAGS_MATCH}")
        string(TOLOWER "${CPU_FLAGS}" CPU_FLAGS)
        
        # Check for each instruction set in the actual CPU flags
        if(CPU_FLAGS MATCHES "avx2")
            set(RUNTIME_CPU_SUPPORTS_AVX2 TRUE)
            message(STATUS "  ✓ AVX2 (detected in /proc/cpuinfo)")
        else()
            set(RUNTIME_CPU_SUPPORTS_AVX2 FALSE)
            message(STATUS "  ✗ AVX2 (not found in /proc/cpuinfo)")
        endif()
        
        if(CPU_FLAGS MATCHES "avx512f")
            set(RUNTIME_CPU_SUPPORTS_AVX512F TRUE)
            message(STATUS "  ✓ AVX-512F (detected in /proc/cpuinfo)")
        else()
            set(RUNTIME_CPU_SUPPORTS_AVX512F FALSE)
            message(STATUS "  ✗ AVX-512F (not found in /proc/cpuinfo)")
        endif()
        
        if(CPU_FLAGS MATCHES "avx512bw")
            set(RUNTIME_CPU_SUPPORTS_AVX512BW TRUE)
            message(STATUS "  ✓ AVX-512BW (detected in /proc/cpuinfo)")
        else()
            set(RUNTIME_CPU_SUPPORTS_AVX512BW FALSE)
            message(STATUS "  ✗ AVX-512BW (not found in /proc/cpuinfo)")
        endif()
        
        message(STATUS "CPU flags summary: ${CPU_FLAGS}")
    else()
        message(WARNING "Could not parse /proc/cpuinfo flags")
        set(RUNTIME_CPU_SUPPORTS_AVX2 FALSE)
        set(RUNTIME_CPU_SUPPORTS_AVX512F FALSE)
        set(RUNTIME_CPU_SUPPORTS_AVX512BW FALSE)
    endif()
else()
    message(STATUS "  /proc/cpuinfo not available, using compiler detection")
    set(RUNTIME_CPU_SUPPORTS_AVX2 FALSE)
    set(RUNTIME_CPU_SUPPORTS_AVX512F FALSE)
    set(RUNTIME_CPU_SUPPORTS_AVX512BW FALSE)
endif()

message(STATUS "Detecting CPU instruction set support...")

# Option 1: Use -march=native for automatic CPU detection (recommended)
# Note: ENABLE_NATIVE_OPTIMIZATIONS is defined at the top of this file

if(ENABLE_NATIVE_OPTIMIZATIONS)
    check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
    if(COMPILER_SUPPORTS_MARCH_NATIVE)
        # Test actual CPU runtime support, not just compiler capabilities
        include(CheckCXXSourceRuns)
        
        # Detect SSE2 (always available on x86_64)
        set(CPU_SUPPORTS_SSE2 TRUE)
        message(STATUS "  ✓ SSE2 (baseline)")
        
        # Test SSE3
        set(CMAKE_REQUIRED_FLAGS "-msse3")
        check_cxx_source_runs("
            #include <immintrin.h>
            int main() {
                __m128i a = _mm_set1_epi32(1);
                __m128i b = _mm_hadd_epi32(a, a);  // SSE3 instruction
                return _mm_cvtsi128_si32(b);
            }
        " CPU_SUPPORTS_SSE3)
        
        # Test SSSE3
        set(CMAKE_REQUIRED_FLAGS "-mssse3")
        check_cxx_source_runs("
            #include <immintrin.h>
            int main() {
                __m128i a = _mm_set1_epi8(1);
                __m128i b = _mm_abs_epi8(a);  // SSSE3 instruction
                return _mm_cvtsi128_si32(b);
            }
        " CPU_SUPPORTS_SSSE3)
        
        # Test SSE4.1
        set(CMAKE_REQUIRED_FLAGS "-msse4.1")
        check_cxx_source_runs("
            #include <immintrin.h>
            int main() {
                __m128i a = _mm_set1_epi32(1);
                __m128i b = _mm_mullo_epi32(a, a);  // SSE4.1 instruction
                return _mm_cvtsi128_si32(b);
            }
        " CPU_SUPPORTS_SSE41)
        
        # Test SSE4.2
        set(CMAKE_REQUIRED_FLAGS "-msse4.2")
        check_cxx_source_runs("
            #include <immintrin.h>
            int main() {
                __m128i a = _mm_set1_epi8(65);
                int result = _mm_cmpestri(a, 1, a, 1, 0);  // SSE4.2 instruction
                return result;
            }
        " CPU_SUPPORTS_SSE42)
        
        # Test AVX
        set(CMAKE_REQUIRED_FLAGS "-mavx")
        check_cxx_source_runs("
            #include <immintrin.h>
            int main() {
                __m256 a = _mm256_set1_ps(1.0f);
                __m256 b = _mm256_add_ps(a, a);  // AVX instruction
                return (int)_mm256_cvtss_f32(b);
            }
        " CPU_SUPPORTS_AVX)
        
        # Test AVX2
        set(CMAKE_REQUIRED_FLAGS "-mavx2")
        check_cxx_source_runs("
            #include <immintrin.h>
            int main() {
                __m256i a = _mm256_set1_epi32(1);
                __m256i b = _mm256_mullo_epi32(a, a);  // AVX2 instruction
                return _mm256_extract_epi32(b, 0);
            }
        " CPU_SUPPORTS_AVX2)
        
        # Test AVX-512F (Foundation)
        set(CMAKE_REQUIRED_FLAGS "-mavx512f")
        check_cxx_source_runs("
            #include <immintrin.h>
            int main() {
                __m512i a = _mm512_set1_epi32(1);
                __m512i b = _mm512_add_epi32(a, a);  // AVX-512F instruction
                return _mm512_cvtsi512_si32(b);
            }
        " CPU_SUPPORTS_AVX512F)
        
        # Only test higher AVX-512 features if AVX-512F is supported
        if(CPU_SUPPORTS_AVX512F)
            # Test AVX-512BW (Byte/Word)
            set(CMAKE_REQUIRED_FLAGS "-mavx512f -mavx512bw")
            check_cxx_source_runs("
                #include <immintrin.h>
                int main() {
                    __m512i a = _mm512_set1_epi8(1);
                    __m512i b = _mm512_add_epi8(a, a);  // AVX-512BW instruction
                    return _mm512_cvtsi512_si32(b);
                }
            " CPU_SUPPORTS_AVX512BW)
        else()
            set(CPU_SUPPORTS_AVX512BW FALSE)
        endif()
        
        set(CMAKE_REQUIRED_FLAGS)
        message(STATUS "  Using runtime CPU detection for optimal SIMD selection")
    else()
        message(WARNING "-march=native not supported, falling back to manual detection")
        set(ENABLE_NATIVE_OPTIMIZATIONS OFF)
    endif()
endif()

# Option 2: Manual feature detection (fallback)
if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
    check_cxx_compiler_flag("-msse2" CPU_SUPPORTS_SSE2)
    check_cxx_compiler_flag("-msse3" CPU_SUPPORTS_SSE3)
    check_cxx_compiler_flag("-mssse3" CPU_SUPPORTS_SSSE3)
    check_cxx_compiler_flag("-msse4.1" CPU_SUPPORTS_SSE41)
    check_cxx_compiler_flag("-msse4.2" CPU_SUPPORTS_SSE42)
    check_cxx_compiler_flag("-mavx" CPU_SUPPORTS_AVX)
    check_cxx_compiler_flag("-mavx2" CPU_SUPPORTS_AVX2)
    check_cxx_compiler_flag("-mavx512f" CPU_SUPPORTS_AVX512F)
    check_cxx_compiler_flag("-mavx512bw" CPU_SUPPORTS_AVX512BW)
    check_cxx_compiler_flag("-mavx512vbmi" CPU_SUPPORTS_AVX512VBMI)
    check_cxx_compiler_flag("-mavx512vbmi2" CPU_SUPPORTS_AVX512VBMI2)
    check_cxx_compiler_flag("-mavx512vnni" CPU_SUPPORTS_AVX512VNNI)
    check_cxx_compiler_flag("-mamx-tile" CPU_SUPPORTS_AMX)
endif()

# Set unsupported advanced features to FALSE if not detected
if(NOT CPU_SUPPORTS_AVX512F)
    set(CPU_SUPPORTS_AVX512BW FALSE)
    set(CPU_SUPPORTS_AVX512VBMI FALSE)
    set(CPU_SUPPORTS_AVX512VBMI2 FALSE)
    set(CPU_SUPPORTS_AVX512VNNI FALSE)
else()
    # Only enable if both compiler and runtime support
    if(NOT CPU_SUPPORTS_AVX512BW)
        set(CPU_SUPPORTS_AVX512VBMI FALSE)
        set(CPU_SUPPORTS_AVX512VBMI2 FALSE)
        set(CPU_SUPPORTS_AVX512VNNI FALSE)
    endif()
endif()

# Parent project overrides for WSL2/VM environments
# Allow parent to force disable AVX-512/AMX even if detected
if(NOT "${FASTJSON_FORCE_AVX512}" STREQUAL "")
    if(FASTJSON_FORCE_AVX512)
        message(STATUS "AVX-512 enabled (forced by parent project)")
    else()
        set(CPU_SUPPORTS_AVX512F FALSE)
        set(CPU_SUPPORTS_AVX512BW FALSE)
        set(CPU_SUPPORTS_AVX512VBMI FALSE)
        set(CPU_SUPPORTS_AVX512VBMI2 FALSE)
        set(CPU_SUPPORTS_AVX512VNNI FALSE)
        message(STATUS "AVX-512 disabled (forced by parent project - likely WSL2/VM)")
    endif()
endif()

if(NOT "${FASTJSON_FORCE_AMX}" STREQUAL "")
    if(FASTJSON_FORCE_AMX)
        message(STATUS "AMX enabled (forced by parent project)")
    else()
        set(CPU_SUPPORTS_AMX FALSE)
        message(STATUS "AMX disabled (forced by parent project - likely WSL2/VM)")
    endif()
endif()

# Build SIMD flags based on what CPU actually supports
set(SIMD_FLAGS "")
set(SIMD_DEFINES "")
set(SIMD_LEVELS "")

# Use -march=native for optimal performance if available
if(ENABLE_NATIVE_OPTIMIZATIONS AND COMPILER_SUPPORTS_MARCH_NATIVE)
    list(APPEND SIMD_FLAGS "-march=native")
    message(STATUS "  ✓ Using -march=native for optimal CPU feature detection")
endif()

# Set feature defines based on detection
if(CPU_SUPPORTS_SSE2)
    list(APPEND SIMD_DEFINES "HAVE_SSE2")
    list(APPEND SIMD_LEVELS "SSE2")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-msse2")
    endif()
    message(STATUS "  ✓ SSE2 (baseline)")
endif()

if(CPU_SUPPORTS_SSE3)
    list(APPEND SIMD_DEFINES "HAVE_SSE3")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-msse3")
    endif()
    message(STATUS "  ✓ SSE3")
endif()

if(CPU_SUPPORTS_SSSE3)
    list(APPEND SIMD_DEFINES "HAVE_SSSE3")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-mssse3")
    endif()
    message(STATUS "  ✓ SSSE3")
endif()

if(CPU_SUPPORTS_SSE41)
    list(APPEND SIMD_DEFINES "HAVE_SSE41")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-msse4.1")
    endif()
    message(STATUS "  ✓ SSE4.1")
endif()

if(CPU_SUPPORTS_SSE42)
    list(APPEND SIMD_DEFINES "HAVE_SSE42")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-msse4.2")
    endif()
    message(STATUS "  ✓ SSE4.2")
endif()

if(CPU_SUPPORTS_AVX)
    list(APPEND SIMD_DEFINES "HAVE_AVX")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-mavx")
    endif()
    message(STATUS "  ✓ AVX")
endif()

if(CPU_SUPPORTS_AVX2)
    list(APPEND SIMD_DEFINES "HAVE_AVX2")
    list(APPEND SIMD_LEVELS "AVX2")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-mavx2")
    endif()
    message(STATUS "  ✓ AVX2")
endif()

if(CPU_SUPPORTS_AVX512F)
    list(APPEND SIMD_DEFINES "HAVE_AVX512F")
    list(APPEND SIMD_LEVELS "AVX-512F")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-mavx512f")
    else()
        # Add explicit AVX-512 support without deprecated EVEX512
        list(APPEND SIMD_FLAGS "-mavx512f" "-mavx512bw")
    endif()
    message(STATUS "  ✓ AVX-512 F (Foundation)")
endif()

if(CPU_SUPPORTS_AVX512BW)
    list(APPEND SIMD_DEFINES "HAVE_AVX512BW")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-mavx512bw")
    endif()
    message(STATUS "  ✓ AVX-512 BW (Byte/Word)")
endif()

if(CPU_SUPPORTS_AVX512VBMI)
    list(APPEND SIMD_DEFINES "HAVE_AVX512VBMI")
    list(APPEND SIMD_LEVELS "AVX-512-VBMI")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-mavx512vbmi")
    endif()
    message(STATUS "  ✓ AVX-512 VBMI")
endif()

if(CPU_SUPPORTS_AVX512VBMI2)
    list(APPEND SIMD_DEFINES "HAVE_AVX512VBMI2")
    list(APPEND SIMD_LEVELS "AVX-512-VBMI2")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-mavx512vbmi2")
    endif()
    message(STATUS "  ✓ AVX-512 VBMI2")
endif()

if(CPU_SUPPORTS_AVX512VNNI)
    list(APPEND SIMD_DEFINES "HAVE_AVX512VNNI")
    list(APPEND SIMD_LEVELS "AVX-512-VNNI")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-mavx512vnni")
    endif()
    message(STATUS "  ✓ AVX-512 VNNI")
endif()

if(CPU_SUPPORTS_AMX)
    list(APPEND SIMD_DEFINES "HAVE_AMX_TILE" "HAVE_AMX_INT8")
    list(APPEND SIMD_LEVELS "AMX")
    if(NOT ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND SIMD_FLAGS "-mamx-tile" "-mamx-int8")
    endif()
    message(STATUS "  ✓ AMX (Advanced Matrix Extensions)")
endif()

# Add additional optimization flags
list(APPEND SIMD_FLAGS "-fno-omit-frame-pointer")

# Enhanced message showing detected capabilities
if(SIMD_LEVELS)
    string(JOIN " → " SIMD_LEVELS_STR ${SIMD_LEVELS})
    message(STATUS "SIMD Optimization Path: ${SIMD_LEVELS_STR}")
else()
    message(STATUS "SIMD Optimizations: Baseline x86-64 (no advanced SIMD)")
endif()

# Apply CPU-specific SIMD flags and defines to the target
if(SIMD_FLAGS)
    target_compile_options(fastjson PRIVATE ${SIMD_FLAGS})
endif()

if(SIMD_DEFINES)
    target_compile_definitions(fastjson PRIVATE ${SIMD_DEFINES})
endif()

# Link required libraries including logger
target_link_libraries(fastjson PRIVATE
    Threads::Threads
    logger
)

# Optional OpenMP linking
if(ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    target_link_libraries(fastjson PUBLIC OpenMP::OpenMP_CXX)
endif()

# Set module output directory and enable position-independent code
set_target_properties(fastjson PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Create test executable for C++23 modules (COMMENTED - file doesn't exist)
# add_executable(module_test tests/module_test.cpp)
# target_link_libraries(module_test fastjson logger)

# Trail calling syntax verification test (COMMENTED - file doesn't exist)
# add_executable(trail_calling_test tests/trail_calling_test.cpp)
# target_link_libraries(trail_calling_test logger)

# Comprehensive feature test (SIMD, OpenMP, threading) (COMMENTED - file doesn't exist)
# add_executable(feature_test tests/feature_test.cpp)
# target_link_libraries(feature_test OpenMP::OpenMP_CXX Threads::Threads logger)
# set_target_properties(feature_test PROPERTIES
#     BUILD_RPATH "/home/linuxbrew/.linuxbrew/lib;/home/linuxbrew/.linuxbrew/Cellar/libomp/21.1.5/lib"
#     INSTALL_RPATH "/home/linuxbrew/.linuxbrew/lib;/home/linuxbrew/.linuxbrew/Cellar/libomp/21.1.5/lib"
# )

# Add MVP test executable (COMMENTED - file doesn't exist)
# add_executable(mvp_test tests/mvp_test.cpp)
# target_link_libraries(mvp_test PRIVATE fastjson ${OpenMP_CXX_LIBRARIES} logger)

# Add SIMD benchmark executable (COMMENTED - file doesn't exist)
# add_executable(simd_benchmark tests/simd_benchmark.cpp)
# target_link_libraries(simd_benchmark PRIVATE fastjson ${OpenMP_CXX_LIBRARIES} logger)

# Apply same compilation options as the library
# target_compile_options(mvp_test PRIVATE
#     -std=c++23
#     -fopenmp
#     -O3
#     -ffast-math
#     -funroll-loops
#     -fvectorize
#     -fslp-vectorize
#     -Wall
#     -Wextra
#     -Wpedantic
#     -Wno-unused-parameter
#     -pthread
# )

# Same comprehensive SIMD options as library
# if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
#     target_compile_options(mvp_test PRIVATE ${SIMD_FLAGS})
#     target_compile_definitions(mvp_test PRIVATE ${SIMD_DEFINES} FASTJSON_ENABLE_SIMD)
# endif()

# set_target_properties(mvp_test PROPERTIES
#     BUILD_RPATH "/home/linuxbrew/.linuxbrew/lib;/home/linuxbrew/.linuxbrew/Cellar/libomp/21.1.5/lib"
#     INSTALL_RPATH "/home/linuxbrew/.linuxbrew/lib;/home/linuxbrew/.linuxbrew/Cellar/libomp/21.1.5/lib"
# )

# Comparative benchmark: FastestJSONInTheWest vs simdjson
add_executable(comparative_benchmark benchmarks/comparative_benchmark.cpp)
target_link_libraries(comparative_benchmark PRIVATE logger)
if(OpenMP_CXX_FOUND)
    target_link_libraries(comparative_benchmark PRIVATE OpenMP::OpenMP_CXX)
endif()
target_compile_options(comparative_benchmark PRIVATE
    -std=c++23
    -O3
    -ffast-math
    -funroll-loops
    -fvectorize
    -fslp-vectorize
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -pthread
)

# LINQ benchmark: Sequential vs Parallel LINQ operations
add_executable(linq_benchmark benchmarks/linq_benchmark.cpp)
target_link_libraries(linq_benchmark PRIVATE fastjson_parallel json_linq)
if(OpenMP_CXX_FOUND)
    target_link_libraries(linq_benchmark PRIVATE OpenMP::OpenMP_CXX)
endif()
target_compile_options(linq_benchmark PRIVATE
    -std=c++23
    -O3
    -ffast-math
    -funroll-loops
    -fvectorize
    -fslp-vectorize
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -pthread
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
)

# Simple performance demo using modules (COMMENTED - file doesn't exist)
# add_executable(performance_demo tests/performance_demo.cpp)
# target_link_libraries(performance_demo fastjson logger)

# Comprehensive test using modules (COMMENTED - file doesn't exist)
# add_executable(comprehensive_test tests/comprehensive_test.cpp)
# target_link_libraries(comprehensive_test fastjson logger)

# C++17 Compatible Header-Only Test (COMMENTED - file doesn't exist)
# add_executable(test_cpp17
#     test_cpp17.cpp
# )

# set_target_properties(test_cpp17 PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED ON
#     CXX_EXTENSIONS OFF
# )

# target_include_directories(test_cpp17 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# if(OpenMP_CXX_FOUND)
#     target_link_libraries(test_cpp17 PRIVATE ${OpenMP_CXX_LIBRARIES})
#     target_compile_options(test_cpp17 PRIVATE ${OpenMP_CXX_FLAGS})
# endif()

# if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
#     target_compile_options(test_cpp17 PRIVATE ${SIMD_FLAGS})
#     target_compile_definitions(test_cpp17 PRIVATE ${SIMD_DEFINES} FASTJSON_ENABLE_SIMD)
# endif()

# target_compile_options(test_cpp17 PRIVATE
#     -O3
#     -ffast-math
#     -funroll-loops
#     -fvectorize
#     -fslp-vectorize
#     -Wall
#     -Wextra
#     -Wpedantic
#     -Wno-unused-parameter
#     -pthread
# )

# set_target_properties(test_cpp17 PROPERTIES
#     BUILD_RPATH "/home/linuxbrew/.linuxbrew/lib;/home/linuxbrew/.linuxbrew/Cellar/libomp/21.1.5/lib"
#     INSTALL_RPATH "/home/linuxbrew/.linuxbrew/lib;/home/linuxbrew/.linuxbrew/Cellar/libomp/21.1.5/lib"
# )

# Legacy GCC Support Test (C++17 without SIMD) (COMMENTED - file doesn't exist)
# add_executable(test_legacy_cpp17
#     test_cpp17.cpp
# )

# set_target_properties(test_legacy_cpp17 PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED ON
#     CXX_EXTENSIONS OFF
# )

# target_include_directories(test_legacy_cpp17 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Disable SIMD for legacy compatibility test
# target_compile_definitions(test_legacy_cpp17 PRIVATE
#     FASTJSON_LEGACY_MODE
# )

# target_compile_options(test_legacy_cpp17 PRIVATE
#     -O2
#     -Wall
#     -Wextra
#     -Wpedantic
#     -Wno-unused-parameter
# )

# Print configuration summary
message(STATUS "=== FastestJSONInTheWest Configuration ===")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Modules Support: Enabled")
message(STATUS "Trail Calling Syntax: Enforced via clang-tidy")
message(STATUS "==========================================")
message(STATUS "=== FastestJSONInTheWest MVP Build ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Header-Only: TRUE")
message(STATUS "Python Bindings: Available in python_bindings/ folder")
message(STATUS "  - Build: mkdir python_bindings/build && cd python_bindings/build && cmake .. && make")
message(STATUS "  - Tests: uv run pytest python_bindings/tests/ -v")
message(STATUS "========================================")

# Quick object parsing test (only if source exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_simple.cpp")
    add_executable(test_simple test_simple.cpp)
    target_link_libraries(test_simple PRIVATE fastjson)
    target_compile_options(test_simple PRIVATE
        ${SIMD_FLAGS}
        -fstack-protector-strong
    )
    target_compile_definitions(test_simple PRIVATE ${SIMD_DEFINES} FASTJSON_ENABLE_SIMD)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(test_simple PRIVATE OpenMP::OpenMP_CXX)
    endif()
    set_target_properties(test_simple PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
endif()

# Comprehensive test suite (only if source exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_comprehensive.cpp")
    add_executable(test_comprehensive test_comprehensive.cpp)
    target_link_libraries(test_comprehensive PRIVATE fastjson)
    target_compile_options(test_comprehensive PRIVATE
        ${SIMD_FLAGS}
        -fstack-protector-strong
    )
    target_compile_definitions(test_comprehensive PRIVATE ${SIMD_DEFINES} FASTJSON_ENABLE_SIMD)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(test_comprehensive PRIVATE OpenMP::OpenMP_CXX)
    endif()
    set_target_properties(test_comprehensive PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
endif()

# 128-bit number parsing test suite (only if source exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_128bit.cpp")
    add_executable(test_128bit test_128bit.cpp)
    target_link_libraries(test_128bit PRIVATE fastjson)
    target_compile_options(test_128bit PRIVATE
        ${SIMD_FLAGS}
        -fstack-protector-strong
    )
    target_compile_definitions(test_128bit PRIVATE ${SIMD_DEFINES} FASTJSON_ENABLE_SIMD)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(test_128bit PRIVATE OpenMP::OpenMP_CXX)
    endif()
    set_target_properties(test_128bit PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
endif()

# Numeric conversion helpers test suite (only if source exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_conversion_helpers.cpp")
    add_executable(test_conversion_helpers test_conversion_helpers.cpp)
    target_link_libraries(test_conversion_helpers PRIVATE fastjson)
    target_compile_options(test_conversion_helpers PRIVATE
        ${SIMD_FLAGS}
        -fstack-protector-strong
    )
    target_compile_definitions(test_conversion_helpers PRIVATE ${SIMD_DEFINES} FASTJSON_ENABLE_SIMD)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(test_conversion_helpers PRIVATE OpenMP::OpenMP_CXX)
    endif()
    set_target_properties(test_conversion_helpers PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
endif()

# Tool: Generate large JSON test files
add_executable(generate_test_file tools/generate_test_file.cpp)
target_compile_options(generate_test_file PRIVATE
    -std=c++23
    -O3
    -Wall
    -Wextra
)
set_target_properties(generate_test_file PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# Tool: Test large file parsing with current parser (only if source exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_large_file.cpp")
    add_executable(test_large_file test_large_file.cpp)
    target_link_libraries(test_large_file PRIVATE fastjson)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(test_large_file PRIVATE OpenMP::OpenMP_CXX)
    endif()
    target_compile_options(test_large_file PRIVATE
        ${SIMD_FLAGS}
        -fstack-protector-strong
    )
    target_compile_definitions(test_large_file PRIVATE ${SIMD_DEFINES} FASTJSON_ENABLE_SIMD)
    set_target_properties(test_large_file PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
endif()

# Tool: Test large file parsing with parallel benchmarks (only if source exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_large_file_parallel.cpp")
    add_executable(test_large_file_parallel test_large_file_parallel.cpp)
    target_link_libraries(test_large_file_parallel PRIVATE fastjson_parallel)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(test_large_file_parallel PRIVATE OpenMP::OpenMP_CXX)
    endif()
    target_compile_options(test_large_file_parallel PRIVATE
        ${SIMD_FLAGS}
        -fstack-protector-strong
        -fopenmp=libomp
    )
    target_compile_definitions(test_large_file_parallel PRIVATE ${SIMD_DEFINES} FASTJSON_ENABLE_SIMD FASTJSON_THREAD_SAFE)
    set_target_properties(test_large_file_parallel PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
endif()

# Tool: Test using the legacy parallel parser (only if source exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_parallel_parser.cpp")
    add_executable(test_parallel_parser test_parallel_parser.cpp)
    target_link_libraries(test_parallel_parser PRIVATE fastjson_parallel)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(test_parallel_parser PRIVATE OpenMP::OpenMP_CXX)
    endif()
    target_compile_options(test_parallel_parser PRIVATE
        ${SIMD_FLAGS}
        -O3
        -DFASTJSON_NO_LOGGER
        -DFASTJSON_ENABLE_SIMD
    )
    target_compile_definitions(test_parallel_parser PRIVATE ${SIMD_DEFINES} FASTJSON_ENABLE_SIMD FASTJSON_NO_LOGGER)
    set_target_properties(test_parallel_parser PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
endif()

# Test executable for parallel module (only if source exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_parallel_module.cpp")
    add_executable(test_parallel_module test_parallel_module.cpp)
    target_link_libraries(test_parallel_module PRIVATE fastjson_parallel)
    if(ENABLE_OPENMP)
        target_link_libraries(test_parallel_module PRIVATE OpenMP::OpenMP_CXX)
    endif()
    target_compile_options(test_parallel_module PRIVATE
        -fPIC
        -fstack-protector-strong
        -D_FORTIFY_SOURCE=2
    )
    target_compile_definitions(test_parallel_module PRIVATE
        FASTJSON_ENABLE_SIMD
        FASTJSON_THREAD_SAFE
    )
    set_target_properties(test_parallel_module PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
endif()

# Type mappings demonstration executable
add_executable(type_mappings_demo examples/type_mappings_demo.cpp)
target_link_libraries(type_mappings_demo PRIVATE fastjson_parallel)
if(ENABLE_OPENMP)
    target_link_libraries(type_mappings_demo PRIVATE OpenMP::OpenMP_CXX)
endif()
target_compile_options(type_mappings_demo PRIVATE
    -fPIC
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
)
target_compile_definitions(type_mappings_demo PRIVATE 
    FASTJSON_ENABLE_SIMD
    FASTJSON_THREAD_SAFE
)
set_target_properties(type_mappings_demo PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
)

# ============================================================================
# SIMD Multi-Register Performance Tests and Benchmarks
# ============================================================================

# SIMD Multi-Register Performance Test Executable  
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_simd_multiregister_performance.cpp")
    add_executable(test_simd_multiregister_performance tests/test_simd_multiregister_performance.cpp)
    target_link_libraries(test_simd_multiregister_performance PRIVATE 
        fastjson_simd_multiregister
        fastjson_parallel
        gtest_main
        gtest
    )
    
    target_compile_options(test_simd_multiregister_performance PRIVATE
        -O3                     # Maximum optimization
        -fPIC
        -ffast-math
        -funroll-loops
        -fvectorize
        -fslp-vectorize
        -march=native
        -mtune=native
        $<$<BOOL:${FASTJSON_FORCE_AVX512}>:-mavx512f -mavx512bw>
        $<$<BOOL:${ENABLE_NATIVE_OPTIMIZATIONS}>:-mavx2 -mfma>
    )
    
    target_compile_definitions(test_simd_multiregister_performance PRIVATE
        FASTJSON_ENABLE_SIMD
        FASTJSON_MULTIREGISTER_SIMD
    )
    
    set_target_properties(test_simd_multiregister_performance PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
endif()

# SIMD Multi-Register Benchmark Executable (if Google Benchmark is available)
find_package(benchmark QUIET)
if(benchmark_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/simd_multiregister_benchmark.cpp")
    add_executable(simd_multiregister_benchmark benchmarks/simd_multiregister_benchmark.cpp)
    target_link_libraries(simd_multiregister_benchmark PRIVATE
        fastjson_simd_multiregister
        fastjson_parallel
        benchmark::benchmark
        benchmark::benchmark_main
    )
    
    target_compile_options(simd_multiregister_benchmark PRIVATE
        -O3                     # Maximum optimization for benchmarking
        -fPIC
        -ffast-math
        -funroll-loops
        -fvectorize
        -fslp-vectorize
        -march=native
        -mtune=native
        $<$<BOOL:${FASTJSON_FORCE_AVX512}>:-mavx512f -mavx512bw>
        $<$<BOOL:${ENABLE_NATIVE_OPTIMIZATIONS}>:-mavx2 -mfma>
    )
    
    target_compile_definitions(simd_multiregister_benchmark PRIVATE
        FASTJSON_ENABLE_SIMD
        FASTJSON_MULTIREGISTER_SIMD
    )
    
    set_target_properties(simd_multiregister_benchmark PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
    
    message(STATUS "Added SIMD multi-register benchmark executable")
else()
    message(STATUS "Google Benchmark not found, skipping benchmark executable")
endif()

# Standalone SIMD Multi-Register Demo
add_executable(simd_multiregister_demo examples/simd_multiregister_demo.cpp)
target_link_libraries(simd_multiregister_demo PRIVATE 
    fastjson_simd_multiregister
    fastjson_parallel
)

target_compile_options(simd_multiregister_demo PRIVATE
    -O2
    -fPIC
    -ffast-math
    -march=native
    $<$<BOOL:${FASTJSON_FORCE_AVX512}>:-mavx512f -mavx512bw>
    $<$<BOOL:${ENABLE_NATIVE_OPTIMIZATIONS}>:-mavx2 -mfma>
)

target_compile_definitions(simd_multiregister_demo PRIVATE
    FASTJSON_ENABLE_SIMD
    FASTJSON_MULTIREGISTER_SIMD
)

set_target_properties(simd_multiregister_demo PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
)

# ============================================================================
# Multi-Register Parser Module Tests (Self-contained C++23 Module)
# ============================================================================

# Test module for multiregister parser (if test module exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiregister_parser.cppm")
    add_library(test_multiregister_parser_lib STATIC)
    target_sources(test_multiregister_parser_lib
        PUBLIC
            FILE_SET CXX_MODULES 
            BASE_DIRS tests
            FILES
                tests/test_multiregister_parser.cppm
    )
    
    # IMPORTANT: Compile flags MUST match fastjson_multiregister_parser exactly
    # to avoid C++23 module configuration mismatch errors
    target_compile_options(test_multiregister_parser_lib PRIVATE
        -O3
        ${MULTIREGISTER_SIMD_FLAGS}
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/external/std/CMakeFiles/std_module.dir
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/fastjson_multiregister_parser.dir
    )
    
    target_link_options(test_multiregister_parser_lib PRIVATE 
        -stdlib=libc++
        -L/opt/clang-21/lib
    )
    
    # Link to parser module to get the PCM file dependency
    target_link_libraries(test_multiregister_parser_lib PRIVATE
        fastjson_multiregister_parser
    )
    
    add_dependencies(test_multiregister_parser_lib 
        std_module 
        fastjson_multiregister_parser
    )
    
    set_target_properties(test_multiregister_parser_lib PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endif()

# Test executable for multiregister parser
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiregister_main.cpp")
    add_executable(test_multiregister_parser tests/test_multiregister_main.cpp)
    target_link_libraries(test_multiregister_parser PRIVATE 
        test_multiregister_parser_lib
        fastjson_multiregister_parser
    )
    
    target_compile_options(test_multiregister_parser PRIVATE
        -O3
        ${MULTIREGISTER_SIMD_FLAGS}
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/external/std/CMakeFiles/std_module.dir
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/fastjson_multiregister_parser.dir
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_multiregister_parser_lib.dir
    )
    
    target_link_options(test_multiregister_parser PRIVATE 
        -stdlib=libc++
        -L/opt/clang-21/lib
    )
    
    add_dependencies(test_multiregister_parser 
        test_multiregister_parser_lib
        fastjson_multiregister_parser
    )
    
    set_target_properties(test_multiregister_parser PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
    
    message(STATUS "Added Multi-Register Parser test executable")
endif()

# Benchmark executable for multiregister vs original parser
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_multiregister.cpp")
    add_executable(benchmark_multiregister tests/benchmark_multiregister.cpp)
    
    target_link_libraries(benchmark_multiregister PRIVATE 
        fastjson_multiregister_parser
        fastjson
    )
    
    target_compile_options(benchmark_multiregister PRIVATE
        -O3
        -stdlib=libc++
        -march=native
        -mtune=native
        -mavx2
        -mfma
        # Must match fastjson module flags for module compatibility
        -fstack-protector-strong
        -D_FORTIFY_SOURCE=2
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/external/std/CMakeFiles/std_module.dir
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/fastjson_multiregister_parser.dir
        -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/fastjson.dir
    )
    
    target_link_options(benchmark_multiregister PRIVATE 
        -stdlib=libc++
        -L/opt/clang-21/lib
    )
    
    add_dependencies(benchmark_multiregister 
        fastjson_multiregister_parser
        fastjson
    )
    
    set_target_properties(benchmark_multiregister PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
        INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    )
    
    message(STATUS "Added Multi-Register Parser benchmark executable")
endif()
