"""
Comprehensive regression tests for fastjson_py nanobind bindings.

Tests parse correctness, serialization round-trip, error handling,
edge cases, file I/O, and API compatibility with stdlib json.

Run: pytest external/fastestjsoninthewest/tests/test_fastjson_nanobind.py -v
"""
import json
import math
import os
import sys
import tempfile

import pytest

# Add the build output directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'python_big_brother_analytics_bot'))

import fastjson_py


# ============================================================================
# Parse Correctness
# ============================================================================

class TestParseCorrectness:
    def test_parse_null(self):
        assert fastjson_py.loads("null") is None

    def test_parse_true(self):
        assert fastjson_py.loads("true") is True

    def test_parse_false(self):
        assert fastjson_py.loads("false") is False

    def test_parse_integer(self):
        assert fastjson_py.loads("42") == 42

    def test_parse_negative_integer(self):
        assert fastjson_py.loads("-17") == -17

    def test_parse_zero(self):
        assert fastjson_py.loads("0") == 0

    def test_parse_float(self):
        result = fastjson_py.loads("3.14")
        assert abs(result - 3.14) < 1e-10

    def test_parse_negative_float(self):
        result = fastjson_py.loads("-2.718")
        assert abs(result - (-2.718)) < 1e-10

    def test_parse_scientific_notation(self):
        result = fastjson_py.loads("1.5e10")
        assert abs(result - 1.5e10) < 1e3

    def test_parse_string(self):
        assert fastjson_py.loads('"hello"') == "hello"

    def test_parse_empty_string(self):
        assert fastjson_py.loads('""') == ""

    def test_parse_unicode_string(self):
        assert fastjson_py.loads(r'"caf\u00e9"') == "caf\u00e9"

    def test_parse_escape_sequences(self):
        result = fastjson_py.loads(r'"line1\nline2\ttab"')
        assert result == "line1\nline2\ttab"

    def test_parse_escaped_quotes(self):
        result = fastjson_py.loads(r'"say \"hello\""')
        assert result == 'say "hello"'

    def test_parse_backslash(self):
        result = fastjson_py.loads(r'"path\\to\\file"')
        assert result == "path\\to\\file"

    def test_parse_empty_array(self):
        assert fastjson_py.loads("[]") == []

    def test_parse_array_of_ints(self):
        assert fastjson_py.loads("[1, 2, 3]") == [1, 2, 3]

    def test_parse_array_of_mixed(self):
        result = fastjson_py.loads('[1, "two", true, null, 3.14]')
        assert result[0] == 1
        assert result[1] == "two"
        assert result[2] is True
        assert result[3] is None
        assert abs(result[4] - 3.14) < 1e-10

    def test_parse_nested_array(self):
        assert fastjson_py.loads("[[1, 2], [3, 4]]") == [[1, 2], [3, 4]]

    def test_parse_empty_object(self):
        assert fastjson_py.loads("{}") == {}

    def test_parse_simple_object(self):
        result = fastjson_py.loads('{"key": "value"}')
        assert result == {"key": "value"}

    def test_parse_object_with_types(self):
        result = fastjson_py.loads('{"a": 1, "b": "two", "c": true, "d": null}')
        assert result["a"] == 1
        assert result["b"] == "two"
        assert result["c"] is True
        assert result["d"] is None

    def test_parse_nested_object(self):
        result = fastjson_py.loads('{"outer": {"inner": 42}}')
        assert result["outer"]["inner"] == 42

    def test_parse_complex_nested(self):
        data = '{"users": [{"name": "Alice", "scores": [95, 87]}, {"name": "Bob", "scores": [72, 88]}]}'
        result = fastjson_py.loads(data)
        assert len(result["users"]) == 2
        assert result["users"][0]["name"] == "Alice"
        assert result["users"][0]["scores"] == [95, 87]
        assert result["users"][1]["name"] == "Bob"


# ============================================================================
# Serialization
# ============================================================================

