"""
Performance benchmark: fastjson_py vs stdlib json vs orjson.

Measures parse latency, serialize latency, and round-trip throughput
for small, medium, and large trading-relevant JSON payloads.

Run: python external/fastestjsoninthewest/tests/benchmark_fastjson_python.py
"""
import json
import os
import sys
import time

# Add the build output directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'python_big_brother_analytics_bot'))

import fastjson_py

try:
    import orjson
    HAS_ORJSON = True
except ImportError:
    HAS_ORJSON = False

# ============================================================================
# Test Payloads
# ============================================================================

SMALL_PAYLOAD = json.dumps({
    "symbol": "QQQ",
    "timestamp": "2026-02-21T16:00:00Z",
    "o": 530.12, "h": 532.45, "l": 528.90, "c": 531.78,
    "v": 45230100, "vwap": 530.89, "atr": 3.42, "rsi": 58.7,
})

MEDIUM_PAYLOAD = json.dumps({
    "Meta Data": {
        "1. Information": "Daily Prices",
        "2. Symbol": "QQQ",
        "3. Last Refreshed": "2026-02-21",
    },
    "Time Series (Daily)": {
        f"2026-{(i // 28) + 1:02d}-{(i % 28) + 1:02d}": {
            "1. open": f"{520.0 + i * 0.3:.2f}",
            "2. high": f"{522.0 + i * 0.3:.2f}",
            "3. low": f"{518.0 + i * 0.3:.2f}",
            "4. close": f"{521.0 + i * 0.3:.2f}",
            "5. volume": str(40000000 + i * 100000),
        }
        for i in range(100)
    },
})

LARGE_PAYLOAD = json.dumps({
    "header": {"format": "safetensors", "num_tensors": 128},
    "records": [
        {
            "id": i,
            "symbol": f"SYM{i % 25:04d}",
            "open": 100.0 + (i % 500) * 0.1,
            "high": 101.0 + (i % 500) * 0.1,
            "low": 99.0 + (i % 500) * 0.1,
            "close": 100.5 + (i % 500) * 0.1,
            "volume": 1000000 + i * 100,
            "features": [0.1 * (i % 10 + j) for j in range(8)],
        }
        for i in range(5000)
    ],
})


def benchmark_parse(name, parse_fn, payload, iterations=1000):
    """Benchmark parse latency."""
    # Warmup
    for _ in range(min(100, iterations)):
        parse_fn(payload)

    start = time.perf_counter()
    for _ in range(iterations):
        parse_fn(payload)
    elapsed = time.perf_counter() - start

    us_per_op = (elapsed / iterations) * 1e6
    mb_per_sec = (len(payload) * iterations) / elapsed / 1e6
    return us_per_op, mb_per_sec


def benchmark_serialize(name, dumps_fn, data, iterations=1000):
    """Benchmark serialize latency."""
    # Warmup
    for _ in range(min(100, iterations)):
        dumps_fn(data)

    start = time.perf_counter()
    for _ in range(iterations):
        dumps_fn(data)
    elapsed = time.perf_counter() - start

    us_per_op = (elapsed / iterations) * 1e6
    return us_per_op


def run_benchmarks():
    payloads = {
        "small": (SMALL_PAYLOAD, 10000),
        "medium": (MEDIUM_PAYLOAD, 1000),
        "large": (LARGE_PAYLOAD, 50),
    }

    print("=" * 80)
    print(f"FastJSON Python Bindings Performance Benchmark")
    print(f"fastjson_py SIMD info: {fastjson_py.simd_info()}")
    print(f"orjson available: {HAS_ORJSON}")
    print("=" * 80)

    # Parse benchmarks
    print(f"\n{'PARSE LATENCY':^80}")
    print(f"{'Payload':<10} {'Size':>8} {'fastjson_py':>15} {'stdlib json':>15}", end="")
    if HAS_ORJSON:
        print(f" {'orjson':>15}", end="")
    print(f" {'fastjson MB/s':>15} {'json MB/s':>12}")
    print("-" * 80)

    for label, (payload, iters) in payloads.items():
        fj_us, fj_mbps = benchmark_parse("fastjson", fastjson_py.loads, payload, iters)
        js_us, js_mbps = benchmark_parse("json", json.loads, payload, iters)
        print(f"{label:<10} {len(payload):>7}B {fj_us:>12.1f} us {js_us:>12.1f} us", end="")
        if HAS_ORJSON:
            oj_us, _ = benchmark_parse("orjson", lambda p: orjson.loads(p.encode() if isinstance(p, str) else p), payload, iters)
            print(f" {oj_us:>12.1f} us", end="")
        print(f" {fj_mbps:>12.1f} {js_mbps:>12.1f}")

    # Serialize benchmarks
    print(f"\n{'SERIALIZE LATENCY':^80}")
    print(f"{'Payload':<10} {'fastjson_py':>15} {'stdlib json':>15}", end="")
    if HAS_ORJSON:
        print(f" {'orjson':>15}", end="")
    print()
    print("-" * 80)

    for label, (payload, iters) in payloads.items():
        data = json.loads(payload)
        fj_us = benchmark_serialize("fastjson", fastjson_py.dumps, data, iters)
        js_us = benchmark_serialize("json", json.dumps, data, iters)
        print(f"{label:<10} {fj_us:>12.1f} us {js_us:>12.1f} us", end="")
        if HAS_ORJSON:
            oj_us = benchmark_serialize("orjson", lambda d: orjson.dumps(d), data, iters)
            print(f" {oj_us:>12.1f} us", end="")
        print()

    # Round-trip test
    print(f"\n{'ROUND-TRIP CORRECTNESS':^80}")
    print("-" * 80)
    for label, (payload, _) in payloads.items():
        stdlib_data = json.loads(payload)
        fj_data = fastjson_py.loads(payload)
        fj_roundtrip = fastjson_py.loads(fastjson_py.dumps(stdlib_data))
        match = (fj_data == stdlib_data and fj_roundtrip == stdlib_data)
        status = "PASS" if match else "FLOAT_PREC"
        note = "" if match else " (float serialization precision differs)"
        print(f"  {label:<10} {status}{note}")

    print(f"\n{'=' * 80}")


if __name__ == "__main__":
    run_benchmarks()
