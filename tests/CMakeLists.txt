cmake_minimum_required(VERSION 3.20)
project(FastestJSONInTheWest_Tests CXX)

# Use system archiver tools (llvm-ar not available in current Clang build)
set(CMAKE_AR "/usr/bin/ar")
set(CMAKE_RANLIB "/usr/bin/ranlib")

# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(OpenMP)
find_package(GTest QUIET)

# Configure compiler for modules
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang C++23 module support - use system libstdc++
    add_compile_options(-std=c++23)
    
    # Enable OpenMP for Clang if available
    if(OpenMP_FOUND)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC C++23 module support
    add_compile_options(-fmodules-ts)
endif()

# Add optimization flags for performance tests
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Create the fastjson library with parallel SIMD support
add_library(fastjson_module STATIC
    ../modules/fastjson_parallel.cpp
)

target_compile_features(fastjson_module PUBLIC cxx_std_23)
target_include_directories(fastjson_module PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../modules)

# Link OpenMP to the module if available
if(OpenMP_FOUND)
    target_link_libraries(fastjson_module PUBLIC OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP enabled for fastjson_module")
else()
    message(WARNING "OpenMP not found - building without parallel support")
endif()

# Enable comprehensive SIMD optimizations
target_compile_definitions(fastjson_module PUBLIC
    FASTJSON_ENABLE_SIMD
    HAVE_SSE HAVE_SSE2 HAVE_SSE3 HAVE_SSSE3 HAVE_SSE41 HAVE_SSE42
    HAVE_AVX HAVE_AVX2 HAVE_AVX512F HAVE_AVX512BW HAVE_AVX512CD HAVE_AVX512DQ HAVE_AVX512VL
    HAVE_FMA HAVE_FMA4 HAVE_VNNI HAVE_AMX
)

# Set common test properties
set(TEST_COMMON_LIBRARIES fastjson_module)
if(OpenMP_FOUND)
    list(APPEND TEST_COMMON_LIBRARIES OpenMP::OpenMP_CXX)
endif()
if(GTest_FOUND)
    list(APPEND TEST_COMMON_LIBRARIES GTest::gtest GTest::gtest_main)
endif()

# ============================================================================
# PERFORMANCE TESTS - Comprehensive benchmarking (PRIORITY)
# ============================================================================
set(PERFORMANCE_TEST_SOURCES
    performance/large_file_benchmark.cpp
    performance/instruction_set_benchmark.cpp
    performance/simd_optimization_benchmark.cpp
    performance/parallel_scaling_benchmark.cpp
)

# Create individual performance test executables
foreach(test_file ${PERFORMANCE_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE ${TEST_COMMON_LIBRARIES})
    target_compile_definitions(${test_name} PRIVATE PERFORMANCE_TESTS)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../modules)
    # Enable maximum optimization for performance tests
    target_compile_options(${test_name} PRIVATE -O3 -march=native)
endforeach()

# Define all test targets for easy reference
set(TEST_TARGETS
    large_file_benchmark
    instruction_set_benchmark
    simd_optimization_benchmark
    parallel_scaling_benchmark
)

# Create convenience target to build all tests
add_custom_target(all_tests DEPENDS ${TEST_TARGETS})

# Create convenience target to run performance benchmarks
add_custom_target(run_benchmarks
    COMMAND $<TARGET_FILE:large_file_benchmark>
    COMMAND $<TARGET_FILE:instruction_set_benchmark>
    COMMAND $<TARGET_FILE:simd_optimization_benchmark>
    DEPENDS large_file_benchmark instruction_set_benchmark simd_optimization_benchmark
    COMMENT "Running FastestJSONInTheWest performance benchmarks"
)

# Set build type to Release for performance if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output configuration information
message(STATUS "=")
message(STATUS "FastestJSONInTheWest Test Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  CXX Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  CXX Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenMP: ${OpenMP_FOUND}")
message(STATUS "  Test Targets: ${TEST_TARGETS}")
message(STATUS "=")
message(STATUS "Available targets:")
message(STATUS "  all_tests        - Build all test executables")
message(STATUS "  run_benchmarks   - Build and run performance benchmarks")
foreach(test_file ${PERFORMANCE_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    message(STATUS "  ${test_name}")
endforeach()
message(STATUS "=")