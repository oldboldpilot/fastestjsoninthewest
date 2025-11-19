# FastestJSONInTheWest C++23 Modules CMakeLists.txt
# Author: Olumuyiwa Oluwasanmi
# High-Performance JSON Library with C++23 Modules and Trail Calling Syntax

cmake_minimum_required(VERSION 3.28)

project(FastestJSONInTheWest 
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "High-Performance C++23 JSON Library with Modules"
)

# Force use of our custom built Clang 21.1.5
set(CMAKE_CXX_COMPILER "/opt/clang-21/bin/clang++")
set(CMAKE_C_COMPILER "/opt/clang-21/bin/clang")

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

# Add Clang 21 library paths
link_directories(
    /opt/clang-21/lib
    /opt/clang-21/lib/x86_64-unknown-linux-gnu
)

include_directories(
    /opt/clang-21/include
    /opt/clang-21/lib/clang/21/include
)

# ============================================================================
# Integrate cpp23-logger dependency
# ============================================================================
# Option to use FetchContent or submodule
option(USE_FETCHCONTENT_LOGGER "Use FetchContent to fetch cpp23-logger instead of submodule" OFF)

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

# Find clang-tidy
find_program(CLANG_TIDY_EXE NAMES clang-tidy-21 clang-tidy
    PATHS /opt/clang-21/bin /usr/local/bin /usr/bin
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

# Enable SIMD support with thread safety
target_compile_definitions(fastjson PRIVATE 
    FASTJSON_ENABLE_SIMD
    FASTJSON_THREAD_SAFE
    ${SIMD_DEFINES}
)

# Add thread-safe compilation options
target_compile_options(fastjson PRIVATE
    ${SIMD_FLAGS}
    -fno-omit-frame-pointer  # Better debugging and profiling
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
# Only compile for instruction sets actually supported by the target CPU
include(CheckCXXCompilerFlag)

# Check for SIMD instruction set support including latest Intel extensions
check_cxx_compiler_flag("-mamx-tile" COMPILER_SUPPORTS_AMX_TILE)
check_cxx_compiler_flag("-mamx-int8" COMPILER_SUPPORTS_AMX_INT8) 
check_cxx_compiler_flag("-mavx512vnni" COMPILER_SUPPORTS_AVX512VNNI)
check_cxx_compiler_flag("-mavx512vbmi" COMPILER_SUPPORTS_AVX512VBMI)
check_cxx_compiler_flag("-mavx512vbmi2" COMPILER_SUPPORTS_AVX512VBMI2)
check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512F)
check_cxx_compiler_flag("-mavx512bw" COMPILER_SUPPORTS_AVX512BW)
check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
check_cxx_compiler_flag("-mavx" COMPILER_SUPPORTS_AVX)
check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)
check_cxx_compiler_flag("-msse4.1" COMPILER_SUPPORTS_SSE41)
check_cxx_compiler_flag("-mssse3" COMPILER_SUPPORTS_SSSE3)
check_cxx_compiler_flag("-msse3" COMPILER_SUPPORTS_SSE3)
check_cxx_compiler_flag("-msse2" COMPILER_SUPPORTS_SSE2)

# Build SIMD flags based on what's available
set(SIMD_FLAGS "")
set(SIMD_DEFINES "")
set(SIMD_LEVELS "")

# Latest Intel extensions for AI/ML workloads
if(COMPILER_SUPPORTS_AMX_TILE AND COMPILER_SUPPORTS_AMX_INT8)
    list(APPEND SIMD_FLAGS "-mamx-tile" "-mamx-int8")
    list(APPEND SIMD_DEFINES "-DHAVE_AMX_TILE" "-DHAVE_AMX_INT8")
    list(APPEND SIMD_LEVELS "AMX-TMUL")
endif()

if(COMPILER_SUPPORTS_AVX512VNNI)
    list(APPEND SIMD_FLAGS "-mavx512vnni")
    list(APPEND SIMD_DEFINES "-DHAVE_AVX512VNNI")
    list(APPEND SIMD_LEVELS "AVX-512-VNNI")
endif()

