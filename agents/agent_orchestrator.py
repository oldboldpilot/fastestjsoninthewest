#!/usr/bin/env python3
"""
Multi-Threaded Coding Agent System for FastestJSONInTheWest

This system coordinates multiple specialized agents to implement, maintain, and test
the FastestJSONInTheWest C++23 JSON processing library according to specified
coding standards.

Architecture:
- AgentOrchestrator: Main coordination system with thread-safe task distribution
- ImplementationAgent: Code generation according to C++23 standards
- EnvironmentAgent: Development environment and dependency management
- DocumentationAgent: Automatic documentation maintenance and generation
- TestingAgent: Comprehensive test generation and validation
- ConfigurationAgent: Build system and tool configuration management

All agents operate with thread safety and coordinated communication.
Monitors PRD.md and CLAUDE.md for changes and updates implementation accordingly.
"""

import asyncio
import threading
import queue
import logging
import json
import time
import hashlib
from abc import ABC, abstractmethod
from typing import Dict, List, Any, Optional, Callable
from dataclasses import dataclass, asdict
from enum import Enum
from pathlib import Path
import subprocess
import yaml
import concurrent.futures
from datetime import datetime

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('agent_system.log'),
        logging.StreamHandler()
    ]
)

class TaskType(Enum):
    """Types of tasks that can be executed by agents."""
    CODE_GENERATION = "code_generation"
    ENVIRONMENT_SETUP = "environment_setup"
    DOCUMENTATION_UPDATE = "documentation_update"
    TEST_GENERATION = "test_generation"
    CONFIG_UPDATE = "config_update"
    BUILD_VALIDATION = "build_validation"
    DEPENDENCY_CHECK = "dependency_check"
    PRD_CHANGE_HANDLER = "prd_change_handler"
    REQUIREMENTS_ANALYSIS = "requirements_analysis"
    TEST_GENERATION = "test_generation"
    CONFIG_UPDATE = "config_update"
    BUILD_VALIDATION = "build_validation"
    DEPENDENCY_CHECK = "dependency_check"

class TaskPriority(Enum):
    """Task priority levels."""
    CRITICAL = 1
    HIGH = 2
    NORMAL = 3
    LOW = 4

@dataclass
class Task:
    """Represents a task to be executed by an agent."""
    task_id: str
    task_type: TaskType
    priority: TaskPriority
    description: str
    parameters: Dict[str, Any]
    assigned_agent: Optional[str] = None
    status: str = "pending"
    created_at: datetime = None
    started_at: Optional[datetime] = None
    completed_at: Optional[datetime] = None
    result: Optional[Dict[str, Any]] = None
    error: Optional[str] = None

    def __post_init__(self):
        if self.created_at is None:
            self.created_at = datetime.now()

class Agent(ABC):
    """Abstract base class for all coding agents."""
    
    def __init__(self, name: str, orchestrator: 'AgentOrchestrator'):
        self.name = name
        self.orchestrator = orchestrator
        self.logger = logging.getLogger(f"Agent.{name}")
        self.is_running = False
        self.task_queue = queue.PriorityQueue()
        self.worker_thread = None
        
    @abstractmethod
    async def execute_task(self, task: Task) -> Dict[str, Any]:
        """Execute a specific task. Must be implemented by subclasses."""
        pass
    
    def start(self):
        """Start the agent's worker thread."""
        if not self.is_running:
            self.is_running = True
            self.worker_thread = threading.Thread(target=self._worker_loop, daemon=True)
            self.worker_thread.start()
            self.logger.info(f"Agent {self.name} started")
    
    def stop(self):
        """Stop the agent's worker thread."""
        self.is_running = False
        if self.worker_thread and self.worker_thread.is_alive():
            self.worker_thread.join(timeout=5)
        self.logger.info(f"Agent {self.name} stopped")
    
    def assign_task(self, task: Task):
        """Assign a task to this agent."""
        task.assigned_agent = self.name
        priority_tuple = (task.priority.value, time.time(), task)
        self.task_queue.put(priority_tuple)
        self.logger.info(f"Task {task.task_id} assigned to agent {self.name}")
    
    def _worker_loop(self):
        """Main worker loop for processing tasks."""
        while self.is_running:
            try:
                # Wait for task with timeout
                priority_tuple = self.task_queue.get(timeout=1)
                priority, timestamp, task = priority_tuple
                
                task.started_at = datetime.now()
                task.status = "in_progress"
                self.logger.info(f"Starting task {task.task_id}: {task.description}")
                
                try:
                    # Execute task
                    result = asyncio.run(self.execute_task(task))
                    task.result = result
                    task.status = "completed"
                    task.completed_at = datetime.now()
                    self.logger.info(f"Completed task {task.task_id}")
                    
                except Exception as e:
                    task.error = str(e)
                    task.status = "failed"
                    task.completed_at = datetime.now()
                    self.logger.error(f"Task {task.task_id} failed: {e}")
                
                # Notify orchestrator of completion
                self.orchestrator.task_completed(task)
                self.task_queue.task_done()
                
            except queue.Empty:
                continue
            except Exception as e:
                self.logger.error(f"Worker loop error: {e}")

