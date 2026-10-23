/**
 * FastJSON vs simdjson Head-to-Head Benchmark
 *
 * Compares parsing, serialization, field access, array iteration,
 * and throughput using trading-relevant JSON payloads across both
 * OnDemand (streaming/indexed) and DOM (materialized) architectures.
 *
 * Build: cmake target fastjson_vs_simdjson_benchmark in parent project
 * Run:   ./fastjson_vs_simdjson_benchmark --benchmark_counters_tabular=true
 *
 * Author: Olumuyiwa Oluwasanmi
 * Date: 2026-02-22
 */

#include "benchmark.h"
#include <simdjson.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Forward-declare fastjson types (from the modules)
import fastjson;
import fastjson_multiregister_parser;
import fastjson_turbo;

namespace {

// ============================================================================
// Test Payload Generation
// ============================================================================

auto generate_small_payload() -> std::string {
    return R"({"symbol":"QQQ","timestamp":"2026-02-21T16:00:00Z","o":530.12,"h":532.45,"l":528.90,"c":531.78,"v":45230100,"vwap":530.89,"atr":3.42,"rsi":58.7})";
}

auto generate_medium_payload() -> std::string {
    std::string json = R"JSON({"Meta Data":{"1. Information":"Daily Prices","2. Symbol":"QQQ","3. Last Refreshed":"2026-02-21","4. Output Size":"Compact","5. Time Zone":"US/Eastern"},"Time Series (Daily)":{)JSON";

    for (int i = 0; i < 100; ++i) {
        if (i > 0) json += ',';
        int month = (i / 28) + 1;
        int day = (i % 28) + 1;
        double base = 520.0 + i * 0.3;
        int vol = 40000000 + i * 100000;
        json += "\"2026-";
        json += (month < 10 ? "0" : "") + std::to_string(month) + "-";
        json += (day < 10 ? "0" : "") + std::to_string(day) + "\":{";
        json += "\"1. open\":\"" + std::to_string(base) + "\",";
        json += "\"2. high\":\"" + std::to_string(base + 2.0) + "\",";
        json += "\"3. low\":\"" + std::to_string(base - 2.0) + "\",";
        json += "\"4. close\":\"" + std::to_string(base + 1.0) + "\",";
        json += "\"5. volume\":\"" + std::to_string(vol) + "\"}";
    }
    json += "}}";
    return json;
}

auto generate_large_payload() -> std::string {
    std::string json = R"({"header":{"format":"safetensors","num_tensors":128,"total_bytes":524288},"records":[)";

    for (int i = 0; i < 10000; ++i) {
        if (i > 0) json += ',';
        double base = 100.0 + (i % 500) * 0.1;
        int vol = 1000000 + i * 100;
        int sym = i % 25;
        json += "{\"id\":" + std::to_string(i);
        json += ",\"symbol\":\"SYM" + std::to_string(sym) + "\"";
        json += ",\"open\":" + std::to_string(base);
        json += ",\"high\":" + std::to_string(base + 1.0);
        json += ",\"low\":" + std::to_string(base - 1.0);
        json += ",\"close\":" + std::to_string(base + 0.5);
        json += ",\"volume\":" + std::to_string(vol);
        json += ",\"features\":[";
        for (int j = 0; j < 8; ++j) {
            if (j > 0) json += ',';
            json += std::to_string(0.1 * (i % 10) * (j + 1));
        }
        json += "]}";
    }
    json += "]}";
    return json;
}

// Pre-generate payloads (avoid allocation in benchmark loops)
const std::string g_small = generate_small_payload();
const std::string g_medium = generate_medium_payload();
const std::string g_large = generate_large_payload();

// simdjson requires padded strings
const simdjson::padded_string g_small_padded{g_small};
const simdjson::padded_string g_medium_padded{g_medium};
const simdjson::padded_string g_large_padded{g_large};

// ============================================================================
// 1. OnDemand / Streaming Parse Benchmarks (FastJSON Turbo vs SimdJSON OnDemand)
// ============================================================================

void BM_Parse_FastJSON_Turbo_Small(benchmark::State& state) {
    fastjson::turbo::turbo_parser parser;
    for (auto _ : state) {
        auto doc = fastjson::turbo::iterate(g_small, parser);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_small.size()));
}
BENCHMARK(BM_Parse_FastJSON_Turbo_Small);

