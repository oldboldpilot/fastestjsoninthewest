cmake_minimum_required(VERSION 3.28)

# ============================================================================
# Toolchain Auto-Detection (before project())
# ============================================================================
# C++23 modules (`import std;`) use Clang 23 (official compiler) with libc++ on Unix/macOS.
# Ensure out-of-the-box `uv pip install .` / `uv run pytest` works even when
# CC/CXX are unset or point to a missing/GCC compiler in the shell environment.
if(NOT WIN32 AND NOT CMAKE_CXX_COMPILER)
    set(_fj_env_cxx "$ENV{CXX}")
    if(_fj_env_cxx AND (NOT EXISTS "${_fj_env_cxx}" OR _fj_env_cxx MATCHES "(^|/)(g\\+\\+|c\\+\\+)(-[0-9]+)?$"))
        unset(ENV{CXX})
        unset(ENV{CC})
        set(_fj_env_cxx "")
    endif()

    if(NOT _fj_env_cxx)
        find_program(_fj_clangxx
            NAMES clang++-23 clang++ clang++-22
            NAMES_PER_DIR
            HINTS
                /home/linuxbrew/.linuxbrew/bin
                /opt/clang-23/bin
                /usr/lib/llvm-23/bin
                /usr/local/bin
                /opt/homebrew/opt/llvm/bin
                /usr/local/opt/llvm/bin
        )
        if(_fj_clangxx)
            get_filename_component(_fj_clang_dir "${_fj_clangxx}" DIRECTORY)
            find_program(_fj_clang
                NAMES clang-23 clang clang-22
                NAMES_PER_DIR
                PATHS "${_fj_clang_dir}" NO_DEFAULT_PATH
            )
            set(CMAKE_CXX_COMPILER "${_fj_clangxx}" CACHE FILEPATH "C++ compiler" FORCE)
            if(_fj_clang)
                set(CMAKE_C_COMPILER "${_fj_clang}" CACHE FILEPATH "C compiler" FORCE)
            endif()
        endif()
    endif()
endif()

project(fastjson_python LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ToolchainBMIGuard.cmake)

# ============================================================================
# Options & Portable ISA Floor
# ============================================================================
option(ENABLE_NATIVE_OPTIMIZATIONS "Enable native CPU optimizations (-march=native)" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer, LeakSanitizer, and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_MSAN "Enable MemorySanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|x64)$")
    set(FASTJSON_IS_X86_64 TRUE)
    if(ENABLE_NATIVE_OPTIMIZATIONS)
        set(FASTJSON_ISA_FLOOR -march=native)
    else()
        set(FASTJSON_ISA_FLOOR -march=x86-64-v3 -mtune=generic)
    endif()
else()
    set(FASTJSON_IS_X86_64 FALSE)
    if(ENABLE_NATIVE_OPTIMIZATIONS)
        set(FASTJSON_ISA_FLOOR -mcpu=native)
    else()
        set(FASTJSON_ISA_FLOOR "")
    endif()
endif()

# ============================================================================
# C++ Standard & Modules Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# ============================================================================
# Dependencies
# ============================================================================
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(Threads REQUIRED)

# TBB for parallel support (system package first, FetchContent fallback)
find_package(TBB QUIET)
if(NOT TARGET TBB::tbb)
    message(STATUS "fastjson_python: system TBB not found; fetching oneTBB v2022.3.0...")
    include(FetchContent)
    FetchContent_Declare(
        TBB
        GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
        GIT_TAG        v2022.3.0
        GIT_SHALLOW    TRUE
    )
    set(TBB_TEST OFF CACHE BOOL "Don't build TBB tests" FORCE)
    set(TBB_EXAMPLES OFF CACHE BOOL "Don't build TBB examples" FORCE)
    set(TBB_STRICT OFF CACHE BOOL "Disable strict warnings" FORCE)
    FetchContent_MakeAvailable(TBB)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
        $<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>
    )
    add_link_options(
        $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
        $<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>
    )
    if(WIN32)
        set(CMAKE_CXX_MODULE_STD OFF)
        add_compile_definitions(SENSEN_NO_IMPORT_STD)
    endif()
endif()

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
elseif(ENABLE_MSAN)
    add_compile_options(
        -fsanitize=memory
        -fsanitize-memory-track-origins
        -fno-sanitize-memory-use-after-dtor
        -fno-omit-frame-pointer
        $<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/../cmake/msan_libcxx_compat.h>
    )
    add_link_options(-fsanitize=memory)
