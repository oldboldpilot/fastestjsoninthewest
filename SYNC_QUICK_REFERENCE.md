# Sync Script Quick Reference

## One-Liner Quick Start

```bash
./sync-to-public.sh && cd ../FastestJSONInTheWest-Public && git push origin main
```

## Common Tasks

### Update Source Code
```bash
# Edit in private repo
vim modules/fastjson.cpp
git add modules/
git commit -m "refactor: improvements"

# Sync to public
./sync-to-public.sh
# Type 'y'

# Push to GitHub
cd ../FastestJSONInTheWest-Public
git push origin main
```

### Update Documentation
```bash
# Edit docs
vim docs/ARCHITECTURE.md
git add docs/
git commit -m "docs: update"

# Sync
./sync-to-public.sh
# Type 'y'

# Push
cd ../FastestJSONInTheWest-Public
git push origin main
```

### Update Python Bindings
```bash
# Edit bindings
vim python_bindings/src/fastjson_bindings.cpp
git add python_bindings/
git commit -m "feat: binding improvements"

# Sync
./sync-to-public.sh
# Type 'y'

# Push
cd ../FastestJSONInTheWest-Public
git push origin main
```

## Command Reference

| Command | Effect |
|---------|--------|
| `./sync-to-public.sh` | Sync to default public repo |
| `./sync-to-public.sh /path/to/repo` | Sync to custom location |
| `cd ../FastestJSONInTheWest-Public && git diff HEAD~1` | Review changes |
| `cd ../FastestJSONInTheWest-Public && git push origin main` | Push to GitHub |
| `cd ../FastestJSONInTheWest-Public && git log --oneline` | View commits |

## Folders Synced

- `modules/` - C++ core
- `python_bindings/` - Python bindings
- `tests/` - Tests
- `docs/` - Documentation
- `documents/` - Additional docs
- `examples/` - Examples
- `tools/` - Utilities
- `ai/` - Coding standards
- `benchmarks/` - Benchmarks

## Files Synced

- `README_PUBLIC.md` → `README.md`
- `LICENSE_BSD_4_CLAUSE`
- `CMakeLists.txt`
- `.clang-format`
- `.clang-tidy`

## What's Excluded

- All `build/` directories
- `.venv/` and `__pycache__/`
- `.pytest_cache/` and `.benchmarks/`
- `.vscode/`, `.ansible/`, `.git/`
- Session state and debug files

## Troubleshooting

**rsync: command not found**
```bash
sudo apt-get install rsync  # Ubuntu/Debian
brew install rsync          # macOS
```

**Public repo not found**
```bash
# Make sure FastestJSONInTheWest-Public exists
ls -la ../FastestJSONInTheWest-Public/
```

**Git conflicts**
```bash
cd ../FastestJSONInTheWest-Public
git status
git reset --hard HEAD  # Discard uncommitted changes if needed
```

## Performance

- First sync: ~5-30 seconds
- Incremental sync: < 1 second
- Total size: ~74 MB

## More Info

Full documentation: `cat SYNC_SCRIPT_README.md`
