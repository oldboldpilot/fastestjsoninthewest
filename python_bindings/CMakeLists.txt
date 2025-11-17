cmake_minimum_required(VERSION 3.15)
project(fastjson_python)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Clang - try custom build first, then system
if(NOT CMAKE_CXX_COMPILER)
    find_program(CLANG_CXX 
        NAMES clang++
        PATHS /home/muyiwa/clang-build/bin
        /opt/clang-21/bin
        /usr/bin
        NO_DEFAULT_PATH
    )
    if(CLANG_CXX)
        set(CMAKE_CXX_COMPILER ${CLANG_CXX})
    else()
        message(WARNING "Clang not found, using default C++ compiler")
    endif()
endif()

# Find Python
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Find pybind11 using the found Python interpreter
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m pybind11 --cmakedir
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

# Link with OpenMP from clang toolchain
set(OPENMP_LIB_PATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu")

# Find the OpenMP library
find_library(OPENMP_LIB NAMES omp PATHS ${OPENMP_LIB_PATH} HINTS ${OPENMP_LIB_PATH} NO_DEFAULT_PATH)

if(NOT OPENMP_LIB)
    message(FATAL_ERROR "OpenMP library not found at ${OPENMP_LIB_PATH}")
endif()

# Add OpenMP flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")

# Link the library
target_link_libraries(fastjson PRIVATE ${OPENMP_LIB})

# Set output directory
set_target_properties(fastjson PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
