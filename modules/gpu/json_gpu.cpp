// FastestJSONInTheWest - GPU Detection and Backend Selection
// Copyright (c) 2025 - Runtime GPU detection and configuration
// ============================================================================

#include "json_gpu.h"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifdef FASTJSON_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

// Forward declarations for backend-specific functions
#ifdef FASTJSON_ENABLE_CUDA
extern "C" {
auto detect_cuda_c() -> bool;
auto get_cuda_info_c(char* device_name, size_t* total_memory, size_t* available_memory, int* compute_units, int* max_threads_per_block, bool* supports_concurrent_kernels) -> bool;
auto cuda_find_whitespace_c(const char* input, size_t size, uint32_t* positions, size_t* count) -> bool;
auto cuda_find_structural_c(const char* input, size_t size, uint32_t* positions, uint8_t* types, size_t* count) -> bool;
auto parse_on_cuda_c(const char* input, size_t size, uint32_t* positions, uint8_t* types, size_t* count, int grid_size, int block_size, bool use_cuda_graph, double* transfer_to_gpu_ms, double* kernel_execution_ms, double* transfer_from_gpu_ms) -> bool;
auto gpu_matrix_multiply_c(const float* A, const float* B, float* C, int M, int N, int K, int backend_val) -> bool;
auto gpu_launch_triton_ptx_c(const char* ptx_code, const char* kernel_name, void** args, int grid_size, int block_size) -> bool;
}

namespace fastjson::gpu::cuda {

auto detect_cuda() -> bool {
    return detect_cuda_c();
}

auto get_cuda_info() -> gpu_info {
    gpu_info info;
    info.backend = gpu_backend::cuda;
    char name_buf[256] = {0};
    if (get_cuda_info_c(name_buf, &info.total_memory, &info.available_memory, &info.compute_units, &info.max_threads_per_block, &info.supports_concurrent_kernels)) {
        info.device_name = name_buf;
    }
    return info;
}

auto parse_on_cuda(std::string_view input, const gpu_parse_config& config) -> gpu_parse_result {
    gpu_parse_result result;
    size_t size = input.size();
    
    int block_size = config.block_size;
    int grid_size = config.grid_size;
    
    result.token_positions.resize(size);
    result.token_types.resize(size);
    size_t count = 0;
    
    bool ok = parse_on_cuda_c(
        input.data(), size,
        result.token_positions.data(), result.token_types.data(), &count,
        grid_size, block_size,
        config.use_cuda_graph || config.backend == gpu_backend::cuda_graph,
        &result.transfer_to_gpu_ms, &result.kernel_execution_ms, &result.transfer_from_gpu_ms
    );
    
    if (ok) {
        result.token_positions.resize(count);
        result.token_types.resize(count);
        result.success = true;
    } else {
        result.success = false;
        result.error_message = "CUDA parsing kernel execution failed";
    }
    return result;
}

auto gpu_matrix_multiply(const float* A, const float* B, float* C, int M, int N, int K, gpu_backend backend) -> bool {
    return gpu_matrix_multiply_c(A, B, C, M, N, K, static_cast<int>(backend));
}

auto gpu_launch_triton_ptx(const char* ptx_code, const char* kernel_name, void** args, int grid_size, int block_size) -> bool {
    return gpu_launch_triton_ptx_c(ptx_code, kernel_name, args, grid_size, block_size);
}

auto cuda_find_whitespace(const char* input, size_t size, uint32_t* positions, size_t* count) -> bool {
    return cuda_find_whitespace_c(input, size, positions, count);
}

auto cuda_find_structural(const char* input, size_t size, uint32_t* positions, uint8_t* types, size_t* count) -> bool {
    return cuda_find_structural_c(input, size, positions, types, count);
}

} // namespace fastjson::gpu::cuda
#endif

