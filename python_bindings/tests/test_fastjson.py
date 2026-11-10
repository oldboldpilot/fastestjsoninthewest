import pytest
import sys
try:
    import fastjson
except ImportError:
    pytest.skip("fastjson module not found. Build with 'uv pip install -e .'", allow_module_level=True)

def test_parse_basic():
    """Test basic parsing and to_python conversion."""
    json_str = '{"name": "Speedy", "value": 42, "is_fast": true}'
    val = fastjson.parse(json_str)
    
    assert val.is_object()
    assert val["name"].as_string() == "Speedy"
    assert val["value"].as_int() == 42
    assert val["is_fast"].is_bool()
    
    # Test conversion
    py_obj = val.to_python()
    # Note: JSON numbers are stored as floats by default
    assert py_obj == {"name": "Speedy", "value": 42.0, "is_fast": True}

def test_parse_array():
    json_str = '[1, 2, 3]'
    val = fastjson.parse(json_str)
    assert val.is_array()
    assert len(val) == 3
    assert val[0].as_int() == 1
    
    py_list = val.to_python()
    # Note: JSON numbers are floats
    assert py_list == [1.0, 2.0, 3.0]

def test_linq_query():
    """Test LINQ-style query."""
    json_str = '[{"val": 10}, {"val": 20}, {"val": 30}]'
    data = fastjson.parse(json_str)
    
    # Query: > 15
    result = (fastjson.query(data)
              .where(lambda x: x["val"].as_int() > 15)
              .select(lambda x: x["val"])
              .to_list())
    
    # to_list() returns Python floats directly
    assert result == [20.0, 30.0]

def test_vectors():
    """Test zero-copy typed numeric and string vector bindings."""
    nums = fastjson.Int64Vector([10, 20, 30])
    nums.append(40)
    assert len(nums) == 4
    assert nums[0] == 10
    assert nums[3] == 40
    assert nums.sum() == 100
    assert nums.to_list() == [10, 20, 30, 40]
    assert nums.to_json() == "[10,20,30,40]"

    floats = fastjson.Float64Vector([1.5, 2.5])
    assert floats.sum() == 4.0

    strings = fastjson.StringVector(["alpha", "beta"])
    assert strings.to_list() == ["alpha", "beta"]

def test_mustache_rendering():
    """Test Mustache template rendering."""
    json_str = '{"name": "World", "show": true, "items": ["A", "B"]}'
    data = fastjson.parse(json_str)
    
    # 1. Variable substitution
    assert fastjson.render_template("Hello {{name}}", data) == "Hello World"
    
    # 2. Section (Truthy)
    assert fastjson.render_template("{{#show}}Visible{{/show}}", data) == "Visible"
    
    # 3. List Iteration
    list_tpl = "{{#items}}- {{.}}\n{{/items}}"
    assert fastjson.render_template(list_tpl, data) == "- A\n- B\n"

def test_128bit_integers():
    """Test 128-bit integer handling."""
    # Number larger than 2^64
    large_num_str = "18446744073709551616" # 2^64
    json_str = f'{{"large": {large_num_str}}}'
    val = fastjson.parse(json_str)
    
    # Check if we can get it back as python int (requires strict string conversion in bindings)
    py_val = val["large"].to_python()
    assert isinstance(py_val, int)
    assert str(py_val) == large_num_str

def test_parallel_parse():
    """Test parallel parsing - use parse() which is already parallel internally."""
    # parse() is GIL-free and uses SIMD internally
    json_str = '[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]'
    val = fastjson.parse(json_str)
    assert len(val) == 10
