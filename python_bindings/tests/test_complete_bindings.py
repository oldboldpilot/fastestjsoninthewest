"""
Complete verification and regression suite for FastestJSONInTheWest Python bindings.

Covers:
- High-speed SIMD TurboDOM API: loads, parse_turbo, loads_file, TurboParser
- Serialization API: dumps, dumps_bytes, stringify, prettify, JSONValue.to_json
- PostgreSQL binary jsonb wire adapter: parse_pg_jsonb, loads_pg_jsonb, dumps_pg_jsonb
- Typed C++ vectors: Int64Vector, UInt64Vector, Float64Vector, StringVector
- Complete JSONValue API: constructors, factories, COW, dict/list protocol, 128-bit ints
- Complete lazy JSONQuery (LINQ) transducer pipeline & early termination
- Parallel parsing (parse_parallel), Mustache templating with dict & JSONValue, GPU fallbacks
- Strict RFC 8259 validation, UTF-8 / surrogate / control-char rejection, depth limits
"""

import concurrent.futures
import json
import math
import os
import tempfile

import fastjson
import numpy as np
import pytest


class TestTurboLoadsAndDumps:
    """Tests for SIMD TurboDOM loads(), parse_turbo(), TurboParser, and dumps()."""

    def test_loads_primitives_exact_types(self):
        assert fastjson.loads("null") is None
        assert fastjson.loads("true") is True
        assert fastjson.loads("false") is False

        # TurboDOM preserves exact Python int vs float distinction
        iv = fastjson.loads("42")
        assert iv == 42 and type(iv) is int

        neg_zero_int = fastjson.loads("-0")
        assert neg_zero_int == 0 and type(neg_zero_int) is int

        neg_zero_flt = fastjson.loads("-0.0")
        assert neg_zero_flt == 0.0 and type(neg_zero_flt) is float
        assert math.copysign(1.0, neg_zero_flt) == -1.0

        fv = fastjson.loads("3.141592653589793")
        assert fv == 3.141592653589793 and type(fv) is float

        sv = fastjson.loads('"hello\\nworld\\t\\u0041"')
        assert sv == "hello\nworld\tA"

    def test_loads_input_buffer_types(self):
        payload_dict = {"k": [1, 2, "🚀"], "ok": True}
        raw_str = json.dumps(payload_dict, ensure_ascii=False)
        raw_bytes = raw_str.encode("utf-8")
        raw_bytearray = bytearray(raw_bytes)
        raw_memview = memoryview(raw_bytes)

        assert fastjson.loads(raw_str) == payload_dict
        assert fastjson.loads(raw_bytes) == payload_dict
        assert fastjson.loads(raw_bytearray) == payload_dict
        assert fastjson.loads(raw_memview) == payload_dict
        assert fastjson.parse_turbo(raw_bytes) == payload_dict

    def test_loads_64bit_and_128bit_exact_integers(self):
        cases = [
            0,
            -1,
            (1 << 63) - 1,       # INT64_MAX
            -(1 << 63),          # INT64_MIN
            (1 << 63),           # INT64_MAX + 1 (uint64)
            (1 << 64) - 1,       # UINT64_MAX
            (1 << 64),           # UINT64_MAX + 1 (uint128)
            -(1 << 63) - 1,      # INT64_MIN - 1 (int128)
            (1 << 127) - 1,      # INT128_MAX
            -(1 << 127),         # INT128_MIN
            (1 << 128) - 1,      # UINT128_MAX
        ]
        payload = json.dumps(cases)
        parsed = fastjson.loads(payload)
        assert parsed == cases
        for got, expected in zip(parsed, cases):
            assert type(got) is int
            assert got == expected

    def test_loads_escaped_unicode_and_surrogate_pairs(self):
        # G-clef U+1D11E encoded as UTF-16 surrogate pair \uD834\uDD1E
        s = fastjson.loads(r'"\uD834\uDD1E and \u65E5\u672C\u8A9E and \"\\\/\b\f\n\r\t"')
        assert s == "𝄞 and 日本語 and \"\\/\b\f\n\r\t"

        # Long string (>8 bytes inline threshold) and short string (<=8 bytes)
        obj = fastjson.loads('{"short": "abcd", "exact8": "12345678", "long_key_name": "abcdefghijkl"}')
        assert obj == {"short": "abcd", "exact8": "12345678", "long_key_name": "abcdefghijkl"}

    def test_loads_rfc8259_strict_rejection(self):
        invalid_cases = [
            "",
            "   \n\t  ",
            "{",
            "[1, 2,]",
            '{"a": 1,}',
            '{"a" 1}',
            '{"a": 1} trailing',
            "01",
            "-01",
            ".5",
            "1.",
            "1e",
            "1e+",
            "NaN",
            "Infinity",
            "1e400",                 # IEEE-754 overflow must be rejected
            '"unclosed',
            '"raw\x01control"',      # Unescaped control char < 0x20
            '"raw\nnewline"',        # Unescaped newline inside string
            r'"\x41"',               # Invalid escape sequence
            r'"\uD800"',             # Lone high surrogate
            r'"\uDC00"',             # Lone low surrogate
            r'"\uD800\u0041"',       # High surrogate not followed by low surrogate
            b'"\xff\xfe"',           # Invalid UTF-8 byte sequence
        ]
        for bad in invalid_cases:
            with pytest.raises(ValueError):
                fastjson.loads(bad)

    def test_turbo_parser_reusable_and_depth_limit(self):
        parser = fastjson.TurboParser(max_depth=8)
        assert parser.max_depth == 8

        for i in range(25):
            doc = parser.loads(f'{{"iter": {i}, "nested": [[1, 2, 3]]}}')
            assert doc["iter"] == i
            assert doc["nested"] == [[1, 2, 3]]

        # Exceed configured max_depth=8
        too_deep = "[" * 10 + "1" + "]" * 10
        with pytest.raises(ValueError):
            parser.loads(too_deep)

    def test_loads_file_and_dumps_roundtrip(self):
        data = {
            "project": "FastestJSONInTheWest",
            "version": 2,
            "pi": 3.141592653589793,
            "flags": [True, False, None],
            "huge": (1 << 100) + 7,
            "unicode": "🚀 café 日本語",
        }
        compact = fastjson.dumps(data)
        assert fastjson.loads(compact) == data

        ascii_only = fastjson.dumps(data, ensure_ascii=True)
        assert all(ord(ch) < 128 for ch in ascii_only)
        assert fastjson.loads(ascii_only) == data

        pretty = fastjson.dumps(data, indent=2)
        assert "\n  " in pretty
        assert fastjson.loads(pretty) == data

        raw = fastjson.dumps_bytes(data)
        assert isinstance(raw, bytes)
        assert fastjson.loads(raw) == data

        with tempfile.NamedTemporaryFile(mode="wb", suffix=".json", delete=False) as tf:
            tf.write(raw)
            path = tf.name
        try:
            assert fastjson.loads_file(path) == data
            tp = fastjson.TurboParser()
            assert tp.loads_file(path) == data
        finally:
            os.remove(path)

    def test_dumps_nan_handling_and_key_validation(self):
        with pytest.raises(ValueError):
            fastjson.dumps({"bad": float("nan")})
        with pytest.raises(ValueError):
            fastjson.dumps({"bad": float("inf")})

        assert fastjson.dumps([float("nan"), float("inf"), float("-inf")], allow_nan=True) == "[NaN,Infinity,-Infinity]"

        with pytest.raises(TypeError):
            fastjson.dumps({123: "non-string-key"})


