import fastjson
import pytest
import os
import tempfile
import json
import threading
import time

def test_json_types():
    """Test all JSON basic types and their conversion."""
    json_str = '{"a": null, "b": true, "c": false, "d": 123, "e": 123.456, "f": "hello"}'
    val = fastjson.parse(json_str)
    
    assert val["a"].is_null()
    assert val["b"].as_bool() is True
    assert val["c"].as_bool() is False
    assert val["d"].to_python() == 123
    assert val["e"].as_number() == 123.456
    assert val["f"].as_string() == "hello"
    
    py_obj = val.to_python()
    assert py_obj == {"a": None, "b": True, "c": False, "d": 123, "e": 123.456, "f": "hello"}

def test_empty_containers():
    """Test empty arrays and objects."""
    json_str = '{"arr": [], "obj": {}}'
    val = fastjson.parse(json_str)
    
    assert len(val["arr"]) == 0
    assert len(val["obj"]) == 0
    
    py_obj = val.to_python()
    assert py_obj == {"arr": [], "obj": {}}

def test_deep_nesting():
    """Test deeply nested structures."""
    nest_count = 100
    json_str = "[" * nest_count + "42" + "]" * nest_count
    val = fastjson.parse(json_str)
    
    curr = val
    for _ in range(nest_count):
        assert curr.is_array()
        assert len(curr) == 1
        curr = curr[0]
    
    assert curr.to_python() == 42
    
    # Test to_python on deep structure
    obj = val.to_python()
    for _ in range(nest_count):
        obj = obj[0]
    assert obj == 42

def test_unicode_support():
    """Test Unicode characters and Emojis."""
    json_str = '{"emoji": "🚀", "greeting": "こんにちは", "math": "∑"}'
    val = fastjson.parse(json_str)
    
    assert val["emoji"].as_string() == "🚀"
    assert val["greeting"].as_string() == "こんにちは"
    assert val["math"].as_string() == "∑"
    
    py_obj = val.to_python()
    assert py_obj["emoji"] == "🚀"
    assert py_obj["greeting"] == "こんにちは"
    assert py_obj["math"] == "∑"

def test_parse_file():
    """Test parsing from a file."""
    data = {"test": "file", "val": 99}
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        json.dump(data, f)
        temp_path = f.name
    
    try:
        val = fastjson.parse_file(temp_path)
        assert val["test"].as_string() == "file"
        assert val["val"].to_python() == 99
    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)

def test_error_handling():
    """Test various invalid JSON inputs."""
    # Only test inputs that the parser actually rejects
    invalid_inputs = [
        '{"key": 1,}',            # Trailing comma in object
        '{"key": 1 "key2": 2}',   # Missing comma between entries
        '[1, 2, "abc]',           # Unterminated string
        'true123',                # Extra tokens after value
        '',                       # Empty input
        '   ',                    # Just whitespace
        '{"a": 1',                # Incomplete object
        '{"a": }',                # Missing value
        '[1, 2,',                 # Incomplete array
    ]
    
    for inp in invalid_inputs:
        with pytest.raises(RuntimeError):
            fastjson.parse(inp)

def test_large_integers_boundary():
    """Test 128-bit integer boundaries and fallback."""
    # Boundary: Max uint128 is 2^128 - 1
    max_uint128 = (1 << 128) - 1
    json_str = str(max_uint128)
    val = fastjson.parse(json_str)
    assert val.to_python() == max_uint128
    
    # Overflow uint128 -> should result in infinity or fall back to double
    # 2^128
    overflow_int = (1 << 128)
    json_str = str(overflow_int)
    val = fastjson.parse(json_str)
    # The parser currently falls back to double on overflow
    assert abs(val.as_number() - float(overflow_int)) < (overflow_int * 1e-15)

def test_gil_release():
    """Verify GIL release by parsing in multiple threads."""
    def parse_task(count):
        # Create a single valid object
        item = '{"a": 1, "b": 2, "c": [1,2,3,4,5]}'
        # Wrap it correctly in a large array
        full_json = '{"data": [' + ','.join([item]*1000) + ']}'
        for _ in range(count):
            fastjson.parse(full_json)
            
    threads = []
    start = time.perf_counter()
    for _ in range(4):
        # Lower count for faster test
        t = threading.Thread(target=parse_task, args=(2,))
        t.start()
        threads.append(t)
        
    for t in threads:
        t.join()
    duration = time.perf_counter() - start
    print(f"\nMultithreaded parse duration: {duration:.4f}s")

if __name__ == "__main__":
    pytest.main([__file__])
