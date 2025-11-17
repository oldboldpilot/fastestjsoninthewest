# Performance Tests

This directory contains comprehensive performance benchmarks and profiling tests.

## Purpose
- Validate parsing speed targets
- SIMD optimization verification
- Large file processing (2GB+ requirement)
- Memory usage monitoring
- Throughput measurements

## Benchmark Categories

### Core Benchmarks
- `large_file_benchmark.cpp` - 2GB+ JSON file processing
- `instruction_set_benchmark.cpp` - Comprehensive SIMD testing
- `simd_optimization_benchmark.cpp` - Individual SIMD operation tests
- `openmp_benchmark.cpp` - Parallel processing validation

### SIMD Instruction Sets Tested
- **SSE**: SSE, SSE2, SSE3, SSE4.1, SSE4.2
- **AVX**: AVX, AVX2, AVX-512F, AVX-512BW
- **Advanced**: FMA, VNNI, AMX (Tile Matrix)
- **ARM**: NEON support
- **OpenMP**: Multi-threaded variants

### Performance Targets
- Small JSON (<1KB): <0.1ms parse time
- Medium JSON (1MB): <10ms parse time
- Large JSON (100MB): <1s parse time
- Massive JSON (2GB+): <30s parse time
- Throughput: >1GB/s on modern hardware

## Running Performance Tests
```bash
# All performance tests
cmake --build . --target performance_tests
./performance_tests

# Individual benchmarks
./instruction_set_benchmark 100  # 100MB test
./large_file_benchmark 2048      # 2GB test
./simd_optimization_benchmark
```

## Benchmark Results
Results are saved to JSON files for analysis:
- `simd_benchmark_results.json` - SIMD performance data
- `large_file_results.json` - Large file processing data
- `instruction_set_results.json` - Per-instruction performance

## Hardware Requirements
- Modern x86_64 processor with AVX2+ support
- Minimum 16GB RAM for 2GB+ file tests
- OpenMP-capable compiler for parallel tests