# C++23 std modules from locally-built libc++
# This resolves the LLVM 21.1.5 promote.h bug by providing
# precompiled standard library modules instead of mixing
# #include directives with import statements.

cmake_minimum_required(VERSION 3.28)

message(STATUS "Building C++23 std modules...")

# Create module library using FILE_SET CXX_MODULES
# This is CMake's native way to handle C++20/23 modules
add_library(std_module STATIC)

target_sources(std_module
    PUBLIC
        FILE_SET CXX_MODULES FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/std.cppm
        ${CMAKE_CURRENT_SOURCE_DIR}/std.compat.cppm
)

# Dynamically locate libc++ headers and library paths based on CMAKE_CXX_COMPILER
get_filename_component(COMPILER_REAL_PATH "${CMAKE_CXX_COMPILER}" REALPATH)
get_filename_component(COMPILER_DIR "${COMPILER_REAL_PATH}" DIRECTORY)
get_filename_component(LLVM_PREFIX "${COMPILER_DIR}" DIRECTORY)

set(LIBCXX_INC "${LLVM_PREFIX}/include/c++/v1")
set(LIBCXX_SHARE "${LLVM_PREFIX}/share/libc++/v1")
set(LIBCXX_TARGET_INC "${LLVM_PREFIX}/include/x86_64-unknown-linux-gnu/c++/v1")
set(LIBCXX_LIB_DIR "${LLVM_PREFIX}/lib")

# Fallbacks if compiler is in standard bin path or something else
if(NOT EXISTS "${LIBCXX_SHARE}/std/algorithm.inc")
    # Try typical paths
    if(EXISTS "/opt/clang-21/share/libc++/v1/std/algorithm.inc")
        set(LIBCXX_INC "/opt/clang-21/include/c++/v1")
        set(LIBCXX_SHARE "/opt/clang-21/share/libc++/v1")
        set(LIBCXX_TARGET_INC "/opt/clang-21/include/x86_64-unknown-linux-gnu/c++/v1")
        set(LIBCXX_LIB_DIR "/opt/clang-21/lib")
    elseif(EXISTS "/home/linuxbrew/.linuxbrew/Cellar/llvm/22.1.5/share/libc++/v1/std/algorithm.inc")
        set(LIBCXX_INC "/home/linuxbrew/.linuxbrew/Cellar/llvm/22.1.5/include/c++/v1")
        set(LIBCXX_SHARE "/home/linuxbrew/.linuxbrew/Cellar/llvm/22.1.5/share/libc++/v1")
        set(LIBCXX_TARGET_INC "/home/linuxbrew/.linuxbrew/Cellar/llvm/22.1.5/include/x86_64-unknown-linux-gnu/c++/v1")
        set(LIBCXX_LIB_DIR "/home/linuxbrew/.linuxbrew/Cellar/llvm/22.1.5/lib")
    endif()
endif()

message(STATUS "Dynamic libc++ detection:")
message(STATUS "  Headers: ${LIBCXX_INC}")
message(STATUS "  Share: ${LIBCXX_SHARE}")
message(STATUS "  Target include: ${LIBCXX_TARGET_INC}")
message(STATUS "  Lib: ${LIBCXX_LIB_DIR}")

# Include paths for std/*.inc files
target_include_directories(std_module PUBLIC
    ${LIBCXX_INC}
    ${LIBCXX_TARGET_INC}
    ${LIBCXX_SHARE}
)

# Use our custom libc++ at link time
target_compile_options(std_module PRIVATE 
    -stdlib=libc++
    -fstack-protector-strong
    -pthread
)
target_link_options(std_module PRIVATE 
    -stdlib=libc++
    -fstack-protector-strong
    -pthread
    -L${LIBCXX_LIB_DIR}
)

# Required for shared libraries
set_target_properties(std_module PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "✓ std modules configured")
message(STATUS "  - std.cppm → ${CMAKE_BINARY_DIR}/external/std/CMakeFiles/std_module.dir/std.pcm")
message(STATUS "  - std.compat.cppm → ${CMAKE_BINARY_DIR}/external/std/CMakeFiles/std_module.dir/std.compat.pcm")