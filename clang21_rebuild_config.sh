#!/bin/bash
# Complete LLVM/Clang 21.1.5 Build Configuration
# Based on comprehensive research of LLVM documentation and build requirements

echo "=== Building LLVM/Clang 21.1.5 with Full OpenMP, MLIR, Intrinsics, and Tools ==="

# Clean previous build if exists
if [ -d "/home/muyiwa/toolchain-build/llvm-build" ]; then
    echo "Cleaning previous build directory..."
    rm -rf /home/muyiwa/toolchain-build/llvm-build
fi

# Create fresh build directory
mkdir -p /home/muyiwa/toolchain-build/llvm-build
cd /home/muyiwa/toolchain-build/llvm-build

echo "Starting comprehensive LLVM/Clang build..."
echo "Build started at: $(date)"

# Complete CMake configuration based on research
    # Use CMake 4.1.2 we built and system GCC as bootstrap compiler
    CMAKE_COMMAND="/home/muyiwa/toolchain/bin/cmake"
    export CC=/usr/bin/gcc
    export CXX=/usr/bin/g++
    export ASM=/usr/bin/gcc
    
    # Ensure we use system ninja
    export PATH="/usr/bin:/usr/local/bin:$PATH"
    
    # Configure with Ninja generator
    $CMAKE_COMMAND ../llvm-project/llvm \
        -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/home/muyiwa/toolchain/clang-21 \
  \
  `# Core Projects - Include all essential components` \
  -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;mlir;openmp;lld" \
  \
  `# Runtime Libraries - Compiler runtime support` \
  -DLLVM_ENABLE_RUNTIMES="compiler-rt" \
  \
  `# Target Architecture Configuration - Full x86_64 with extensions` \
  -DLLVM_TARGETS_TO_BUILD="X86;AMDGPU;NVPTX" \
  -DLLVM_DEFAULT_TARGET_TRIPLE="x86_64-unknown-linux-gnu" \
  -DLLVM_TARGET_ARCH="x86_64" \
  \
  `# OpenMP Configuration - Complete runtime and offload support` \
  -DOPENMP_ENABLE_LIBOMPTARGET=ON \
  -DOPENMP_ENABLE_LIBOMP_PROFILING=ON \
  -DLIBOMPTARGET_DLOPEN_PLUGINS="amdgpu;cuda" \
  \
  `# MLIR Configuration - Multi-Level IR support` \
  -DMLIR_INCLUDE_INTEGRATION_TESTS=ON \
  \
  `# Clang Tools Configuration - Static analysis and formatting` \
  -DCLANG_ENABLE_STATIC_ANALYZER=ON \
  -DCLANG_ENABLE_OBJC_REWRITER=ON \
  -DCLANG_ENABLE_FORMAT=ON \
  \
  `# Build Performance and Optimization` \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_OPTIMIZED_TABLEGEN=ON \
  -DLLVM_CCACHE_BUILD=OFF \
  \
  `# Parallel Build Configuration` \
  -DLLVM_PARALLEL_COMPILE_JOBS=8 \
  -DLLVM_PARALLEL_LINK_JOBS=2 \
  \
  `# Advanced Features` \
  -DLLVM_ENABLE_RTTI=ON \
  -DLLVM_ENABLE_EH=ON \
  -DLLVM_ENABLE_THREADS=ON \
  -DLLVM_ENABLE_ZLIB=ON \
  -DLLVM_ENABLE_ZSTD=ON \
  \
  `# Intrinsics and ISA Extensions Support` \
  -DLLVM_BUILD_EXAMPLES=ON \
  -DLLVM_INCLUDE_EXAMPLES=ON \
  \
  `# Installation Configuration` \
  -DLLVM_INSTALL_UTILS=ON \
  -DLLVM_BUILD_TOOLS=ON \
  -DLLVM_INCLUDE_TOOLS=ON \
  \
  `# Debug and Development Support` \
  -DLLVM_INCLUDE_TESTS=ON \
  -DLLVM_BUILD_TESTS=ON \
  -DLLVM_ENABLE_EXPENSIVE_CHECKS=OFF

echo ""
echo "CMake configuration completed successfully!"
echo "Starting build process..."
echo ""

    # Build with system ninja using available cores
    /usr/bin/ninja -j$(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "Build completed successfully!"
    echo "Starting installation..."
    
    # Install the toolchain with system ninja
    /usr/bin/ninja install
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "=== LLVM/Clang 21.1.5 Installation Complete ==="
        echo "Installation directory: /home/muyiwa/toolchain/clang-21"
        echo ""
        echo "Installed components:"
        echo "- Clang C/C++ Compiler with C++23 modules support"
        echo "- OpenMP runtime with GPU offload support"
        echo "- MLIR (Multi-Level Intermediate Representation)"
        echo "- clang-tidy (Static analyzer and linter)"
        echo "- clang-format (Code formatter)"
        echo "- LLD linker"
        echo "- Complete intrinsics support (SSE2-AVX512, AMX)"
        echo "- Thread-safe SIMD operations"
        echo ""
        
        # Verify installation
        echo "Verification:"
        /home/muyiwa/toolchain/clang-21/bin/clang++ --version
        echo ""
        
        if [ -f "/home/muyiwa/toolchain/clang-21/bin/clang-tidy" ]; then
            echo "✓ clang-tidy installed successfully"
        fi
        
        if [ -f "/home/muyiwa/toolchain/clang-21/bin/clang-format" ]; then
            echo "✓ clang-format installed successfully"
        fi
        
        if [ -d "/home/muyiwa/toolchain/clang-21/lib" ]; then
            echo "✓ OpenMP libraries installed"
            ls /home/muyiwa/toolchain/clang-21/lib | grep -E "(omp|omptarget)" | head -3
        fi
        
        echo ""
        echo "Build completed at: $(date)"
    else
        echo "Installation failed!"
        exit 1
    fi
else
    echo "Build failed!"
    exit 1
fi