# FastestJSONInTheWest CMake Find Module for Source-Compiled gRPC
# Author: Olumuyiwa Oluwasanmi
# Automatic detection of source-compiled gRPC in /usr/local/

#[=======================================================================[.rst:
FindgRPC_SourceCompiled
-----------------------

Find gRPC libraries compiled from source, typically installed in /usr/local/

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``gRPC::grpc++``
  The gRPC C++ library
``gRPC::grpc``
  The gRPC C library
``gRPC::grpc_cpp_plugin``
  The protoc plugin for gRPC C++

Result Variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``gRPC_FOUND``
  True if gRPC has been found
``gRPC_VERSION``
  The version of gRPC found
``gRPC_INCLUDE_DIRS``
  Include directories needed to use gRPC
``gRPC_LIBRARIES``
  Libraries needed to link to gRPC
``gRPC_DEFINITIONS``
  Compile definitions needed to use gRPC

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``gRPC_INCLUDE_DIR``
  The directory containing gRPC headers
``gRPC_LIBRARY``
  The path to the gRPC library
``gRPC_CPP_PLUGIN``
  The path to the gRPC C++ protoc plugin

#]=======================================================================]

# Set search paths for source-compiled installations
set(_gRPC_SEARCH_PATHS
    /usr/local
    /opt/local
    ${CMAKE_PREFIX_PATH}
)

# Find the main gRPC include directory
find_path(gRPC_INCLUDE_DIR
    NAMES grpcpp/grpcpp.h grpc++/grpc++.h
    PATHS ${_gRPC_SEARCH_PATHS}
    PATH_SUFFIXES include
    DOC "gRPC include directory"
)

