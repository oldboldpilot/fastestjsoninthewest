// FastestJSONInTheWest - CUDA Implementation
// Copyright (c) 2025 - NVIDIA CUDA GPU acceleration
// ============================================================================

#ifdef __CUDACC__
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cublas_v2.h>
#include <cuda.h>
#include <cstdint>
#include <cstddef>

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

// CUDA Graph capture helper
static cudaGraph_t g_cuda_graph = nullptr;
static cudaGraphExec_t g_cuda_graph_exec = nullptr;
static bool g_cuda_graph_captured = false;
static size_t g_cuda_graph_last_size = 0;
static char* g_cuda_graph_d_input = nullptr;
static uint32_t* g_cuda_graph_d_positions = nullptr;
static uint8_t* g_cuda_graph_d_types = nullptr;
static uint32_t* g_cuda_graph_d_count = nullptr;

// cuTile GEMM Kernel
__global__ void cutile_gemm_kernel(const float* A, const float* B, float* C, int M, int N, int K) {
    __shared__ float tileA[16][16];
    __shared__ float tileB[16][16];
    
    int tx = threadIdx.x;
    int ty = threadIdx.y;
    int row = blockIdx.y * 16 + ty;
    int col = blockIdx.x * 16 + tx;
    
    float sum = 0.0f;
    for (int p = 0; p < (K + 15) / 16; ++p) {
        if (row < M && p * 16 + tx < K) {
            tileA[ty][tx] = A[row * K + p * 16 + tx];
        } else {
            tileA[ty][tx] = 0.0f;
        }
        if (col < N && p * 16 + ty < K) {
            tileB[ty][tx] = B[(p * 16 + ty) * N + col];
        } else {
            tileB[ty][tx] = 0.0f;
        }
        __syncthreads();
        
        #pragma unroll
        for (int i = 0; i < 16; ++i) {
            sum += tileA[ty][i] * tileB[i][tx];
        }
        __syncthreads();
    }
    if (row < M && col < N) {
        C[row * N + col] = sum;
    }
}

// ============================================================================
// CUDA API Implementation
// ============================================================================

