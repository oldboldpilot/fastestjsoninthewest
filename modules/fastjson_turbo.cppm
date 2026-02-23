module;

#include <cstdint>
#include <string_view>
#include <vector>
#include <memory>
#include <expected>
#include <charconv>
#include <ranges>
#include <thread>
#include <immintrin.h>
#include <cstring>

export module fastjson_turbo;

import std;

export namespace fastjson::turbo {

enum class error_code {
    success = 0,
    invalid_json,
    capacity_exceeded,
    type_error,
    key_not_found,
    out_of_bounds
};

struct parse_error {
    error_code code;
    std::string_view message;
};

template <typename T>
using result = std::expected<T, parse_error>;

class turbo_value;
class turbo_object;
class turbo_array;

class turbo_parser {
    std::unique_ptr<uint32_t[]> structurals_;
    std::unique_ptr<char[]> structural_chars_;
    std::unique_ptr<uint64_t[]> structural_masks_;
    std::unique_ptr<uint32_t[]> matching_;
    size_t capacity_ = 0;
    
public:
    turbo_parser() = default;
    
    void allocate(size_t capacity) {
        if (capacity > capacity_) {
            structurals_ = std::make_unique<uint32_t[]>(capacity + 32);
            structural_chars_ = std::make_unique<char[]>(capacity + 32);
            structural_masks_ = std::make_unique<uint64_t[]>(capacity / 64 + 1);
            matching_ = std::make_unique<uint32_t[]>(capacity + 32);
            capacity_ = capacity;
        }
    }
    
    auto structurals() noexcept -> uint32_t* { return structurals_.get(); }
    auto structural_chars() noexcept -> char* { return structural_chars_.get(); }
    auto structural_masks() noexcept -> uint64_t* { return structural_masks_.get(); }
    auto matching() noexcept -> uint32_t* { return matching_.get(); }
};

class turbo_document {
    std::string_view input_;
    uint32_t* structurals_;
    char* structural_chars_;
    uint64_t* structural_masks_;
    uint32_t* matching_;
    size_t structurals_count_ = 0;
    
public:
    explicit turbo_document(std::string_view input, turbo_parser& parser) : input_(input) {
        parser.allocate(input.size());
        structurals_ = parser.structurals();
        structural_chars_ = parser.structural_chars();
        structural_masks_ = parser.structural_masks();
        matching_ = parser.matching();
    }
    
    [[nodiscard]] uint32_t get_structural(size_t idx) const noexcept {
        return structurals_[idx];
    }
    
    [[nodiscard]] char get_structural_char(size_t idx) const noexcept {
        if (idx >= structurals_count_) return '\0';
        return structural_chars_[idx];
    }

    [[nodiscard]] uint32_t get_matching(size_t idx) const noexcept {
        return matching_[idx];
    }
    
    // Skip over the value at idx; returns index past the closing token.
    [[nodiscard]] size_t skip_value(size_t idx) const noexcept {
        char c = structural_chars_[idx];
        if (c == '{' || c == '[') {
            return matching_[idx] + 1;
        } else if (c == '"') {
            return idx + 2;
        } else if (c == '\0') {
            return structurals_count_;
        } else {
            return idx + 1;
        }
    }
    
    [[nodiscard]] auto root() const noexcept -> turbo_value;
    
    auto structurals() noexcept -> uint32_t* { return structurals_; }
    auto structurals() const noexcept -> const uint32_t* { return structurals_; }
    auto structural_chars() noexcept -> char* { return structural_chars_; }
    auto structural_chars() const noexcept -> const char* { return structural_chars_; }
    auto structural_masks() noexcept -> uint64_t* { return structural_masks_; }
    auto structural_masks() const noexcept -> const uint64_t* { return structural_masks_; }
    auto matching() noexcept -> uint32_t* { return matching_; }
    auto matching() const noexcept -> const uint32_t* { return matching_; }
    auto structurals_count() const noexcept -> size_t { return structurals_count_; }
    void set_structurals_count(size_t count) noexcept { structurals_count_ = count; }
    [[nodiscard]] auto input() const noexcept -> std::string_view { return input_; }
};

class turbo_value {
    const turbo_document* doc_;
    size_t idx_;
    
public:
    constexpr turbo_value(const turbo_document* doc, size_t idx) noexcept 
        : doc_(doc), idx_(idx) {}
        
