// Test UTF-8 validation on 4-byte sequences
#include <iomanip>
#include <iostream>
#include <span>

#include "../modules/fastjson_parallel.cpp"

int main() {
    // Musical note: F0 9D 84 9E
    std::string test1 = "\xF0\x9D\x84\x9E";

    std::cout << "Testing 4-byte UTF-8: ";
    for (unsigned char c : test1) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << "\n";

    bool valid = fastjson::validate_utf8_scalar(std::span<const char>(test1.data(), test1.size()),
                                                0, test1.size());

    std::cout << "Valid: " << valid << "\n";

    // Test emoji: F0 9F 98 80 (😀)
    std::string test2 = "\xF0\x9F\x98\x80";
    std::cout << "\nTesting emoji UTF-8: ";
    for (unsigned char c : test2) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << "\n";

    valid = fastjson::validate_utf8_scalar(std::span<const char>(test2.data(), test2.size()), 0,
                                           test2.size());

    std::cout << "Valid: " << valid << "\n";

    return 0;
}
