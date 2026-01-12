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
find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(Threads REQUIRED)

# OpenMP for parallel support
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    add_compile_options(${OpenMP_CXX_FLAGS})
else()
    message(WARNING "OpenMP not found, parallel features may be limited")
endif()

# ============================================================================
# Build fastjson Python Module
# ============================================================================

# We need to compile the C++ modules we depend on
# In a real scenario with C++ modules, we might need to add the module sources directly
# or link against a pre-built library that exports them. 
# For simplicity in this binding, we'll compile the core modules here as part of the extension
# or assume they are header-only / module-interface-only for now, but strictly speaking
# we should link against the 'fastjson' target if it were available via find_package.
# Here we add sources directly to ensure self-contained build for the wheel.

set(FASTJSON_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../modules")

# Define the extension module
nanobind_add_module(fastjson 
    src/fastjson_bindings.cpp
    # Include core module sources to ensure they are compiled and available
    ${FASTJSON_MODULES_DIR}/fastjson.cppm
    ${FASTJSON_MODULES_DIR}/fastjson.cpp
    ${FASTJSON_MODULES_DIR}/fastjson_parallel.cppm
    ${FASTJSON_MODULES_DIR}/numa_allocator.cpp
    ${FASTJSON_MODULES_DIR}/json_linq.cppm
    ${FASTJSON_MODULES_DIR}/json_template.cppm
    # Add SIMD implementations
    ${FASTJSON_MODULES_DIR}/fastjson_simd_api.cpp
    ${FASTJSON_MODULES_DIR}/fastjson_simd_multiregister.cpp
)

# Enable C++23 modules for the target
target_sources(fastjson PUBLIC
    FILE_SET CXX_MODULES FILES
    src/fastjson_bindings.cpp  # It's a consumer, but allows scanning
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
    -flto=thin
    -Wall -Wextra
    ${OpenMP_CXX_FLAGS}
)

# Link Libraries
target_link_libraries(fastjson PRIVATE 
    nanobind::nanobind
    Threads::Threads
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(fastjson PRIVATE OpenMP::OpenMP_CXX)
endif()

# Install rule
install(TARGETS fastjson LIBRARY DESTINATION .)