#ifdef __HIP__
namespace fastjson::gpu::rocm {
auto detect_rocm() -> bool;
auto get_rocm_info() -> gpu_info;
auto parse_on_rocm(std::string_view input, const gpu_parse_config& config) -> gpu_parse_result;
}  // namespace fastjson::gpu::rocm
#endif

#ifdef SYCL_LANGUAGE_VERSION
namespace fastjson::gpu::sycl {
auto detect_sycl() -> bool;
auto get_sycl_info() -> gpu_info;
auto parse_on_sycl(std::string_view input, const gpu_parse_config& config) -> gpu_parse_result;
}  // namespace fastjson::gpu::sycl
#endif

namespace fastjson {
namespace gpu {

// ============================================================================
// Runtime Library Detection
// ============================================================================

static auto check_library_exists(const char* lib_name) -> bool {
#ifdef _WIN32
    HMODULE handle = GetModuleHandleA(lib_name);
    if (handle) {
        return true;
    }
    handle = LoadLibraryA(lib_name);
    if (handle) {
        FreeLibrary(handle);
        return true;
    }
    return false;
#else
    void* handle = dlopen(lib_name, RTLD_NOW | RTLD_NOLOAD);
    if (handle) {
        dlclose(handle);
        return true;
    }

    // Try loading it
    handle = dlopen(lib_name, RTLD_NOW | RTLD_LAZY);
    if (handle) {
        dlclose(handle);
        return true;
    }

    return false;
#endif
}

static auto detect_cuda_runtime() -> bool {
    // Check for CUDA runtime library
#ifdef _WIN32
    const char* cuda_libs[] = {"cudart64_12.dll", "cudart64_110.dll", "cudart64_102.dll", "cudart.dll", nullptr};
#else
    const char* cuda_libs[] = {"libcudart.so", "libcudart.so.12", "libcudart.so.11", nullptr};
#endif

    for (int i = 0; cuda_libs[i]; ++i) {
        if (check_library_exists(cuda_libs[i])) {
            return true;
        }
    }

    return false;
}

static auto detect_rocm_runtime() -> bool {
    // Check for ROCm/HIP runtime library
    const char* rocm_libs[] = {"libamdhip64.so", "libhip_hcc.so", nullptr};

    for (int i = 0; rocm_libs[i]; ++i) {
        if (check_library_exists(rocm_libs[i])) {
            return true;
        }
    }

    return false;
}

static auto detect_sycl_runtime() -> bool {
    // Check for Intel oneAPI SYCL runtime
    const char* sycl_libs[] = {"libsycl.so", "libsycl.so.7", "libpi_level_zero.so", nullptr};

    for (int i = 0; sycl_libs[i]; ++i) {
        if (check_library_exists(sycl_libs[i])) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// GPU Backend Detection
// ============================================================================

auto detect_gpu_backend() -> gpu_backend {
    // Try detecting in order of preference: CUDA -> ROCm -> SYCL

#ifdef FASTJSON_ENABLE_CUDA
    if (detect_cuda_runtime() && cuda::detect_cuda()) {
        return gpu_backend::cuda;
    }
#endif

#ifdef __HIP__
    if (detect_rocm_runtime() && rocm::detect_rocm()) {
        return gpu_backend::rocm;
    }
#endif

#ifdef SYCL_LANGUAGE_VERSION
    if (detect_sycl_runtime() && sycl::detect_sycl()) {
        return gpu_backend::sycl;
    }
#endif

    // Runtime detection without compile-time support
    if (detect_cuda_runtime()) {
        return gpu_backend::cuda;
    }

    if (detect_rocm_runtime()) {
        return gpu_backend::rocm;
    }

    if (detect_sycl_runtime()) {
        return gpu_backend::sycl;
    }

    return gpu_backend::none;
}

auto get_gpu_info(gpu_backend backend) -> gpu_info {
    if (backend == gpu_backend::auto_detect) {
        backend = detect_gpu_backend();
    }

    gpu_info info;
    info.backend = backend;

    switch (backend) {
#ifdef FASTJSON_ENABLE_CUDA
        case gpu_backend::cuda:
            if (detect_cuda_runtime()) {
                return cuda::get_cuda_info();
            }
            break;
#endif

#ifdef __HIP__
        case gpu_backend::rocm:
            if (detect_rocm_runtime()) {
                return rocm::get_rocm_info();
            }
            break;
#endif

#ifdef SYCL_LANGUAGE_VERSION
        case gpu_backend::sycl:
            if (detect_sycl_runtime()) {
                return sycl::get_sycl_info();
            }
            break;
#endif

        case gpu_backend::none:
        case gpu_backend::auto_detect:
        default:
            break;
    }

    return info;
}

auto is_gpu_available() -> bool {
    return detect_gpu_backend() != gpu_backend::none;
}

// ============================================================================
// GPU Buffer Implementation
// ============================================================================

gpu_buffer::gpu_buffer(size_t size, gpu_backend backend)
    : size_(size), backend_(backend == gpu_backend::auto_detect ? detect_gpu_backend() : backend) {
    if (backend_ == gpu_backend::none) {
        return;
    }

    // Allocate device memory based on backend
    switch (backend_) {
        case gpu_backend::cuda:
#ifdef FASTJSON_ENABLE_CUDA
            if (detect_cuda_runtime()) {
                cudaMalloc(&device_ptr_, size_);
            }
#endif
            break;

        case gpu_backend::rocm:
#ifdef __HIP__
            if (detect_rocm_runtime()) {
                hipMalloc(&device_ptr_, size_);
            }
#endif
            break;

        case gpu_backend::sycl:
#ifdef SYCL_LANGUAGE_VERSION
            if (detect_sycl_runtime()) {
                // SYCL allocation would go here
            }
#endif
            break;

        default:
            break;
    }
}

gpu_buffer::~gpu_buffer() {
    if (device_ptr_ == nullptr) {
        return;
    }

    switch (backend_) {
        case gpu_backend::cuda:
#ifdef FASTJSON_ENABLE_CUDA
            if (detect_cuda_runtime()) {
                cudaFree(device_ptr_);
            }
#endif
            break;

        case gpu_backend::rocm:
#ifdef __HIP__
            if (detect_rocm_runtime()) {
                hipFree(device_ptr_);
            }
#endif
            break;

        case gpu_backend::sycl:
#ifdef SYCL_LANGUAGE_VERSION
            if (detect_sycl_runtime()) {
                // SYCL deallocation would go here
            }
#endif
            break;

        default:
            break;
    }

    device_ptr_ = nullptr;
}

gpu_buffer::gpu_buffer(gpu_buffer&& other) noexcept
    : device_ptr_(other.device_ptr_), size_(other.size_), backend_(other.backend_),
      stream_(other.stream_) {
    other.device_ptr_ = nullptr;
    other.stream_ = nullptr;
}

gpu_buffer& gpu_buffer::operator=(gpu_buffer&& other) noexcept {
    if (this != &other) {
        // Clean up existing resources
        this->~gpu_buffer();

        // Move from other
        device_ptr_ = other.device_ptr_;
        size_ = other.size_;
        backend_ = other.backend_;
        stream_ = other.stream_;

        other.device_ptr_ = nullptr;
        other.stream_ = nullptr;
    }
    return *this;
}

auto gpu_buffer::copy_to_device(const void* host_ptr, size_t size, size_t offset) -> bool {
    if (device_ptr_ == nullptr || size + offset > size_) {
        return false;
    }

    char* dest = static_cast<char*>(device_ptr_) + offset;

    switch (backend_) {
        case gpu_backend::cuda:
#ifdef FASTJSON_ENABLE_CUDA
            if (detect_cuda_runtime()) {
                return cudaMemcpy(dest, host_ptr, size, cudaMemcpyHostToDevice) == cudaSuccess;
            }
#endif
            break;

        case gpu_backend::rocm:
#ifdef __HIP__
            if (detect_rocm_runtime()) {
                return hipMemcpy(dest, host_ptr, size, hipMemcpyHostToDevice) == hipSuccess;
            }
#endif
            break;

        case gpu_backend::sycl:
            // SYCL copy implementation
            break;

        default:
            break;
    }

    return false;
}

auto gpu_buffer::copy_from_device(void* host_ptr, size_t size, size_t offset) -> bool {
    if (device_ptr_ == nullptr || size + offset > size_) {
        return false;
    }

    char* src = static_cast<char*>(device_ptr_) + offset;

    switch (backend_) {
        case gpu_backend::cuda:
#ifdef FASTJSON_ENABLE_CUDA
            if (detect_cuda_runtime()) {
                return cudaMemcpy(host_ptr, src, size, cudaMemcpyDeviceToHost) == cudaSuccess;
            }
#endif
            break;

        case gpu_backend::rocm:
#ifdef __HIP__
            if (detect_rocm_runtime()) {
                return hipMemcpy(host_ptr, src, size, hipMemcpyDeviceToHost) == hipSuccess;
            }
#endif
            break;

        case gpu_backend::sycl:
            // SYCL copy implementation
            break;

        default:
            break;
    }

    return false;
}

auto gpu_buffer::copy_to_device_async(const void* host_ptr, size_t size, size_t offset) -> bool {
    // Async copy implementation similar to sync but with streams
    return copy_to_device(host_ptr, size, offset);
}

auto gpu_buffer::copy_from_device_async(void* host_ptr, size_t size, size_t offset) -> bool {
    // Async copy implementation similar to sync but with streams
    return copy_from_device(host_ptr, size, offset);
}

// ============================================================================
// GPU Parsing Interface
// ============================================================================

auto parse_on_gpu(std::string_view input, const gpu_parse_config& config) -> gpu_parse_result {
    gpu_parse_result result;

    // Determine backend
    gpu_backend backend = config.backend;
    if (backend == gpu_backend::auto_detect) {
        backend = detect_gpu_backend();
    }

    // Check if GPU is available
    if (backend == gpu_backend::none) {
        result.success = false;
        result.error_message = "No GPU backend available";
        return result;
    }

    // Check minimum size threshold
    if (input.size() < config.min_size_for_gpu) {
        result.success = false;
        result.error_message = "Input size below GPU threshold, use CPU parsing";
        return result;
    }

    // Verify runtime libraries are loaded
    switch (backend) {
        case gpu_backend::cuda:
            if (!detect_cuda_runtime()) {
                result.success = false;
                result.error_message = "CUDA runtime not available";
                return result;
            }
#ifdef FASTJSON_ENABLE_CUDA
            return cuda::parse_on_cuda(input, config);
#else
            result.success = false;
            result.error_message = "CUDA support not compiled in";
            return result;
#endif

        case gpu_backend::rocm:
            if (!detect_rocm_runtime()) {
                result.success = false;
                result.error_message = "ROCm/HIP runtime not available";
                return result;
            }
#ifdef __HIP__
            return rocm::parse_on_rocm(input, config);
#else
            result.success = false;
            result.error_message = "ROCm support not compiled in";
            return result;
#endif

        case gpu_backend::sycl:
            if (!detect_sycl_runtime()) {
                result.success = false;
                result.error_message = "Intel oneAPI SYCL runtime not available";
                return result;
            }
#ifdef SYCL_LANGUAGE_VERSION
            return sycl::parse_on_sycl(input, config);
#else
            result.success = false;
            result.error_message = "SYCL support not compiled in";
            return result;
#endif

        default:
            result.success = false;
            result.error_message = "Unknown GPU backend";
            return result;
    }
}

// ============================================================================
// GPU Kernel Operations
// ============================================================================

auto gpu_find_whitespace(const char* input, size_t size, uint32_t* positions, size_t* count,
                         gpu_backend backend) -> bool {
    if (backend == gpu_backend::auto_detect) {
        backend = detect_gpu_backend();
    }

    switch (backend) {
#ifdef FASTJSON_ENABLE_CUDA
        case gpu_backend::cuda:
            if (detect_cuda_runtime()) {
                return cuda::cuda_find_whitespace(input, size, positions, count);
            }
            break;
#endif
        default:
            return false;
    }

    return false;
}

auto gpu_find_strings(const char* input, size_t size, uint32_t* positions, size_t* count,
                      gpu_backend backend) -> bool {
    // Similar implementation to whitespace
    return false;
}

auto gpu_find_numbers(const char* input, size_t size, uint32_t* positions, size_t* count,
                      gpu_backend backend) -> bool {
    // Similar implementation to whitespace
    return false;
}

auto gpu_find_structural_chars(const char* input, size_t size, uint32_t* positions,
                               uint8_t* char_types, size_t* count, gpu_backend backend) -> bool {
    if (backend == gpu_backend::auto_detect) {
        backend = detect_gpu_backend();
    }

    switch (backend) {
#ifdef FASTJSON_ENABLE_CUDA
        case gpu_backend::cuda:
            if (detect_cuda_runtime()) {
                return cuda::cuda_find_structural(input, size, positions, char_types, count);
            }
            break;
#endif
        default:
            return false;
    }

    return false;
}

// ============================================================================
// Performance Utilities
// ============================================================================

auto benchmark_gpu_vs_cpu(size_t test_size) -> std::vector<gpu_benchmark_result> {
    std::vector<gpu_benchmark_result> results;

    // Test each available backend
    std::vector<gpu_backend> backends = {gpu_backend::cuda, gpu_backend::rocm, gpu_backend::sycl};

    for (auto backend : backends) {
        auto info = get_gpu_info(backend);
        if (info.backend != gpu_backend::none) {
            gpu_benchmark_result result;
            result.backend = backend;
            result.input_size = test_size;
            result.details = info.device_name;
            // TODO: Implement actual benchmark
            results.push_back(result);
        }
    }

    return results;
}

auto get_optimal_gpu_config(size_t input_size, gpu_backend backend) -> gpu_parse_config {
    gpu_parse_config config;
    config.backend = backend;

    if (backend == gpu_backend::auto_detect) {
        backend = detect_gpu_backend();
    }

    auto info = get_gpu_info(backend);

    // Calculate optimal block size based on device capabilities
    if (info.max_threads_per_block > 0) {
        config.block_size = std::min(256u, static_cast<unsigned>(info.max_threads_per_block));
    }

    // Calculate optimal grid size
    if (info.compute_units > 0) {
        config.grid_size = info.compute_units * 4;  // 4 blocks per SM is a good heuristic
    }

    // Adjust thresholds based on device memory
    if (info.total_memory > 0) {
        config.min_size_for_gpu = std::max(static_cast<size_t>(10000), info.total_memory / 10000);
    }

    return config;
}

auto gpu_matrix_multiply(const float* A, const float* B, float* C, int M, int N, int K,
                         gpu_backend backend) -> bool {
#ifdef FASTJSON_ENABLE_CUDA
    return cuda::gpu_matrix_multiply(A, B, C, M, N, K, backend);
#else
    (void)A; (void)B; (void)C; (void)M; (void)N; (void)K; (void)backend;
    return false;
#endif
}

auto gpu_launch_triton_ptx(const char* ptx_code, const char* kernel_name, void** args,
                           int grid_size, int block_size) -> bool {
#ifdef FASTJSON_ENABLE_CUDA
    return cuda::gpu_launch_triton_ptx(ptx_code, kernel_name, args, grid_size, block_size);
#else
    (void)ptx_code; (void)kernel_name; (void)args; (void)grid_size; (void)block_size;
    return false;
#endif
}

}  // namespace gpu
}  // namespace fastjson