void BM_Parse_SimdJSON_OnDemand_Small(benchmark::State& state) {
    simdjson::ondemand::parser parser;
    for (auto _ : state) {
        auto doc = parser.iterate(g_small_padded);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_small.size()));
}
BENCHMARK(BM_Parse_SimdJSON_OnDemand_Small);

void BM_Parse_FastJSON_Turbo_Medium(benchmark::State& state) {
    fastjson::turbo::turbo_parser parser;
    for (auto _ : state) {
        auto doc = fastjson::turbo::iterate(g_medium, parser);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_medium.size()));
}
BENCHMARK(BM_Parse_FastJSON_Turbo_Medium);

void BM_Parse_SimdJSON_OnDemand_Medium(benchmark::State& state) {
    simdjson::ondemand::parser parser;
    for (auto _ : state) {
        auto doc = parser.iterate(g_medium_padded);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_medium.size()));
}
BENCHMARK(BM_Parse_SimdJSON_OnDemand_Medium);

void BM_Parse_FastJSON_Turbo_Large(benchmark::State& state) {
    fastjson::turbo::turbo_parser parser;
    for (auto _ : state) {
        auto doc = fastjson::turbo::iterate(g_large, parser);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_large.size()));
}
BENCHMARK(BM_Parse_FastJSON_Turbo_Large);

void BM_Parse_SimdJSON_OnDemand_Large(benchmark::State& state) {
    simdjson::ondemand::parser parser;
    for (auto _ : state) {
        auto doc = parser.iterate(g_large_padded);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_large.size()));
}
BENCHMARK(BM_Parse_SimdJSON_OnDemand_Large);

// ============================================================================
// 2. DOM Parse Benchmarks (FastJSON MR vs SimdJSON DOM vs FastJSON Arena)
// ============================================================================

void BM_Parse_FastJSON_DOM_Small(benchmark::State& state) {
    for (auto _ : state) {
        auto result = fastjson::mr::parse(g_small);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_small.size()));
}
BENCHMARK(BM_Parse_FastJSON_DOM_Small);

void BM_Parse_SimdJSON_DOM_Small(benchmark::State& state) {
    simdjson::dom::parser parser;
    for (auto _ : state) {
        auto doc = parser.parse(g_small_padded);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_small.size()));
}
BENCHMARK(BM_Parse_SimdJSON_DOM_Small);

void BM_Parse_FastJSON_DOM_Medium(benchmark::State& state) {
    for (auto _ : state) {
        auto result = fastjson::mr::parse(g_medium);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_medium.size()));
}
BENCHMARK(BM_Parse_FastJSON_DOM_Medium);

void BM_Parse_SimdJSON_DOM_Medium(benchmark::State& state) {
    simdjson::dom::parser parser;
    for (auto _ : state) {
        auto doc = parser.parse(g_medium_padded);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_medium.size()));
}
BENCHMARK(BM_Parse_SimdJSON_DOM_Medium);

void BM_Parse_FastJSON_DOM_Large(benchmark::State& state) {
    for (auto _ : state) {
        auto result = fastjson::mr::parse(g_large);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_large.size()));
}
BENCHMARK(BM_Parse_FastJSON_DOM_Large);

void BM_Parse_SimdJSON_DOM_Large(benchmark::State& state) {
    simdjson::dom::parser parser;
    for (auto _ : state) {
        auto doc = parser.parse(g_large_padded);
        benchmark::DoNotOptimize(doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_large.size()));
}
BENCHMARK(BM_Parse_SimdJSON_DOM_Large);

void BM_Parse_FastJSON_Arena_Small(benchmark::State& state) {
    for (auto _ : state) {
        auto result = fastjson::parse_arena(std::string(g_small));
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_small.size()));
}
BENCHMARK(BM_Parse_FastJSON_Arena_Small);

void BM_Parse_FastJSON_Arena_Medium(benchmark::State& state) {
    for (auto _ : state) {
        auto result = fastjson::parse_arena(std::string(g_medium));
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_medium.size()));
}
BENCHMARK(BM_Parse_FastJSON_Arena_Medium);

void BM_Parse_FastJSON_Arena_Large(benchmark::State& state) {
    for (auto _ : state) {
        auto result = fastjson::parse_arena(std::string(g_large));
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_large.size()));
}
BENCHMARK(BM_Parse_FastJSON_Arena_Large);

// ============================================================================
// 3. Serialization Benchmarks (FastJSON only — simdjson is read-only)
// ============================================================================

void BM_Serialize_FastJSON_Small(benchmark::State& state) {
    auto val = fastjson::mr::parse(g_small).value();
    for (auto _ : state) {
        auto s = val.to_string();
        benchmark::DoNotOptimize(s);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_small.size()));
}
BENCHMARK(BM_Serialize_FastJSON_Small);

