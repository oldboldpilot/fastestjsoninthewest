#!/usr/bin/env python3
"""
Main execution script for the FastestJSONInTheWest Multi-Agent System

This script initializes and coordinates all coding agents to generate
the complete development environment setup including Ansible playbooks.
"""

import asyncio
import sys
from pathlib import Path
import logging

# Add current directory to path for agent imports
sys.path.insert(0, str(Path(__file__).parent))

from agent_orchestrator import AgentOrchestrator, TaskType, TaskPriority
from implementation_agent import ImplementationAgent
from environment_agent import EnvironmentAgent

async def main():
    """Main execution function."""
    # Set up logging
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )
    logger = logging.getLogger("Main")
    
    # Initialize orchestrator
    project_root = Path(__file__).parent.parent
    orchestrator = AgentOrchestrator(project_root)
    
    try:
        # Register agents
        impl_agent = ImplementationAgent(orchestrator)
        env_agent = EnvironmentAgent(orchestrator)
        
        orchestrator.register_agent(impl_agent)
        orchestrator.register_agent(env_agent)
        
        # Start all agents
        orchestrator.start_all_agents()
        logger.info("All agents started successfully")
        
        # Generate Ansible playbook with source installations
        logger.info("Generating comprehensive source-based Ansible playbook...")
        
        playbook_task = orchestrator.create_task(
            task_type=TaskType.ENVIRONMENT_SETUP,
            description="Generate complete source-based Ansible playbook for development environment",
            parameters={
                "action": "generate_ansible_playbook",
                "playbook_name": "fastjson_development_environment",
                "platforms": ["ubuntu", "debian", "redhat", "centos", "fedora", "windows_wsl"],
                "include_all_dependencies": True,
                "install_from_source": True,
                "use_uv_python": True,
                "windows_winrm_support": True
            },
            priority=TaskPriority.HIGH
        )
        
        task_id = orchestrator.submit_task(playbook_task)
        logger.info(f"Submitted source-based Ansible playbook generation task: {task_id}")
        
        # Wait for task completion
        timeout = 120  # 2 minutes timeout for source-based setup
        for i in range(timeout):
            await asyncio.sleep(1)
            status = orchestrator.get_task_status(task_id)
            if status and status["status"] in ["completed", "failed"]:
                break
        
        # Get final status
        final_status = orchestrator.get_task_status(task_id)
        if final_status:
            if final_status["status"] == "completed":
                result = final_status["result"]
                logger.info("Source-based Ansible playbook generated successfully!")
                logger.info(f"Playbook file: {result.get('playbook_file')}")
                logger.info(f"Roles created: {len(result.get('roles_created', []))}")
                logger.info(f"UV Python environment configured: {result.get('uv_configured')}")
                logger.info(f"CMakeLists.txt updated: {result.get('cmake_updated')}")
                
                # Update CMakeLists.txt with source dependencies
                cmake_task = orchestrator.create_task(
                    task_type=TaskType.CONFIG_UPDATE,
                    description="Update CMakeLists.txt with source-built dependencies",
                    parameters={
                        "action": "update_cmake_dependencies",
                        "source_installations": True,
                        "custom_paths": result.get('dependency_paths', {})
                    },
                    priority=TaskPriority.HIGH
                )
                
                cmake_task_id = orchestrator.submit_task(cmake_task)
                
                # Wait for CMake update
                for i in range(60):
                    await asyncio.sleep(1)
                    cmake_status = orchestrator.get_task_status(cmake_task_id)
                    if cmake_status and cmake_status["status"] in ["completed", "failed"]:
                        break
                
                cmake_final = orchestrator.get_task_status(cmake_task_id)
                if cmake_final and cmake_final["status"] == "completed":
                    logger.info(f"CMakeLists.txt updated successfully")
                
            else:
                logger.error(f"Task failed: {final_status.get('error')}")
        else:
            logger.error("Task timed out or status unavailable")
        
        # Display agent status
        agent_status = orchestrator.get_agent_status()
        logger.info("Agent Status:")
        for agent_name, status in agent_status.items():
            logger.info(f"  {agent_name}: Running={status['running']}, Queue={status['queue_size']}")
        
    except Exception as e:
        logger.error(f"Error in main execution: {e}")
        return 1
    
    finally:
        # Clean shutdown
        orchestrator.stop_all_agents()
        logger.info("All agents stopped")
    
    return 0

if __name__ == "__main__":
    exit_code = asyncio.run(main())
    sys.exit(exit_code)