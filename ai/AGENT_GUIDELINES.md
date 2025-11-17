# FastestJSONInTheWest - AI Agent Development Guidelines

## ⚠️ CRITICAL: Git Author Policy (ENFORCE THIS!)

### MANDATORY Author Configuration
**Before ANY git commit, ensure:**
```bash
git config user.name "Olumuyiwa Oluwasanmi"
git config --unset user.email  # Privacy - NO email
```

### Commit Message Rules:
1. ✅ Author: Olumuyiwa Oluwasanmi ONLY
2. ❌ NEVER add "Co-authored-by:" lines
3. ❌ NEVER include email addresses
4. ❌ NEVER attribute AI assistants (Claude, Copilot, etc.)
5. ❌ NEVER add "with help from" or similar phrases

### Why:
- Privacy: No email exposure
- Clean history: Single author for consistency
- Professionalism: No AI assistant attribution

**All agents MUST follow this policy. Non-compliance is a critical error.**

## ⚠️ CRITICAL: Compiler and Build System (NEVER FORGET!)

### Exact Toolchain Paths - MEMORIZE THESE!
```bash
C++ COMPILER:  /opt/clang-21/bin/clang++    # Clang 21.1.5 (built from source)
C COMPILER:    /opt/clang-21/bin/clang      # DO NOT use linuxbrew compilers!
NINJA:         /usr/local/bin/ninja         # Version 1.13.1 (built from source)
CMAKE:         /usr/bin/cmake               # System CMake 3.28.3
```

### ❌ FORBIDDEN: Do NOT Use These Paths
- `/home/linuxbrew/.linuxbrew/bin/clang++` (BROKEN - missing headers)
- Any Homebrew/Linuxbrew compilers
- System package clang without full path
- Default `clang++` in PATH

### Required Build Configuration
```cmake
# MANDATORY for all CMakeLists.txt files:
cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_COMPILER /opt/clang-21/bin/clang++)
set(CMAKE_C_COMPILER /opt/clang-21/bin/clang)
set(CMAKE_MAKE_PROGRAM /usr/local/bin/ninja)
set(CMAKE_CXX_STANDARD 23)

# Header and library paths for Clang 21 (built from source)
include_directories(
    /opt/clang-21/include
    /opt/clang-21/lib/clang/21/include
)

link_directories(
    /opt/clang-21/lib
    /opt/clang-21/lib/x86_64-unknown-linux-gnu
)

# MUST use Ninja generator:
# cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
#   -DCMAKE_CXX_COMPILER=/opt/clang-21/bin/clang++ \
#   -DCMAKE_MAKE_PROGRAM=/usr/local/bin/ninja \
#   ../
```

### Why This Matters
- **Wrong compiler = missing headers** (stddef.h, etc.)
- **Wrong Ninja = no C++23 modules support**
- **Linuxbrew paths = broken builds**
- Using exact paths prevents 90% of build errors

### Session State Files
**ALWAYS read these files first:**
- `.project_session_state.md` - Current project status, compiler paths
- `.llvm_build_session.md` - LLVM build status
- `docs/CODING_STANDARDS.md` - Complete coding standards with compiler info

### Current LLVM Build Status
- **Full feature build in progress** (Process 238689)
- **Directory:** `/home/muyiwa/toolchain-build/llvm-full-build`
- **Check progress:** `tail /home/muyiwa/toolchain-build/llvm-full-build/build.log`
- **After completion:** `sudo ninja install` updates `/opt/clang-21`

---

## Overview
This document provides mandatory guidelines for AI agents working on FastestJSONInTheWest. All AI agents must follow these rules to ensure consistent, high-quality development.

## Project Understanding Requirements

### 1. Core Project Goals (MANDATORY KNOWLEDGE)
Every AI agent MUST understand:
- **Primary Objective**: Create the fastest JSON parser in the industry
- **Performance Target**: >1GB/s parsing throughput
- **Large File Support**: Handle 2GB+ files efficiently
- **Thread Safety**: All operations must be thread-safe
- **C++23 Compliance**: Strict modern C++ standards
- **SIMD Optimization**: Maximum hardware utilization

