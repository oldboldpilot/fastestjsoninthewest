/**
 * Lightweight, zero-dependency, header-only Google Benchmark compatibility layer.
 * Designed to compile on Linux and Windows (MSVC/clang-cl).
 *
 * Author: Olumuyiwa Oluwasanmi
 * Date: 2026-06-08
 */

#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <iomanip>
#include <numeric>
#include <algorithm>

namespace benchmark {

enum TimeUnit {
    kNanosecond,
    kMicrosecond,
    kMillisecond
};

template <class T>
inline void DoNotOptimize(T const& value) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "g"(value) : "memory");
#else
    // Fallback for MSVC / clang-cl
    volatile const T* p = &value;
    (void)p;
#endif
}

class State {
private:
    size_t max_iterations_{1000};
    size_t iterations_{0};
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    size_t bytes_processed_{0};
    std::vector<int64_t> range_;

public:
    State(size_t max_iter, const std::vector<int64_t>& range_args = {}) 
        : max_iterations_(max_iter), range_(range_args) {}

    struct Iterator {
        State* state_;
        bool operator!=(const Iterator&) const {
            return state_->KeepRunning();
        }
        Iterator& operator++() {
            return *this;
        }
        int operator*() { return 0; }
    };

    Iterator begin() { return Iterator{this}; }
    Iterator end() { return Iterator{this}; }

    bool KeepRunning() {
        if (iterations_ == 0) {
            start_time_ = std::chrono::high_resolution_clock::now();
        }
        if (iterations_ < max_iterations_) {
            iterations_++;
            return true;
        }
        end_time_ = std::chrono::high_resolution_clock::now();
        return false;
    }

    void SetBytesProcessed(size_t bytes) {
        bytes_processed_ = bytes;
    }

    void SetItemsProcessed(int64_t) {}
    void SetLabel(const std::string&) {}

    void SkipWithError(const char* msg) {
        std::cerr << "Error: " << msg << std::endl;
        iterations_ = max_iterations_;
    }
    void SkipWithError(const std::string& msg) {
        SkipWithError(msg.c_str());
    }

    int64_t range(size_t index) const {
        if (index < range_.size()) return range_[index];
        return 0;
    }

    size_t iterations() const { return iterations_; }
    size_t bytes_processed() const { return bytes_processed_; }
    
    double GetDurationSeconds() const {
        auto diff = end_time_ - start_time_;
        return std::chrono::duration<double>(diff).count();
    }
};

using BenchmarkFunction = std::function<void(State&)>;

struct BenchmarkRegistration {
    std::string name;
    BenchmarkFunction fn;
    bool has_range{false};
    int64_t range_start{0};
    int64_t range_end{0};
    std::vector<int64_t> args;

    BenchmarkRegistration(const std::string& n, BenchmarkFunction f)
        : name(n), fn(f) {}
};

inline std::vector<BenchmarkRegistration>& GetRegistry() {
    static std::vector<BenchmarkRegistration> registry;
    return registry;
}

class BenchmarkRegisterer {
private:
    size_t index_;
public:
    BenchmarkRegisterer(const std::string& name, BenchmarkFunction fn) {
        auto& registry = GetRegistry();
        registry.emplace_back(name, fn);
        index_ = registry.size() - 1;
    }
    BenchmarkRegisterer* operator->() { return this; }
    BenchmarkRegisterer* Range(int64_t start, int64_t end) {
        GetRegistry()[index_].has_range = true;
        GetRegistry()[index_].range_start = start;
        GetRegistry()[index_].range_end = end;
        return this;
    }
    BenchmarkRegisterer* Arg(int64_t val) {
        GetRegistry()[index_].args.push_back(val);
        return this;
    }
    BenchmarkRegisterer* Unit(TimeUnit) {
        return this;
    }
};

class Fixture {
public:
    virtual void SetUp(const State&) {}
    virtual void SetUp(State& state) { SetUp(const_cast<const State&>(state)); }
    virtual void TearDown(const State&) {}
    virtual void TearDown(State& state) { TearDown(const_cast<const State&>(state)); }
    virtual ~Fixture() = default;
};