class TestPostgreSQLJsonbAdapter:
    """Tests for PostgreSQL binary-mode jsonb wire format (\x01 + JSON text)."""

    def test_wire_version_constant(self):
        assert fastjson.PG_JSONB_WIRE_VERSION == 1

    def test_pg_jsonb_roundtrip(self):
        payload = {"id": 42, "tags": ["simd", "postgres"], "active": True, "meta": None}
        wire = fastjson.dumps_pg_jsonb(payload)
        assert isinstance(wire, bytes)
        assert wire[0] == 1
        assert wire[1:] == fastjson.dumps_bytes(payload)

        # Native Python object path (TurboDOM)
        got = fastjson.loads_pg_jsonb(wire)
        assert got == payload
        assert type(got["id"]) is int

        # COW JSONValue path
        jv = fastjson.parse_pg_jsonb(wire)
        assert jv.is_object()
        assert jv["id"].as_int() == 42
        assert jv["tags"][0].as_string() == "simd"

        # Scalar jsonb roots
        assert fastjson.loads_pg_jsonb(b'\x01"hello"') == "hello"
        assert fastjson.loads_pg_jsonb(b"\x01null") is None
        assert fastjson.parse_pg_jsonb(b'\x01"hello"').as_string() == "hello"
        assert fastjson.parse_pg_jsonb(b"\x01null").is_null()

    def test_pg_jsonb_error_cases(self):
        # Empty wire (no version byte)
        for fn in (fastjson.parse_pg_jsonb, fastjson.loads_pg_jsonb):
            with pytest.raises(ValueError, match="empty wire"):
                fn(b"")

            # Version byte with empty JSON body
            with pytest.raises(ValueError):
                fn(b"\x01")

            # Unsupported version bytes (0, 2, or plain text '{' = 0x7b)
            for bad_wire in (b"\x00{}", b"\x02{}", b'{"a":1}'):
                with pytest.raises(ValueError, match="unsupported wire version"):
                    fn(bad_wire)

            # Malformed JSON after valid version byte
            with pytest.raises(ValueError):
                fn(b'\x01{"a":')

            # Passing str instead of binary wire bytes must raise TypeError
            with pytest.raises(TypeError):
                fn('\x01{"a":1}')


