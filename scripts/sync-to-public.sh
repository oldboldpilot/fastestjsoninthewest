#!/bin/bash

################################################################################
#                    SYNC PRIVATE TO PUBLIC REPOSITORY                         #
################################################################################
#
# This script synchronizes selected folders and files from the private
# FastestJSONInTheWest repository to the public FastestJSONInTheWest-Public
# repository, while excluding development-only artifacts.
#
# Usage:
#   ./sync-to-public.sh                 # Sync to default location
#   ./sync-to-public.sh /path/to/public # Sync to custom location
#
################################################################################

set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRIVATE_REPO="$(dirname "$SCRIPT_DIR")"
PUBLIC_REPO="${1:-$(dirname "$PRIVATE_REPO")/FastestJSONInTheWest-Public}"

# Ensure public repo exists
if [ ! -d "$PUBLIC_REPO/.git" ]; then
    echo -e "${RED}✗ Error: Public repository not found at $PUBLIC_REPO${NC}"
    echo -e "${YELLOW}[i] Please ensure FastestJSONInTheWest-Public exists${NC}"
    exit 1
fi

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     SYNCING PRIVATE TO PUBLIC REPOSITORY                     ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}[*] Source (Private)${NC}"
echo "    $PRIVATE_REPO"
echo ""
echo -e "${BLUE}[*] Target (Public)${NC}"
echo "    $PUBLIC_REPO"
echo ""

# Define folders to sync
FOLDERS_TO_SYNC=(
    "modules"
    "python_bindings"
    "tests"
    "docs"
    "documents"
    "examples"
    "tools"
    "ai"
    "benchmarks"
    "external"
)

# Define files to sync
FILES_TO_SYNC=(
    "README_PUBLIC.md:README.md"
    "LICENSE_BSD_4_CLAUSE"
    "CMakeLists.txt"
    ".clang-format"
    ".clang-tidy"
    ".gitmodules"
    "WORKSPACE"
    "BUILD.bazel"
)

# Function to exclude patterns from rsync
get_exclude_patterns() {
    cat <<'EOF'
build/
build_*
.venv/
.pytest_cache/
.benchmarks/
__pycache__/
*.pyc
*.o
*.a
*.so
*.dylib
.git/
.vscode/
.ansible/
.session_state.md*
.project_session_state.md
.llvm_build_*
valgrind_results/
valgrind-openmp.supp
openmp_scaling_results.json
.venv
EOF
}

# Function to copy folders with exclusions
sync_folder() {
    local folder=$1
    local src="$PRIVATE_REPO/$folder"
    local dest="$PUBLIC_REPO/$folder"
    
    if [ ! -d "$src" ]; then
        echo -e "${YELLOW}[!] Skipping $folder (not found)${NC}"
        return
    fi
    
    echo -e "${BLUE}[*] Syncing $folder/${NC}"
    
    # Build rsync exclude options
    local exclude_opts=""
    while IFS= read -r pattern; do
        exclude_opts="$exclude_opts --exclude=$pattern"
    done < <(get_exclude_patterns)
    
    # Use rsync to sync with exclusions
    rsync -av --delete $exclude_opts "$src/" "$dest/" 2>&1 | grep -E "(^|/).*" | head -20
    
    echo -e "${GREEN}    ✓ Complete${NC}"
}

# Function to copy single file, optionally with rename
sync_file() {
    local file_spec=$1
    local src_file=$(echo "$file_spec" | cut -d: -f1)
    local dest_file=$(echo "$file_spec" | cut -d: -f2)
    [ -z "$dest_file" ] && dest_file="$src_file"
    
    local src="$PRIVATE_REPO/$src_file"
    local dest="$PUBLIC_REPO/$dest_file"
    
    if [ ! -f "$src" ]; then
        echo -e "${YELLOW}[!] Skipping $src_file (not found)${NC}"
        return
    fi
    
    echo -e "${BLUE}[*] Copying $src_file${NC}"
    cp "$src" "$dest"
    echo -e "${GREEN}    ✓ Complete${NC}"
}

# Sync all folders
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Syncing Folders${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
for folder in "${FOLDERS_TO_SYNC[@]}"; do
    sync_folder "$folder"
done

# Sync all files
echo ""
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Syncing Root Files${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
for file in "${FILES_TO_SYNC[@]}"; do
    sync_file "$file"
done

# Initialize submodules in public repository
echo ""
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Initializing Git Submodules${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

cd "$PUBLIC_REPO"
if [ -f .gitmodules ]; then
    echo -e "${BLUE}[*] Initializing and updating submodules...${NC}"
    git submodule update --init --recursive 2>&1 | head -10
    echo -e "${GREEN}    ✓ Submodules updated${NC}"
else
    echo -e "${YELLOW}[!] No .gitmodules file found${NC}"
fi

# Summary of changes
echo ""
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Git Status in Public Repository${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

cd "$PUBLIC_REPO"
echo ""
git status --short | head -30
echo ""

# Count changes
STAGED=$(git diff --cached --name-only | wc -l)
UNSTAGED=$(git diff --name-only | wc -l)
UNTRACKED=$(git ls-files --others --exclude-standard | wc -l)

echo -e "${BLUE}[*] Changes Summary:${NC}"
echo "    Staged: $STAGED files"
echo "    Unstaged: $UNSTAGED files"
echo "    Untracked: $UNTRACKED files"
echo ""

# Ask if user wants to commit
if [ "$STAGED" -gt 0 ] || [ "$UNSTAGED" -gt 0 ]; then
    echo -e "${YELLOW}[?] Do you want to commit these changes? (y/n)${NC}"
    read -r response
    
    if [ "$response" = "y" ] || [ "$response" = "Y" ]; then
        git add -A
        
        # Generate commit message
        COMMIT_MSG="chore: Sync from private repository

Synced folders:
$(printf '%s\n' "${FOLDERS_TO_SYNC[@]}" | sed 's/^/  - /')

Updated files:
$(printf '%s\n' "${FILES_TO_SYNC[@]}" | sed 's/:.*/ (renamed)/' | sed 's/^/  - /')

Excludes development artifacts, build outputs, and session state files."
        
        git commit -m "$COMMIT_MSG"
        echo -e "${GREEN}✓ Committed successfully${NC}"
        echo ""
        git log --oneline | head -3
    fi
fi

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║     ✓ SYNC COMPLETE                                           ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}[*] Next Steps:${NC}"
echo "    1. Review changes: git diff HEAD~1"
echo "    2. Push to remote: git push origin main"
echo "    3. Or make additional changes and commit again"
echo ""
