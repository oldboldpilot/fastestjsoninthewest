// FastestJSONInTheWest - Unicode Handling (UTF-8, UTF-16, UTF-32)
// Author: Olumuyiwa Oluwasanmi
// ============================================================================
// Comprehensive Unicode support for JSON string parsing:
// - UTF-16 surrogate pair handling (U+D800-U+DFFF)
// - UTF-32 full codepoint range (U+0000-U+10FFFF)
// - UTF-8 encoding output
// - Validation and error detection

#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace fastjson {
namespace unicode {

// ============================================================================
// Unicode Constants
// ============================================================================

constexpr uint32_t UNICODE_MAX = 0x10FFFF;         // Maximum valid Unicode codepoint
constexpr uint32_t SURROGATE_HIGH_START = 0xD800;  // High surrogate range start
constexpr uint32_t SURROGATE_HIGH_END = 0xDBFF;    // High surrogate range end
constexpr uint32_t SURROGATE_LOW_START = 0xDC00;   // Low surrogate range start
constexpr uint32_t SURROGATE_LOW_END = 0xDFFF;     // Low surrogate range end
constexpr uint32_t SURROGATE_OFFSET = 0x10000;     // Offset for surrogate pair decoding

// ============================================================================
// Unicode Classification
// ============================================================================

// Check if codepoint is a high surrogate (U+D800-U+DBFF)
inline constexpr auto is_high_surrogate(uint32_t codepoint) -> bool {
    return codepoint >= SURROGATE_HIGH_START && codepoint <= SURROGATE_HIGH_END;
}

// Check if codepoint is a low surrogate (U+DC00-U+DFFF)
inline constexpr auto is_low_surrogate(uint32_t codepoint) -> bool {
    return codepoint >= SURROGATE_LOW_START && codepoint <= SURROGATE_LOW_END;
}

// Check if codepoint is any surrogate (U+D800-U+DFFF)
inline constexpr auto is_surrogate(uint32_t codepoint) -> bool {
    return codepoint >= SURROGATE_HIGH_START && codepoint <= SURROGATE_LOW_END;
}

// Check if codepoint is valid Unicode (not surrogate, within range)
inline constexpr auto is_valid_codepoint(uint32_t codepoint) -> bool {
    return codepoint <= UNICODE_MAX && !is_surrogate(codepoint);
}

// ============================================================================
// UTF-16 Surrogate Pair Decoding
// ============================================================================

// Decode UTF-16 surrogate pair to UTF-32 codepoint
// high: U+D800-U+DBFF
// low:  U+DC00-U+DFFF
// Returns codepoint in range U+10000-U+10FFFF
inline constexpr auto decode_surrogate_pair(uint32_t high, uint32_t low) -> uint32_t {
    return ((high - SURROGATE_HIGH_START) << 10) + (low - SURROGATE_LOW_START) + SURROGATE_OFFSET;
}

// Check if codepoint needs UTF-16 surrogate pair (outside BMP)
inline constexpr auto needs_surrogate_pair(uint32_t codepoint) -> bool {
    return codepoint >= SURROGATE_OFFSET && codepoint <= UNICODE_MAX;
}

// ============================================================================
// UTF-8 Encoding
// ============================================================================

// Encode UTF-32 codepoint to UTF-8 and append to string
// Returns true on success, false if codepoint is invalid
inline auto encode_utf8(uint32_t codepoint, std::string& output) -> bool {
    if (codepoint > UNICODE_MAX || is_surrogate(codepoint)) {
        return false;  // Invalid codepoint
    }

    if (codepoint <= 0x7F) {
        // 1-byte sequence: 0xxxxxxx
        output += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        // 2-byte sequence: 110xxxxx 10xxxxxx
        output += static_cast<char>(0xC0 | (codepoint >> 6));
        output += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
        output += static_cast<char>(0xE0 | (codepoint >> 12));
        output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        output += static_cast<char>(0xF0 | (codepoint >> 18));
        output += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (codepoint & 0x3F));
    }

    return true;
}

// ============================================================================
// UTF-8 Length Calculation
// ============================================================================

// Calculate UTF-8 byte length for a UTF-32 codepoint
inline constexpr auto utf8_length(uint32_t codepoint) -> int {
    if (codepoint <= 0x7F)
        return 1;
    if (codepoint <= 0x7FF)
        return 2;
    if (codepoint <= 0xFFFF)
        return 3;
    if (codepoint <= UNICODE_MAX)
        return 4;
    return 0;  // Invalid
}

// ============================================================================
// Hex Digit Parsing
// ============================================================================

