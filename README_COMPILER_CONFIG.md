# 🔴 CRITICAL: Compiler Configuration - DO NOT IGNORE!

**This file exists because we lost hours debugging due to wrong compiler paths.**

## The ONE Thing You Must Remember

```
COMPILER: /opt/clang-21/bin/clang++
NEVER:    /home/linuxbrew/.linuxbrew/bin/clang++  ❌ BROKEN!
```

## Quick Reference Card

| Tool | Correct Path | Version |
|------|-------------|---------|
| C++ Compiler | `/opt/clang-21/bin/clang++` | 21.1.5 |
| C Compiler | `/opt/clang-21/bin/clang` | 21.1.5 |
| Ninja | `/usr/local/bin/ninja` | 1.13.1 |
| CMake | `/usr/bin/cmake` | 3.28.3 |

## Standard Build Recipe

Copy-paste this every time:

```bash
cd /home/muyiwa/Development/FastestJSONInTheWest/build_tests
rm -rf *
/usr/bin/cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/opt/clang-21/bin/clang++ \
  -DCMAKE_C_COMPILER=/opt/clang-21/bin/clang \
  -DCMAKE_MAKE_PROGRAM=/usr/local/bin/ninja \
  ../tests
/usr/local/bin/ninja all_tests
```

## Why This Exists

**Problem History:**
1. Initial build used Linuxbrew Clang
2. Linuxbrew installation was incomplete
3. Got errors: `fatal error: 'stddef.h' file not found`
4. Spent hours debugging
5. Root cause: Wrong compiler with missing headers

**Solution:**
- Built Clang 21 from source at `/opt/clang-21`
- Built Ninja from source at `/usr/local/bin/ninja`
- Created THIS file so we never forget again

## Verification (Run These First!)

```bash
# 1. Check Clang works
/opt/clang-21/bin/clang++ --version
# Expected: clang version 21.1.5

# 2. Check Ninja works
/usr/local/bin/ninja --version
# Expected: 1.13.1

# 3. Check headers exist
ls /opt/clang-21/lib/clang/21/include/stddef.h
# Expected: File exists

# If ANY of these fail, DO NOT proceed with build!
```

## More Information

- **Full Documentation:** See `.ai_agent_readme.md`
- **Coding Standards:** See `docs/CODING_STANDARDS.md`
- **Session State:** See `.project_session_state.md`
- **LLVM Build:** See `.llvm_build_session.md`

## Current LLVM Build Status

**Active:** Full feature LLVM build running in background
**Process:** 238689
**Progress:** ~25% complete (1863/7449 targets)
**Check:** `tail -f /home/muyiwa/toolchain-build/llvm-full-build/build.log`

After completion: `sudo ninja install` to update `/opt/clang-21`

---

**Remember:** If you get "file not found" errors for system headers,
you're using the wrong compiler. Use `/opt/clang-21/bin/clang++`!
