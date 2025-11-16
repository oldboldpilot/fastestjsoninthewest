#include "build_cpp17/fastjson_final.hpp"
#include <iostream>

int main() {
    // Test JSON creation and SIMD features
    fastjson17::json_value json = std::string("test");
    std::cout << "Basic JSON test passed" << std::endl;
    
    // Test SIMD capabilities
    std::cout << "AVX2: " << fastjson17::simd_capabilities::has_avx2() << std::endl;
    std::cout << "AVX512-F: " << fastjson17::simd_capabilities::has_avx512_f() << std::endl;
    std::cout << "AVX512-VNNI: " << fastjson17::simd_capabilities::has_avx512_vnni() << std::endl;
    
    return 0;
}
