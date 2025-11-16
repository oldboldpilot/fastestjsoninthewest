# Complete LLVM/Clang 21.1.5 Build Configuration Research

## Executive Summary

Based on comprehensive research of official LLVM documentation, this document provides the definitive configuration for building LLVM/Clang 21.1.5 with full OpenMP functionality, MLIR support, intrinsics, and all Clang tools including clang-tidy and clang-format.

## Key Research Sources

1. **LLVM CMake Documentation**: https://llvm.org/docs/CMake.html
2. **MLIR Getting Started**: https://mlir.llvm.org/getting_started/
3. **OpenMP Support Documentation**: https://openmp.llvm.org/docs/SupportAndFAQ.html
4. **Clang Tools Overview**: https://clang.llvm.org/docs/ClangTools.html
5. **Clang Command Reference**: https://clang.llvm.org/docs/ClangCommandLineReference.html

## Core Configuration Components

### 1. OpenMP Runtime Support

**Research Finding**: OpenMP requires both project-level and runtime-level enablement:

```cmake
-DLLVM_ENABLE_PROJECTS="openmp"          # Build OpenMP project
-DLLVM_ENABLE_RUNTIMES="openmp"          # Build OpenMP runtime libraries
-DOPENMP_ENABLE_LIBOMPTARGET=ON         # Enable GPU offload support
-DOPENMP_ENABLE_LIBOMP_PROFILING=ON     # Enable profiling support
-DLIBOMPTARGET_DLOPEN_PLUGINS="amdgpu;cuda"  # Dynamic plugin loading
```

**Key Documentation**: 
- OpenMP requires separate compilation for host and device runtimes
- GPU offload support needs specific target architectures (AMDGPU, NVPTX)
- Dynamic plugin loading prevents build-time dependencies on CUDA/ROCm

### 2. MLIR (Multi-Level Intermediate Representation)

**Research Finding**: MLIR integration is straightforward but requires specific project inclusion:

```cmake
-DLLVM_ENABLE_PROJECTS="mlir"            # Include MLIR in build
-DMLIR_INCLUDE_INTEGRATION_TESTS=ON     # Enable integration tests
```

**Key Documentation**: 
- MLIR provides advanced compiler optimizations
- Integration tests ensure proper functionality
- No additional runtime dependencies required

### 3. Target Architecture and Intrinsics

**Research Finding**: Comprehensive intrinsics support requires proper target configuration:

```cmake
-DLLVM_TARGETS_TO_BUILD="X86;AMDGPU;NVPTX"  # Essential targets
-DLLVM_DEFAULT_TARGET_TRIPLE="x86_64-unknown-linux-gnu"
-DLLVM_TARGET_ARCH="x86_64"
```

**Intrinsics Coverage**: 
- **SSE2-SSE4.2**: Base x86_64 requirement
- **AVX/AVX2**: Included in X86 target
- **AVX-512**: Full support including AVX512F, AVX512BW, AVX512VBMI, AVX512VNNI
- **AMX-TMUL**: Advanced Matrix Extensions for tile operations
- All controlled by `-mavx512*` and `-mamx-*` flags at compile time

### 4. Clang Tools (clang-tidy, clang-format)

**Research Finding**: All tools included in `clang-tools-extra` project:

```cmake
-DLLVM_ENABLE_PROJECTS="clang-tools-extra"  # Include all extra tools
-DCLANG_ENABLE_STATIC_ANALYZER=ON          # Enable static analysis
-DCLANG_ENABLE_ARCMT=ON                    # Enable ARC migration tool
-DCLANG_ENABLE_FORMAT=ON                   # Enable clang-format
```

**Included Tools**:
- `clang-tidy`: Extensible C++ linter and static analyzer
- `clang-format`: Code formatter with configurable styles
- `clang-check`: Fast syntax checking
- `clangd`: Language server for IDE integration
- `clang-doc`: Documentation generator
- Additional refactoring and analysis tools

### 5. Performance and Build Optimizations

**Research Finding**: Build performance significantly improved with:

```cmake
-DLLVM_OPTIMIZED_TABLEGEN=ON            # Optimize TableGen for faster builds
-DLLVM_USE_LINKER=lld                   # Use LLD for faster linking
-DLLVM_CCACHE_BUILD=ON                  # Enable compilation caching
-DLLVM_PARALLEL_COMPILE_JOBS=8          # Parallel compilation
-DLLVM_PARALLEL_LINK_JOBS=2             # Parallel linking (memory limited)
```

### 6. Thread Safety and OpenMP Integration

**Research Finding**: OpenMP provides comprehensive thread safety for SIMD operations:

- **Atomic Operations**: Full support for lock-free algorithms
- **Thread-Local Storage**: Proper isolation of SIMD state
- **Memory Barriers**: Correct ordering for vectorized operations
- **Critical Sections**: Safe access to shared SIMD resources

## Complete Build Command

The research culminated in this comprehensive CMake configuration:

```bash
cmake ../llvm-project/llvm \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/home/muyiwa/toolchain/clang-21 \
  -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;mlir;openmp;lld" \
  -DLLVM_ENABLE_RUNTIMES="openmp;compiler-rt" \
  -DLLVM_TARGETS_TO_BUILD="X86;AMDGPU;NVPTX" \
  -DOPENMP_ENABLE_LIBOMPTARGET=ON \
  -DMLIR_INCLUDE_INTEGRATION_TESTS=ON \
  -DCLANG_ENABLE_STATIC_ANALYZER=ON \
  -DLLVM_OPTIMIZED_TABLEGEN=ON \
  -DLLVM_USE_LINKER=lld \
  -DLLVM_CCACHE_BUILD=ON \
  -DLLVM_PARALLEL_COMPILE_JOBS=8 \
  -DLLVM_PARALLEL_LINK_JOBS=2
```

## Verification Steps

After installation, verify functionality:

1. **Compiler Version**: `clang++ --version`
2. **OpenMP Support**: Check for OpenMP libraries in lib directory
3. **SIMD Intrinsics**: Test compilation with `-mavx512f` flags
4. **Tools Available**: Verify `clang-tidy` and `clang-format` binaries
5. **C++23 Modules**: Test module compilation with `--precompile`

## Integration with FastestJSONInTheWest

This configuration provides:
- Full SIMD intrinsics waterfall (AMX-TMUL → AVX-512-VNNI → AVX-512 → AVX2 → SSE4.2 → SSE2)
- Thread-safe OpenMP operations for parallel JSON parsing
- C++23 modules support for modern module interface
- clang-tidy integration for code quality assurance
- clang-format for consistent code style

## Documentation References

- **LLVM CMake Variables**: https://llvm.org/docs/CMake.html#llvm-related-variables
- **OpenMP Runtime Build**: https://openmp.llvm.org/docs/SupportAndFAQ.html#build-offload-capable-compiler
- **MLIR Configuration**: https://mlir.llvm.org/getting_started/
- **Clang Tools Documentation**: https://clang.llvm.org/docs/ClangTools.html