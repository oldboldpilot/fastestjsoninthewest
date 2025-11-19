// Debug UTF-8 validation
#include <iomanip>
#include <iostream>

#include "../modules/unicode.h"

int main() {
    std::string output;

    // Test musical note (U+1D11E = 𝄞)
    // High surrogate: 0xD834, Low surrogate: 0xDD1E
    uint32_t high = 0xD834;
    uint32_t low = 0xDD1E;
    uint32_t codepoint = fastjson::unicode::decode_surrogate_pair(high, low);

    std::cout << "Codepoint: U+" << std::hex << std::uppercase << codepoint << std::dec << "\n";
    std::cout << "Is valid: " << fastjson::unicode::is_valid_codepoint(codepoint) << "\n";
    std::cout << "UTF-8 length: " << fastjson::unicode::utf8_length(codepoint) << "\n";

    bool success = fastjson::unicode::encode_utf8(codepoint, output);
    std::cout << "Encode success: " << success << "\n";
    std::cout << "Output bytes: ";
    for (unsigned char c : output) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << "\n";
    std::cout << "Output string: " << output << "\n";

    return 0;
}
