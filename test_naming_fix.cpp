#include "build_cpp17/fastjson_final.hpp"

int main() {
    fastjson17::simd_capabilities caps;
    std::cout << "AVX2: " << caps.has_avx2() << std::endl;
    std::cout << "AVX512-F: " << caps.has_avx512_f() << std::endl; 
    std::cout << "AVX512-VNNI: " << caps.has_avx512_vnni() << std::endl;
    return 0;
}
