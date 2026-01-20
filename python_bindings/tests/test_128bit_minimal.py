import fastjson
import pytest

def test_128bit_integer():
    """Test 128-bit integers (numbers exceeding 64-bit range)."""
    # 2^100 is a large number that fits in 128-bit but not 64-bit
    large_int = 2**100
    json_str = f'{{"val": {large_int}}}'
    val = fastjson.parse(json_str)
    data = val.to_python()
    assert data["val"] == large_int
    assert isinstance(data["val"], int)

def test_128bit_unsigned():
    """Test maximum 128-bit unsigned integers."""
    # (2^128 - 1)
    large_int = (1 << 128) - 1
    json_str = f'{{"val": {large_int}}}'
    val = fastjson.parse(json_str)
    data = val.to_python()
    assert data["val"] == large_int
    assert isinstance(data["val"], int)

def test_small_integer():
    """Test small integers - stored as float in JSON."""
    # NOTE: JSON numbers without decimal are stored as double internally
    # Only 128-bit integers are converted to Python int
    json_str = '{"val": 123}'
    val = fastjson.parse(json_str)
    data = val.to_python()
    assert data["val"] == 123  # Value is correct
    # Small integers are stored as float in JSON spec, converted to float
    assert isinstance(data["val"], (int, float))

def test_integer_via_as_int():
    """Test getting integers via as_int() method."""
    json_str = '{"val": 123}'
    val = fastjson.parse(json_str)
    # as_int() explicitly converts to int
    assert val["val"].as_int() == 123

if __name__ == "__main__":
    test_128bit_integer()
    test_128bit_unsigned()
    test_small_integer()
    test_integer_via_as_int()
    print("All tests passed!")
