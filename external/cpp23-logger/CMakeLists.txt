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

# ============================================================================
# `import std` is NOT used by the logger's modules — their global module
# fragments textually #include the std headers, so no std module is ever
# required to build this library. CMAKE_CXX_MODULE_STD is forced OFF so an
# embedding parent that enables the experimental `import std` support globally
# does not drag a std-module build into the logger's targets.
#
# NOTE: enabling CMAKE_CXX_MODULE_STD does NOT provide a module dependency
# scanner for clang-cl — CMake has no scanning rule at all for Clang with the
# MSVC frontend (see the toolchain gate below).
# ============================================================================
set(CMAKE_CXX_MODULE_STD OFF)

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
option(BUILD_BENCHMARKS "Build micro-benchmarks (perf harness)" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy checks" OFF)
option(ENABLE_NATIVE_OPTIMIZATIONS "Enable native CPU optimizations (AVX-512, AVX2, etc) to match parent project" OFF)
option(ENABLE_OPENMP "Enable OpenMP parallel processing support" OFF)

# ============================================================================
# Configurable Library Settings (for embedding in parent projects)
# ============================================================================
# These options allow parent projects to customize the logger library to:
# 1. Prevent target name clashes when multiple projects use cpp23-logger
# 2. Control static vs shared library linking
# 3. Specify output directory for library files

# Target name prefix (e.g., "sgee_" -> target becomes "sgee_logger")
set(CPP23_LOGGER_TARGET_PREFIX "" CACHE STRING "Prefix for logger target name (e.g., 'sgee_' -> 'sgee_logger')")

# Shared vs static library build
option(CPP23_LOGGER_BUILD_SHARED "Build logger as shared library (default: follows BUILD_SHARED_LIBS)" OFF)

# Custom output directory for library files
set(CPP23_LOGGER_OUTPUT_DIR "" CACHE PATH "Custom output directory for logger library files")

# Derive the actual target name
set(CPP23_LOGGER_TARGET_NAME "${CPP23_LOGGER_TARGET_PREFIX}logger" CACHE STRING "Final logger target name" FORCE)

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
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # clang-cl on Windows: uses the MSVC STL (and the MSVC std module, std.ixx),
    # NOT libc++. Must use the cl-style driver flags; -stdlib=libc++ is invalid.
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "17.0")
        message(FATAL_ERROR "clang-cl 17.0 or higher required for C++23 modules support")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest /EHsc")

    # CMake ships no module-dependency scanner rule for Clang with the MSVC
    # frontend: the clang-scan-deps (p1689) rule is wired only for the GNU
    # driver, and clang-cl ignores MSVC's -scanDependencies flag. Without a
    # scanner rule, generation fails with "the compiler does not provide a way
    # to discover the import graph dependencies". Fail fast at configure time
    # with actionable guidance; the gate opens automatically if a future CMake
    # adds clang-cl scanning support.
    if(NOT CMAKE_CXX_SCANDEP_SOURCE)
        message(FATAL_ERROR
            "clang-cl cannot build C++23 named modules with this CMake "
            "(no module dependency scanner rule exists for the MSVC frontend). "
            "Configure with clang++.exe (GNU driver; targets the MSVC ABI/STL "
            "by default on Windows) or native MSVC cl.exe (VS 17.4+) instead.")
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "17.0")
        message(FATAL_ERROR "Clang 17.0 or higher required for C++23 modules support")
    endif()
    # Clang C++23 modules flags. On Windows, clang++ (GNU driver) targets the
    # MSVC ABI and uses the MSVC STL by default — -stdlib=libc++ does not apply.
    if(WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23 -stdlib=libc++")
    endif()

    # Optional: Add portable CPU optimizations if requested by parent project.
    # We deliberately do NOT use -march=native: it bakes in host-specific ISA
    # extensions, which breaks BMI/binary portability and cross-host
    # floating-point parity (the parent project's policy bans it for exactly this
    # reason). -march=x86-64-v3 is a stable baseline (AVX2+FMA+BMI) that matches
    # the parent's canonical flags; per-function [[gnu::target(...)]] is the way
    # to opt into AVX-512, not a global -m flag.
    if(ENABLE_NATIVE_OPTIMIZATIONS)
        message(STATUS "Enabling portable CPU optimizations (x86-64-v3) for module compatibility")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=x86-64-v3 -mtune=generic -mavx -mavx2 -mfma")
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
        message(FATAL_ERROR "GCC 14.0 or higher required for C++23 modules support")
    endif()
    # GCC C++23 modules flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23 -fmodules-ts")

    # Optional: Add portable CPU optimizations if requested by parent project.
    # We deliberately do NOT use -march=native: it bakes in host-specific ISA
    # extensions, which breaks BMI/binary portability and cross-host
    # floating-point parity (the parent project's policy bans it for exactly this
    # reason). -march=x86-64-v3 is a stable baseline (AVX2+FMA+BMI) that matches
    # the parent's canonical flags; per-function [[gnu::target(...)]] is the way
    # to opt into AVX-512, not a global -m flag.
    if(ENABLE_NATIVE_OPTIMIZATIONS)
        message(STATUS "Enabling portable CPU optimizations (x86-64-v3) for module compatibility")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=x86-64-v3 -mtune=generic -mavx -mavx2 -mfma")
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.34")
        message(FATAL_ERROR "MSVC 19.34 (VS 2022 17.4) or higher required for C++23 modules")
    endif()
    # Standard C++23 named modules: cl.exe >= 19.34 scans via -scanDependencies
    # (wired by CMake's MSVC rules). The retired /experimental:module flag must
    # NOT be passed. /EHsc matches the exception model the MSVC STL expects.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest /EHsc")

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
# Auto-Build Standard Library Module PCM (std.pcm)
# ============================================================================
# Build std.pcm with the SAME flags as the logger library to ensure binary
# compatibility. This PCM can be used by any code that links against the logger.

if(NOT CMAKE_CXX_MODULE_STD AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # ---- clang-cl on Windows: precompile the MSVC std module (std.ixx) ----
    # (Currently unreachable: the clang-cl scanner gate above fails the
    #  configure first. Kept for the day a CMake release adds clang-cl module
    #  scanning, at which point this provides the std PCM service for parents.)
    # clang-cl uses the MSVC STL, so the module source is MSVC's std.ixx /
    # std.compat.ixx (shipped under <VCToolsInstallDir>/modules), not libc++.
    if(NOT MSVC_MODULES_PATH)
        if(DEFINED ENV{VCToolsInstallDir} AND EXISTS "$ENV{VCToolsInstallDir}/modules/std.ixx")
            cmake_path(SET MSVC_MODULES_PATH NORMALIZE "$ENV{VCToolsInstallDir}/modules")
        else()
            message(FATAL_ERROR "clang-cl: could not find the MSVC std module (std.ixx). "
                                "Build from a Visual Studio dev shell (vcvars sets VCToolsInstallDir), "
                                "or pass -DMSVC_MODULES_PATH=<...\\VC\\Tools\\MSVC\\<ver>\\modules>.")
        endif()
    endif()

    set(STD_IXX "${MSVC_MODULES_PATH}/std.ixx")
    set(STD_COMPAT_IXX "${MSVC_MODULES_PATH}/std.compat.ixx")
    if(NOT EXISTS "${STD_IXX}")
        message(FATAL_ERROR "std.ixx not found at ${STD_IXX}. Set MSVC_MODULES_PATH correctly.")
    endif()

    set(STD_PCM "${CMAKE_CURRENT_BINARY_DIR}/modules/std.pcm")
    set(STD_COMPAT_PCM "${CMAKE_CURRENT_BINARY_DIR}/modules/std.compat.pcm")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/modules")

    set(STD_PCM_FLAGS /std:c++latest /EHsc)
    if(CMAKE_BUILD_TYPE MATCHES "Release")
        list(APPEND STD_PCM_FLAGS /O2)
    endif()

    add_custom_command(
        OUTPUT ${STD_PCM}
        COMMAND ${CMAKE_CXX_COMPILER} ${STD_PCM_FLAGS} --precompile ${STD_IXX} -o ${STD_PCM}
        DEPENDS ${STD_IXX}
        COMMENT "Precompiling MSVC std module for logger (std.pcm, clang-cl)"
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${STD_COMPAT_PCM}
        COMMAND ${CMAKE_CXX_COMPILER} ${STD_PCM_FLAGS}
                -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/modules
                --precompile ${STD_COMPAT_IXX} -o ${STD_COMPAT_PCM}
        DEPENDS ${STD_COMPAT_IXX} ${STD_PCM}
        COMMENT "Precompiling MSVC std.compat module for logger (clang-cl)"
        VERBATIM
    )
    add_custom_target(build_logger_std_modules DEPENDS ${STD_PCM} ${STD_COMPAT_PCM})

    message(STATUS "std module PCM auto-build enabled (clang-cl / MSVC std.ixx):")
    message(STATUS "  Module source: ${MSVC_MODULES_PATH}")
    set(LOGGER_STD_PCM "${STD_PCM}" CACHE PATH "Path to logger's precompiled std.pcm" FORCE)
    set(LOGGER_STD_COMPAT_PCM "${STD_COMPAT_PCM}" CACHE PATH "Path to logger's precompiled std.compat.pcm" FORCE)
    set(LOGGER_STD_PCM_FLAGS "${STD_PCM_FLAGS}" CACHE STRING "Compiler flags used for logger's std.pcm" FORCE)
    set(LOGGER_STD_PCM_DIR "${CMAKE_CURRENT_BINARY_DIR}/modules" CACHE PATH "Directory containing logger's PCM files" FORCE)
    mark_as_advanced(LOGGER_STD_PCM LOGGER_STD_COMPAT_PCM LOGGER_STD_PCM_FLAGS LOGGER_STD_PCM_DIR)

elseif(NOT CMAKE_CXX_MODULE_STD AND CMAKE_CXX_COMPILER_ID MATCHES "Clang"
       AND (NOT WIN32 OR LIBCXX_MODULES_PATH))
    # libc++ std.pcm precompile (a service exported to parent projects via
    # LOGGER_STD_PCM — the logger itself never imports std). On Windows, clang++
    # uses the MSVC STL and libc++ is normally absent, so this branch is skipped
    # unless LIBCXX_MODULES_PATH is passed explicitly.
    # Configurable path to libc++ std module source
    # Try multiple common locations, can be overridden via cmake -DLIBCXX_MODULES_PATH=...
    if(NOT LIBCXX_MODULES_PATH)
        # Prefer the libc++ that ships with the active compiler: its std.cppm
        # lives at <clang-prefix>/share/libc++/v1, where <clang-prefix> is two
        # directories up from the compiler binary (…/bin/clang++). This is what
        # makes the build work out-of-the-box for non-system toolchains
        # (Homebrew/Linuxbrew, custom LLVM installs) instead of only /usr/local.
        get_filename_component(_cxx_bindir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(_cxx_prefix "${_cxx_bindir}" DIRECTORY)
        set(_libcxx_from_compiler "${_cxx_prefix}/share/libc++/v1")
        if(EXISTS "${_libcxx_from_compiler}/std.cppm")
            set(LIBCXX_MODULES_PATH "${_libcxx_from_compiler}" CACHE STRING "Path to libc++ module sources")
        elseif(EXISTS "${LIBCXX_DEFAULT_INCLUDE}/std.cppm")
            set(LIBCXX_MODULES_PATH "${LIBCXX_DEFAULT_INCLUDE}" CACHE STRING "Path to libc++ module sources")
        elseif(EXISTS "/usr/local/share/libc++/v1/std.cppm")
            set(LIBCXX_MODULES_PATH "/usr/local/share/libc++/v1" CACHE STRING "Path to libc++ module sources")
        elseif(EXISTS "/usr/local/include/c++/v1/std.cppm")
            set(LIBCXX_MODULES_PATH "/usr/local/include/c++/v1" CACHE STRING "Path to libc++ module sources")
        else()
            message(FATAL_ERROR "Could not find libc++ module sources (looked for std.cppm under "
                "'${_libcxx_from_compiler}' and /usr/local). Please set -DLIBCXX_MODULES_PATH=<dir containing std.cppm>.")
        endif()
    endif()

    set(STD_CPPM "${LIBCXX_MODULES_PATH}/std.cppm")
    set(STD_COMPAT_CPPM "${LIBCXX_MODULES_PATH}/std.compat.cppm")

    if(NOT EXISTS "${STD_CPPM}")
        message(FATAL_ERROR "std.cppm not found at ${STD_CPPM}. Please set LIBCXX_MODULES_PATH correctly.")
    endif()

    # Output PCM files in modules/ subdirectory
    set(STD_PCM "${CMAKE_CURRENT_BINARY_DIR}/modules/std.pcm")
    set(STD_COMPAT_PCM "${CMAKE_CURRENT_BINARY_DIR}/modules/std.compat.pcm")

    # Create modules directory
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/modules")

    # Collect all compiler flags that will be used for the logger
    set(STD_PCM_FLAGS
        -std=c++23
        -stdlib=libc++
        -fPIC
    )

    # Add optimization flags if building in Release mode
    if(CMAKE_BUILD_TYPE MATCHES "Release")
        list(APPEND STD_PCM_FLAGS -O3)
    endif()

    # Add portable optimizations if enabled (must match logger compilation).
    # -ffast-math is excluded to maintain binary compatibility with logger.pcm,
    # and -march=native is excluded for cross-host portability / FP parity (see
    # the per-toolchain blocks above) — std.pcm must stay ABI-compatible with the
    # logger BMI across machines, so we use the same x86-64-v3 baseline.
    if(ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND STD_PCM_FLAGS -march=x86-64-v3 -mtune=generic -mavx -mavx2 -mfma)
    endif()

    # Add OpenMP flags if enabled (must match logger compilation)
    if(ENABLE_OPENMP)
        find_package(OpenMP QUIET)
        if(OpenMP_CXX_FOUND)
            list(APPEND STD_PCM_FLAGS -fopenmp=libomp)
        endif()
    endif()

    # Build std.pcm
    add_custom_command(
        OUTPUT ${STD_PCM}
        COMMAND ${CMAKE_CXX_COMPILER} ${STD_PCM_FLAGS}
                --precompile ${STD_CPPM} -o ${STD_PCM}
        DEPENDS ${STD_CPPM}
        COMMENT "Precompiling libc++ std module for logger (std.pcm)"
        VERBATIM
    )

    # Build std.compat.pcm (depends on std.pcm)
    add_custom_command(
        OUTPUT ${STD_COMPAT_PCM}
        COMMAND ${CMAKE_CXX_COMPILER} ${STD_PCM_FLAGS}
                -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/modules
                --precompile ${STD_COMPAT_CPPM} -o ${STD_COMPAT_PCM}
        DEPENDS ${STD_COMPAT_CPPM} ${STD_PCM}
        COMMENT "Precompiling libc++ std.compat module for logger (std.compat.pcm)"
        VERBATIM
    )

    # Custom target to build both PCM files
    add_custom_target(build_logger_std_modules
        DEPENDS ${STD_PCM} ${STD_COMPAT_PCM}
        COMMENT "Building standard library modules for logger"
    )

    message(STATUS "std module PCM auto-build enabled:")
    message(STATUS "  Module source: ${LIBCXX_MODULES_PATH}")
    message(STATUS "  Output: ${CMAKE_CURRENT_BINARY_DIR}/modules/")
    message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER}")
    message(STATUS "  Flags: ${STD_PCM_FLAGS}")
    message(STATUS "  OpenMP: ${ENABLE_OPENMP}")
    message(STATUS "  Native opts: ${ENABLE_NATIVE_OPTIMIZATIONS}")

    # Export variables for external projects (parent CMakeLists.txt)
    # These allow external code to use the same PCM with binary compatibility
    set(LOGGER_STD_PCM "${STD_PCM}" CACHE PATH "Path to logger's precompiled std.pcm" FORCE)
    set(LOGGER_STD_COMPAT_PCM "${STD_COMPAT_PCM}" CACHE PATH "Path to logger's precompiled std.compat.pcm" FORCE)
    set(LOGGER_STD_PCM_FLAGS "${STD_PCM_FLAGS}" CACHE STRING "Compiler flags used for logger's std.pcm" FORCE)
    set(LOGGER_STD_PCM_DIR "${CMAKE_CURRENT_BINARY_DIR}/modules" CACHE PATH "Directory containing logger's PCM files" FORCE)

    # Mark these as advanced so they don't clutter the CMake GUI
    mark_as_advanced(LOGGER_STD_PCM LOGGER_STD_COMPAT_PCM LOGGER_STD_PCM_FLAGS LOGGER_STD_PCM_DIR)
endif()

# ============================================================================
# Logger Module Library
# ============================================================================

# Determine library type based on configuration
if(CPP23_LOGGER_BUILD_SHARED)
    add_library(${CPP23_LOGGER_TARGET_NAME} SHARED)
    message(STATUS "Building ${CPP23_LOGGER_TARGET_NAME} as SHARED library")
else()
    add_library(${CPP23_LOGGER_TARGET_NAME} STATIC)
    message(STATUS "Building ${CPP23_LOGGER_TARGET_NAME} as STATIC library")
endif()

# Create an alias for the original 'logger' name if using a prefix
# This allows code to still use 'logger' target when linked
if(CPP23_LOGGER_TARGET_PREFIX)
    add_library(logger ALIAS ${CPP23_LOGGER_TARGET_NAME})
    message(STATUS "Created alias: logger -> ${CPP23_LOGGER_TARGET_NAME}")
endif()

target_sources(${CPP23_LOGGER_TARGET_NAME}
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            include/logger/logger.cppm
            include/logger/code_generator.cppm
)

# Enable C++23 module dependency scanning on THIS target only. clang-cl on
# Windows does not auto-enable scanning (unlike clang-on-Linux), so CMake errors
# that the target "has C++ sources that may use modules". Scoping it to the
# target (rather than the global CMAKE_CXX_SCAN_FOR_MODULES) avoids forcing
# scanning into CMake's feature-check try_compile projects, which breaks them.
set_target_properties(${CPP23_LOGGER_TARGET_NAME} PROPERTIES CXX_SCAN_FOR_MODULES ON)

target_include_directories(${CPP23_LOGGER_TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Thread support required for std::mutex
find_package(Threads REQUIRED)
target_link_libraries(${CPP23_LOGGER_TARGET_NAME} PUBLIC Threads::Threads)

# Optional OpenMP support for parallel processing
if(ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "Enabling OpenMP support for parallel processing")
        target_compile_options(${CPP23_LOGGER_TARGET_NAME} PRIVATE ${OpenMP_CXX_FLAGS})
        target_link_libraries(${CPP23_LOGGER_TARGET_NAME} PUBLIC OpenMP::OpenMP_CXX)
    else()
        message(WARNING "OpenMP requested but not found")
    endif()
endif()

# Determine output directory
if(CPP23_LOGGER_OUTPUT_DIR)
    set(_LOGGER_OUTPUT_DIR "${CPP23_LOGGER_OUTPUT_DIR}")
else()
    set(_LOGGER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib")
endif()

# Set module output directory and enable position-independent code for shared library linking
set_target_properties(${CPP23_LOGGER_TARGET_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY "${_LOGGER_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${_LOGGER_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${_LOGGER_OUTPUT_DIR}"
)

# Export the target name for parent projects
set(CPP23_LOGGER_LIBRARY_TARGET "${CPP23_LOGGER_TARGET_NAME}" CACHE STRING "Logger library target name" FORCE)

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

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library and module files
install(TARGETS ${CPP23_LOGGER_TARGET_NAME}
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

# install(EXPORT loggerTargets
#     FILE loggerTargets.cmake
#     NAMESPACE logger::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logger
# )

# ============================================================================
# Package Information
# ============================================================================

message(STATUS "")
message(STATUS "cpp23-logger Configuration:")
message(STATUS "  Version:           ${PROJECT_VERSION}")
message(STATUS "  Target name:       ${CPP23_LOGGER_TARGET_NAME}")
message(STATUS "  Library type:      ${CPP23_LOGGER_BUILD_SHARED}")
message(STATUS "  Output directory:  ${_LOGGER_OUTPUT_DIR}")
message(STATUS "  Build tests:       ${BUILD_TESTS}")
message(STATUS "  Build examples:    ${BUILD_EXAMPLES}")
message(STATUS "  Enable clang-tidy: ${ENABLE_CLANG_TIDY}")
message(STATUS "  Clang path:        ${CLANG_DEFAULT_PATH}")
message(STATUS "  libc++ include:    ${LIBCXX_DEFAULT_INCLUDE}")
message(STATUS "  libc++ lib:        ${LIBCXX_DEFAULT_LIB}")
message(STATUS "")
