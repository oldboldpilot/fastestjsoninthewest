# Integration Tests

This directory contains end-to-end workflow tests for FastestJSONInTheWest.

## Purpose
- Test complete parsing workflows
- Validate feature interactions
- Real-world usage scenarios
- Module integration validation

## Test Categories
- `module_test.cpp` - C++23 module integration
- `feature_test.cpp` - Complete feature validation
- `workflow_test.cpp` - End-to-end JSON processing
- `comprehensive_test.cpp` - Large-scale integration
- `api_test.cpp` - Public API validation

## Running Integration Tests
```bash
cmake --build . --target integration_tests
./integration_tests
```