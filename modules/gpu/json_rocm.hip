// FastestJSONInTheWest - ROCm/HIP Implementation
// Copyright (c) 2025 - AMD ROCm/HIP GPU acceleration
// Author: Olumuyiwa Oluwasanmi
// ============================================================================

#ifdef __HIP__
#include "json_gpu.h"
#include <hip/hip_runtime.h>
#include <cstdio>
#include <chrono>

namespace fastjson {
namespace gpu {
namespace rocm {

// ============================================================================
// HIP Error Handling
// ============================================================================

#define HIP_CHECK(call) \
    do { \
        hipError_t err = call; \
        if (err != hipSuccess) { \
            fprintf(stderr, "HIP error in %s:%d: %s\n", __FILE__, __LINE__, \
                    hipGetErrorString(err)); \
            return false; \
        } \
    } while(0)

#define HIP_CHECK_RETURN(call, ret) \
    do { \
        hipError_t err = call; \
        if (err != hipSuccess) { \
            fprintf(stderr, "HIP error in %s:%d: %s\n", __FILE__, __LINE__, \
                    hipGetErrorString(err)); \
            return ret; \
        } \
    } while(0)

// ============================================================================
// HIP Kernels for JSON Parsing
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

// String detection kernel - finds string boundaries (quotes)
__global__ void find_strings_kernel(const char* input, size_t size,
                                   uint32_t* start_positions,
                                   uint32_t* count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;
    
    for (uint32_t i = idx; i < size - 1; i += stride) {
        if (input[i] == '"') {
            // Check if it's escaped by looking backwards
            bool escaped = false;
            if (i > 0 && input[i-1] == '\\') {
                // Count consecutive backslashes
                int backslash_count = 0;
                for (int j = i - 1; j >= 0 && input[j] == '\\'; --j) {
                    backslash_count++;
                }
                // Odd number of backslashes means this quote is escaped
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

// Number detection kernel - finds number start positions
__global__ void find_numbers_kernel(const char* input, size_t size,
                                   uint32_t* positions, uint32_t* count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;
    
    for (uint32_t i = idx; i < size; i += stride) {
        char c = input[i];
        char prev = (i > 0) ? input[i-1] : ' ';
        
        // Number starts with digit or minus sign (if followed by digit)
        bool is_number_start = false;
        
        if (c >= '0' && c <= '9') {
            // Digit - check if it's the start (prev must be structural or whitespace)
            if (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r' ||
                prev == '[' || prev == '{' || prev == ',' || prev == ':') {
                is_number_start = true;
            }
        } else if (c == '-') {
            // Minus sign - check if followed by digit
            if (i + 1 < size && input[i+1] >= '0' && input[i+1] <= '9') {
                if (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r' ||
                    prev == '[' || prev == '{' || prev == ',' || prev == ':') {
                    is_number_start = true;
                }
            }
        }
        
        if (is_number_start) {
            uint32_t pos = atomicAdd(count, 1);
            if (pos < size) {
                positions[pos] = i;
            }
        }
    }
}

// Combined structural scanning kernel (optimized)
// Detects whitespace, structural chars, strings, and numbers in one pass
__global__ void scan_tokens_kernel(const char* input, size_t size,
                                  uint32_t* token_positions,
                                  uint8_t* token_types,
                                  uint32_t* count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;
    
    // Token types:
    // 0 = whitespace, 1-6 = structural ({}[]:,), 7 = string, 8 = number
    
    for (uint32_t i = idx; i < size; i += stride) {
        char c = input[i];
        uint8_t type = 0xFF;  // Invalid
        
        // Check structural characters
        switch (c) {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                type = 0;  // Whitespace
                break;
            case '{': type = 1; break;
            case '}': type = 2; break;
            case '[': type = 3; break;
            case ']': type = 4; break;
            case ':': type = 5; break;
            case ',': type = 6; break;
            case '"':
                // TODO: Check if escaped
                type = 7;  // String
                break;
            default:
                // Check if number
                if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
                    // TODO: Better number detection
                    type = 8;
                }
                break;
        }
        
        if (type != 0xFF) {
            uint32_t pos = atomicAdd(count, 1);
            if (pos < size) {
                token_positions[pos] = i;
                token_types[pos] = type;
            }
        }
    }
}

// ============================================================================
// Host Functions
// ============================================================================

auto detect_rocm() -> bool {
    int device_count = 0;
    hipError_t err = hipGetDeviceCount(&device_count);
    return (err == hipSuccess && device_count > 0);
}

auto get_rocm_info() -> gpu_info {
    gpu_info info;
    info.backend = gpu_backend::rocm;
    
    int device = 0;
    hipError_t err = hipGetDevice(&device);
    if (err != hipSuccess) {
        return info;
    }
    
    hipDeviceProp_t props;
    err = hipGetDeviceProperties(&props, device);
    if (err != hipSuccess) {
        return info;
    }
    
    info.device_name = props.name;
    info.total_memory = props.totalGlobalMem;
    info.compute_units = props.multiProcessorCount;
    info.max_threads_per_block = props.maxThreadsPerBlock;
    info.supports_concurrent_kernels = props.concurrentKernels;
    
    size_t free_mem, total_mem;
    err = hipMemGetInfo(&free_mem, &total_mem);
    if (err == hipSuccess) {
        info.available_memory = free_mem;
    }
    
    return info;
}

auto parse_on_rocm(std::string_view input, const gpu_parse_config& config) -> gpu_parse_result {
    gpu_parse_result result;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Allocate device memory
    char* d_input = nullptr;
    uint32_t* d_positions = nullptr;
    uint8_t* d_types = nullptr;
    uint32_t* d_count = nullptr;
    
    size_t input_size = input.size();
    
    HIP_CHECK_RETURN(hipMalloc(&d_input, input_size), result);
    HIP_CHECK_RETURN(hipMalloc(&d_positions, input_size * sizeof(uint32_t)), result);
    HIP_CHECK_RETURN(hipMalloc(&d_types, input_size * sizeof(uint8_t)), result);
    HIP_CHECK_RETURN(hipMalloc(&d_count, sizeof(uint32_t)), result);
    
    // Initialize count to 0
    uint32_t zero = 0;
    HIP_CHECK_RETURN(hipMemcpy(d_count, &zero, sizeof(uint32_t), hipMemcpyHostToDevice), result);
    
    auto transfer_to_start = std::chrono::high_resolution_clock::now();
    
    // Copy input to device
    HIP_CHECK_RETURN(hipMemcpy(d_input, input.data(), input_size, hipMemcpyHostToDevice), result);
    
    auto transfer_to_end = std::chrono::high_resolution_clock::now();
    auto kernel_start = std::chrono::high_resolution_clock::now();
    
    // Launch kernel
    int block_size = config.block_size;
    int grid_size = config.grid_size > 0 ? config.grid_size : (input_size + block_size - 1) / block_size;
    
    hipLaunchKernelGGL(scan_tokens_kernel, dim3(grid_size), dim3(block_size), 0, 0,
                       d_input, input_size, d_positions, d_types, d_count);
    
    HIP_CHECK_RETURN(hipDeviceSynchronize(), result);
    
    auto kernel_end = std::chrono::high_resolution_clock::now();
    auto transfer_from_start = std::chrono::high_resolution_clock::now();
    
    // Get count
    uint32_t token_count = 0;
    HIP_CHECK_RETURN(hipMemcpy(&token_count, d_count, sizeof(uint32_t), hipMemcpyDeviceToHost), result);
    
    // Copy results back
    result.token_positions.resize(token_count);
    result.token_types.resize(token_count);
    
    HIP_CHECK_RETURN(hipMemcpy(result.token_positions.data(), d_positions, 
                               token_count * sizeof(uint32_t), hipMemcpyDeviceToHost), result);
    HIP_CHECK_RETURN(hipMemcpy(result.token_types.data(), d_types,
                               token_count * sizeof(uint8_t), hipMemcpyDeviceToHost), result);
    
    auto transfer_from_end = std::chrono::high_resolution_clock::now();
    
    // Cleanup
    hipFree(d_input);
    hipFree(d_positions);
    hipFree(d_types);
    hipFree(d_count);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Calculate timings
    result.transfer_to_gpu_ms = std::chrono::duration<double, std::milli>(transfer_to_end - transfer_to_start).count();
    result.kernel_execution_ms = std::chrono::duration<double, std::milli>(kernel_end - kernel_start).count();
    result.transfer_from_gpu_ms = std::chrono::duration<double, std::milli>(transfer_from_end - transfer_from_start).count();
    result.total_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    result.success = true;
    return result;
}

} // namespace rocm
} // namespace gpu
} // namespace fastjson

#endif // __HIP__
