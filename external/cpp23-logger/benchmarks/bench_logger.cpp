// Micro-benchmark for the cpp23-logger hot paths (format + named templates).
// Build mirrors the library's canonical flags; profiled with perf/VTune.
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

import logger;

using namespace logger;

template <typename F>
static auto time_ms(int iters, F&& f) -> double {
    auto const t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; ++i) f(i);
    auto const t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

auto main() -> int {
    constexpr int N = 500000;
    volatile std::size_t sink = 0;

    // Path 1: positional format() via std::vformat
    double const t_fmt = time_ms(N, [&](int i) {
        auto s = Logger::format("Trade #{}: {} {} @ ${} risk={}", i, "BUY", 100, 245.67, 0.12);
        sink += s.size();
    });

    // Path 2: named-template processTemplate() (exercises TemplateValue::to_string -> ostringstream)
    double const t_tmpl = time_ms(N, [&](int i) {
        auto s = Logger::processTemplate("User {id} from {ip} step {step} t={temp}",
                                         {{"id", i}, {"ip", "192.168.1.100"}, {"step", i % 64}, {"temp", 36.6}});
        sink += s.size();
    });

    std::printf("format()         : %8.1f ms  (%6.2f M ops/s)\n", t_fmt, N / t_fmt / 1000.0);
    std::printf("processTemplate(): %8.1f ms  (%6.2f M ops/s)\n", t_tmpl, N / t_tmpl / 1000.0);
    std::printf("sink=%zu\n", static_cast<std::size_t>(sink));
    return 0;
}
