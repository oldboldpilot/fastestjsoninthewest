// FastestJSONInTheWest - CUDA Implementation
// Copyright (c) 2025 - NVIDIA CUDA GPU acceleration
// ============================================================================

#ifdef __CUDACC__
#include "json_gpu.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cstdio>
#include <chrono>

namespace fastjson {
namespace gpu {
namespace cuda {

// ============================================================================
// CUDA Error Handling
// ============================================================================

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA error in %s:%d: %s\n", __FILE__, __LINE__, \
                    cudaGetErrorString(err)); \
            return false; \
        } \
    } while(0)

#define CUDA_CHECK_RETURN(call, ret) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA error in %s:%d: %s\n", __FILE__, __LINE__, \
                    cudaGetErrorString(err)); \
            return ret; \
        } \
    } while(0)

// ============================================================================
// CUDA Kernels
// ============================================================================

// Whitespace detection kernel - finds all whitespace positions
__global__ void find_whitespace_kernel(const char* input, size_t size, 
                                      uint32_t* positions, uint32_t* count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;
    
    for (uint32_t i = idx; i < size; i += stride) {
        char c = input[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            uint32_t pos = atomicAdd(count, 1);
            if (pos < size) {  // Bounds check
                positions[pos] = i;
            }
        }
    }
}

// Structural character detection kernel - finds {}[]:,
__global__ void find_structural_kernel(const char* input, size_t size,
                                      uint32_t* positions, uint8_t* types,
                                      uint32_t* count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;
    
    for (uint32_t i = idx; i < size; i += stride) {
        char c = input[i];
        uint8_t type = 0;
        
        switch (c) {
            case '{': type = 1; break;
            case '}': type = 2; break;
            case '[': type = 3; break;
            case ']': type = 4; break;
            case ':': type = 5; break;
            case ',': type = 6; break;
            default: continue;
        }
        
        uint32_t pos = atomicAdd(count, 1);
        if (pos < size) {
            positions[pos] = i;
            types[pos] = type;
        }
    }
}

// String detection kernel - finds string boundaries
__global__ void find_strings_kernel(const char* input, size_t size,
                                   uint32_t* start_positions,
                                   uint32_t* end_positions,
                                   uint32_t* count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;
    
    for (uint32_t i = idx; i < size - 1; i += stride) {
        if (input[i] == '"') {
            // Check if it's escaped
            bool escaped = false;
            if (i > 0 && input[i-1] == '\\') {
                // Count consecutive backslashes
                int backslash_count = 0;
                for (int j = i - 1; j >= 0 && input[j] == '\\'; j--) {
                    backslash_count++;
                }
                escaped = (backslash_count % 2) == 1;
            }
            
            if (!escaped) {
                uint32_t pos = atomicAdd(count, 1);
                if (pos < size) {
                    start_positions[pos] = i;
                }
            }
        }
    }
}

// Number detection kernel - finds number boundaries
__global__ void find_numbers_kernel(const char* input, size_t size,
                                   uint32_t* positions, uint32_t* lengths,
                                   uint32_t* count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;
    
    for (uint32_t i = idx; i < size; i += stride) {
        char c = input[i];
        bool is_number_start = (c == '-') || (c >= '0' && c <= '9');
        
        if (!is_number_start) continue;
        
        // Check context - not inside a string
        // (Simple heuristic: previous char is whitespace or structural)
        if (i > 0) {
            char prev = input[i-1];
            if (prev != ' ' && prev != '\t' && prev != '\n' && prev != '\r' &&
                prev != '[' && prev != '{' && prev != ',' && prev != ':') {
                continue;
            }
        }
        
        // Find end of number
        uint32_t start = i;
        uint32_t end = i + 1;
        bool in_exponent = false;
        
        while (end < size) {
            char nc = input[end];
            
            if ((nc >= '0' && nc <= '9') || nc == '.' ||
                (in_exponent && (nc == '+' || nc == '-'))) {
                end++;
            } else if ((nc == 'e' || nc == 'E') && !in_exponent) {
                in_exponent = true;
                end++;
            } else {
                break;
            }
        }
        
        uint32_t pos = atomicAdd(count, 1);
        if (pos < size) {
            positions[pos] = start;
            lengths[pos] = end - start;
        }
        
        i = end - 1;  // Skip to end of number
    }
}

