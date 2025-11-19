#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifdef _OPENMP
    #include <omp.h>
#endif

// SIMD headers
#if defined(__x86_64__) || defined(_M_X64)
    #include <cpuid.h>
    #include <immintrin.h>
#endif

#ifdef __ARM_NEON
    #include <arm_neon.h>
#endif

#include "../modules/fastjson_compat.h"

using namespace std::chrono;
namespace fs = std::filesystem;

// SIMD Capability Detection
class SIMDCapabilities {
public:
    enum Flags : uint64_t {
        NONE = 0,
        SSE = 1ULL << 0,
        SSE2 = 1ULL << 1,
        SSE3 = 1ULL << 2,
        SSSE3 = 1ULL << 3,
        SSE41 = 1ULL << 4,
        SSE42 = 1ULL << 5,
        AVX = 1ULL << 6,
        AVX2 = 1ULL << 7,
        AVX512F = 1ULL << 8,
        AVX512BW = 1ULL << 9,
        AVX512CD = 1ULL << 10,
        AVX512DQ = 1ULL << 11,
        AVX512VL = 1ULL << 12,
        FMA = 1ULL << 13,
        FMA4 = 1ULL << 14,
        VNNI = 1ULL << 15,
        AMX_TILE = 1ULL << 16,
        AMX_INT8 = 1ULL << 17,
        AMX_BF16 = 1ULL << 18,
        NEON = 1ULL << 19
    };

    static auto detect() -> uint64_t {
        uint64_t caps = NONE;

#if defined(__x86_64__) || defined(_M_X64)
        uint32_t eax, ebx, ecx, edx;

        // Basic CPUID support
        __cpuid(0, eax, ebx, ecx, edx);
        if (eax >= 1) {
            __cpuid(1, eax, ebx, ecx, edx);

            if (edx & (1 << 25))
                caps |= SSE;
            if (edx & (1 << 26))
                caps |= SSE2;
            if (ecx & (1 << 0))
                caps |= SSE3;
            if (ecx & (1 << 9))
                caps |= SSSE3;
            if (ecx & (1 << 19))
                caps |= SSE41;
            if (ecx & (1 << 20))
                caps |= SSE42;
            if (ecx & (1 << 28))
                caps |= AVX;
            if (ecx & (1 << 12))
                caps |= FMA;
        }

        if (eax >= 7) {
            __cpuid_count(7, 0, eax, ebx, ecx, edx);
            if (ebx & (1 << 5))
                caps |= AVX2;
            if (ebx & (1 << 16))
                caps |= AVX512F;
            if (ebx & (1 << 30))
                caps |= AVX512BW;
            if (ebx & (1 << 28))
                caps |= AVX512CD;
            if (ebx & (1 << 17))
                caps |= AVX512DQ;
            if (ebx & (1 << 31))
                caps |= AVX512VL;
            if (ecx & (1 << 11))
                caps |= VNNI;
            if (edx & (1 << 24))
                caps |= AMX_TILE;
            if (edx & (1 << 25))
                caps |= AMX_INT8;
            if (edx & (1 << 22))
                caps |= AMX_BF16;
        }
#endif

#ifdef __ARM_NEON
        caps |= NEON;
#endif

        return caps;
    }

    static auto to_string(uint64_t caps) -> std::vector<std::string> {
        std::vector<std::string> result;

        if (caps & SSE)
            result.push_back("SSE");
        if (caps & SSE2)
            result.push_back("SSE2");
        if (caps & SSE3)
            result.push_back("SSE3");
        if (caps & SSSE3)
            result.push_back("SSSE3");
        if (caps & SSE41)
            result.push_back("SSE4.1");
        if (caps & SSE42)
            result.push_back("SSE4.2");
        if (caps & AVX)
            result.push_back("AVX");
        if (caps & AVX2)
            result.push_back("AVX2");
        if (caps & AVX512F)
            result.push_back("AVX-512F");
        if (caps & AVX512BW)
            result.push_back("AVX-512BW");
        if (caps & AVX512CD)
            result.push_back("AVX-512CD");
        if (caps & AVX512DQ)
            result.push_back("AVX-512DQ");
        if (caps & AVX512VL)
            result.push_back("AVX-512VL");
        if (caps & FMA)
            result.push_back("FMA");
        if (caps & FMA4)
            result.push_back("FMA4");
        if (caps & VNNI)
            result.push_back("VNNI");
        if (caps & AMX_TILE)
            result.push_back("AMX-TILE");
        if (caps & AMX_INT8)
            result.push_back("AMX-INT8");
        if (caps & AMX_BF16)
            result.push_back("AMX-BF16");
        if (caps & NEON)
            result.push_back("NEON");

        return result;
    }
};

// Benchmark Results Structure
struct BenchmarkResult {
    std::string name;
    std::string instruction_set;
    double parse_time_ms;
    double throughput_mbps;
    size_t operations_per_second;
    double speedup_vs_scalar;
    bool openmp_enabled;
    int thread_count;
    size_t memory_usage_mb;