class TestTypedVectors:
    """Tests for Int64Vector, UInt64Vector, Float64Vector, and StringVector."""

    def test_int64_and_uint64_vectors(self):
        iv = fastjson.Int64Vector([-10, 0, 10, 25])
        iv.append(75)
        assert len(iv) == 5
        assert iv.sum() == 100
        assert iv[-1] == 75
        assert fastjson.dumps(iv) == "[-10,0,10,25,75]"

        uv = fastjson.UInt64Vector([1, (1 << 64) - 2])
        assert uv.sum() == (1 << 64) - 1
        assert uv.to_list() == [1, (1 << 64) - 2]

    def test_float64_and_string_vectors(self):
        fv = fastjson.Float64Vector([1.25, 2.75, 4.0])
        assert fv.sum() == 8.0
        assert fastjson.loads(fv.to_json()) == [1.25, 2.75, 4.0]

        sv = fastjson.StringVector(["hello", 'quote"here', "🚀"])
        assert len(sv) == 3
        assert fastjson.loads(sv.to_json()) == ["hello", 'quote"here', "🚀"]
        assert fastjson.loads(fastjson.dumps(sv)) == ["hello", 'quote"here', "🚀"]


class TestJSONValueCompleteAPI:
    """Tests for JSONValue constructors, factories, dict/list operations, and serialization."""

    def test_constructors_and_factories(self):
        obj = fastjson.JSONValue.object()
        assert obj.is_object() and obj.empty()
        obj["name"] = "Apollo"
        obj["crew"] = 3
        obj["ok"] = True
        obj["nested"] = {"stage": "lunar"}

        assert "name" in obj
        assert obj.contains("crew")
        assert "missing" not in obj
        assert obj.get("missing", "fallback") == "fallback"
        assert obj.get("name").as_string() == "Apollo"
        assert set(obj.keys()) == {"name", "crew", "ok", "nested"}
        assert len(obj.values()) == 4
        assert len(obj.items()) == 4

        del obj["ok"]
        assert "ok" not in obj

        arr = fastjson.JSONValue.array()
        arr.append(10)
        arr.append("two")
        arr.append({"three": 3})
        assert len(arr) == 3
        del arr[1]
        assert len(arr) == 2
        assert arr[1]["three"].as_int() == 3

        compact = obj.stringify()
        pretty = obj.prettify(2)
        assert fastjson.loads(compact) == fastjson.loads(pretty)
        assert fastjson.stringify(obj) == compact
        assert fastjson.prettify(obj, 2) == pretty

    def test_from_python_and_bytes_parse(self):
        jv = fastjson.JSONValue.from_python({"nums": [1, 2, 3], "flag": False})
        assert jv["nums"][2].as_int() == 3
        assert jv["flag"].as_bool() is False

        jv2 = fastjson.parse(b'{"from_bytes": 99}')
        assert jv2["from_bytes"].as_int() == 99