// Parallel reduction kernel for counting
__global__ void count_tokens_kernel(const uint32_t* flags, size_t size, uint32_t* result) {
    __shared__ uint32_t shared[256];
    
    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;
    
    uint32_t sum = 0;
    for (uint32_t i = idx; i < size; i += stride) {
        sum += flags[i];
    }
    
    shared[tid] = sum;
    __syncthreads();
    
    // Reduction in shared memory
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            shared[tid] += shared[tid + s];
        }
        __syncthreads();
    }
    
    if (tid == 0) {
        atomicAdd(result, shared[0]);
    }
}

// ============================================================================
// CUDA API Implementation
// ============================================================================

auto detect_cuda() -> bool {
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    return (err == cudaSuccess && device_count > 0);
}

auto get_cuda_info() -> gpu_info {
    gpu_info info;
    info.backend = gpu_backend::cuda;
    
    int device = 0;
    cudaError_t err = cudaGetDevice(&device);
    if (err != cudaSuccess) {
        return info;
    }
    
    cudaDeviceProp prop;
    err = cudaGetDeviceProperties(&prop, device);
    if (err != cudaSuccess) {
        return info;
    }
    
    info.device_name = prop.name;
    info.total_memory = prop.totalGlobalMem;
    info.compute_units = prop.multiProcessorCount;
    info.max_threads_per_block = prop.maxThreadsPerBlock;
    info.supports_concurrent_kernels = prop.concurrentKernels > 0;
    
    size_t free_mem, total_mem;
    err = cudaMemGetInfo(&free_mem, &total_mem);
    if (err == cudaSuccess) {
        info.available_memory = free_mem;
    }
    
    return info;
}

