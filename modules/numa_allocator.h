// FastestJSONInTheWest - NUMA-Aware Memory Allocation
// Author: Olumuyiwa Oluwasanmi
// ============================================================================

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>

namespace fastjson {
namespace numa {

// ============================================================================
// NUMA Topology Detection
// ============================================================================

struct numa_node_info {
    int node_id = -1;
    size_t total_memory = 0;
    size_t free_memory = 0;
    std::vector<int> cpu_list;
    double distance_to_node_0 = 1.0;  // Relative distance metric
};

struct numa_topology {
    int num_nodes = 1;
    std::vector<numa_node_info> nodes;
    bool is_numa_available = false;
    bool is_interleaved = false;
};

// Detect NUMA topology of the system
auto detect_numa_topology() -> numa_topology;

// Get current thread's NUMA node
auto get_current_numa_node() -> int;

// ============================================================================
// NUMA-Aware Allocator
// ============================================================================

class numa_allocator {
public:
    numa_allocator();
    ~numa_allocator();
    
    // Allocate memory on specific NUMA node
    auto allocate(size_t size, int preferred_node = -1) -> void*;
    
    // Allocate memory interleaved across all NUMA nodes
    auto allocate_interleaved(size_t size) -> void*;
    
    // Allocate memory local to current thread's NUMA node
    auto allocate_local(size_t size) -> void*;
    
    // Free NUMA-allocated memory
    auto deallocate(void* ptr, size_t size) -> void;
    
    // Get allocation statistics
    auto get_allocated_bytes(int node = -1) const -> size_t;
    auto get_allocation_count(int node = -1) const -> size_t;
    
    // NUMA topology accessors
    auto get_topology() const -> const numa_topology& { return topology_; }
    auto is_numa_available() const -> bool { return topology_.is_numa_available; }
    auto get_num_nodes() const -> int { return topology_.num_nodes; }
    
private:
    static constexpr int MAX_NUMA_NODES = 16;
    numa_topology topology_;
    std::atomic<size_t> allocated_bytes_[MAX_NUMA_NODES];
    std::atomic<size_t> allocation_count_[MAX_NUMA_NODES];
    bool numa_lib_available_ = false;
};

// ============================================================================
// NUMA-Aware RAII Memory Buffer
// ============================================================================

template<typename T>
class numa_buffer {
public:
    numa_buffer() = default;
    
    // Allocate on specific node
    numa_buffer(size_t count, int preferred_node, numa_allocator& allocator)
        : size_(count), allocator_(&allocator) {
        data_ = static_cast<T*>(allocator.allocate(count * sizeof(T), preferred_node));
    }
    
    // Allocate interleaved
    numa_buffer(size_t count, numa_allocator& allocator, bool interleaved = true)
        : size_(count), allocator_(&allocator) {
        if (interleaved) {
            data_ = static_cast<T*>(allocator.allocate_interleaved(count * sizeof(T)));
        } else {
            data_ = static_cast<T*>(allocator.allocate_local(count * sizeof(T)));
        }
    }
    
    ~numa_buffer() {
        if (data_ && allocator_) {
            allocator_->deallocate(data_, size_ * sizeof(T));
        }
    }
    
    // No copy, move only
    numa_buffer(const numa_buffer&) = delete;
    numa_buffer& operator=(const numa_buffer&) = delete;
    
    numa_buffer(numa_buffer&& other) noexcept
        : data_(other.data_), size_(other.size_), allocator_(other.allocator_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }
    
    numa_buffer& operator=(numa_buffer&& other) noexcept {
        if (this != &other) {
            if (data_ && allocator_) {
                allocator_->deallocate(data_, size_ * sizeof(T));
            }
            data_ = other.data_;
            size_ = other.size_;
            allocator_ = other.allocator_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
    
    // Accessors
    auto data() -> T* { return data_; }
    auto data() const -> const T* { return data_; }
    auto size() const -> size_t { return size_; }
    auto operator[](size_t idx) -> T& { return data_[idx]; }
    auto operator[](size_t idx) const -> const T& { return data_[idx]; }
    
private:
    T* data_ = nullptr;
    size_t size_ = 0;
    numa_allocator* allocator_ = nullptr;
};

// ============================================================================
// NUMA-Aware Thread Binding
// ============================================================================

// Bind current thread to specific NUMA node
auto bind_thread_to_numa_node(int node) -> bool;

// Bind thread to CPUs on specific NUMA node
auto bind_thread_to_node_cpus(int node, int cpu_offset = 0) -> bool;

// Get optimal NUMA node for thread based on thread ID
auto get_optimal_node_for_thread(int thread_id, int num_threads, int num_nodes) -> int;

// ============================================================================
// NUMA-Aware Parallel Configuration
// ============================================================================

struct numa_parallel_config {
    bool enable_numa_awareness = true;
    bool bind_threads_to_nodes = true;
    bool use_interleaved_allocation = false;  // false = node-local allocation
    bool balance_work_by_node = true;
    int min_work_per_node = 1000;  // Minimum work size to justify NUMA awareness
};

// Apply NUMA configuration to OpenMP threads
auto configure_openmp_numa(const numa_parallel_config& config) -> void;

// Get thread-local allocator configured for NUMA
auto get_thread_local_allocator() -> numa_allocator&;

} // namespace numa
} // namespace fastjson
