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
if(NOT CMAKE_CXX_COMPILER AND EXISTS "/usr/local/bin/clang++")
    set(CMAKE_CXX_COMPILER "/usr/local/bin/clang++")
endif()

# ============================================================================
# Dependencies
# ============================================================================
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(Threads REQUIRED)

# TBB for parallel support
find_package(TBB REQUIRED)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-stdlib=libc++ -fexperimental-library)
    add_link_options(-stdlib=libc++ -fexperimental-library)
    if(WIN32)
        set(CMAKE_CXX_MODULE_STD OFF)
        add_compile_definitions(SENSEN_NO_IMPORT_STD)
    endif()
endif()

# ============================================================================
# Standard Library Module
# ============================================================================
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

add_library(std_module STATIC)
target_include_directories(std_module PUBLIC 
    ${LIBCXX_INC}
    ${LIBCXX_TARGET_INC}
    ${LIBCXX_SHARE}
)
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
)

# Embed RPATH so libomp.so is found without LD_LIBRARY_PATH
set_target_properties(fastjson PROPERTIES
    BUILD_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    INSTALL_RPATH "/opt/clang-21/lib/x86_64-unknown-linux-gnu"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Link Libraries
target_link_libraries(fastjson PRIVATE 
    Threads::Threads
    std_module
    TBB::tbb
)

# Conditionally enable CUDA and compile GPU files
# Conditionally enable CUDA and compile GPU files
option(ENABLE_CUDA "Enable CUDA GPU acceleration" ON)
if(ENABLE_CUDA)
    enable_language(CUDA)
    if(WIN32)
        foreach(_var CMAKE_CUDA_COMPILE_OBJECT CMAKE_CUDA_DEVICE_LINK_LIBRARY CMAKE_CUDA_DEVICE_LINK_EXECUTABLE)
            if(DEFINED ${_var})
                string(REPLACE "-Xcompiler=-Fd<TARGET_COMPILE_PDB>,-FS" "-Xcompiler=-FS" ${_var}_NEW "${${_var}}")
                set(${_var} "${${_var}_NEW}")
            endif()
        endforeach()
        
        # Workaround for CMake 4.3+ bug where it groups host compiler options with spaces under -Xcompiler="..."
        # which NVCC splits on spaces under Windows/Ninja, causing compilation failures.
        foreach(_config RELEASE RELWITHDEBINFO MINSIZEREL DEBUG)
            if(CMAKE_CUDA_FLAGS_${_config})
                string(REPLACE "-Xcompiler=\"-O2 -Ob2\"" "-Xcompiler=-O2 -Xcompiler=-Ob2" CMAKE_CUDA_FLAGS_${_config} "${CMAKE_CUDA_FLAGS_${_config}}")
                string(REPLACE "-Xcompiler=\"-MD -O2 -Ob2\"" "-Xcompiler=-MD -Xcompiler=-O2 -Xcompiler=-Ob2" CMAKE_CUDA_FLAGS_${_config} "${CMAKE_CUDA_FLAGS_${_config}}")
                string(REPLACE "-Xcompiler=\"-MDd  -Ob0 -Od\"" "-Xcompiler=-MDd -Xcompiler=-Ob0 -Xcompiler=-Od" CMAKE_CUDA_FLAGS_${_config} "${CMAKE_CUDA_FLAGS_${_config}}")
                string(REPLACE "-Xcompiler=\"-MDd -Zi -Ob0 -Od\"" "-Xcompiler=-MDd -Xcompiler=-Zi -Xcompiler=-Ob0 -Xcompiler=-Od" CMAKE_CUDA_FLAGS_${_config} "${CMAKE_CUDA_FLAGS_${_config}}")
            endif()
        endforeach()
    endif()
    find_package(CUDAToolkit REQUIRED)
    
    add_library(fastjson_cuda STATIC ${FASTJSON_MODULES_DIR}/gpu/json_cuda.cu)
    
    set_target_properties(fastjson_cuda PROPERTIES
        CUDA_ARCHITECTURES "89;90;120"
        CUDA_STANDARD 20
        CUDA_STANDARD_REQUIRED ON
        COMPILE_OPTIONS ""
        POSITION_INDEPENDENT_CODE ON
    )
    
    if(CMAKE_CUDA_COMPILER_ID MATCHES "Clang")
        target_compile_options(fastjson_cuda PRIVATE
            $<$<COMPILE_LANGUAGE:CUDA>:
                -D_ALLOW_UNSUPPORTED_LIBCPP
            >
        )
    endif()
    
    
    target_include_directories(fastjson_cuda PRIVATE
        ${FASTJSON_MODULES_DIR}
    )
    
    target_link_libraries(fastjson_cuda PRIVATE
        CUDA::cudart
        CUDA::cublas
        CUDA::cuda_driver
    )
    
    target_sources(fastjson PRIVATE
        ${FASTJSON_MODULES_DIR}/gpu/json_gpu.cpp
    )
    
    target_compile_definitions(fastjson PRIVATE FASTJSON_ENABLE_CUDA)
    target_link_libraries(fastjson PRIVATE 
        fastjson_cuda
        CUDA::cudart 
        CUDA::cublas 
        CUDA::cuda_driver
    )
    
    set_source_files_properties(${FASTJSON_MODULES_DIR}/gpu/json_cuda.cu PROPERTIES
        CXX_SCAN_FOR_MODULES OFF
    )
endif()

# Install rule
install(TARGETS fastjson LIBRARY DESTINATION .)