# Find gRPC++ library
find_library(gRPC_LIBRARY_CPP
    NAMES grpc++
    PATHS ${_gRPC_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
    DOC "gRPC C++ library"
)

# Find gRPC library  
find_library(gRPC_LIBRARY
    NAMES grpc
    PATHS ${_gRPC_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
    DOC "gRPC C library"
)

# Find protobuf library (gRPC dependency)
find_library(Protobuf_LIBRARY
    NAMES protobuf
    PATHS ${_gRPC_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
    DOC "Protocol Buffers library"
)

# Find gRPC C++ plugin
find_program(gRPC_CPP_PLUGIN
    NAMES grpc_cpp_plugin
    PATHS ${_gRPC_SEARCH_PATHS}
    PATH_SUFFIXES bin
    DOC "gRPC C++ protoc plugin"
)

# Find protoc compiler
find_program(Protobuf_PROTOC_EXECUTABLE
    NAMES protoc
    PATHS ${_gRPC_SEARCH_PATHS}
    PATH_SUFFIXES bin
    DOC "Protocol Buffers compiler"
)

# Try to determine gRPC version
if(gRPC_INCLUDE_DIR AND EXISTS "${gRPC_INCLUDE_DIR}/grpcpp/version_info.h")
    file(STRINGS "${gRPC_INCLUDE_DIR}/grpcpp/version_info.h" _gRPC_VERSION_LINES
         REGEX "#define[ \t]+GRPC_VERSION_STRING")
    
    if(_gRPC_VERSION_LINES)
        string(REGEX REPLACE ".*#define[ \t]+GRPC_VERSION_STRING[ \t]+\"([^\"]+)\".*"
               "\\1" gRPC_VERSION "${_gRPC_VERSION_LINES}")
    endif()
endif()

# If version not found in header, try pkg-config
if(NOT gRPC_VERSION)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PC_gRPC QUIET grpc++)
        if(PC_gRPC_VERSION)
            set(gRPC_VERSION "${PC_gRPC_VERSION}")
        endif()
    endif()
endif()

# Set the include directories and libraries
if(gRPC_INCLUDE_DIR)
    set(gRPC_INCLUDE_DIRS "${gRPC_INCLUDE_DIR}")
endif()

set(gRPC_LIBRARIES "")
if(gRPC_LIBRARY_CPP)
    list(APPEND gRPC_LIBRARIES "${gRPC_LIBRARY_CPP}")
endif()
if(gRPC_LIBRARY)
    list(APPEND gRPC_LIBRARIES "${gRPC_LIBRARY}")
endif()
if(Protobuf_LIBRARY)
    list(APPEND gRPC_LIBRARIES "${Protobuf_LIBRARY}")
endif()

# Find additional system libraries that gRPC needs
find_library(_gRPC_SSL_LIBRARY ssl)
find_library(_gRPC_CRYPTO_LIBRARY crypto)
find_library(_gRPC_Z_LIBRARY z)

if(_gRPC_SSL_LIBRARY)
    list(APPEND gRPC_LIBRARIES "${_gRPC_SSL_LIBRARY}")
endif()
if(_gRPC_CRYPTO_LIBRARY)
    list(APPEND gRPC_LIBRARIES "${_gRPC_CRYPTO_LIBRARY}")
endif()
if(_gRPC_Z_LIBRARY)
    list(APPEND gRPC_LIBRARIES "${_gRPC_Z_LIBRARY}")
endif()

# Handle standard arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gRPC_SourceCompiled
    REQUIRED_VARS
        gRPC_LIBRARY_CPP
        gRPC_LIBRARY
        gRPC_INCLUDE_DIR
        gRPC_CPP_PLUGIN
        Protobuf_PROTOC_EXECUTABLE
    VERSION_VAR
        gRPC_VERSION
    FAIL_MESSAGE
        "Could not find source-compiled gRPC. Make sure it's installed in /usr/local/ or CMAKE_PREFIX_PATH."
)

# Create imported targets if found
if(gRPC_SourceCompiled_FOUND)
    set(gRPC_FOUND TRUE)
    
    # gRPC++ target
    if(NOT TARGET gRPC::grpc++)
        add_library(gRPC::grpc++ UNKNOWN IMPORTED)
        set_target_properties(gRPC::grpc++ PROPERTIES
            IMPORTED_LOCATION "${gRPC_LIBRARY_CPP}"
            INTERFACE_INCLUDE_DIRECTORIES "${gRPC_INCLUDE_DIRS}"
        )
        
        # Link dependencies
        if(gRPC_LIBRARY)
            set_target_properties(gRPC::grpc++ PROPERTIES
                INTERFACE_LINK_LIBRARIES "gRPC::grpc"
            )
        endif()
    endif()
    
    # gRPC C target
    if(NOT TARGET gRPC::grpc AND gRPC_LIBRARY)
        add_library(gRPC::grpc UNKNOWN IMPORTED)
        set_target_properties(gRPC::grpc PROPERTIES
            IMPORTED_LOCATION "${gRPC_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${gRPC_INCLUDE_DIRS}"
        )
        
        # Link system dependencies
        set(_grpc_link_libraries "")
        if(_gRPC_SSL_LIBRARY)
            list(APPEND _grpc_link_libraries "${_gRPC_SSL_LIBRARY}")
        endif()
        if(_gRPC_CRYPTO_LIBRARY)
            list(APPEND _grpc_link_libraries "${_gRPC_CRYPTO_LIBRARY}")
        endif()
        if(_gRPC_Z_LIBRARY)
            list(APPEND _grpc_link_libraries "${_gRPC_Z_LIBRARY}")
        endif()
        if(Protobuf_LIBRARY)
            list(APPEND _grpc_link_libraries "${Protobuf_LIBRARY}")
        endif()
        
        if(_grpc_link_libraries)
            set_target_properties(gRPC::grpc PROPERTIES
                INTERFACE_LINK_LIBRARIES "${_grpc_link_libraries}"
            )
        endif()
    endif()
    
    # gRPC C++ plugin target
    if(NOT TARGET gRPC::grpc_cpp_plugin AND gRPC_CPP_PLUGIN)
        add_executable(gRPC::grpc_cpp_plugin IMPORTED)
        set_target_properties(gRPC::grpc_cpp_plugin PROPERTIES
            IMPORTED_LOCATION "${gRPC_CPP_PLUGIN}"
        )
    endif()
    
    # Protoc target
    if(NOT TARGET protobuf::protoc AND Protobuf_PROTOC_EXECUTABLE)
        add_executable(protobuf::protoc IMPORTED)
        set_target_properties(protobuf::protoc PROPERTIES
            IMPORTED_LOCATION "${Protobuf_PROTOC_EXECUTABLE}"
        )
    endif()
    
    # Set compatibility variables
    set(gRPC_DEFINITIONS "")
    
    # Helper function to generate gRPC code
    function(grpc_generate_cpp SRCS HDRS GRPC_SRCS GRPC_HDRS)
        if(NOT ARGN)
            message(SEND_ERROR "Error: grpc_generate_cpp() called without any proto files")
            return()
        endif()

        set(${SRCS})
        set(${HDRS})
        set(${GRPC_SRCS})
        set(${GRPC_HDRS})
        
        foreach(FIL ${ARGN})
            get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
            get_filename_component(FIL_WE ${FIL} NAME_WE)
            get_filename_component(FIL_DIR ${ABS_FIL} DIRECTORY)
            
            list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc")
            list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h")
            list(APPEND ${GRPC_SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.cc")
            list(APPEND ${GRPC_HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.h")
            
            add_custom_command(
                OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc"
                       "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h"
                       "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.cc"
                       "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.h"
                COMMAND $<TARGET_FILE:protobuf::protoc>
                ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                     --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
                     --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
                     -I ${FIL_DIR}
                     ${ABS_FIL}
                DEPENDS ${ABS_FIL} protobuf::protoc gRPC::grpc_cpp_plugin
                COMMENT "Running gRPC C++ protocol buffer compiler on ${FIL}"
                VERBATIM
            )
        endforeach()
        
        set(${SRCS} ${${SRCS}} PARENT_SCOPE)
        set(${HDRS} ${${HDRS}} PARENT_SCOPE)
        set(${GRPC_SRCS} ${${GRPC_SRCS}} PARENT_SCOPE)
        set(${GRPC_HDRS} ${${GRPC_HDRS}} PARENT_SCOPE)
    endfunction()
    
    # Print found information
    if(NOT gRPC_SourceCompiled_FIND_QUIETLY)
        message(STATUS "Found source-compiled gRPC: ${gRPC_VERSION}")
        message(STATUS "  gRPC include dir: ${gRPC_INCLUDE_DIR}")
        message(STATUS "  gRPC C++ library: ${gRPC_LIBRARY_CPP}")
        message(STATUS "  gRPC C library: ${gRPC_LIBRARY}")
        message(STATUS "  gRPC C++ plugin: ${gRPC_CPP_PLUGIN}")
    endif()
endif()

# Mark as advanced
mark_as_advanced(
    gRPC_INCLUDE_DIR
    gRPC_LIBRARY_CPP
    gRPC_LIBRARY
    gRPC_CPP_PLUGIN
    Protobuf_LIBRARY
    Protobuf_PROTOC_EXECUTABLE
    _gRPC_SSL_LIBRARY
    _gRPC_CRYPTO_LIBRARY
    _gRPC_Z_LIBRARY
)