class TestSerialization:
    def test_dumps_none(self):
        assert fastjson_py.dumps(None) == "null"

    def test_dumps_true(self):
        assert fastjson_py.dumps(True) == "true"

    def test_dumps_false(self):
        assert fastjson_py.dumps(False) == "false"

    def test_dumps_integer(self):
        assert fastjson_py.dumps(42) == "42"

    def test_dumps_negative(self):
        assert fastjson_py.dumps(-17) == "-17"

    def test_dumps_float(self):
        result = fastjson_py.dumps(3.14)
        assert abs(float(result) - 3.14) < 1e-10

    def test_dumps_string(self):
        result = fastjson_py.dumps("hello")
        assert result == '"hello"'

    def test_dumps_empty_list(self):
        assert fastjson_py.dumps([]) == "[]"

    def test_dumps_list(self):
        result = fastjson_py.dumps([1, 2, 3])
        assert json.loads(result) == [1, 2, 3]

    def test_dumps_empty_dict(self):
        assert fastjson_py.dumps({}) == "{}"

    def test_dumps_dict(self):
        result = fastjson_py.dumps({"key": "value"})
        assert json.loads(result) == {"key": "value"}

    def test_dumps_pretty(self):
        result = fastjson_py.dumps({"a": 1}, 2)
        assert "\n" in result
        assert "  " in result

    def test_dumps_tuple(self):
        result = fastjson_py.dumps((1, 2, 3))
        assert json.loads(result) == [1, 2, 3]


# ============================================================================
# Round-Trip (loads -> dumps -> loads)
# ============================================================================

class TestRoundTrip:
    @pytest.mark.parametrize("value", [
        None,
        True,
        False,
        42,
        -17,
        "hello world",
        "",
        [],
        [1, 2, 3],
        {},
        {"key": "value"},
    ])
    def test_round_trip_simple(self, value):
        result = fastjson_py.loads(fastjson_py.dumps(value))
        assert result == value

    def test_round_trip_float(self):
        result = fastjson_py.loads(fastjson_py.dumps(3.14))
        assert abs(result - 3.14) < 1e-10

    def test_round_trip_nested(self):
        original = {
            "users": [
                {"name": "Alice", "age": 30, "active": True},
                {"name": "Bob", "age": 25, "active": False},
            ],
            "count": 2,
            "metadata": None,
        }
        result = fastjson_py.loads(fastjson_py.dumps(original))
        assert result == original

    def test_round_trip_trading_payload(self):
        """Round-trip a realistic OHLCV trading payload."""
        original = {
            "symbol": "QQQ",
            "date": "2026-02-21",
            "open": 530.12,
            "high": 532.45,
            "low": 528.90,
            "close": 531.78,
            "volume": 45230100,
        }
        result = fastjson_py.loads(fastjson_py.dumps(original))
        assert result["symbol"] == "QQQ"
        assert result["volume"] == 45230100
        assert abs(result["close"] - 531.78) < 0.01

    def test_round_trip_large_array(self):
        original = list(range(1000))
        result = fastjson_py.loads(fastjson_py.dumps(original))
        assert result == original


# ============================================================================
# Error Handling
# ============================================================================

class TestErrorHandling:
    def test_invalid_json(self):
        with pytest.raises(ValueError):
            fastjson_py.loads("{invalid")

    def test_empty_string(self):
        with pytest.raises(ValueError):
            fastjson_py.loads("")

    def test_truncated_array(self):
        with pytest.raises(ValueError):
            fastjson_py.loads("[1, 2,")

    def test_truncated_object(self):
        with pytest.raises(ValueError):
            fastjson_py.loads('{"key":')

    def test_trailing_comma_array(self):
        with pytest.raises(ValueError):
            fastjson_py.loads("[1, 2, 3,]")

    def test_single_quote_string(self):
        with pytest.raises(ValueError):
            fastjson_py.loads("{'key': 'value'}")


# ============================================================================
# Edge Cases
# ============================================================================

