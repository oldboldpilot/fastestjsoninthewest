#!/bin/bash
# Post-Reboot Recovery Script for Clang 21.1.5 Build
# Author: Olumuyiwa Oluwasanmi
# Created: November 16, 2025

echo "=== Clang 21.1.5 Build Recovery Script ==="
echo "Date: $(date)"
echo

# Check if build directory exists
if [ -d "/home/muyiwa/toolchain-build/llvm-build" ]; then
    echo "✓ Build directory found: /home/muyiwa/toolchain-build/llvm-build"
    cd /home/muyiwa/toolchain-build/llvm-build
    
    echo "Build files status:"
    ls -la | head -10
    echo
    
    echo "=== Attempting to diagnose build failure ==="
    echo "Checking for specific compilation error..."
    
    # Try to get detailed error information
    echo "Running ninja with verbose output to see the exact error:"
    echo "ninja -v 2>&1 | head -20"
    echo
    
    # Suggest recovery options
    echo "=== Recovery Options ==="
    echo "1. Single-threaded build:     ninja -j1"
    echo "2. Reduced parallelism:       ninja -j2"
    echo "3. Continue current build:    ninja"
    echo "4. Clean and rebuild without MLIR"
    echo "5. Use system LLVM/Clang packages instead"
    echo
    
    echo "Previous issue: Build stuck on SPIRVOpDefinition.cpp.o"
    echo "Likely solutions:"
    echo "  - Reduce memory usage with -j1"
    echo "  - Exclude MLIR component"
    echo "  - Check system memory: free -h"
    echo
    
else
    echo "⚠️  Build directory not found. Need to restart from scratch."
    echo "Run: /home/muyiwa/Development/FastestJSONInTheWest/clang21_rebuild_config.sh"
fi

echo "Session state saved in: /home/muyiwa/Development/FastestJSONInTheWest/SESSION_STATE.md"
echo "Auto-monitor script: /home/muyiwa/Development/FastestJSONInTheWest/clang21_auto_monitor.sh"
echo