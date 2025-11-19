// FastestJSONInTheWest - GPU Acceleration Interface
// Copyright (c) 2025 - CUDA, ROCm/HIP, and Intel oneAPI/SYCL support
// ============================================================================

#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace fastjson {
namespace gpu {

// ============================================================================
// GPU Backend Detection and Selection
// ============================================================================

enum class gpu_backend {
    none,
    cuda,  // NVIDIA CUDA
    rocm,  // AMD ROCm/HIP
    sycl,  // Intel oneAPI/SYCL
    auto_detect
};

struct gpu_info {
    gpu_backend backend = gpu_backend::none;
    std::string device_name;
    size_t total_memory = 0;
    size_t available_memory = 0;
    int compute_units = 0;
    int max_threads_per_block = 0;
    bool supports_concurrent_kernels = false;
};

// Detect available GPU backend
auto detect_gpu_backend() -> gpu_backend;

// Get GPU device information
auto get_gpu_info(gpu_backend backend = gpu_backend::auto_detect) -> gpu_info;

// Check if GPU is available and suitable for JSON parsing
auto is_gpu_available() -> bool;

// ============================================================================
// GPU Memory Management
// ============================================================================

class gpu_buffer {
public:
    gpu_buffer(size_t size, gpu_backend backend);
    ~gpu_buffer();

    // No copy, move only
    gpu_buffer(const gpu_buffer&) = delete;
    gpu_buffer& operator=(const gpu_buffer&) = delete;
    gpu_buffer(gpu_buffer&&) noexcept;
    gpu_buffer& operator=(gpu_buffer&&) noexcept;

    // Copy data to/from GPU
    auto copy_to_device(const void* host_ptr, size_t size, size_t offset = 0) -> bool;
    auto copy_from_device(void* host_ptr, size_t size, size_t offset = 0) -> bool;

    // Async transfers
    auto copy_to_device_async(const void* host_ptr, size_t size, size_t offset = 0) -> bool;
    auto copy_from_device_async(void* host_ptr, size_t size, size_t offset = 0) -> bool;

    // Accessors
    auto device_ptr() -> void* { return device_ptr_; }

    auto size() const -> size_t { return size_; }

    auto backend() const -> gpu_backend { return backend_; }

private:
    void* device_ptr_ = nullptr;
    size_t size_ = 0;
    gpu_backend backend_ = gpu_backend::none;
    void* stream_ = nullptr;  // CUDA stream / HIP stream / SYCL queue
};

// ============================================================================
// GPU JSON Parsing Interface
// ============================================================================

struct gpu_parse_config {
    gpu_backend backend = gpu_backend::auto_detect;
    size_t min_size_for_gpu = 10000;  // Minimum input size to use GPU
    size_t block_size = 256;          // GPU block/workgroup size
    int grid_size = 0;                // GPU grid size (0 = auto)
    bool async_execution = true;      // Use asynchronous GPU execution
    bool pin_host_memory = true;      // Pin host memory for faster transfers
};

// Parse JSON on GPU (returns parsed structure indices and values)
struct gpu_parse_result {
    bool success = false;
    std::string error_message;

    // Parsed structure (indices into input for each token)
    std::vector<uint32_t> token_positions;
    std::vector<uint8_t> token_types;
    std::vector<uint32_t> token_lengths;

    // Performance metrics
    double transfer_to_gpu_ms = 0.0;
    double kernel_execution_ms = 0.0;
    double transfer_from_gpu_ms = 0.0;
    double total_ms = 0.0;
};

// GPU-accelerated JSON parsing
auto parse_on_gpu(std::string_view input, const gpu_parse_config& config = {}) -> gpu_parse_result;

// ============================================================================
// GPU Kernel Operations (Low-level)
// ============================================================================

// Whitespace detection kernel
auto gpu_find_whitespace(const char* input, size_t size, uint32_t* positions, size_t* count,
                         gpu_backend backend) -> bool;

// String scanning kernel
auto gpu_find_strings(const char* input, size_t size, uint32_t* positions, size_t* count,
                      gpu_backend backend) -> bool;

// Number scanning kernel
auto gpu_find_numbers(const char* input, size_t size, uint32_t* positions, size_t* count,
                      gpu_backend backend) -> bool;

// Structural character scanning ({}[]:,)
auto gpu_find_structural_chars(const char* input, size_t size, uint32_t* positions,
                               uint8_t* char_types, size_t* count, gpu_backend backend) -> bool;

// ============================================================================
// GPU Performance Utilities
// ============================================================================

struct gpu_benchmark_result {
    gpu_backend backend;
    double throughput_mb_s = 0.0;
    double latency_ms = 0.0;
    size_t input_size = 0;
    std::string details;
};

// Benchmark GPU vs CPU performance
auto benchmark_gpu_vs_cpu(size_t test_size) -> std::vector<gpu_benchmark_result>;

// Get optimal configuration for given input size
auto get_optimal_gpu_config(size_t input_size, gpu_backend backend) -> gpu_parse_config;

}  // namespace gpu
}  // namespace fastjson