### 2. Architecture Constraints (NON-NEGOTIABLE)
- **Smart Pointers Only**: NO raw pointers allowed
- **Module System**: C++23 modules exclusively
- **Error Handling**: Use std::expected pattern
- **Memory Safety**: Zero tolerance for memory issues
- **Thread Safety**: All public APIs must be thread-safe

## Coding Standards Enforcement

### 1. Code Quality Rules (MUST ENFORCE)
```cpp
// ✅ ACCEPTABLE: Modern C++23 style
auto parse_value() -> json_result<json_value> {
    return parse_implementation();
}

// ❌ FORBIDDEN: Raw pointers, old C++ style
json_value* parse_value() {  // NEVER ALLOW THIS
    return new json_value(); // IMMEDIATE REJECTION
}
```

### 2. Naming Conventions (STRICT)
- **Functions/Variables**: snake_case only
- **Types/Classes**: snake_case (STL style)
- **Constants**: snake_case with descriptive names
- **No Abbreviations**: Use full words

### 3. Performance Requirements (CRITICAL)
- **SIMD First**: Always consider SIMD optimization opportunities
- **Memory Efficiency**: Minimize allocations and copies
- **Thread Safety**: Design for concurrent access
- **Error Paths**: Fast failure with detailed context

## Test Organization Policy (MANDATORY)

### Test Directory Structure (ENFORCE STRICTLY)
```
tests/
├── unit/           # Individual component tests
├── integration/    # End-to-end workflow tests
├── performance/    # Benchmarks and large-scale tests
└── compatibility/  # Cross-platform validation tests
```

### Agent Test Enforcement Rules
1. **ALL tests MUST be in tests/ directory**
2. **NO test files outside of tests/ directory**
3. **Proper categorization**: Tests must be in correct subdirectory
4. **Performance focus**: Large file tests (2GB+) are MANDATORY
5. **Coverage requirements**: Aim for >95% code coverage

### Test File Standards
```cpp
// ✅ GOOD: Properly organized test
// File: tests/unit/json_value_test.cpp
#include <gtest/gtest.h>
import fastjson;

TEST(JsonValue, ParsesNullCorrectly) {
    auto result = fastjson::parse("null");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_null());
}
```

## Development Workflow Requirements

### 1. Before Making Changes (MANDATORY CHECKLIST)
- [ ] Read and understand PRD requirements
- [ ] Review architecture documentation
- [ ] Check coding standards compliance
- [ ] Verify test organization policy
- [ ] Understand performance expectations

### 2. During Development (CONTINUOUS VALIDATION)
- [ ] Follow smart pointer policy strictly
- [ ] Implement SIMD optimizations where applicable
- [ ] Ensure thread safety in all public APIs
- [ ] Write comprehensive tests in correct directories
- [ ] Document performance characteristics

### 3. After Changes (VALIDATION REQUIREMENTS)
- [ ] Verify all tests pass
- [ ] Check performance hasn't regressed
- [ ] Validate memory safety (no leaks)
- [ ] Confirm thread safety
- [ ] Update documentation if needed

## JSON Type System Rules (STRICTLY ENFORCE)

### Container Mappings (EXACT SPECIFICATIONS)
```cpp
// ✅ MANDATORY: Exact type definitions
using json_null = std::nullptr_t;                              // nullptr equivalence
using json_boolean = bool;
using json_number = double;
using json_string = std::string;
using json_array = std::vector<json_value>;                    // Vector for arrays
using json_object = std::unordered_map<std::string, json_value>; // Map for objects
```

### Type Safety Requirements
- **Variant Storage**: Use std::variant for type safety
- **Move Semantics**: Implement efficient move operations
- **Const Correctness**: Proper const method design
- **Exception Safety**: Strong exception guarantees

## Performance Optimization Guidelines

### 1. SIMD Integration (HIGH PRIORITY)
```cpp
// ✅ REQUIRED: SIMD with fallback
auto process_string_simd(const char* data, size_t len) -> result {
    if (has_avx512()) {
        return process_string_avx512(data, len);
    }
    if (has_avx2()) {
        return process_string_avx2(data, len);
    }
    return process_string_scalar(data, len); // Always provide fallback
}
```

