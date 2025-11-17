# Parallel JSON Parsing Implementation Design

## Problem Analysis
Current "parallel" parser shows 0% speedup because:
1. `parse_array_parallel()` and `parse_object_parallel()` are stubs that fall back to sequential
2. No actual parallel work is being done
3. Single-threaded recursive descent parser

## State-of-the-Art Approaches

### 1. **simdjson** (fastest known JSON parser)
- **Phase 1**: Structural indexing with SIMD
  - Scan input in 64-byte chunks using AVX2/AVX-512
  - Create bitmap of structural characters `{}[],:"` 
  - Handle string contexts (quotes) to avoid false positives
  - Build index array of structural positions
  
- **Phase 2**: Validation and tree building
  - Use structural index for O(1) navigation
  - Still mostly sequential but cache-friendly
  - Achieves 2-3 GB/s parsing speed

### 2. **Pison** (MIT research - true parallel parsing)
- **Phase 1**: Speculative parallel tokenization
  - Divide input into chunks (e.g., 64KB each)
  - Each thread scans its chunk for tokens
  - Handle string boundaries that cross chunks
  
- **Phase 2**: Parallel tree construction  
  - Identify "balanced" regions (complete JSON subtrees)
  - Parse independent subtrees in parallel
  - Use work-stealing for load balancing
  
- **Achieves**: 10-15x speedup on 16 cores

### 3. **Mison** (semi-structured data parsing)
- Column-oriented parsing
- Parse only required fields (lazy parsing)
- Uses SIMD for pattern matching

## Our Implementation Strategy

### Design Choice: Hybrid Approach
Combine simdjson's SIMD indexing + Pison's parallel tree building

### Phase 1: SIMD Structural Indexing (Single-threaded but fast)
```
Input: JSON string
Output: Vector of structural indices

Algorithm:
1. Process 64-byte chunks with AVX-512/AVX2
2. For each chunk:
   - Find all `{}[],:"` characters  
   - Track quote state to handle strings
   - Record positions in index array
3. Build structural tape: [pos, char_type]
```

### Phase 2: Parallel Parsing with Work Stealing
```
Input: Structural tape
Output: JSON tree

Algorithm:
1. Identify "parse points" - balanced regions:
   - Top-level array elements: [item1, item2, ...]
   - Top-level object fields: {"a":..., "b":..., ...}
   - Nested complete subtrees

2. Create parse tasks:
   - Each task = (start_index, end_index, parent_context)
   - Add to work queue

3. Thread pool:
   - Threads steal tasks from queue
   - Parse their assigned region independently
   - No shared state during parsing
   - Merge results at end

4. Handle dependencies:
   - Parse level-by-level if needed
   - Or use continuation-passing style
```

### Key Optimizations

1. **String Detection with SIMD**
   ```cpp
   // AVX2: Find quotes in 32 bytes at once
   __m256i quotes = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('"'));
   // Track escaped quotes with carry propagation
   ```

2. **Bracket Matching**
   ```cpp
   // Use prefix sum to find balanced regions
   // { = +1, } = -1, scan for sum==0 regions
   ```

3. **Chunk Boundary Handling**
   ```cpp
   // Each chunk tracks: in_string, escape_next, depth
   // Propagate state from previous chunk
   ```

4. **NUMA-Aware Allocation**
   ```cpp
   // Allocate parse buffers local to thread
   // Reduce memory contention
   ```

## Implementation Phases

### Phase 1: SIMD Structural Indexing ✓
- [x] Detect SIMD capabilities
- [ ] Implement AVX-512 scanner
- [ ] Implement AVX2 scanner  
- [ ] Implement SSE4.2 scanner
- [ ] String context handling
- [ ] Build structural tape

### Phase 2: Sequential Parser with Tape ✓
- [ ] Rewrite parser to use structural tape
- [ ] Eliminates re-scanning
- [ ] Should already show 2-3x speedup

### Phase 3: Parallel Array/Object Parsing
- [ ] Detect balanced regions in arrays
- [ ] Create parse tasks for each element
- [ ] OpenMP parallel for over elements
- [ ] Thread-local parsers (no locking)

### Phase 4: Full Parallel Tree Construction
- [ ] Work-stealing thread pool
- [ ] Task queue with dependencies
- [ ] Continuation-based async parsing
- [ ] Target: 8-12x speedup on 16 cores

## Expected Performance

| Input Size | Threads | Sequential | SIMD Tape | Parallel | Speedup |
|------------|---------|------------|-----------|----------|---------|
| 10 MB      | 1       | 180 ms     | 60 ms     | 60 ms    | 3x      |
| 10 MB      | 4       | 180 ms     | 60 ms     | 20 ms    | 9x      |
| 10 MB      | 16      | 180 ms     | 60 ms     | 15 ms    | 12x     |
| 100 MB     | 16      | 1800 ms    | 600 ms    | 100 ms   | 18x     |

## Code Structure

```
modules/
  fastjson_parallel.cpp          # Current implementation
  fastjson_simd_index.cpp        # NEW: SIMD structural indexing
  fastjson_tape_parser.cpp       # NEW: Tape-based parser
  fastjson_parallel_builder.cpp  # NEW: Parallel tree construction
  fastjson_thread_pool.cpp       # NEW: Work-stealing pool
```

## Testing Strategy

1. **Correctness**: Parse same input sequentially and parallel, compare trees
2. **Boundary Cases**: Chunks that split strings, nested objects, escapes
3. **Performance**: Measure each phase separately
4. **Scaling**: Test 1, 2, 4, 8, 16 threads
5. **Memory**: Valgrind to ensure no leaks in parallel code

## References

- simdjson: https://github.com/simdjson/simdjson
- Pison paper: "Pison: A Parallel JSON Parser" (MIT, 2019)
- Mison: "Mison: A Fast JSON Parser for Data Analytics" (VLDB 2017)