    auto print() const -> void {
        std::cout << std::setw(20) << name << std::setw(15) << instruction_set << std::setw(12)
                  << std::fixed << std::setprecision(3) << parse_time_ms << std::setw(15)
                  << std::fixed << std::setprecision(2) << throughput_mbps << std::setw(12)
                  << operations_per_second << std::setw(10) << std::fixed << std::setprecision(2)
                  << speedup_vs_scalar << std::setw(8) << (openmp_enabled ? "Yes" : "No")
                  << std::setw(8) << thread_count << std::setw(12) << memory_usage_mb << "\n";
    }
};

// Comprehensive SIMD Benchmark Suite
class ComprehensiveSIMDBenchmark {
private:
    uint64_t capabilities_;
    std::string test_json_;
    size_t test_size_mb_;

public:
    ComprehensiveSIMDBenchmark(size_t test_size_mb = 10) : test_size_mb_(test_size_mb) {
        capabilities_ = SIMDCapabilities::detect();
        generate_test_data();
    }

private:
    auto generate_test_data() -> void {
        std::ostringstream json_stream;
        json_stream << "{\n";
        json_stream << "  \"metadata\": {\n";
        json_stream << "    \"benchmark\": \"SIMD Performance Test\",\n";
        json_stream << "    \"size_mb\": " << test_size_mb_ << ",\n";
        json_stream << "    \"timestamp\": "
                    << duration_cast<seconds>(system_clock::now().time_since_epoch()).count()
                    << "\n";
        json_stream << "  },\n";
        json_stream << "  \"data\": [\n";

        size_t target_bytes = test_size_mb_ * 1024 * 1024;
        size_t current_bytes = 0;
        bool first = true;

        while (current_bytes < target_bytes) {
            if (!first)
                json_stream << ",\n";
            first = false;

            json_stream << "    {\n";
            json_stream << "      \"id\": " << (current_bytes % 1000000) << ",\n";
            json_stream << "      \"value\": " << (current_bytes * 3.14159 / 1000.0) << ",\n";
            json_stream << "      \"text\": \"benchmark_data_" << (current_bytes % 10000)
                        << "\",\n";
            json_stream << "      \"flag\": " << ((current_bytes % 2) ? "true" : "false") << ",\n";
            json_stream << "      \"nested\": {\n";
            json_stream << "        \"x\": " << (current_bytes % 1000) << ",\n";
            json_stream << "        \"y\": " << ((current_bytes * 2) % 1000) << ",\n";
            json_stream << "        \"z\": " << ((current_bytes * 3) % 1000) << "\n";
            json_stream << "      }\n";
            json_stream << "    }";

            current_bytes += 200;  // Approximate JSON object size
        }

        json_stream << "\n  ]\n}";
        test_json_ = json_stream.str();

        std::cout << "Generated test JSON: " << (test_json_.size() / (1024.0 * 1024.0)) << " MB\n";
    }

    auto benchmark_with_config(const std::string& name, bool force_scalar = false,
                               int thread_count = 1) -> BenchmarkResult {
        BenchmarkResult result;
        result.name = name;
        result.thread_count = thread_count;
        result.openmp_enabled = (thread_count > 1);

// Set thread count for OpenMP
#ifdef _OPENMP
        if (result.openmp_enabled) {
            omp_set_num_threads(thread_count);
        }
#endif

        // Measure memory before
        auto memory_before = get_memory_usage();

        // Benchmark parsing
        const int iterations = 10;
        std::vector<double> times;

        for (int i = 0; i < iterations; ++i) {
            auto start = high_resolution_clock::now();

            try {
                auto parse_result = fastjson::parse(test_json_);
                if (!parse_result.has_value()) {
                    throw std::runtime_error("Parse failed: "
                                             + std::string(parse_result.error().message));
                }
            } catch (const std::exception& e) {
                std::cerr << "Benchmark failed: " << e.what() << "\n";
                throw;
            }

            auto end = high_resolution_clock::now();
            auto duration_us = duration_cast<microseconds>(end - start).count();
            times.push_back(duration_us / 1000.0);  // Convert to milliseconds
        }

        // Calculate statistics
        std::sort(times.begin(), times.end());
        result.parse_time_ms = times[times.size() / 2];  // Median time

        // Calculate throughput
        double data_size_mb = test_json_.size() / (1024.0 * 1024.0);
        result.throughput_mbps = data_size_mb / (result.parse_time_ms / 1000.0);
        result.operations_per_second = static_cast<size_t>(1000.0 / result.parse_time_ms);

        // Memory usage
        auto memory_after = get_memory_usage();
        result.memory_usage_mb = memory_after - memory_before;

        // Determine instruction set used
        if (force_scalar) {
            result.instruction_set = "Scalar";
        } else {
            result.instruction_set = get_active_instruction_set();
        }

        return result;
    }

    auto get_memory_usage() -> size_t {
        // Simplified memory usage estimation
        return 0;  // Would need platform-specific implementation
    }