### 2. Memory Optimization (CRITICAL)
- **Minimize Allocations**: Reuse buffers where possible
- **Cache Friendly**: Sequential memory access patterns
- **Move Semantics**: Avoid unnecessary copies
- **Smart Sizing**: Appropriate container reserve() calls

### 3. Thread Safety Design (ESSENTIAL)
```cpp
// ✅ GOOD: Thread-safe by design
class json_value {
public:
    // Const methods are inherently thread-safe
    auto is_string() const noexcept -> bool;
    auto as_string() const -> const std::string&;
    
    // Mutating methods require exclusive access
    auto push_back(json_value value) -> json_value&;  // Not thread-safe
};
```

## Error Handling Standards (MANDATORY)

### Error Reporting Requirements
```cpp
// ✅ REQUIRED: Comprehensive error context
struct json_error {
    json_error_code code;    // Specific error classification
    std::string message;     // Human-readable description
    size_t position;         // Exact character position
    size_t line;            // Line number for debugging
    size_t column;          // Column number for debugging
};
```

### Error Handling Patterns
- **No Exceptions**: Use std::expected pattern
- **Rich Context**: Provide detailed error information
- **Fast Failure**: Detect errors as early as possible
- **Graceful Degradation**: Handle edge cases elegantly

## Documentation Requirements

### 1. Code Documentation (MANDATORY)
- **Public APIs**: Doxygen comments for all public functions
- **Complex Logic**: Explain non-obvious algorithms
- **Performance Notes**: Document optimization decisions
- **Thread Safety**: Explicitly document thread safety guarantees

### 2. Architecture Updates (WHEN REQUIRED)
- **Significant Changes**: Update architecture documentation
- **New Optimizations**: Document in performance guide
- **API Changes**: Update interface documentation
- **Breaking Changes**: Document migration path

## Build System Requirements

### CMake Standards (ENFORCE)
- **C++23 Modules**: Proper module compilation support
- **SIMD Detection**: Runtime capability detection
- **OpenMP Integration**: Parallel compilation flags
- **Cross-Platform**: Support multiple compilers

### Compiler Support
- **GCC 13+**: Primary development compiler
- **Clang 16+**: Secondary compiler support
- **MSVC 19.35+**: Windows compatibility

## Quality Assurance Checklist

### Before Code Submission (MANDATORY)
- [ ] All tests pass in all categories
- [ ] No memory leaks detected
- [ ] Performance benchmarks pass
- [ ] Thread safety validation complete
- [ ] Documentation updated
- [ ] Coding standards compliance verified

### Performance Validation (REQUIRED)
- [ ] Large file tests (2GB+) pass
- [ ] SIMD optimizations active
- [ ] Memory usage within limits
- [ ] Throughput targets achieved

## Agent Collaboration Rules

### 1. Knowledge Sharing (MANDATORY)
- **Document Decisions**: Explain complex choices
- **Share Context**: Provide reasoning for changes
- **Update Guidelines**: Improve documentation continuously
- **Mentor New Agents**: Help maintain standards

### 2. Conflict Resolution (PROCESS)
- **Architecture First**: Consult architecture docs
- **Standards Compliance**: Coding standards are non-negotiable
- **Performance Impact**: Measure before deciding
- **Test Validation**: Let tests guide decisions

## Escalation Procedures

### When to Escalate
1. **Performance Regression**: Any measurable slowdown
2. **Memory Issues**: Leaks or excessive usage
3. **Thread Safety**: Concurrency problems
4. **Standard Violations**: Coding standard conflicts
5. **Architecture Changes**: Major design modifications

### How to Escalate
1. **Document Issue**: Clear problem description
2. **Provide Context**: Include relevant code/tests
3. **Suggest Solutions**: Propose alternatives
4. **Impact Assessment**: Performance/safety implications

---
*These guidelines ensure all AI agents contribute effectively to FastestJSONInTheWest while maintaining the highest standards of code quality and performance.*