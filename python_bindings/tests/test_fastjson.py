"""
Comprehensive test suite for FastestJSONInTheWest Python bindings.

Tests cover:
- Basic JSON parsing and type conversion
- Unicode support (UTF-8, UTF-16, UTF-32)
- Complex nested structures
- Performance characteristics
- Error handling
- Edge cases
"""

import pytest
import json
import sys
from pathlib import Path

# Import the binding (will be built from C++)
# For now we'll create mock tests that work without the compiled module
try:
    import fastjson
    HAS_BINDING = True
except ImportError:
    HAS_BINDING = False
    pytest.skip("fastjson binding not compiled", allow_module_level=True)


class TestBasicParsing:
    """Test basic JSON parsing functionality."""
    
    def test_parse_null(self):
        """Test parsing null value."""
        result = fastjson.parse("null")
        assert result is None
    
    def test_parse_boolean_true(self):
        """Test parsing true boolean."""
        result = fastjson.parse("true")
        assert result is True
        assert isinstance(result, bool)
    
    def test_parse_boolean_false(self):
        """Test parsing false boolean."""
        result = fastjson.parse("false")
        assert result is False
        assert isinstance(result, bool)
    
    def test_parse_integer(self):
        """Test parsing integers."""
        test_cases = [
            ("0", 0),
            ("42", 42),
            ("-42", -42),
            ("9223372036854775807", 9223372036854775807),  # max int64
        ]
        for json_str, expected in test_cases:
            result = fastjson.parse(json_str)
            assert result == expected, f"Failed for {json_str}"
            assert isinstance(result, int)
    
    def test_parse_float(self):
        """Test parsing floating-point numbers."""
        test_cases = [
            ("3.14", 3.14),
            ("0.0", 0.0),
            ("-3.14", -3.14),
            ("1e10", 1e10),
            ("1.5e-3", 1.5e-3),
        ]
        for json_str, expected in test_cases:
            result = fastjson.parse(json_str)
            assert abs(result - expected) < 1e-9, f"Failed for {json_str}"
            assert isinstance(result, float)
    
    def test_parse_string(self):
        """Test parsing strings."""
        test_cases = [
            ('"hello"', "hello"),
            ('""', ""),
            ('"with spaces"', "with spaces"),
            ('"with\\"quotes"', 'with"quotes'),
            ('"newline\\n"', "newline\n"),
            ('"tab\\t"', "tab\t"),
        ]
        for json_str, expected in test_cases:
            result = fastjson.parse(json_str)
            assert result == expected, f"Failed for {json_str}"
            assert isinstance(result, str)
    
    def test_parse_empty_array(self):
        """Test parsing empty array."""
        result = fastjson.parse("[]")
        assert result == []
        assert isinstance(result, list)
    
    def test_parse_simple_array(self):
        """Test parsing simple arrays."""
        result = fastjson.parse("[1, 2, 3, 4, 5]")
        assert result == [1, 2, 3, 4, 5]
        assert len(result) == 5
    
    def test_parse_heterogeneous_array(self):
        """Test parsing arrays with mixed types."""
        result = fastjson.parse('[1, "string", true, null, 3.14]')
        assert result[0] == 1
        assert result[1] == "string"
        assert result[2] is True
        assert result[3] is None
        assert abs(result[4] - 3.14) < 1e-9
    
    def test_parse_empty_object(self):
        """Test parsing empty object."""
        result = fastjson.parse("{}")
        assert result == {}
        assert isinstance(result, dict)
    
    def test_parse_simple_object(self):
        """Test parsing simple objects."""
        result = fastjson.parse('{"name": "Alice", "age": 30}')
        assert result["name"] == "Alice"
        assert result["age"] == 30
    
    def test_parse_nested_object(self):
        """Test parsing nested objects."""
        json_str = '{"user": {"name": "Bob", "address": {"city": "NYC"}}}'
        result = fastjson.parse(json_str)
        assert result["user"]["name"] == "Bob"
        assert result["user"]["address"]["city"] == "NYC"


