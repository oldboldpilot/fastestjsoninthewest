#!/bin/bash

# Comprehensive Valgrind test script for FastestJSONInTheWest
# Tests: memory leaks, thread safety (Helgrind/DRD), cache performance (Cachegrind)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build_valgrind"
RESULTS_DIR="$PROJECT_ROOT/valgrind_results"
SUPPRESSION_FILE="$PROJECT_ROOT/valgrind-openmp.supp"

echo "=============================================================
    Comprehensive Valgrind Test Suite
=============================================================
"

# Create directories
mkdir -p "$BUILD_DIR"
mkdir -p "$RESULTS_DIR"

# Check if Valgrind is installed
if ! command -v valgrind &> /dev/null; then
    echo -e "${RED}ERROR: Valgrind is not installed${NC}"
    echo "Install with: sudo apt-get install valgrind"
    exit 1
fi

echo "Valgrind version:"
valgrind --version
echo ""

# Function to compile with debug symbols
compile_for_valgrind() {
    local source=$1
    local output=$2
    local name=$(basename "$source" .cpp)
    
    echo -e "${YELLOW}Compiling $name for Valgrind...${NC}"
    
    /opt/clang-21/bin/clang++ \
        -std=c++23 \
        -O0 \
        -g3 \
        -fno-omit-frame-pointer \
        -fno-optimize-sibling-calls \
        -march=native \
        -fopenmp \
        -I"$PROJECT_ROOT" \
        "$source" \
        "$BUILD_DIR/numa_allocator.o" \
        -o "$output" \
        -ldl -lpthread
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Compiled successfully${NC}"
        return 0
    else
        echo -e "${RED}✗ Compilation failed${NC}"
        return 1
    fi
}

# Function to run Memcheck (memory leak detection)
run_memcheck() {
    local executable=$1
    local name=$(basename "$executable")
    local log_file="$RESULTS_DIR/${name}_memcheck.log"
    
    echo -e "\n${YELLOW}Running Memcheck on $name...${NC}"
    
    LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH \
    valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --verbose \
        --log-file="$log_file" \
        --suppressions="$SUPPRESSION_FILE" \
        --error-exitcode=1 \
        "$executable" 2>&1 | head -50
    
    local exit_code=$?
    
    # Parse results
    local definitely_lost=$(grep "definitely lost:" "$log_file" | awk '{print $4}')
    local indirectly_lost=$(grep "indirectly lost:" "$log_file" | awk '{print $4}')
    local possibly_lost=$(grep "possibly lost:" "$log_file" | awk '{print $4}')
    local errors=$(grep "ERROR SUMMARY:" "$log_file" | awk '{print $4}')
    
    echo ""
    echo "Memory Leak Summary:"
    echo "  Definitely lost:  ${definitely_lost:-0} bytes"
    echo "  Indirectly lost:  ${indirectly_lost:-0} bytes"
    echo "  Possibly lost:    ${possibly_lost:-0} bytes"
    echo "  Total errors:     ${errors:-0}"
    echo "  Full log: $log_file"
    
    if [ "$exit_code" -eq 0 ] && [ "${definitely_lost:-0}" -eq 0 ]; then
        echo -e "${GREEN}✓ PASSED: No memory leaks detected${NC}"
        return 0
    else
        echo -e "${RED}✗ FAILED: Memory issues detected${NC}"
        return 1
    fi
}

# Function to run Helgrind (thread safety detection)
run_helgrind() {
    local executable=$1
    local name=$(basename "$executable")
    local log_file="$RESULTS_DIR/${name}_helgrind.log"
    
    echo -e "\n${YELLOW}Running Helgrind on $name...${NC}"
    
    LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH \
    valgrind \
        --tool=helgrind \
        --verbose \
        --log-file="$log_file" \
        --suppressions="$SUPPRESSION_FILE" \
        --error-exitcode=1 \
        "$executable" 2>&1 | head -50
    
    local exit_code=$?
    
    # Parse results
    local errors=$(grep "ERROR SUMMARY:" "$log_file" | awk '{print $4}')
    local data_races=$(grep -c "Possible data race" "$log_file" || echo "0")
    
    echo ""
    echo "Thread Safety Summary:"
    echo "  Total errors:       ${errors:-0}"
    echo "  Data races:         $data_races"
    echo "  Full log: $log_file"
    
    if [ "$exit_code" -eq 0 ] && [ "$data_races" -eq 0 ]; then
        echo -e "${GREEN}✓ PASSED: No thread safety issues${NC}"
        return 0
    else
        echo -e "${RED}✗ FAILED: Thread safety issues detected${NC}"
        return 1
    fi
}

