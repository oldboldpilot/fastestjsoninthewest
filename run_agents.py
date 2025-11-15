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

# Add agents directory to path
sys.path.append(str(Path(__file__).parent / "agents"))

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
    project_root = Path("/home/muyiwa/Development/FastestJSONInTheWest")
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
        
        # Generate Ansible playbook
        logger.info("Generating comprehensive Ansible playbook...")
        
        playbook_task = orchestrator.create_task(
            task_type=TaskType.ENVIRONMENT_SETUP,
            description="Generate complete Ansible playbook for development environment",
            parameters={
                "action": "generate_ansible_playbook",
                "playbook_name": "fastjson_development_environment",
                "platforms": ["ubuntu", "centos", "macos"],
                "include_all_dependencies": True
            },
            priority=TaskPriority.HIGH
        )
        
        task_id = orchestrator.submit_task(playbook_task)
        logger.info(f"Submitted Ansible playbook generation task: {task_id}")
        
        # Wait for task completion
        timeout = 60  # 60 seconds timeout
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
                logger.info("Ansible playbook generated successfully!")
                logger.info(f"Playbook file: {result.get('playbook_file')}")
                logger.info(f"Roles created: {len(result.get('roles_created', []))}")
                logger.info(f"Inventory file: {result.get('inventory_file')}")
                
                # Create setup script
                logger.info("Creating automated setup script...")
                script_task = orchestrator.create_task(
                    task_type=TaskType.ENVIRONMENT_SETUP,
                    description="Create automated setup script",
                    parameters={"action": "create_setup_script"},
                    priority=TaskPriority.NORMAL
                )
                
                script_task_id = orchestrator.submit_task(script_task)
                
                # Wait for script completion
                for i in range(30):
                    await asyncio.sleep(1)
                    script_status = orchestrator.get_task_status(script_task_id)
                    if script_status and script_status["status"] in ["completed", "failed"]:
                        break
                
                script_final_status = orchestrator.get_task_status(script_task_id)
                if script_final_status and script_final_status["status"] == "completed":
                    script_result = script_final_status["result"]
                    logger.info(f"Setup script created: {script_result.get('script_file')}")
                
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