elseif(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# ============================================================================
# Standard Library Module
# ============================================================================
get_filename_component(COMPILER_REAL_PATH "${CMAKE_CXX_COMPILER}" REALPATH)
get_filename_component(COMPILER_DIR "${COMPILER_REAL_PATH}" DIRECTORY)
get_filename_component(LLVM_PREFIX "${COMPILER_DIR}" DIRECTORY)

set(LIBCXX_INC "${LLVM_PREFIX}/include/c++/v1")
set(LIBCXX_SHARE "${LLVM_PREFIX}/share/libc++/v1")
set(LIBCXX_LIB_DIR "${LLVM_PREFIX}/lib")

# Fallbacks if compiler is a wrapper or symlink outside LLVM prefix
if(NOT EXISTS "${LIBCXX_SHARE}/std/algorithm.inc")
    foreach(_fj_prefix
        "/home/linuxbrew/.linuxbrew/opt/llvm"
        "/opt/clang-23"
        "/usr/lib/llvm-23"
        "/opt/homebrew/opt/llvm"
    )
        if(EXISTS "${_fj_prefix}/share/libc++/v1/std/algorithm.inc")
            set(LIBCXX_INC "${_fj_prefix}/include/c++/v1")
            set(LIBCXX_SHARE "${_fj_prefix}/share/libc++/v1")
            set(LIBCXX_LIB_DIR "${_fj_prefix}/lib")
            break()
        endif()
    endforeach()
endif()

set(LIBCXX_EXTRA_INCS "")
foreach(_triple x86_64-unknown-linux-gnu aarch64-unknown-linux-gnu)
    if(EXISTS "${LLVM_PREFIX}/include/${_triple}/c++/v1")
        list(APPEND LIBCXX_EXTRA_INCS "${LLVM_PREFIX}/include/${_triple}/c++/v1")
    endif()
endforeach()

if(EXISTS "${LIBCXX_SHARE}/std.cppm" AND EXISTS "${LIBCXX_SHARE}/std.compat.cppm")
    set(FASTJSON_STD_CPPM "${LIBCXX_SHARE}/std.cppm")
    set(FASTJSON_STD_COMPAT_CPPM "${LIBCXX_SHARE}/std.compat.cppm")
    set(FASTJSON_STD_BASE_DIRS "${LIBCXX_SHARE}")
else()
    set(FASTJSON_STD_CPPM "${CMAKE_CURRENT_SOURCE_DIR}/../external/std/std.cppm")
    set(FASTJSON_STD_COMPAT_CPPM "${CMAKE_CURRENT_SOURCE_DIR}/../external/std/std.compat.cppm")
    set(FASTJSON_STD_BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../external/std")
endif()

add_library(std_module STATIC)
target_include_directories(std_module PUBLIC
    ${LIBCXX_INC}
    ${LIBCXX_EXTRA_INCS}
    ${LIBCXX_SHARE}
)
target_sources(std_module PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${FASTJSON_STD_BASE_DIRS}
    FILES
        ${FASTJSON_STD_CPPM}
        ${FASTJSON_STD_COMPAT_CPPM}
)
if(ENABLE_MSAN AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/msan_libcxx_shim.cpp")
    target_sources(std_module PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/msan_libcxx_shim.cpp")
    target_link_libraries(std_module PUBLIC ${CMAKE_DL_LIBS})
endif()
target_compile_features(std_module PUBLIC cxx_std_23)
target_compile_options(std_module PRIVATE -Wno-error -fPIC -fstack-protector-strong)
set_target_properties(std_module PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# Build fastjson Python Module
# ============================================================================
set(FASTJSON_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../modules")

nanobind_add_module(fastjson
    src/fastjson_bindings.cpp
    ${FASTJSON_MODULES_DIR}/numa_allocator.cpp
    ${FASTJSON_MODULES_DIR}/fastjson_simd_api.cpp
    ${FASTJSON_MODULES_DIR}/fastjson_simd_multiregister.cpp
    ${FASTJSON_MODULES_DIR}/gpu/json_gpu.cpp
)

set(FASTJSON_MODULE_FILES
    ${FASTJSON_MODULES_DIR}/fastjson.cppm
    ${FASTJSON_MODULES_DIR}/fastjson_parallel.cppm
    ${FASTJSON_MODULES_DIR}/json_linq.cppm
    ${FASTJSON_MODULES_DIR}/json_template.cppm
    ${FASTJSON_MODULES_DIR}/fastjson_simd_multiregister.cppm
)
if(FASTJSON_IS_X86_64)
    list(APPEND FASTJSON_MODULE_FILES
        ${FASTJSON_MODULES_DIR}/fastjson_multiregister_parser.cppm
        ${FASTJSON_MODULES_DIR}/fastjson_turbo.cppm
    )
    target_compile_definitions(fastjson PRIVATE FASTJSON_PYTHON_HAS_TURBO=1)
endif()

target_sources(fastjson PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${FASTJSON_MODULES_DIR}
    FILES ${FASTJSON_MODULE_FILES}
)

target_include_directories(fastjson PRIVATE
    ${FASTJSON_MODULES_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(fastjson PRIVATE
    FASTJSON_ENABLE_SIMD
    FASTJSON_THREAD_SAFE
    FASTJSON_MULTIREGISTER_SIMD
    ${SIMD_DEFINES}
)

if(NOT MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
    target_compile_options(fastjson PRIVATE
        -O3
        ${FASTJSON_ISA_FLOOR}
        -funroll-loops
        -fvectorize
        -fslp-vectorize
        -fstack-protector-strong
        -Wall -Wextra
    )
endif()

# Compute RPATH entries for libc++ and TBB
set(_fj_rpaths "${LIBCXX_LIB_DIR}")
foreach(_triple x86_64-unknown-linux-gnu aarch64-unknown-linux-gnu)
    if(EXISTS "${LIBCXX_LIB_DIR}/${_triple}")
        list(APPEND _fj_rpaths "${LIBCXX_LIB_DIR}/${_triple}")
    endif()
endforeach()
get_target_property(_tbb_loc TBB::tbb IMPORTED_LOCATION_RELEASE)
if(NOT _tbb_loc)
    get_target_property(_tbb_loc TBB::tbb IMPORTED_LOCATION)
endif()
if(_tbb_loc)
    get_filename_component(_tbb_dir "${_tbb_loc}" DIRECTORY)
    list(APPEND _fj_rpaths "${_tbb_dir}")
endif()
list(REMOVE_DUPLICATES _fj_rpaths)

set_target_properties(fastjson PROPERTIES
    BUILD_RPATH "${_fj_rpaths}"
    INSTALL_RPATH "${_fj_rpaths}"
    BUILD_WITH_INSTALL_RPATH TRUE
)

target_link_libraries(fastjson PRIVATE
    Threads::Threads
    std_module
    TBB::tbb
)

# ============================================================================
# Conditionally Enable CUDA GPU Acceleration (Auto-Detect nvcc)
# ============================================================================
if(NOT DEFINED ENABLE_CUDA)
    set(_fastjson_cuda_found OFF)
    foreach(_hint ${CMAKE_CUDA_COMPILER} $ENV{CUDACXX})
        if(_hint AND EXISTS "${_hint}")
            set(_fastjson_cuda_found ON)
        endif()
    endforeach()
    if(NOT _fastjson_cuda_found)
        find_program(_fastjson_nvcc NAMES nvcc
                     HINTS ${CUDAToolkit_ROOT} $ENV{CUDAToolkit_ROOT}
                           ${CUDA_PATH} $ENV{CUDA_PATH}
                           ${CUDA_HOME} $ENV{CUDA_HOME}
                           /usr/local/cuda
                     PATH_SUFFIXES bin)
        if(_fastjson_nvcc)
            set(_fastjson_cuda_found ON)
            set(CMAKE_CUDA_COMPILER "${_fastjson_nvcc}" CACHE FILEPATH "CUDA compiler" FORCE)
        endif()
    endif()
    option(ENABLE_CUDA "Enable CUDA GPU acceleration" ${_fastjson_cuda_found})
    message(STATUS "fastjson_python: ENABLE_CUDA auto-detected ${_fastjson_cuda_found}"
                   " (pass -DENABLE_CUDA=ON/OFF to override)")
else()
    option(ENABLE_CUDA "Enable CUDA GPU acceleration" ${ENABLE_CUDA})
endif()

if(ENABLE_CUDA)
    enable_language(CUDA)
    if(WIN32)
        foreach(_var CMAKE_CUDA_COMPILE_OBJECT CMAKE_CUDA_DEVICE_LINK_LIBRARY CMAKE_CUDA_DEVICE_LINK_EXECUTABLE)
            if(DEFINED ${_var})
                string(REPLACE "-Xcompiler=-Fd<TARGET_COMPILE_PDB>,-FS" "-Xcompiler=-FS" ${_var}_NEW "${${_var}}")
                set(${_var} "${${_var}_NEW}")
            endif()
        endforeach()
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
            $<$<COMPILE_LANGUAGE:CUDA>:-D_ALLOW_UNSUPPORTED_LIBCPP>
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
