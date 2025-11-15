#!/bin/bash
# FastestJSONInTheWest Ansible Deployment Script
# This script executes the Ansible playbook to set up the development environment

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
ANSIBLE_DIR="$SCRIPT_DIR"

echo -e "${BLUE}FastestJSONInTheWest Development Environment Setup${NC}"
echo -e "${BLUE}====================================================${NC}"

# Check if Ansible is installed
if ! command -v ansible-playbook &> /dev/null; then
    echo -e "${YELLOW}Ansible not found. Installing Ansible...${NC}"
    
    if command -v pip3 &> /dev/null; then
        pip3 install ansible
    elif command -v apt-get &> /dev/null; then
        sudo apt-get update
        sudo apt-get install -y ansible
    elif command -v yum &> /dev/null; then
        sudo yum install -y ansible
    elif command -v brew &> /dev/null; then
        brew install ansible
    else
        echo -e "${RED}Error: Cannot install Ansible. Please install it manually.${NC}"
        exit 1
    fi
fi

# Verify Ansible installation
echo -e "${GREEN}Ansible version:${NC}"
ansible --version

echo ""
echo -e "${YELLOW}Available deployment options:${NC}"
echo "1. Local development setup (localhost)"
echo "2. Remote development servers"
echo "3. Production servers"
echo "4. Custom inventory"

read -p "Select option [1-4]: " OPTION

case $OPTION in
    1)
        INVENTORY="hosts.ini"
        TARGET="local"
        echo -e "${GREEN}Setting up local development environment...${NC}"
        ;;
    2)
        INVENTORY="hosts.ini"
        TARGET="development_servers"
        echo -e "${GREEN}Setting up remote development servers...${NC}"
        ;;
    3)
        INVENTORY="hosts.ini"
        TARGET="production_servers"
        echo -e "${GREEN}Setting up production servers...${NC}"
        ;;
    4)
        read -p "Enter custom inventory file path: " CUSTOM_INVENTORY
        INVENTORY="$CUSTOM_INVENTORY"
        read -p "Enter target hosts group: " TARGET
        echo -e "${GREEN}Using custom configuration...${NC}"
        ;;
    *)
        echo -e "${RED}Invalid option. Using local setup.${NC}"
        INVENTORY="hosts.ini"
        TARGET="local"
        ;;
esac

echo ""
echo -e "${YELLOW}Configuration:${NC}"
echo "Inventory: $INVENTORY"
echo "Target: $TARGET"
echo "Playbook: comprehensive_setup.yml"

# Prompt for confirmation
read -p "Continue with deployment? [y/N]: " CONFIRM
if [[ ! $CONFIRM =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Deployment cancelled.${NC}"
    exit 0
fi

echo ""
echo -e "${GREEN}Starting Ansible deployment...${NC}"

# Change to ansible directory
cd "$ANSIBLE_DIR"

# Run syntax check
echo -e "${BLUE}Checking playbook syntax...${NC}"
if ansible-playbook --syntax-check -i "$INVENTORY" comprehensive_setup.yml; then
    echo -e "${GREEN}✓ Syntax check passed${NC}"
else
    echo -e "${RED}✗ Syntax check failed${NC}"
    exit 1
fi

# Run the playbook
echo ""
echo -e "${BLUE}Executing playbook...${NC}"

ANSIBLE_CMD="ansible-playbook -i $INVENTORY comprehensive_setup.yml"

# Add target limitation if not all hosts
if [ "$TARGET" != "all" ]; then
    ANSIBLE_CMD="$ANSIBLE_CMD --limit $TARGET"
fi

# Add verbose output if requested
read -p "Enable verbose output? [y/N]: " VERBOSE
if [[ $VERBOSE =~ ^[Yy]$ ]]; then
    ANSIBLE_CMD="$ANSIBLE_CMD -vvv"
fi

# Execute the playbook
echo -e "${GREEN}Executing: $ANSIBLE_CMD${NC}"
echo ""

if eval "$ANSIBLE_CMD"; then
    echo ""
    echo -e "${GREEN}✓ Deployment completed successfully!${NC}"
    echo ""
    echo -e "${YELLOW}Next steps:${NC}"
    echo "1. Source the environment: source /etc/profile.d/fastjson-dev.sh"
    echo "2. Verify installation: clang++-21 --version && cmake --version"
    echo "3. Clone and build your project"
    echo ""
    echo -e "${BLUE}Environment information saved to ~/fastjson_environment_info.txt${NC}"
else
    echo ""
    echo -e "${RED}✗ Deployment failed!${NC}"
    echo "Check the output above for error details."
    exit 1
fi