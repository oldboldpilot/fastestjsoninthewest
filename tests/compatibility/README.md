# Compatibility Tests

This directory contains cross-platform and compiler validation tests.

## Purpose
- Cross-platform validation (Linux/Windows/macOS)
- Compiler compatibility (GCC/Clang/MSVC)
- C++23 standard compliance
- Legacy support verification

## Test Categories
- `compiler_test.cpp` - Multi-compiler validation
- `platform_test.cpp` - Cross-platform testing
- `cpp23_test.cpp` - C++23 feature validation
- `simd_compatibility_test.cpp` - SIMD instruction set compatibility
- `threading_test.cpp` - Thread safety validation

## Running Compatibility Tests
```bash
cmake --build . --target compatibility_tests
./compatibility_tests
```