// Parse single hex digit (0-9, a-f, A-F) to value 0-15
// Returns -1 if not a valid hex digit
inline constexpr auto parse_hex_digit(char c) -> int {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Parse 4 hex digits to 16-bit value
// Returns value on success, -1 on error
inline auto parse_hex4(const char* str) -> int {
    int value = 0;
    for (int i = 0; i < 4; ++i) {
        int digit = parse_hex_digit(str[i]);
        if (digit < 0)
            return -1;
        value = (value << 4) | digit;
    }
    return value;
}

// ============================================================================
// JSON \uXXXX Escape Sequence Parser
// ============================================================================

struct unicode_parse_result {
    bool success;
    uint32_t codepoint;
    int bytes_consumed;  // Number of input bytes consumed
    const char* error;
};

// Parse JSON Unicode escape sequence starting after '\u'
// Handles:
// - \uXXXX for BMP characters (U+0000-U+FFFF)
// - \uXXXX\uYYYY for surrogate pairs (U+10000-U+10FFFF)
//
// input: Points to first hex digit after '\u'
// length: Remaining bytes in input
//
// Returns: Result with codepoint and bytes consumed
inline auto parse_unicode_escape(const char* input, size_t length) -> unicode_parse_result {
    if (length < 4) {
        return {false, 0, 0, "Incomplete Unicode escape (need 4 hex digits)"};
    }

    // Parse first \uXXXX
    int first_value = parse_hex4(input);
    if (first_value < 0) {
        return {false, 0, 0, "Invalid hex digits in Unicode escape"};
    }

    uint32_t first_code = static_cast<uint32_t>(first_value);

    // Check if it's a high surrogate (start of surrogate pair)
    if (is_high_surrogate(first_code)) {
        // Need to parse low surrogate: \uXXXX\uYYYY
        if (length < 10 || input[4] != '\\' || input[5] != 'u') {
            return {false, 0, 4, "High surrogate without low surrogate pair"};
        }

        // Parse second \uYYYY
        int second_value = parse_hex4(input + 6);
        if (second_value < 0) {
            return {false, 0, 4, "Invalid hex digits in surrogate pair"};
        }

        uint32_t second_code = static_cast<uint32_t>(second_value);

        // Verify it's a low surrogate
        if (!is_low_surrogate(second_code)) {
            return {false, 0, 4, "High surrogate not followed by low surrogate"};
        }

        // Decode surrogate pair
        uint32_t codepoint = decode_surrogate_pair(first_code, second_code);

        return {true, codepoint, 10, nullptr};  // Consumed \uXXXX\uYYYY
    }

    // Check if it's a lone low surrogate (invalid)
    if (is_low_surrogate(first_code)) {
        return {false, 0, 4, "Lone low surrogate (invalid UTF-16)"};
    }

    // Regular BMP character (U+0000-U+FFFF, excluding surrogates)
    return {true, first_code, 4, nullptr};  // Consumed \uXXXX
}

// ============================================================================
// High-Level API for JSON String Parsing
// ============================================================================

// Parse and decode \uXXXX escape, append UTF-8 to output
// input: Points to character after '\u'
// length: Remaining input length
// output: String to append UTF-8 bytes to
// Returns bytes consumed on success, 0 on error
inline auto decode_json_unicode_escape(const char* input, size_t length, std::string& output)
    -> std::expected<int, const char*> {
    auto result = parse_unicode_escape(input, length);

    if (!result.success) {
        return std::unexpected(result.error);
    }

    if (!encode_utf8(result.codepoint, output)) {
        return std::unexpected("Failed to encode codepoint as UTF-8");
    }

    return result.bytes_consumed;
}

// ============================================================================
// Statistics and Information
// ============================================================================

// Get Unicode plane (0-16) for codepoint
inline constexpr auto get_unicode_plane(uint32_t codepoint) -> int {
    if (codepoint > UNICODE_MAX)
        return -1;
    return static_cast<int>(codepoint >> 16);
}

// Get Unicode block name (simplified)
inline auto get_unicode_block_name(uint32_t codepoint) -> const char* {
    if (codepoint <= 0x7F)
        return "Basic Latin (ASCII)";
    if (codepoint <= 0xFF)
        return "Latin-1 Supplement";
    if (codepoint <= 0x17F)
        return "Latin Extended-A";
    if (codepoint <= 0x24F)
        return "Latin Extended-B";
    if (codepoint <= 0x2AF)
        return "IPA Extensions";
    if (codepoint <= 0x4FF)
        return "Cyrillic";
    if (codepoint <= 0x9FF)
        return "Devanagari / Bengali / Gurmukhi";
    if (codepoint <= 0x1FFF)
        return "Various Asian Scripts";
    if (codepoint <= 0x2FFF)
        return "CJK Symbols and Punctuation";
    if (codepoint <= 0x9FFF)
        return "CJK Unified Ideographs";
    if (codepoint <= 0xFFFF)
        return "BMP (Plane 0)";
    if (codepoint <= 0x1FFFF)
        return "SMP (Plane 1) - Historic / Emoji";
    if (codepoint <= 0x2FFFF)
        return "SIP (Plane 2) - CJK Extension";
    if (codepoint <= 0x10FFFF)
        return "Planes 3-16";
    return "Invalid";
}

}  // namespace unicode
}  // namespace fastjson