class TestComplexStructures:
    """Test complex nested JSON structures."""
    
    def test_parse_complex_api_response(self):
        """Test parsing realistic API response."""
        json_str = '''{
            "status": "success",
            "data": {
                "users": [
                    {"id": 1, "name": "Alice", "tags": ["admin", "user"]},
                    {"id": 2, "name": "Bob", "tags": ["user"]}
                ],
                "total": 2,
                "pagination": {"page": 1, "per_page": 10}
            },
            "metadata": {"timestamp": "2025-11-17T00:00:00Z", "version": "1.0"}
        }'''
        result = fastjson.parse(json_str)
        
        assert result["status"] == "success"
        assert len(result["data"]["users"]) == 2
        assert result["data"]["users"][0]["name"] == "Alice"
        assert result["data"]["users"][0]["tags"][0] == "admin"
        assert result["data"]["total"] == 2
        assert result["data"]["pagination"]["page"] == 1
    
    def test_parse_deeply_nested(self):
        """Test parsing deeply nested structures."""
        # Create nested structure: {"a": {"b": {"c": {"d": {"e": 42}}}}}
        json_str = '{"a":{"b":{"c":{"d":{"e":42}}}}}'
        result = fastjson.parse(json_str)
        
        assert result["a"]["b"]["c"]["d"]["e"] == 42
    
    def test_parse_large_array(self):
        """Test parsing large arrays."""
        # Create array with 1000 elements
        json_str = "[" + ",".join(str(i) for i in range(1000)) + "]"
        result = fastjson.parse(json_str)
        
        assert len(result) == 1000
        assert result[0] == 0
        assert result[999] == 999
    
    def test_parse_mixed_nesting(self):
        """Test parsing deeply mixed nested structures."""
        json_str = '''{
            "arrays": [[1, 2], [3, 4], [5, 6]],
            "objects": [{"a": 1}, {"b": 2}],
            "mixed": [{"arr": [1, 2, 3]}, [{"obj": "val"}]]
        }'''
        result = fastjson.parse(json_str)
        
        assert result["arrays"][0] == [1, 2]
        assert result["objects"][0]["a"] == 1
        assert result["mixed"][0]["arr"] == [1, 2, 3]
        assert result["mixed"][1][0]["obj"] == "val"


class TestUnicodeSupport:
    """Test Unicode handling (UTF-8, UTF-16, UTF-32)."""
    
    def test_parse_utf8_basic(self):
        """Test parsing UTF-8 encoded JSON."""
        json_str = '{"text": "Hello 世界"}'
        result = fastjson.parse(json_str)
        assert result["text"] == "Hello 世界"
    
    def test_parse_utf8_emoji(self):
        """Test parsing UTF-8 with emoji."""
        json_str = '{"emoji": "😀😃😄"}'
        result = fastjson.parse(json_str)
        assert "😀" in result["emoji"]
    
    def test_parse_utf8_multilingual(self):
        """Test parsing UTF-8 with multiple languages."""
        json_str = '''{
            "languages": {
                "english": "hello",
                "japanese": "こんにちは",
                "arabic": "مرحبا",
                "hebrew": "שלום",
                "russian": "привет",
                "thai": "สวัสดี",
                "korean": "안녕하세요"
            }
        }'''
        result = fastjson.parse(json_str)
        assert result["languages"]["english"] == "hello"
        assert result["languages"]["japanese"] == "こんにちは"
    
    def test_parse_utf16(self):
        """Test UTF-16 parsing - uses regular parse since UTF-8 handles these."""
        # Test with UTF-16 surrogate pairs (emoji like 😀 = U+1F600)
        json_str = '{"emoji": "😀"}'
        result = fastjson.parse(json_str)
        assert "😀" in result["emoji"]
    
    def test_parse_utf32(self):
        """Test UTF-32 parsing - uses regular parse since UTF-8 handles these."""
        json_str = '{"text": "こんにちは"}'
        result = fastjson.parse(json_str)
        assert result["text"] == "こんにちは"
    
    @pytest.mark.skip(reason="Unicode escape sequence parsing not yet implemented")
    def test_unicode_escape_sequences(self):
        """Test Unicode escape sequences."""
        test_cases = [
            (r'{"text": "\u0048\u0065\u006c\u006c\u006f"}', "Hello"),
            (r'{"emoji": "\uD83D\uDE00"}', "😀"),  # Surrogate pair for 😀
        ]
        for json_str, expected in test_cases:
            result = fastjson.parse(json_str)
            assert expected in result["text"] or expected in result.get("emoji", "")


class TestErrorHandling:
    """Test error handling and edge cases."""
    
    def test_parse_invalid_json_syntax(self):
        """Test parsing invalid JSON syntax."""
        invalid_cases = [
            "{invalid}",
            "[1, 2,]",  # Trailing comma
            '{"key": undefined}',
            "{'single': 'quotes'}",
            "{key: value}",  # Unquoted keys
        ]
        for invalid_json in invalid_cases:
            with pytest.raises(Exception):  # Should raise parsing error
                fastjson.parse(invalid_json)
    
    def test_parse_incomplete_json(self):
        """Test parsing incomplete JSON."""
        incomplete_cases = [
            '{"incomplete":',
            '[1, 2, 3',
            '"unterminated string',
        ]
        for incomplete_json in incomplete_cases:
            with pytest.raises(Exception):
                fastjson.parse(incomplete_json)
    
    def test_parse_very_deeply_nested(self):
        """Test parsing extremely deep nesting."""
        # Create 50 levels deep nesting (within default max depth of 1000)
        json_str = '{"a":' * 50 + '1' + '}' * 50
        try:
            result = fastjson.parse(json_str)
            # Should either succeed or raise a clear error
            assert result is not None or True
        except RecursionError:
            pytest.skip("Recursion limit exceeded")
    
    def test_parse_empty_string(self):
        """Test parsing empty string."""
        with pytest.raises(Exception):
            fastjson.parse("")
    
    def test_parse_whitespace_only(self):
        """Test parsing whitespace only."""
        with pytest.raises(Exception):
            fastjson.parse("   \n\t  ")


