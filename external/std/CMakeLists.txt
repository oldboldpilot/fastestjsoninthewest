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

# Include paths for std/*.inc files
target_include_directories(std_module PUBLIC
    /opt/clang-21/include/c++/v1
    /opt/clang-21/include/x86_64-unknown-linux-gnu/c++/v1  
    /opt/clang-21/share/libc++/v1
)

# Use our custom libc++ at link time
target_compile_options(std_module PRIVATE 
    -stdlib=libc++
)
target_link_options(std_module PRIVATE 
    -stdlib=libc++
    -L/opt/clang-21/lib
)

# Required for shared libraries
set_target_properties(std_module PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "✓ std modules configured")
message(STATUS "  - std.cppm → ${CMAKE_BINARY_DIR}/external/std/CMakeFiles/std_module.dir/std.pcm")
message(STATUS "  - std.compat.cppm → ${CMAKE_BINARY_DIR}/external/std/CMakeFiles/std_module.dir/std.compat.pcm")