extern "C" {

auto detect_cuda_c() -> bool {
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    return (err == cudaSuccess && device_count > 0);
}

auto get_cuda_info_c(
    char* device_name,
    size_t* total_memory,
    size_t* available_memory,
    int* compute_units,
    int* max_threads_per_block,
    bool* supports_concurrent_kernels
) -> bool {
    int device = 0;
    cudaError_t err = cudaGetDevice(&device);
    if (err != cudaSuccess) {
        return false;
    }
    
    cudaDeviceProp prop;
    err = cudaGetDeviceProperties(&prop, device);
    if (err != cudaSuccess) {
        return false;
    }
    
    // Copy the device name safely
    int i = 0;
    for (; i < 255 && prop.name[i] != '\0'; ++i) {
        device_name[i] = prop.name[i];
    }
    device_name[i] = '\0';
    
    *total_memory = prop.totalGlobalMem;
    *compute_units = prop.multiProcessorCount;
    *max_threads_per_block = prop.maxThreadsPerBlock;
    *supports_concurrent_kernels = prop.concurrentKernels > 0;
    
    size_t free_mem, total_mem;
    err = cudaMemGetInfo(&free_mem, &total_mem);
    if (err == cudaSuccess) {
        *available_memory = free_mem;
    } else {
        *available_memory = 0;
    }
    
    return true;
}

auto cuda_find_whitespace_c(const char* input, size_t size, uint32_t* positions, size_t* count) -> bool {
    char* d_input = nullptr;
    uint32_t* d_positions = nullptr;
    uint32_t* d_count = nullptr;
    
    cudaError_t err1 = cudaMalloc(&d_input, size);
    cudaError_t err2 = cudaMalloc(&d_positions, size * sizeof(uint32_t));
    cudaError_t err3 = cudaMalloc(&d_count, sizeof(uint32_t));
    if (err1 != cudaSuccess || err2 != cudaSuccess || err3 != cudaSuccess) {
        if (d_input) cudaFree(d_input);
        if (d_positions) cudaFree(d_positions);
        if (d_count) cudaFree(d_count);
        return false;
    }
    
    cudaMemcpy(d_input, input, size, cudaMemcpyHostToDevice);
    cudaMemset(d_count, 0, sizeof(uint32_t));
    
    int block_size = 256;
    int grid_size = (size + block_size - 1) / block_size;
    grid_size = grid_size > 65535 ? 65535 : grid_size;
    
    find_whitespace_kernel<<<grid_size, block_size>>>(d_input, size, d_positions, d_count);
    
    cudaDeviceSynchronize();
    
    uint32_t h_count = 0;
    cudaMemcpy(&h_count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost);
    *count = h_count;
    
    if (h_count > 0) {
        cudaMemcpy(positions, d_positions, h_count * sizeof(uint32_t), cudaMemcpyDeviceToHost);
    }
    
    cudaFree(d_input);
    cudaFree(d_positions);
    cudaFree(d_count);
    
    return true;
}

auto cuda_find_structural_c(const char* input, size_t size, uint32_t* positions, 
                           uint8_t* types, size_t* count) -> bool {
    char* d_input = nullptr;
    uint32_t* d_positions = nullptr;
    uint8_t* d_types = nullptr;
    uint32_t* d_count = nullptr;
    
    cudaError_t err1 = cudaMalloc(&d_input, size);
    cudaError_t err2 = cudaMalloc(&d_positions, size * sizeof(uint32_t));
    cudaError_t err3 = cudaMalloc(&d_types, size * sizeof(uint8_t));
    cudaError_t err4 = cudaMalloc(&d_count, sizeof(uint32_t));
    if (err1 != cudaSuccess || err2 != cudaSuccess || err3 != cudaSuccess || err4 != cudaSuccess) {
        if (d_input) cudaFree(d_input);
        if (d_positions) cudaFree(d_positions);
        if (d_types) cudaFree(d_types);
        if (d_count) cudaFree(d_count);
        return false;
    }
    
    cudaMemcpy(d_input, input, size, cudaMemcpyHostToDevice);
    cudaMemset(d_count, 0, sizeof(uint32_t));
    
    int block_size = 256;
    int grid_size = (size + block_size - 1) / block_size;
    grid_size = grid_size > 65535 ? 65535 : grid_size;
    
    find_structural_kernel<<<grid_size, block_size>>>(d_input, size, d_positions, d_types, d_count);
    
    cudaDeviceSynchronize();
    
    uint32_t h_count = 0;
    cudaMemcpy(&h_count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost);
    *count = h_count;
    
    if (h_count > 0) {
        cudaMemcpy(positions, d_positions, h_count * sizeof(uint32_t), cudaMemcpyDeviceToHost);
        cudaMemcpy(types, d_types, h_count * sizeof(uint8_t), cudaMemcpyDeviceToHost);
    }
    
    cudaFree(d_input);
    cudaFree(d_positions);
    cudaFree(d_types);
    cudaFree(d_count);
    
    return true;
}

auto capture_and_execute_graph_c(const char* input, size_t size, uint32_t* positions, uint8_t* types, size_t* count, int grid_size, int block_size) -> bool {
    if (g_cuda_graph_captured && g_cuda_graph_last_size == size) {
        cudaMemcpy(g_cuda_graph_d_input, input, size, cudaMemcpyHostToDevice);
        cudaMemset(g_cuda_graph_d_count, 0, sizeof(uint32_t));
        
        cudaGraphLaunch(g_cuda_graph_exec, 0);
        cudaDeviceSynchronize();
        
        uint32_t h_count = 0;
        cudaMemcpy(&h_count, g_cuda_graph_d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost);
        *count = h_count;
        if (h_count > 0) {
            cudaMemcpy(positions, g_cuda_graph_d_positions, h_count * sizeof(uint32_t), cudaMemcpyDeviceToHost);
            cudaMemcpy(types, g_cuda_graph_d_types, h_count * sizeof(uint8_t), cudaMemcpyDeviceToHost);
        }
        return true;
    }
    
    if (g_cuda_graph_captured) {
        cudaGraphExecDestroy(g_cuda_graph_exec);
        cudaGraphDestroy(g_cuda_graph);
        cudaFree(g_cuda_graph_d_input);
        cudaFree(g_cuda_graph_d_positions);
        cudaFree(g_cuda_graph_d_types);
        cudaFree(g_cuda_graph_d_count);
        g_cuda_graph_captured = false;
    }
    
    cudaMalloc(&g_cuda_graph_d_input, size);
    cudaMalloc(&g_cuda_graph_d_positions, size * sizeof(uint32_t));
    cudaMalloc(&g_cuda_graph_d_types, size * sizeof(uint8_t));
    cudaMalloc(&g_cuda_graph_d_count, sizeof(uint32_t));
    g_cuda_graph_last_size = size;
    
    cudaStream_t stream;
    cudaStreamCreate(&stream);
    
    cudaMemcpyAsync(g_cuda_graph_d_input, input, size, cudaMemcpyHostToDevice, stream);
    cudaMemsetAsync(g_cuda_graph_d_count, 0, sizeof(uint32_t), stream);
    
    cudaStreamBeginCapture(stream, cudaStreamCaptureModeGlobal);
    
    find_structural_kernel<<<grid_size, block_size, 0, stream>>>(g_cuda_graph_d_input, size, g_cuda_graph_d_positions, g_cuda_graph_d_types, g_cuda_graph_d_count);
    
    cudaStreamEndCapture(stream, &g_cuda_graph);
    cudaGraphInstantiate(&g_cuda_graph_exec, g_cuda_graph, nullptr, nullptr, 0);
    
    cudaGraphLaunch(g_cuda_graph_exec, stream);
    cudaStreamSynchronize(stream);
    
    uint32_t h_count = 0;
    cudaMemcpy(&h_count, g_cuda_graph_d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost);
    *count = h_count;
    if (h_count > 0) {
        cudaMemcpy(positions, g_cuda_graph_d_positions, h_count * sizeof(uint32_t), cudaMemcpyDeviceToHost);
        cudaMemcpy(types, g_cuda_graph_d_types, h_count * sizeof(uint8_t), cudaMemcpyDeviceToHost);
    }
    
    cudaStreamDestroy(stream);
    g_cuda_graph_captured = true;
    return true;
}

auto parse_on_cuda_c(
    const char* input,
    size_t size,
    uint32_t* positions,
    uint8_t* types,
    size_t* count,
    int grid_size,
    int block_size,
    bool use_cuda_graph,
    double* transfer_to_gpu_ms,
    double* kernel_execution_ms,
    double* transfer_from_gpu_ms
) -> bool {
    if (grid_size <= 0) {
        grid_size = (size + block_size - 1) / block_size;
    }
    grid_size = grid_size > 65535 ? 65535 : grid_size;

    if (use_cuda_graph) {
        cudaEvent_t start_event, stop_event;
        cudaEventCreate(&start_event);
        cudaEventCreate(&stop_event);
        
        cudaEventRecord(start_event, 0);
        bool ok = capture_and_execute_graph_c(input, size, positions, types, count, grid_size, block_size);
        cudaEventRecord(stop_event, 0);
        cudaEventSynchronize(stop_event);
        
        float ms = 0;
        cudaEventElapsedTime(&ms, start_event, stop_event);
        *transfer_to_gpu_ms = 0.0;
        *kernel_execution_ms = ms;
        *transfer_from_gpu_ms = 0.0;
        
        cudaEventDestroy(start_event);
        cudaEventDestroy(stop_event);
        return ok;
    }
    
    char* d_input = nullptr;
    uint32_t* d_positions = nullptr;
    uint8_t* d_types = nullptr;
    uint32_t* d_count = nullptr;
    
    cudaError_t err1 = cudaMalloc(&d_input, size);
    cudaError_t err2 = cudaMalloc(&d_positions, size * sizeof(uint32_t));
    cudaError_t err3 = cudaMalloc(&d_types, size * sizeof(uint8_t));
    cudaError_t err4 = cudaMalloc(&d_count, sizeof(uint32_t));
    if (err1 != cudaSuccess || err2 != cudaSuccess || err3 != cudaSuccess || err4 != cudaSuccess) {
        if (d_input) cudaFree(d_input);
        if (d_positions) cudaFree(d_positions);
        if (d_types) cudaFree(d_types);
        if (d_count) cudaFree(d_count);
        return false;
    }

    cudaEvent_t start_h2d, start_kernel, start_d2h, end_d2h;
    cudaEventCreate(&start_h2d);
    cudaEventCreate(&start_kernel);
    cudaEventCreate(&start_d2h);
    cudaEventCreate(&end_d2h);

    cudaEventRecord(start_h2d, 0);
    cudaMemcpy(d_input, input, size, cudaMemcpyHostToDevice);
    cudaMemset(d_count, 0, sizeof(uint32_t));
    
    cudaEventRecord(start_kernel, 0);
    find_structural_kernel<<<grid_size, block_size>>>(d_input, size, d_positions, d_types, d_count);
    
    cudaEventRecord(start_d2h, 0);
    uint32_t h_count = 0;
    cudaMemcpy(&h_count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost);
    *count = h_count;
    
    if (h_count > 0) {
        cudaMemcpy(positions, d_positions, h_count * sizeof(uint32_t), cudaMemcpyDeviceToHost);
        cudaMemcpy(types, d_types, h_count * sizeof(uint8_t), cudaMemcpyDeviceToHost);
    }
    cudaEventRecord(end_d2h, 0);
    cudaDeviceSynchronize();
    cudaEventRecord(end_d2h, 0); // record end
    cudaEventSynchronize(end_d2h);
    
    cudaFree(d_input);
    cudaFree(d_positions);
    cudaFree(d_types);
    cudaFree(d_count);
    
    float h2d_ms = 0;
    cudaEventElapsedTime(&h2d_ms, start_h2d, start_kernel);
    *transfer_to_gpu_ms = h2d_ms;

    float kernel_ms = 0;
    cudaEventElapsedTime(&kernel_ms, start_kernel, start_d2h);
    *kernel_execution_ms = kernel_ms;

    float d2h_ms = 0;
    cudaEventElapsedTime(&d2h_ms, start_d2h, end_d2h);
    *transfer_from_gpu_ms = d2h_ms;

    cudaEventDestroy(start_h2d);
    cudaEventDestroy(start_kernel);
    cudaEventDestroy(start_d2h);
    cudaEventDestroy(end_d2h);
    
    return true;
}

auto gpu_matrix_multiply_c(
    const float* A,
    const float* B,
    float* C,
    int M,
    int N,
    int K,
    int backend_val
) -> bool {
    float* d_A = nullptr;
    float* d_B = nullptr;
    float* d_C = nullptr;
    
    cudaMalloc(&d_A, M * K * sizeof(float));
    cudaMalloc(&d_B, K * N * sizeof(float));
    cudaMalloc(&d_C, M * N * sizeof(float));
    
    cudaMemcpy(d_A, A, M * K * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, B, K * N * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemset(d_C, 0, M * N * sizeof(float));
    
    bool success = false;
    if (backend_val == 23) { // cublas
        cublasHandle_t handle;
        if (cublasCreate(&handle) == CUBLAS_STATUS_SUCCESS) {
            float alpha = 1.0f;
            float beta = 0.0f;
            if (cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, N, M, K, &alpha, d_B, N, d_A, K, &beta, d_C, N) == CUBLAS_STATUS_SUCCESS) {
                success = true;
            }
            cublasDestroy(handle);
        }
    } else { // cutile
        dim3 block(16, 16);
        dim3 grid((N + 15) / 16, (M + 15) / 16);
        cutile_gemm_kernel<<<grid, block>>>(d_A, d_B, d_C, M, N, K);
        if (cudaDeviceSynchronize() == cudaSuccess) {
            success = true;
        }
    }
    
    if (success) {
        cudaMemcpy(C, d_C, M * N * sizeof(float), cudaMemcpyDeviceToHost);
    }
    
    cudaFree(d_A);
    cudaFree(d_B);
    cudaFree(d_C);
    return success;
}

auto gpu_launch_triton_ptx_c(
    const char* ptx_code,
    const char* kernel_name,
    void** args,
    int grid_size,
    int block_size
) -> bool {
    CUmodule module;
    CUfunction function;
    
    cuInit(0);
    CUdevice device;
    cuDeviceGet(&device, 0);
    CUcontext context;
    cuCtxGetCurrent(&context);
    if (!context) {
        cuCtxCreate(&context, nullptr, 0, device);
    }
    
    if (cuModuleLoadData(&module, ptx_code) != CUDA_SUCCESS) {
        return false;
    }
    
    if (cuModuleGetFunction(&function, module, kernel_name) != CUDA_SUCCESS) {
        cuModuleUnload(module);
        return false;
    }
    
    if (cuLaunchKernel(function, grid_size, 1, 1, block_size, 1, 1, 0, nullptr, args, nullptr) != CUDA_SUCCESS) {
        cuModuleUnload(module);
        return false;
    }
    
    cudaDeviceSynchronize();
    cuModuleUnload(module);
    return true;
}

} // extern "C"

} // namespace cuda
} // namespace gpu
} // namespace fastjson

#endif // __CUDACC__
