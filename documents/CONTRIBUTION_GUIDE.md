# Contribution Guide

Thank you for your interest in contributing to FastestJSONInTheWest! This guide will help you get started.

## Getting Started

### Prerequisites

- **Compiler**: Clang 21+ (C++23 support required)
- **Build System**: CMake 3.28+
- **Tools**: Git, OpenMP 5.0+
- **Optional**: Valgrind (for memory profiling)

### Setup Development Environment

```bash
# Clone repository
git clone https://github.com/oldboldpilot/fastestjsoninthewest.git
cd fastestjsoninthewest

# Create build directory
mkdir build && cd build

# Build with debug symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Run tests
./comparative_benchmark
```

---

## Code Style Guide

### C++23 Standards

- **Standard**: C++23 with modules
- **Style**: Modern C++ (RAII, smart pointers, move semantics)
- **Formatting**: Use clang-format (provided: `.clang-format`)

### Naming Conventions

```cpp
// Classes and structs: PascalCase
class JsonParser { };
struct ParseResult { };

// Functions and methods: snake_case
void parse_json(const std::string& input);

// Member variables: snake_case with trailing underscore (optional)
private:
    std::vector<json> elements_;

// Constants: UPPER_SNAKE_CASE
constexpr size_t DEFAULT_BUFFER_SIZE = 4096;

// Template parameters: PascalCase or T, U, V for simple cases
template<typename Container>
void process(Container& data);

// Namespaces: snake_case
namespace fastjson::linq { }
```

### Code Structure

```cpp
// 1. Includes (grouped and sorted)
#include <vector>
#include <string>
#include <algorithm>

// 2. Using declarations (minimal, prefer std::)
using namespace std;

// 3. Type definitions
using StringVector = std::vector<std::string>;

// 4. Class definition
class MyClass {
public:
    // Constructor, destructor
    MyClass() = default;
    ~MyClass() = default;
    
    // Public interface
    void public_method();
    
private:
    // Private implementation
    std::vector<int> data_;
};

// 5. Implementation
inline void MyClass::public_method() {
    // Implementation
}
```

### Documentation

```cpp
/// Brief description of what this function does.
/// 
/// Detailed explanation of behavior, edge cases, and important notes.
///
/// @param input The input parameter description
/// @param threshold Filter threshold value
/// @return Description of return value or behavior
/// @throws std::invalid_argument When input is invalid
///
/// @example
/// auto result = process_data(input, 100);
template<typename T>
std::vector<T> process_data(const std::vector<T>& input, int threshold);
```

---

## Testing Requirements

### Test Organization

```
tests/
├── unit_tests/
│   ├── json_parser_test.cpp
│   ├── linq_operations_test.cpp
│   └── unicode_test.cpp
├── integration_tests/
│   ├── end_to_end_test.cpp
│   └── performance_test.cpp
└── benchmark/
    └── comparative_benchmark.cpp
```

### Writing Tests

```cpp
#include <cassert>
#include <iostream>
#include "modules/json_linq.h"

using namespace fastjson::linq;

void test_basic_filtering() {
    // Arrange
    auto data = std::vector<int>{1, 2, 3, 4, 5};
    
    // Act
    auto result = from(data)
        .where([](int x) { return x > 2; })
        .to_vector();
    
    // Assert
    assert(result.size() == 3);
    assert(result[0] == 3);
    assert(result[1] == 4);
    assert(result[2] == 5);
    
    std::cout << "✓ test_basic_filtering passed\n";
}

int main() {
    test_basic_filtering();
    // More tests...
    std::cout << "All tests passed!\n";
    return 0;
}
```

### Running Tests

```bash
# Build all tests
make tests

# Run specific test
./json_parser_test

# Run with memory checking
valgrind --leak-check=full ./json_parser_test

# Run with thread safety checking
valgrind --tool=helgrind ./parallel_test
```

### Test Coverage Goals

- **Unit tests**: >90% for core functionality
- **Integration tests**: >80% for workflows
- **Edge cases**: All error conditions
- **Performance**: Benchmark suite included

---

## Benchmarking

### Running Benchmarks

```bash
# Compile benchmark
./comparative_benchmark

# Output will show detailed metrics
```

### Adding New Benchmarks

```cpp
class MyBenchmark {
public:
    void setup() {
        // Prepare test data
        data_ = generate_large_dataset(100000);
    }
    
    void run() {
        // Your benchmark
        auto result = from(data_)
            .where([](int x) { return x > 50; })
            .to_vector();
    }
    
    void teardown() {
        // Cleanup
        data_.clear();
    }
    
private:
    std::vector<int> data_;
};
```

### Performance Expectations

- **Parsing**: Should not regress > 5%
- **LINQ ops**: Should maintain 2-8x advantage over simdjson
- **Memory**: Zero new leaks in Valgrind
- **Parallelization**: Should scale linearly with threads

---

## Memory Safety

### Valgrind Checks

Before submitting PR, verify memory safety:

```bash
# Memcheck (memory leaks)
valgrind --leak-check=full --show-leak-kinds=all ./test

# Helgrind (thread safety)
valgrind --tool=helgrind ./parallel_test

# Cachegrind (cache performance)
valgrind --tool=cachegrind ./benchmark
```

