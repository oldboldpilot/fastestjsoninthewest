# FastestJSONInTheWest - Public Documentation

Welcome to the FastestJSONInTheWest public documentation. This folder contains all the curated, production-ready documentation for developers integrating and using the library.

## ⚡ v2.0 Performance Update

The default parser now uses **4x AVX2 multi-register SIMD** (128 bytes per iteration), delivering 2-6x faster parsing with **zero API changes**. Existing code gets faster automatically.

## 📚 Documentation Index

### Getting Started

1. **[API_REFERENCE.md](./API_REFERENCE.md)** ⭐ START HERE
   - Comprehensive API documentation
   - Quick start guide
   - Complete method reference
   - Code examples for all features
   - Performance tips and best practices
   - **Best for:** Developers new to the library

### Deep Dives

2. **[JSON_OBJECT_MAPPING.md](./JSON_OBJECT_MAPPING.md)** 
   - JSON to C++ type mappings
   - `std::unordered_map<std::string, json_value>` patterns
   - Custom class mapping techniques
   - Error handling strategies
   - Performance optimization tips
   - **Best for:** Developers working with complex JSON structures

3. **[LINQ_IMPLEMENTATION.md](./LINQ_IMPLEMENTATION.md)**
   - Detailed LINQ implementation guide
   - Sequential and parallel operations
   - Advanced querying patterns
   - Real-world examples
   - Performance characteristics
   - **Best for:** Data-intensive applications

4. **[BENCHMARK_RESULTS.md](./BENCHMARK_RESULTS.md)**
   - Comparative performance analysis
   - FastestJSONInTheWest vs simdjson benchmarks
   - Performance metrics across workloads
   - Recommendations for use cases
   - **Best for:** Performance evaluation and optimization

### Architecture & Design

5. **[ARCHITECTURE.md](../docs/ARCHITECTURE.md)**
   - System design overview
   - Module structure
   - Design patterns used
   - Thread-safety guarantees
   - **Best for:** Library contributors and deep understanding

---

## 🚀 Quick Start Guide

### Installation

```bash
# Clone the repository
git clone https://github.com/oldboldpilot/fastestjsoninthewest.git
cd fastestjsoninthewest

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests
./comparative_benchmark
```

### First Program

```cpp
#include "modules/json_linq.h"
using namespace fastjson::linq;

int main() {
    // Create a LINQ query
    auto result = from({1, 2, 3, 4, 5})
        .where([](int x) { return x > 2; })
        .select([](int x) { return x * 2; })
        .to_vector();
    
    // Result: {6, 8, 10}
    for (int val : result) {
        std::cout << val << " ";  // Output: 6 8 10
    }
    return 0;
}
```

---

## 📊 Feature Overview

| Feature | Status | Documentation |
|---------|--------|-----------------|
| JSON Parsing | ✅ Production | API Reference |
| LINQ Queries | ✅ 40+ operations | LINQ_IMPLEMENTATION.md |
| Functional API | ✅ map, filter, reduce, etc. | API Reference |
| Parallel Processing | ✅ OpenMP integrated | API Reference |
| Unicode Support | ✅ UTF-8/16/32 | API Reference |
| SIMD Optimization | ✅ Auto-vectorization | Benchmarks |
| Memory Safety | ✅ Zero leaks (Valgrind verified) | Benchmarks |

---

## 🎯 Common Use Cases

### Use Case 1: Data Filtering & Transformation

```cpp
#include "modules/json_linq.h"
using namespace fastjson::linq;

struct User { int id; std::string name; int age; };

auto filtered_users = from(all_users)
    .where([](const User& u) { return u.age > 18; })
    .order_by([](const User& u) { return u.name; })
    .select([](const User& u) { return u.name; })
    .to_vector();
```

**See:** API_REFERENCE.md - Filtering and Sorting sections

### Use Case 2: Parallel Data Processing

```cpp
auto result = parallel_from(large_dataset)
    .parallel_where([](int x) { return expensive_check(x); })
    .parallel_select([](int x) { return process(x); })
    .parallel_sum([](int x) { return x; });
```

**See:** API_REFERENCE.md - Parallel Operations section

