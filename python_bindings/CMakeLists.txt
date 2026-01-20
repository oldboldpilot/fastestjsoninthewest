cmake_minimum_required(VERSION 3.28)
project(fastjson_python LANGUAGES CXX)

# ============================================================================
# C++ Standard & Modules Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Find Clang 21 if not already set (reusing project conventions)
if(NOT CMAKE_CXX_COMPILER AND EXISTS "/opt/clang-21/bin/clang++")
    set(CMAKE_CXX_COMPILER "/opt/clang-21/bin/clang++")
endif()

# ============================================================================
# Dependencies
# ============================================================================
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(Threads REQUIRED)

# OpenMP for parallel support
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    add_compile_options(${OpenMP_CXX_FLAGS})
else()
    message(WARNING "OpenMP not found, parallel features may be limited")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++)
endif()

# ============================================================================
# Standard Library Module
# ============================================================================
add_library(std_module STATIC)
target_include_directories(std_module PUBLIC /opt/clang-21/share/libc++/v1)
target_sources(std_module PUBLIC
    FILE_SET CXX_MODULES 
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../external/std
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/../external/std/std.cppm
)
target_compile_features(std_module PUBLIC cxx_std_23)
target_compile_options(std_module PRIVATE -Wno-error)

# ============================================================================
# Build fastjson Python Module
# ============================================================================

set(FASTJSON_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../modules")

# Define the extension module with binding source and implementation files
nanobind_add_module(fastjson 
    src/fastjson_bindings.cpp
    ${FASTJSON_MODULES_DIR}/fastjson.cpp
    ${FASTJSON_MODULES_DIR}/numa_allocator.cpp
    ${FASTJSON_MODULES_DIR}/fastjson_simd_api.cpp
    ${FASTJSON_MODULES_DIR}/fastjson_simd_multiregister.cpp
)

# Enable C++23 modules for the target
target_sources(fastjson PUBLIC
    FILE_SET CXX_MODULES 
    BASE_DIRS ${FASTJSON_MODULES_DIR}
    FILES
    ${FASTJSON_MODULES_DIR}/fastjson.cppm
    ${FASTJSON_MODULES_DIR}/fastjson_parallel.cppm
    ${FASTJSON_MODULES_DIR}/json_linq.cppm
    ${FASTJSON_MODULES_DIR}/json_template.cppm
    ${FASTJSON_MODULES_DIR}/fastjson_simd_multiregister.cppm
)

# Includes
target_include_directories(fastjson PRIVATE 
    ${FASTJSON_MODULES_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compile Definitions & Options
target_compile_definitions(fastjson PRIVATE 
    FASTJSON_ENABLE_SIMD
    FASTJSON_THREAD_SAFE
    FASTJSON_MULTIREGISTER_SIMD
    ${SIMD_DEFINES}
)

# Optimization Flags
target_compile_options(fastjson PRIVATE 
    -O3 
    -march=native 
    -funroll-loops
    -fvectorize
    -Wall -Wextra
    ${OpenMP_CXX_FLAGS}
)

# Link Libraries
target_link_libraries(fastjson PRIVATE 
    Threads::Threads
    std_module
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(fastjson PRIVATE OpenMP::OpenMP_CXX)
endif()

# Install rule
install(TARGETS fastjson LIBRARY DESTINATION .)
