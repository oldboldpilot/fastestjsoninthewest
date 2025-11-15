# FastestJSONInTheWest Ansible Environment Setup

This directory contains comprehensive Ansible playbooks and scripts for setting up the FastestJSONInTheWest development environment across multiple platforms.

## Quick Start

### Prerequisites

- Ansible 2.9 or later
- Python 3.8 or later
- SSH access to target machines (for remote setup)
- Sudo privileges on target systems

### Local Development Setup

```bash
# 1. Navigate to ansible directory
cd /home/muyiwa/Development/FastestJSONInTheWest/ansible

# 2. Run the automated deployment script
./deploy.sh

# 3. Select option 1 for local setup when prompted
```

### Manual Ansible Execution

```bash
# Local setup
ansible-playbook -i hosts.ini comprehensive_setup.yml --limit local

# With verbose output
ansible-playbook -i hosts.ini comprehensive_setup.yml --limit local -vvv

# Specific tags only
ansible-playbook -i hosts.ini comprehensive_setup.yml --tags "compiler,build"
```

## Files Description

### Core Playbooks

- **`comprehensive_setup.yml`**: Main playbook that installs all development dependencies
- **`fastjson_development_environment.yml`**: Generated playbook from the agent system
- **`hosts.ini`**: Inventory file defining target hosts and groups

### Scripts

- **`deploy.sh`**: Interactive deployment script with menu options
- **`setup_environment.sh`**: Direct shell setup script (generated)

### Generated Roles

- **`roles/clang_compiler/`**: Clang 21 compiler installation and configuration
- **`roles/cmake_build/`**: CMake build system setup
- **`roles/openmpi_parallel/`**: OpenMPI parallel computing setup
- **`roles/zeromq_messaging/`**: ZeroMQ messaging library installation

## Supported Platforms

### Linux Distributions

#### Ubuntu/Debian
- Ubuntu 20.04 LTS, 22.04 LTS, 24.04 LTS
- Debian 11, 12
- Uses APT package manager
- Installs from LLVM official repositories

#### CentOS/RHEL/Fedora
- CentOS 7, 8, 9
- RHEL 7, 8, 9
- Fedora 35+
- Uses YUM/DNF package manager

### macOS
- macOS 11 (Big Sur) or later
- Uses Homebrew package manager
- Requires Xcode Command Line Tools

## Installed Components

### Primary Toolchain

| Component | Version | Description |
|-----------|---------|-------------|
| Clang | 21 | Primary C++23 compiler |
| Clang++ | 21 | C++ compiler frontend |
| Clang-tidy | 21 | Static analysis tool |
| Clang-format | 21 | Code formatter |
| CMake | 3.28+ | Build system |
| OpenMPI | 5.0+ | Parallel computing |
| ZeroMQ | 4.3.5+ | High-performance messaging |

### Development Tools

- **Testing**: Google Test, Valgrind, GDB
- **Build**: Ninja, CCCache for faster compilation
- **Libraries**: libfmt, libbenchmark for performance testing
- **Python**: Conan, vcpkg package managers

### Environment Configuration

After installation, the following environment variables are configured:

```bash
export CC=clang-21
export CXX=clang++-21
export OMPI_CC=clang-21
export OMPI_CXX=clang++-21
export CMAKE_C_COMPILER=clang-21
export CMAKE_CXX_COMPILER=clang++-21
export CMAKE_BUILD_TYPE=Release
export CMAKE_CXX_STANDARD=23
```

## Usage Examples

### 1. Complete Local Setup

```bash
./deploy.sh
# Select option 1: Local development setup
# Follow prompts for confirmation and verbose output
```

### 2. Remote Development Servers

```bash
# Edit hosts.ini to add your development servers
nano hosts.ini

# Add servers under [development_servers] section:
# dev-server-1 ansible_host=192.168.1.100 ansible_user=developer

./deploy.sh
# Select option 2: Remote development servers
```

### 3. Production Deployment

```bash
# Configure production servers in hosts.ini
./deploy.sh
# Select option 3: Production servers
```

### 4. Specific Components Only

```bash
# Install only compiler toolchain
ansible-playbook -i hosts.ini comprehensive_setup.yml --tags "compiler"

# Install only parallel computing tools
ansible-playbook -i hosts.ini comprehensive_setup.yml --tags "parallel"

# Install messaging libraries only
ansible-playbook -i hosts.ini comprehensive_setup.yml --tags "messaging"
```

## Customization

### Adding New Platforms

To add support for a new Linux distribution:

1. Edit `comprehensive_setup.yml`
2. Add new conditional tasks for the distribution
3. Update package names for the specific package manager
4. Test the configuration

Example for Arch Linux:
```yaml
- name: Install Clang (Arch Linux)
  pacman:
    name:
      - clang
      - llvm
      - cmake
    state: present
  when: ansible_distribution == 'Archlinux'
```

### Modifying Versions

To change component versions:

1. Edit the `vars` section in `comprehensive_setup.yml`:
```yaml
vars:
  clang_version: 22  # Change to desired version
  cmake_version: "3.29.0"  # Update CMake version
```