### Use Case 3: Complex Analytics

```cpp
auto analysis = from(events)
    .group_by([](const Event& e) { return e.category; })
    // Group by category, then analyze each group
    .to_vector();
```

**See:** LINQ_IMPLEMENTATION.md - Advanced Patterns

### Use Case 4: JSON Data Queries

```cpp
auto json = fastjson::Parser::parse(json_string);
if (json && json->is_array()) {
    auto items = json->as_array();
    auto filtered = from(items)
        .where([](const fastjson::json& j) { 
            return j["status"].as_string() == "active"; 
        })
        .to_vector();
}
```

**See:** API_REFERENCE.md - Core API section

---

## 💡 Performance Insights

### Benchmark Highlights

- **Parsing**: 9-16% faster than simdjson on all workloads
- **Queries**: 2-8x faster than simdjson with LINQ interface
- **Parallel**: 8-12x speedup with OpenMP on large datasets
- **Memory**: Zero definite leaks (Valgrind verified)

**Full Results:** See [BENCHMARK_RESULTS.md](./BENCHMARK_RESULTS.md)

### Performance Recommendations

1. Use **parallel operations** for datasets > 10K elements
2. **Chain operations** to avoid intermediate copies
3. Use **SIMD-aware** compilation flags (-march=native)
4. Prefer **LINQ** over manual event-driven parsing

**Details:** See API_REFERENCE.md - Performance Tips

---

## 🔧 Requirements

| Requirement | Version | Notes |
|-------------|---------|-------|
| Compiler | Clang 21+ | C++23 support required |
| C++ Standard | C++23 | Modules support required |
| OpenMP | 5.0+ | For parallel operations |
| CMake | 3.28+ | For building |

---

## 🐛 Troubleshooting

### Common Issues

**Q: Compilation fails with C++23 errors**
- Ensure you're using Clang 21 or later
- Check compiler path in CMakeLists.txt
- Verify `-std=c++23` flag is set

**Q: Parallel operations not showing speedup**
- Ensure dataset size > 10K elements
- Check thread count: `omp_get_num_threads()`
- Profile with Valgrind: `valgrind --tool=helgrind`

**Q: Memory usage is high**
- Use parallel operations for processing in batches
- Consider using `take()` and `skip()` for large datasets
- Profile with Valgrind: `valgrind --tool=massif`

**For more issues:** See Architecture.md or file an issue on GitHub

---

## 📖 Learning Path

### Beginner (30 minutes)
1. Read "Quick Start" in API_REFERENCE.md
2. Try first program example
3. Explore basic filtering/selection

### Intermediate (2 hours)
1. Complete API_REFERENCE.md
2. Try examples in examples/ directory
3. Experiment with chained operations

### Advanced (1 day)
1. Study LINQ_IMPLEMENTATION.md
2. Review BENCHMARK_RESULTS.md
3. Explore parallel operations
4. Contribute optimizations

---

## 🤝 Contributing

Contributions are welcome! Please see CONTRIBUTION_GUIDE.md for:
- Code style guide
- Testing requirements
- Benchmarking process
- Pull request procedure

---

## 📄 License

MIT License - See LICENSE file in root directory

---

## 🔗 Quick Links

- 📖 **Full API Reference**: [API_REFERENCE.md](./API_REFERENCE.md)
- 📊 **Benchmarks**: [BENCHMARK_RESULTS.md](./BENCHMARK_RESULTS.md)
- 🔧 **LINQ Guide**: [LINQ_IMPLEMENTATION.md](./LINQ_IMPLEMENTATION.md)
- 🏗️ **Architecture**: [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md)
- 💻 **Examples**: [../examples/](../examples/)
- 📝 **Contributing**: [CONTRIBUTION_GUIDE.md](./CONTRIBUTION_GUIDE.md)

---

## 📞 Support

- **GitHub Issues**: Report bugs and request features
- **Documentation**: Check the guides above
- **Examples**: Browse examples/ directory
- **Discussions**: GitHub Discussions tab

---

**Last Updated:** November 17, 2025  
**Version:** 1.0.0  
**Status:** Production Ready ✅