class TestJSONQueryCompleteLINQ:
    """Tests for lazy JSONQuery LINQ operations and early termination."""

    def test_lazy_pipeline_and_early_termination(self):
        evaluated = 0

        def pred(x):
            nonlocal evaluated
            evaluated += 1
            return x.as_int() % 2 == 0

        q = fastjson.query(list(range(1, 1000))).where(pred).take(3)
        assert q.is_lazy()
        assert evaluated == 0  # Deferred until consumed!

        res = q.to_list()
        assert res == [2.0, 4.0, 6.0]
        # Early termination: stopped right after finding the 3rd even number (item 6)!
        assert evaluated == 6

    def test_linq_ordering_grouping_scan_and_aggregations(self):
        records = fastjson.parse(
            '[{"dept": "eng", "score": 30}, '
            ' {"dept": "ops", "score": 10}, '
            ' {"dept": "eng", "score": 20}, '
            ' {"dept": "ops", "score": 10}]'
        )
        q = fastjson.query(records)

        assert q.count() == 4
        assert q.count(lambda x: x["dept"].as_string() == "eng") == 2
        assert q.any(lambda x: x["score"].as_int() == 30)
        assert q.all(lambda x: x["score"].as_int() >= 10)
        assert q.sum(lambda x: x["score"].as_number()) == 70.0
        assert q.min(lambda x: x["score"].as_number()) == 10.0
        assert q.max(lambda x: x["score"].as_number()) == 30.0
        assert q.average(lambda x: x["score"].as_number()) == 17.5

        asc_scores = (
            q.order_by(lambda x: x["score"].as_number())
            .select(lambda x: x["score"])
            .to_list()
        )
        assert asc_scores == [10.0, 10.0, 20.0, 30.0]

        desc_scores = (
            q.order_by_descending(lambda x: x["score"].as_number())
            .select(lambda x: x["score"])
            .to_list()
        )
        assert desc_scores == [30.0, 20.0, 10.0, 10.0]

        # distinct & distinct_by
        assert len(q.distinct().to_list()) == 3
        assert len(q.distinct_by(lambda x: x["dept"].as_string()).to_list()) == 2

        # group_by
        groups = q.group_by(lambda x: x["dept"].as_string())
        assert set(groups.keys()) == {"eng", "ops"}
        assert len(groups["eng"]) == 2
        assert len(groups["ops"]) == 2

        # prefix_sum / scan
        nums = fastjson.query([1, 2, 3, 4])
        assert nums.prefix_sum().to_list() == [1.0, 3.0, 6.0, 10.0]
        assert nums.scan(lambda a, b: a.as_number() * b.as_number()).to_list() == [1.0, 2.0, 6.0, 24.0]

        # skip / take_while / skip_while / reverse / first / last / find_index / reduce / zip / concat
        assert nums.skip(2).to_list() == [3.0, 4.0]
        assert nums.take_while(lambda x: x.as_number() < 3).to_list() == [1.0, 2.0]
        assert nums.skip_while(lambda x: x.as_number() < 3).to_list() == [3.0, 4.0]
        assert nums.reverse().to_list() == [4.0, 3.0, 2.0, 1.0]
        assert nums.first().as_int() == 1
        assert nums.last().as_int() == 4
        assert nums.find_index(lambda x: x.as_int() == 3) == 2
        assert nums.reduce(lambda acc, x: acc + x.as_int(), 0) == 10

        zipped = fastjson.query([1, 2]).zip(fastjson.query(["a", "b"])).to_list()
        assert zipped == [[1.0, "a"], [2.0, "b"]]

        unioned = fastjson.query([1, 2]).union_with(fastjson.query([2, 3])).to_list()
        assert unioned == [1.0, 2.0, 3.0]


