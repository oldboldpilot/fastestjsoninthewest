import fastjson
import pytest
import numpy as np
import json
import time

def test_basic_integers():
    """Test zero, 1, -1 and small integers.
    
    NOTE: JSON numbers without decimals are stored as double internally.
    Only 128-bit integers (exceeding 64-bit range) are converted to Python int.
    """
    for i in [0, 1, -1, 42, -42, 1000, -1000]:
        json_str = str(i)
        val = fastjson.parse(json_str)
        py_val = val.to_python()
        assert py_val == i, f"Failed for {i}"
        # JSON numbers are stored as float, but value equality still works
        assert isinstance(py_val, (int, float)), f"Expected int or float for {i}, got {type(py_val)}"

def test_int64_boundaries():
    """Test boundaries of 64-bit integers.
    
    Numbers within 64-bit range are stored as double (may lose precision).
    Numbers exceeding 64-bit range are promoted to 128-bit int.
    """
    int64_max = 9223372036854775807
    int64_min = -9223372036854775808
    
    # Numbers just outside 64-bit range get promoted to 128-bit int
    for i in [int64_max + 1, int64_min - 1]:
        json_str = str(i)
        val = fastjson.parse(json_str)
        py_val = val.to_python()
        assert py_val == i, f"Failed for {i}"
        assert isinstance(py_val, int), f"Expected int for {i}, got {type(py_val)}"

def test_int128_boundaries():
    """Test boundaries of 128-bit integers."""
    int128_max = (1 << 127) - 1
    int128_min = -(1 << 127)
    uint128_max = (1 << 128) - 1
    
    test_cases = [
        int128_max,
        int128_min,
        uint128_max,
        int128_max - 1,
        int128_min + 1,
        uint128_max - 1
    ]
    
    for i in test_cases:
        json_str = str(i)
        val = fastjson.parse(json_str)
        py_val = val.to_python()
        assert py_val == i, f"Failed for {i}"
        assert isinstance(py_val, int), f"Expected int for {i}, got {type(py_val)}"

def test_overflow_128bit():
    """Test numbers that overflow 128-bit (should fall back to float)."""
    # 1 << 128 is the first number that doesn't fit in uint128
    huge_int = 1 << 128
    json_str = str(huge_int)
    val = fastjson.parse(json_str)
    py_val = val.to_python()
    
    # It should be a float now (or a very large int if we supported arbitrary precision, 
    # but the parser falls back to double)
    assert isinstance(py_val, (float, int))
    if isinstance(py_val, float):
        # Allow some precision loss for extremely large numbers falling back to double
        assert abs(py_val - float(huge_int)) / float(huge_int) < 1e-15
    else:
        # If it's still an int, it means it didn't overflow or we handle > 128 bit
        assert py_val == huge_int

def test_mixed_types_in_containers():
    """Test integers in arrays and objects."""
    data = {
        "int128": 170141183460469231731687303715884105727,
        "neg_int128": -170141183460469231731687303715884105728,
        "array": [1, 1 << 64, 1 << 120, 3.14],
        "nested": {
            "val": 123456789012345678901234567890
        }
    }
    json_str = json.dumps(data)
    val = fastjson.parse(json_str)
    py_val = val.to_python()
    
    assert py_val["int128"] == data["int128"]
    assert py_val["neg_int128"] == data["neg_int128"]
    # array[1] is 2^64 - promoted to 128-bit
    assert py_val["array"][1] == data["array"][1]
    assert py_val["array"][2] == data["array"][2]
    assert py_val["nested"]["val"] == data["nested"]["val"]
    # Small int (1) stored as float
    assert isinstance(py_val["array"][0], (int, float))
    assert isinstance(py_val["array"][3], float)

def test_parallel_mixed_parsing():
    """Test parallel parsing of large arrays containing mixed ints and floats."""
    size = 100000
    large_array = []
    for i in range(size):
        if i % 3 == 0:
            large_array.append(i)
        elif i % 3 == 1:
            large_array.append(float(i) + 0.5)
        else:
            large_array.append(1 << (64 + (i % 60)))
            
    json_str = json.dumps(large_array)
    
    # Parse and convert in parallel
    start = time.perf_counter()
    val = fastjson.parse(json_str)
    py_obj = val.to_python(threads=8)
    end = time.perf_counter()
    
    print(f"\nParallel parse/convert of {size} items: {end - start:.4f}s")
    
    assert len(py_obj) == size
    for i in range(size):
        assert py_obj[i] == large_array[i]
        # 128-bit ints (i % 3 == 2) are proper Python ints
        # Small ints (i % 3 == 0) may be floats
        # Floats (i % 3 == 1) are floats
        if i % 3 == 2:
            assert isinstance(py_obj[i], int), f"128-bit int at {i} should be int, got {type(py_obj[i])}"
        elif i % 3 == 1:
            assert isinstance(py_obj[i], float)

if __name__ == "__main__":
    pytest.main([__file__])
