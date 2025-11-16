# FastestJSONInTheWest CMake Find Module for Source-Compiled ZeroMQ
# Author: Olumuyiwa Oluwasanmi
# Automatic detection of source-compiled ZeroMQ in /usr/local/

#[=======================================================================[.rst:
FindZeroMQ_SourceCompiled
-------------------------

Find ZeroMQ libraries compiled from source, typically installed in /usr/local/

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``ZeroMQ::libzmq``
  The ZeroMQ library

Result Variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``ZeroMQ_FOUND``
  True if ZeroMQ has been found
``ZeroMQ_VERSION``
  The version of ZeroMQ found
``ZeroMQ_INCLUDE_DIRS``
  Include directories needed to use ZeroMQ
``ZeroMQ_LIBRARIES``
  Libraries needed to link to ZeroMQ

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``ZeroMQ_INCLUDE_DIR``
  The directory containing ZeroMQ headers
``ZeroMQ_LIBRARY``
  The path to the ZeroMQ library

#]=======================================================================]

# Set search paths for source-compiled installations
set(_ZeroMQ_SEARCH_PATHS
    /usr/local
    /opt/local
    ${CMAKE_PREFIX_PATH}
)

# Find ZeroMQ include directory
find_path(ZeroMQ_INCLUDE_DIR
    NAMES zmq.h
    PATHS ${_ZeroMQ_SEARCH_PATHS}
    PATH_SUFFIXES include
    DOC "ZeroMQ include directory"
)

# Find ZeroMQ library
find_library(ZeroMQ_LIBRARY
    NAMES zmq libzmq
    PATHS ${_ZeroMQ_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
    DOC "ZeroMQ library"
)

# Try to determine ZeroMQ version from header
if(ZeroMQ_INCLUDE_DIR AND EXISTS "${ZeroMQ_INCLUDE_DIR}/zmq.h")
    file(STRINGS "${ZeroMQ_INCLUDE_DIR}/zmq.h" _ZeroMQ_VERSION_LINES
         REGEX "#define[ \t]+ZMQ_VERSION_(MAJOR|MINOR|PATCH)")
    
    if(_ZeroMQ_VERSION_LINES)
        string(REGEX REPLACE ".*#define[ \t]+ZMQ_VERSION_MAJOR[ \t]+([0-9]+).*"
               "\\1" ZeroMQ_VERSION_MAJOR "${_ZeroMQ_VERSION_LINES}")
        string(REGEX REPLACE ".*#define[ \t]+ZMQ_VERSION_MINOR[ \t]+([0-9]+).*"
               "\\1" ZeroMQ_VERSION_MINOR "${_ZeroMQ_VERSION_LINES}")
        string(REGEX REPLACE ".*#define[ \t]+ZMQ_VERSION_PATCH[ \t]+([0-9]+).*"
               "\\1" ZeroMQ_VERSION_PATCH "${_ZeroMQ_VERSION_LINES}")
               
        if(ZeroMQ_VERSION_MAJOR AND ZeroMQ_VERSION_MINOR AND ZeroMQ_VERSION_PATCH)
            set(ZeroMQ_VERSION "${ZeroMQ_VERSION_MAJOR}.${ZeroMQ_VERSION_MINOR}.${ZeroMQ_VERSION_PATCH}")
        endif()
    endif()
endif()

# If version not found in header, try pkg-config
if(NOT ZeroMQ_VERSION)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PC_ZeroMQ QUIET libzmq)
        if(PC_ZeroMQ_VERSION)
            set(ZeroMQ_VERSION "${PC_ZeroMQ_VERSION}")
        endif()
    endif()
endif()

# Set the include directories and libraries
if(ZeroMQ_INCLUDE_DIR)
    set(ZeroMQ_INCLUDE_DIRS "${ZeroMQ_INCLUDE_DIR}")
endif()

if(ZeroMQ_LIBRARY)
    set(ZeroMQ_LIBRARIES "${ZeroMQ_LIBRARY}")
endif()

# Handle standard arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZeroMQ_SourceCompiled
    REQUIRED_VARS
        ZeroMQ_LIBRARY
        ZeroMQ_INCLUDE_DIR
    VERSION_VAR
        ZeroMQ_VERSION
    FAIL_MESSAGE
        "Could not find source-compiled ZeroMQ. Make sure it's installed in /usr/local/ or CMAKE_PREFIX_PATH."
)

# Create imported target if found
if(ZeroMQ_SourceCompiled_FOUND)
    set(ZeroMQ_FOUND TRUE)
    
    if(NOT TARGET ZeroMQ::libzmq)
        add_library(ZeroMQ::libzmq UNKNOWN IMPORTED)
        set_target_properties(ZeroMQ::libzmq PROPERTIES
            IMPORTED_LOCATION "${ZeroMQ_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ZeroMQ_INCLUDE_DIRS}"
        )
        
        # ZeroMQ might need pthread
        find_package(Threads QUIET)
        if(Threads_FOUND)
            set_target_properties(ZeroMQ::libzmq PROPERTIES
                INTERFACE_LINK_LIBRARIES Threads::Threads
            )
        endif()
    endif()
    
    # Set compatibility variables
    set(ZMQ_FOUND TRUE)
    set(ZMQ_INCLUDE_DIRS "${ZeroMQ_INCLUDE_DIRS}")
    set(ZMQ_LIBRARIES "${ZeroMQ_LIBRARIES}")
    set(ZMQ_VERSION "${ZeroMQ_VERSION}")
    
    # Print found information
    if(NOT ZeroMQ_SourceCompiled_FIND_QUIETLY)
        message(STATUS "Found source-compiled ZeroMQ: ${ZeroMQ_VERSION}")
        message(STATUS "  ZeroMQ include dir: ${ZeroMQ_INCLUDE_DIR}")
        message(STATUS "  ZeroMQ library: ${ZeroMQ_LIBRARY}")
    endif()
endif()

# Mark as advanced
mark_as_advanced(
    ZeroMQ_INCLUDE_DIR
    ZeroMQ_LIBRARY
)