    [[nodiscard]] auto is_object() const noexcept -> bool {
        return doc_->get_structural_char(idx_) == '{';
    }
    
    [[nodiscard]] auto is_array() const noexcept -> bool {
        return doc_->get_structural_char(idx_) == '[';
    }
    
    [[nodiscard]] auto is_string() const noexcept -> bool {
        return doc_->get_structural_char(idx_) == '"';
    }
    
    [[nodiscard]] auto is_number() const noexcept -> bool {
        char c = doc_->get_structural_char(idx_);
        return c == '-' || (c >= '0' && c <= '9');
    }
    
    [[nodiscard]] auto get_object() const noexcept -> result<turbo_object>;
    [[nodiscard]] auto get_array() const noexcept -> result<turbo_array>;
    [[nodiscard]] auto get_string() const noexcept -> result<std::string_view>;
    [[nodiscard]] auto get_double() const noexcept -> result<double>;
    [[nodiscard]] auto get_int64() const noexcept -> result<int64_t>;
    [[nodiscard]] auto get_bool() const noexcept -> result<bool>;

    // Fluent API: zero-copy view into the source JSON buffer.
    // The caller must not outlive the turbo_document that owns the input.
    [[nodiscard]] auto as_string() const -> std::string_view {
        auto r = get_string();
        if (!r) throw std::runtime_error("turbo_value: not a string");
        return *r;
    }

    // Fluent API: materialize an owned std::string (safe to outlive the document).
    [[nodiscard]] auto as_std_string() const -> std::string {
        return std::string(as_string());
    }
};

class turbo_object {
    const turbo_document* doc_;
    size_t idx_;
    
public:
    constexpr turbo_object(const turbo_document* doc, size_t idx) noexcept 
        : doc_(doc), idx_(idx) {}
        
    [[nodiscard]] auto find_field(std::string_view key) const noexcept -> result<turbo_value>;
    
    [[nodiscard]] auto operator[](std::string_view key) const noexcept -> turbo_value {
        auto res = find_field(key);
        if (res) return *res;
        return turbo_value(doc_, 0); // Invalid value
    }
};

class turbo_array {
    const turbo_document* doc_;
    size_t idx_;
    
public:
    constexpr turbo_array(const turbo_document* doc, size_t idx) noexcept 
        : doc_(doc), idx_(idx) {}
        
    class iterator {
        const turbo_document* doc_;
        size_t idx_;
    public:
        constexpr iterator(const turbo_document* doc, size_t idx) noexcept : doc_(doc), idx_(idx) {}
        auto operator*() const noexcept -> turbo_value { return turbo_value(doc_, idx_); }
        auto operator++() noexcept -> iterator& {
            // Skip current value using cache-friendly depth scan on structural_chars_
            idx_ = doc_->skip_value(idx_);
            // Skip comma if present
            if (idx_ < doc_->structurals_count() && doc_->get_structural_char(idx_) == ',') {
                idx_++;
            }
            return *this;
        }
        auto operator!=(const iterator& other) const noexcept -> bool { return idx_ < other.idx_; }
    };

