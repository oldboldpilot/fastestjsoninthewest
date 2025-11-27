# FastestJSONInTheWest - Test Organization Policy

## Policy Statement
This document establishes the MANDATORY test organization policy for FastestJSONInTheWest. All tests must be organized according to this structure, and no deviations are permitted.

## Test Directory Structure (ENFORCED)

### Root Test Directory
```
tests/                    # ALL tests must be under this directory
├── unit/                # Individual component tests
├── integration/         # End-to-end workflow tests
├── performance/         # Benchmarks and stress tests
└── compatibility/       # Cross-platform validation tests
```

### Strict Rules
1. **ALL test files MUST be in the tests/ directory**
2. **NO test files are permitted outside tests/**
3. **NO exceptions to this structure**
4. **All agents must enforce this policy**

## Test Category Definitions

### 1. Unit Tests (tests/unit/)
**Purpose**: Test individual components in isolation

#### Criteria for Unit Tests
- Tests single functions or classes
- No external dependencies
- Fast execution (<1ms per test)
- High code coverage focus

#### Unit Test Examples
```cpp
// File: tests/unit/json_value_test.cpp
TEST(JsonValue, ConstructsFromNull) {
    json_value val(nullptr);
    EXPECT_TRUE(val.is_null());
}

// File: tests/unit/parser_test.cpp
TEST(Parser, ParsesSimpleString) {
    parser p("\"hello\"");
    auto result = p.parse_string();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "hello");
}
```

### 2. Integration Tests (tests/integration/)
**Purpose**: Test complete workflows and feature interactions

#### Criteria for Integration Tests
- Tests multiple components together
- Real-world usage scenarios
- End-to-end functionality
- Module integration validation

#### Integration Test Examples
```cpp
// File: tests/integration/parse_workflow_test.cpp
TEST(ParseWorkflow, CompleteJsonDocument) {
    const auto json = R"(
    {
        "users": [
            {"name": "Alice", "age": 30},
            {"name": "Bob", "age": 25}
        ]
    }
    )";
    
    auto result = fastjson::parse(json);
    ASSERT_TRUE(result.has_value());
    
    auto& root = result.value();
    EXPECT_TRUE(root.is_object());
    EXPECT_TRUE(root.contains("users"));
}
```

### 3. Performance Tests (tests/performance/)
**Purpose**: Benchmarking and performance validation

#### Criteria for Performance Tests
- Measure parsing speed
- Memory usage validation
- Large file processing (2GB+ requirement)
- SIMD optimization verification
- Throughput measurements

#### Performance Test Examples
```cpp
// File: tests/performance/large_file_benchmark.cpp
TEST(Performance, Parse2GBJsonFile) {
    auto large_json = generate_large_json(2048); // 2GB
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = fastjson::parse(large_json);
    auto end = std::chrono::high_resolution_clock::now();
    
    ASSERT_TRUE(result.has_value());
    
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    EXPECT_LT(duration.count(), 30); // Must parse in <30 seconds
}

// File: tests/performance/simd_benchmark.cpp
TEST(Performance, SIMDOptimizationActive) {
    // Verify SIMD instructions are being used
    auto caps = detect_simd_capabilities();
    EXPECT_NE(caps & SIMD_AVX2, 0); // Should have at least AVX2
}
```

### 4. Compatibility Tests (tests/compatibility/)
**Purpose**: Cross-platform and compiler validation

#### Criteria for Compatibility Tests
- Different compiler support (GCC, Clang, MSVC)
- Platform-specific features
- C++ standard compliance
- Legacy compatibility where required

#### Compatibility Test Examples
```cpp
// File: tests/compatibility/cpp23_features_test.cpp
TEST(Compatibility, Cpp23ModulesWork) {
    // Test that C++23 modules function correctly
    auto result = fastjson::parse("{\"test\": true}");
    EXPECT_TRUE(result.has_value());
}

// File: tests/compatibility/cross_platform_test.cpp
TEST(Compatibility, PlatformAgnosticParsing) {
    // Test consistent behavior across platforms
    const auto test_cases = get_platform_test_cases();
    for (const auto& test_case : test_cases) {
        auto result = fastjson::parse(test_case.input);
        EXPECT_EQ(result.has_value(), test_case.should_succeed);
    }
}
```

## File Naming Conventions

### Standard Naming Pattern
```
<component>_test.cpp          # Unit tests
<workflow>_integration.cpp    # Integration tests  
<feature>_benchmark.cpp       # Performance tests
<platform>_compatibility.cpp  # Compatibility tests
```

### Examples
- `tests/unit/json_parser_test.cpp`
- `tests/unit/simd_operations_test.cpp`
- `tests/integration/module_integration.cpp`
- `tests/integration/complete_workflow.cpp`
- `tests/performance/large_scale_benchmark.cpp`
- `tests/performance/memory_usage_benchmark.cpp`
- `tests/compatibility/gcc_compatibility.cpp`
- `tests/compatibility/windows_platform.cpp`

## CMakeLists.txt Organization

### Test Target Structure
```cmake
# Unit Tests
add_executable(unit_tests)
target_sources(unit_tests PRIVATE
    tests/unit/json_value_test.cpp
    tests/unit/parser_test.cpp
    tests/unit/simd_test.cpp
)

# Integration Tests
add_executable(integration_tests)
target_sources(integration_tests PRIVATE
    tests/integration/module_test.cpp
    tests/integration/workflow_test.cpp
)

# Performance Tests
add_executable(performance_tests)
target_sources(performance_tests PRIVATE
    tests/performance/large_scale_benchmark.cpp
    tests/performance/simd_benchmark.cpp
)

# Compatibility Tests
add_executable(compatibility_tests)
target_sources(compatibility_tests PRIVATE
    tests/compatibility/cpp23_test.cpp
    tests/compatibility/cross_platform_test.cpp
)
```

## Test Execution Requirements

### Continuous Integration
```bash
# All test categories must pass
cmake --build . --target unit_tests
./unit_tests

cmake --build . --target integration_tests
./integration_tests

cmake --build . --target performance_tests
./performance_tests

cmake --build . --target compatibility_tests
./compatibility_tests
```

### Performance Benchmarks
- **Unit Tests**: Complete in <1 second total
- **Integration Tests**: Complete in <10 seconds total
- **Performance Tests**: Validate specific performance targets
- **Compatibility Tests**: Complete in <5 seconds total

## Quality Gates

### Test Coverage Requirements
- **Unit Tests**: >95% line coverage
- **Integration Tests**: >90% feature coverage
- **Performance Tests**: All benchmarks must pass
- **Compatibility Tests**: All platforms must pass

### Performance Gates
- **Large File Processing**: 2GB+ files in <30 seconds
- **Small JSON**: <0.1ms parse time
- **Memory Usage**: <10% overhead
- **Thread Safety**: Zero race conditions

## Enforcement Procedures

### Automated Checks
1. **Directory Structure**: CI validates all tests are in tests/
2. **Naming Conventions**: Automated filename validation
3. **Category Compliance**: Tests must be in correct subdirectories
4. **Performance Gates**: Benchmark validation in CI

### Manual Reviews
1. **Test Quality**: Code review for test effectiveness
2. **Coverage Analysis**: Ensure adequate test coverage
3. **Performance Validation**: Review benchmark results
4. **Documentation**: Verify test documentation

## Migration Procedures

### Moving Existing Tests
```bash
# Example migration commands
mv old_test_location/parser_test.cpp tests/unit/
mv benchmark.cpp tests/performance/parser_benchmark.cpp
mv integration_test.cpp tests/integration/
```

### Updating Build System
```cmake
# Update CMakeLists.txt to reference new locations
# Remove old target definitions
# Add new organized target structure
```

## Compliance Validation

### Checklist for All Tests
- [ ] Test is in correct tests/ subdirectory
- [ ] Filename follows naming conventions
- [ ] Test category is appropriate
- [ ] Performance requirements met
- [ ] Documentation is adequate
- [ ] CMakeLists.txt updated

### Regular Audits
- **Weekly**: Verify no tests outside tests/ directory
- **Monthly**: Review test categorization accuracy
- **Quarterly**: Performance benchmark validation
- **Release**: Complete compliance audit

## Exceptions and Waivers

### No Exceptions Policy
**There are NO exceptions to this test organization policy.**

- No test files outside tests/ directory
- No alternative directory structures
- No temporary test locations
- No legacy test organization

### Historical Migration
Any existing tests not following this structure must be migrated immediately upon discovery.

---
*This test organization policy is mandatory and must be enforced by all developers and AI agents working on FastestJSONInTheWest.*