cmake_minimum_required(VERSION 3.28)

# ============================================================================
# Toolchain Configuration (before project())
# ============================================================================
# Configurable toolchain paths with fallback detection
# Can be overridden via: cmake -DCLANG_DEFAULT_PATH=/custom/path ...
# Or via environment: CC=/custom/clang CXX=/custom/clang++ cmake ...
# These variables are typically inherited from parent project when using FetchContent

# Default toolchain paths (can be overridden)
set(CLANG_DEFAULT_PATH "/usr/local/bin" CACHE STRING "Default path for Clang binaries")
set(LIBCXX_DEFAULT_INCLUDE "/usr/local/include/c++/v1" CACHE STRING "Default path for libc++ headers")
set(LIBCXX_DEFAULT_LIB "/usr/local/lib" CACHE STRING "Default path for libc++ libraries")

project(cpp23-logger
    VERSION 1.0.0
    DESCRIPTION "Production-grade C++23 logger with Mustache templates and fluent API"
    LANGUAGES CXX
)

# ============================================================================
# Project Options
# ============================================================================

option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(ENABLE_CLANG_TIDY "Enable clang-tidy checks" OFF)
option(ENABLE_NATIVE_OPTIMIZATIONS "Enable native CPU optimizations (AVX-512, AVX2, etc) to match parent project" OFF)
option(ENABLE_OPENMP "Enable OpenMP parallel processing support" OFF)

# ============================================================================
# C++23 Standard Requirement
# ============================================================================

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Compiler Detection and Configuration
# ============================================================================

# Check for C++23 modules support
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "17.0")
        message(FATAL_ERROR "Clang 17.0 or higher required for C++23 modules support")
    endif()
    # Clang C++23 modules flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23 -stdlib=libc++")

    # Optional: Add native CPU optimizations if requested by parent project
    # Note: Use -march=native only (no explicit AVX512 flags) to allow runtime detection
    if(ENABLE_NATIVE_OPTIMIZATIONS)
        message(STATUS "Enabling native CPU optimizations for module compatibility")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mavx2 -mfma")
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
        message(FATAL_ERROR "GCC 14.0 or higher required for C++23 modules support")
    endif()
    # GCC C++23 modules flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23 -fmodules-ts")

    # Optional: Add native CPU optimizations if requested by parent project
    # Note: Use -march=native only (no explicit AVX512 flags) to allow runtime detection
    if(ENABLE_NATIVE_OPTIMIZATIONS)
        message(STATUS "Enabling native CPU optimizations for module compatibility")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mavx2 -mfma")
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.34")
        message(FATAL_ERROR "MSVC 19.34 (VS 2022 17.4) or higher required for C++23 modules")
    endif()
    # MSVC C++23 modules flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest /experimental:module")

else()
    message(FATAL_ERROR "Unsupported compiler. Requires Clang 17+, GCC 14+, or MSVC 19.34+")
endif()

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: C++23")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ============================================================================
# Clang-Tidy Configuration (Optional)
# ============================================================================
# Configurable clang-tidy search paths
set(CLANG_TIDY_SEARCH_PATHS "${CLANG_DEFAULT_PATH}" "/usr/lib/llvm-21/bin" "/usr/lib/llvm-19/bin" CACHE STRING "Search paths for clang-tidy")

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy-21 clang-tidy
        PATHS ${CLANG_TIDY_SEARCH_PATHS}
        DOC "Path to clang-tidy executable"
    )
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy requested but not found")
    endif()
endif()

# ============================================================================
# Logger Module Library
# ============================================================================

add_library(logger)

target_sources(logger
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            include/logger/logger.cppm
            include/logger/code_generator.cppm
)

target_include_directories(logger
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Thread support required for std::mutex
find_package(Threads REQUIRED)
target_link_libraries(logger PUBLIC Threads::Threads)

# Optional OpenMP support for parallel processing
if(ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "Enabling OpenMP support for parallel processing")
        target_compile_options(logger PRIVATE ${OpenMP_CXX_FLAGS})
        target_link_libraries(logger PUBLIC OpenMP::OpenMP_CXX)
    else()
        message(WARNING "OpenMP requested but not found")
    endif()
endif()

# Set module output directory and enable position-independent code for shared library linking
set_target_properties(logger PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# ============================================================================
# Tests
# ============================================================================

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Examples
# ============================================================================

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library and module files
install(TARGETS logger
    EXPORT loggerTargets
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/logger
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Generate and install package config files
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/loggerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/loggerConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logger
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/loggerConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/loggerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/loggerConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logger
)

install(EXPORT loggerTargets
    FILE loggerTargets.cmake
    NAMESPACE logger::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logger
)

# ============================================================================
# Package Information
# ============================================================================

message(STATUS "")
message(STATUS "cpp23-logger Configuration:")
message(STATUS "  Version:           ${PROJECT_VERSION}")
message(STATUS "  Build tests:       ${BUILD_TESTS}")
message(STATUS "  Build examples:    ${BUILD_EXAMPLES}")
message(STATUS "  Enable clang-tidy: ${ENABLE_CLANG_TIDY}")
message(STATUS "  Clang path:        ${CLANG_DEFAULT_PATH}")
message(STATUS "  libc++ include:    ${LIBCXX_DEFAULT_INCLUDE}")
message(STATUS "  libc++ lib:        ${LIBCXX_DEFAULT_LIB}")
message(STATUS "")
