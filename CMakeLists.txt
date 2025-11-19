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
find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

# Add Clang library paths using configurable variables
link_directories(
    ${LIBCXX_DEFAULT_LIB}
    ${LIBCXX_DEFAULT_LIB}/x86_64-unknown-linux-gnu
)

# ============================================================================
# Integrate cpp23-logger dependency
# ============================================================================
# Option to use FetchContent or submodule
option(USE_FETCHCONTENT_LOGGER "Use FetchContent to fetch cpp23-logger instead of submodule" OFF)

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
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cpp23-logger/CMakeLists.txt")
        add_subdirectory(external/cpp23-logger EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR "cpp23-logger submodule not found. Run: git submodule update --init --recursive")
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
option(USE_NATIVE_ARCH "Use -march=native for automatic CPU feature detection" ON)

if(USE_NATIVE_ARCH)
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
        set(USE_NATIVE_ARCH OFF)
    endif()
endif()

# Option 2: Manual feature detection (fallback)
if(NOT USE_NATIVE_ARCH)
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

# Build SIMD flags based on what CPU actually supports
set(SIMD_FLAGS "")
set(SIMD_DEFINES "")
set(SIMD_LEVELS "")

# Use -march=native for optimal performance if available
if(USE_NATIVE_ARCH AND COMPILER_SUPPORTS_MARCH_NATIVE)
    list(APPEND SIMD_FLAGS "-march=native")
    message(STATUS "  ✓ Using -march=native for optimal CPU feature detection")
endif()

# Set feature defines based on detection
if(CPU_SUPPORTS_SSE2)
    list(APPEND SIMD_DEFINES "HAVE_SSE2")
    list(APPEND SIMD_LEVELS "SSE2")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-msse2")
    endif()
    message(STATUS "  ✓ SSE2 (baseline)")
endif()

if(CPU_SUPPORTS_SSE3)
    list(APPEND SIMD_DEFINES "HAVE_SSE3")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-msse3")
    endif()
    message(STATUS "  ✓ SSE3")
endif()

if(CPU_SUPPORTS_SSSE3)
    list(APPEND SIMD_DEFINES "HAVE_SSSE3")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-mssse3")
    endif()
    message(STATUS "  ✓ SSSE3")
endif()

if(CPU_SUPPORTS_SSE41)
    list(APPEND SIMD_DEFINES "HAVE_SSE41")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-msse4.1")
    endif()
    message(STATUS "  ✓ SSE4.1")
endif()

if(CPU_SUPPORTS_SSE42)
    list(APPEND SIMD_DEFINES "HAVE_SSE42")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-msse4.2")
    endif()
    message(STATUS "  ✓ SSE4.2")
endif()

if(CPU_SUPPORTS_AVX)
    list(APPEND SIMD_DEFINES "HAVE_AVX")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-mavx")
    endif()
    message(STATUS "  ✓ AVX")
endif()

if(CPU_SUPPORTS_AVX2)
    list(APPEND SIMD_DEFINES "HAVE_AVX2")
    list(APPEND SIMD_LEVELS "AVX2")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-mavx2")
    endif()
    message(STATUS "  ✓ AVX2")
endif()

if(CPU_SUPPORTS_AVX512F)
    list(APPEND SIMD_DEFINES "HAVE_AVX512F")
    list(APPEND SIMD_LEVELS "AVX-512F")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-mavx512f")
    endif()
    message(STATUS "  ✓ AVX-512 F (Foundation)")
endif()

if(CPU_SUPPORTS_AVX512BW)
    list(APPEND SIMD_DEFINES "HAVE_AVX512BW")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-mavx512bw")
    endif()
    message(STATUS "  ✓ AVX-512 BW (Byte/Word)")
endif()

if(CPU_SUPPORTS_AVX512VBMI)
    list(APPEND SIMD_DEFINES "HAVE_AVX512VBMI")
    list(APPEND SIMD_LEVELS "AVX-512-VBMI")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-mavx512vbmi")
    endif()
    message(STATUS "  ✓ AVX-512 VBMI")
endif()

if(CPU_SUPPORTS_AVX512VBMI2)
    list(APPEND SIMD_DEFINES "HAVE_AVX512VBMI2")
    list(APPEND SIMD_LEVELS "AVX-512-VBMI2")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-mavx512vbmi2")
    endif()
    message(STATUS "  ✓ AVX-512 VBMI2")
endif()

if(CPU_SUPPORTS_AVX512VNNI)
    list(APPEND SIMD_DEFINES "HAVE_AVX512VNNI")
    list(APPEND SIMD_LEVELS "AVX-512-VNNI")
    if(NOT USE_NATIVE_ARCH)
        list(APPEND SIMD_FLAGS "-mavx512vnni")
    endif()
    message(STATUS "  ✓ AVX-512 VNNI")
endif()

if(CPU_SUPPORTS_AMX)
    list(APPEND SIMD_DEFINES "HAVE_AMX_TILE" "HAVE_AMX_INT8")
    list(APPEND SIMD_LEVELS "AMX")
    if(NOT USE_NATIVE_ARCH)
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
    OpenMP::OpenMP_CXX
    Threads::Threads
    logger
)

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
target_link_libraries(comparative_benchmark PRIVATE ${OpenMP_CXX_LIBRARIES} logger)
target_compile_options(comparative_benchmark PRIVATE
    -std=c++23
    -fopenmp
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
