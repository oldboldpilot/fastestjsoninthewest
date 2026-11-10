#!/bin/bash
# FastJSON Python Bindings Build Script
# Automates CMake configuration, compilation, and testing

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"

CMAKE_BIN="${CMAKE_BIN:-$(command -v cmake 2>/dev/null || echo "/usr/local/bin/cmake")}"

if [ -z "${CLANG_BIN:-}" ]; then
    for candidate in \
        "/home/linuxbrew/.linuxbrew/bin/clang++" \
        "/opt/clang-23/bin/clang++" \
        "/usr/lib/llvm-23/bin/clang++" \
        "$(command -v clang++-23 2>/dev/null)" \
        "/opt/homebrew/opt/llvm/bin/clang++" \
        "/usr/local/bin/clang++" \
        "$(command -v clang++ 2>/dev/null)"; do
        if [ -n "$candidate" ] && [ -x "$candidate" ]; then
            CLANG_BIN="$candidate"
            break
        fi
    done
fi
CLANG_C_BIN="${CLANG_BIN%/*}/clang"

echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║       FastJSON Python Bindings Build Script           ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"

print_status() { echo -e "${GREEN}[✓]${NC} $1"; }
print_error()  { echo -e "${RED}[✗]${NC} $1"; exit 1; }
print_info()   { echo -e "${YELLOW}[i]${NC} $1"; }

print_info "Checking prerequisites..."

if ! command -v "$CMAKE_BIN" &> /dev/null; then
    print_error "CMake not found at $CMAKE_BIN"
fi
print_status "CMake found: $CMAKE_BIN"

if [ -z "$CLANG_BIN" ] || ! [ -x "$CLANG_BIN" ]; then
    print_error "Clang++ not found"
fi
print_status "Clang++ found: $CLANG_BIN"

if [ -x "${PROJECT_DIR}/.venv/bin/python" ]; then
    PYTHON_BIN="${PROJECT_DIR}/.venv/bin/python"
else
    PYTHON_BIN=$(command -v python3 || command -v python || echo "")
fi
if [ -z "$PYTHON_BIN" ]; then
    print_error "Python not found"
fi
print_status "Python found: $PYTHON_BIN"

NANOBIND_DIR=$("$PYTHON_BIN" -c "import nanobind; print(nanobind.cmake_dir())" 2>/dev/null || echo "")
if [ -z "$NANOBIND_DIR" ]; then
    print_error "nanobind not found in $PYTHON_BIN. Run 'uv sync' or 'pip install nanobind'."
fi
print_status "nanobind found at: $NANOBIND_DIR"

print_info "Preparing build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
print_status "Build directory ready: $BUILD_DIR"

print_info "Configuring CMake..."
"$CMAKE_BIN" -G Ninja \
    -DCMAKE_C_COMPILER="$CLANG_C_BIN" \
    -DCMAKE_CXX_COMPILER="$CLANG_BIN" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPython_EXECUTABLE="$PYTHON_BIN" \
    "-Dnanobind_ROOT=$NANOBIND_DIR" \
    .. || print_error "CMake configuration failed"

print_status "CMake configuration successful"

print_info "Compiling Python extension..."
"$CMAKE_BIN" --build . -j"$(nproc)" || print_error "Build failed"
print_status "Build successful"

cd "$PROJECT_DIR"
export PYTHONPATH="${BUILD_DIR}:${BUILD_DIR}/lib:${PYTHONPATH:-}"

print_info "Verifying module..."
if "$PYTHON_BIN" -c "import sys; sys.path.insert(0, '${BUILD_DIR}'); sys.path.insert(0, '${BUILD_DIR}/lib'); import fastjson; print(f'Version: {fastjson.__version__}')" ; then
    print_status "Module verification successful"
else
    print_error "Module verification failed"
fi

if "$PYTHON_BIN" -m pytest --version &> /dev/null; then
    print_info "Running test suite..."
    PYTHONPATH="${BUILD_DIR}:${BUILD_DIR}/lib" "$PYTHON_BIN" -m pytest tests/ -v --tb=short -x || print_error "Tests failed"
    print_status "All tests passed"
else
    print_info "pytest not installed in $PYTHON_BIN; skipping test run."
fi

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║            Build Completed Successfully! ✓             ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════╝${NC}"
