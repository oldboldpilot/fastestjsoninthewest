#include "modules/json_linq.h"
#include <iostream>
using namespace fastjson::linq;

int main() {
    std::vector<int> numbers = {2, 3, 4, 5, 6};
    auto result = from_parallel(numbers)
        .prefix_sum([](int a, int b) { return a * b; })
        .to_vector();
    
    std::cout << "Input: ";
    for (auto n : numbers) std::cout << n << " ";
    std::cout << "\nResult: ";
    for (auto n : result) std::cout << n << " ";
    std::cout << "\nExpected: 2 6 24 120 720" << std::endl;
    
    return 0;
}