#define BENCHMARK(fn) \
    static ::benchmark::BenchmarkRegisterer* benchmark_reg_##fn = \
        (new ::benchmark::BenchmarkRegisterer(#fn, fn))

#define BENCHMARK_DEFINE_F(FixtureClass, TestName) \
    class FixtureClass_##TestName##_Benchmark : public FixtureClass { \
    public: \
        void Run(::benchmark::State& state); \
    }; \
    static void FixtureClass_##TestName##_Benchmark_Wrapper(::benchmark::State& state) { \
        FixtureClass_##TestName##_Benchmark fixture; \
        fixture.SetUp(state); \
        fixture.Run(state); \
        fixture.TearDown(state); \
    } \
    void FixtureClass_##TestName##_Benchmark::Run

#define BENCHMARK_REGISTER_F(FixtureClass, TestName) \
    static ::benchmark::BenchmarkRegisterer* benchmark_reg_##FixtureClass_##TestName = \
        (new ::benchmark::BenchmarkRegisterer(#FixtureClass "/" #TestName, FixtureClass_##TestName##_Benchmark_Wrapper))

inline void RunAllBenchmarks() {
    std::cout << std::left 
              << std::setw(55) << "Benchmark" 
              << std::setw(15) << "Time" 
              << std::setw(15) << "Iterations" 
              << std::setw(20) << "Throughput" 
              << std::endl;
    std::cout << std::string(105, '-') << std::endl;

    for (const auto& reg : GetRegistry()) {
        std::vector<std::vector<int64_t>> runs;
        if (reg.has_range) {
            for (int64_t r = reg.range_start; r <= reg.range_end; r *= 8) {
                runs.push_back({r});
                if (r <= 0) break;
            }
        } else if (!reg.args.empty()) {
            for (auto arg : reg.args) {
                runs.push_back({arg});
            }
        } else {
            runs.push_back({});
        }

        for (const auto& run_args : runs) {
            std::string benchmark_name = reg.name;
            if (!run_args.empty()) {
                benchmark_name += "/" + std::to_string(run_args[0]);
            }

            // Step 1: Warmup and find appropriate iteration count
            size_t iters = 10;
            double duration = 0.0;
            while (iters < 10000000) {
                State state(iters, run_args);
                reg.fn(state);
                duration = state.GetDurationSeconds();
                if (duration >= 0.02) { // 20ms minimal run for warmup
                    break;
                }
                iters *= 10;
            }

            // Step 2: Run benchmark
            State state(iters, run_args);
            reg.fn(state);
            double total_seconds = state.GetDurationSeconds();
            double ns_per_iter = (total_seconds / state.iterations()) * 1e9;
            
            std::cout << std::left << std::setw(55) << benchmark_name;
            
            // Print Time
            if (ns_per_iter >= 1e6) {
                std::cout << std::setw(15) << (std::to_string(ns_per_iter / 1e6) + " ms");
            } else if (ns_per_iter >= 1e3) {
                std::cout << std::setw(15) << (std::to_string(ns_per_iter / 1e3) + " us");
            } else {
                std::cout << std::setw(15) << (std::to_string(ns_per_iter) + " ns");
            }

            std::cout << std::setw(15) << state.iterations();

            // Print Throughput
            if (state.bytes_processed() > 0) {
                double bytes_per_sec = static_cast<double>(state.bytes_processed()) / total_seconds;
                if (bytes_per_sec >= 1e9) {
                    std::cout << std::setw(20) << (std::to_string(bytes_per_sec / 1e9) + " GB/s");
                } else if (bytes_per_sec >= 1e6) {
                    std::cout << std::setw(20) << (std::to_string(bytes_per_sec / 1e6) + " MB/s");
                } else {
                    std::cout << std::setw(20) << (std::to_string(bytes_per_sec / 1e3) + " KB/s");
                }
            } else {
                std::cout << std::setw(20) << "-";
            }
            std::cout << std::endl;
        }
    }
}

} // namespace benchmark

#define BENCHMARK_MAIN() \
    int main(int, char**) { \
        ::benchmark::RunAllBenchmarks(); \
        return 0; \
    }
