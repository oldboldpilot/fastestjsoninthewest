#!/usr/bin/env python3
"""
Environment Agent for FastestJSONInTheWest

This agent is responsible for managing the development environment including:
- Generating and maintaining Ansible playbooks
- Installing and configuring Clang 21, OpenMPI, CMake, and other dependencies
- Environment setup and validation
- Dependency version management
- Platform-specific configurations

Key Responsibilities:
- Generate Ansible playbooks for automated environment setup
- Manage software dependencies and versions
- Platform detection and configuration
- Environment validation and health checks
"""

import asyncio
import platform
import subprocess
import yaml
import json
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple
from dataclasses import dataclass
import re
from agent_orchestrator import Agent, Task, TaskType

@dataclass
class DependencySpec:
    """Specification for a software dependency."""
    name: str
    version: str
    package_managers: Dict[str, str]  # manager -> package_name
    build_from_source: Optional[Dict[str, Any]] = None
    validation_command: Optional[str] = None
    environment_vars: Optional[Dict[str, str]] = None

class EnvironmentAgent(Agent):
    """Agent responsible for development environment management."""
    
    def __init__(self, orchestrator):
        super().__init__("EnvironmentAgent", orchestrator)
        self.project_root = orchestrator.project_root
        self.ansible_dir = self.project_root / "ansible"
        self.scripts_dir = self.project_root / "scripts"
        
        # Ensure directories exist
        self.ansible_dir.mkdir(exist_ok=True)
        self.scripts_dir.mkdir(exist_ok=True)
        
        # Platform detection
        self.platform_info = self._detect_platform()
        self.logger.info(f"Detected platform: {self.platform_info}")
        
        # Define dependencies - will be initialized after method definitions
        self.dependencies = {}
        
        # Initialize dependencies after methods are available
        self._initialize_dependencies()
    
    def _initialize_dependencies(self):
        """Initialize dependencies specification."""
        self.dependencies = self._get_dependencies_spec()
    
    def _detect_platform(self) -> Dict[str, str]:
        """Detect current platform and distribution including WSL and Unix variants."""
        info = {
            "system": platform.system(),
            "machine": platform.machine(),
            "platform": platform.platform(),
            "python_version": platform.python_version()
        }
        
        if platform.system() == "Linux":
            # Check if running under WSL
            try:
                with open("/proc/version", "r") as f:
                    proc_version = f.read().lower()
                    if "microsoft" in proc_version or "wsl" in proc_version:
                        info["wsl"] = True
                        info["wsl_version"] = "2" if "wsl2" in proc_version else "1"
            except FileNotFoundError:
                info["wsl"] = False
            
            try:
                # Try to detect Linux distribution
                with open("/etc/os-release", "r") as f:
                    os_release = f.read()
                    
                for line in os_release.split('\n'):
                    if line.startswith('ID='):
                        info["distro"] = line.split('=')[1].strip('"')
                    elif line.startswith('VERSION_ID='):
                        info["distro_version"] = line.split('=')[1].strip('"')
                    elif line.startswith('ID_LIKE='):
                        info["distro_like"] = line.split('=')[1].strip('"')
                        
            except FileNotFoundError:
                # Try alternative methods for older systems
                try:
                    with open("/etc/redhat-release", "r") as f:
                        redhat_release = f.read().strip()
                        if "red hat" in redhat_release.lower():
                            info["distro"] = "rhel"
                        elif "centos" in redhat_release.lower():
                            info["distro"] = "centos"
                        elif "fedora" in redhat_release.lower():
                            info["distro"] = "fedora"
                except FileNotFoundError:
                    pass
                
                try:
                    with open("/etc/debian_version", "r") as f:
                        info["distro"] = "debian"
                        info["distro_version"] = f.read().strip()
                except FileNotFoundError:
                    pass
                
                if "distro" not in info:
                    info["distro"] = "unknown"
        
        elif platform.system() == "Windows":
            info["windows_version"] = platform.version()
            # Check if we can use WSL
            try:
                result = subprocess.run(["wsl", "--status"], capture_output=True, text=True)
                if result.returncode == 0:
                    info["wsl_available"] = True
            except FileNotFoundError:
                info["wsl_available"] = False
        
        # Detect Unix variants
        elif platform.system() in ["FreeBSD", "OpenBSD", "NetBSD", "SunOS", "AIX"]:
            info["unix_variant"] = platform.system()
            if platform.system() == "SunOS":
                info["solaris"] = True
        
        return info
    
    def _get_dependencies_spec(self) -> Dict[str, DependencySpec]:
        """Define all project dependencies."""
        return {
            "clang": DependencySpec(
                name="Clang Compiler",
                version="21",
                package_managers={
                    "apt": "clang-21 clang++-21 clang-tools-21 clang-tidy-21 clang-format-21",
                    "yum": "clang clang-tools-extra",
                    "dnf": "clang clang-tools-extra",
                    "pacman": "clang",
                    "brew": "llvm@21"
                },
                validation_command="clang++-21 --version",
                environment_vars={"CC": "clang-21", "CXX": "clang++-21"}
            ),
            
            "cmake": DependencySpec(
                name="CMake Build System",
                version="3.28",
                package_managers={
                    "apt": "cmake",
                    "yum": "cmake",
                    "dnf": "cmake",
                    "pacman": "cmake",
                    "brew": "cmake"
                },
                build_from_source={
                    "url": "https://github.com/Kitware/CMake/releases/download/v3.28.0/cmake-3.28.0.tar.gz",
                    "configure_command": "./bootstrap --prefix=/usr/local",
                    "build_command": "make -j$(nproc)",
                    "install_command": "sudo make install"
                },
                validation_command="cmake --version"
            ),
            
            "openmpi": DependencySpec(
                name="OpenMPI",
                version="5.0",
                package_managers={
                    "apt": "libopenmpi-dev openmpi-bin",
                    "yum": "openmpi-devel",
                    "dnf": "openmpi-devel",
                    "pacman": "openmpi",
                    "brew": "open-mpi"
                },
                validation_command="mpirun --version",
                environment_vars={"OMPI_CC": "clang-21", "OMPI_CXX": "clang++-21"}
            ),
            
            "zeromq": DependencySpec(
                name="ZeroMQ Messaging Library",
                version="4.3.5",
                package_managers={
                    "apt": "libzmq3-dev libcppzmq-dev",
                    "yum": "zeromq-devel cppzmq-devel",
                    "dnf": "zeromq-devel cppzmq-devel",
                    "pacman": "zeromq cppzmq",
                    "brew": "zmq cppzmq"
                },
                validation_command="pkg-config --modversion libzmq"
            ),
            
            "imgui": DependencySpec(
                name="Dear ImGui",
                version="1.90",
                package_managers={
                    "brew": "imgui"
                },
                build_from_source={
                    "url": "https://github.com/ocornut/imgui.git",
                    "type": "git",
                    "build_command": "# Header-only library, will be included in CMake"
                }
            ),
            
            "gtest": DependencySpec(
                name="Google Test Framework",
                version="1.14",
                package_managers={
                    "apt": "libgtest-dev",
                    "yum": "gtest-devel",
                    "dnf": "gtest-devel", 
                    "pacman": "gtest",
                    "brew": "googletest"
                },
                validation_command="pkg-config --modversion gtest"
            ),
            
            "valgrind": DependencySpec(
                name="Valgrind Memory Debugger",
                version="latest",
                package_managers={
                    "apt": "valgrind",
                    "yum": "valgrind",
                    "dnf": "valgrind",
                    "pacman": "valgrind",
                    "brew": "valgrind"
                },
                validation_command="valgrind --version"
            ),
            
            "ansible": DependencySpec(
                name="Ansible Automation",
                version="latest",
                package_managers={
                    "pip": "ansible",
                    "apt": "ansible",
                    "yum": "ansible",
                    "dnf": "ansible",
                    "pacman": "ansible",
                    "brew": "ansible"
                },
                validation_command="ansible --version"
            )
        }
    
    async def execute_task(self, task: Task) -> Dict[str, Any]:
        """Execute environment-related tasks."""
        try:
            if task.task_type not in [TaskType.ENVIRONMENT_SETUP, TaskType.DEPENDENCY_CHECK]:
                raise ValueError(f"Unexpected task type: {task.task_type}")
            
            action = task.parameters.get("action")
            
            if action == "generate_ansible_playbook":
                return await self._generate_ansible_playbook(task.parameters)
            elif action == "install_dependencies":
                return await self._install_dependencies(task.parameters)
            elif action == "validate_environment":
                return await self._validate_environment(task.parameters)
            elif action == "update_dependencies":
                return await self._update_dependencies(task.parameters)
            elif action == "create_setup_script":
                return await self._create_setup_script(task.parameters)
            else:
                raise ValueError(f"Unknown action: {action}")
        
        except Exception as e:
            self.logger.error(f"Task execution failed: {e}")
            raise
    
    async def _generate_ansible_playbook(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Generate comprehensive Ansible playbook."""
        playbook_name = params.get("playbook_name", "setup_fastjson_environment")
        target_platforms = params.get("platforms", ["ubuntu", "centos", "macos"])
        
        self.logger.info(f"Generating Ansible playbook: {playbook_name}")
        
        # Generate main playbook
        playbook = self._create_main_playbook(target_platforms)
        playbook_file = self.ansible_dir / f"{playbook_name}.yml"
        
        with open(playbook_file, 'w') as f:
            yaml.dump(playbook, f, default_flow_style=False, sort_keys=False)
        
        # Generate platform-specific role files
        roles_created = []
        for platform in target_platforms:
            role_files = await self._create_platform_role(platform)
            roles_created.extend(role_files)
        
        # Generate inventory file
        inventory_file = await self._create_inventory_file()
        
        # Generate group variables
        group_vars = await self._create_group_variables()
        
        return {
            "playbook_file": str(playbook_file),
            "roles_created": roles_created,
            "inventory_file": str(inventory_file),
            "group_vars": group_vars
        }
    
    def _create_main_playbook(self, target_platforms: List[str]) -> Dict[str, Any]:
        """Create the main Ansible playbook."""
        return {
            "name": "FastestJSONInTheWest Development Environment Setup",
            "hosts": "all",
            "become": True,
            "gather_facts": True,
            "vars": {
                "project_name": "FastestJSONInTheWest",
                "cpp_standard": "c++23",
                "clang_version": "21",
                "cmake_min_version": "3.28",
                "build_type": "Release"
            },
            "pre_tasks": [
                {
                    "name": "Update package cache",
                    "package": {"update_cache": True},
                    "when": "ansible_os_family == 'Debian'"
                },
                {
                    "name": "Install basic development tools",
                    "package": {
                        "name": ["build-essential", "wget", "curl", "git", "python3", "python3-pip"],
                        "state": "present"
                    },
                    "when": "ansible_os_family == 'Debian'"
                }
            ],
            "roles": [
                {"role": "clang_compiler", "tags": ["compiler"]},
                {"role": "cmake_build", "tags": ["build"]},
                {"role": "openmpi_parallel", "tags": ["parallel"]},
                {"role": "zeromq_messaging", "tags": ["messaging"]},
                {"role": "development_tools", "tags": ["tools"]},
                {"role": "validation", "tags": ["validate"]}
            ],
            "post_tasks": [
                {
                    "name": "Validate installation",
                    "command": "{{ item.command }}",
                    "register": "validation_results",
                    "failed_when": "validation_results.rc != 0",
                    "loop": [
                        {"command": "clang++-21 --version", "name": "Clang compiler"},
                        {"command": "cmake --version", "name": "CMake"},
                        {"command": "mpirun --version", "name": "OpenMPI"},
                        {"command": "pkg-config --modversion libzmq", "name": "ZeroMQ"}
                    ]
                }
            ]
        }
    
    async def _create_platform_role(self, platform: str) -> List[str]:
        """Create Ansible role for specific platform."""
        roles_dir = self.ansible_dir / "roles"
        roles_dir.mkdir(exist_ok=True)
        
        created_files = []
        
        # Clang compiler role
        clang_role = await self._create_clang_role(platform)
        clang_file = roles_dir / "clang_compiler" / "tasks" / "main.yml"
        clang_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(clang_file, 'w') as f:
            yaml.dump(clang_role, f, default_flow_style=False)
        created_files.append(str(clang_file))
        
        # CMake role
        cmake_role = await self._create_cmake_role(platform)
        cmake_file = roles_dir / "cmake_build" / "tasks" / "main.yml"
        cmake_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(cmake_file, 'w') as f:
            yaml.dump(cmake_role, f, default_flow_style=False)
        created_files.append(str(cmake_file))
        
        # OpenMPI role
        openmpi_role = await self._create_openmpi_role(platform)
        openmpi_file = roles_dir / "openmpi_parallel" / "tasks" / "main.yml"
        openmpi_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(openmpi_file, 'w') as f:
            yaml.dump(openmpi_role, f, default_flow_style=False)
        created_files.append(str(openmpi_file))
        
        # ZeroMQ role
        zeromq_role = await self._create_zeromq_role(platform)
        zeromq_file = roles_dir / "zeromq_messaging" / "tasks" / "main.yml"
        zeromq_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(zeromq_file, 'w') as f:
            yaml.dump(zeromq_role, f, default_flow_style=False)
        created_files.append(str(zeromq_file))
        
        return created_files
    
    async def _create_clang_role(self, platform: str) -> List[Dict[str, Any]]:
        """Create Clang compiler installation role."""
        if platform in ["ubuntu", "debian"]:
            return [
                {
                    "name": "Add LLVM APT repository",
                    "apt_repository": {
                        "repo": "deb http://apt.llvm.org/{{ ansible_distribution_release }}/ llvm-toolchain-{{ ansible_distribution_release }}-21 main",
                        "state": "present"
                    }
                },
                {
                    "name": "Add LLVM APT key",
                    "apt_key": {
                        "url": "https://apt.llvm.org/llvm-snapshot.gpg.key",
                        "state": "present"
                    }
                },
                {
                    "name": "Update package cache",
                    "apt": {"update_cache": True}
                },
                {
                    "name": "Install Clang 21 and tools",
                    "apt": {
                        "name": [
                            "clang-21",
                            "clang++-21", 
                            "clang-tools-21",
                            "clang-tidy-21",
                            "clang-format-21",
                            "libc++-21-dev",
                            "libc++abi-21-dev",
                            "lld-21"
                        ],
                        "state": "present"
                    }
                },
                {
                    "name": "Set Clang as default compiler",
                    "alternatives": {
                        "name": "{{ item.name }}",
                        "link": "{{ item.link }}",
                        "path": "{{ item.path }}",
                        "priority": 100
                    },
                    "loop": [
                        {"name": "cc", "link": "/usr/bin/cc", "path": "/usr/bin/clang-21"},
                        {"name": "c++", "link": "/usr/bin/c++", "path": "/usr/bin/clang++-21"},
                        {"name": "clang", "link": "/usr/bin/clang", "path": "/usr/bin/clang-21"},
                        {"name": "clang++", "link": "/usr/bin/clang++", "path": "/usr/bin/clang++-21"}
                    ]
                }
            ]
        elif platform in ["centos", "rhel", "fedora"]:
            return [
                {
                    "name": "Install EPEL repository",
                    "yum": {"name": "epel-release", "state": "present"},
                    "when": "ansible_distribution in ['CentOS', 'RedHat']"
                },
                {
                    "name": "Install development tools",
                    "yum": {
                        "name": ["@development-tools", "clang", "clang-tools-extra"],
                        "state": "present"
                    }
                }
            ]
        elif platform == "macos":
            return [
                {
                    "name": "Install Clang via Homebrew",
                    "homebrew": {"name": "llvm@21", "state": "present"}
                },
                {
                    "name": "Add LLVM to PATH",
                    "lineinfile": {
                        "path": "{{ ansible_env.HOME }}/.zshrc",
                        "line": "export PATH=/opt/homebrew/opt/llvm@21/bin:$PATH",
                        "create": True
                    }
                }
            ]
        
        return []
    
    async def _create_cmake_role(self, platform: str) -> List[Dict[str, Any]]:
        """Create CMake installation role."""
        return [
            {
                "name": "Download CMake source",
                "get_url": {
                    "url": "https://github.com/Kitware/CMake/releases/download/v3.28.0/cmake-3.28.0.tar.gz",
                    "dest": "/tmp/cmake-3.28.0.tar.gz"
                }
            },
            {
                "name": "Extract CMake source",
                "unarchive": {
                    "src": "/tmp/cmake-3.28.0.tar.gz",
                    "dest": "/tmp/",
                    "remote_src": True
                }
            },
            {
                "name": "Configure CMake build",
                "command": "./bootstrap --prefix=/usr/local --parallel={{ ansible_processor_vcpus }}",
                "args": {"chdir": "/tmp/cmake-3.28.0"}
            },
            {
                "name": "Build CMake",
                "command": "make -j{{ ansible_processor_vcpus }}",
                "args": {"chdir": "/tmp/cmake-3.28.0"}
            },
            {
                "name": "Install CMake",
                "command": "make install",
                "args": {"chdir": "/tmp/cmake-3.28.0"}
            }
        ]
    
    async def _create_openmpi_role(self, platform: str) -> List[Dict[str, Any]]:
        """Create OpenMPI installation role."""
        if platform in ["ubuntu", "debian"]:
            return [
                {
                    "name": "Install OpenMPI",
                    "apt": {
                        "name": ["libopenmpi-dev", "openmpi-bin", "openmpi-common"],
                        "state": "present"
                    }
                }
            ]
        elif platform in ["centos", "rhel", "fedora"]:
            return [
                {
                    "name": "Install OpenMPI",
                    "yum": {
                        "name": ["openmpi", "openmpi-devel"],
                        "state": "present"
                    }
                }
            ]
        
        return []
    
    async def _create_zeromq_role(self, platform: str) -> List[Dict[str, Any]]:
        """Create ZeroMQ installation role."""
        if platform in ["ubuntu", "debian"]:
            return [
                {
                    "name": "Install ZeroMQ",
                    "apt": {
                        "name": ["libzmq3-dev", "libcppzmq-dev", "pkg-config"],
                        "state": "present"
                    }
                }
            ]
        elif platform in ["centos", "rhel", "fedora"]:
            return [
                {
                    "name": "Install ZeroMQ",
                    "yum": {
                        "name": ["zeromq-devel", "cppzmq-devel", "pkgconfig"],
                        "state": "present"
                    }
                }
            ]
        
        return []
    
    async def _create_inventory_file(self) -> str:
        """Create Ansible inventory file."""
        inventory_content = """[development]
localhost ansible_connection=local

[production]
# Add production servers here

[all:vars]
ansible_python_interpreter=/usr/bin/python3
"""
        
        inventory_file = self.ansible_dir / "inventory.ini"
        with open(inventory_file, 'w') as f:
            f.write(inventory_content)
        
        return str(inventory_file)
    
    async def _create_group_variables(self) -> str:
        """Create group variables file."""
        group_vars = {
            "project_name": "FastestJSONInTheWest",
            "cpp_standard": "c++23",
            "compiler": {
                "primary": "clang++-21",
                "fallback": "g++",
                "flags": ["-std=c++23", "-O3", "-march=native"]
            },
            "dependencies": {
                "clang_version": "21",
                "cmake_version": "3.28",
                "openmpi_version": "5.0",
                "zeromq_version": "4.3.5"
            },
            "build_config": {
                "parallel_jobs": "{{ ansible_processor_vcpus }}",
                "build_type": "Release",
                "enable_testing": True,
                "enable_benchmarking": True
            }
        }
        
        group_vars_dir = self.ansible_dir / "group_vars"
        group_vars_dir.mkdir(exist_ok=True)
        
        all_vars_file = group_vars_dir / "all.yml"
        with open(all_vars_file, 'w') as f:
            yaml.dump(group_vars, f, default_flow_style=False)
        
        return str(all_vars_file)
    
    async def _install_dependencies(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Install project dependencies."""
        dependencies = params.get("dependencies", list(self.dependencies.keys()))
        
        self.logger.info(f"Installing dependencies: {dependencies}")
        
        installed = []
        failed = []
        
        for dep_name in dependencies:
            if dep_name not in self.dependencies:
                failed.append(f"Unknown dependency: {dep_name}")
                continue
            
            try:
                result = await self._install_single_dependency(self.dependencies[dep_name])
                if result["success"]:
                    installed.append(dep_name)
                else:
                    failed.append(f"{dep_name}: {result['error']}")
            except Exception as e:
                failed.append(f"{dep_name}: {str(e)}")
        
        return {
            "installed": installed,
            "failed": failed,
            "total_requested": len(dependencies)
        }
    
    async def _install_single_dependency(self, dep_spec: DependencySpec) -> Dict[str, Any]:
        """Install a single dependency."""
        system = self.platform_info["system"].lower()
        distro = self.platform_info.get("distro", "").lower()
        
        # Determine package manager
        package_manager = self._get_package_manager(system, distro)
        
        if package_manager not in dep_spec.package_managers:
            if dep_spec.build_from_source:
                return await self._build_from_source(dep_spec)
            else:
                return {"success": False, "error": f"No package manager found for {dep_spec.name}"}
        
        # Install using package manager
        package_name = dep_spec.package_managers[package_manager]
        
        try:
            if package_manager == "apt":
                result = await self._run_command(f"sudo apt-get install -y {package_name}")
            elif package_manager in ["yum", "dnf"]:
                result = await self._run_command(f"sudo {package_manager} install -y {package_name}")
            elif package_manager == "pacman":
                result = await self._run_command(f"sudo pacman -S --noconfirm {package_name}")
            elif package_manager == "brew":
                result = await self._run_command(f"brew install {package_name}")
            elif package_manager == "pip":
                result = await self._run_command(f"pip3 install {package_name}")
            else:
                return {"success": False, "error": f"Unsupported package manager: {package_manager}"}
            
            return {"success": result["success"], "error": result.get("error")}
            
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def _get_package_manager(self, system: str, distro: str) -> str:
        """Determine the appropriate package manager."""
        if system == "darwin":
            return "brew"
        elif system == "linux":
            if distro in ["ubuntu", "debian"]:
                return "apt"
            elif distro in ["centos", "rhel"]:
                return "yum"
            elif distro in ["fedora"]:
                return "dnf"
            elif distro in ["arch"]:
                return "pacman"
        
        return "unknown"
    
    async def _build_from_source(self, dep_spec: DependencySpec) -> Dict[str, Any]:
        """Build dependency from source."""
        if not dep_spec.build_from_source:
            return {"success": False, "error": "No source build configuration"}
        
        build_config = dep_spec.build_from_source
        
        try:
            # Download source
            if build_config.get("type") == "git":
                result = await self._run_command(f"git clone {build_config['url']} /tmp/{dep_spec.name}")
            else:
                result = await self._run_command(f"wget -O /tmp/{dep_spec.name}.tar.gz {build_config['url']}")
                if result["success"]:
                    result = await self._run_command(f"tar -xzf /tmp/{dep_spec.name}.tar.gz -C /tmp/")
            
            if not result["success"]:
                return result
            
            # Configure and build
            if "configure_command" in build_config:
                result = await self._run_command(build_config["configure_command"], cwd=f"/tmp/{dep_spec.name}")
                if not result["success"]:
                    return result
            
            if "build_command" in build_config:
                result = await self._run_command(build_config["build_command"], cwd=f"/tmp/{dep_spec.name}")
                if not result["success"]:
                    return result
            
            if "install_command" in build_config:
                result = await self._run_command(build_config["install_command"], cwd=f"/tmp/{dep_spec.name}")
                return result
            
            return {"success": True}
            
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    async def _run_command(self, command: str, cwd: Optional[str] = None) -> Dict[str, Any]:
        """Run a shell command asynchronously."""
        try:
            process = await asyncio.create_subprocess_shell(
                command,
                cwd=cwd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            
            stdout, stderr = await process.communicate()
            
            return {
                "success": process.returncode == 0,
                "stdout": stdout.decode(),
                "stderr": stderr.decode(),
                "return_code": process.returncode,
                "error": stderr.decode() if process.returncode != 0 else None
            }
        
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    async def _validate_environment(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Validate the development environment."""
        dependencies = params.get("dependencies", list(self.dependencies.keys()))
        
        self.logger.info("Validating environment setup")
        
        validation_results = {}
        all_valid = True
        
        for dep_name in dependencies:
            if dep_name not in self.dependencies:
                validation_results[dep_name] = {"valid": False, "error": "Unknown dependency"}
                all_valid = False
                continue
            
            dep_spec = self.dependencies[dep_name]
            if dep_spec.validation_command:
                result = await self._run_command(dep_spec.validation_command)
                validation_results[dep_name] = {
                    "valid": result["success"],
                    "output": result.get("stdout", ""),
                    "error": result.get("error")
                }
                if not result["success"]:
                    all_valid = False
            else:
                validation_results[dep_name] = {"valid": True, "note": "No validation command specified"}
        
        return {
            "all_valid": all_valid,
            "individual_results": validation_results,
            "platform_info": self.platform_info
        }
    
    async def _update_dependencies(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Update project dependencies to latest versions."""
        self.logger.info("Updating dependencies")
        
        # This would implement dependency updates
        # For now, return placeholder
        return {
            "updated": [],
            "failed": [],
            "message": "Dependency update functionality to be implemented"
        }
    
    async def _create_setup_script(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Create automated setup script."""
        script_content = self._generate_setup_script()
        
        script_file = self.scripts_dir / "setup_environment.sh"
        with open(script_file, 'w') as f:
            f.write(script_content)
        
        # Make script executable
        script_file.chmod(0o755)
        
        return {
            "script_file": str(script_file),
            "executable": True
        }
    
    def _generate_setup_script(self) -> str:
        """Generate shell script for environment setup."""
        return '''#!/bin/bash
# FastestJSONInTheWest Environment Setup Script
# Generated by EnvironmentAgent

set -e

echo "Setting up FastestJSONInTheWest development environment..."

# Detect platform
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
    if command -v apt-get &> /dev/null; then
        PACKAGE_MANAGER="apt"
    elif command -v yum &> /dev/null; then
        PACKAGE_MANAGER="yum"
    elif command -v dnf &> /dev/null; then
        PACKAGE_MANAGER="dnf"
    else
        echo "Unsupported Linux distribution"
        exit 1
    fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
    PACKAGE_MANAGER="brew"
else
    echo "Unsupported platform: $OSTYPE"
    exit 1
fi

echo "Detected platform: $PLATFORM with package manager: $PACKAGE_MANAGER"

# Install dependencies based on platform
if [[ "$PACKAGE_MANAGER" == "apt" ]]; then
    sudo apt-get update
    sudo apt-get install -y wget curl git build-essential python3 python3-pip
    
    # Add LLVM repository
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
    echo "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-21 main" | sudo tee /etc/apt/sources.list.d/llvm.list
    
    sudo apt-get update
    sudo apt-get install -y clang-21 clang++-21 clang-tools-21 clang-tidy-21 clang-format-21
    sudo apt-get install -y libopenmpi-dev openmpi-bin
    sudo apt-get install -y libzmq3-dev libcppzmq-dev
    sudo apt-get install -y libgtest-dev valgrind

elif [[ "$PACKAGE_MANAGER" == "brew" ]]; then
    brew install llvm@21 cmake open-mpi zmq cppzmq googletest valgrind
fi

# Install Python dependencies
pip3 install ansible pyyaml

# Validate installation
echo "Validating installation..."
clang++-21 --version || echo "Warning: clang++-21 not found"
cmake --version || echo "Warning: cmake not found"
mpirun --version || echo "Warning: mpirun not found"

echo "Environment setup complete!"
'''