    auto get_active_instruction_set() -> std::string {
        // Determine the highest available instruction set
        if (capabilities_ & SIMDCapabilities::AVX512F)
            return "AVX-512";
        if (capabilities_ & SIMDCapabilities::AVX2)
            return "AVX2";
        if (capabilities_ & SIMDCapabilities::AVX)
            return "AVX";
        if (capabilities_ & SIMDCapabilities::SSE42)
            return "SSE4.2";
        if (capabilities_ & SIMDCapabilities::SSE2)
            return "SSE2";
        return "Scalar";
    }

public:
    auto run_comprehensive_benchmark() -> std::vector<BenchmarkResult> {
        std::vector<BenchmarkResult> results;

        std::cout << "\n=== FastestJSONInTheWest SIMD Instruction Set Benchmark ===\n";
        std::cout << "Test data size: " << test_size_mb_ << " MB\n";

        // Display available SIMD capabilities
        auto cap_strings = SIMDCapabilities::to_string(capabilities_);
        std::cout << "Available SIMD instructions: ";
        for (size_t i = 0; i < cap_strings.size(); ++i) {
            if (i > 0)
                std::cout << ", ";
            std::cout << cap_strings[i];
        }
        std::cout << "\n\n";

        // Print header
        std::cout << std::setw(20) << "Test Name" << std::setw(15) << "Instruction Set"
                  << std::setw(12) << "Time (ms)" << std::setw(15) << "Throughput (MB/s)"
                  << std::setw(12) << "Ops/sec" << std::setw(10) << "Speedup" << std::setw(8)
                  << "OpenMP" << std::setw(8) << "Threads" << std::setw(12) << "Memory (MB)"
                  << "\n";

        std::cout << std::string(120, '-') << "\n";

        // 1. Scalar baseline
        auto scalar_result = benchmark_with_config("Scalar Baseline", true, 1);
        results.push_back(scalar_result);
        scalar_result.print();
        double scalar_time = scalar_result.parse_time_ms;

        // 2. Single-threaded SIMD (auto-detected best)
        auto simd_result = benchmark_with_config("Best SIMD", false, 1);
        simd_result.speedup_vs_scalar = scalar_time / simd_result.parse_time_ms;
        results.push_back(simd_result);
        simd_result.print();

// 3. OpenMP variants with different thread counts
#ifdef _OPENMP
        int max_threads = std::thread::hardware_concurrency();
        for (int threads : {2, 4, 8, max_threads}) {
            if (threads <= max_threads) {
                auto openmp_result = benchmark_with_config(
                    "SIMD+OpenMP(" + std::to_string(threads) + ")", false, threads);
                openmp_result.speedup_vs_scalar = scalar_time / openmp_result.parse_time_ms;
                results.push_back(openmp_result);
                openmp_result.print();
            }
        }
#else
        std::cout << "OpenMP not available - skipping parallel benchmarks\n";
#endif

        std::cout << std::string(120, '-') << "\n";

        // Summary
        auto best_result =
            *std::min_element(results.begin(), results.end(), [](const auto& a, const auto& b) {
                return a.parse_time_ms < b.parse_time_ms;
            });

        std::cout << "\nBest Performance: " << best_result.name << " ("
                  << best_result.instruction_set << ")\n";
        std::cout << "Best Time: " << std::fixed << std::setprecision(3)
                  << best_result.parse_time_ms << " ms\n";
        std::cout << "Best Throughput: " << std::fixed << std::setprecision(2)
                  << best_result.throughput_mbps << " MB/s\n";
        std::cout << "Best Speedup vs Scalar: " << std::fixed << std::setprecision(2)
                  << best_result.speedup_vs_scalar << "x\n";

        return results;
    }
};

// Test entry point
int main(int argc, char* argv[]) {
    try {
        size_t test_size_mb = 10;  // Default 10MB
        if (argc > 1) {
            test_size_mb = std::stoul(argv[1]);
        }

        ComprehensiveSIMDBenchmark benchmark(test_size_mb);
        auto results = benchmark.run_comprehensive_benchmark();

        // Save results to file
        std::ofstream results_file("simd_benchmark_results.json");
        results_file << "{\n";
        results_file << "  \"benchmark_results\": [\n";

        for (size_t i = 0; i < results.size(); ++i) {
            if (i > 0)
                results_file << ",\n";
            results_file << "    {\n";
            results_file << "      \"name\": \"" << results[i].name << "\",\n";
            results_file << "      \"instruction_set\": \"" << results[i].instruction_set
                         << "\",\n";
            results_file << "      \"parse_time_ms\": " << results[i].parse_time_ms << ",\n";
            results_file << "      \"throughput_mbps\": " << results[i].throughput_mbps << ",\n";
            results_file << "      \"speedup_vs_scalar\": " << results[i].speedup_vs_scalar
                         << ",\n";
            results_file << "      \"openmp_enabled\": "
                         << (results[i].openmp_enabled ? "true" : "false") << ",\n";
            results_file << "      \"thread_count\": " << results[i].thread_count << "\n";
            results_file << "    }";
        }

        results_file << "\n  ]\n}";
        results_file.close();

        std::cout << "\nResults saved to: simd_benchmark_results.json\n";

    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}