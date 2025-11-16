#include <iostream>
#include <immintrin.h>

int main() {
    std::cout << "=== SIMD Feature Detection Test ===" << std::endl;
    
#ifdef __AVX512BW__
    std::cout << "✓ __AVX512BW__ defined" << std::endl;
#else
    std::cout << "✗ __AVX512BW__ not defined" << std::endl;
#endif

#ifdef __AVX2__
    std::cout << "✓ __AVX2__ defined" << std::endl;
#else
    std::cout << "✗ __AVX2__ not defined" << std::endl;
#endif

#ifdef __SSE2__
    std::cout << "✓ __SSE2__ defined" << std::endl;
#else
    std::cout << "✗ __SSE2__ not defined" << std::endl;
#endif

    // Runtime detection
    std::cout << "\n=== Runtime Detection ===" << std::endl;
    std::cout << "AVX2 support: " << (__builtin_cpu_supports("avx2") ? "Yes" : "No") << std::endl;
    std::cout << "AVX512F support: " << (__builtin_cpu_supports("avx512f") ? "Yes" : "No") << std::endl;
    std::cout << "SSE2 support: " << (__builtin_cpu_supports("sse2") ? "Yes" : "No") << std::endl;
    
    return 0;
}