# Function to run DRD (alternative thread error detector)
run_drd() {
    local executable=$1
    local name=$(basename "$executable")
    local log_file="$RESULTS_DIR/${name}_drd.log"
    
    echo -e "\n${YELLOW}Running DRD on $name...${NC}"
    
    LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH \
    valgrind \
        --tool=drd \
        --verbose \
        --log-file="$log_file" \
        --suppressions="$SUPPRESSION_FILE" \
        --error-exitcode=1 \
        "$executable" 2>&1 | head -50
    
    local exit_code=$?
    
    # Parse results
    local errors=$(grep "ERROR SUMMARY:" "$log_file" | awk '{print $4}')
    
    echo ""
    echo "DRD Summary:"
    echo "  Total errors:  ${errors:-0}"
    echo "  Full log: $log_file"
    
    if [ "$exit_code" -eq 0 ]; then
        echo -e "${GREEN}✓ PASSED: No data races detected${NC}"
        return 0
    else
        echo -e "${RED}✗ FAILED: Data races detected${NC}"
        return 1
    fi
}

# Function to run Cachegrind (cache performance)
run_cachegrind() {
    local executable=$1
    local name=$(basename "$executable")
    local log_file="$RESULTS_DIR/${name}_cachegrind.log"
    
    echo -e "\n${YELLOW}Running Cachegrind on $name...${NC}"
    
    LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH \
    valgrind \
        --tool=cachegrind \
        --verbose \
        --log-file="$log_file" \
        --cachegrind-out-file="$RESULTS_DIR/${name}_cachegrind.out" \
        "$executable" > /dev/null 2>&1
    
    # Parse results with cg_annotate
    if [ -f "$RESULTS_DIR/${name}_cachegrind.out" ]; then
        cg_annotate "$RESULTS_DIR/${name}_cachegrind.out" > "$RESULTS_DIR/${name}_cachegrind_annotated.txt" 2>&1
        
        echo ""
        echo "Cache Performance Summary:"
        grep "I   refs:" "$RESULTS_DIR/${name}_cachegrind_annotated.txt" || echo "  (see full report)"
        grep "I1  misses:" "$RESULTS_DIR/${name}_cachegrind_annotated.txt" || true
        grep "LLi misses:" "$RESULTS_DIR/${name}_cachegrind_annotated.txt" || true
        grep "D   refs:" "$RESULTS_DIR/${name}_cachegrind_annotated.txt" || true
        grep "D1  misses:" "$RESULTS_DIR/${name}_cachegrind_annotated.txt" || true
        grep "LLd misses:" "$RESULTS_DIR/${name}_cachegrind_annotated.txt" || true
        echo "  Full report: $RESULTS_DIR/${name}_cachegrind_annotated.txt"
        
        echo -e "${GREEN}✓ COMPLETED: Cache analysis done${NC}"
    else
        echo -e "${RED}✗ FAILED: Cachegrind output not found${NC}"
        return 1
    fi
    
    return 0
}

# Function to run Massif (heap profiler)
run_massif() {
    local executable=$1
    local name=$(basename "$executable")
    local log_file="$RESULTS_DIR/${name}_massif.log"
    
    echo -e "\n${YELLOW}Running Massif on $name...${NC}"
    
    LD_LIBRARY_PATH=/opt/clang-21/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH \
    valgrind \
        --tool=massif \
        --verbose \
        --log-file="$log_file" \
        --massif-out-file="$RESULTS_DIR/${name}_massif.out" \
        "$executable" > /dev/null 2>&1
    
    # Parse results with ms_print
    if [ -f "$RESULTS_DIR/${name}_massif.out" ]; then
        ms_print "$RESULTS_DIR/${name}_massif.out" > "$RESULTS_DIR/${name}_massif_report.txt" 2>&1
        
        echo ""
        echo "Heap Profile Summary:"
        head -30 "$RESULTS_DIR/${name}_massif_report.txt" | tail -20
        echo "  Full report: $RESULTS_DIR/${name}_massif_report.txt"
        
        echo -e "${GREEN}✓ COMPLETED: Heap profiling done${NC}"
    else
        echo -e "${RED}✗ FAILED: Massif output not found${NC}"
        return 1
    fi
    
    return 0
}

# Compile NUMA allocator
echo -e "${YELLOW}Compiling NUMA allocator...${NC}"
/opt/clang-21/bin/clang++ \
    -std=c++23 \
    -O0 \
    -g3 \
    -fno-omit-frame-pointer \
    -march=native \
    -fopenmp \
    -I"$PROJECT_ROOT" \
    -c "$PROJECT_ROOT/modules/numa_allocator.cpp" \
    -o "$BUILD_DIR/numa_allocator.o"

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to compile NUMA allocator${NC}"
    exit 1