class TestParallelMustacheAndGPU:
    """Tests for parse_parallel, Mustache dict overload, GPU graceful fallback, and multithreading."""

    def test_parse_parallel_and_multithreaded_loads(self):
        payload = json.dumps([{"i": i, "v": f"item_{i}"} for i in range(2000)])
        jv = fastjson.parse_parallel(payload, num_threads=4)
        assert len(jv) == 2000
        assert jv[1999]["i"].as_int() == 1999

        def worker(tid: int):
            doc = fastjson.loads(payload)
            return len(doc) + doc[tid]["i"]

        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            outs = list(pool.map(worker, range(32)))
        assert outs == [2000 + i for i in range(32)]

    def test_mustache_with_python_dict(self):
        rendered = fastjson.render_template(
            "Mission: {{mission}} ({{#astronauts}}{{.}} {{/astronauts}})",
            {"mission": "Artemis", "astronauts": ["Reid", "Victor", "Christina", "Jeremy"]},
        )
        assert rendered == "Mission: Artemis (Reid Victor Christina Jeremy )"

    def test_gpu_detection_and_graceful_behavior(self):
        backend = fastjson.detect_gpu_backend()
        assert isinstance(backend, fastjson.GPUBackend)
        avail = fastjson.is_gpu_available()
        assert isinstance(avail, bool)
        info = fastjson.get_gpu_info()
        assert isinstance(info, fastjson.GPUInfo)
        cfg = fastjson.GPUParseConfig()
        cfg.min_size_for_gpu = 64
        res = fastjson.parse_on_gpu('{"gpu": [1, 2, 3]}', cfg)
        assert isinstance(res, fastjson.GPUParseResult)

        if not avail:
            a = np.ones((2, 2), dtype=np.float32)
            b = np.ones((2, 2), dtype=np.float32)
            with pytest.raises(RuntimeError):
                fastjson.gpu_matrix_multiply(a, b)

    def test_simd_literal_bounds_regression(self):
        """Regression test for SSE2 literal matching on short (<16B) and padded (>=16B) inputs."""
        # Short (<16-byte) buffers exercising bounds-safe tail fallback
        assert fastjson.parse("true").as_bool() is True
        assert fastjson.parse("false").as_bool() is False
        assert fastjson.parse("null").is_null() is True
        assert fastjson.parse("[true,false,null]").to_python() == [True, False, None]

        # >=16-byte buffers exercising aligned 16-byte literal staging in match_literal_sse2
        padded = '{"a": true, "b": false, "c": null, "tail": "0123456789abcdef"}'
        parsed = fastjson.parse(padded).to_python()
        assert parsed == {"a": True, "b": False, "c": None, "tail": "0123456789abcdef"}

        for bad in ["truX", "falsX", "nulX", '{"a": truX, "pad": "0123456789abcdef"}']:
            with pytest.raises(RuntimeError):
                fastjson.parse(bad)
            with pytest.raises(ValueError):
                fastjson.loads(bad)

        # Regression: INT128_MIN exact parse + stringify without signed negation UB,
        # plus overflow wrap detection for magnitudes > INT128_MIN / UINT128_MAX
        int128_min = -(1 << 127)
        jv_min = fastjson.parse(str(int128_min))
        assert jv_min.is_int_128()
        assert jv_min.to_python() == int128_min
        assert jv_min.stringify() == str(int128_min)

        below_int128_min = "-200000000000000000000000000000000000000"
        jv_below = fastjson.parse(below_int128_min)
        assert jv_below.is_number()
        assert jv_below.to_python() < 0

        above_uint128_max = "400000000000000000000000000000000000000"
        jv_above = fastjson.parse(above_uint128_max)
        assert jv_above.is_number()
        assert jv_above.to_python() > 0
