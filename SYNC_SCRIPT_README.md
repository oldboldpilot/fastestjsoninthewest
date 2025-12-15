# Sync Script Documentation

## Overview

`sync-to-public.sh` is a utility script that synchronizes selected folders and files from the private FastestJSONInTheWest repository to the public FastestJSONInTheWest-Public repository.

The script intelligently copies only the folders and files you want public while **automatically excluding**:
- Build artifacts and temporary builds
- Python virtual environments and caches
- IDE configurations
- Session state and debug files
- Local configuration files

## Usage

### Basic Usage (Default Location)

```bash
cd /home/muyiwa/Development/FastestJSONInTheWest
./sync-to-public.sh
```

This syncs to the default public repository at:
```
/home/muyiwa/Development/FastestJSONInTheWest-Public
```

### Custom Public Repository Location

```bash
./sync-to-public.sh /path/to/custom/public/repo
```

## What Gets Synced

### Folders (Complete Directories)
- `modules/` - Core C++ implementation
- `python_bindings/` - Python 3.13 bindings
- `tests/` - Test suite
- `docs/` - Documentation
- `documents/` - Additional documentation
- `examples/` - Code examples
- `tools/` - Utility scripts
- `ai/` - Coding standards
- `benchmarks/` - Benchmark code

### Root Files
- `README_PUBLIC.md` → `README.md` (renamed)
- `LICENSE_BSD_4_CLAUSE`
- `CMakeLists.txt`
- `.clang-format`
- `.clang-tidy`

## What Gets Excluded

The script uses smart exclusion patterns to skip:

**Build Artifacts:**
```
build/
build_*
*.o
*.a
*.so
*.dylib
```

**Python Development:**
```
.venv/
__pycache__/
*.pyc
.pytest_cache/
.benchmarks/
```

**IDE & Version Control:**
```
.git/
.vscode/
.ansible/
```

**Session & Debug Files:**
```
.session_state.md*
.project_session_state.md
.llvm_build_*
valgrind_results/
```

## How It Works

1. **Pre-flight Check**: Verifies the public repository exists
2. **Folder Sync**: Uses `rsync` with smart exclusion patterns
3. **File Sync**: Copies root files individually (with optional renaming)
4. **Status Report**: Shows git status and change summary
5. **Commit Prompt**: Asks if you want to commit changes

## Output Example

```
╔════════════════════════════════════════════════════════════════╗
║     SYNCING PRIVATE TO PUBLIC REPOSITORY                     ║
╚════════════════════════════════════════════════════════════════╝

[*] Source (Private)
    /home/muyiwa/Development/FastestJSONInTheWest

[*] Target (Public)
    /home/muyiwa/Development/FastestJSONInTheWest-Public

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Syncing Folders
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[*] Syncing modules/
    ✓ Complete
[*] Syncing python_bindings/
    ✓ Complete
[*] Syncing tests/
    ✓ Complete
...

[*] Changes Summary:
    Staged: 5 files
    Unstaged: 2 files
    Untracked: 0 files

[?] Do you want to commit these changes? (y/n)
```

## Workflow Example

### Step 1: Make changes in private repo
```bash
cd FastestJSONInTheWest
# ... make changes to source code, docs, etc.
git add -A && git commit -m "feat: Add new feature"
```

### Step 2: Sync to public repo
```bash
./sync-to-public.sh
# Review changes
# Type 'y' to commit
```

### Step 3: Push to GitHub
```bash
cd ../FastestJSONInTheWest-Public
git push origin main
```

## Advanced Usage

### Manual Sync Without Commit Prompt
```bash
./sync-to-public.sh
# When prompted, type 'n' to skip the commit
# Then manually edit/commit in the public repo
```

### Custom Exclusions

To add additional exclusions, edit the `get_exclude_patterns()` function in the script:

```bash
get_exclude_patterns() {
    cat <<'EOF'
build/
*.myextension
custom_pattern/
EOF
}
```

### Sync to Multiple Public Repos

```bash
# Sync to primary public repo
./sync-to-public.sh

# Sync to secondary public repo
./sync-to-public.sh /path/to/another/public/repo
```

## Dry Run (Safe Testing)

To see what would be synced without making changes:

```bash
# Comment out the rsync/cp commands to see what would happen
# Or use rsync with --dry-run flag directly
rsync -av --dry-run --delete \
  --exclude='build/' \
  --exclude='.venv/' \
  /path/to/private/modules/ /path/to/public/modules/
```

## Troubleshooting

### Error: Public repository not found
```
✗ Error: Public repository not found at /path/to/repo
[i] Please ensure FastestJSONInTheWest-Public exists
```

**Solution:** Ensure the public repository exists at the expected location or pass the correct path as an argument.

### Git conflicts in public repo
If the public repo has uncommitted changes:
```bash
cd ../FastestJSONInTheWest-Public
git status
# Either stash, commit, or discard changes before syncing
```

### rsync: command not found
```bash
# Install rsync
sudo apt-get install rsync  # Ubuntu/Debian
brew install rsync          # macOS
```

## Best Practices

1. **Commit before syncing**: Always commit changes in the private repo first
   ```bash
   git status
   git add -A && git commit -m "message"
   ```

2. **Review changes**: Check what's being synced before committing to public repo
   ```bash
   cd ../FastestJSONInTheWest-Public
   git diff --cached | less
   ```

3. **Keep sync frequent**: Run the sync script regularly to keep repos in sync
   ```bash
   # After significant changes
   ./sync-to-public.sh
   ```

4. **Create release tags**: Tag important versions in public repo
   ```bash
   cd ../FastestJSONInTheWest-Public
   git tag -a v1.0.0 -m "Release version 1.0.0"
   git push origin v1.0.0
   ```

## Performance

- **First sync**: ~5-30 seconds (depends on filesystem speed)
- **Incremental sync**: < 1 second (only changes detected)
- **Largest folder**: modules/ (~350KB+)
- **Total content**: ~70MB public repository

## Maintenance

The script is designed to be maintainable. To add new folders or files:

1. Edit `FOLDERS_TO_SYNC` array to add folders
2. Edit `FILES_TO_SYNC` array to add files
3. Update `get_exclude_patterns()` for new exclusion patterns

```bash
# Example: Add a new folder
FOLDERS_TO_SYNC=(
    "modules"
    "python_bindings"
    "my_new_folder"  # Add here
    ...
)
```

## Related Scripts

- `BUILD.sh` - Python bindings build automation
- `tools/run_valgrind_tests.sh` - Memory checking
- `tools/valgrind_comprehensive_test.sh` - Comprehensive tests

## License

This script is part of FastestJSONInTheWest and uses the same BSD 4-Clause License.