void BM_Serialize_FastJSON_Medium(benchmark::State& state) {
    auto val = fastjson::mr::parse(g_medium).value();
    for (auto _ : state) {
        auto s = val.to_string();
        benchmark::DoNotOptimize(s);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_medium.size()));
}
BENCHMARK(BM_Serialize_FastJSON_Medium);

void BM_Serialize_FastJSON_Large(benchmark::State& state) {
    auto val = fastjson::mr::parse(g_large).value();
    for (auto _ : state) {
        auto s = val.to_string();
        benchmark::DoNotOptimize(s);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_large.size()));
}
BENCHMARK(BM_Serialize_FastJSON_Large);

// ============================================================================
// 4. Field Access Benchmarks
// ============================================================================

// DOM Field Access (materialized AST)
void BM_FieldAccess_FastJSON_DOM(benchmark::State& state) {
    auto val = fastjson::mr::parse(g_small).value();
    for (auto _ : state) {
        double o = val["o"].as_float().value_or(0.0);
        double h = val["h"].as_float().value_or(0.0);
        double l = val["l"].as_float().value_or(0.0);
        double c = val["c"].as_float().value_or(0.0);
        double v = val["v"].as_float().value_or(0.0);
        benchmark::DoNotOptimize(o + h + l + c + v);
    }
}
BENCHMARK(BM_FieldAccess_FastJSON_DOM);

void BM_FieldAccess_SimdJSON_DOM(benchmark::State& state) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(g_small_padded).value();
    for (auto _ : state) {
        double o = doc["o"].get_double().value();
        double h = doc["h"].get_double().value();
        double l = doc["l"].get_double().value();
        double c = doc["c"].get_double().value();
        double v = doc["v"].get_double().value();
        benchmark::DoNotOptimize(o + h + l + c + v);
    }
}
BENCHMARK(BM_FieldAccess_SimdJSON_DOM);

// Streaming / OnDemand Field Access (Parse + Query)
void BM_FieldAccess_FastJSON_Turbo(benchmark::State& state) {
    fastjson::turbo::turbo_parser parser;
    for (auto _ : state) {
        auto doc = fastjson::turbo::iterate(g_small, parser).value();
        auto root = doc.root();
        auto obj = root.get_object().value();
        double o = obj["o"].get_double().value();
        double h = obj["h"].get_double().value();
        double l = obj["l"].get_double().value();
        double c = obj["c"].get_double().value();
        double v = obj["v"].get_double().value();
        benchmark::DoNotOptimize(o + h + l + c + v);
    }
}
BENCHMARK(BM_FieldAccess_FastJSON_Turbo);

void BM_FieldAccess_SimdJSON_OnDemand(benchmark::State& state) {
    simdjson::ondemand::parser parser;
    for (auto _ : state) {
        auto doc = parser.iterate(g_small_padded);
        double o = doc["o"].get_double().value();
        double h = doc["h"].get_double().value();
        double l = doc["l"].get_double().value();
        double c = doc["c"].get_double().value();
        double v = doc["v"].get_double().value();
        benchmark::DoNotOptimize(o + h + l + c + v);
    }
}
BENCHMARK(BM_FieldAccess_SimdJSON_OnDemand);

// ============================================================================
// 5. Array Iteration Benchmarks
// ============================================================================