auto cuda_find_whitespace(const char* input, size_t size, uint32_t* positions, size_t* count) -> bool {
    char* d_input = nullptr;
    uint32_t* d_positions = nullptr;
    uint32_t* d_count = nullptr;
    
    CUDA_CHECK(cudaMalloc(&d_input, size));
    CUDA_CHECK(cudaMalloc(&d_positions, size * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&d_count, sizeof(uint32_t)));
    
    CUDA_CHECK(cudaMemcpy(d_input, input, size, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemset(d_count, 0, sizeof(uint32_t)));
    
    int block_size = 256;
    int grid_size = (size + block_size - 1) / block_size;
    grid_size = std::min(grid_size, 65535);
    
    find_whitespace_kernel<<<grid_size, block_size>>>(d_input, size, d_positions, d_count);
    
    CUDA_CHECK(cudaDeviceSynchronize());
    
    uint32_t h_count;
    CUDA_CHECK(cudaMemcpy(&h_count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost));
    *count = h_count;
    
    if (h_count > 0) {
        CUDA_CHECK(cudaMemcpy(positions, d_positions, h_count * sizeof(uint32_t), cudaMemcpyDeviceToHost));
    }
    
    cudaFree(d_input);
    cudaFree(d_positions);
    cudaFree(d_count);
    
    return true;
}

auto cuda_find_structural(const char* input, size_t size, uint32_t* positions, 
                         uint8_t* types, size_t* count) -> bool {
    char* d_input = nullptr;
    uint32_t* d_positions = nullptr;
    uint8_t* d_types = nullptr;
    uint32_t* d_count = nullptr;
    
    CUDA_CHECK(cudaMalloc(&d_input, size));
    CUDA_CHECK(cudaMalloc(&d_positions, size * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&d_types, size * sizeof(uint8_t)));
    CUDA_CHECK(cudaMalloc(&d_count, sizeof(uint32_t)));
    
    CUDA_CHECK(cudaMemcpy(d_input, input, size, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemset(d_count, 0, sizeof(uint32_t)));
    
    int block_size = 256;
    int grid_size = (size + block_size - 1) / block_size;
    grid_size = std::min(grid_size, 65535);
    
    find_structural_kernel<<<grid_size, block_size>>>(d_input, size, d_positions, d_types, d_count);
    
    CUDA_CHECK(cudaDeviceSynchronize());
    
    uint32_t h_count;
    CUDA_CHECK(cudaMemcpy(&h_count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost));
    *count = h_count;
    
    if (h_count > 0) {
        CUDA_CHECK(cudaMemcpy(positions, d_positions, h_count * sizeof(uint32_t), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(types, d_types, h_count * sizeof(uint8_t), cudaMemcpyDeviceToHost));
    }
    
    cudaFree(d_input);
    cudaFree(d_positions);
    cudaFree(d_types);
    cudaFree(d_count);
    
    return true;
}

auto parse_on_cuda(std::string_view input, const gpu_parse_config& config) -> gpu_parse_result {
    gpu_parse_result result;
    
    auto start_total = std::chrono::high_resolution_clock::now();
    
    size_t size = input.size();
    char* d_input = nullptr;
    uint32_t* d_positions = nullptr;
    uint8_t* d_types = nullptr;
    uint32_t* d_count = nullptr;
    
    // Allocate device memory
    auto start_alloc = std::chrono::high_resolution_clock::now();
    CUDA_CHECK_RETURN(cudaMalloc(&d_input, size), result);
    CUDA_CHECK_RETURN(cudaMalloc(&d_positions, size * sizeof(uint32_t)), result);
    CUDA_CHECK_RETURN(cudaMalloc(&d_types, size * sizeof(uint8_t)), result);
    CUDA_CHECK_RETURN(cudaMalloc(&d_count, sizeof(uint32_t)), result);
    auto end_alloc = std::chrono::high_resolution_clock::now();
    
    // Transfer to device
    auto start_h2d = std::chrono::high_resolution_clock::now();
    CUDA_CHECK_RETURN(cudaMemcpy(d_input, input.data(), size, cudaMemcpyHostToDevice), result);
    CUDA_CHECK_RETURN(cudaMemset(d_count, 0, sizeof(uint32_t)), result);
    auto end_h2d = std::chrono::high_resolution_clock::now();
    
    // Launch kernel
    auto start_kernel = std::chrono::high_resolution_clock::now();
    int block_size = config.block_size;
    int grid_size = config.grid_size > 0 ? config.grid_size : (size + block_size - 1) / block_size;
    grid_size = std::min(grid_size, 65535);
    
    find_structural_kernel<<<grid_size, block_size>>>(d_input, size, d_positions, d_types, d_count);
    CUDA_CHECK_RETURN(cudaDeviceSynchronize(), result);
    auto end_kernel = std::chrono::high_resolution_clock::now();
    
    // Transfer results back
    auto start_d2h = std::chrono::high_resolution_clock::now();
    uint32_t h_count;
    CUDA_CHECK_RETURN(cudaMemcpy(&h_count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost), result);
    
    result.token_positions.resize(h_count);
    result.token_types.resize(h_count);
    
    if (h_count > 0) {
        CUDA_CHECK_RETURN(cudaMemcpy(result.token_positions.data(), d_positions, 
                                     h_count * sizeof(uint32_t), cudaMemcpyDeviceToHost), result);
        CUDA_CHECK_RETURN(cudaMemcpy(result.token_types.data(), d_types,
                                     h_count * sizeof(uint8_t), cudaMemcpyDeviceToHost), result);
    }
    auto end_d2h = std::chrono::high_resolution_clock::now();
    
    // Cleanup
    cudaFree(d_input);
    cudaFree(d_positions);
    cudaFree(d_types);
    cudaFree(d_count);
    
    auto end_total = std::chrono::high_resolution_clock::now();
    
    // Calculate timings
    result.transfer_to_gpu_ms = std::chrono::duration<double, std::milli>(end_h2d - start_h2d).count();
    result.kernel_execution_ms = std::chrono::duration<double, std::milli>(end_kernel - start_kernel).count();
    result.transfer_from_gpu_ms = std::chrono::duration<double, std::milli>(end_d2h - start_d2h).count();
    result.total_ms = std::chrono::duration<double, std::milli>(end_total - start_total).count();
    
    result.success = true;
    return result;
}

} // namespace cuda
} // namespace gpu
} // namespace fastjson

#endif // __CUDACC__
