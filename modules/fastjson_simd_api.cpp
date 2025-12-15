#include "fastjson_simd_api.h"
#include <chrono>
#include <algorithm>
#include <stdexcept>

#if defined(__x86_64__) || defined(_M_X64)
    #include <cpuid.h>
#endif

namespace fastjson::simd {

// ============================================================================
// SIMDContext Implementation
// ============================================================================

auto SIMDContext::create() -> std::unique_ptr<SIMDContext> {
    auto caps = detect_capabilities();
    return std::unique_ptr<SIMDContext>(new SIMDContext(std::move(caps)));
}

auto SIMDContext::create_with_capabilities(std::span<const char> required_features) 
    -> std::optional<std::unique_ptr<SIMDContext>> {
    
    auto caps = detect_capabilities();
    
    // Check if required features are supported
    for (auto feature_iter = required_features.begin(); 
         feature_iter != required_features.end(); ) {
        
        // Extract null-terminated feature string
        const char* feature_start = &(*feature_iter);
        while (feature_iter != required_features.end() && *feature_iter != '\0') {
            ++feature_iter;
        }
        if (feature_iter != required_features.end()) ++feature_iter; // Skip null terminator
        
        // Calculate string length manually to avoid iterator type conflicts
        const char* feature_end = &(*feature_iter) - 1;  // Point to before current position
        size_t feature_length = static_cast<size_t>(feature_end - feature_start);
        std::string_view feature{feature_start, feature_length};
        
        if (feature == "avx512" && !caps.avx512_detected) {
            return std::nullopt;
        }
        if (feature == "avx2" && !caps.avx2_detected) {
            return std::nullopt;
        }
    }
    
    return std::unique_ptr<SIMDContext>(new SIMDContext(std::move(caps)));
}

auto SIMDContext::supports_avx512() const noexcept -> bool {
    return capabilities_.avx512_detected && capabilities_.avx512_enabled;
}

auto SIMDContext::supports_avx2() const noexcept -> bool {
    return capabilities_.avx2_detected;
}

auto SIMDContext::supports_multi_register() const noexcept -> bool {
    return capabilities_.multi_register_enabled && 
           (capabilities_.avx2_detected || capabilities_.avx512_detected);
}

auto SIMDContext::get_optimal_chunk_size() const noexcept -> size_t {
    if (supports_avx512()) {
        return 512;  // 8 x 64-byte registers
    }
    if (supports_avx2()) {
        return 128;  // 4 x 32-byte registers  
    }
    return 16;  // SSE2 fallback
}

auto SIMDContext::detect_capabilities() -> Capabilities {
    Capabilities caps;
    
#if defined(__x86_64__) || defined(_M_X64)
    // Use __builtin_cpu_supports for runtime detection
    caps.avx2_detected = __builtin_cpu_supports("avx2");
    caps.avx512_detected = __builtin_cpu_supports("avx512f") && 
                          __builtin_cpu_supports("avx512bw");
    
    if (caps.avx512_detected) {
        caps.optimal_chunk_size = 512;
    } else if (caps.avx2_detected) {
        caps.optimal_chunk_size = 128;
    } else {
        caps.optimal_chunk_size = 16;
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    // ARM NEON is always available on AArch64
    caps.avx2_detected = false;
    caps.avx512_detected = false;
    caps.optimal_chunk_size = 64;  // 4 x 16-byte NEON registers
#endif
    
    return caps;
}

// ============================================================================
// SIMDOperations Implementation  
// ============================================================================

auto SIMDOperations::skip_whitespace(std::span<const char> data, size_t start_pos) const -> size_t {
    return timed_operation(data, [&]() {
        return multi::skip_whitespace_multiregister(data.data(), data.size(), start_pos);
    });
}

auto SIMDOperations::find_string_end(std::span<const char> data, size_t start_pos) const -> size_t {
    return timed_operation(data, [&]() {
        if (context_->supports_multi_register()) {
            if (context_->supports_avx512()) {
                return multi::find_string_end_8x_avx512(data.data(), data.size(), start_pos);
            }
            if (context_->supports_avx2()) {
                return multi::find_string_end_4x_avx2(data.data(), data.size(), start_pos);
            }
        }
        
        return multi::find_string_end_multiregister(data.data(), data.size(), start_pos);
    });
}

auto SIMDOperations::validate_number_chars(std::span<const char> data, 
                                          size_t start_pos, 
                                          std::optional<size_t> end_pos) const -> bool {
    size_t actual_end = end_pos.value_or(data.size());
    
    return timed_operation(data, [&]() {
        if (context_->supports_multi_register() && context_->supports_avx2()) {
            return multi::validate_number_chars_4x_avx2(data.data(), start_pos, actual_end);
        }
        
        return multi::validate_number_chars_multiregister(data.data(), start_pos, actual_end);
    });
}

auto SIMDOperations::find_structural_chars(std::span<const char> data, 
                                          size_t start_pos) const -> multi::StructuralChars {
    multi::StructuralChars result;
    
    timed_operation(data, [&]() {
        if (context_->supports_multi_register() && context_->supports_avx2()) {
            multi::find_structural_chars_4x_avx2(data.data(), data.size(), start_pos, result);
        } else {
            result = multi::find_structural_chars_multiregister(data.data(), data.size(), start_pos);
        }
        return 0; // Dummy return for timed_operation
    });
    
    return result;
}

// ============================================================================
// JSONScanner Implementation
// ============================================================================

auto JSONScanner::scan_tokens(std::span<const char> json_data) -> std::vector<Token> {
    std::vector<Token> tokens;
    tokens.reserve(json_data.size() / 10); // Rough estimate
    
    size_t position = 0;
    
    while (position < json_data.size()) {
        auto token = scan_next_token(json_data, position);
        if (token) {
            tokens.emplace_back(*token);
            position = token->position + token->length;
        } else {
            ++position; // Skip unknown character
        }
    }
    
    return tokens;
}

auto JSONScanner::scan_next_token(std::span<const char> json_data, 
                                 size_t& position) -> std::optional<Token> {
    if (position >= json_data.size()) {
        return std::nullopt;
    }
    
    // Skip whitespace first
    size_t ws_end = operations_.skip_whitespace(json_data, position);
    if (ws_end > position) {
        Token ws_token{TokenType::Whitespace, position, ws_end - position};
        position = ws_end;
        return ws_token;
    }
    
    if (position >= json_data.size()) {
        return std::nullopt;
    }
    
    TokenType type = classify_token_at(json_data, position);
    
    switch (type) {
        case TokenType::String:
            return scan_string_token(json_data, position);
            
        case TokenType::Number:
            return scan_number_token(json_data, position);
            
        case TokenType::BooleanTrue:
        case TokenType::BooleanFalse:
        case TokenType::Null:
            return scan_literal_token(json_data, position);
            
        case TokenType::OpenBrace:
        case TokenType::CloseBrace:
        case TokenType::OpenBracket:
        case TokenType::CloseBracket:
        case TokenType::Colon:
        case TokenType::Comma:
            return Token{type, position, 1};
            
        default:
            return std::nullopt;
    }
}

auto JSONScanner::find_all_strings(std::span<const char> json_data) -> std::vector<Token> {
    std::vector<Token> strings;
    size_t position = 0;
    
    while (position < json_data.size()) {
        // Find next quote
        while (position < json_data.size() && json_data[position] != '"') {
            ++position;
        }
        
        if (position >= json_data.size()) break;
        
        // Found a string start
        size_t string_start = position;
        size_t string_end = operations_.find_string_end(json_data, position + 1);
        
        if (string_end < json_data.size() && json_data[string_end] == '"') {
            strings.emplace_back(Token{TokenType::String, string_start, 
                                      string_end - string_start + 1});
            position = string_end + 1;
        } else {
            ++position;
        }
    }
    
    return strings;
}

auto JSONScanner::find_all_numbers(std::span<const char> json_data) -> std::vector<Token> {
    std::vector<Token> numbers;
    size_t position = 0;
    
    while (position < json_data.size()) {
        // Skip to potential number start
        while (position < json_data.size()) {
            char c = json_data[position];
            if (c == '-' || (c >= '0' && c <= '9')) {
                break;
            }
            ++position;
        }
        
        if (position >= json_data.size()) break;
        
        // Scan number
        auto number_token = scan_number_token(json_data, position);
        if (number_token.type == TokenType::Number) {
            numbers.emplace_back(number_token);
            position = number_token.position + number_token.length;
        } else {
            ++position;
        }
    }
    
    return numbers;
}

auto JSONScanner::find_structural_layout(std::span<const char> json_data) -> std::vector<Token> {
    std::vector<Token> structural_tokens;
    
    if (enable_structural_detection_) {
        auto structural_chars = operations_.find_structural_chars(json_data);
        structural_tokens.reserve(structural_chars.count);
        
        for (size_t i = 0; i < structural_chars.count; ++i) {
            TokenType type = TokenType::Unknown;
            
            switch (structural_chars.types[i]) {
                case 1: type = TokenType::OpenBrace; break;
                case 2: type = TokenType::CloseBrace; break;
                case 3: type = TokenType::OpenBracket; break;
                case 4: type = TokenType::CloseBracket; break;
                case 5: type = TokenType::Colon; break;
                case 6: type = TokenType::Comma; break;
            }
            
            structural_tokens.emplace_back(Token{type, structural_chars.positions[i], 1});
        }
        
        // Sort by position for correct order
        std::ranges::sort(structural_tokens, 
                         [](const Token& a, const Token& b) { 
                             return a.position < b.position; 
                         });
    }
    
    return structural_tokens;
}

auto JSONScanner::classify_token_at(std::span<const char> data, size_t position) -> TokenType {
    if (position >= data.size()) {
        return TokenType::Unknown;
    }
    
    char c = data[position];
    
    switch (c) {
        case '"': return TokenType::String;
        case '{': return TokenType::OpenBrace;
        case '}': return TokenType::CloseBrace;
        case '[': return TokenType::OpenBracket;
        case ']': return TokenType::CloseBracket;
        case ':': return TokenType::Colon;
        case ',': return TokenType::Comma;
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return TokenType::Number;
        case 't':
            if (position + 4 <= data.size() && 
                data.subspan(position, 4).data()[0] == 't' &&
                data.subspan(position, 4).data()[1] == 'r' &&
                data.subspan(position, 4).data()[2] == 'u' &&
                data.subspan(position, 4).data()[3] == 'e') {
                return TokenType::BooleanTrue;
            }
            break;
        case 'f':
            if (position + 5 <= data.size() &&
                std::string_view{data.data() + position, 5} == "false") {
                return TokenType::BooleanFalse;
            }
            break;
        case 'n':
            if (position + 4 <= data.size() &&
                std::string_view{data.data() + position, 4} == "null") {
                return TokenType::Null;
            }
            break;
    }
    
    return TokenType::Unknown;
}

auto JSONScanner::scan_string_token(std::span<const char> data, size_t start_pos) -> Token {
    if (start_pos >= data.size() || data[start_pos] != '"') {
        return {TokenType::Unknown, start_pos, 0};
    }
    
    size_t end_pos = operations_.find_string_end(data, start_pos + 1);
    
    if (end_pos < data.size() && data[end_pos] == '"') {
        return {TokenType::String, start_pos, end_pos - start_pos + 1};
    }
    
    return {TokenType::Unknown, start_pos, 1};
}

auto JSONScanner::scan_number_token(std::span<const char> data, size_t start_pos) -> Token {
    size_t pos = start_pos;
    
    // Handle negative sign
    if (pos < data.size() && data[pos] == '-') {
        ++pos;
    }
    
    // Scan digits
    size_t digit_start = pos;
    while (pos < data.size() && data[pos] >= '0' && data[pos] <= '9') {
        ++pos;
    }
    
    if (pos == digit_start) {
        return {TokenType::Unknown, start_pos, 0}; // No digits found
    }
    
    // Handle decimal point
    if (pos < data.size() && data[pos] == '.') {
        ++pos;
        size_t frac_start = pos;
        while (pos < data.size() && data[pos] >= '0' && data[pos] <= '9') {
            ++pos;
        }
        if (pos == frac_start) {
            return {TokenType::Unknown, start_pos, 0}; // No fractional digits
        }
    }
    
    // Handle exponent
    if (pos < data.size() && (data[pos] == 'e' || data[pos] == 'E')) {
        ++pos;
        if (pos < data.size() && (data[pos] == '+' || data[pos] == '-')) {
            ++pos;
        }
        size_t exp_start = pos;
        while (pos < data.size() && data[pos] >= '0' && data[pos] <= '9') {
            ++pos;
        }
        if (pos == exp_start) {
            return {TokenType::Unknown, start_pos, 0}; // No exponent digits
        }
    }
    
    // Validate the entire number if validation is enabled
    if (enable_validation_) {
        if (!operations_.validate_number_chars(data, start_pos, pos)) {
            return {TokenType::Unknown, start_pos, 0};
        }
    }
    
    return {TokenType::Number, start_pos, pos - start_pos};
}

auto JSONScanner::scan_literal_token(std::span<const char> data, size_t start_pos) -> Token {
    if (start_pos >= data.size()) {
        return {TokenType::Unknown, start_pos, 0};
    }
    
    char c = data[start_pos];
    
    if (c == 't' && start_pos + 4 <= data.size() &&
        std::string_view{data.data() + start_pos, 4} == "true") {
        return {TokenType::BooleanTrue, start_pos, 4};
    }
    
    if (c == 'f' && start_pos + 5 <= data.size() &&
        std::string_view{data.data() + start_pos, 5} == "false") {
        return {TokenType::BooleanFalse, start_pos, 5};
    }
    
    if (c == 'n' && start_pos + 4 <= data.size() &&
        std::string_view{data.data() + start_pos, 4} == "null") {
        return {TokenType::Null, start_pos, 4};
    }
    
    return {TokenType::Unknown, start_pos, 0};
}

} // namespace fastjson::simd