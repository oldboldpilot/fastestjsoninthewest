// NUMA Awareness Test
// Author: Olumuyiwa Oluwasanmi

#include <cstring>
#include <iomanip>
#include <iostream>

#include "modules/numa_allocator.h"

auto main() -> int {
    using namespace fastjson::numa;

    std::cout << "=== NUMA Topology Detection ===\n\n";

    auto topo = detect_numa_topology();

    std::cout << "NUMA Available: " << (topo.is_numa_available ? "YES" : "NO") << "\n";
    std::cout << "Number of NUMA Nodes: " << topo.num_nodes << "\n\n";

    if (topo.is_numa_available) {
        for (int i = 0; i < topo.num_nodes; ++i) {
            const auto& node = topo.nodes[i];
            std::cout << "Node " << node.node_id << ":\n";
            std::cout << "  Total Memory: " << (node.total_memory / (1024.0 * 1024.0 * 1024.0))
                      << " GB\n";
            std::cout << "  Free Memory:  " << (node.free_memory / (1024.0 * 1024.0 * 1024.0))
                      << " GB\n";
            std::cout << "  CPUs: ";
            for (size_t j = 0; j < node.cpu_list.size(); ++j) {
                if (j > 0)
                    std::cout << ",";
                std::cout << node.cpu_list[j];
                if (j >= 15) {
                    std::cout << "...(" << (node.cpu_list.size() - j - 1) << " more)";
                    break;
                }
            }
            std::cout << "\n";
            std::cout << "  Distance to Node 0: " << node.distance_to_node_0 << "\n\n";
        }
    }

    std::cout << "Current NUMA Node: " << get_current_numa_node() << "\n\n";

    // Test allocations
    std::cout << "=== Testing NUMA Allocations ===\n\n";

    numa_allocator allocator;

    std::cout << "Allocator initialized:\n";
    std::cout << "  NUMA Available: " << allocator.is_numa_available() << "\n";
    std::cout << "  Number of Nodes: " << allocator.get_num_nodes() << "\n\n";

    // Test local allocation
    constexpr size_t test_size = 1024 * 1024;  // 1 MB

    std::cout << "Allocating " << (test_size / 1024) << " KB locally...\n";
    void* local_ptr = allocator.allocate_local(test_size);
    if (local_ptr) {
        std::cout << "  Success! Allocated at: " << local_ptr << "\n";

        // Touch the memory to ensure it's actually allocated
        ::memset(local_ptr, 0xAA, test_size);

        std::cout << "  Statistics:\n";
        for (int i = 0; i < allocator.get_num_nodes(); ++i) {
            size_t bytes = allocator.get_allocated_bytes(i);
            size_t count = allocator.get_allocation_count(i);
            if (bytes > 0 || count > 0) {
                std::cout << "    Node " << i << ": " << (bytes / 1024) << " KB, " << count
                          << " allocations\n";
            }
        }

        allocator.deallocate(local_ptr, test_size);
        std::cout << "  Deallocated successfully\n\n";
    } else {
        std::cout << "  Failed to allocate\n\n";
    }

    // Test interleaved allocation
    std::cout << "Allocating " << (test_size / 1024) << " KB interleaved...\n";
    void* interleaved_ptr = allocator.allocate_interleaved(test_size);
    if (interleaved_ptr) {
        std::cout << "  Success! Allocated at: " << interleaved_ptr << "\n";
        ::memset(interleaved_ptr, 0xBB, test_size);
        allocator.deallocate(interleaved_ptr, test_size);
        std::cout << "  Deallocated successfully\n\n";
    } else {
        std::cout << "  Failed to allocate\n\n";
    }

    // Test node-specific allocation
    if (topo.num_nodes > 1) {
        std::cout << "Allocating " << (test_size / 1024) << " KB on each node...\n";
        for (int node = 0; node < topo.num_nodes; ++node) {
            void* node_ptr = allocator.allocate(test_size, node);
            if (node_ptr) {
                std::cout << "  Node " << node << ": Success at " << node_ptr << "\n";
                ::memset(node_ptr, 0xCC, test_size);
                allocator.deallocate(node_ptr, test_size);
            } else {
                std::cout << "  Node " << node << ": Failed\n";
            }
        }
        std::cout << "\n";
    }

    // Test NUMA buffer
    std::cout << "=== Testing NUMA Buffer ===\n\n";

    {
        numa_buffer<double> buffer(10000, allocator, true);  // Interleaved
        std::cout << "Created interleaved buffer of " << buffer.size() << " doubles\n";
        std::cout << "  Buffer address: " << buffer.data() << "\n";

        // Use the buffer
        for (size_t i = 0; i < buffer.size(); ++i) {
            buffer[i] = static_cast<double>(i);
        }

        std::cout << "  Buffer[0] = " << buffer[0] << "\n";
        std::cout << "  Buffer[9999] = " << buffer[9999] << "\n";
    }
    std::cout << "  Buffer automatically deallocated\n\n";

    std::cout << "=== Thread Binding Test ===\n\n";

    if (topo.num_nodes > 1) {
        std::cout << "Attempting to bind current thread to node 0...\n";
        if (bind_thread_to_numa_node(0)) {
            std::cout << "  Success!\n";
            std::cout << "  Current node after binding: " << get_current_numa_node() << "\n";
        } else {
            std::cout << "  Failed (may require privileges)\n";
        }
    } else {
        std::cout << "Single NUMA node system, thread binding not needed\n";
    }

    std::cout << "\n=== NUMA Test Complete ===\n";

    return 0;
}
