# Unit Tests

This directory contains individual component tests for FastestJSONInTheWest.

## Purpose
- Test individual functions and classes in isolation
- Validate core functionality components
- Ensure high code coverage (>95%)
- Fast execution (<1ms per test)

## Test Categories
- `json_value_test.cpp` - JSON value type system tests
- `parser_test.cpp` - Core parsing engine tests
- `simd_test.cpp` - SIMD operation validation
- `error_handling_test.cpp` - Error reporting tests
- `memory_management_test.cpp` - Smart pointer compliance tests

## Running Unit Tests
```bash
cmake --build . --target unit_tests
./unit_tests
```