#!/bin/bash
# Build and test FastestJSONInTheWest Python bindings

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "FastestJSONInTheWest Python Bindings"
echo "Build and Test Verification"
echo "=========================================="
echo ""

# Step 1: Check dependencies
echo "[1/4] Checking dependencies..."
command -v uv >/dev/null 2>&1 || { echo "uv not found!"; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "CMake not found!"; exit 1; }
CLANG_BIN=""
for candidate in \
    "/home/linuxbrew/.linuxbrew/bin/clang++" \
    "/opt/clang-23/bin/clang++" \
    "/usr/lib/llvm-23/bin/clang++" \
    "$(command -v clang++-23 2>/dev/null)" \
    "/opt/homebrew/opt/llvm/bin/clang++"; do
    if [ -n "$candidate" ] && [ -x "$candidate" ]; then
        CLANG_BIN="$candidate"
        break
    fi
done
[ -n "$CLANG_BIN" ] || { echo "Clang 23+ not found!"; exit 1; }
echo "✓ All dependencies available (Clang: $CLANG_BIN)"
echo ""

# Step 2: Setup Python environment & build C++ extension via uv
echo "[2/4] Syncing Python environment and building C++23 extension..."
uv sync --reinstall-package fastjson
echo "✓ Extension built and installed into .venv"
echo ""

# Step 3: Verify module can be imported
echo "[3/4] Verifying module import..."
uv run python -c "import fastjson; print('✓ Imported fastjson', fastjson.__version__, 'from', fastjson.__file__)"
echo ""

# Step 4: Run tests
echo "[4/4] Running test suite..."
uv run pytest tests/ -v --tb=short

echo ""
echo "=========================================="
echo "Setup Complete!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  cd python_bindings"
echo "  uv run pytest tests/ -v                    # Run all tests"
echo "  uv run pytest tests/ -m benchmark -v       # Run benchmarks"
echo "  uv run pytest tests/ --cov=fastjson -v     # With coverage"
echo ""
echo "To import the module:"
echo "  python -c 'import sys; sys.path.insert(0, \"build/lib\"); import fastjson'"
echo ""
