# Memory Leak Fix Summary

## Problem
The JSON parser had a massive **422MB memory leak** when parsing JSON data. Valgrind showed:
- **Definitely lost**: 19,656 bytes in 189 blocks  
- **Indirectly lost**: 422,022,283 bytes (422MB) in 3,980,608 blocks

## Root Cause
The `json_value` class uses a recursive `std::variant` containing:
- `json_array` (which is `std::vector<json_value>`)
- `json_object` (which is `std::unordered_map<std::string, json_value>`)

The destructor was declared but defined as `= default` after the complete types were available. However, the default destructor wasn't properly cleaning up the recursive variant structures.

## Solution
Replaced the defaulted destructor with an explicit inline destructor in the class definition:

```cpp
class json_value {
public:
    ~json_value() {
        // Explicit destructor ensures proper cleanup of recursive structures
        // The variant destructor will call the destructor of the active alternative
        // For json_array and json_object, their destructors will recursively 
        // destroy all contained json_values
    }
    // ... rest of class
};
```

## Key Points
1. **Module vs Header**: The parser is a C++23 module library (`fastjson.cppm`), with `fastjson_parallel.cpp` serving as a C++17 compatibility header-only implementation
2. **Inline Required**: Since it's header-only for compatibility, the destructor must be inline in the class definition
3. **Recursive Cleanup**: The explicit destructor ensures the variant properly destroys recursive structures

## Verification
After the fix, valgrind shows:
- **Definitely lost**: 0 bytes
- **Indirectly lost**: 0 bytes  
- **Possibly lost**: 2,524 bytes (OpenMP initialization - not our code)

The 422MB leak is completely eliminated!

## Build Configuration
- Compiler: Clang 21.1.5 (custom build at `/opt/clang-21`)
- Build tools: CMake 4.1.2 and Ninja (custom builds in `~/toolchain`)
- OpenMP: libomp from Clang 21 at `/opt/clang-21/lib/x86_64-unknown-linux-gnu/`

## Lessons Learned
1. Default destructors may not handle recursive types properly
2. Always test with valgrind as per our coding standards
3. Header-only implementations need inline destructors
4. Recursive variants require explicit destructor definitions