### Memory Management

- **RAII**: Use constructors/destructors for resource management
- **Smart pointers**: Prefer `std::unique_ptr`, `std::shared_ptr`
- **No raw pointers**: Unless absolutely necessary with clear ownership
- **Move semantics**: Implement move constructors/assignments

```cpp
// Good
class DataProcessor {
private:
    std::unique_ptr<std::vector<int>> data_;
    std::vector<std::string> results_;  // RAII via vector
};

// Avoid
class BadDataProcessor {
private:
    int* data_;  // Raw pointer - memory leak risk
    std::string* results_;  // Raw pointer - memory leak risk
};
```

---

## Pull Request Process

### Before Submitting

1. **Code Quality**
   ```bash
   clang-format -i modules/*.h modules/*.cpp
   clang-tidy modules/*.cpp -- -std=c++23 -I.
   ```

2. **Tests Pass**
   ```bash
   make tests
   ./test_all
   ```

3. **Memory Safe**
   ```bash
   valgrind --leak-check=full ./test_all
   ```

4. **Documentation Updated**
   - Update API_REFERENCE.md if adding API
   - Update LINQ_IMPLEMENTATION.md for query changes
   - Add code examples

### Commit Message Format

```
type(scope): subject

Detailed description explaining the change, why it was made,
and any related issue numbers.

- Bullet point 1
- Bullet point 2

Fixes #123
```

**Types**: feat, fix, docs, style, refactor, test, perf, chore

**Example**:
```
feat(linq): Add where clause optimization for small datasets

Implemented specialized path for <100 element datasets.
Avoids allocating filter vector for trivial queries.

- Added fast path detection in where()
- Added benchmark test showing 3-5x speedup
- Updated API_REFERENCE.md with performance notes

Fixes #45
```

### Pull Request Checklist

- [ ] Code follows style guide
- [ ] All tests pass
- [ ] Valgrind reports zero leaks
- [ ] Documentation updated
- [ ] Commit message is descriptive
- [ ] No breaking changes (or well-justified)

---

## Performance Optimization Guidelines

### Profiling

```bash
# CPU profiling
perf record -g ./comparative_benchmark
perf report

# Memory profiling
valgrind --tool=massif ./program
ms_print massif.out.* | less
```

### Optimization Checklist

- [ ] Use `-O3 -march=native` for release builds
- [ ] Enable SIMD when applicable
- [ ] Consider parallelization for >10K elements
- [ ] Use move semantics for heavy objects
- [ ] Avoid unnecessary copies in hot paths
- [ ] Profile before and after optimization

### Code Review Feedback

Common areas of review:

1. **Memory safety**: Are resources properly managed?
2. **Performance**: Does it regress existing benchmarks?
3. **Thread safety**: Is the code thread-safe?
4. **Documentation**: Are changes documented?
5. **Testing**: Are edge cases covered?
6. **Style**: Does it follow conventions?

---

## Issue Guidelines

### Reporting Bugs

**Title**: "[BUG] Brief description"

**Content**:
```markdown
## Description
Clear description of the bug

## Steps to Reproduce
1. Step 1
2. Step 2
3. ...

## Expected Behavior
What should happen

## Actual Behavior
What actually happened

## Environment
- Compiler: Clang X.X.X
- OS: Linux/Mac/Windows
- Version: X.X.X

## Minimal Reproducible Example
```cpp
// Code that demonstrates the bug
```
```

### Feature Requests

**Title**: "[FEATURE] Brief description"

**Content**:
```markdown
## Description
What feature is needed and why

## Use Case
Example of how this feature would be used

## Proposed Solution
How this could be implemented (if known)

## Alternatives
Other approaches that were considered
```

---

## Development Workflow

### Creating a Feature Branch

```bash
git checkout -b feature/my-feature
git commit -m "feat(scope): Add my feature"
git push origin feature/my-feature
```

### Keep Fork Updated

```bash
git remote add upstream https://github.com/oldboldpilot/fastestjsoninthewest.git
git fetch upstream
git rebase upstream/main
git push --force-with-lease origin main
```

### Squash Commits Before PR

```bash
# Squash last 3 commits
git rebase -i HEAD~3

# Mark commits as 'squash' (s) except first one
# Save and resolve any conflicts
git push --force-with-lease origin feature/my-feature
```

---

## Resources

### Documentation
- [API Reference](./API_REFERENCE.md)
- [LINQ Implementation](./LINQ_IMPLEMENTATION.md)
- [Architecture Guide](../docs/ARCHITECTURE.md)

### External Resources
- [C++23 Features](https://en.cppreference.com/w/cpp/compiler_support/23)
- [OpenMP Documentation](https://www.openmp.org/)
- [Valgrind Manual](https://valgrind.org/docs/manual/)
- [Git Workflow](https://git-scm.com/book/en/v2/Git-Branching-Branching-Workflow)

---

## Getting Help

- **Documentation**: Check the guides above
- **Examples**: Browse `examples/` directory
- **GitHub Issues**: Search existing issues
- **Discussions**: Use GitHub Discussions tab
- **Code Review**: Ask in PR comments

---

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

**Thank you for contributing to FastestJSONInTheWest! 🚀**