    [[nodiscard]] auto begin() const noexcept -> iterator { return iterator(doc_, idx_ + 1); }
    [[nodiscard]] auto end() const noexcept -> iterator {
        // Compute end lazily: scan for the closing ']'
        size_t end_idx = doc_->skip_value(idx_);
        if (end_idx > 0) end_idx--; // Point to the closing bracket
        return iterator(doc_, end_idx);
    }
};

inline auto turbo_document::root() const noexcept -> turbo_value {
    return turbo_value(this, 0);
}

inline auto turbo_value::get_object() const noexcept -> result<turbo_object> {
    if (!is_object()) return std::unexpected(parse_error{error_code::type_error, "Not an object"});
    return turbo_object(doc_, idx_);
}

inline auto turbo_value::get_array() const noexcept -> result<turbo_array> {
    if (!is_array()) return std::unexpected(parse_error{error_code::type_error, "Not an array"});
    return turbo_array(doc_, idx_);
}

inline auto turbo_value::get_string() const noexcept -> result<std::string_view> {
    if (!is_string()) return std::unexpected(parse_error{error_code::type_error, "Not a string"});
    uint32_t start = doc_->get_structural(idx_) + 1;
    uint32_t end = doc_->get_structural(idx_ + 1);
    return doc_->input().substr(start, end - start);
}

inline auto turbo_value::get_double() const noexcept -> result<double> {
    if (!is_number()) return std::unexpected(parse_error{error_code::type_error, "Not a number"});
    uint32_t start = doc_->get_structural(idx_);
    uint32_t end = doc_->get_structural(idx_ + 1);
    auto input = doc_->input();
    double val;
    auto [ptr, ec] = std::from_chars(input.data() + start, input.data() + end, val);
    if (ec == std::errc()) return val;
    return std::unexpected(parse_error{error_code::type_error, "Invalid number"});
}

inline auto turbo_value::get_int64() const noexcept -> result<int64_t> {
    if (!is_number()) return std::unexpected(parse_error{error_code::type_error, "Not a number"});
    uint32_t start = doc_->get_structural(idx_);
    uint32_t end = doc_->get_structural(idx_ + 1);
    auto input = doc_->input();
    int64_t val;
    auto [ptr, ec] = std::from_chars(input.data() + start, input.data() + end, val);
    if (ec == std::errc()) return val;
    return std::unexpected(parse_error{error_code::type_error, "Invalid integer"});
}

inline auto turbo_value::get_bool() const noexcept -> result<bool> {
    char c = doc_->get_structural_char(idx_);
    if (c == 't') return true;
    if (c == 'f') return false;
    return std::unexpected(parse_error{error_code::type_error, "Not a boolean"});
}

inline auto turbo_object::find_field(std::string_view key) const noexcept -> result<turbo_value> {
    size_t curr = idx_ + 1;
    auto input = doc_->input();
    const auto structurals_count = doc_->structurals_count();
    
    while (curr < structurals_count && doc_->get_structural_char(curr) != '}') {
        if (doc_->get_structural_char(curr) == '"') {
            uint32_t start = doc_->get_structural(curr) + 1;
            uint32_t end = doc_->get_structural(curr + 1);
            
            curr += 2; // Skip start and end quote
            if (curr < structurals_count && doc_->get_structural_char(curr) == ':') {
                curr++; // Skip colon
            }
            
            if (end - start == key.size() && __builtin_memcmp(input.data() + start, key.data(), key.size()) == 0) {
                return turbo_value(doc_, curr);
            }
            
            // Skip value using cache-friendly structural_chars_ scan
            curr = doc_->skip_value(curr);
            
            if (curr < structurals_count && doc_->get_structural_char(curr) == ',') {
                curr++;
            }
        } else {
            curr++;
        }
    }
    return std::unexpected(parse_error{error_code::key_not_found, "Key not found"});
}

namespace detail {

__attribute__((target("avx2,pclmul")))
inline size_t build_structural_index_avx2(std::string_view input, uint32_t* structurals, char* structural_chars, uint64_t* structural_masks, uint32_t* matching) {
    const char* data = input.data();
    size_t len = input.size();
    size_t pos = 0;
    size_t mask_idx = 0;
    
    uint64_t prev_escaped = 0;
    uint64_t prev_string_mask = 0;
    uint64_t prev_scalar_bit = 0;
    
    const __m256i quote = _mm256_set1_epi8('"');
    const __m256i backslash = _mm256_set1_epi8('\\');
    
    const __m256i lower_tbl = _mm256_setr_epi8(16, 0, 0, 0, 0, 0, 0, 0, 0, 32, 36, 1, 8, 34, 0, 0,
                                               16, 0, 0, 0, 0, 0, 0, 0, 0, 32, 36, 1, 8, 34, 0, 0);
    const __m256i upper_tbl = _mm256_setr_epi8(32, 0, 24, 4, 0, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0,
                                               32, 0, 24, 4, 0, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0);
    const __m256i lower_mask = _mm256_set1_epi8(0x0F);
    const __m256i struct_bitmask = _mm256_set1_epi8(0x0F);
    const __m256i ws_bitmask = _mm256_set1_epi8(0x30);
    const __m256i zero = _mm256_setzero_si256();
    
    // Pass 1: SIMD bitmask generation
    while (pos + 63 < len) {
        __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        
        uint64_t quote_mask = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk0, quote))) |
                             (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, quote)))) << 32);
        uint64_t backslash_mask = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk0, backslash))) |
                                 (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, backslash)))) << 32);
        
        uint64_t escaped_mask = 0;
        uint64_t bs = backslash_mask;
        if (prev_escaped) {
            escaped_mask |= 1;
        }
        while (bs) {
            uint64_t bit = bs & -bs;
            if (!(escaped_mask & bit)) {
                escaped_mask |= (bit << 1);
            }
            bs &= bs - 1;
        }
        prev_escaped = (backslash_mask & (1ULL << 63)) && !(escaped_mask & (1ULL << 63));
        
        uint64_t unescaped_quotes = quote_mask & ~escaped_mask;
        
        __m128i v = _mm_set_epi64x(0, unescaped_quotes);
        __m128i ones = _mm_set1_epi8(0xFF);
        __m128i string_mask_vec = _mm_clmulepi64_si128(v, ones, 0);
        uint64_t string_mask = _mm_cvtsi128_si64(string_mask_vec);
        string_mask ^= prev_string_mask;
        prev_string_mask = static_cast<uint64_t>(static_cast<int64_t>(string_mask) >> 63);
        
        __m256i lower_nibbles0 = _mm256_and_si256(chunk0, lower_mask);
        __m256i upper_nibbles0 = _mm256_and_si256(_mm256_srli_epi16(chunk0, 4), lower_mask);
        __m256i lower_class0 = _mm256_shuffle_epi8(lower_tbl, lower_nibbles0);
        __m256i upper_class0 = _mm256_shuffle_epi8(upper_tbl, upper_nibbles0);
        __m256i class_mask0 = _mm256_and_si256(lower_class0, upper_class0);
        __m256i struct_vec0 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask0, struct_bitmask), zero);
        __m256i ws_vec0 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask0, ws_bitmask), zero);
        
        __m256i lower_nibbles1 = _mm256_and_si256(chunk1, lower_mask);
        __m256i upper_nibbles1 = _mm256_and_si256(_mm256_srli_epi16(chunk1, 4), lower_mask);
        __m256i lower_class1 = _mm256_shuffle_epi8(lower_tbl, lower_nibbles1);
        __m256i upper_class1 = _mm256_shuffle_epi8(upper_tbl, upper_nibbles1);
        __m256i class_mask1 = _mm256_and_si256(lower_class1, upper_class1);
        __m256i struct_vec1 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask1, struct_bitmask), zero);
        __m256i ws_vec1 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask1, ws_bitmask), zero);
        
        uint64_t struct_mask = static_cast<uint32_t>(_mm256_movemask_epi8(struct_vec0)) |
                              (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(struct_vec1))) << 32);
        uint64_t ws_mask = static_cast<uint32_t>(_mm256_movemask_epi8(ws_vec0)) |
                          (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(ws_vec1))) << 32);
        
        uint64_t scalar_mask = ~(quote_mask | struct_mask | ws_mask);
        uint64_t shifted_scalar_mask = (scalar_mask << 1) | prev_scalar_bit;
        prev_scalar_bit = scalar_mask >> 63;
        
        uint64_t scalar_start_mask = scalar_mask & ~shifted_scalar_mask;
        
        struct_mask &= ~string_mask;
        scalar_start_mask &= ~string_mask;
        
        uint64_t combined_mask = unescaped_quotes | struct_mask | scalar_start_mask;
        
        structural_masks[mask_idx++] = combined_mask;
        pos += 64;
    }
    
    // Pass 2: Extract positions and chars from bitmasks
    size_t struct_idx = 0;
    for (size_t i = 0; i < mask_idx; i++) {
        uint64_t combined_mask = structural_masks[i];
        uint32_t base_pos = i * 64;
        while (combined_mask != 0) {
            uint64_t bit_pos = __builtin_ctzll(combined_mask);
            combined_mask &= combined_mask - 1;
            uint32_t p = base_pos + bit_pos;
            structurals[struct_idx] = p;
            structural_chars[struct_idx] = data[p];
            struct_idx++;
        }
    }
    
    // Scalar fallback for the rest
    bool in_string = (prev_string_mask != 0);
    while (pos < len) {
        char c = data[pos];
        if (c == '\\') {
            prev_escaped = !prev_escaped;
        } else {
            if (c == '"') {
                if (!prev_escaped) {
                    structurals[struct_idx] = pos;
                    structural_chars[struct_idx] = c;
                    struct_idx++;
                    in_string = !in_string;
                }
            } else if (!in_string) {
                if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',') {
                    structurals[struct_idx] = pos;
                    structural_chars[struct_idx] = c;
                    struct_idx++;
                } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                    // Scalar start
                    if (pos == 0 || data[pos-1] == ' ' || data[pos-1] == '\n' || data[pos-1] == '\r' || data[pos-1] == '\t' ||
                        data[pos-1] == '{' || data[pos-1] == '}' || data[pos-1] == '[' || data[pos-1] == ']' ||
                        data[pos-1] == ':' || data[pos-1] == ',' || data[pos-1] == '"') {
                        structurals[struct_idx] = pos;
                        structural_chars[struct_idx] = c;
                        struct_idx++;
                    }
                }
            }
            prev_escaped = false;
        }
        pos++;
    }
    
    // Add a dummy structural at the end to prevent out-of-bounds access
    structurals[struct_idx] = len;
    structural_chars[struct_idx] = '\0';

    // Pass 3: Build O(1) bracket matching table using a stack.
    // matching[i] = index of corresponding closing bracket for '{' or '['.
    // This is O(N) once at parse time, O(1) at every skip during traversal.
    {
        uint32_t stack_buf[65536]; // max nesting depth
        uint32_t stack_top = 0;
        for (size_t i = 0; i < struct_idx; i++) {
            char c = structural_chars[i];
            if (c == '{' || c == '[') {
                stack_buf[stack_top++] = static_cast<uint32_t>(i);
            } else if (c == '}' || c == ']') {
                if (stack_top > 0) {
                    uint32_t open_idx = stack_buf[--stack_top];
                    matching[open_idx] = static_cast<uint32_t>(i);
                }
            }
        }
    }
    
    return struct_idx;
}

} // namespace detail

[[nodiscard]] auto parse(std::string_view input, turbo_parser& parser) -> result<turbo_document> {
    turbo_document doc(input, parser);
    size_t count = detail::build_structural_index_avx2(input, doc.structurals(), doc.structural_chars(), doc.structural_masks(), doc.matching());
    doc.set_structurals_count(count);
    return doc;
}

} // namespace fastjson::turbo
