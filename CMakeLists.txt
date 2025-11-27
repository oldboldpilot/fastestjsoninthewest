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

# Default toolchain paths (can be overridden)
set(CLANG_DEFAULT_PATH "/opt/clang-21/bin" CACHE STRING "Default path for Clang binaries")
set(LIBCXX_DEFAULT_INCLUDE "/opt/clang-21/include/c++/v1" CACHE STRING "Default path for libc++ headers")
set(LIBCXX_DEFAULT_LIB "/opt/clang-21/lib" CACHE STRING "Default path for libc++ libraries")

# Set compilers using the configurable paths
set(CMAKE_CXX_COMPILER "${CLANG_DEFAULT_PATH}/clang++")
set(CMAKE_C_COMPILER "${CLANG_DEFAULT_PATH}/clang")

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
option(ENABLE_OPENMP "Enable OpenMP parallel processing support" OFF)

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

# Optional OpenMP support for parallel JSON parsing
if(ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "Enabling OpenMP support for parallel JSON parsing")
    else()
        message(WARNING "OpenMP requested but not found")
    endif()
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
            add_subdirectory(external/cpp23-logger EXCLUDE_FROM_ALL)
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

# Create FastJSON C++23 module library
add_library(fastjson)
target_sources(fastjson
    PUBLIC
        FILE_SET CXX_MODULES FILES
        modules/fastjson.cppm
    PRIVATE
        modules/fastjson.cpp
)

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
)

# Set compile features and properties
target_compile_features(fastjson PUBLIC cxx_std_23)
set_target_properties(fastjson PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# SIMD Optimizations - CPU-specific conditional compilation
# Use -march=native to let compiler detect and enable all supported features
include(CheckCXXCompilerFlag)

message(STATUS "Detecting CPU instruction set support...")

# Option 1: Use -march=native for automatic CPU detection (recommended)
# Note: ENABLE_NATIVE_OPTIMIZATIONS is defined at the top of this file

if(ENABLE_NATIVE_OPTIMIZATIONS)
    check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
    if(COMPILER_SUPPORTS_MARCH_NATIVE)
        set(CPU_SUPPORTS_SSE2 TRUE)
        set(CPU_SUPPORTS_SSE3 TRUE)
        set(CPU_SUPPORTS_SSSE3 TRUE)
        set(CPU_SUPPORTS_SSE41 TRUE)
        set(CPU_SUPPORTS_SSE42 TRUE)
        set(CPU_SUPPORTS_AVX TRUE)
        set(CPU_SUPPORTS_AVX2 TRUE)
        # Check for AVX-512 support by testing compilation
        check_cxx_compiler_flag("-mavx512f" COMPILER_HAS_AVX512F)
        if(COMPILER_HAS_AVX512F)
            include(CheckCXXSourceCompiles)
            set(CMAKE_REQUIRED_FLAGS "-march=native -mavx512f")
            check_cxx_source_compiles("
                #include <immintrin.h>
                int main() { __m512i a = _mm512_setzero_si512(); return 0; }
            " CPU_SUPPORTS_AVX512F)
            set(CMAKE_REQUIRED_FLAGS)
        endif()
        message(STATUS "  Using -march=native (auto-detects CPU features)")
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

# Quick object parsing test
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

# Comprehensive test suite (70+ tests)
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

# 128-bit number parsing test suite
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

# Numeric conversion helpers test suite
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
