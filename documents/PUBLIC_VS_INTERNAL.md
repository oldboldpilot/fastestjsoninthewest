# Repository Structure: Public vs. Internal

## Quick Reference

### 🟢 PUBLIC (Safe to export to public repository)

```
FastestJSONInTheWest/
├── 📁 modules/                    ✅ CORE LIBRARY
│   ├── fastjson.cppm             Public: Main parsing module
│   ├── json_linq.h               Public: LINQ queries
│   ├── unicode.h                 Public: Unicode support
│   ├── json_parallel.h           Public: Parallel ops
│   └── *.h                        Public: Other headers
│
├── 📁 documents/                  ✅ PUBLIC DOCUMENTATION
│   ├── README.md                 Public: Doc index
│   ├── API_REFERENCE.md          Public: API guide
│   ├── LINQ_IMPLEMENTATION.md    Public: LINQ guide
│   ├── CONTRIBUTION_GUIDE.md     Public: For contributors
│   └── BENCHMARK_RESULTS.md      Public: Performance data
│
├── 📁 examples/                   ✅ USAGE EXAMPLES
│   ├── *.cpp                     Public: Example programs
│   └── *.md                      Public: Example docs
│
├── 📁 benchmarks/                 ✅ PUBLIC BENCHMARKS
│   ├── comparative_benchmark.cpp Public: Performance tests
│   └── *.cpp                     Public: Benchmark code
│
├── 📄 LICENSE_BSD_4_CLAUSE       ✅ PUBLIC LICENSE
├── 📄 README.md                  ✅ PUBLIC README
├── 📄 CMakeLists.txt             ✅ PUBLIC BUILD CONFIG
└── 📄 .gitignore                 ✅ PUBLIC GIT CONFIG
```

### 🔴 INTERNAL (Should NOT be in public repo)

```
FastestJSONInTheWest/
├── 📁 ai/                         ❌ INTERNAL
│   ├── AGENT_GUIDELINES.md       Internal: Agent instructions
│   ├── CLAUDE.md                 Internal: Development notes
│   └── coding_standards.md       Internal: Team standards
│
├── 📁 docs/                       ⚠️  PARTIALLY INTERNAL
│   ├── ARCHITECTURE.md           Can be public (technical deep-dive)
│   ├── FUTURE_WORK_STATUS.md     Internal: Development tracking
│   ├── *_IMPLEMENTATION.md       Internal: Implementation details
│   ├── MEMORY_LEAK_FIX.md        Internal: Bug fixes
│   ├── MVP.md                    Internal: MVP planning
│   ├── TEST_ORGANIZATION_POLICY.md Internal: Testing procedures
│   ├── VALGRIND_ANALYSIS_REPORT.md Can be public (performance data)
│   └── ...                       Internal: Development docs
│
├── 📁 tests/                      ⚠️  PARTIALLY PUBLIC
│   ├── *_test.cpp               Public: Unit tests
│   ├── benchmark/                Public: Benchmarks
│   └── internal/                 Internal: Internal test utils
│
├── 📁 tools/                      ⚠️  PARTIALLY PUBLIC
│   ├── generate_test_file.cpp   Public: Test data generator
│   ├── valgrind_*.sh            Internal: Testing scripts
│   └── *.py                     Internal: Build scripts
│
├── 📁 build/                      ❌ NEVER PUBLIC
├── 📁 build_*/                    ❌ NEVER PUBLIC
├── 📁 .vscode/                    ❌ NEVER PUBLIC
├── 📁 .ansible/                   ❌ NEVER PUBLIC
├── 📄 .llvm_build_state.md       ❌ NEVER PUBLIC
├── 📄 .session_state.md.backup   ❌ NEVER PUBLIC
├── 📄 valgrind-openmp.supp       Internal: Valgrind config
├── 📄 openmp_scaling_results.json Internal: Benchmark data
└── Various binary executables    ❌ NEVER PUBLIC
```

---

## Decision Matrix

| File/Folder | Public? | Reason | Export to Public Repo? |
|-------------|---------|--------|------------------------|
| `modules/` | ✅ YES | Core library code | ✅ YES |
| `documents/` | ✅ YES | User documentation | ✅ YES |
| `examples/` | ✅ YES | Usage examples | ✅ YES |
| `benchmarks/` | ✅ YES | Performance tests | ✅ YES |
| `tests/` | ✅ YES | Unit tests | ✅ YES |
| `LICENSE_BSD_4_CLAUSE` | ✅ YES | License | ✅ YES |
| `README.md` | ✅ YES | Project overview | ✅ YES |
| `CMakeLists.txt` | ✅ YES | Build system | ✅ YES |
| `ai/` | ❌ NO | Internal guidelines | ❌ NO |
| `docs/` (most) | ⚠️ SELECTIVE | Development docs | Only ARCHITECTURE.md, BENCHMARK_RESULTS.md |
| `.vscode/` | ❌ NO | Editor config | ❌ NO |
| `build/` | ❌ NO | Build artifacts | ❌ NO |
| Binary files | ❌ NO | Executables | ❌ NO |
| `.session_state.md*` | ❌ NO | Session tracking | ❌ NO |

