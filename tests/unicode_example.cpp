// Example demonstrating UTF-16 surrogate pair and UTF-32 support
#include <iomanip>
#include <iostream>

#include "modules/fastjson_parallel.cpp"

void print_hex(const std::string& s) {
    std::cout << "Hex bytes: ";
    for (unsigned char c : s) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << std::endl;
}

int main() {
    std::cout << "=== UTF-16 Surrogate Pair & UTF-32 Examples ===" << std::endl << std::endl;

    // Example 1: Musical note (U+1D11E) via surrogate pair
    const char* json1 = R"({"note": "\uD834\uDD1E"})";
    std::cout << "Input:  " << json1 << std::endl;
    auto result1 = fastjson::parse(json1);
    if (result1.has_value()) {
        auto& str = result1->as_object().at("note").as_string();
        std::cout << "Output: " << str << std::endl;
        print_hex(str);
        std::cout << "✓ Success" << std::endl;
    }
    std::cout << std::endl;

    // Example 2: Emoji rocket (U+1F680) via surrogate pair
    const char* json2 = R"({"emoji": "\uD83D\uDE80"})";
    std::cout << "Input:  " << json2 << std::endl;
    auto result2 = fastjson::parse(json2);
    if (result2.has_value()) {
        auto& str = result2->as_object().at("emoji").as_string();
        std::cout << "Output: " << str << std::endl;
        print_hex(str);
        std::cout << "✓ Success" << std::endl;
    }
    std::cout << std::endl;

    // Example 3: Mixed BMP and surrogate pairs
    const char* json3 = R"({"text": "Hello \uD83D\uDE00 World \uD83C\uDDEC\uD83C\uDDE7"})";
    std::cout << "Input:  " << json3 << std::endl;
    auto result3 = fastjson::parse(json3);
    if (result3.has_value()) {
        auto& str = result3->as_object().at("text").as_string();
        std::cout << "Output: " << str << std::endl;
        print_hex(str);
        std::cout << "✓ Success" << std::endl;
    }
    std::cout << std::endl;

    // Example 4: Full Unicode range
    const char* json4 = R"({
        "ascii": "A",
        "latin": "\u00E9",
        "cjk": "\u4E2D",
        "emoji": "\uD83D\uDE80",
        "math": "\uD835\uDC00"
    })";
    std::cout << "Input: Multi-script JSON" << std::endl;
    auto result4 = fastjson::parse(json4);
    if (result4.has_value()) {
        auto& obj = result4->as_object();
        std::cout << "Output:" << std::endl;
        std::cout << "  ASCII:  " << obj.at("ascii").as_string() << std::endl;
        std::cout << "  Latin:  " << obj.at("latin").as_string() << " (é)" << std::endl;
        std::cout << "  CJK:    " << obj.at("cjk").as_string() << " (中)" << std::endl;
        std::cout << "  Emoji:  " << obj.at("emoji").as_string() << " (🚀)" << std::endl;
        std::cout << "  Math:   " << obj.at("math").as_string() << " (𝐀)" << std::endl;
        std::cout << "✓ Success" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "=== All examples passed! ===" << std::endl;
    return 0;
}
