// FastestJSONInTheWest - NUMA-Aware Memory Allocation Implementation
// Author: Olumuyiwa Oluwasanmi
// ============================================================================

#include "numa_allocator.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <dlfcn.h>

#ifdef __linux__
#include <sched.h>
#include <pthread.h>
#endif

namespace fastjson {
namespace numa {

// ============================================================================
// Dynamic NUMA Library Loading
// ============================================================================

struct numa_functions {
    int (*numa_available)() = nullptr;
    int (*numa_num_configured_nodes)() = nullptr;
    long long (*numa_node_size64)(int node, long long *freep) = nullptr;
    void* (*numa_alloc_onnode)(size_t size, int node) = nullptr;
    void* (*numa_alloc_interleaved)(size_t size) = nullptr;
    void* (*numa_alloc_local)(size_t size) = nullptr;
    void (*numa_free)(void *start, size_t size) = nullptr;
    int (*numa_node_of_cpu)(int cpu) = nullptr;
    int (*get_mempolicy)(int *mode, unsigned long *nodemask, unsigned long maxnode,
                         void *addr, unsigned long flags) = nullptr;
    int (*set_mempolicy)(int mode, const unsigned long *nodemask, unsigned long maxnode) = nullptr;
    struct bitmask* (*numa_allocate_cpumask)() = nullptr;
    void (*numa_bitmask_free)(struct bitmask *bmp) = nullptr;
    int (*numa_node_to_cpus)(int node, struct bitmask *mask) = nullptr;
};

static numa_functions g_numa_funcs;
static void* g_numa_lib_handle = nullptr;
static bool g_numa_initialized = false;

static auto init_numa_library() -> bool {
    if (g_numa_initialized) {
        return g_numa_funcs.numa_available != nullptr;
    }
    
    g_numa_initialized = true;
    
#ifdef __linux__
    // Try to load libnuma dynamically
    g_numa_lib_handle = dlopen("libnuma.so.1", RTLD_NOW);
    if (!g_numa_lib_handle) {
        g_numa_lib_handle = dlopen("libnuma.so", RTLD_NOW);
    }
    
    if (!g_numa_lib_handle) {
        return false;
    }
    
    // Load function pointers
    #define LOAD_FUNC(name) \
        g_numa_funcs.name = reinterpret_cast<decltype(g_numa_funcs.name)>(dlsym(g_numa_lib_handle, #name))
    
    LOAD_FUNC(numa_available);
    LOAD_FUNC(numa_num_configured_nodes);
    LOAD_FUNC(numa_node_size64);
    LOAD_FUNC(numa_alloc_onnode);
    LOAD_FUNC(numa_alloc_interleaved);
    LOAD_FUNC(numa_alloc_local);
    LOAD_FUNC(numa_free);
    LOAD_FUNC(numa_node_of_cpu);
    LOAD_FUNC(numa_allocate_cpumask);
    LOAD_FUNC(numa_bitmask_free);
    LOAD_FUNC(numa_node_to_cpus);
    
    #undef LOAD_FUNC
    
    // Check if NUMA is actually available
    if (g_numa_funcs.numa_available && g_numa_funcs.numa_available() >= 0) {
        return true;
    }
#endif
    
    return false;
}

// ============================================================================
// NUMA Topology Detection
// ============================================================================

auto detect_numa_topology() -> numa_topology {
    numa_topology topo;
    
    if (!init_numa_library() || !g_numa_funcs.numa_available) {
        topo.is_numa_available = false;
        topo.num_nodes = 1;
        return topo;
    }
    
    int available = g_numa_funcs.numa_available();
    if (available < 0) {
        topo.is_numa_available = false;
        topo.num_nodes = 1;
        return topo;
    }
    
    topo.is_numa_available = true;
    topo.num_nodes = g_numa_funcs.numa_num_configured_nodes();
    topo.nodes.resize(topo.num_nodes);
    
    for (int node = 0; node < topo.num_nodes; ++node) {
        numa_node_info& info = topo.nodes[node];
        info.node_id = node;
        
        // Get memory info
        long long free_mem = 0;
        long long total_mem = g_numa_funcs.numa_node_size64(node, &free_mem);
        info.total_memory = total_mem > 0 ? total_mem : 0;
        info.free_memory = free_mem > 0 ? free_mem : 0;
        
        // Get CPU list from sysfs
        std::string cpulist_path = "/sys/devices/system/node/node" + std::to_string(node) + "/cpulist";
        std::ifstream cpulist_file(cpulist_path);
        if (cpulist_file.is_open()) {
            std::string cpulist;
            std::getline(cpulist_file, cpulist);
            
            // Parse CPU list (e.g., "0-7,16-23" or "0,1,2,3")
            std::istringstream ss(cpulist);
            std::string token;
            while (std::getline(ss, token, ',')) {
                size_t dash_pos = token.find('-');
                if (dash_pos != std::string::npos) {
                    int start = std::stoi(token.substr(0, dash_pos));
                    int end = std::stoi(token.substr(dash_pos + 1));
                    for (int cpu = start; cpu <= end; ++cpu) {
                        info.cpu_list.push_back(cpu);
                    }
                } else {
                    info.cpu_list.push_back(std::stoi(token));
                }
            }
        }
        
        // Simple distance metric (can be refined with /sys/devices/system/node/nodeX/distance)
        info.distance_to_node_0 = (node == 0) ? 1.0 : 1.5;
    }
    
    return topo;
}

auto get_current_numa_node() -> int {
    if (!init_numa_library() || !g_numa_funcs.numa_available) {
        return 0;
    }
    
#ifdef __linux__
    int cpu = sched_getcpu();
    if (cpu >= 0 && g_numa_funcs.numa_node_of_cpu) {
        int node = g_numa_funcs.numa_node_of_cpu(cpu);
        return node >= 0 ? node : 0;
    }
#endif
    
    return 0;
}

// ============================================================================
// NUMA Allocator Implementation
// ============================================================================

numa_allocator::numa_allocator() {
    numa_lib_available_ = init_numa_library();
    topology_ = detect_numa_topology();
    
    // Initialize atomic counters
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        allocated_bytes_[i].store(0);
        allocation_count_[i].store(0);
    }
}

numa_allocator::~numa_allocator() {
    // Statistics are automatically cleaned up
}

auto numa_allocator::allocate(size_t size, int preferred_node) -> void* {
    if (!numa_lib_available_ || !topology_.is_numa_available) {
        // Fallback to regular allocation
        return std::aligned_alloc(64, size);  // 64-byte alignment for cache lines
    }
    
    if (preferred_node < 0) {
        preferred_node = get_current_numa_node();
    }
    
    if (preferred_node >= topology_.num_nodes) {
        preferred_node = 0;
    }
    
    void* ptr = g_numa_funcs.numa_alloc_onnode(size, preferred_node);
    
    if (ptr) {
        allocated_bytes_[preferred_node].fetch_add(size, std::memory_order_relaxed);
        allocation_count_[preferred_node].fetch_add(1, std::memory_order_relaxed);
    }
    
    return ptr;
}

auto numa_allocator::allocate_interleaved(size_t size) -> void* {
    if (!numa_lib_available_ || !topology_.is_numa_available) {
        return std::aligned_alloc(64, size);
    }
    
    void* ptr = g_numa_funcs.numa_alloc_interleaved(size);
    
    if (ptr) {
        // Count on node 0 for interleaved allocations
        allocated_bytes_[0].fetch_add(size, std::memory_order_relaxed);
        allocation_count_[0].fetch_add(1, std::memory_order_relaxed);
    }
    
    return ptr;
}

auto numa_allocator::allocate_local(size_t size) -> void* {
    if (!numa_lib_available_ || !topology_.is_numa_available) {
        return std::aligned_alloc(64, size);
    }
    
    int node = get_current_numa_node();
    void* ptr = g_numa_funcs.numa_alloc_local(size);
    
    if (ptr) {
        allocated_bytes_[node].fetch_add(size, std::memory_order_relaxed);
        allocation_count_[node].fetch_add(1, std::memory_order_relaxed);
    }
    
    return ptr;
}

auto numa_allocator::deallocate(void* ptr, size_t size) -> void {
    if (!ptr) return;
    
    if (!numa_lib_available_ || !topology_.is_numa_available) {
        std::free(ptr);
        return;
    }
    
    g_numa_funcs.numa_free(ptr, size);
}

auto numa_allocator::get_allocated_bytes(int node) const -> size_t {
    if (node < 0) {
        size_t total = 0;
        for (int i = 0; i < topology_.num_nodes && i < MAX_NUMA_NODES; ++i) {
            total += allocated_bytes_[i].load(std::memory_order_relaxed);
        }
        return total;
    }
    
    if (node >= MAX_NUMA_NODES || node >= topology_.num_nodes) {
        return 0;
    }
    
    return allocated_bytes_[node].load(std::memory_order_relaxed);
}

auto numa_allocator::get_allocation_count(int node) const -> size_t {
    if (node < 0) {
        size_t total = 0;
        for (int i = 0; i < topology_.num_nodes && i < MAX_NUMA_NODES; ++i) {
            total += allocation_count_[i].load(std::memory_order_relaxed);
        }
        return total;
    }
    
    if (node >= MAX_NUMA_NODES || node >= topology_.num_nodes) {
        return 0;
    }
    
    return allocation_count_[node].load(std::memory_order_relaxed);
}

// ============================================================================
// Thread Binding
// ============================================================================

auto bind_thread_to_numa_node(int node) -> bool {
#ifdef __linux__
    if (!init_numa_library() || !g_numa_funcs.numa_available) {
        return false;
    }
    
    auto topo = detect_numa_topology();
    if (!topo.is_numa_available || node >= topo.num_nodes) {
        return false;
    }
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    // Add all CPUs from this NUMA node
    for (int cpu : topo.nodes[node].cpu_list) {
        CPU_SET(cpu, &cpuset);
    }
    
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    return false;
#endif
}

auto bind_thread_to_node_cpus(int node, int cpu_offset) -> bool {
#ifdef __linux__
    if (!init_numa_library() || !g_numa_funcs.numa_available) {
        return false;
    }
    
    auto topo = detect_numa_topology();
    if (!topo.is_numa_available || node >= topo.num_nodes) {
        return false;
    }
    
    const auto& cpus = topo.nodes[node].cpu_list;
    if (cpus.empty() || cpu_offset >= static_cast<int>(cpus.size())) {
        return false;
    }
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpus[cpu_offset], &cpuset);
    
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    return false;
#endif
}

auto get_optimal_node_for_thread(int thread_id, int num_threads, int num_nodes) -> int {
    if (num_nodes <= 1) {
        return 0;
    }
    
    // Distribute threads evenly across NUMA nodes
    return thread_id % num_nodes;
}

// ============================================================================
// OpenMP NUMA Configuration
// ============================================================================

auto configure_openmp_numa(const numa_parallel_config& config) -> void {
    if (!config.enable_numa_awareness) {
        return;
    }
    
    if (!init_numa_library()) {
        return;
    }
    
    auto topo = detect_numa_topology();
    if (!topo.is_numa_available) {
        return;
    }
    
    // OpenMP thread binding will be done at runtime using bind_thread_to_numa_node
    // This is called from within parallel regions
}

// Thread-local allocator
thread_local numa_allocator g_thread_local_allocator;

auto get_thread_local_allocator() -> numa_allocator& {
    return g_thread_local_allocator;
}

} // namespace numa
} // namespace fastjson
