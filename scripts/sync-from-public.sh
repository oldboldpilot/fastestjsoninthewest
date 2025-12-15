#!/bin/bash

################################################################################
#                    SYNC PUBLIC TO PRIVATE REPOSITORY                         #
################################################################################
#
# This script synchronizes selected folders and files from the public
# FastestJSONInTheWest-Public repository back to the private
# FastestJSONInTheWest repository.
#
# Usage:
#   ./sync-from-public.sh                 # Sync from default location
#   ./sync-from-public.sh /path/to/public # Sync from custom location
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
echo -e "${BLUE}║     SYNCING PUBLIC TO PRIVATE REPOSITORY                     ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}[*] Source (Public)${NC}"
echo "    $PUBLIC_REPO"
echo ""
echo -e "${BLUE}[*] Target (Private)${NC}"
echo "    $PRIVATE_REPO"
echo ""

# Pull latest changes from public repository
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Updating Public Repository${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

cd "$PUBLIC_REPO"

# Check if there are uncommitted changes
if [ -n "$(git status --porcelain)" ]; then
    echo -e "${YELLOW}[!] Public repository has uncommitted changes${NC}"
    echo -e "${YELLOW}[?] Stash changes before pulling? (y/n)${NC}"
    read -r stash_response
    
    if [ "$stash_response" = "y" ] || [ "$stash_response" = "Y" ]; then
        echo -e "${BLUE}[*] Stashing uncommitted changes...${NC}"
        git stash push -m "Auto-stash before sync-from-public at $(date)"
        echo -e "${GREEN}    ✓ Changes stashed${NC}"
    else
        echo -e "${RED}✗ Cannot pull with uncommitted changes${NC}"
        echo -e "${YELLOW}[i] Please commit or stash changes in public repo first${NC}"
        exit 1
    fi
fi

echo -e "${BLUE}[*] Fetching latest changes from remote...${NC}"
if git fetch origin 2>&1 | head -5; then
    echo -e "${GREEN}    ✓ Fetch complete${NC}"
else
    echo -e "${RED}✗ Fetch failed${NC}"
    exit 1
fi

echo -e "${BLUE}[*] Pulling latest changes...${NC}"
if git pull origin main 2>&1 | head -10; then
    echo -e "${GREEN}    ✓ Pull complete${NC}"
else
    echo -e "${RED}✗ Pull failed${NC}"
    exit 1
fi

# Show current commit
echo -e "${BLUE}[*] Current commit:${NC}"
git log --oneline -1
echo ""

cd "$PRIVATE_REPO"

# Define folders to sync back
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
)

# Define files to sync back (reverse mapping)
FILES_TO_SYNC=(
    "README.md:README_PUBLIC.md"
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
    local src="$PUBLIC_REPO/$folder"
    local dest="$PRIVATE_REPO/$folder"
    
    if [ ! -d "$src" ]; then
        echo -e "${YELLOW}[!] Skipping $folder (not found in public repo)${NC}"
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
    
    local src="$PUBLIC_REPO/$src_file"
    local dest="$PRIVATE_REPO/$dest_file"
    
    if [ ! -f "$src" ]; then
        echo -e "${YELLOW}[!] Skipping $src_file (not found in public repo)${NC}"
        return
    fi
    
    echo -e "${BLUE}[*] Copying $src_file${NC}"
    cp "$src" "$dest"
    echo -e "${GREEN}    ✓ Complete${NC}"
}

# Warning about sync direction
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}⚠  WARNING: Reverse Sync${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}This will overwrite files in the PRIVATE repository${NC}"
echo -e "${YELLOW}with content from the PUBLIC repository.${NC}"
echo ""
echo -e "${YELLOW}Make sure you have committed any important changes in the${NC}"
echo -e "${YELLOW}private repository before proceeding.${NC}"
echo ""
echo -e "${YELLOW}[?] Continue with reverse sync? (y/n)${NC}"
read -r response

if [ "$response" != "y" ] && [ "$response" != "Y" ]; then
    echo -e "${RED}✗ Sync cancelled${NC}"
    exit 0
fi

# Sync all folders
echo ""
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

# Summary of changes
echo ""
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Git Status in Private Repository${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

cd "$PRIVATE_REPO"
echo ""
git status --short | head -30
echo ""

# Count changes
STAGED=$(git diff --cached --name-only 2>/dev/null | wc -l)
UNSTAGED=$(git diff --name-only 2>/dev/null | wc -l)
UNTRACKED=$(git ls-files --others --exclude-standard 2>/dev/null | wc -l)

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
        COMMIT_MSG="chore: Sync from public repository

Synced folders:
$(printf '%s\n' "${FOLDERS_TO_SYNC[@]}" | sed 's/^/  - /')

Updated files:
$(printf '%s\n' "${FILES_TO_SYNC[@]}" | sed 's/:/ -> /' | sed 's/^/  - /')

Reverse sync from public repository to incorporate external contributions."
        
        git config user.name "Olumuyiwa Oluwasanmi"
        git config --unset user.email 2>/dev/null || true
        git commit --author="Olumuyiwa Oluwasanmi <>" -m "$COMMIT_MSG"
        echo -e "${GREEN}✓ Committed successfully${NC}"
        echo ""
        git log --oneline | head -3
    fi
fi

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║     ✓ REVERSE SYNC COMPLETE                                   ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}[*] Next Steps:${NC}"
echo "    1. Review changes: git diff HEAD~1"
echo "    2. Test the changes thoroughly"
echo "    3. Push to remote: git push origin main"
echo "    4. Or make additional changes and commit again"
echo ""