class DocumentMonitor:
    """Monitors PRD and CLAUDE.md for changes"""
    
    def __init__(self, project_root: Path, orchestrator: 'AgentOrchestrator'):
        self.project_root = project_root
        self.orchestrator = orchestrator
        self.logger = logging.getLogger("DocumentMonitor")
        self.prd_path = project_root / "docs" / "PRD.md"
        self.claude_path = project_root / "ai" / "CLAUDE.md"
        self.prd_hash = self._compute_hash(self.prd_path)
        self.claude_hash = self._compute_hash(self.claude_path)
        self.monitoring = False
        self.monitor_thread = None
        
    def _compute_hash(self, file_path: Path) -> str:
        """Compute MD5 hash of file contents"""
        if not file_path.exists():
            return ""
        with open(file_path, 'rb') as f:
            return hashlib.md5(f.read()).hexdigest()
    
    def check_for_changes(self) -> Dict[str, bool]:
        """Check if PRD or CLAUDE.md have changed"""
        changes = {"prd": False, "claude": False}
        
        new_prd_hash = self._compute_hash(self.prd_path)
        if new_prd_hash != self.prd_hash:
            changes["prd"] = True
            self.prd_hash = new_prd_hash
            self.logger.info(f"PRD.md has been modified at {datetime.now()}")
            self._trigger_prd_update()
        
        new_claude_hash = self._compute_hash(self.claude_path)
        if new_claude_hash != self.claude_hash:
            changes["claude"] = True
            self.claude_hash = new_claude_hash
            self.logger.info(f"CLAUDE.md has been modified at {datetime.now()}")
            self._trigger_claude_update()
        
        return changes
    
    def _trigger_prd_update(self):
        """Trigger tasks when PRD changes"""
        self.logger.info("PRD changed - triggering requirements analysis")
        task = Task(
            task_id=f"prd_update_{int(time.time())}",
            task_type=TaskType.REQUIREMENTS_ANALYSIS,
            priority=TaskPriority.HIGH,
            description="Analyze PRD changes and update implementation",
            parameters={"document": "PRD.md"}
        )
        self.orchestrator.submit_task(task)
    
    def _trigger_claude_update(self):
        """Trigger tasks when CLAUDE.md changes"""
        self.logger.info("CLAUDE.md changed - triggering requirements analysis")
        task = Task(
            task_id=f"claude_update_{int(time.time())}",
            task_type=TaskType.REQUIREMENTS_ANALYSIS,
            priority=TaskPriority.HIGH,
            description="Analyze CLAUDE.md changes and update implementation",
            parameters={"document": "CLAUDE.md"}
        )
        self.orchestrator.submit_task(task)
    
    def start_monitoring(self, interval: int = 30):
        """Start monitoring documents in background thread"""
        self.monitoring = True
        self.monitor_thread = threading.Thread(
            target=self._monitor_loop, 
            args=(interval,), 
            daemon=True
        )
        self.monitor_thread.start()
        self.logger.info(f"Started document monitoring (interval: {interval}s)")
    
    def stop_monitoring(self):
        """Stop monitoring documents"""
        self.monitoring = False
        if self.monitor_thread:
            self.monitor_thread.join(timeout=5)
        self.logger.info("Stopped document monitoring")
    
    def _monitor_loop(self, interval: int):
        """Background monitoring loop"""
        while self.monitoring:
            self.check_for_changes()
            time.sleep(interval)

