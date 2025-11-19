// FastestJSONInTheWest - ROCm/HIP GPU Accelerated JSON Parser
// Implements parallel JSON parsing using AMD GPUs via HIP
// ============================================================================

#pragma once

#include <expected>
#include <span>
#include <string_view>
#include <vector>

#include <hip/hip_runtime.h>

namespace fastjson {
namespace gpu {
namespace rocm {

// ============================================================================
// HIP Error Handling
// ============================================================================

#define HIP_CHECK(call)                                                                           \
    do {                                                                                          \
        hipError_t err = call;                                                                    \
        if (err != hipSuccess) {                                                                  \
            return std::unexpected(gpu_error{gpu_error_code::hip_error,                           \
                                             std::string("HIP error: ") + hipGetErrorString(err), \
                                             static_cast<int>(err)});                             \
        }                                                                                         \
    } while (0)

#define HIP_CHECK_THROW(call)                                                              \
    do {                                                                                   \
        hipError_t err = call;                                                             \
        if (err != hipSuccess) {                                                           \
            throw std::runtime_error(std::string("HIP error: ") + hipGetErrorString(err)); \
        }                                                                                  \
    } while (0)

// ============================================================================
// GPU Error Types
// ============================================================================

enum class gpu_error_code {
    no_device,
    unsupported_device,
    memory_allocation_failed,
    kernel_launch_failed,
    hip_error,
    invalid_input
};

struct gpu_error {
    gpu_error_code code;
    std::string message;
    int hip_error_code;
};

template <typename T> using gpu_result = std::expected<T, gpu_error>;

// ============================================================================
// GPU Device Information
// ============================================================================

struct gpu_device_info {
    int device_id;
    std::string name;
    size_t total_memory;
    size_t free_memory;
    int compute_units;
    int max_threads_per_block;
    bool supports_cooperative_launch;

    gpu_device_info() = default;
};

// ============================================================================
// ROCm Device Detection and Management
// ============================================================================

inline auto detect_rocm_devices() -> gpu_result<std::vector<gpu_device_info>> {
    int device_count = 0;
    hipError_t err = hipGetDeviceCount(&device_count);

    if (err != hipSuccess || device_count == 0) {
        return std::unexpected(gpu_error{
            gpu_error_code::no_device, "No ROCm-capable GPU devices found", static_cast<int>(err)});
    }

    std::vector<gpu_device_info> devices;
    devices.reserve(device_count);

    for (int i = 0; i < device_count; ++i) {
        hipDeviceProp_t prop;
        HIP_CHECK(hipGetDeviceProperties(&prop, i));

        size_t free_mem = 0, total_mem = 0;
        HIP_CHECK(hipSetDevice(i));
        HIP_CHECK(hipMemGetInfo(&free_mem, &total_mem));

        gpu_device_info info;
        info.device_id = i;
        info.name = prop.name;
        info.total_memory = total_mem;
        info.free_memory = free_mem;
        info.compute_units = prop.multiProcessorCount;
        info.max_threads_per_block = prop.maxThreadsPerBlock;
        info.supports_cooperative_launch = prop.cooperativeLaunch;

        devices.push_back(std::move(info));
    }

    return devices;
}

inline auto select_best_device() -> gpu_result<int> {
    auto devices_result = detect_rocm_devices();
    if (!devices_result) {
        return std::unexpected(devices_result.error());
    }

    auto& devices = *devices_result;
    if (devices.empty()) {
        return std::unexpected(
            gpu_error{gpu_error_code::no_device, "No suitable GPU devices found", 0});
    }

    // Select device with most free memory
    int best_device = 0;
    size_t max_free_mem = 0;

    for (size_t i = 0; i < devices.size(); ++i) {
        if (devices[i].free_memory > max_free_mem) {
            max_free_mem = devices[i].free_memory;
            best_device = static_cast<int>(i);
        }
    }

    HIP_CHECK(hipSetDevice(best_device));
    return best_device;
}

// ============================================================================
// GPU Memory Management (RAII)
// ============================================================================

template <typename T> class gpu_buffer {
public:
    gpu_buffer() : ptr_(nullptr), size_(0) {}

    explicit gpu_buffer(size_t count) : ptr_(nullptr), size_(count) {
        if (count > 0) {
            HIP_CHECK_THROW(hipMalloc(&ptr_, count * sizeof(T)));
        }
    }

    ~gpu_buffer() {
        if (ptr_) {
            hipFree(ptr_);  // Don't check error in destructor
        }
    }

    // No copy
    gpu_buffer(const gpu_buffer&) = delete;
    gpu_buffer& operator=(const gpu_buffer&) = delete;

    // Move only
    gpu_buffer(gpu_buffer&& other) noexcept : ptr_(other.ptr_), size_(other.size_) {
        other.ptr_ = nullptr;
        other.size_ = 0;
    }

    gpu_buffer& operator=(gpu_buffer&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                hipFree(ptr_);
            }
            ptr_ = other.ptr_;
            size_ = other.size_;
            other.ptr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    auto copy_to_device(const T* host_data, size_t count) -> gpu_result<void> {
        if (count > size_) {
            return std::unexpected(
                gpu_error{gpu_error_code::invalid_input, "Buffer too small for copy operation", 0});
        }
        HIP_CHECK(hipMemcpy(ptr_, host_data, count * sizeof(T), hipMemcpyHostToDevice));
        return {};
    }

    auto copy_to_host(T* host_data, size_t count) const -> gpu_result<void> {
        if (count > size_) {
            return std::unexpected(
                gpu_error{gpu_error_code::invalid_input, "Buffer too small for copy operation", 0});
        }
        HIP_CHECK(hipMemcpy(host_data, ptr_, count * sizeof(T), hipMemcpyDeviceToHost));
        return {};
    }

    auto data() -> T* { return ptr_; }

    auto data() const -> const T* { return ptr_; }

    auto size() const -> size_t { return size_; }

private:
    T* ptr_;
    size_t size_;
};

// ============================================================================
// GPU JSON Parsing Kernels (Forward Declarations)
// ============================================================================

// These will be implemented in rocm_kernels.hip
extern "C" {
auto launch_json_tokenizer_kernel(const char* input, size_t input_size, int* tokens,
                                  size_t max_tokens, int* token_count, int threads_per_block,
                                  int blocks) -> hipError_t;

auto launch_json_parser_kernel(const char* input, const int* tokens, int token_count, void* output,
                               int threads_per_block, int blocks) -> hipError_t;
}

// ============================================================================
// High-Level GPU Parse Interface
// ============================================================================

struct gpu_parse_config {
    int device_id = -1;               // -1 = auto-select
    size_t min_size_for_gpu = 10000;  // Only use GPU for inputs larger than this
    int threads_per_block = 256;
    bool enable_tokenizer = true;
};

inline auto parse_json_gpu(std::string_view input, const gpu_parse_config& config = {})
    -> gpu_result<void> {
    // This is a placeholder - full implementation will follow
    // For now, return not implemented
    return std::unexpected(gpu_error{gpu_error_code::unsupported_device,
                                     "GPU parsing not yet fully implemented - coming soon!", 0});
}

}  // namespace rocm
}  // namespace gpu
}  // namespace fastjson
