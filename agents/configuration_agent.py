#!/usr/bin/env python3
"""
Configuration Agent for FastestJSONInTheWest

This agent is responsible for managing build system configurations including:
- Automatic CMakeLists.txt updates for source-built dependencies
- Environment variable integration
- Cross-platform build configuration
- Dependency path management
- Custom find modules generation
"""

import os
import re
import json
from pathlib import Path
from typing import Dict, List, Any, Optional
from dataclasses import dataclass
from agent_orchestrator import Agent, Task, TaskType

@dataclass
class DependencyPath:
    """Path configuration for a source-built dependency."""
    name: str
    root_path: str
    include_dirs: List[str]
    library_dirs: List[str]
    libraries: List[str]
    cmake_config_path: Optional[str] = None
    pkg_config_path: Optional[str] = None

class ConfigurationAgent(Agent):
    """Agent responsible for build system configuration management."""
    
    def __init__(self, orchestrator):
        super().__init__("ConfigurationAgent", orchestrator)
        self.project_root = orchestrator.project_root
        self.cmake_dir = self.project_root / "cmake"
        self.cmake_modules_dir = self.cmake_dir / "modules"
        
        # Ensure directories exist
        self.cmake_dir.mkdir(exist_ok=True)
        self.cmake_modules_dir.mkdir(exist_ok=True)
        
        # Source installation base path
        self.source_install_base = "/opt/fastjson"
        
        # Dependency configurations for source builds
        self.source_dependency_configs = {
            "llvm": DependencyPath(
                name="LLVM",
                root_path=f"{self.source_install_base}/llvm",
                include_dirs=["include"],
                library_dirs=["lib"],
                libraries=["clang", "clang-cpp", "LLVM"],
                cmake_config_path="lib/cmake/llvm",
            ),
            "openmpi": DependencyPath(
                name="MPI",
                root_path=f"{self.source_install_base}/openmpi",
                include_dirs=["include"],
                library_dirs=["lib"],
                libraries=["mpi", "mpi_cxx"],
                pkg_config_path="lib/pkgconfig"
            ),
            "zeromq": DependencyPath(
                name="ZeroMQ",
                root_path=f"{self.source_install_base}/zeromq",
                include_dirs=["include"],
                library_dirs=["lib"],
                libraries=["zmq"],
                cmake_config_path="lib/cmake/ZeroMQ",
                pkg_config_path="lib/pkgconfig"
            ),
            "googletest": DependencyPath(
                name="GTest",
                root_path=f"{self.source_install_base}/googletest",
                include_dirs=["include"],
                library_dirs=["lib", "lib64"],
                libraries=["gtest", "gtest_main", "gmock", "gmock_main"],
                cmake_config_path="lib/cmake/GTest"
            ),
            "imgui": DependencyPath(
                name="ImGui",
                root_path=f"{self.source_install_base}/imgui",
                include_dirs=["include"],
                library_dirs=["lib"],
                libraries=["imgui"],
                cmake_config_path="lib/cmake/imgui"
            )
        }
    
    async def handle_task(self, task: Task) -> Dict[str, Any]:
        """Handle configuration-related tasks."""
        try:
            action = task.parameters.get("action")
            
            if action == "update_cmake_dependencies":
                return await self._update_cmake_dependencies(task.parameters)
            elif action == "generate_find_modules":
                return await self._generate_find_modules(task.parameters)
            elif action == "update_environment_config":
                return await self._update_environment_config(task.parameters)
            elif action == "validate_dependencies":
                return await self._validate_dependencies(task.parameters)
            else:
                return {"success": False, "error": f"Unknown action: {action}"}
                
        except Exception as e:
            self.logger.error(f"Error handling task: {e}")
            return {"success": False, "error": str(e)}
    
    async def _update_cmake_dependencies(self, parameters: Dict[str, Any]) -> Dict[str, Any]:
        """Update CMakeLists.txt with source-built dependency configurations."""
        
        cmake_file = self.project_root / "CMakeLists.txt"
        source_installations = parameters.get("source_installations", True)
        custom_paths = parameters.get("custom_paths", {})
        
        # Read existing CMakeLists.txt
        if cmake_file.exists():
            with open(cmake_file, 'r') as f:
                content = f.read()
        else:
            content = self._generate_base_cmake()
        
        # Generate source-built dependency configuration
        if source_installations:
            dependency_config = self._generate_source_dependency_config(custom_paths)
            
            # Remove existing FastJSON dependency configuration
            content = re.sub(
                r'# BEGIN FASTJSON AUTO-GENERATED.*?# END FASTJSON AUTO-GENERATED',
                '',
                content,
                flags=re.DOTALL
            )
            
            # Insert new configuration
            config_section = f"""
# BEGIN FASTJSON AUTO-GENERATED CONFIGURATION
# This section is automatically managed by the Configuration Agent
# Do not modify manually

{dependency_config}

# END FASTJSON AUTO-GENERATED CONFIGURATION
"""
            
            # Find insertion point (after project declaration)
            project_match = re.search(r'project\s*\([^)]*\)', content, re.IGNORECASE)
            if project_match:
                insert_pos = project_match.end()
                content = content[:insert_pos] + config_section + content[insert_pos:]
            else:
                # If no project found, add at the beginning
                content = config_section + content
        
        # Write updated CMakeLists.txt
        with open(cmake_file, 'w') as f:
            f.write(content)
        
        # Generate custom find modules
        find_modules_result = await self._generate_find_modules({})
        
        return {
            "success": True,
            "cmake_file": str(cmake_file),
            "dependency_paths": {name: config.root_path for name, config in self.source_dependency_configs.items()},
            "find_modules": find_modules_result.get("modules", []),
            "source_installations": source_installations
        }
    
    def _generate_base_cmake(self) -> str:
        """Generate a basic CMakeLists.txt template."""
        return '''cmake_minimum_required(VERSION 3.20)
project(FastestJSONInTheWest
    VERSION 1.0.0
    DESCRIPTION "The fastest JSON library in the west"
    LANGUAGES CXX
)

# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable modules (experimental)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
    set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "aa1f7df0-828a-4fcd-9afc-2dc80491aca7")
    set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP ON)
endif()

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific settings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++)
endif()

# Enable SIMD optimizations
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    add_compile_options(-march=native)
endif()

# FastJSON library sources will be added here
# add_subdirectory(src)

# Tests
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples and benchmarks
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
'''
    
    def _generate_source_dependency_config(self, custom_paths: Dict[str, str]) -> str:
        """Generate CMake configuration for source-built dependencies."""
        
        config_lines = [
            "# Source-built dependency configuration",
            "set(FASTJSON_SOURCE_BUILT ON)",
            f"set(FASTJSON_ROOT \"{self.source_install_base}\")",
            ""
        ]
        
        # Set root paths for each dependency
        for name, dep_config in self.source_dependency_configs.items():
            root_path = custom_paths.get(name, dep_config.root_path)
            config_lines.extend([
                f"set({dep_config.name}_ROOT \"{root_path}\")",
                f"set({name.upper()}_ROOT \"{root_path}\")",
            ])
        
        config_lines.extend([
            "",
            "# Add custom find modules path",
            "list(APPEND CMAKE_MODULE_PATH \"${CMAKE_SOURCE_DIR}/cmake/modules\")",
            "",
            "# Environment-specific compiler configuration",
            "if(FASTJSON_SOURCE_BUILT)",
        ])
        
        # Compiler configuration for LLVM
        llvm_config = self.source_dependency_configs["llvm"]
        config_lines.extend([
            f"    set(CMAKE_C_COMPILER \"{llvm_config.root_path}/bin/clang\")",
            f"    set(CMAKE_CXX_COMPILER \"{llvm_config.root_path}/bin/clang++\")",
            f"    set(CMAKE_CXX_FLAGS \"${{CMAKE_CXX_FLAGS}} -stdlib=libc++\")",
            f"    set(CMAKE_EXE_LINKER_FLAGS \"${{CMAKE_EXE_LINKER_FLAGS}} -stdlib=libc++\")",
            "endif()",
            ""
        ])
        
        # Find package configurations
        config_lines.extend([
            "# Find source-built dependencies",
            "find_package(LLVM REQUIRED CONFIG HINTS ${LLVM_ROOT})",
            "find_package(MPI REQUIRED HINTS ${OPENMPI_ROOT})",
            "find_package(PkgConfig REQUIRED)",
        ])
        
        # pkg-config configurations
        for name, dep_config in self.source_dependency_configs.items():
            if dep_config.pkg_config_path:
                config_lines.extend([
                    f"set(ENV{{PKG_CONFIG_PATH}} \"${{ENV{{PKG_CONFIG_PATH}}}}:{dep_config.root_path}/{dep_config.pkg_config_path}\")",
                ])
        
        config_lines.extend([
            "pkg_check_modules(ZMQ REQUIRED libzmq)",
            "",
            "# Find testing framework",
            "find_package(GTest REQUIRED HINTS ${GOOGLETEST_ROOT})",
            "",
            "# Find GUI framework", 
            "find_package(ImGui REQUIRED HINTS ${IMGUI_ROOT})",
            "",
            "# Create interface targets for easier linking",
            "if(TARGET LLVM::LLVM)",
            "    add_library(FastJSON::LLVM ALIAS LLVM::LLVM)",
            "endif()",
            "",
            "if(TARGET MPI::MPI_CXX)",
            "    add_library(FastJSON::MPI ALIAS MPI::MPI_CXX)",
            "endif()",
            "",
            "if(ZMQ_FOUND)",
            "    add_library(FastJSON::ZeroMQ INTERFACE IMPORTED)",
            "    target_include_directories(FastJSON::ZeroMQ INTERFACE ${ZMQ_INCLUDE_DIRS})",
            "    target_link_libraries(FastJSON::ZeroMQ INTERFACE ${ZMQ_LIBRARIES})",
            "    target_compile_options(FastJSON::ZeroMQ INTERFACE ${ZMQ_CFLAGS_OTHER})",
            "endif()",
            "",
            "if(TARGET GTest::gtest)",
            "    add_library(FastJSON::GTest ALIAS GTest::gtest)",
            "    add_library(FastJSON::GTestMain ALIAS GTest::gtest_main)",
            "endif()",
            "",
            "# Print configuration summary",
            "message(STATUS \"FastestJSONInTheWest Configuration Summary:\")",
            "message(STATUS \"  LLVM Root: ${LLVM_ROOT}\")",
            "message(STATUS \"  OpenMPI Root: ${OPENMPI_ROOT}\")",
            "message(STATUS \"  ZeroMQ Root: ${ZMQ_ROOT}\")",
            "message(STATUS \"  GoogleTest Root: ${GOOGLETEST_ROOT}\")",
            "message(STATUS \"  ImGui Root: ${IMGUI_ROOT}\")",
            "message(STATUS \"  Source Built: ${FASTJSON_SOURCE_BUILT}\")",
        ])
        
        return "\\n".join(config_lines)
    
    async def _generate_find_modules(self, parameters: Dict[str, Any]) -> Dict[str, Any]:
        """Generate custom find modules for source-built dependencies."""
        
        generated_modules = []
        
        # Generate find module for ImGui (doesn't have standard CMake support)
        imgui_module = self._generate_imgui_find_module()
        imgui_file = self.cmake_modules_dir / "FindImGui.cmake"
        with open(imgui_file, 'w') as f:
            f.write(imgui_module)
        generated_modules.append(str(imgui_file))
        
        # Generate enhanced find module for ZeroMQ
        zeromq_module = self._generate_zeromq_find_module()
        zeromq_file = self.cmake_modules_dir / "FindZeroMQ.cmake"
        with open(zeromq_file, 'w') as f:
            f.write(zeromq_module)
        generated_modules.append(str(zeromq_file))
        
        return {
            "success": True,
            "modules": generated_modules,
            "cmake_modules_dir": str(self.cmake_modules_dir)
        }
    
    def _generate_imgui_find_module(self) -> str:
        """Generate CMake find module for ImGui."""
        return '''# FindImGui.cmake
# Find ImGui library
# This module defines:
#  ImGui_FOUND - True if ImGui is found
#  ImGui_INCLUDE_DIRS - Include directories for ImGui
#  ImGui_LIBRARIES - Libraries to link against
#  ImGui::ImGui - Imported target for ImGui

find_path(ImGui_INCLUDE_DIR
    NAMES imgui.h
    HINTS ${IMGUI_ROOT} ${ImGui_ROOT}
    PATH_SUFFIXES include
)

find_library(ImGui_LIBRARY
    NAMES imgui
    HINTS ${IMGUI_ROOT} ${ImGui_ROOT}
    PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ImGui
    REQUIRED_VARS ImGui_LIBRARY ImGui_INCLUDE_DIR
)

if(ImGui_FOUND)
    set(ImGui_LIBRARIES ${ImGui_LIBRARY})
    set(ImGui_INCLUDE_DIRS ${ImGui_INCLUDE_DIR})
    
    if(NOT TARGET ImGui::ImGui)
        add_library(ImGui::ImGui UNKNOWN IMPORTED)
        set_target_properties(ImGui::ImGui PROPERTIES
            IMPORTED_LOCATION "${ImGui_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ImGui_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(ImGui_INCLUDE_DIR ImGui_LIBRARY)
'''
    
    def _generate_zeromq_find_module(self) -> str:
        """Generate enhanced CMake find module for ZeroMQ."""
        return '''# FindZeroMQ.cmake
# Enhanced find module for ZeroMQ
# This module defines:
#  ZeroMQ_FOUND - True if ZeroMQ is found
#  ZeroMQ_INCLUDE_DIRS - Include directories for ZeroMQ
#  ZeroMQ_LIBRARIES - Libraries to link against
#  ZeroMQ_VERSION - Version of ZeroMQ found
#  ZeroMQ::libzmq - Imported target for ZeroMQ

find_path(ZeroMQ_INCLUDE_DIR
    NAMES zmq.h
    HINTS ${ZMQ_ROOT} ${ZeroMQ_ROOT}
    PATH_SUFFIXES include
)

find_library(ZeroMQ_LIBRARY
    NAMES zmq libzmq
    HINTS ${ZMQ_ROOT} ${ZeroMQ_ROOT}
    PATH_SUFFIXES lib
)

# Try to find version information
if(ZeroMQ_INCLUDE_DIR AND EXISTS "${ZeroMQ_INCLUDE_DIR}/zmq.h")
    file(STRINGS "${ZeroMQ_INCLUDE_DIR}/zmq.h" ZMQ_VERSION_MAJOR_LINE
         REGEX "^#define[\t ]+ZMQ_VERSION_MAJOR[\t ]+[0-9]+")
    file(STRINGS "${ZeroMQ_INCLUDE_DIR}/zmq.h" ZMQ_VERSION_MINOR_LINE
         REGEX "^#define[\t ]+ZMQ_VERSION_MINOR[\t ]+[0-9]+")
    file(STRINGS "${ZeroMQ_INCLUDE_DIR}/zmq.h" ZMQ_VERSION_PATCH_LINE
         REGEX "^#define[\t ]+ZMQ_VERSION_PATCH[\t ]+[0-9]+")
    
    string(REGEX REPLACE "^#define[\t ]+ZMQ_VERSION_MAJOR[\t ]+([0-9]+).*"
           "\\\\1" ZMQ_VERSION_MAJOR "${ZMQ_VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define[\t ]+ZMQ_VERSION_MINOR[\t ]+([0-9]+).*"
           "\\\\1" ZMQ_VERSION_MINOR "${ZMQ_VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define[\t ]+ZMQ_VERSION_PATCH[\t ]+([0-9]+).*"
           "\\\\1" ZMQ_VERSION_PATCH "${ZMQ_VERSION_PATCH_LINE}")
    
    set(ZeroMQ_VERSION "${ZMQ_VERSION_MAJOR}.${ZMQ_VERSION_MINOR}.${ZMQ_VERSION_PATCH}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZeroMQ
    REQUIRED_VARS ZeroMQ_LIBRARY ZeroMQ_INCLUDE_DIR
    VERSION_VAR ZeroMQ_VERSION
)

if(ZeroMQ_FOUND)
    set(ZeroMQ_LIBRARIES ${ZeroMQ_LIBRARY})
    set(ZeroMQ_INCLUDE_DIRS ${ZeroMQ_INCLUDE_DIR})
    
    if(NOT TARGET ZeroMQ::libzmq)
        add_library(ZeroMQ::libzmq UNKNOWN IMPORTED)
        set_target_properties(ZeroMQ::libzmq PROPERTIES
            IMPORTED_LOCATION "${ZeroMQ_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ZeroMQ_INCLUDE_DIR}"
        )
        
        # Add threading support if required
        if(CMAKE_THREAD_LIBS_INIT)
            set_property(TARGET ZeroMQ::libzmq APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES "${CMAKE_THREAD_LIBS_INIT}")
        endif()
    endif()
endif()

mark_as_advanced(ZeroMQ_INCLUDE_DIR ZeroMQ_LIBRARY)
'''
    
    async def _update_environment_config(self, parameters: Dict[str, Any]) -> Dict[str, Any]:
        """Update environment configuration files."""
        
        env_file = Path("/etc/environment")
        bashrc_file = Path.home() / ".bashrc"
        
        # Environment variables to set
        env_vars = {
            "FASTJSON_ROOT": self.source_install_base,
            "LLVM_ROOT": f"{self.source_install_base}/llvm",
            "OPENMPI_ROOT": f"{self.source_install_base}/openmpi", 
            "ZMQ_ROOT": f"{self.source_install_base}/zeromq",
            "GTEST_ROOT": f"{self.source_install_base}/googletest",
            "IMGUI_ROOT": f"{self.source_install_base}/imgui"
        }
        
        # Update PATH
        path_additions = [
            f"{self.source_install_base}/llvm/bin",
            f"{self.source_install_base}/openmpi/bin"
        ]
        
        # Update LD_LIBRARY_PATH
        lib_path_additions = [
            f"{self.source_install_base}/llvm/lib",
            f"{self.source_install_base}/openmpi/lib",
            f"{self.source_install_base}/zeromq/lib",
            f"{self.source_install_base}/googletest/lib"
        ]
        
        # Update PKG_CONFIG_PATH
        pkg_config_additions = [
            f"{self.source_install_base}/openmpi/lib/pkgconfig",
            f"{self.source_install_base}/zeromq/lib/pkgconfig"
        ]
        
        updated_files = []
        
        # Generate environment script
        env_script = self._generate_environment_script(env_vars, path_additions, lib_path_additions, pkg_config_additions)
        env_script_file = self.project_root / "setup_environment.sh"
        with open(env_script_file, 'w') as f:
            f.write(env_script)
        env_script_file.chmod(0o755)
        updated_files.append(str(env_script_file))
        
        return {
            "success": True,
            "environment_variables": env_vars,
            "path_additions": path_additions,
            "updated_files": updated_files,
            "environment_script": str(env_script_file)
        }
    
    def _generate_environment_script(self, env_vars: Dict[str, str], path_additions: List[str], 
                                   lib_path_additions: List[str], pkg_config_additions: List[str]) -> str:
        """Generate shell script to set up FastJSON environment."""
        
        script_lines = [
            "#!/bin/bash",
            "# FastestJSONInTheWest Environment Setup Script",
            "# Source this script to set up your development environment",
            "",
            "# FastJSON environment variables"
        ]
        
        for var, value in env_vars.items():
            script_lines.append(f'export {var}="{value}"')
        
        script_lines.extend([
            "",
            "# Update PATH",
            f'export PATH="$PATH:{":".join(path_additions)}"',
            "",
            "# Update library path",
            f'export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:{":".join(lib_path_additions)}"',
            "",
            "# Update pkg-config path",
            f'export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:{":".join(pkg_config_additions)}"',
            "",
            "# Compiler preferences",
            f'export CC="{env_vars["LLVM_ROOT"]}/bin/clang"',
            f'export CXX="{env_vars["LLVM_ROOT"]}/bin/clang++"',
            "",
            "echo \"FastestJSONInTheWest environment configured!\"",
            "echo \"  LLVM/Clang: $($CC --version | head -n1)\"",
            "echo \"  OpenMPI: $(which mpicc 2>/dev/null && mpicc --version | head -n1 || echo 'Not found')\"",
            "echo \"  ZeroMQ: $(pkg-config --modversion libzmq 2>/dev/null || echo 'Not found')\"",
        ]
        
        return "\\n".join(script_lines)
    
    async def _validate_dependencies(self, parameters: Dict[str, Any]) -> Dict[str, Any]:
        """Validate that all source-built dependencies are correctly installed."""
        
        validation_results = {}
        overall_success = True
        
        for name, dep_config in self.source_dependency_configs.items():
            result = {
                "installed": False,
                "version": None,
                "paths_valid": False,
                "errors": []
            }
            
            # Check if root directory exists
            if not Path(dep_config.root_path).exists():
                result["errors"].append(f"Root path does not exist: {dep_config.root_path}")
                overall_success = False
            else:
                result["paths_valid"] = True
                
                # Check include directories
                for inc_dir in dep_config.include_dirs:
                    full_path = Path(dep_config.root_path) / inc_dir
                    if not full_path.exists():
                        result["errors"].append(f"Include directory missing: {full_path}")
                
                # Check library directories
                for lib_dir in dep_config.library_dirs:
                    full_path = Path(dep_config.root_path) / lib_dir
                    if not full_path.exists():
                        result["errors"].append(f"Library directory missing: {full_path}")
                
                if not result["errors"]:
                    result["installed"] = True
            
            validation_results[name] = result
        
        return {
            "success": overall_success,
            "validation_results": validation_results,
            "source_install_base": self.source_install_base
        }