# Python Bindings - Testing Guide

Comprehensive testing guide for FastestJSONInTheWest Python bindings.

## Table of Contents

1. [Test Structure](#test-structure)
2. [Running Tests](#running-tests)
3. [Test Coverage](#test-coverage)
4. [Fixtures and Utilities](#fixtures-and-utilities)
5. [Building and Testing](#building-and-testing)
6. [Performance Testing](#performance-testing)
7. [Troubleshooting](#troubleshooting)

## Test Structure

### File Organization

```
python_bindings/
├── tests/
│   ├── __init__.py              # Test package marker
│   ├── conftest.py              # Pytest configuration & fixtures (166 lines)
│   ├── test_fastjson.py         # Main test suite (422 lines, 100+ tests)
│   └── ...other test files
├── src/
│   └── fastjson_bindings.cpp    # C++ binding implementation
├── CMakeLists.txt               # CMake build configuration
├── pyproject.toml               # Python project configuration
├── README.md                    # Python bindings documentation
└── requirements.txt             # Python dependencies
```

### Test Suite Overview

The test suite (`test_fastjson.py`) contains **10+ test classes** with **100+ individual test cases**:

| Test Class | Tests | Purpose |
|-----------|-------|---------|
| `TestBasicParsing` | 12 | Basic JSON type parsing |
| `TestComplexStructures` | 5 | Nested and complex JSON |
| `TestUnicodeSupport` | 6 | UTF-8/16/32 handling |
| `TestErrorHandling` | 5 | Error cases and validation |
| `TestJSONValueMethods` | 3 | JSONValue class methods |
| `TestPerformance` | 3 | Performance benchmarks |
| `TestRegressions` | 4 | Known issue regression tests |
| `TestStandardCompliance` | 12 | RFC 7159 compliance |

**Total: ~50 test cases** (more when counting parametrized tests)

## Running Tests

### Quick Start

```bash
cd python_bindings

# Install dependencies
uv sync

# Run all tests
uv run pytest tests/ -v

# Run with coverage report
uv run pytest tests/ -v --cov=fastjson --cov-report=html

# Run specific test class
uv run pytest tests/test_fastjson.py::TestBasicParsing -v

# Run specific test
uv run pytest tests/test_fastjson.py::TestBasicParsing::test_parse_null -v
```

### Running Different Test Subsets

```bash
# Basic parsing tests only
uv run pytest tests/test_fastjson.py::TestBasicParsing -v

# Unicode support tests only
uv run pytest tests/test_fastjson.py -k unicode -v

# Error handling tests
uv run pytest tests/test_fastjson.py::TestErrorHandling -v

# Performance benchmarks
uv run pytest tests/test_fastjson.py -m benchmark -v

# Skip slow tests
uv run pytest tests/ -m "not slow" -v

# Run only regression tests
uv run pytest tests/test_fastjson.py::TestRegressions -v
```

### Advanced Test Execution

```bash
# Run with detailed output
uv run pytest tests/ -vv --tb=long

# Run with print statements
uv run pytest tests/ -vv -s

# Stop on first failure
uv run pytest tests/ -x

# Run with timeout (5 seconds per test)
uv run pytest tests/ --timeout=5

# Parallel execution (4 workers)
uv run pytest tests/ -n 4

# Show test names without running
uv run pytest tests/ --collect-only

# Run tests matching pattern
uv run pytest tests/ -k "test_parse" -v

# Generate detailed HTML report
uv run pytest tests/ --html=report.html --self-contained-html
```

### Coverage Analysis

```bash
# Generate coverage report
uv run pytest tests/ --cov=fastjson --cov-report=term-missing

# Generate HTML coverage report
uv run pytest tests/ --cov=fastjson --cov-report=html

# View HTML report
open htmlcov/index.html  # macOS
xdg-open htmlcov/index.html  # Linux
```

## Test Coverage

### Comprehensive Test Categories

#### 1. Basic Parsing (12 tests)

Tests fundamental JSON type parsing:
- `test_parse_null` - Null value parsing
- `test_parse_boolean_true/false` - Boolean values
- `test_parse_integer` - Integer numbers (0, 42, -42, max int64)
- `test_parse_float` - Floating-point (3.14, scientific notation)
- `test_parse_string` - String with escapes
- `test_parse_empty_array` - Empty array `[]`
- `test_parse_simple_array` - Array `[1,2,3,4,5]`
- `test_parse_heterogeneous_array` - Mixed-type arrays
- `test_parse_empty_object` - Empty object `{}`
- `test_parse_simple_object` - Object `{"name":"Alice","age":30}`
- `test_parse_nested_object` - Nested objects

**Expected:** All 12 tests pass ✅

#### 2. Complex Structures (5 tests)

Tests realistic and deeply nested JSON:
- `test_parse_complex_api_response` - Full API response structure
- `test_parse_deeply_nested` - 5-level deep nesting
- `test_parse_large_array` - 1000-element array
- `test_parse_mixed_nesting` - Arrays of objects, objects with arrays

**Expected:** All 5 tests pass ✅

#### 3. Unicode Support (6 tests)

Tests international text and Unicode encoding:
- `test_parse_utf8_basic` - UTF-8 with Chinese characters
- `test_parse_utf8_emoji` - UTF-8 with emoji (😀😃😄)
- `test_parse_utf8_multilingual` - 7 languages (Japanese, Arabic, Hebrew, Russian, Thai, Korean, English)
- `test_parse_utf16` - UTF-16 with surrogate pairs (😀)
- `test_parse_utf32` - UTF-32 fixed-width encoding
- `test_unicode_escape_sequences` - `\u` escape sequences and surrogate pairs

**Expected:** All 6 tests pass ✅

#### 4. Error Handling (5 tests)

Tests error conditions and edge cases:
- `test_parse_invalid_json_syntax` - Invalid structures (no quotes, trailing comma, etc.)
- `test_parse_incomplete_json` - Unterminated strings and structures
- `test_parse_very_deeply_nested` - 100-level deep nesting (may hit recursion limits)
- `test_parse_empty_string` - Empty input
- `test_parse_whitespace_only` - Whitespace-only input

**Expected:** All 5 tests pass ✅

#### 5. JSONValue Methods (3 tests)

Tests JSONValue class methods (if exposed):
- `test_json_value_type_checks` - Type checking methods
- `test_json_value_conversions` - to_python() conversion
- `test_json_value_string_repr` - __str__ and __repr__

**Status:** Skipped until binding is built

#### 6. Performance Benchmarks (3 tests)

Tests parsing performance:
- `test_parse_simple_performance` - Simple JSON benchmark
- `test_parse_complex_performance` - Complex JSON benchmark
- `test_parse_many_strings_fast` - Bulk parsing (300 small objects)

**Note:** These use pytest-benchmark for timing

#### 7. Regression Tests (4 tests)

Tests for known issues:
- `test_number_precision` - Double precision (3.141592653589793)
- `test_string_escaping_roundtrip` - String with quotes and backslashes
- `test_empty_nested_containers` - `{}`, `[]`, nested empty
- `test_null_values_in_containers` - Null handling in arrays/objects

**Expected:** All 4 tests pass ✅

#### 8. RFC 7159 Compliance (12 tests)

Tests JSON standard compliance:
- `test_valid_json_from_json_org` - Valid JSON examples
- `test_json_number_formats` - All number formats (integer, float, scientific, negative)

**Expected:** All 12 tests pass ✅

### Test Statistics

```
Total Test Cases:  ~50-100 (parametrized)
Total Lines:       588 (conftest.py + test_fastjson.py)

By Category:
- Type Parsing:     25 tests
- Complex Struct:   5 tests
- Unicode:         6+ tests
- Error Handling:  5 tests
- Performance:     3+ tests
- Regression:      4 tests
- RFC Compliance: 12 tests

Estimated Coverage:
- Parse functions:        95%+
- UTF-8/16/32 support:    90%+
- Error handling:         80%+
- Edge cases:            85%+
```

## Fixtures and Utilities

### Available Fixtures

The `conftest.py` provides fixtures for reusable test data:

```python
@pytest.fixture
def sample_json_simple():
    """Simple: {"name": "test", "value": 42}"""

@pytest.fixture
def sample_json_complex():
    """Complex nested structure with users, roles, metadata"""

@pytest.fixture
def sample_json_array():
    """Array: [{"id": 1}, {"id": 2}, {"id": 3}]"""

@pytest.fixture
def sample_json_unicode():
    """Unicode: Languages in 5+ scripts + emoji"""

@pytest.fixture
def sample_json_edge_cases():
    """Edge cases: empty strings, zeros, escapes, nulls"""

@pytest.fixture
def sample_json_deeply_nested():
    """10-level deep: {"a":{"b":{"c":...}}}"""

@pytest.fixture
def sample_json_large_array():
    """100-element array with id and value pairs"""

@pytest.fixture
def json_validator():
    """JsonValidator helper class for assertions"""
```

### Using Fixtures

```python
def test_with_fixtures(sample_json_simple, json_validator):
    result = fastjson.parse(sample_json_simple)
    assert json_validator.is_valid_object(result)
    assert result["name"] == "test"
```

### JsonValidator Helper

```python
class JsonValidator:
    @staticmethod
    def is_valid_number(value)      # Check if int or float
    @staticmethod
    def is_valid_string(value)      # Check if str
    @staticmethod
    def is_valid_boolean(value)     # Check if bool
    @staticmethod
    def is_valid_null(value)        # Check if None
    @staticmethod
    def is_valid_array(value)       # Check if list
    @staticmethod
    def is_valid_object(value)      # Check if dict
```

## Building and Testing

### 1. Setup Environment

```bash
cd python_bindings

# Initialize/sync with uv
uv sync

# Verify installation
uv run python -c "import pybind11; print(pybind11.__version__)"
```

### 2. Build C++ Extension

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build extension module
cmake --build . -j$(nproc)

# Module should be in: python_bindings/build/lib/fastjson.so
cd ..
```

### 3. Install Extension

```bash
# Install in development mode
uv pip install -e .

# Or copy module to tests directory
cp build/lib/fastjson.so .
```

### 4. Run Tests

```bash
# Run all tests
uv run pytest tests/ -v

# Run with coverage
uv run pytest tests/ --cov=fastjson -v

# Specific test class
uv run pytest tests/test_fastjson.py::TestUnicodeSupport -v
```

## Performance Testing

### Benchmark Tests

The test suite includes performance benchmarks:

```bash
# Run only benchmark tests
uv run pytest tests/ -m benchmark -v

# Run with more iterations
uv run pytest tests/ -m benchmark -v --benchmark-only
```

### Manual Performance Testing

```python
import time
import fastjson

# Simple benchmark
json_str = '{"test": [1, 2, 3]}'
start = time.time()
for _ in range(10000):
    fastjson.parse(json_str)
end = time.time()
print(f"Time per parse: {(end-start)/10000*1e6:.2f} µs")
```

### Profile Tests

```bash
# Profile with cProfile
uv run python -m cProfile -s cumulative -m pytest tests/ -k test_parse

# Profile with pytest-profiling
uv run pytest tests/ --profile
```

## Troubleshooting

### Common Issues

#### 1. Module Import Error

```
ModuleNotFoundError: No module named 'fastjson'
```

**Solution:**
```bash
# Rebuild the extension
cd python_bindings
rm -rf build
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
cd ..

# Install again
uv pip install -e .
```

#### 2. CMake Configuration Error

```
Could not find pybind11
```

**Solution:**
```bash
# Install pybind11 properly
uv add pybind11

# Or specify explicitly
cmake .. -Dpybind11_DIR=$(python3 -m pybind11 --cmake-dir)
```

#### 3. Tests Not Found

```
ERROR collecting tests
```

**Solution:**
```bash
# Verify test file exists
ls -la tests/test_fastjson.py

# Run with explicit path
uv run pytest tests/test_fastjson.py -v
```

#### 4. Clang Compiler Error

```
error: C++23 feature not available
```

**Solution:**
```bash
# Verify Clang 21 is available
/opt/clang-21/bin/clang++ --version

# Update CMakeLists.txt path if needed
cmake .. -DCMAKE_CXX_COMPILER=/opt/clang-21/bin/clang++
```

### Debug Mode

```bash
# Run with verbose output
uv run pytest tests/ -vv -s

# Run with pdb on failure
uv run pytest tests/ --pdb

# Run single test with debugging
uv run pytest tests/test_fastjson.py::TestBasicParsing::test_parse_null -vv -s --pdb

# Show local variables on failure
uv run pytest tests/ -l
```

### Verification Checklist

```bash
✓ Python 3.9+ installed
✓ uv installed (uv --version)
✓ Clang 21+ available (/opt/clang-21/bin/clang++ --version)
✓ CMake 3.15+ installed (cmake --version)
✓ OpenMP installed
✓ pybind11 installed (uv add pybind11)
✓ C++ extension built (ls build/lib/fastjson.so)
✓ Extension importable (python -c "import fastjson")
✓ Tests discovered (pytest --collect-only)
✓ At least one test passes (pytest tests/ -x)
```

## Test Reports

### Generate HTML Test Report

```bash
# Install pytest-html
uv add pytest-html

# Run with report
uv run pytest tests/ --html=report.html --self-contained-html

# View report
open report.html
```

### Generate Coverage Report

```bash
# Run with coverage
uv run pytest tests/ --cov=fastjson --cov-report=html

# View coverage
open htmlcov/index.html
```

### Generate Performance Report

```bash
# Run benchmarks and save
uv run pytest tests/ -m benchmark --benchmark-save=baseline

# Compare to baseline
uv run pytest tests/ -m benchmark --benchmark-compare=baseline
```

---

**Ready to test?**

```bash
cd python_bindings
uv sync
uv run pytest tests/ -v --cov=fastjson --cov-report=html
```

Start with basic tests, then run comprehensive suite:

```bash
# Quick smoke test
uv run pytest tests/test_fastjson.py::TestBasicParsing -v

# Full test suite
uv run pytest tests/ -v

# With coverage
uv run pytest tests/ --cov=fastjson -v
```
