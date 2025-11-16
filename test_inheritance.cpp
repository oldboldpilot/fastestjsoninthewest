#include "build_cpp17/fastjson_inheritance.hpp"
#include <iostream>

int main() {
    fastjson17::json_value json = fastjson17::json_object{{"hello", fastjson17::json_string{"world"}}};
    
    // Test polymorphic interface
    auto result = json.to_string();
    std::cout << "JSON: " << result << std::endl;
    
    // Test SIMD capabilities 
    auto caps = fastjson17::simd_json_parser::get_capabilities();
    std::cout << "AVX2: " << caps.has_avx2() << std::endl;
    std::cout << "AVX512-F: " << caps.has_avx512_f() << std::endl;
    
    return 0;
}