// Materialized DOM Array Iteration
void BM_ArrayIteration_FastJSON_DOM(benchmark::State& state) {
    auto val = fastjson::mr::parse(g_large).value();
    for (auto _ : state) {
        double sum = 0.0;
        const auto* records = val["records"].as_array();
        if (records) {
            for (const auto& record : *records) {
                sum += record["close"].as_float().value_or(0.0);
            }
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_ArrayIteration_FastJSON_DOM);

void BM_ArrayIteration_SimdJSON_DOM(benchmark::State& state) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(g_large_padded).value();
    for (auto _ : state) {
        double sum = 0.0;
        for (auto record : doc["records"].get_array()) {
            sum += record["close"].get_double().value();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_ArrayIteration_SimdJSON_DOM);

// Streaming / OnDemand Array Iteration (Parse + Iterate)
void BM_ArrayIteration_FastJSON_Turbo(benchmark::State& state) {
    fastjson::turbo::turbo_parser parser;
    for (auto _ : state) {
        double sum = 0.0;
        auto doc = fastjson::turbo::iterate(g_large, parser).value();
        auto root = doc.root();
        auto obj = root.get_object().value();
        auto arr = obj["records"].get_array().value();
        for (auto item : arr) {
            auto rec = item.get_object().value();
            sum += rec["close"].get_double().value();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_ArrayIteration_FastJSON_Turbo);

void BM_ArrayIteration_SimdJSON_OnDemand(benchmark::State& state) {
    simdjson::ondemand::parser parser;
    for (auto _ : state) {
        double sum = 0.0;
        auto doc = parser.iterate(g_large_padded);
        for (auto record : doc["records"].get_array()) {
            sum += record["close"].get_double().value();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_ArrayIteration_SimdJSON_OnDemand);

// ============================================================================
// 6. Single Field Lookup from Large Document (10,000 Records, 1.3 MB)
// ============================================================================

void BM_SingleField_FastJSON_Turbo_Large(benchmark::State& state) {
    fastjson::turbo::turbo_parser parser;
    for (auto _ : state) {
        auto doc = fastjson::turbo::iterate(g_large, parser).value();
        auto root = doc.root();
        auto obj = root.get_object().value();
        auto records = obj["records"].get_array().value();
        auto first = *records.begin();
        auto inner = first.get_object().value();
        auto close_val = inner["close"].get_double().value();
        benchmark::DoNotOptimize(close_val);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_large.size()));
}
BENCHMARK(BM_SingleField_FastJSON_Turbo_Large);

void BM_SingleField_SimdJSON_OnDemand_Large(benchmark::State& state) {
    simdjson::ondemand::parser parser;
    for (auto _ : state) {
        auto doc = parser.iterate(g_large_padded);
        double close_val = doc["records"].at(0)["close"].get_double().value();
        benchmark::DoNotOptimize(close_val);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_large.size()));
}
BENCHMARK(BM_SingleField_SimdJSON_OnDemand_Large);

void BM_SingleField_FastJSON_DOM_Large(benchmark::State& state) {
    for (auto _ : state) {
        auto val = fastjson::mr::parse(g_large).value();
        auto close_val = val["records"].as_array()->at(0)["close"].as_float().value_or(0.0);
        benchmark::DoNotOptimize(close_val);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_large.size()));
}
BENCHMARK(BM_SingleField_FastJSON_DOM_Large);

// ============================================================================
// 7. Throughput Benchmarks (sustained parse, GB/s)
// ============================================================================

void BM_Throughput_FastJSON_Turbo(benchmark::State& state) {
    fastjson::turbo::turbo_parser parser;
    for (auto _ : state) {
        for (int i = 0; i < 100; ++i) {
            auto doc = fastjson::turbo::iterate(g_medium, parser);
            benchmark::DoNotOptimize(doc);
        }
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_medium.size() * 100));
    state.SetLabel(std::to_string(g_medium.size() / 1024) + "KB x 100");
}
BENCHMARK(BM_Throughput_FastJSON_Turbo);

void BM_Throughput_SimdJSON_OnDemand(benchmark::State& state) {
    simdjson::ondemand::parser parser;
    for (auto _ : state) {
        for (int i = 0; i < 100; ++i) {
            auto doc = parser.iterate(g_medium_padded);
            benchmark::DoNotOptimize(doc);
        }
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_medium.size() * 100));
    state.SetLabel(std::to_string(g_medium.size() / 1024) + "KB x 100");
}
BENCHMARK(BM_Throughput_SimdJSON_OnDemand);

void BM_Throughput_FastJSON_DOM(benchmark::State& state) {
    for (auto _ : state) {
        for (int i = 0; i < 100; ++i) {
            auto result = fastjson::mr::parse(g_medium);
            benchmark::DoNotOptimize(result);
        }
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_medium.size() * 100));
    state.SetLabel(std::to_string(g_medium.size() / 1024) + "KB x 100");
}
BENCHMARK(BM_Throughput_FastJSON_DOM);

void BM_Throughput_SimdJSON_DOM(benchmark::State& state) {
    simdjson::dom::parser parser;
    for (auto _ : state) {
        for (int i = 0; i < 100; ++i) {
            auto doc = parser.parse(g_medium_padded);
            benchmark::DoNotOptimize(doc);
        }
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * g_medium.size() * 100));
    state.SetLabel(std::to_string(g_medium.size() / 1024) + "KB x 100");
}
BENCHMARK(BM_Throughput_SimdJSON_DOM);

}  // namespace

BENCHMARK_MAIN();