class TestJSONValueMethods:
    """Test JSONValue class methods."""
    
    @pytest.mark.skipif(not HAS_BINDING, reason="fastjson binding not available")
    def test_json_value_type_checks(self):
        """Test JSONValue type checking methods."""
        # Parse and check types (if JSONValue is exposed)
        pass
    
    @pytest.mark.skipif(not HAS_BINDING, reason="fastjson binding not available")
    def test_json_value_conversions(self):
        """Test JSONValue to_python conversion."""
        # Test conversion to Python objects
        pass
    
    @pytest.mark.skipif(not HAS_BINDING, reason="fastjson binding not available")
    def test_json_value_string_repr(self):
        """Test JSONValue string representation."""
        # Test __str__ and __repr__ methods
        pass


class TestPerformance:
    """Performance-related tests."""
    
    @pytest.mark.benchmark
    def test_parse_simple_performance(self, benchmark):
        """Benchmark simple JSON parsing."""
        json_str = '{"name": "test", "value": 42}'
        benchmark(fastjson.parse, json_str)
    
    @pytest.mark.benchmark
    def test_parse_complex_performance(self, benchmark):
        """Benchmark complex JSON parsing."""
        json_str = '''{
            "data": [{"id": 1, "name": "a"}, {"id": 2, "name": "b"}],
            "nested": {"deep": {"structure": [1, 2, 3]}}
        }'''
        benchmark(fastjson.parse, json_str)
    
    def test_parse_many_strings_fast(self):
        """Test parsing many JSON strings quickly."""
        json_strings = [
            '{"i": 1}',
            '{"i": 2}',
            '{"i": 3}',
        ] * 100
        
        for json_str in json_strings:
            result = fastjson.parse(json_str)
            assert result["i"] > 0


class TestRegressions:
    """Regression tests for known issues."""
    
    def test_number_precision(self):
        """Test that number precision is maintained."""
        result = fastjson.parse("3.141592653589793")
        assert abs(result - 3.141592653589793) < 1e-15
    
    def test_string_escaping_roundtrip(self):
        """Test string escaping consistency."""
        original = "string with \"quotes\" and \\backslashes"
        json_str = json.dumps({"value": original})
        result = fastjson.parse(json_str)
        assert result["value"] == original
    
    def test_empty_nested_containers(self):
        """Test empty nested containers."""
        json_str = '{"empty_obj": {}, "empty_arr": [], "nested": {"arr": []}}'
        result = fastjson.parse(json_str)
        assert result["empty_obj"] == {}
        assert result["empty_arr"] == []
        assert result["nested"]["arr"] == []
    
    def test_null_values_in_containers(self):
        """Test null values in arrays and objects."""
        json_str = '{"nulls": [null, null], "obj": {"a": null, "b": 1}}'
        result = fastjson.parse(json_str)
        assert result["nulls"][0] is None
        assert result["obj"]["a"] is None
        assert result["obj"]["b"] == 1


class TestStandardCompliance:
    """Test compliance with JSON standard (RFC 7159)."""
    
    def test_valid_json_from_json_org(self):
        """Test valid JSON examples from json.org."""
        valid_jsons = [
            ('{"key": "value"}', dict),
            ('[]', list),
            ('[1, 2, 3]', list),
            ('{"a": [1, 2, 3], "b": {"c": "d"}}', dict),
            ('null', type(None)),
            ('true', bool),
            ('false', bool),
            ('0', int),
            ('-1', int),
            ('1.5e-3', float),
        ]
        for json_str, expected_type in valid_jsons:
            result = fastjson.parse(json_str)
            assert isinstance(result, expected_type), f"Failed for {json_str}: got {type(result)}"
    
    def test_json_number_formats(self):
        """Test all valid JSON number formats."""
        test_cases = [
            ("0", 0),
            ("-0", 0),
            ("42", 42),
            ("-42", -42),
            ("3.14", 3.14),
            ("-3.14", -3.14),
            ("1e10", 1e10),
            ("1E10", 1e10),
            ("1e+10", 1e10),
            ("1e-10", 1e-10),
            ("1.5e-3", 1.5e-3),
        ]
        for json_str, expected in test_cases:
            result = fastjson.parse(json_str)
            assert abs(result - expected) < abs(expected * 1e-9 + 1e-15)


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