2. Update any version-specific tasks accordingly

### Custom Package Sources

To use custom repositories or build from source:

1. Add custom repository configuration
2. Modify package installation tasks
3. Add validation steps for custom builds

## Troubleshooting

### Common Issues

#### 1. Clang 21 Not Available

**Error**: Package not found in repositories

**Solution**:
```bash
# Manual repository setup for Ubuntu
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
echo "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-21 main" | sudo tee /etc/apt/sources.list.d/llvm.list
sudo apt-get update
```

#### 2. CMake Version Too Old

**Error**: System CMake version insufficient

**Solution**: The playbook automatically builds CMake from source if needed, but you can force it:
```bash
ansible-playbook -i hosts.ini comprehensive_setup.yml --tags "cmake" --extra-vars "force_cmake_build=true"
```

#### 3. OpenMPI Configuration Issues

**Error**: MPI not finding Clang compiler

**Solution**: Ensure environment variables are sourced:
```bash
source /etc/profile.d/fastjson-dev.sh
mpirun --version
```

#### 4. Permission Denied Errors

**Error**: Sudo password not provided

**Solution**:
```bash
# Set environment variable
export ANSIBLE_BECOME_PASS=your_sudo_password

# Or use --ask-become-pass
ansible-playbook -i hosts.ini comprehensive_setup.yml --ask-become-pass
```

### Validation Commands

After installation, verify the setup:

```bash
# Check compiler
clang++-21 --version

# Check build system
cmake --version

# Check parallel computing
mpirun --version

# Check messaging library
pkg-config --modversion libzmq

# Check environment file
cat ~/fastjson_environment_info.txt
```

### Debug Mode

Run with maximum verbosity for troubleshooting:

```bash
ansible-playbook -i hosts.ini comprehensive_setup.yml -vvvv --diff
```

### Log Analysis

Check logs for specific issues:

```bash
# Ansible execution log
tail -f /var/log/ansible.log

# System package manager logs
# Ubuntu/Debian
tail -f /var/log/apt/history.log

# CentOS/RHEL/Fedora
tail -f /var/log/yum.log
```

## Advanced Configuration

### Custom Inventory

Create custom inventory for complex deployments:

```ini
# custom_inventory.ini
[webservers]
web[01:03].example.com

[databases]
db[01:02].example.com

[all:vars]
ansible_user=admin
ansible_ssh_private_key_file=~/.ssh/production.pem

[webservers:vars]
server_role=web

[databases:vars]
server_role=db
```

### Group Variables

Create group-specific configurations:

```bash
mkdir -p group_vars
echo "
clang_version: 21
cmake_version: '3.28.0'
enable_cuda: false
" > group_vars/webservers.yml
```

### Host Variables

Create host-specific configurations:

```bash
mkdir -p host_vars
echo "
special_packages:
  - additional-dev-tool
  - custom-library
" > host_vars/dev-server-1.yml
```

## Integration with CI/CD

### GitHub Actions Integration

```yaml
# .github/workflows/environment-setup.yml
name: Setup Development Environment
on:
  push:
    branches: [main]

jobs:
  setup:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup environment
        run: |
          cd ansible
          ./deploy.sh
```

### Jenkins Integration

```groovy
pipeline {
    agent any
    stages {
        stage('Environment Setup') {
            steps {
                sh '''
                    cd ansible
                    ansible-playbook -i hosts.ini comprehensive_setup.yml
                '''
            }
        }
    }
}
```

## Maintenance

### Regular Updates

Keep the environment updated:

```bash
# Update package cache
ansible all -i hosts.ini -m apt -a "update_cache=yes" --become

# Upgrade packages
ansible all -i hosts.ini -m apt -a "upgrade=yes" --become

# Update Ansible playbook from agents
cd ..
python3 run_agents.py
```

### Backup and Recovery

Backup critical configurations:

```bash
# Backup environment config
tar -czf fastjson-env-backup-$(date +%Y%m%d).tar.gz \
  /etc/profile.d/fastjson-dev.sh \
  ~/fastjson_environment_info.txt \
  ~/.bashrc \
  ~/.zshrc
```

## Support and Contributing

### Getting Help

1. Check the troubleshooting section above
2. Review the main project documentation
3. Examine agent system logs
4. Create an issue with full error output

### Contributing Improvements

1. Fork the repository
2. Create feature branch: `git checkout -b feature/ansible-improvement`
3. Test changes on supported platforms
4. Submit pull request with detailed description

### Testing Changes

Before submitting changes, test on multiple platforms:

```bash
# Test syntax
ansible-playbook --syntax-check comprehensive_setup.yml

# Dry run
ansible-playbook -i hosts.ini comprehensive_setup.yml --check

# Limited test
ansible-playbook -i hosts.ini comprehensive_setup.yml --limit test_host
```

---

*This Ansible configuration is maintained by the FastestJSONInTheWest Multi-Agent Development System*
*For questions or issues, refer to the main project documentation*