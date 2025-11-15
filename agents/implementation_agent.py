#!/usr/bin/env python3
"""
Implementation Agent for FastestJSONInTheWest

This agent is responsible for generating C++23 code according to the project's
coding standards. It ensures all generated code follows modern C++ best practices
including modules, smart pointers, concepts, and fluent API design.

Key Responsibilities:
- Generate C++23 module files (.cppm)
- Implement fluent API patterns
- Ensure smart pointer usage and no raw allocations
- Apply concepts and constraints
- Generate code following C++ Core Guidelines
- Integrate with ImGui simulator for visual debugging
"""

import asyncio
import os
import re
from pathlib import Path
from typing import Dict, List, Any, Optional
from dataclasses import dataclass
import subprocess
import json
from agent_orchestrator import Agent, Task, TaskType

@dataclass
class ModuleSpec:
    """Specification for a C++23 module."""
    name: str
    interface_file: str
    implementation_file: Optional[str] = None
    dependencies: Optional[List[str]] = None
    exports: Optional[List[str]] = None
    
    def __post_init__(self):
        if self.dependencies is None:
            self.dependencies = []
        if self.exports is None:
            self.exports = []

class ImplementationAgent(Agent):
    """Agent responsible for generating C++23 implementation code."""
    
    def __init__(self, orchestrator):
        super().__init__("ImplementationAgent", orchestrator)
        self.project_root = orchestrator.project_root
        self.src_dir = self.project_root / "src"
        self.modules_dir = self.src_dir / "modules"
        self.include_dir = self.src_dir / "include"
        
        # Ensure directories exist
        self.modules_dir.mkdir(parents=True, exist_ok=True)
        self.include_dir.mkdir(parents=True, exist_ok=True)
        
        # Load coding standards
        self.coding_standards = self._load_coding_standards()
        
        # Initialize module templates after methods are defined
        self._init_module_templates()
    
    def _init_module_templates(self):
        """Initialize module templates."""
        self.module_templates = {
            "core": self._get_core_module_template(),
            "performance": self._get_performance_module_template(),
            "simulator": self._get_simulator_module_template(),
            "mapreduce": self._get_mapreduce_module_template(),
            "zeromq": self._get_zeromq_module_template()
        }
    
    def _load_coding_standards(self) -> Dict[str, Any]:
        """Load coding standards from the project."""
        standards_file = self.project_root / "ai" / "coding_standards.md"
        if standards_file.exists():
            with open(standards_file, 'r') as f:
                content = f.read()
                return {
                    "cpp_version": "C++23",
                    "use_modules": True,
                    "no_raw_pointers": True,
                    "use_concepts": True,
                    "fluent_api": True,
                    "smart_pointers": True,
                    "stl_containers": True,
                    "ranges_views": True,
                    "error_handling": "std::expected"
                }
        return {}
    
    async def execute_task(self, task: Task) -> Dict[str, Any]:
        """Execute implementation-related tasks."""
        try:
            if task.task_type != TaskType.CODE_GENERATION:
                raise ValueError(f"Unexpected task type: {task.task_type}")
            
            action = task.parameters.get("action")
            
            if action == "generate_module":
                return await self._generate_module(task.parameters)
            elif action == "generate_fluent_api":
                return await self._generate_fluent_api(task.parameters)
            elif action == "implement_class":
                return await self._implement_class(task.parameters)
            elif action == "generate_concepts":
                return await self._generate_concepts(task.parameters)
            elif action == "update_simulator":
                return await self._update_simulator(task.parameters)
            else:
                raise ValueError(f"Unknown action: {action}")
        
        except Exception as e:
            self.logger.error(f"Task execution failed: {e}")
            raise
    
    async def _generate_module(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Generate a C++23 module."""
        module_name = params["module_name"]
        module_type = params.get("module_type", "core")
        dependencies = params.get("dependencies", [])
        
        self.logger.info(f"Generating module: {module_name}")
        
        # Get appropriate template
        template = self.module_templates.get(module_type, self.module_templates["core"])
        
        # Generate module content
        module_content = template.format(
            module_name=module_name,
            dependencies=self._format_dependencies(dependencies),
            timestamp=self._get_timestamp(),
            **params
        )
        
        # Write module file
        module_file = self.modules_dir / f"{module_name}.cppm"
        with open(module_file, 'w') as f:
            f.write(module_content)
        
        self.logger.info(f"Module {module_name} generated at {module_file}")
        
        return {
            "module_name": module_name,
            "file_path": str(module_file),
            "dependencies": dependencies
        }
    
    async def _generate_fluent_api(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Generate fluent API implementation."""
        class_name = params["class_name"]
        methods = params.get("methods", [])
        
        self.logger.info(f"Generating fluent API for: {class_name}")
        
        api_code = self._generate_fluent_class(class_name, methods)
        
        # Write to appropriate module
        module_name = f"fastjson.{class_name.lower()}"
        module_file = self.modules_dir / f"{module_name}.cppm"
        
        with open(module_file, 'w') as f:
            f.write(api_code)
        
        return {
            "class_name": class_name,
            "module_file": str(module_file),
            "methods_count": len(methods)
        }
    
    async def _implement_class(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Implement a class according to coding standards."""
        class_name = params["class_name"]
        base_class = params.get("base_class")
        interfaces = params.get("interfaces", [])
        members = params.get("members", [])
        
        self.logger.info(f"Implementing class: {class_name}")
        
        class_code = self._generate_class_implementation(
            class_name, base_class, interfaces, members
        )
        
        module_name = f"fastjson.{class_name.lower()}"
        module_file = self.modules_dir / f"{module_name}.cppm"
        
        with open(module_file, 'w') as f:
            f.write(class_code)
        
        return {
            "class_name": class_name,
            "module_file": str(module_file),
            "members_count": len(members)
        }
    
    async def _generate_concepts(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Generate C++20 concepts for type constraints."""
        concept_name = params["concept_name"]
        constraints = params.get("constraints", [])
        
        self.logger.info(f"Generating concept: {concept_name}")
        
        concept_code = self._generate_concept_definition(concept_name, constraints)
        
        concepts_file = self.modules_dir / "fastjson.concepts.cppm"
        
        # Append to concepts module
        with open(concepts_file, 'a') as f:
            f.write(f"\n{concept_code}\n")
        
        return {
            "concept_name": concept_name,
            "file_path": str(concepts_file)
        }
    
    async def _update_simulator(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Update ImGui simulator integration."""
        component_name = params["component_name"]
        visualization_type = params.get("visualization_type", "flow")
        
        self.logger.info(f"Updating simulator for: {component_name}")
        
        simulator_code = self._generate_simulator_integration(
            component_name, visualization_type
        )
        
        simulator_file = self.modules_dir / "fastjson.simulator.cppm"
        
        # Update simulator module
        self._update_file_section(simulator_file, component_name, simulator_code)
        
        return {
            "component_name": component_name,
            "simulator_file": str(simulator_file)
        }
    
    def _get_core_module_template(self) -> str:
        """Template for core modules."""
        return '''// {module_name}.cppm - Core FastestJSONInTheWest module
// Generated by ImplementationAgent at {timestamp}
// Follows C++23 standards with modern best practices

module;

// System includes
#include <memory>
#include <string_view>
#include <vector>
#include <expected>
#include <concepts>
#include <ranges>

export module {module_name};

{dependencies}

export namespace fastjson {{

/// @brief Core {module_name} functionality
/// @details Implements fluent API with modern C++23 features
class {class_name} {{
public:
    using value_type = std::string_view;
    using error_type = std::string;
    using result_type = std::expected<value_type, error_type>;
    
    /// @brief Default constructor
    {class_name}() = default;
    
    /// @brief Move constructor
    {class_name}({class_name}&&) noexcept = default;
    
    /// @brief Move assignment
    {class_name}& operator=({class_name}&&) noexcept = default;
    
    /// @brief Deleted copy operations (move-only type)
    {class_name}(const {class_name}&) = delete;
    {class_name}& operator=(const {class_name}&) = delete;
    
    /// @brief Virtual destructor
    virtual ~{class_name}() = default;
    
    /// @brief Fluent API builder pattern
    auto with_option(std::string_view option) -> {class_name}&;
    
    /// @brief Process data with error handling
    auto process(std::span<const std::byte> data) -> result_type;
    
    /// @brief Get current state
    auto state() const noexcept -> std::string_view;

private:
    std::unique_ptr<class Impl> impl_;
}};

// Concept for JSON processable types
template<typename T>
concept JsonProcessable = requires(T t) {{
    {{ t.process(std::span<const std::byte>{{}}) }} -> std::same_as<typename T::result_type>;
    {{ t.state() }} -> std::convertible_to<std::string_view>;
}};

}} // namespace fastjson
'''
    
    def _get_performance_module_template(self) -> str:
        """Template for performance-oriented modules."""
        return '''// {module_name}.cppm - Performance optimized FastestJSONInTheWest module
// Generated by ImplementationAgent at {timestamp}
// SIMD-optimized with lock-free data structures

module;

#include <memory>
#include <atomic>
#include <vector>
#include <span>
#include <expected>
#include <immintrin.h>  // SIMD intrinsics

export module {module_name};

{dependencies}

export namespace fastjson::performance {{

/// @brief SIMD-optimized JSON processing
/// @details Uses AVX2/AVX-512 when available, falls back to SSE2
class SIMDProcessor {{
public:
    using result_type = std::expected<std::span<const std::byte>, std::string>;
    
    /// @brief Constructor with SIMD capability detection
    explicit SIMDProcessor();
    
    /// @brief Process JSON data with SIMD optimization
    auto process_vectorized(std::span<const std::byte> input) -> result_type;
    
    /// @brief Get supported SIMD instruction set
    auto simd_capabilities() const noexcept -> std::string_view;

private:
    enum class SIMDLevel {{ SSE2, AVX, AVX2, AVX512 }};
    SIMDLevel simd_level_;
    
    auto detect_simd_support() -> SIMDLevel;
    auto process_sse2(std::span<const std::byte> input) -> result_type;
    auto process_avx2(std::span<const std::byte> input) -> result_type;
    auto process_avx512(std::span<const std::byte> input) -> result_type;
}};

/// @brief Lock-free queue for high-throughput processing
template<typename T>
class LockFreeQueue {{
public:
    explicit LockFreeQueue(size_t capacity);
    
    auto push(T&& item) -> bool;
    auto pop() -> std::expected<T, std::string>;
    auto size() const noexcept -> size_t;

private:
    struct alignas(64) Node {{
        std::atomic<T*> data;
        std::atomic<Node*> next;
    }};
    
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    const size_t capacity_;
}};

}} // namespace fastjson::performance
'''
    
    def _get_simulator_module_template(self) -> str:
        """Template for ImGui simulator integration."""
        return '''// {module_name}.cppm - ImGui Simulator for FastestJSONInTheWest
// Generated by ImplementationAgent at {timestamp}
// Visual debugging and data flow visualization

module;

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

export module {module_name};

{dependencies}

export namespace fastjson::simulator {{

/// @brief Visual data flow node for ImGui rendering
struct FlowNode {{
    std::string id;
    std::string label;
    ImVec2 position;
    ImVec2 size;
    ImU32 color;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
}};

/// @brief Connection between flow nodes
struct FlowConnection {{
    std::string from_node;
    std::string to_node;
    std::string from_output;
    std::string to_input;
    ImU32 color;
}};

/// @brief Main simulator class for visual debugging
class JSONFlowSimulator {{
public:
    /// @brief Constructor
    explicit JSONFlowSimulator();
    
    /// @brief Destructor
    ~JSONFlowSimulator();
    
    /// @brief Initialize ImGui context
    auto initialize() -> bool;
    
    /// @brief Render one frame
    auto render_frame() -> void;
    
    /// @brief Add a processing node to visualization
    auto add_node(const FlowNode& node) -> void;
    
    /// @brief Add connection between nodes
    auto add_connection(const FlowConnection& connection) -> void;
    
    /// @brief Update node with real-time data
    auto update_node_data(std::string_view node_id, std::string_view data) -> void;
    
    /// @brief Log message to visual console
    auto log_message(std::string_view message) -> void;
    
    /// @brief Check if simulator should continue running
    auto should_continue() const noexcept -> bool;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    auto render_flow_graph() -> void;
    auto render_console() -> void;
    auto render_metrics() -> void;
}};

/// @brief RAII logger integration with simulator
class SimulatorLogger {{
public:
    explicit SimulatorLogger(std::shared_ptr<JSONFlowSimulator> simulator);
    
    template<typename... Args>
    auto log(std::string_view format, Args&&... args) -> void;
    
    auto log_performance(std::string_view operation, std::chrono::nanoseconds duration) -> void;

private:
    std::shared_ptr<JSONFlowSimulator> simulator_;
}};

}} // namespace fastjson::simulator
'''
    
    def _get_mapreduce_module_template(self) -> str:
        """Template for map-reduce functionality."""
        return '''// {module_name}.cppm - Map-Reduce operations for FastestJSONInTheWest
// Generated by ImplementationAgent at {timestamp}
// Functional programming with fluent API

module;

#include <memory>
#include <functional>
#include <ranges>
#include <concepts>
#include <expected>
#include <execution>

export module {module_name};

{dependencies}

export namespace fastjson::mapreduce {{

/// @brief Concept for mappable types
template<typename T, typename F>
concept Mappable = requires(T t, F f) {{
    {{ std::ranges::transform(t, f) }};
}};

/// @brief Concept for reducible types
template<typename T, typename F, typename Init>
concept Reducible = requires(T t, F f, Init init) {{
    {{ std::ranges::fold_left(t, init, f) }};
}};

/// @brief Fluent map-reduce operations on JSON data
template<typename ValueType>
class MapReduceChain {{
public:
    using value_type = ValueType;
    using container_type = std::vector<ValueType>;
    using result_type = std::expected<container_type, std::string>;
    
    /// @brief Constructor with initial data
    explicit MapReduceChain(container_type data);
    
    /// @brief Map operation with transformation function
    template<std::invocable<const ValueType&> F>
    auto map(F&& func) -> MapReduceChain<std::invoke_result_t<F, const ValueType&>>;
    
    /// @brief Filter operation with predicate
    template<std::predicate<const ValueType&> P>
    auto filter(P&& predicate) -> MapReduceChain&;
    
    /// @brief Reduce operation with combining function
    template<typename InitType, typename F>
        requires Reducible<container_type, F, InitType>
    auto reduce(InitType&& init, F&& func) -> InitType;
    
    /// @brief Parallel execution
    auto parallel() -> MapReduceChain&;
    
    /// @brief Execute and get result
    auto execute() -> result_type;
    
    /// @brief Get current data (for debugging)
    auto current_data() const noexcept -> const container_type&;

private:
    container_type data_;
    bool use_parallel_ = false;
    
    template<typename F>
    auto apply_transformation(F&& func) -> void;
}};

/// @brief JSON document query builder with fluent API
class JSONQuery {{
public:
    using result_type = std::expected<std::vector<std::string>, std::string>;
    
    /// @brief Constructor
    explicit JSONQuery();
    
    /// @brief Select fields
    auto select(std::span<const std::string_view> fields) -> JSONQuery&;
    
    /// @brief Where clause
    auto where(std::string_view field, std::string_view value) -> JSONQuery&;
    
    /// @brief Order by field
    auto order_by(std::string_view field, bool ascending = true) -> JSONQuery&;
    
    /// @brief Limit results
    auto limit(size_t count) -> JSONQuery&;
    
    /// @brief Execute query on JSON document
    auto execute(std::string_view json_doc) -> result_type;

private:
    std::vector<std::string> selected_fields_;
    std::vector<std::pair<std::string, std::string>> where_clauses_;
    std::optional<std::pair<std::string, bool>> order_field_;
    std::optional<size_t> limit_count_;
}};

}} // namespace fastjson::mapreduce
'''
    
    def _get_zeromq_module_template(self) -> str:
        """Template for ZeroMQ integration."""
        return '''// {module_name}.cppm - ZeroMQ integration for FastestJSONInTheWest
// Generated by ImplementationAgent at {timestamp}
// High-performance async messaging

module;

#include <memory>
#include <string>
#include <expected>
#include <chrono>
#include <zmq.hpp>

export module {module_name};

{dependencies}

export namespace fastjson::messaging {{

/// @brief ZeroMQ message patterns
enum class MessagePattern {{
    REQUEST_REPLY,
    PUBLISH_SUBSCRIBE,
    PUSH_PULL,
    PAIR
}};

/// @brief ZeroMQ socket wrapper with RAII
class ZMQSocket {{
public:
    using result_type = std::expected<std::string, std::string>;
    
    /// @brief Constructor
    explicit ZMQSocket(MessagePattern pattern);
    
    /// @brief Destructor
    ~ZMQSocket();
    
    /// @brief Bind to endpoint
    auto bind(std::string_view endpoint) -> std::expected<void, std::string>;
    
    /// @brief Connect to endpoint
    auto connect(std::string_view endpoint) -> std::expected<void, std::string>;
    
    /// @brief Send message
    auto send(std::string_view message) -> std::expected<void, std::string>;
    
    /// @brief Receive message
    auto receive(std::chrono::milliseconds timeout = std::chrono::milliseconds{{1000}}) -> result_type;
    
    /// @brief Set socket option
    auto set_option(int option, const void* value, size_t size) -> std::expected<void, std::string>;

private:
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> socket_;
    MessagePattern pattern_;
}};

/// @brief High-level JSON messaging client
class JSONMessenger {{
public:
    /// @brief Constructor
    explicit JSONMessenger(MessagePattern pattern);
    
    /// @brief Send JSON message
    auto send_json(std::string_view json_message) -> std::expected<void, std::string>;
    
    /// @brief Receive JSON message
    auto receive_json() -> std::expected<std::string, std::string>;
    
    /// @brief Start message broker (for pub-sub patterns)
    auto start_broker(std::string_view frontend_endpoint, std::string_view backend_endpoint) 
        -> std::expected<void, std::string>;

private:
    ZMQSocket socket_;
}};

}} // namespace fastjson::messaging
'''
    
    def _format_dependencies(self, dependencies: List[str]) -> str:
        """Format module dependencies."""
        if not dependencies:
            return ""
        
        imports = "\n".join([f"import {dep};" for dep in dependencies])
        return f"\n// Module dependencies\n{imports}\n"
    
    def _get_timestamp(self) -> str:
        """Get current timestamp for code generation."""
        from datetime import datetime
        return datetime.now().strftime("%Y-%m-%d %H:%M:%S UTC")
    
    def _generate_fluent_class(self, class_name: str, methods: List[Dict[str, Any]]) -> str:
        """Generate a fluent API class implementation."""
        template = f'''// {class_name}.cppm - Fluent API implementation
// Generated by ImplementationAgent
// Modern C++23 with fluent method chaining

module;

#include <memory>
#include <string_view>
#include <expected>

export module fastjson.{class_name.lower()};

export namespace fastjson {{

class {class_name} {{
public:
    /// @brief Default constructor
    {class_name}() = default;
    
'''
        
        # Generate fluent methods
        for method in methods:
            method_name = method["name"]
            return_type = method.get("return_type", f"{class_name}&")
            params = method.get("parameters", [])
            
            param_str = ", ".join([f"{p['type']} {p['name']}" for p in params])
            
            template += f'''    /// @brief {method.get("description", f"Configure {method_name}")}
    auto {method_name}({param_str}) -> {return_type};
    
'''
        
        template += '''
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}} // namespace fastjson
'''
        
        return template
    
    def _generate_class_implementation(self, class_name: str, base_class: Optional[str], 
                                     interfaces: List[str], members: List[Dict[str, Any]]) -> str:
        """Generate a complete class implementation."""
        inheritance = ""
        if base_class or interfaces:
            inherits = []
            if base_class:
                inherits.append(f"public {base_class}")
            inherits.extend([f"public {iface}" for iface in interfaces])
            inheritance = f" : {', '.join(inherits)}"
        
        template = f'''// {class_name}.cppm - Implementation
// Generated by ImplementationAgent
// Follows C++23 best practices

module;

#include <memory>
#include <string>
#include <expected>

export module fastjson.{class_name.lower()};

export namespace fastjson {{

class {class_name}{inheritance} {{
public:
    /// @brief Constructor
    explicit {class_name}();
    
    /// @brief Destructor  
    ~{class_name}() override = default;
    
    /// @brief Move constructor
    {class_name}({class_name}&&) noexcept = default;
    
    /// @brief Move assignment
    {class_name}& operator=({class_name}&&) noexcept = default;
    
    /// @brief Deleted copy operations
    {class_name}(const {class_name}&) = delete;
    {class_name}& operator=(const {class_name}&) = delete;
    
'''
        
        # Generate member functions
        for member in members:
            member_name = member["name"]
            member_type = member.get("type", "auto")
            is_const = member.get("const", False)
            const_qualifier = " const" if is_const else ""
            
            template += f'''    /// @brief {member.get("description", f"Access {member_name}")}
    auto {member_name}(){const_qualifier} -> {member_type};
    
'''
        
        template += '''
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}} // namespace fastjson
'''
        
        return template
    
    def _generate_concept_definition(self, concept_name: str, constraints: List[str]) -> str:
        """Generate a C++20 concept definition."""
        constraint_code = " && ".join(constraints)
        
        return f'''
/// @brief Concept {concept_name}
/// @details Type must satisfy all specified constraints
template<typename T>
concept {concept_name} = {constraint_code};
'''
    
    def _generate_simulator_integration(self, component_name: str, visualization_type: str) -> str:
        """Generate ImGui integration code for a component."""
        return f'''
/// @brief Simulator integration for {component_name}
class {component_name}Visualizer {{
public:
    explicit {component_name}Visualizer(std::shared_ptr<JSONFlowSimulator> simulator);
    
    auto render() -> void;
    auto update_data(std::string_view data) -> void;
    
private:
    std::shared_ptr<JSONFlowSimulator> simulator_;
    std::string component_id_ = "{component_name}";
}};
'''
    
    def _update_file_section(self, file_path: Path, section_name: str, content: str) -> None:
        """Update a specific section in a file."""
        if not file_path.exists():
            with open(file_path, 'w') as f:
                f.write(content)
            return
        
        with open(file_path, 'r') as f:
            existing_content = f.read()
        
        # Simple append for now - could be more sophisticated
        with open(file_path, 'a') as f:
            f.write(f"\n{content}\n")