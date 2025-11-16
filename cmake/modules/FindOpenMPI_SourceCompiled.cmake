# FastestJSONInTheWest CMake Find Module for Source-Compiled OpenMPI
# Author: Olumuyiwa Oluwasanmi
# Automatic detection of source-compiled OpenMPI in /usr/local/

#[=======================================================================[.rst:
FindOpenMPI_SourceCompiled
--------------------------

Find OpenMPI libraries compiled from source, typically installed in /usr/local/

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``MPI::MPI_CXX``
  The MPI C++ library
``MPI::MPI_C``
  The MPI C library

Result Variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``MPI_FOUND``
  True if MPI has been found
``MPI_CXX_FOUND``
  True if MPI C++ bindings have been found
``MPI_VERSION``
  The version of MPI found
``MPI_CXX_INCLUDE_PATH``
  Include directories needed to use MPI C++
``MPI_CXX_LIBRARIES``
  Libraries needed to link to MPI C++

#]=======================================================================]

# Set search paths for source-compiled installations
set(_MPI_SEARCH_PATHS
    /usr/local
    /opt/local
    ${CMAKE_PREFIX_PATH}
)

# Find MPI executables first
find_program(MPI_CXX_COMPILER
    NAMES mpicxx mpiCC mpic++ mpiccxx
    PATHS ${_MPI_SEARCH_PATHS}
    PATH_SUFFIXES bin
    DOC "MPI C++ compiler wrapper"
)

find_program(MPI_C_COMPILER
    NAMES mpicc
    PATHS ${_MPI_SEARCH_PATHS}
    PATH_SUFFIXES bin
    DOC "MPI C compiler wrapper"
)

find_program(MPIEXEC_EXECUTABLE
    NAMES mpiexec mpirun
    PATHS ${_MPI_SEARCH_PATHS}
    PATH_SUFFIXES bin
    DOC "MPI execution command"
)

# Find MPI include directory
find_path(MPI_CXX_INCLUDE_PATH
    NAMES mpi.h
    PATHS ${_MPI_SEARCH_PATHS}
    PATH_SUFFIXES include
    DOC "MPI include directory"
)

# Find MPI libraries
find_library(MPI_CXX_LIBRARIES
    NAMES mpi_cxx mpi_mpifh mpi
    PATHS ${_MPI_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
    DOC "MPI C++ libraries"
)

find_library(MPI_C_LIBRARIES
    NAMES mpi
    PATHS ${_MPI_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
    DOC "MPI C library"
)

# Try to get MPI version from mpiexec
if(MPIEXEC_EXECUTABLE)
    execute_process(
        COMMAND ${MPIEXEC_EXECUTABLE} --version
        OUTPUT_VARIABLE _MPI_VERSION_OUTPUT
        ERROR_VARIABLE _MPI_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _MPI_VERSION_RESULT
    )
    
    if(_MPI_VERSION_RESULT EQUAL 0)
        # Extract version from output
        string(REGEX MATCH "([0-9]+\\.)+[0-9]+" MPI_VERSION "${_MPI_VERSION_OUTPUT}")
    endif()
endif()

# If version not found from mpiexec, try mpicc
if(NOT MPI_VERSION AND MPI_C_COMPILER)
    execute_process(
        COMMAND ${MPI_C_COMPILER} --version
        OUTPUT_VARIABLE _MPI_VERSION_OUTPUT
        ERROR_VARIABLE _MPI_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _MPI_VERSION_RESULT
    )
    
    if(_MPI_VERSION_RESULT EQUAL 0)
        string(REGEX MATCH "([0-9]+\\.)+[0-9]+" MPI_VERSION "${_MPI_VERSION_OUTPUT}")
    endif()
endif()

# Get compiler flags if we have the wrapper
if(MPI_CXX_COMPILER)
    execute_process(
        COMMAND ${MPI_CXX_COMPILER} --showme:compile
        OUTPUT_VARIABLE MPI_CXX_COMPILE_FLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    execute_process(
        COMMAND ${MPI_CXX_COMPILER} --showme:link
        OUTPUT_VARIABLE MPI_CXX_LINK_FLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

# Handle standard arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenMPI_SourceCompiled
    REQUIRED_VARS
        MPI_CXX_COMPILER
        MPI_CXX_INCLUDE_PATH
        MPIEXEC_EXECUTABLE
    VERSION_VAR
        MPI_VERSION
    FAIL_MESSAGE
        "Could not find source-compiled OpenMPI. Make sure it's installed in /usr/local/ or CMAKE_PREFIX_PATH."
)

# Set standard MPI variables for compatibility
if(OpenMPI_SourceCompiled_FOUND)
    set(MPI_FOUND TRUE)
    set(MPI_CXX_FOUND TRUE)
    
    # Create imported targets
    if(NOT TARGET MPI::MPI_CXX)
        add_library(MPI::MPI_CXX INTERFACE IMPORTED)
        
        if(MPI_CXX_INCLUDE_PATH)
            set_target_properties(MPI::MPI_CXX PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${MPI_CXX_INCLUDE_PATH}"
            )
        endif()
        
        if(MPI_CXX_LIBRARIES)
            set_target_properties(MPI::MPI_CXX PROPERTIES
                INTERFACE_LINK_LIBRARIES "${MPI_CXX_LIBRARIES}"
            )
        endif()
        
        if(MPI_CXX_COMPILE_FLAGS)
            set_target_properties(MPI::MPI_CXX PROPERTIES
                INTERFACE_COMPILE_OPTIONS "${MPI_CXX_COMPILE_FLAGS}"
            )
        endif()
    endif()
    
    if(NOT TARGET MPI::MPI_C AND MPI_C_LIBRARIES)
        add_library(MPI::MPI_C INTERFACE IMPORTED)
        
        if(MPI_CXX_INCLUDE_PATH)
            set_target_properties(MPI::MPI_C PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${MPI_CXX_INCLUDE_PATH}"
            )
        endif()
        
        set_target_properties(MPI::MPI_C PROPERTIES
            INTERFACE_LINK_LIBRARIES "${MPI_C_LIBRARIES}"
        )
    endif()
    
    # Set standard variables
    set(MPIEXEC_NUMPROC_FLAG "-n")
    
    # Print found information
    if(NOT OpenMPI_SourceCompiled_FIND_QUIETLY)
        message(STATUS "Found source-compiled OpenMPI: ${MPI_VERSION}")
        message(STATUS "  MPI C++ compiler: ${MPI_CXX_COMPILER}")
        message(STATUS "  MPI include path: ${MPI_CXX_INCLUDE_PATH}")
        message(STATUS "  MPI executor: ${MPIEXEC_EXECUTABLE}")
    endif()
endif()

# Mark as advanced
mark_as_advanced(
    MPI_CXX_COMPILER
    MPI_C_COMPILER
    MPIEXEC_EXECUTABLE
    MPI_CXX_INCLUDE_PATH
    MPI_CXX_LIBRARIES
    MPI_C_LIBRARIES
)