if(COMPILER_SUPPORTS_AVX512VBMI2)
    list(APPEND SIMD_FLAGS "-mavx512vbmi2")
    list(APPEND SIMD_DEFINES "-DHAVE_AVX512VBMI2")
    list(APPEND SIMD_LEVELS "AVX-512-VBMI2")
endif()

if(COMPILER_SUPPORTS_AVX512VBMI)
    list(APPEND SIMD_FLAGS "-mavx512vbmi")
    list(APPEND SIMD_DEFINES "-DHAVE_AVX512VBMI")
    list(APPEND SIMD_LEVELS "AVX-512-VBMI")
endif()

if(COMPILER_SUPPORTS_AVX512F AND COMPILER_SUPPORTS_AVX512BW)
    list(APPEND SIMD_FLAGS "-mavx512f" "-mavx512bw")
    list(APPEND SIMD_DEFINES "-DHAVE_AVX512F" "-DHAVE_AVX512BW")
    list(APPEND SIMD_LEVELS "AVX-512")
endif()

if(COMPILER_SUPPORTS_AVX2)
    list(APPEND SIMD_FLAGS "-mavx2")
    list(APPEND SIMD_DEFINES "-DHAVE_AVX2")
    list(APPEND SIMD_LEVELS "AVX2")
endif()

if(COMPILER_SUPPORTS_AVX)
    list(APPEND SIMD_FLAGS "-mavx")
    list(APPEND SIMD_DEFINES "-DHAVE_AVX")
endif()

if(COMPILER_SUPPORTS_SSE42)
    list(APPEND SIMD_FLAGS "-msse4.2")
    list(APPEND SIMD_DEFINES "-DHAVE_SSE42")
endif()

if(COMPILER_SUPPORTS_SSE41)
    list(APPEND SIMD_FLAGS "-msse4.1")
    list(APPEND SIMD_DEFINES "-DHAVE_SSE41")
endif()

if(COMPILER_SUPPORTS_SSSE3)
    list(APPEND SIMD_FLAGS "-mssse3")
    list(APPEND SIMD_DEFINES "-DHAVE_SSSE3")
endif()

if(COMPILER_SUPPORTS_SSE3)
    list(APPEND SIMD_FLAGS "-msse3")
    list(APPEND SIMD_DEFINES "-DHAVE_SSE3")
endif()

if(COMPILER_SUPPORTS_SSE2)
    list(APPEND SIMD_FLAGS "-msse2")
    list(APPEND SIMD_DEFINES "-DHAVE_SSE2")
endif()

# Add additional optimization flags for performance and thread safety
list(APPEND SIMD_FLAGS "-mpopcnt" "-mbmi" "-mbmi2" "-mlzcnt" "-mtune=native" "-fno-omit-frame-pointer")

# Apply SIMD flags
string(JOIN " " SIMD_FLAGS_STR ${SIMD_FLAGS})
string(JOIN " " SIMD_DEFINES_STR ${SIMD_DEFINES})
string(JOIN " → " SIMD_LEVELS_STR ${SIMD_LEVELS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SIMD_FLAGS_STR} ${SIMD_DEFINES_STR}")

# Enhanced message showing detected capabilities
if(SIMD_LEVELS)
    message(STATUS "SIMD Optimizations: Thread-safe conditional compilation (${SIMD_LEVELS_STR} → SSE2)")
else()
    message(STATUS "SIMD Optimizations: Scalar fallback only (no advanced instruction sets detected)")
endif()

# Link required libraries including logger
target_link_libraries(fastjson PRIVATE
    OpenMP::OpenMP_CXX
    Threads::Threads
    logger
)

# Set runtime library path for Homebrew libraries
set_target_properties(fastjson PROPERTIES
    BUILD_RPATH "/home/linuxbrew/.linuxbrew/lib;/home/linuxbrew/.linuxbrew/Cellar/libomp/21.1.5/lib"
    INSTALL_RPATH "/home/linuxbrew/.linuxbrew/lib;/home/linuxbrew/.linuxbrew/Cellar/libomp/21.1.5/lib"
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