---

## How to Create Public Repository

### Option 1: Git Subtree (Clean Export)

```bash
# Create new public repo
mkdir FastestJSONInTheWest-Public
cd FastestJSONInTheWest-Public
git init

# Copy only public files
cp -r ../FastestJSONInTheWest/modules .
cp -r ../FastestJSONInTheWest/documents .
cp -r ../FastestJSONInTheWest/examples .
cp -r ../FastestJSONInTheWest/benchmarks .
cp -r ../FastestJSONInTheWest/tests .
cp ../FastestJSONInTheWest/README.md .
cp ../FastestJSONInTheWest/CMakeLists.txt .
cp ../FastestJSONInTheWest/LICENSE_BSD_4_CLAUSE .
cp ../FastestJSONInTheWest/.gitignore .

git add .
git commit -m "Initial commit: FastestJSONInTheWest"
git remote add origin https://github.com/oldboldpilot/fastestjsoninthewest-public.git
git push -u origin main
```

### Option 2: Git Filter-Branch (For Existing Repo)

```bash
# Clone dev repo
git clone <dev-repo> fjitw-public

# Filter out internal folders
git filter-branch --tree-filter '
  rm -rf ai/
  rm -rf docs/
  rm -rf .vscode/
  rm -rf build*/
  rm -f .session_state*
  rm -f .llvm_build_state.md
' -- --all

# Push to public repository
git push origin main
```

### Option 3: Manual .gitignore Approach

Create `.gitignore_public`:
```
# Build
build/
build_*/
CMakeFiles/
*.o
*.a
*.so

# IDE
.vscode/
.idea/
.vs/

# Internal
ai/
docs/
.session_state*
.llvm_build_state.md
valgrind_results/
*.supp

# Binaries
*.exe
*.out
benchmark
prefix_sum_test
utf8_test
```

---

## Public Folder Contents Checklist

### ✅ Include These

- [x] `modules/` - Core library
- [x] `examples/` - Usage examples
- [x] `documents/` - Public documentation
- [x] `benchmarks/` - Performance tests
- [x] `tests/` - Unit tests
- [x] `README.md` - Project overview
- [x] `CMakeLists.txt` - Build instructions
- [x] `LICENSE_BSD_4_CLAUSE` - License
- [x] `.gitignore` - Git ignore rules
- [x] `.clang-format` - Code formatting

### ❌ Exclude These

- [ ] `ai/` - Internal agent guidelines
- [ ] `docs/` (most files) - Internal development docs
- [ ] `.vscode/` - Editor configuration
- [ ] `build/` - Build artifacts
- [ ] Binary executables
- [ ] `.session_state*` - Session tracking
- [ ] `.llvm_build_state.md` - Build state
- [ ] `valgrind_results/` - Testing artifacts
- [ ] `openmp_scaling_results.json` - Internal metrics

---

## Current Public Content

### Ready for Public Export:

**Core Library:**
- ✅ `modules/fastjson.cppm` - Main parser
- ✅ `modules/json_linq.h` - LINQ interface
- ✅ `modules/unicode.h` - Unicode support
- ✅ `modules/json_parallel.h` - Parallel ops

**Documentation:**
- ✅ `documents/API_REFERENCE.md` - API guide
- ✅ `documents/LINQ_IMPLEMENTATION.md` - LINQ guide
- ✅ `documents/CONTRIBUTION_GUIDE.md` - Contributing
- ✅ `documents/README.md` - Doc index

**Examples & Tests:**
- ✅ `examples/` - All example code
- ✅ `tests/` - All unit tests
- ✅ `benchmarks/comparative_benchmark.cpp` - Performance tests

**Configuration:**
- ✅ `README.md` - Project overview
- ✅ `CMakeLists.txt` - Build system
- ✅ `LICENSE_BSD_4_CLAUSE` - License

---

## Summary

| Category | Public? | Confidence |
|----------|---------|------------|
| Core library (`modules/`) | ✅ 100% | Safe to share |
| Documentation (`documents/`) | ✅ 100% | Safe to share |
| Examples | ✅ 100% | Safe to share |
| Tests | ✅ 95% | Safe (exclude internal utils) |
| Benchmarks | ✅ 100% | Safe to share |
| Internal docs (`ai/`, most of `docs/`) | ❌ 0% | Keep private |
| Build artifacts | ❌ 0% | Never share |

---

**Bottom Line:** You can safely export `modules/`, `documents/`, `examples/`, `benchmarks/`, and `tests/` to a public repository. Everything in `ai/` and most of `docs/` should remain private.