fi
echo -e "${GREEN}✓ NUMA allocator compiled${NC}\n"

# Test suite
declare -A test_files=(
    ["unicode_compliance_test"]="$PROJECT_ROOT/tests/unicode_compliance_test.cpp"
    ["utf8_validation_test"]="$PROJECT_ROOT/tests/utf8_validation_test.cpp"
    ["numa_test"]="$PROJECT_ROOT/tests/numa_test.cpp"
)

# Benchmark suite  
declare -A benchmark_files=(
    ["linq_benchmark"]="$PROJECT_ROOT/benchmarks/linq_benchmark.cpp"
)

# Track results
total_tests=0
passed_tests=0
failed_tests=0

# Run tests
echo "=============================================================
    Running Tests
=============================================================
"

for test_name in "${!test_files[@]}"; do
    test_source="${test_files[$test_name]}"
    test_exe="$BUILD_DIR/$test_name"
    
    if [ ! -f "$test_source" ]; then
        echo -e "${YELLOW}Skipping $test_name (source not found)${NC}\n"
        continue
    fi
    
    echo "-------------------------------------------------------------"
    echo "Testing: $test_name"
    echo "-------------------------------------------------------------"
    
    # Compile
    if ! compile_for_valgrind "$test_source" "$test_exe"; then
        echo -e "${RED}Skipping due to compilation failure${NC}\n"
        continue
    fi
    
    # Run Memcheck
    total_tests=$((total_tests + 1))
    if run_memcheck "$test_exe"; then
        passed_tests=$((passed_tests + 1))
    else
        failed_tests=$((failed_tests + 1))
    fi
    
    # Run Helgrind (only for parallel tests)
    if [[ "$test_name" == *"parallel"* ]] || [[ "$test_name" == *"numa"* ]]; then
        total_tests=$((total_tests + 1))
        if run_helgrind "$test_exe"; then
            passed_tests=$((passed_tests + 1))
        else
            failed_tests=$((failed_tests + 1))
        fi
    fi
    
    # Run Cachegrind
    total_tests=$((total_tests + 1))
    if run_cachegrind "$test_exe"; then
        passed_tests=$((passed_tests + 1))
    else
        failed_tests=$((failed_tests + 1))
    fi
    
    echo ""
done

# Run benchmarks
echo "=============================================================
    Running Benchmarks
=============================================================
"

for benchmark_name in "${!benchmark_files[@]}"; do
    benchmark_source="${benchmark_files[$benchmark_name]}"
    benchmark_exe="$BUILD_DIR/$benchmark_name"
    
    if [ ! -f "$benchmark_source" ]; then
        echo -e "${YELLOW}Skipping $benchmark_name (source not found)${NC}\n"
        continue
    fi
    
    echo "-------------------------------------------------------------"
    echo "Benchmarking: $benchmark_name"
    echo "-------------------------------------------------------------"
    
    # Compile
    if ! compile_for_valgrind "$benchmark_source" "$benchmark_exe"; then
        echo -e "${RED}Skipping due to compilation failure${NC}\n"
        continue
    fi
    
    # Run Memcheck
    total_tests=$((total_tests + 1))
    if run_memcheck "$benchmark_exe"; then
        passed_tests=$((passed_tests + 1))
    else
        failed_tests=$((failed_tests + 1))
    fi
    
    # Run Helgrind
    total_tests=$((total_tests + 1))
    if run_helgrind "$benchmark_exe"; then
        passed_tests=$((passed_tests + 1))
    else
        failed_tests=$((failed_tests + 1))
    fi
    
    # Run Massif
    total_tests=$((total_tests + 1))
    if run_massif "$benchmark_exe"; then
        passed_tests=$((passed_tests + 1))
    else
        failed_tests=$((failed_tests + 1))
    fi
    
    echo ""
done

# Final summary
echo "============================================================="
echo "    Final Results"
echo "============================================================="
echo ""
echo "Total tests:   $total_tests"
echo -e "${GREEN}Passed:        $passed_tests${NC}"
echo -e "${RED}Failed:        $failed_tests${NC}"
echo ""
echo "All results saved to: $RESULTS_DIR"
echo ""

if [ $failed_tests -eq 0 ]; then
    echo -e "${GREEN}✓ ALL TESTS PASSED${NC}"
    exit 0
else
    echo -e "${RED}✗ SOME TESTS FAILED${NC}"
    exit 1
fi
