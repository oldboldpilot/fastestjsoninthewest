#!/bin/bash
# FastJSON Python Bindings Build Script
# Automates CMake configuration, compilation, and testing

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
INSTALL_DIR="${PROJECT_DIR}/install"
PYTHON_VERSION="${PYTHON_VERSION:-3.13}"

# Tool paths (customize as needed)
CMAKE_BIN="${CMAKE_BIN:-/home/muyiwa/toolchain-build/cmake-4.1.2/bin/cmake}"
CLANG_BIN="${CLANG_BIN:-/home/muyiwa/toolchain/clang-21/bin/clang++}"
CLANG_C_BIN="${CLANG_BIN%/*}/clang"

echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║       FastJSON Python Bindings Build Script           ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"

# Function to print status
print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
    exit 1
}

print_info() {
    echo -e "${YELLOW}[i]${NC} $1"
}

# Check prerequisites
print_info "Checking prerequisites..."

if ! command -v $CMAKE_BIN &> /dev/null; then
    print_error "CMake not found at $CMAKE_BIN"
fi
print_status "CMake found"

if ! command -v $CLANG_BIN &> /dev/null; then
    print_error "Clang++ not found at $CLANG_BIN"
fi
print_status "Clang++ found"

PYTHON_BIN=$(which python${PYTHON_VERSION} || which python3 || which python)
if [ -z "$PYTHON_BIN" ]; then
    print_error "Python ${PYTHON_VERSION} not found"
fi
print_status "Python found: $PYTHON_BIN"

# Get pybind11 directory
PYBIND11_DIR=$($PYTHON_BIN -m pybind11 --cmakedir 2>/dev/null || echo "")
if [ -z "$PYBIND11_DIR" ]; then
    print_error "pybind11 not found. Install with: pip install pybind11"
fi
print_status "pybind11 found at: $PYBIND11_DIR"

# Create build directory
print_info "Preparing build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
print_status "Build directory ready: $BUILD_DIR"

# Configure with CMake
print_info "Configuring CMake..."
$CMAKE_BIN \
    -DCMAKE_C_COMPILER="$CLANG_C_BIN" \
    -DCMAKE_CXX_COMPILER="$CLANG_BIN" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPython3_EXECUTABLE="$PYTHON_BIN" \
    "-Dpybind11_ROOT=$PYBIND11_DIR" \
    .. || print_error "CMake configuration failed"

print_status "CMake configuration successful"

# Build
print_info "Compiling Python extension..."
make -j$(nproc) || print_error "Build failed"
print_status "Build successful: $BUILD_DIR/lib/fastjson*.so"

# Set up environment
cd "$PROJECT_DIR"
export LD_LIBRARY_PATH="/opt/clang-21/lib/x86_64-unknown-linux-gnu:${LD_LIBRARY_PATH}"
export PYTHONPATH="${BUILD_DIR}/lib:${PYTHONPATH}"

# Verify import
print_info "Verifying module..."
if $PYTHON_BIN -c "import sys; sys.path.insert(0, '${BUILD_DIR}/lib'); import fastjson; print(f'Version: {fastjson.__version__}')" 2>/dev/null; then
    print_status "Module verification successful"
else
    print_error "Module verification failed"
fi

# Optional: Run tests
if command -v pytest &> /dev/null; then
    print_info "Running test suite..."
    PYTHONPATH="${BUILD_DIR}/lib" pytest tests/ -v --tb=short -x || print_error "Tests failed"
    print_status "All tests passed"
else
    print_info "pytest not found (optional). Skipping tests."
    print_info "To run tests: pip install pytest && PYTHONPATH=${BUILD_DIR}/lib pytest tests/"
fi

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║            Build Completed Successfully! ✓             ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════╝${NC}"
echo ""
echo "To use the compiled module:"
echo "  1. Add to your Python path:"
echo "     export PYTHONPATH=${BUILD_DIR}/lib:\$PYTHONPATH"
echo ""
echo "  2. Or install system-wide:"
echo "     cd ${PROJECT_DIR} && pip install ."
echo ""
echo "  3. Or use directly in Python:"
echo "     import sys"
echo "     sys.path.insert(0, '${BUILD_DIR}/lib')"
echo "     import fastjson"
echo ""
echo "Quick test:"
echo "  PYTHONPATH=${BUILD_DIR}/lib python -c \"import fastjson; print(fastjson.parse('{\\\"x\\\": 1}'))\""
echo ""
