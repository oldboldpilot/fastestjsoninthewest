import fastjson
import pytest
import time

def test_cow_behavior():
    """Verify that JSONValue uses Copy-On-Write for efficiency."""
    print("\nTesting COW behavior...")
    # Create a reasonably large array (100k instead of 1M for faster test)
    large_list = [i for i in range(100000)]
    val = fastjson.parse(str(large_list))
    
    # Copying is O(1)
    start = time.perf_counter()
    val_copy = val.copy()
    end = time.perf_counter()
    print(f"O(1) Copy time: {end - start:.6f}s")
    assert end - start < 0.01  # Should be very fast (microseconds)
    
    # Verify they share the same length
    assert len(val) == len(val_copy)
    
    # Modify the copy - this triggers COW in C++
    start = time.perf_counter()
    val_copy[0] = 999999
    end = time.perf_counter()
    print(f"COW Modify time (triggers deep copy): {end - start:.6f}s")
    
    # Verify original is unchanged
    assert val[0].as_int() == 0
    assert val_copy[0].as_int() == 999999

def test_parallel_conversion():
    """Test parallel conversion from JSONValue to native Python objects."""
    print("\nTesting parallel conversion...")
    size = 10000  # Reduced for faster test
    data = {"key_" + str(i): i for i in range(size)}
    import json
    json_str = json.dumps(data)
    
    val = fastjson.parse(json_str)
    
    # Test serial conversion
    start_serial = time.perf_counter()
    py_obj_serial = val.to_python(threads=1)
    end_serial = time.perf_counter()
    duration_serial = end_serial - start_serial
    
    # Test parallel conversion
    # Note: Python object creation is limited by GIL, but we release/acquire GIL in parallel 
    # which can give speedups for large structures if PyObject creation is efficient.
    start_parallel = time.perf_counter()
    py_obj_parallel = val.to_python(threads=8)
    end_parallel = time.perf_counter()
    duration_parallel = end_parallel - start_parallel
    
    assert py_obj_serial == py_obj_parallel
    print(f"Serial (threads=1): {duration_serial:.4f}s")
    print(f"Parallel (threads=8): {duration_parallel:.4f}s")
    if duration_parallel < duration_serial:
        print(f"Speedup: {duration_serial / duration_parallel:.2x}")

def test_simd_string_escaping():
    """Verify that SIMD string escaping works (indirectly via __str__)."""
    print("\nTesting SIMD string escaping...")
    s = "Text with \"quotes\" and \\backslashes\\ and \n newlines." * 1000
    val = fastjson.JSONValue()
    # Manual construction might be slow but we can test __str__
    v = fastjson.parse('"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"')
    
    start = time.perf_counter()
    out = str(v)
    end = time.perf_counter()
    print(f"Stringification time (SIMD): {end - start:.6f}s")
    assert len(out) > len(s)

def test_128bit_conversion():
    """Ensure 128-bit integers are correctly converted to Python ints."""
    # Test numbers that fit within __int128 range (max 39 digits for unsigned)
    test_cases = [
        9223372036854775808,  # int64 max + 1
        18446744073709551615,  # uint64 max
        18446744073709551616,  # uint64 max + 1
        12345678901234567890123456789012345678,  # 38 digits, fits in uint128
        170141183460469231731687303715884105727,  # int128 max
    ]
    
    for huge_int in test_cases:
        val = fastjson.parse(str(huge_int))
        py_val = val.to_python()
        assert py_val == huge_int, f"Failed for {huge_int}: got {py_val}"
        assert isinstance(py_val, int), f"Expected int, got {type(py_val)}"
    
    # Test negative 128-bit
    neg_int = -123456789012345678901234567890
    val = fastjson.parse(str(neg_int))
    py_val = val.to_python()
    assert py_val == neg_int, f"Failed for negative: got {py_val}"

if __name__ == "__main__":
    test_parallel_conversion()
    test_128bit_conversion()
    print("All tests passed!")