class TestEdgeCases:
    def test_large_integer(self):
        result = fastjson_py.loads("9007199254740992")
        assert result == 9007199254740992

    def test_large_negative_integer(self):
        result = fastjson_py.loads("-9007199254740992")
        assert result == -9007199254740992

    def test_very_small_float(self):
        result = fastjson_py.loads("1e-300")
        assert result == pytest.approx(1e-300, rel=1e-5)

    def test_deep_nesting(self):
        """Test deeply nested structures (50 levels)."""
        depth = 50
        nested = "[" * depth + "1" + "]" * depth
        result = fastjson_py.loads(nested)
        # Unwrap levels
        current = result
        for _ in range(depth):
            assert isinstance(current, list)
            current = current[0]
        assert current == 1

    def test_empty_object_in_array(self):
        result = fastjson_py.loads("[{}, {}, {}]")
        assert result == [{}, {}, {}]

    def test_empty_array_in_object(self):
        result = fastjson_py.loads('{"data": []}')
        assert result == {"data": []}

    def test_whitespace_handling(self):
        result = fastjson_py.loads('  {  "key"  :  "value"  }  ')
        assert result == {"key": "value"}

    def test_newline_in_json(self):
        result = fastjson_py.loads('{\n  "key": "value"\n}')
        assert result == {"key": "value"}


# ============================================================================
# File I/O
# ============================================================================

class TestFileIO:
    def test_dump_and_load(self):
        original = {"symbol": "TQQQ", "signal": "BULLISH", "confidence": 0.93}
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            path = f.name
        try:
            fastjson_py.dump(original, path)
            result = fastjson_py.load(path)
            assert result["symbol"] == "TQQQ"
            assert result["signal"] == "BULLISH"
            assert abs(result["confidence"] - 0.93) < 0.01
        finally:
            os.unlink(path)

    def test_dump_pretty(self):
        original = {"a": 1, "b": [2, 3]}
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            path = f.name
        try:
            fastjson_py.dump(original, path, indent=2)
            with open(path) as f:
                content = f.read()
            assert "\n" in content
            result = fastjson_py.load(path)
            assert result["a"] == 1
            assert result["b"] == [2, 3]
        finally:
            os.unlink(path)

    def test_load_nonexistent_file(self):
        with pytest.raises(ValueError, match="Cannot open"):
            fastjson_py.load("/nonexistent/path.json")


# ============================================================================
# API Compatibility with stdlib json
# ============================================================================

class TestAPICompatibility:
    """Verify fastjson_py produces same results as stdlib json."""

    @pytest.mark.parametrize("json_str", [
        "null",
        "true",
        "false",
        "42",
        "-17",
        '"hello"',
        "[]",
        "[1, 2, 3]",
        "{}",
        '{"key": "value"}',
        '{"a": [1, 2], "b": {"c": true}}',
    ])
    def test_loads_matches_stdlib(self, json_str):
        stdlib_result = json.loads(json_str)
        fastjson_result = fastjson_py.loads(json_str)
        assert fastjson_result == stdlib_result

    def test_dumps_parseable_by_stdlib(self):
        """Ensure fastjson dumps output is valid for stdlib json.loads."""
        data = {"users": [{"name": "Alice"}, {"name": "Bob"}], "count": 2}
        fastjson_str = fastjson_py.dumps(data)
        stdlib_result = json.loads(fastjson_str)
        assert stdlib_result == data

    def test_stdlib_dumps_parseable_by_fastjson(self):
        """Ensure stdlib json dumps output is valid for fastjson.loads."""
        data = {"users": [{"name": "Alice"}, {"name": "Bob"}], "count": 2}
        stdlib_str = json.dumps(data)
        fastjson_result = fastjson_py.loads(stdlib_str)
        assert fastjson_result == data


# ============================================================================
# Prettify
# ============================================================================

class TestPrettify:
    def test_prettify_basic(self):
        result = fastjson_py.prettify('{"a":1,"b":2}')
        assert "\n" in result
        assert "a" in result
        assert "b" in result

    def test_prettify_custom_indent(self):
        result = fastjson_py.prettify('{"a":1}', indent=2)
        assert "  " in result


# ============================================================================
# SIMD Info
# ============================================================================

class TestSimdInfo:
    def test_simd_info_returns_string(self):
        info = fastjson_py.simd_info()
        assert isinstance(info, str)
        assert len(info) > 0
        assert "FastJSON" in info


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
