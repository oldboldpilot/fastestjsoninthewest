cmake_minimum_required(VERSION 3.15)
project(fastjson_python)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force Clang 21
set(CMAKE_CXX_COMPILER "/opt/clang-21/bin/clang++")

# Find Python
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Find pybind11
execute_process(
    COMMAND python3 -m pybind11 --cmake-dir
    OUTPUT_VARIABLE PYBIND11_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
find_package(pybind11 CONFIG REQUIRED)

# Include the main fastjson headers
include_directories(${CMAKE_SOURCE_DIR}/../modules)

# Create the extension module
pybind11_add_module(fastjson 
    src/fastjson_bindings.cpp
)

# Link with OpenMP
find_package(OpenMP REQUIRED)
target_link_libraries(fastjson PRIVATE OpenMP::OpenMP_CXX)

# Set output directory
set_target_properties(fastjson PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