class AgentOrchestrator:
    """Main orchestrator that coordinates all coding agents."""
    
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.logger = logging.getLogger("AgentOrchestrator")
        self.agents: Dict[str, Agent] = {}
        self.task_history: List[Task] = []
        self.active_tasks: Dict[str, Task] = {}
        self.task_completion_callbacks: List[Callable] = []
        self.lock = threading.RLock()
        
        # Load project configuration
        self.config = self._load_config()
        
        # Initialize communication channels
        self.event_bus = queue.Queue()
        
        # Initialize document monitor
        self.doc_monitor = DocumentMonitor(project_root, self)
        
        self.logger.info("AgentOrchestrator initialized")
    
    def _load_config(self) -> Dict[str, Any]:
        """Load project configuration."""
        config_file = self.project_root / "agent_config.yaml"
        if config_file.exists():
            with open(config_file, 'r') as f:
                return yaml.safe_load(f)
        return {
            "project_name": "FastestJSONInTheWest",
            "cpp_standard": "C++23",
            "primary_compiler": "clang++-21",
            "build_system": "cmake",
            "test_framework": "gtest"
        }
    
    def register_agent(self, agent: Agent):
        """Register a new agent with the orchestrator."""
        with self.lock:
            self.agents[agent.name] = agent
            self.logger.info(f"Registered agent: {agent.name}")
    
    def start_all_agents(self):
        """Start all registered agents."""
        with self.lock:
            for agent in self.agents.values():
                agent.start()
        
        # Start document monitoring
        self.doc_monitor.start_monitoring(interval=30)
        
        self.logger.info("All agents started")
    
    def stop_all_agents(self):
        """Stop all registered agents."""
        # Stop document monitoring first
        self.doc_monitor.stop_monitoring()
        
        with self.lock:
            for agent in self.agents.values():
                agent.stop()
        self.logger.info("All agents stopped")
    
    def submit_task(self, task: Task) -> str:
        """Submit a task to be executed by appropriate agent."""
        with self.lock:
            # Determine appropriate agent for task
            agent_name = self._get_agent_for_task(task.task_type)
            if not agent_name or agent_name not in self.agents:
                raise ValueError(f"No suitable agent found for task type: {task.task_type}")
            
            # Assign task to agent
            agent = self.agents[agent_name]
            agent.assign_task(task)
            
            # Track active task
            self.active_tasks[task.task_id] = task
            self.logger.info(f"Task {task.task_id} submitted to agent {agent_name}")
            
            return task.task_id
    
    def _get_agent_for_task(self, task_type: TaskType) -> Optional[str]:
        """Determine which agent should handle a specific task type."""
        agent_mapping = {
            TaskType.CODE_GENERATION: "ImplementationAgent",
            TaskType.ENVIRONMENT_SETUP: "EnvironmentAgent",
            TaskType.DOCUMENTATION_UPDATE: "DocumentationAgent",
            TaskType.TEST_GENERATION: "TestingAgent",
            TaskType.CONFIG_UPDATE: "ConfigurationAgent",
            TaskType.BUILD_VALIDATION: "ConfigurationAgent",
            TaskType.DEPENDENCY_CHECK: "EnvironmentAgent"
        }
        return agent_mapping.get(task_type)
    
    def task_completed(self, task: Task):
        """Callback when a task is completed."""
        with self.lock:
            if task.task_id in self.active_tasks:
                del self.active_tasks[task.task_id]
            
            self.task_history.append(task)
            self.logger.info(f"Task {task.task_id} completed with status: {task.status}")
            
            # Trigger any completion callbacks
            for callback in self.task_completion_callbacks:
                try:
                    callback(task)
                except Exception as e:
                    self.logger.error(f"Callback error: {e}")
    
    def get_task_status(self, task_id: str) -> Optional[Dict[str, Any]]:
        """Get the status of a specific task."""
        with self.lock:
            # Check active tasks
            if task_id in self.active_tasks:
                return asdict(self.active_tasks[task_id])
            
            # Check task history
            for task in self.task_history:
                if task.task_id == task_id:
                    return asdict(task)
            
            return None
    
    def get_agent_status(self) -> Dict[str, Dict[str, Any]]:
        """Get status of all agents."""
        with self.lock:
            status = {}
            for name, agent in self.agents.items():
                status[name] = {
                    "running": agent.is_running,
                    "queue_size": agent.task_queue.qsize()
                }
            return status
    
    def broadcast_event(self, event: Dict[str, Any]):
        """Broadcast an event to all agents."""
        self.event_bus.put(event)
        self.logger.info(f"Event broadcasted: {event.get('type', 'unknown')}")

    def create_task(self, task_type: TaskType, description: str, 
                   parameters: Dict[str, Any], priority: TaskPriority = TaskPriority.NORMAL) -> Task:
        """Create a new task with unique ID."""
        task_id = f"{task_type.value}_{int(time.time() * 1000)}"
        return Task(
            task_id=task_id,
            task_type=task_type,
            priority=priority,
            description=description,
            parameters=parameters
        )

if __name__ == "__main__":
    # Example usage will be implemented in the main execution script
    pass