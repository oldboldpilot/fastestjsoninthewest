module;

#include <cstdint>
#include <string_view>
#include <memory>
#include <expected>
#include <charconv>
#include <immintrin.h>
#if defined(__x86_64__) || defined(_M_X64)
#  include <cpuid.h>
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-WTU-local-entity-exposure"
#endif

export module fastjson_turbo;

import std;

// ---------------------------------------------------------------------------
// fastjson_turbo v5.1 — AVX-512/AVX2 runtime-dispatch JSON parser
//
// Key design decisions:
//   • AVX-512 path: 8× zmm = 512 bytes/iteration
//     – _mm512_cmpeq_epi8_mask / _mm512_cmpgt_epi8_mask return native 64-bit
//       masks — zero vmovmskb instructions on the hot path
//   • AVX2 fallback: 8× ymm = 256 bytes/iteration
//   • Runtime CPUID dispatch: AVX-512BW → AVX2 (one branch, static bool)
//   • Packed tokens: bits[31:24]=char, bits[23:0]=byte-offset → one array
//   • Single-pass tape build: SIMD scan + bracket matching overlapped via OOO
//   • Supports JSON up to 16 777 215 bytes (~16 MB)
//
// v5.1 perf fix:
//   • tape_entry stores use __builtin_memcpy(8) → single QWORD MOV
//     (struct aggregate init {tok, 0u} was generating 2× DWORD stores)
//   • Added uint32_t* overloads of process_group64 / process_scalar_tail
//     for pure IndexOnly path: zero bracket stack, 1 DWORD store per token
//   • Added build_structural_index(string_view, uint32_t*) dispatch
//
// Decoupled two-pass investigation (v5.1 → rejected):
//   Three two-pass variants were benchmarked against single-pass (836 MB/s):
//     bitmask intermediate (75 KB): 649 MB/s  — data[p] re-read at L2/L3
//     position-array intermediate:  474 MB/s  — 4.3 MB vs 2.4 MB total traffic
//   Single-pass wins: OOO execution overlaps SIMD mask computation with the
//   bracket-stack scalar loop; data[base+bit] is L1-resident in that same
//   iteration.  Any two-pass forces a second touch of the input at L2+ latency.
// ---------------------------------------------------------------------------

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
    error_code       code;
    std::string_view message;
};

template <typename T>
using result = std::expected<T, parse_error>;

class turbo_value;
class turbo_object;
class turbo_array;

// -------------------------------------------------------------------------
// tape_entry: single interleaved 64-bit token slot.
//   token:   (char<<24 | byte_offset24) — same encoding as old structurals_[i]
//   partner: index of matching bracket; 0 for non-brackets
// Memory layout: 8 bytes/token, same total as old (4B structurals_ + 4B matching_).
// Spatial locality: skip_value() reads token+partner from the SAME 8-byte slot.
// -------------------------------------------------------------------------
struct tape_entry {
    uint32_t token;    // bits[31:24]=char, bits[23:0]=byte-offset
    uint32_t partner;  // matching bracket index (0 = no partner)
};
static_assert(sizeof(tape_entry) == 8, "tape_entry must be 8 bytes");

// Input bytes per tile of the decoupled two-phase tape build. Multiple of 512
// so the chunked scan's block pattern matches the monolithic scanner. 32 KB
// measured fastest on Skylake-SP (16K equal, 64K/128K/256K worse): the tile's
// tokens stay L2-resident between the scan and expand phases.
inline constexpr size_t decoupled_chunk_size = 32 * 1024;

// -------------------------------------------------------------------------
// turbo_parser
// -------------------------------------------------------------------------
class turbo_parser {
    std::unique_ptr<tape_entry[]> tape_;
    std::unique_ptr<uint32_t[]>   indexes_;  // phase-1 structural indexes for iterate()
    std::unique_ptr<uint32_t[]>   tokens_;   // phase-1 scratch for decoupled AVX-512 build
    size_t capacity_ = 0;
    size_t capacity_indexes_ = 0;

public:
    turbo_parser() = default;

    [[gnu::always_inline]] inline void allocate(size_t capacity) {
        // `|| !tape_` : a fresh parser fed an empty input (allocate(0)) must
        // still own a tape — the build writes a sentinel entry even for "".
        if (__builtin_expect(capacity > capacity_ || !tape_, 0)) {
            tape_     = std::make_unique_for_overwrite<tape_entry[]>(capacity + 32);
            indexes_  = std::make_unique_for_overwrite<uint32_t[]>(capacity + 128);
            // Phase-1 scratch holds at most one tile's tokens (≤1/byte) plus
            // the <64B scalar tail — fixed size, not O(input).
            if (!tokens_)
                tokens_ = std::make_unique_for_overwrite<uint32_t[]>(decoupled_chunk_size + 64);
            capacity_ = capacity;
            capacity_indexes_ = capacity;
        }
    }

    [[gnu::always_inline]] inline void allocate_indexes(size_t capacity) {
        if (__builtin_expect(capacity > capacity_indexes_ || !indexes_, 0)) {
            indexes_ = std::make_unique_for_overwrite<uint32_t[]>(capacity + 128);
            capacity_indexes_ = capacity;
        }
    }

    [[nodiscard, gnu::always_inline, gnu::returns_nonnull]] inline tape_entry* tape()    noexcept { return tape_.get(); }
    [[nodiscard, gnu::always_inline, gnu::returns_nonnull]] inline uint32_t*   indexes() noexcept { return indexes_.get(); }
    [[nodiscard, gnu::always_inline]] inline uint32_t*   tokens()  noexcept { return tokens_.get(); }
};

// -------------------------------------------------------------------------
// turbo_document
// -------------------------------------------------------------------------
class turbo_document {
    std::string_view   input_;
    const tape_entry*  tape_ = nullptr;
    const uint32_t*    indexes_ = nullptr;
    size_t             count_ = 0;

public:
    [[gnu::always_inline]] inline explicit turbo_document(std::string_view input, turbo_parser& parser)
        : input_(input), tape_(nullptr), indexes_(nullptr), count_(0)
    {
        parser.allocate(input.size());
        tape_ = parser.tape();
    }

    [[gnu::always_inline]] inline explicit turbo_document(std::string_view input, const tape_entry* tape, size_t count) noexcept
        : input_(input), tape_(tape), indexes_(nullptr), count_(count)
    {}

    [[gnu::always_inline]] inline explicit turbo_document(std::string_view input, const uint32_t* indexes, size_t count) noexcept
        : input_(input), tape_(nullptr), indexes_(indexes), count_(count)
    {}

    // Byte position of structural token i
    [[nodiscard, gnu::always_inline]] inline uint32_t get_structural(size_t idx) const noexcept {
        if (indexes_) return indexes_[idx];
        return tape_[idx].token & 0x00FF'FFFFu;
    }

    // Character type of structural token i
    [[nodiscard, gnu::always_inline]] inline char get_structural_char(size_t idx) const noexcept {
        if (idx >= count_) return '\0';
        if (indexes_) {
            return input_[indexes_[idx]];
        }
        char c = static_cast<char>(tape_[idx].token >> 24);
        if (__builtin_expect(c != '\0', 1)) return c;
        return input_[tape_[idx].token & 0x00FF'FFFFu];
    }

    [[nodiscard, gnu::always_inline]] inline size_t skip_container_lazy(size_t idx) const noexcept {
        int depth = 1;
        if (indexes_) {
            const char* in = input_.data();
            const uint32_t* idxs = indexes_;
            size_t i = idx + 1;
            while (i + 4 <= count_) {
                const uint8_t b0 = static_cast<uint8_t>(in[idxs[i]])     | 0x20u;
                const uint8_t b1 = static_cast<uint8_t>(in[idxs[i + 1]]) | 0x20u;
                const uint8_t b2 = static_cast<uint8_t>(in[idxs[i + 2]]) | 0x20u;
                const uint8_t b3 = static_cast<uint8_t>(in[idxs[i + 3]]) | 0x20u;
                if (__builtin_expect((b0 < 0x7Bu) & (b1 < 0x7Bu) & (b2 < 0x7Bu) & (b3 < 0x7Bu), 1)) {
                    i += 4;
                    continue;
                }
                depth += static_cast<int>(b0 == 0x7Bu) - static_cast<int>(b0 == 0x7Du);
                if (depth == 0) return i + 1;
                depth += static_cast<int>(b1 == 0x7Bu) - static_cast<int>(b1 == 0x7Du);
                if (depth == 0) return i + 2;
                depth += static_cast<int>(b2 == 0x7Bu) - static_cast<int>(b2 == 0x7Du);
                if (depth == 0) return i + 3;
                depth += static_cast<int>(b3 == 0x7Bu) - static_cast<int>(b3 == 0x7Du);
                if (depth == 0) return i + 4;
                i += 4;
            }
            for (; i < count_; ++i) {
                const uint8_t b = static_cast<uint8_t>(in[idxs[i]]) | 0x20u;
                depth += static_cast<int>(b == 0x7Bu) - static_cast<int>(b == 0x7Du);
                if (depth == 0) return i + 1;
            }
            return count_;
        }
        for (size_t i = idx + 1; i < count_; ++i) {
            char ch = get_structural_char(i);
            if (ch == '}' || ch == ']') {
                if (--depth == 0) return i + 1;
            } else if (ch == '{' || ch == '[') {
                ++depth;
            }
        }
        return count_;
    }

    // O(1) skip for pre-matched DOM; lazy linear scan for streaming ondemand
    [[nodiscard, gnu::always_inline]] inline size_t skip_value(size_t idx) const noexcept {
        if (tape_) {
            const tape_entry& e = tape_[idx];
            const char c = static_cast<char>(e.token >> 24);
            if (c == '"') return idx + 2;
            if (c == '{' || c == '[') return e.partner != 0 ? e.partner + 1 : skip_container_lazy(idx);
            return idx + 1;
        }
        const uint8_t b = static_cast<uint8_t>(input_[indexes_[idx]]) | 0x20u;
        if (__builtin_expect(b == 0x7Bu, 0)) return skip_container_lazy(idx);
        return idx + 1;
    }

    // Skip remainder of container when child cursor has advanced past original start
    [[nodiscard, gnu::always_inline]] inline size_t skip_child_lazy(size_t curr, size_t original_idx) const noexcept {
        if (curr <= original_idx) {
            return skip_value(original_idx);
        }
        if (tape_) {
            const tape_entry& e = tape_[original_idx];
            if (e.partner != 0) return e.partner + 1;
        }
        int depth = 1;
        if (indexes_) {
            const char* in = input_.data();
            const uint32_t* idxs = indexes_;
            size_t i = curr;
            while (i + 4 <= count_) {
                const uint8_t b0 = static_cast<uint8_t>(in[idxs[i]])     | 0x20u;
                const uint8_t b1 = static_cast<uint8_t>(in[idxs[i + 1]]) | 0x20u;
                const uint8_t b2 = static_cast<uint8_t>(in[idxs[i + 2]]) | 0x20u;
                const uint8_t b3 = static_cast<uint8_t>(in[idxs[i + 3]]) | 0x20u;
                if (__builtin_expect((b0 < 0x7Bu) & (b1 < 0x7Bu) & (b2 < 0x7Bu) & (b3 < 0x7Bu), 1)) {
                    i += 4;
                    continue;
                }
                depth += static_cast<int>(b0 == 0x7Bu) - static_cast<int>(b0 == 0x7Du);
                if (depth == 0) return i + 1;
                depth += static_cast<int>(b1 == 0x7Bu) - static_cast<int>(b1 == 0x7Du);
                if (depth == 0) return i + 2;
                depth += static_cast<int>(b2 == 0x7Bu) - static_cast<int>(b2 == 0x7Du);
                if (depth == 0) return i + 3;
                depth += static_cast<int>(b3 == 0x7Bu) - static_cast<int>(b3 == 0x7Du);
                if (depth == 0) return i + 4;
                i += 4;
            }
            for (; i < count_; ++i) {
                const uint8_t b = static_cast<uint8_t>(in[idxs[i]]) | 0x20u;
                depth += static_cast<int>(b == 0x7Bu) - static_cast<int>(b == 0x7Du);
                if (depth == 0) return i + 1;
            }
            return count_;
        }
        for (size_t i = curr; i < count_; ++i) {
            char ch = get_structural_char(i);
            if (ch == '}' || ch == ']') {
                if (--depth == 0) return i + 1;
            } else if (ch == '{' || ch == '[') {
                ++depth;
            }
        }
        return count_;
    }

    [[nodiscard]] auto root()              const noexcept -> turbo_value;
    [[nodiscard, gnu::always_inline]] inline size_t structurals_count() const noexcept { return count_; }
    [[gnu::always_inline]] inline void set_structurals_count(size_t n) noexcept { count_ = n; }
    [[nodiscard, gnu::always_inline]] inline std::string_view input() const noexcept { return input_; }
    [[nodiscard, gnu::always_inline]] inline const tape_entry* tape() const noexcept { return tape_; }
    [[gnu::always_inline]] inline void set_tape(const tape_entry* t) noexcept { tape_ = t; indexes_ = nullptr; }
    [[nodiscard, gnu::always_inline]] inline const uint32_t* indexes() const noexcept { return indexes_; }
    [[gnu::always_inline]] inline void set_indexes(const uint32_t* idxs) noexcept { indexes_ = idxs; tape_ = nullptr; }
};

// -------------------------------------------------------------------------
// turbo_value
// -------------------------------------------------------------------------
class turbo_value {
    const turbo_document* doc_;
    size_t                idx_;
    size_t*               parent_cursor_ = nullptr;

public:
    constexpr turbo_value(const turbo_document* doc, size_t idx, size_t* parent_cursor = nullptr) noexcept
        : doc_(doc), idx_(idx), parent_cursor_(parent_cursor) {}

    [[nodiscard, gnu::always_inline]] inline bool is_object() const noexcept { return doc_->get_structural_char(idx_) == '{'; }
    [[nodiscard, gnu::always_inline]] inline bool is_array()  const noexcept { return doc_->get_structural_char(idx_) == '['; }
    [[nodiscard, gnu::always_inline]] inline bool is_string() const noexcept { return doc_->get_structural_char(idx_) == '"'; }
    [[nodiscard, gnu::always_inline]] inline bool is_number() const noexcept {
        char c = doc_->get_structural_char(idx_);
        return c == '-' || (c >= '0' && c <= '9');
    }

    [[nodiscard]] result<turbo_object>   get_object() const noexcept;
    [[nodiscard]] result<turbo_array>    get_array()  const noexcept;
    [[nodiscard]] result<std::string_view> get_string() const noexcept;
    [[nodiscard]] result<double>         get_double() const noexcept;
    [[nodiscard]] result<int64_t>        get_int64()  const noexcept;
    [[nodiscard]] result<bool>           get_bool()   const noexcept;

    [[nodiscard]] std::string_view as_string() const {
        auto r = get_string();
        if (!r) throw std::runtime_error("turbo_value: not a string");
        return *r;
    }
    [[nodiscard]] std::string as_std_string() const { return std::string(as_string()); }
};

// -------------------------------------------------------------------------
// turbo_object
// -------------------------------------------------------------------------
class turbo_object {
    const turbo_document* doc_;
    size_t                idx_;
    mutable size_t        cursor_;
    size_t*               parent_cursor_ = nullptr;

public:
    constexpr turbo_object(const turbo_document* doc, size_t idx, size_t* parent_cursor = nullptr) noexcept
        : doc_(doc), idx_(idx), cursor_(idx + 1), parent_cursor_(parent_cursor) {}

    [[nodiscard]] result<turbo_value> find_field(std::string_view key) const noexcept;

    [[nodiscard, gnu::always_inline]] inline turbo_value operator[](std::string_view key) const noexcept {
        auto r = find_field(key);
        return r ? *r : turbo_value(doc_, 0);
    }
};

// -------------------------------------------------------------------------
// turbo_array + iterator
// -------------------------------------------------------------------------
class turbo_array {
    const turbo_document* doc_;
    size_t                idx_;

public:
    constexpr turbo_array(const turbo_document* doc, size_t idx) noexcept
        : doc_(doc), idx_(idx) {}

    struct iterator {
        const turbo_document* doc_;
        size_t                idx_;
        mutable size_t        cursor_ = 0;

        [[gnu::always_inline]] inline turbo_value operator*()  const noexcept { return turbo_value(doc_, idx_, &cursor_); }
        [[gnu::always_inline]] inline iterator& operator++() noexcept {
            size_t start = (cursor_ > idx_) ? cursor_ : idx_;
            idx_ = doc_->skip_child_lazy(start, idx_);
            cursor_ = idx_;
            if (idx_ < doc_->structurals_count()) {
                const uint32_t* idxs = doc_->indexes();
                char c = idxs ? doc_->input()[idxs[idx_]] : doc_->get_structural_char(idx_);
                if (c == ',') {
                    ++idx_;
                    cursor_ = idx_;
                }
            }
            return *this;
        }
        [[gnu::always_inline]] inline bool operator!=(const iterator& o) const noexcept {
            if (o.idx_ == 0) {
                if (idx_ >= doc_->structurals_count()) return false;
                const uint32_t* idxs = doc_->indexes();
                char c = idxs ? doc_->input()[idxs[idx_]] : doc_->get_structural_char(idx_);
                return c != ']';
            }
            return idx_ < o.idx_;
        }
    };

    [[gnu::always_inline]] inline iterator begin() const noexcept { return {doc_, idx_ + 1, idx_ + 1}; }
    [[gnu::always_inline]] inline iterator end()   const noexcept {
        return {doc_, 0, 0};
    }
};

// -------------------------------------------------------------------------
// Deferred inline definitions
// -------------------------------------------------------------------------
[[gnu::always_inline]] inline auto turbo_document::root() const noexcept -> turbo_value {
    return turbo_value(this, 0);
}
[[gnu::always_inline]] inline auto turbo_value::get_object() const noexcept -> result<turbo_object> {
    if (!is_object())
        return std::unexpected(parse_error{error_code::type_error, "Not an object"});
    return turbo_object(doc_, idx_, parent_cursor_);
}
[[gnu::always_inline]] inline auto turbo_value::get_array() const noexcept -> result<turbo_array> {
    if (!is_array())
        return std::unexpected(parse_error{error_code::type_error, "Not an array"});
    return turbo_array(doc_, idx_);
}
[[gnu::always_inline]] inline auto turbo_value::get_string() const noexcept -> result<std::string_view> {
    if (const uint32_t* idxs = doc_->indexes()) {
        const char* in = doc_->input().data();
        if (__builtin_expect(idx_ >= doc_->structurals_count() || in[idxs[idx_]] != '"', 0))
            return std::unexpected(parse_error{error_code::type_error, "Not a string"});
        uint32_t start = idxs[idx_] + 1;
        uint32_t end   = idxs[idx_ + 1] - 1;
        if (__builtin_expect(end >= start && in[end] == '"', 1)) {
            return std::string_view(in + start, end - start);
        }
        while (end > start && (in[end] == ' ' || in[end] == '\n' || in[end] == '\r' || in[end] == '\t')) {
            --end;
        }
        if (__builtin_expect(end < start || in[end] != '"', 0))
            return std::unexpected(parse_error{error_code::invalid_json, "Unterminated string"});
        return std::string_view(in + start, end - start);
    }
    if (!is_string())
        return std::unexpected(parse_error{error_code::type_error, "Not a string"});
    uint32_t start = doc_->get_structural(idx_) + 1;
    uint32_t end   = doc_->get_structural(idx_ + 1);
    return doc_->input().substr(start, end - start);
}
[[gnu::always_inline]] inline auto turbo_value::get_double() const noexcept -> result<double> {
    const uint32_t* idxs = doc_->indexes();
    const size_t in_size = doc_->input().size();
    const size_t total   = doc_->structurals_count();
    uint32_t start = idxs ? idxs[idx_] : doc_->get_structural(idx_);
    const char* p = doc_->input().data() + start;

    static constexpr double pow10_inv[] = {
        1.0, 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8,
        1.0e-9, 1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16
    };

    if (__builtin_expect(idx_ + 1 < total && start < in_size, 1)) {
        const bool negative = (*p == '-');
        p += negative;
        uint32_t d = static_cast<uint32_t>(static_cast<uint8_t>(*p) - '0');
        if (__builtin_expect(d > 9u, 0))
            return std::unexpected(parse_error{error_code::type_error, "Not a number"});
        uint64_t val = d;
        const char* cur = p + 1;
        while (true) {
            d = static_cast<uint32_t>(static_cast<uint8_t>(*cur) - '0');
            if (d > 9u) break;
            val = val * 10 + d;
            ++cur;
        }
        int decimal_digits = 0;
        if (*cur == '.') {
            const char* frac_start = ++cur;
            d = static_cast<uint32_t>(static_cast<uint8_t>(*cur) - '0');
            if (__builtin_expect(d > 9u, 0))
                return std::unexpected(parse_error{error_code::type_error, "Invalid number"});
            val = val * 10 + d;
            ++cur;
            while (true) {
                d = static_cast<uint32_t>(static_cast<uint8_t>(*cur) - '0');
                if (d > 9u) break;
                val = val * 10 + d;
                ++cur;
            }
            decimal_digits = static_cast<int>(cur - frac_start);
        }
        const int total_digits = static_cast<int>((cur - p) - (decimal_digits > 0 ? 1 : 0));
        if (__builtin_expect(total_digits <= 19 && decimal_digits <= 16 &&
                             (*cur | 0x20) != 'e' && *cur != '.', 1)) {
            double res = static_cast<double>(val) * pow10_inv[decimal_digits];
            return negative ? -res : res;
        }
    }

    uint32_t end = idxs ? idxs[idx_ + 1] : ((idx_ + 1 < total) ? doc_->get_structural(idx_ + 1) : static_cast<uint32_t>(in_size));
    const char* p_end = doc_->input().data() + end;
    if (idx_ + 1 >= total) {
        if (__builtin_expect(p >= p_end, 0))
            return std::unexpected(parse_error{error_code::type_error, "Not a number"});
        bool negative = (*p == '-');
        p += negative;
        if (__builtin_expect(p >= p_end || static_cast<uint32_t>(static_cast<uint8_t>(*p) - '0') > 9u, 0))
            return std::unexpected(parse_error{error_code::type_error, "Not a number"});
        uint64_t val = 0;
        const char* cur = p;
        while (cur < p_end) {
            uint32_t d = static_cast<uint32_t>(static_cast<uint8_t>(*cur) - '0');
            if (d > 9u) break;
            val = val * 10 + d;
            ++cur;
        }
        int decimal_digits = 0;
        if (cur < p_end && *cur == '.') {
            const char* frac_start = ++cur;
            while (cur < p_end) {
                uint32_t d = static_cast<uint32_t>(static_cast<uint8_t>(*cur) - '0');
                if (d > 9u) break;
                val = val * 10 + d;
                ++cur;
            }
            decimal_digits = static_cast<int>(cur - frac_start);
            if (__builtin_expect(decimal_digits == 0, 0))
                return std::unexpected(parse_error{error_code::type_error, "Invalid number"});
        }
        const int total_digits = static_cast<int>((cur - p) - (decimal_digits > 0 ? 1 : 0));
        if (__builtin_expect(total_digits <= 19 && decimal_digits <= 16 &&
                             (cur == p_end || ((*cur | 0x20) != 'e' && *cur != '.')), 1)) {
            double res = static_cast<double>(val) * pow10_inv[decimal_digits];
            return negative ? -res : res;
        }
    }

    double d_val;
    auto [ptr, ec] = std::from_chars(doc_->input().data() + start, p_end, d_val);
    if (ec == std::errc()) return d_val;
    return std::unexpected(parse_error{error_code::type_error, "Invalid number"});
}
[[gnu::always_inline]] inline auto turbo_value::get_int64() const noexcept -> result<int64_t> {
    if (!is_number())
        return std::unexpected(parse_error{error_code::type_error, "Not a number"});
    uint32_t start = doc_->get_structural(idx_);
    uint32_t end   = doc_->get_structural(idx_ + 1);
    const char* p = doc_->input().data() + start;
    const char* p_end = doc_->input().data() + end;

    bool negative = false;
    if (p < p_end && *p == '-') {
        negative = true;
        ++p;
    }
    uint64_t val = 0;
    const char* cur = p;
    while (cur < p_end) {
        uint32_t d = static_cast<uint32_t>(static_cast<uint8_t>(*cur) - '0');
        if (d > 9u) break;
        val = val * 10 + d;
        ++cur;
    }
    if (__builtin_expect(cur > p && (cur - p) <= 18 && (cur == p_end || *cur == ',' || *cur == '}' || *cur == ']' || *cur == ' ' || *cur == '\n' || *cur == '\r' || *cur == '\t'), 1)) {
        int64_t res = static_cast<int64_t>(val);
        return negative ? -res : res;
    }
    int64_t i_val;
    auto [ptr, ec] = std::from_chars(doc_->input().data() + start, doc_->input().data() + end, i_val);
    if (ec == std::errc()) return i_val;
    return std::unexpected(parse_error{error_code::type_error, "Invalid integer"});
}
[[gnu::always_inline]] inline auto turbo_value::get_bool() const noexcept -> result<bool> {
    if (idx_ >= doc_->structurals_count())
        return std::unexpected(parse_error{error_code::type_error, "Not a boolean"});
    const uint32_t pos = doc_->get_structural(idx_);
    const auto inp = doc_->input();
    const size_t rem = inp.size() - pos;
    const char* p = inp.data() + pos;
    if (rem >= 4 && std::memcmp(p, "true", 4) == 0) return true;
    if (rem >= 5 && std::memcmp(p, "false", 5) == 0) return false;
    return std::unexpected(parse_error{error_code::type_error, "Not a boolean"});
}
[[gnu::always_inline]] inline auto turbo_object::find_field(std::string_view key) const noexcept
    -> result<turbo_value>
{
    auto   inp   = doc_->input();
    size_t total = doc_->structurals_count();
    const size_t first_start = (cursor_ > idx_) ? cursor_ : idx_ + 1;
    const size_t key_len = key.size();
    const char* in = inp.data();
    const uint32_t* idxs = doc_->indexes();

    if (__builtin_expect(idxs != nullptr, 1)) {
        const char key_first = key_len > 0 ? key[0] : '"';
        size_t seg_begin = first_start, seg_end = total;
        for (int seg = 0; seg < 2; ++seg) {
            size_t curr = seg_begin;
            if (curr < seg_end && in[idxs[curr]] == '}') {
                if (first_start == idx_ + 1) break;
                seg_begin = idx_ + 1; seg_end = first_start;
                continue;
            }
            while (curr + 2 < seg_end) {
                char c = in[idxs[curr]];
                if (__builtin_expect(c == '"', 1)) {
                    uint32_t ks = idxs[curr] + 1;
                    size_t val_curr = curr + 2;
                    if (in[ks] == key_first && in[ks + key_len] == '"' &&
                        __builtin_expect(ks + key_len < idxs[curr + 1] &&
                                         std::memcmp(in + ks, key.data(), key_len) == 0, 1)) {
                        uint8_t val_b = static_cast<uint8_t>(in[idxs[val_curr]]) | 0x20u;
                        size_t nxt = val_curr;
                        if (__builtin_expect(val_b != 0x7Bu, 1)) {
                            nxt = val_curr + 1;
                            if (nxt < total && in[idxs[nxt]] == ',') ++nxt;
                        }
                        cursor_ = nxt;
                        if (parent_cursor_) *parent_cursor_ = nxt;
                        return turbo_value(doc_, val_curr);
                    }

                    // Not a match: advance past key, colon, and value
                    uint8_t val_b = static_cast<uint8_t>(in[idxs[val_curr]]) | 0x20u;
                    if (__builtin_expect(val_b != 0x7Bu, 1)) {
                        curr = val_curr + 1;
                        if (__builtin_expect(curr < seg_end && in[idxs[curr]] == ',', 1)) {
                            ++curr;
                            continue;
                        }
                    } else {
                        curr = doc_->skip_container_lazy(val_curr);
                        if (curr < seg_end && in[idxs[curr]] == ',') {
                            ++curr;
                            continue;
                        }
                    }
                    if (curr >= seg_end || in[idxs[curr]] == '}') break;
                } else if (c == '}') {
                    break;
                } else {
                    ++curr;
                }
            }
            if (first_start == idx_ + 1) break;
            seg_begin = idx_ + 1; seg_end = first_start;
        }
        return std::unexpected(parse_error{error_code::key_not_found, "Key not found"});
    }

    size_t seg_begin = first_start, seg_end = total;
    for (int seg = 0; seg < 2; ++seg) {
        size_t curr = seg_begin;
        while (curr < seg_end) {
            char c = doc_->get_structural_char(curr);
            if (c == '}') break;
            if (__builtin_expect(c == '"', 1)) {
                uint32_t ks = doc_->get_structural(curr) + 1;
                if (__builtin_expect(ks + key_len < inp.size() && inp[ks + key_len] == '"' &&
                                     std::memcmp(inp.data() + ks, key.data(), key_len) == 0, 1)) {
                    size_t val_curr = curr + 2;
                    if (val_curr < total && doc_->get_structural_char(val_curr) == ':') ++val_curr;
                    char val_ch = doc_->get_structural_char(val_curr);
                    size_t nxt;
                    if (val_ch == '{' || val_ch == '[') {
                        nxt = val_curr + 1;
                    } else if (val_ch == '"') {
                        nxt = val_curr + 2;
                        if (nxt < total && doc_->get_structural_char(nxt) == ',') ++nxt;
                    } else {
                        nxt = val_curr + 1;
                        if (nxt < total && doc_->get_structural_char(nxt) == ',') ++nxt;
                    }
                    cursor_ = nxt;
                    if (parent_cursor_) *parent_cursor_ = nxt;
                    return turbo_value(doc_, val_curr);
                }

                size_t val_curr = curr + 2;
                if (val_curr < total && doc_->get_structural_char(val_curr) == ':') ++val_curr;
                char val_ch = doc_->get_structural_char(val_curr);
                if (val_ch == '"') {
                    curr = val_curr + 2;
                } else if (val_ch == '{' || val_ch == '[') {
                    curr = doc_->skip_value(val_curr);
                } else {
                    curr = val_curr + 1;
                }
                if (curr < total && doc_->get_structural_char(curr) == ',') {
                    ++curr;
                }
            } else {
                ++curr;
            }
        }
        if (first_start == idx_ + 1) break;    // started at the front: no wrap
        seg_begin = idx_ + 1; seg_end = first_start;
    }
    return std::unexpected(parse_error{error_code::key_not_found, "Key not found"});
}

// =========================================================================
// SIMD structural indexer  — v5.1
//   AVX-512:  8 × zmm  = 512 bytes/iteration  (native 64-bit masks)
//   AVX2 fb:  8 × ymm  = 256 bytes/iteration  (2×movemask per group)
// =========================================================================
namespace detail {

// ── Shared carry state ────────────────────────────────────────────────────
struct scan_state {
    uint64_t prev_escaped     = 0;
    uint64_t prev_string_mask = 0;
    uint64_t prev_scalar_bit  = 0;
};

// ── process_group64 [tape_entry*]: carry-chain, PCLMUL string-region, bracket matching ──
// Uses pclmul — caller must be in a pclmul-targeted function.
__attribute__((target("pclmul")))
inline void process_group64(
        scan_state& st,
        uint64_t qm, uint64_t bm, uint64_t stm, uint64_t wsm,
        uint64_t open_m, uint64_t close_m,
        uint32_t base,
        const char* __restrict__ data,
        tape_entry* __restrict__ te, size_t& si,
        uint32_t* __restrict__ bkt_stk, int32_t& bkt_top) noexcept
{
    // Escape prefix-sum.
    uint64_t em = 0, tmp_bm = bm;
    if (st.prev_escaped) em |= 1ULL;
    while (tmp_bm) { uint64_t b = tmp_bm & -tmp_bm; if (!(em & b)) em |= (b << 1); tmp_bm &= tmp_bm - 1; }
    st.prev_escaped = (bm >> 63) & (1ULL - ((em >> 63) & 1ULL));

    const uint64_t uq = qm & ~em;

    // String-region spans via PCLMULQDQ carry-less multiply.
    uint64_t sm = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128(
                       _mm_set_epi64x(0LL, static_cast<int64_t>(uq)),
                       _mm_set1_epi8('\xFF'), 0))) ^ st.prev_string_mask;
    st.prev_string_mask = static_cast<uint64_t>(static_cast<int64_t>(sm) >> 63);

    // Scalar-start bit.
    const uint64_t scalar   = ~(qm | stm | wsm);
    const uint64_t sc_start = scalar & ~((scalar << 1) | st.prev_scalar_bit);
    st.prev_scalar_bit = scalar >> 63;

    const uint64_t combined = uq | (stm & ~sm) | (sc_start & ~sm);

    // Emit tape entries. Bracket classification uses precomputed open/close
    // bitmasks (one set of cmpeq per 64-byte block in the caller) instead of a
    // per-structural data[p] char comparison — replaces the data-dependent
    // ch=='{'||'[' / '}'||']' branches with single bit-tests on isolated bits,
    // which the branch predictor handles far better. The char is still loaded
    // for the tape token's high byte.
    uint64_t mask = combined;
    while (mask) {
        const uint64_t bbit = mask & (~mask + 1ULL);  // lowest set bit, isolated
        const uint32_t bit  = static_cast<uint32_t>(__builtin_ctzll(mask));
        mask &= mask - 1ULL;
        const uint32_t  p   = base + bit;
        const uint8_t   ch  = static_cast<uint8_t>(data[p]);
        const uint32_t  tok = (static_cast<uint32_t>(ch) << 24) | p;

        if (open_m & bbit) {
            te[si] = {tok, 0u};
            bkt_stk[++bkt_top] = static_cast<uint32_t>(si);
        } else if ((close_m & bbit) && (bkt_top >= 0)) {
            const uint32_t open = bkt_stk[bkt_top--];
            te[open].partner    = static_cast<uint32_t>(si);
            te[si]              = {tok, open};
        } else {
            te[si] = {tok, 0u};
        }
        ++si;
    }
}

// ── process_group64 [uint32_t*]: lean IndexOnly — no bracket matching ─────
// Zero bracket-stack overhead. Single DWORD store per structural token.
__attribute__((target("pclmul")))
inline void process_group64(
        scan_state& st,
        uint64_t qm, uint64_t bm, uint64_t stm, uint64_t wsm,
        uint32_t base,
        const char* __restrict__ data,
        uint32_t* __restrict__ sp, size_t& si) noexcept
{
    uint64_t em = 0, tmp_bm = bm;
    if (st.prev_escaped) em |= 1ULL;
    while (tmp_bm) { uint64_t b = tmp_bm & -tmp_bm; if (!(em & b)) em |= (b << 1); tmp_bm &= tmp_bm - 1; }
    st.prev_escaped = (bm >> 63) & (1ULL - ((em >> 63) & 1ULL));

    const uint64_t uq = qm & ~em;

    uint64_t sm = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128(
                       _mm_set_epi64x(0LL, static_cast<int64_t>(uq)),
                       _mm_set1_epi8('\xFF'), 0))) ^ st.prev_string_mask;
    st.prev_string_mask = static_cast<uint64_t>(static_cast<int64_t>(sm) >> 63);

    const uint64_t scalar   = ~(qm | stm | wsm);
    const uint64_t sc_start = scalar & ~((scalar << 1) | st.prev_scalar_bit);
    st.prev_scalar_bit = scalar >> 63;

    const uint64_t combined = uq | (stm & ~sm) | (sc_start & ~sm);

    uint64_t mask = combined;
    while (mask) {
        const uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(mask));
        mask &= mask - 1ULL;
        const uint32_t p  = base + bit;
        const uint8_t  ch = static_cast<uint8_t>(data[p]);
        sp[si++] = (static_cast<uint32_t>(ch) << 24) | p;
    }
}

// ── Scalar tail [tape_entry*]: byte-by-byte for last <64 bytes ────────────
inline void process_scalar_tail(
        scan_state& st, size_t pos, size_t len,
        const char* __restrict__ data,
        tape_entry* __restrict__ te, size_t& si,
        uint32_t* __restrict__ bkt_stk, int32_t& bkt_top) noexcept
{
    bool     in_string    = (st.prev_string_mask != 0);
    uint64_t prev_escaped = st.prev_escaped;
    while (pos < len) {
        char c = data[pos];
        if (c == '\\') {
            prev_escaped = !prev_escaped;
        } else {
            if (c == '"' && !prev_escaped) {
                const uint32_t tok = (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 24)
                                   | static_cast<uint32_t>(pos);
                uint64_t _q = static_cast<uint64_t>(tok);
                te[si++] = std::bit_cast<tape_entry>(_q);
                in_string = !in_string;
            } else if (!in_string) {
                const bool is_struct = (c=='{' || c=='}' || c=='[' || c==']' || c==':' || c==',');
                auto is_prev_delim = [](char p) noexcept {
                    return p==' '||p=='\n'||p=='\r'||p=='\t'||p=='{'||p=='}'||p=='['||p==']'||p==':'||p==','||p=='"';
                };
                const bool is_scalar_start = !is_struct && c != ' ' && c != '\n' && c != '\r' && c != '\t'
                    && (pos == 0 || is_prev_delim(data[pos - 1]));
                if (is_struct || is_scalar_start) {
                    const uint32_t tok = (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 24)
                                       | static_cast<uint32_t>(pos);
                    if (c == '{' || c == '[') {
                        uint64_t _q = static_cast<uint64_t>(tok);
                        te[si] = std::bit_cast<tape_entry>(_q);
                        bkt_stk[++bkt_top] = static_cast<uint32_t>(si);
                    } else if ((c == '}' || c == ']') && bkt_top >= 0) {
                        uint32_t open = bkt_stk[bkt_top--];
                        te[open].partner = static_cast<uint32_t>(si);
                        te[si].token   = tok;
                        te[si].partner = open;
                    } else {
                        uint64_t _q = static_cast<uint64_t>(tok);
                        te[si] = std::bit_cast<tape_entry>(_q);
                    }
                    ++si;
                }
            }
            prev_escaped = false;
        }
        ++pos;
    }
    // Sentinel: position past end, no partner
    uint64_t _sentinel = static_cast<uint64_t>(len);
    te[si] = std::bit_cast<tape_entry>(_sentinel);
}

// ── Scalar tail [uint32_t*]: lean IndexOnly — no bracket matching ─────────
inline void process_scalar_tail(
        scan_state& st, size_t pos, size_t len,
        const char* __restrict__ data,
        uint32_t* __restrict__ sp, size_t& si) noexcept
{
    bool     in_string    = (st.prev_string_mask != 0);
    uint64_t prev_escaped = st.prev_escaped;
    while (pos < len) {
        char c = data[pos];
        if (c == '\\') {
            prev_escaped = !prev_escaped;
        } else {
            if (c == '"' && !prev_escaped) {
                sp[si++] = (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 24)
                          | static_cast<uint32_t>(pos);
                in_string = !in_string;
            } else if (!in_string) {
                const bool is_struct = (c=='{' || c=='}' || c=='[' || c==']' || c==':' || c==',');
                bool is_scalar_start = false;
                if (!is_struct && c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                    if (pos == 0) {
                        is_scalar_start = true;
                    } else {
                        char p = data[pos - 1];
                        is_scalar_start = (p==' '||p=='\n'||p=='\r'||p=='\t'||p=='{'||p=='}'||p=='['||p==']'||p==':'||p==','||p=='"');
                    }
                }
                if (is_struct || is_scalar_start) {
                    sp[si++] = (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 24)
                              | static_cast<uint32_t>(pos);
                }
            }
            prev_escaped = false;
        }
        ++pos;
    }
    sp[si] = static_cast<uint32_t>(len);  // sentinel
}

// ── Scalar tail [uint32_t*]: lean OnDemand indexes ───────────────────────
inline void process_scalar_tail_indexes(
        scan_state& st, size_t pos, size_t len,
        const char* __restrict__ data,
        uint32_t* __restrict__ indexes, size_t& si) noexcept
{
    bool     in_string    = (st.prev_string_mask != 0);
    uint64_t prev_escaped = st.prev_escaped;
    while (pos < len) {
        char c = data[pos];
        if (c == '\\') {
            prev_escaped = !prev_escaped;
        } else {
            if (c == '"' && !prev_escaped) {
                if (!in_string) {
                    indexes[si++] = static_cast<uint32_t>(pos);
                }
                in_string = !in_string;
            } else if (!in_string) {
                const bool is_struct = (c=='{' || c=='}' || c=='[' || c==']' || c==':' || c==',');
                bool is_scalar_start = false;
                if (!is_struct && c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                    if (pos == 0) {
                        is_scalar_start = true;
                    } else {
                        char p = data[pos - 1];
                        is_scalar_start = (p==' '||p=='\n'||p=='\r'||p=='\t'||p=='{'||p=='}'||p=='['||p==']'||p==':'||p==','||p=='"');
                    }
                }
                if (is_struct || is_scalar_start) {
                    indexes[si++] = static_cast<uint32_t>(pos);
                }
            }
            prev_escaped = false;
        }
        ++pos;
    }
    indexes[si] = static_cast<uint32_t>(len);  // sentinel
}

// =========================================================================
// AVX2 scanner [tape_entry*] — 8× ymm = 256 bytes/iteration
// =========================================================================
__attribute__((target("avx2,pclmul")))
inline size_t build_structural_index_avx2(
        std::string_view input,
        tape_entry* __restrict__ te) noexcept
{
    const char*  data = input.data();
    const size_t len  = input.size();
    size_t pos = 0, si = 0;
    scan_state st{};
    uint32_t bkt_stk[4096]; int32_t bkt_top = -1;

    const __m256i vquote    = _mm256_set1_epi8('"');
    const __m256i vbs       = _mm256_set1_epi8('\\');
    const __m256i vlbrace   = _mm256_set1_epi8('{');
    const __m256i vlbracket = _mm256_set1_epi8('[');
    const __m256i vrbrace   = _mm256_set1_epi8('}');
    const __m256i vrbracket = _mm256_set1_epi8(']');
    const __m256i lower_tbl = _mm256_setr_epi8(
        16,0,0,0,0,0,0,0,0,32,36,1,8,34,0,0,
        16,0,0,0,0,0,0,0,0,32,36,1,8,34,0,0);
    const __m256i upper_tbl = _mm256_setr_epi8(
        32,0,24,4,0,3,0,3,0,0,0,0,0,0,0,0,
        32,0,24,4,0,3,0,3,0,0,0,0,0,0,0,0);
    const __m256i nibmask   = _mm256_set1_epi8(0x0F);
    const __m256i sc_bit    = _mm256_set1_epi8(0x0F);
    const __m256i ws_bit    = _mm256_set1_epi8(0x30);
    const __m256i vzero     = _mm256_setzero_si256();

    auto cl32 = [&](__m256i ch) __attribute__((always_inline)) -> std::pair<uint32_t, uint32_t> {
        __m256i lo = _mm256_shuffle_epi8(lower_tbl, _mm256_and_si256(ch, nibmask));
        __m256i hi = _mm256_shuffle_epi8(upper_tbl, _mm256_and_si256(_mm256_srli_epi16(ch, 4), nibmask));
        __m256i cl = _mm256_and_si256(lo, hi);
        return {
            static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpgt_epi8(_mm256_and_si256(cl, sc_bit), vzero))),
            static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpgt_epi8(_mm256_and_si256(cl, ws_bit), vzero)))
        };
    };
    auto qm64 = [&](__m256i a, __m256i b, __m256i v) __attribute__((always_inline)) -> uint64_t {
        return static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, v))) |
               (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(b, v)))) << 32);
    };

#define PG64_AVX2(c0, c1, OFF) do { \
    const uint64_t _qm  = qm64(c0, c1, vquote); \
    const uint64_t _bm  = qm64(c0, c1, vbs); \
    const uint64_t _om  = qm64(c0, c1, vlbrace) | qm64(c0, c1, vlbracket); \
    const uint64_t _cm  = qm64(c0, c1, vrbrace) | qm64(c0, c1, vrbracket); \
    const auto [_s0,_w0] = cl32(c0); const auto [_s1,_w1] = cl32(c1); \
    const uint64_t _stm = static_cast<uint64_t>(_s0) | (static_cast<uint64_t>(_s1) << 32); \
    const uint64_t _wsm = static_cast<uint64_t>(_w0) | (static_cast<uint64_t>(_w1) << 32); \
    process_group64(st, _qm, _bm, _stm, _wsm, _om, _cm, static_cast<uint32_t>(pos + (OFF)), data, te, si, bkt_stk, bkt_top); \
} while(0)

    // Main loop: 8× ymm = 256 bytes.
    while (__builtin_expect(pos + 255 < len, 1)) {
        const __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos +  32));
        const __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos +  64));
        const __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos +  96));
        const __m256i c4 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 128));
        const __m256i c5 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 160));
        const __m256i c6 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 192));
        const __m256i c7 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 224));
        PG64_AVX2(c0, c1,   0); PG64_AVX2(c2, c3,  64);
        PG64_AVX2(c4, c5, 128); PG64_AVX2(c6, c7, 192);
        pos += 256;
    }
    // Residual 64-byte chunks.
    while (pos + 63 < len) {
        const __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        PG64_AVX2(c0, c1, 0);
        pos += 64;
    }
#undef PG64_AVX2

    process_scalar_tail(st, pos, len, data, te, si, bkt_stk, bkt_top);
    return si;
}

// =========================================================================
// AVX2 scanner [uint32_t*] — lean IndexOnly, no bracket stack
// =========================================================================
__attribute__((target("avx2,pclmul")))
inline size_t build_structural_index_avx2_index(
        std::string_view input,
        uint32_t* __restrict__ sp) noexcept
{
    const char*  data = input.data();
    const size_t len  = input.size();
    size_t pos = 0, si = 0;
    scan_state st{};

    const __m256i vquote    = _mm256_set1_epi8('"');
    const __m256i vbs       = _mm256_set1_epi8('\\');
    const __m256i lower_tbl = _mm256_setr_epi8(
        16,0,0,0,0,0,0,0,0,32,36,1,8,34,0,0,
        16,0,0,0,0,0,0,0,0,32,36,1,8,34,0,0);
    const __m256i upper_tbl = _mm256_setr_epi8(
        32,0,24,4,0,3,0,3,0,0,0,0,0,0,0,0,
        32,0,24,4,0,3,0,3,0,0,0,0,0,0,0,0);
    const __m256i nibmask   = _mm256_set1_epi8(0x0F);
    const __m256i sc_bit    = _mm256_set1_epi8(0x0F);
    const __m256i ws_bit    = _mm256_set1_epi8(0x30);
    const __m256i vzero     = _mm256_setzero_si256();

    auto cl32 = [&](__m256i ch) __attribute__((always_inline)) -> std::pair<uint32_t, uint32_t> {
        __m256i lo = _mm256_shuffle_epi8(lower_tbl, _mm256_and_si256(ch, nibmask));
        __m256i hi = _mm256_shuffle_epi8(upper_tbl, _mm256_and_si256(_mm256_srli_epi16(ch, 4), nibmask));
        __m256i cl = _mm256_and_si256(lo, hi);
        return {
            static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpgt_epi8(_mm256_and_si256(cl, sc_bit), vzero))),
            static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpgt_epi8(_mm256_and_si256(cl, ws_bit), vzero)))
        };
    };
    auto qm64 = [&](__m256i a, __m256i b, __m256i v) __attribute__((always_inline)) -> uint64_t {
        return static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, v))) |
               (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(b, v)))) << 32);
    };

#define PG64_AVX2_IDX(c0, c1, OFF) do { \
    const uint64_t _qm  = qm64(c0, c1, vquote); \
    const uint64_t _bm  = qm64(c0, c1, vbs); \
    const auto [_s0,_w0] = cl32(c0); const auto [_s1,_w1] = cl32(c1); \
    const uint64_t _stm = static_cast<uint64_t>(_s0) | (static_cast<uint64_t>(_s1) << 32); \
    const uint64_t _wsm = static_cast<uint64_t>(_w0) | (static_cast<uint64_t>(_w1) << 32); \
    process_group64(st, _qm, _bm, _stm, _wsm, static_cast<uint32_t>(pos + (OFF)), data, sp, si); \
} while(0)

    while (__builtin_expect(pos + 255 < len, 1)) {
        const __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos +  32));
        const __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos +  64));
        const __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos +  96));
        const __m256i c4 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 128));
        const __m256i c5 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 160));
        const __m256i c6 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 192));
        const __m256i c7 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 224));
        PG64_AVX2_IDX(c0, c1,   0); PG64_AVX2_IDX(c2, c3,  64);
        PG64_AVX2_IDX(c4, c5, 128); PG64_AVX2_IDX(c6, c7, 192);
        pos += 256;
    }
    while (pos + 63 < len) {
        const __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        PG64_AVX2_IDX(c0, c1, 0);
        pos += 64;
    }
#undef PG64_AVX2_IDX

    process_scalar_tail(st, pos, len, data, sp, si);
    return si;
}

// =========================================================================
// AVX-512 scanner [tape_entry*] — 8× zmm = 512 bytes/iteration
//
// Key advantages over AVX2:
//   • _mm512_cmpeq_epi8_mask  → native __mmask64  (no vmovmskb)
//   • _mm512_cmpgt_epi8_mask  → native __mmask64  (no vmovmskb)
//   • 2× more bytes per outer loop iteration
//   • Residual 64-byte chunks handled by single zmm load
// =========================================================================
__attribute__((target("avx512f,avx512bw,pclmul"), aligned(64)))
inline size_t build_structural_index_avx512(
        std::string_view input,
        tape_entry* __restrict__ te) noexcept
{
    const char*  data = input.data();
    const size_t len  = input.size();
    size_t pos = 0, si = 0;
    scan_state st{};
    uint32_t bkt_stk[4096]; int32_t bkt_top = -1;

    const __m512i vquote512 = _mm512_set1_epi8('"');
    const __m512i vbs512    = _mm512_set1_epi8('\\');
    const __m512i vlbrace   = _mm512_set1_epi8('{');
    const __m512i vlbracket = _mm512_set1_epi8('[');
    const __m512i vrbrace   = _mm512_set1_epi8('}');
    const __m512i vrbracket = _mm512_set1_epi8(']');

    // Broadcast 16-byte lookup tables into all 4 zmm lanes.
    const __m512i lower_tbl = _mm512_broadcast_i32x4(
        _mm_setr_epi8(16,0,0,0,0,0,0,0,0,32,36,1,8,34,0,0));
    const __m512i upper_tbl = _mm512_broadcast_i32x4(
        _mm_setr_epi8(32,0,24,4,0,3,0,3,0,0,0,0,0,0,0,0));
    const __m512i nibmask   = _mm512_set1_epi8(0x0F);
    const __m512i sc_bit    = _mm512_set1_epi8(0x0F);
    const __m512i ws_bit    = _mm512_set1_epi8(0x30);
    const __m512i vzero     = _mm512_setzero_si512();

    // One zmm → (64-bit struct mask, 64-bit ws mask) — inlined per-register.
    // NOTE: macro so all intrinsics expand inside this avx512-targeted function body.
    //       A lambda's operator() does not inherit __attribute__((target(...))).
#define CL64_AVX512(zmm, stm_out, wsm_out) do { \
    __m512i _lo = _mm512_shuffle_epi8(lower_tbl, _mm512_and_si512(zmm, nibmask)); \
    __m512i _hi = _mm512_shuffle_epi8(upper_tbl, \
                      _mm512_and_si512(_mm512_srli_epi16(zmm, 4), nibmask)); \
    __m512i _cl = _mm512_and_si512(_lo, _hi); \
    stm_out = static_cast<uint64_t>(_mm512_cmpgt_epi8_mask( \
                  _mm512_and_si512(_cl, sc_bit), vzero)); \
    wsm_out = static_cast<uint64_t>(_mm512_cmpgt_epi8_mask( \
                  _mm512_and_si512(_cl, ws_bit), vzero)); \
} while(0)

#define PG64_AVX512(zmm, OFF) do { \
    const uint64_t _qm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(zmm, vquote512)); \
    const uint64_t _bm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(zmm, vbs512)); \
    const uint64_t _om = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(zmm, vlbrace)) \
                       | static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(zmm, vlbracket)); \
    const uint64_t _cm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(zmm, vrbrace)) \
                       | static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(zmm, vrbracket)); \
    uint64_t _stm = 0, _wsm = 0; \
    CL64_AVX512(zmm, _stm, _wsm); \
    process_group64(st, _qm, _bm, _stm, _wsm, _om, _cm, \
                    static_cast<uint32_t>(pos + (OFF)), \
                    data, te, si, bkt_stk, bkt_top); \
} while(0)

    // Main loop: 8× zmm = 512 bytes.
    while (__builtin_expect(pos + 511 < len, 1)) {
        const __m512i c0 = _mm512_loadu_si512(static_cast<const void*>(data + pos));
        const __m512i c1 = _mm512_loadu_si512(static_cast<const void*>(data + pos +  64));
        const __m512i c2 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 128));
        const __m512i c3 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 192));
        const __m512i c4 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 256));
        const __m512i c5 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 320));
        const __m512i c6 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 384));
        const __m512i c7 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 448));
        PG64_AVX512(c0,   0); PG64_AVX512(c1,  64);
        PG64_AVX512(c2, 128); PG64_AVX512(c3, 192);
        PG64_AVX512(c4, 256); PG64_AVX512(c5, 320);
        PG64_AVX512(c6, 384); PG64_AVX512(c7, 448);
        pos += 512;
    }
    // Residual 64-byte chunks via single zmm load.
    while (pos + 63 < len) {
        const __m512i c = _mm512_loadu_si512(static_cast<const void*>(data + pos));
        PG64_AVX512(c, 0);
        pos += 64;
    }

    process_scalar_tail(st, pos, len, data, te, si, bkt_stk, bkt_top);
    return si;
}
#undef CL64_AVX512
#undef PG64_AVX512

// =========================================================================
// AVX-512 scanner [uint32_t*] — lean IndexOnly, no bracket stack
// =========================================================================
__attribute__((target("avx512f,avx512bw,pclmul")))
inline size_t build_structural_index_avx512_index(
        std::string_view input,
        uint32_t* __restrict__ sp) noexcept
{
    const char*  data = input.data();
    const size_t len  = input.size();
    size_t pos = 0, si = 0;
    scan_state st{};

    const __m512i vquote512 = _mm512_set1_epi8('"');
    const __m512i vbs512    = _mm512_set1_epi8('\\');

    const __m512i lower_tbl = _mm512_broadcast_i32x4(
        _mm_setr_epi8(16,0,0,0,0,0,0,0,0,32,36,1,8,34,0,0));
    const __m512i upper_tbl = _mm512_broadcast_i32x4(
        _mm_setr_epi8(32,0,24,4,0,3,0,3,0,0,0,0,0,0,0,0));
    const __m512i nibmask   = _mm512_set1_epi8(0x0F);
    const __m512i sc_bit    = _mm512_set1_epi8(0x0F);
    const __m512i ws_bit    = _mm512_set1_epi8(0x30);
    const __m512i vzero     = _mm512_setzero_si512();

#define CL64_AVX512_IDX(zmm, stm_out, wsm_out) do { \
    __m512i _lo = _mm512_shuffle_epi8(lower_tbl, _mm512_and_si512(zmm, nibmask)); \
    __m512i _hi = _mm512_shuffle_epi8(upper_tbl, \
                      _mm512_and_si512(_mm512_srli_epi16(zmm, 4), nibmask)); \
    __m512i _cl = _mm512_and_si512(_lo, _hi); \
    stm_out = static_cast<uint64_t>(_mm512_cmpgt_epi8_mask( \
                  _mm512_and_si512(_cl, sc_bit), vzero)); \
    wsm_out = static_cast<uint64_t>(_mm512_cmpgt_epi8_mask( \
                  _mm512_and_si512(_cl, ws_bit), vzero)); \
} while(0)

#define PG64_AVX512_IDX(zmm, OFF) do { \
    const uint64_t _qm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(zmm, vquote512)); \
    const uint64_t _bm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(zmm, vbs512)); \
    uint64_t _stm = 0, _wsm = 0; \
    CL64_AVX512_IDX(zmm, _stm, _wsm); \
    process_group64(st, _qm, _bm, _stm, _wsm, \
                    static_cast<uint32_t>(pos + (OFF)), \
                    data, sp, si); \
} while(0)

    while (__builtin_expect(pos + 511 < len, 1)) {
        const __m512i c0 = _mm512_loadu_si512(static_cast<const void*>(data + pos));
        const __m512i c1 = _mm512_loadu_si512(static_cast<const void*>(data + pos +  64));
        const __m512i c2 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 128));
        const __m512i c3 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 192));
        const __m512i c4 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 256));
        const __m512i c5 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 320));
        const __m512i c6 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 384));
        const __m512i c7 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 448));
        PG64_AVX512_IDX(c0,   0); PG64_AVX512_IDX(c1,  64);
        PG64_AVX512_IDX(c2, 128); PG64_AVX512_IDX(c3, 192);
        PG64_AVX512_IDX(c4, 256); PG64_AVX512_IDX(c5, 320);
        PG64_AVX512_IDX(c6, 384); PG64_AVX512_IDX(c7, 448);
        pos += 512;
    }
    while (pos + 63 < len) {
        const __m512i c = _mm512_loadu_si512(static_cast<const void*>(data + pos));
        PG64_AVX512_IDX(c, 0);
        pos += 64;
    }
#undef CL64_AVX512_IDX
#undef PG64_AVX512_IDX

    process_scalar_tail(st, pos, len, data, sp, si);
    return si;
}

// =========================================================================
// Decoupled two-phase tape build (AVX-512 only), L2-tiled.
//
// Phase 1 (per 32 KB input tile): lean token scan → uint32_t tokens
//          (ch<<24 | pos) in a small scratch buffer at full SIMD scan speed
//          (no per-structural 8-byte store / bracket branch interleaved).
// Phase 2 (same chunk, tokens still L1/L2-hot): expand tokens → tape entries
//          {token, partner=0}. SIMD: one zmm load = 16 tokens, 2× vpmovzxdq
//          (uint32 → uint64 zero-extend == little-endian {token, 0u}) =
//          2 zmm stores.
// Phase 3 (fused into the same 16-token iteration): bracket-partner patch.
//          (tok>>24)|0x20 maps '{','[' → 0x7B and '}',']' → 0x7D (and no
//          other byte to either), so two vpcmpeqd masks find all brackets;
//          set bits replay the exact stack logic of the fused emit at
//          process_group64[tape_entry*], including the unmatched-close case
//          (close with empty stack → partner stays 0, nothing popped).
//
// The tiling is the fix for the v5.1 two-pass rejection: a full-input token
// buffer (~0.9 MB for the 628 KB benchmark) is re-read at L3 latency, which
// cost more than the fused emit saved. A 32 KB tile's tokens (≤128 KB worst
// case, ~96 KB typical) stay L2-resident between the two phases.
// =========================================================================

// ── Phase 2+3 over one chunk of tokens. base = global tape index of
//    tokens[0]; bracket stack persists across chunks. Returns base+count.
__attribute__((target("avx512f,avx512bw")))
inline size_t expand_tokens_avx512(
        const uint32_t* __restrict__ tokens, size_t count, size_t base,
        tape_entry* __restrict__ te,
        uint32_t* __restrict__ bkt_stk, int32_t& bkt_top) noexcept
{
    const __m512i v20 = _mm512_set1_epi32(0x20);
    const __m512i v7B = _mm512_set1_epi32(0x7B);   // '{' and '['|0x20
    const __m512i v7D = _mm512_set1_epi32(0x7D);   // '}' and ']'|0x20

    size_t i = 0;
    for (; i + 16 <= count; i += 16) {
        const __m512i t  = _mm512_loadu_si512(static_cast<const void*>(tokens + i));
        const __m256i lo = _mm512_castsi512_si256(t);
        const __m256i hi = _mm512_extracti64x4_epi64(t, 1);
        _mm512_storeu_si512(static_cast<void*>(te + base + i),     _mm512_cvtepu32_epi64(lo));
        _mm512_storeu_si512(static_cast<void*>(te + base + i + 8), _mm512_cvtepu32_epi64(hi));

        const __m512i ch = _mm512_or_si512(_mm512_srli_epi32(t, 24), v20);
        const uint32_t open_m  = _mm512_cmpeq_epi32_mask(ch, v7B);
        const uint32_t close_m = _mm512_cmpeq_epi32_mask(ch, v7D);
        uint32_t both = open_m | close_m;
        while (both) {
            const uint32_t j  = static_cast<uint32_t>(__builtin_ctz(both));
            both &= both - 1u;
            const uint32_t si = static_cast<uint32_t>(base + i) + j;
            if (open_m & (1u << j)) {
                bkt_stk[++bkt_top] = si;
            } else if (bkt_top >= 0) {
                const uint32_t open = bkt_stk[bkt_top--];
                te[open].partner    = si;
                te[si].partner      = open;
            }
        }
    }
    // Scalar tail (< 16 tokens).
    for (; i < count; ++i) {
        const uint32_t tok = tokens[i];
        const size_t   si  = base + i;
        te[si] = {tok, 0u};
        const uint32_t ch = (tok >> 24) | 0x20u;
        if (ch == 0x7Bu) {
            bkt_stk[++bkt_top] = static_cast<uint32_t>(si);
        } else if (ch == 0x7Du && bkt_top >= 0) {
            const uint32_t open = bkt_stk[bkt_top--];
            te[open].partner    = static_cast<uint32_t>(si);
            te[si].partner      = open;
        }
    }
    return base + count;
}

__attribute__((target("avx2")))
inline size_t expand_tokens_avx2(
        const uint32_t* __restrict__ tokens, size_t count, size_t base,
        tape_entry* __restrict__ te,
        uint32_t* __restrict__ bkt_stk, int32_t& bkt_top) noexcept
{
    const __m256i v20 = _mm256_set1_epi32(0x20);
    const __m256i v7B = _mm256_set1_epi32(0x7B);   // '{' and '['|0x20
    const __m256i v7D = _mm256_set1_epi32(0x7D);   // '}' and ']'|0x20

    size_t i = 0;
    for (; i + 16 <= count; i += 16) {
        const __m256i t0  = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(tokens + i));
        const __m256i t1  = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(tokens + i + 8));

        const __m128i lo0 = _mm256_castsi256_si128(t0);
        const __m128i hi0 = _mm256_extracti128_si256(t0, 1);
        const __m128i lo1 = _mm256_castsi256_si128(t1);
        const __m128i hi1 = _mm256_extracti128_si256(t1, 1);

        _mm256_storeu_si256(reinterpret_cast<__m256i*>(te + base + i),      _mm256_cvtepu32_epi64(lo0));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(te + base + i + 4),  _mm256_cvtepu32_epi64(hi0));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(te + base + i + 8),  _mm256_cvtepu32_epi64(lo1));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(te + base + i + 12), _mm256_cvtepu32_epi64(hi1));

        const __m256i ch0 = _mm256_or_si256(_mm256_srli_epi32(t0, 24), v20);
        const __m256i ch1 = _mm256_or_si256(_mm256_srli_epi32(t1, 24), v20);

        const uint32_t open_m0  = static_cast<uint32_t>(_mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpeq_epi32(ch0, v7B))));
        const uint32_t close_m0 = static_cast<uint32_t>(_mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpeq_epi32(ch0, v7D))));
        const uint32_t open_m1  = static_cast<uint32_t>(_mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpeq_epi32(ch1, v7B))));
        const uint32_t close_m1 = static_cast<uint32_t>(_mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpeq_epi32(ch1, v7D))));

        const uint32_t open_m  = open_m0 | (open_m1 << 8);
        const uint32_t close_m = close_m0 | (close_m1 << 8);
        uint32_t both = open_m | close_m;
        while (both) {
            const uint32_t j  = static_cast<uint32_t>(__builtin_ctz(both));
            both &= both - 1u;
            const uint32_t si = static_cast<uint32_t>(base + i) + j;
            if (open_m & (1u << j)) {
                bkt_stk[++bkt_top] = si;
            } else if (bkt_top >= 0) {
                const uint32_t open = bkt_stk[bkt_top--];
                te[open].partner    = si;
                te[si].partner      = open;
            }
        }
    }
    // Scalar tail (< 16 tokens).
    for (; i < count; ++i) {
        const uint32_t tok = tokens[i];
        const size_t   si  = base + i;
        te[si] = {tok, 0u};
        const uint32_t ch = (tok >> 24) | 0x20u;
        if (ch == 0x7Bu) {
            bkt_stk[++bkt_top] = static_cast<uint32_t>(si);
        } else if (ch == 0x7Du && bkt_top >= 0) {
            const uint32_t open = bkt_stk[bkt_top--];
            te[open].partner    = static_cast<uint32_t>(si);
            te[si].partner      = open;
        }
    }
    return base + count;
}

__attribute__((target("avx2,pclmul")))
inline uint64_t compute_mask64_common(
        scan_state& st,
        uint64_t qm, uint64_t bm, uint64_t stm, uint64_t wsm) noexcept
{
    uint64_t em = 0;
    if (__builtin_expect((bm | st.prev_escaped) != 0ULL, 0)) {
        uint64_t tmp_bm = bm;
        if (st.prev_escaped) em |= 1ULL;
        while (tmp_bm) { uint64_t b = tmp_bm & -tmp_bm; if (!(em & b)) em |= (b << 1); tmp_bm &= tmp_bm - 1; }
        st.prev_escaped = (bm >> 63) & (1ULL - ((em >> 63) & 1ULL));
    }

    const uint64_t uq = qm & ~em;

    uint64_t sm = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128(
                       _mm_set_epi64x(0LL, static_cast<int64_t>(uq)),
                       _mm_set1_epi8('\xFF'), 0))) ^ st.prev_string_mask;
    st.prev_string_mask = static_cast<uint64_t>(static_cast<int64_t>(sm) >> 63);

    const uint64_t scalar   = ~(qm | stm | wsm);
    const uint64_t sc_start = scalar & ~((scalar << 1) | st.prev_scalar_bit);
    st.prev_scalar_bit = scalar >> 63;

    return uq | (stm & ~sm) | (sc_start & ~sm);
}

__attribute__((target("avx2,pclmul")))
inline uint64_t compute_mask64_nobs(
        scan_state& st,
        uint64_t qm, uint64_t stm, uint64_t wsm) noexcept
{
    uint64_t sm = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128(
                       _mm_set_epi64x(0LL, static_cast<int64_t>(qm)),
                       _mm_set1_epi8('\xFF'), 0))) ^ st.prev_string_mask;
    st.prev_string_mask = static_cast<uint64_t>(static_cast<int64_t>(sm) >> 63);

    const uint64_t scalar   = ~(qm | stm | wsm);
    const uint64_t sc_start = scalar & ~((scalar << 1) | st.prev_scalar_bit);
    st.prev_scalar_bit = scalar >> 63;

    return qm | (stm & ~sm) | (sc_start & ~sm);
}

__attribute__((target("avx2,pclmul")))
inline uint64_t compute_mask64_common_nonscalar(
        scan_state& st,
        uint64_t qm, uint64_t bm, uint64_t stm, uint64_t non_scalar) noexcept
{
    uint64_t em = 0;
    if (__builtin_expect((bm | st.prev_escaped) != 0ULL, 0)) {
        uint64_t tmp_bm = bm;
        if (st.prev_escaped) em |= 1ULL;
        while (tmp_bm) { uint64_t b = tmp_bm & -tmp_bm; if (!(em & b)) em |= (b << 1); tmp_bm &= tmp_bm - 1; }
        st.prev_escaped = (bm >> 63) & (1ULL - ((em >> 63) & 1ULL));
    }

    const uint64_t uq = qm & ~em;

    uint64_t sm = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128(
                       _mm_set_epi64x(0LL, static_cast<int64_t>(uq)),
                       _mm_set1_epi8('\xFF'), 0))) ^ st.prev_string_mask;
    st.prev_string_mask = static_cast<uint64_t>(static_cast<int64_t>(sm) >> 63);

    const uint64_t scalar   = ~non_scalar;
    const uint64_t sc_start = scalar & ~((scalar << 1) | st.prev_scalar_bit);
    st.prev_scalar_bit = scalar >> 63;

    return uq | (stm & ~sm) | (sc_start & ~sm);
}

// ── Phase 1 over one chunk: scan whole 64-byte blocks in [pos, end) into tile_masks (AVX-512).
__attribute__((target("avx512f,avx512bw,pclmul")))
inline size_t scan_masks_avx512_chunk(
        const char* __restrict__ data, size_t pos, size_t end,
        scan_state& st, uint64_t* __restrict__ masks) noexcept
{
    size_t bi = 0;

    const __m512i vquote512 = _mm512_set1_epi8('"');
    const __m512i vbs512    = _mm512_set1_epi8('\\');

    const __m512i lower_tbl = _mm512_broadcast_i32x4(
        _mm_setr_epi8(16,0,0,0,0,0,0,0,0,32,36,1,8,34,0,0));
    const __m512i upper_tbl = _mm512_broadcast_i32x4(
        _mm_setr_epi8(32,0,24,4,0,3,0,3,0,0,0,0,0,0,0,0));
    const __m512i nibmask   = _mm512_set1_epi8(0x0F);
    const __m512i sc_bit    = _mm512_set1_epi8(0x0F);
    const __m512i ws_bit    = _mm512_set1_epi8(0x30);
    const __m512i vzero     = _mm512_setzero_si512();

#define CL64_AVX512_CHK(zmm, stm_out, wsm_out) do { \
    __m512i _lo = _mm512_shuffle_epi8(lower_tbl, _mm512_and_si512(zmm, nibmask)); \
    __m512i _hi = _mm512_shuffle_epi8(upper_tbl, \
                      _mm512_and_si512(_mm512_srli_epi16(zmm, 4), nibmask)); \
    __m512i _cl = _mm512_and_si512(_lo, _hi); \
    stm_out = static_cast<uint64_t>(_mm512_cmpgt_epi8_mask( \
                  _mm512_and_si512(_cl, sc_bit), vzero)); \
    wsm_out = static_cast<uint64_t>(_mm512_cmpgt_epi8_mask( \
                  _mm512_and_si512(_cl, ws_bit), vzero)); \
} while(0)

#define CM64_AVX512(zmm) do { \
    const uint64_t _qm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(zmm, vquote512)); \
    const uint64_t _bm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(zmm, vbs512)); \
    uint64_t _stm = 0, _wsm = 0; \
    CL64_AVX512_CHK(zmm, _stm, _wsm); \
    masks[bi++] = compute_mask64_common(st, _qm, _bm, _stm, _wsm); \
} while(0)

    while (__builtin_expect(pos + 511 < end, 1)) {
        const __m512i c0 = _mm512_loadu_si512(static_cast<const void*>(data + pos));
        const __m512i c1 = _mm512_loadu_si512(static_cast<const void*>(data + pos +  64));
        const __m512i c2 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 128));
        const __m512i c3 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 192));
        const __m512i c4 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 256));
        const __m512i c5 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 320));
        const __m512i c6 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 384));
        const __m512i c7 = _mm512_loadu_si512(static_cast<const void*>(data + pos + 448));
        CM64_AVX512(c0); CM64_AVX512(c1);
        CM64_AVX512(c2); CM64_AVX512(c3);
        CM64_AVX512(c4); CM64_AVX512(c5);
        CM64_AVX512(c6); CM64_AVX512(c7);
        pos += 512;
    }
    while (pos + 63 < end) {
        const __m512i c = _mm512_loadu_si512(static_cast<const void*>(data + pos));
        CM64_AVX512(c);
        pos += 64;
    }
#undef CL64_AVX512_CHK
#undef CM64_AVX512

    return bi;
}

// ── Decoupled driver: L1-tiled phase-1 mask scan / phase-2 tape build (AVX-512) ────
inline size_t build_tape_decoupled_avx512(
        std::string_view input,
        uint32_t* __restrict__ tokens,
        tape_entry* __restrict__ te) noexcept
{
    constexpr size_t chunk_size = decoupled_chunk_size;
    constexpr size_t max_blocks_per_chunk = chunk_size / 64;

    const char*  data = input.data();
    const size_t len  = input.size();
    scan_state st{};
    uint32_t bkt_stk[4096]; int32_t bkt_top = -1;
    uint64_t tile_masks[max_blocks_per_chunk];

    size_t si  = 0;
    size_t pos = 0;
    const size_t blocks_end = len & ~static_cast<size_t>(63);
    while (pos < blocks_end) {
        const size_t chunk_end = (blocks_end - pos > chunk_size)
                                     ? pos + chunk_size : blocks_end;
        const size_t num_blocks = scan_masks_avx512_chunk(data, pos, chunk_end, st, tile_masks);
        
        for (size_t b = 0; b < num_blocks; ++b) {
            uint64_t mask = tile_masks[b];
            const uint32_t base = static_cast<uint32_t>(pos + b * 64);
            while (mask) {
                const uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(mask));
                mask &= mask - 1ULL;
                const uint32_t p  = base + bit;
                const uint8_t  ch = static_cast<uint8_t>(data[p]);
                const uint32_t tok = (static_cast<uint32_t>(ch) << 24) | p;

                const uint32_t bch = ch | 0x20u;
                if (__builtin_expect(bch == 0x7Bu, 0)) {
                    te[si] = {tok, 0u};
                    bkt_stk[++bkt_top] = static_cast<uint32_t>(si);
                } else if (__builtin_expect(bch == 0x7Du, 0)) {
                    if (bkt_top >= 0) {
                        const uint32_t open = bkt_stk[bkt_top--];
                        te[open].partner    = static_cast<uint32_t>(si);
                        te[si]              = {tok, open};
                    } else {
                        te[si] = {tok, 0u};
                    }
                } else {
                    te[si] = {tok, 0u};
                }
                ++si;
            }
        }
        pos = chunk_end;
    }
    process_scalar_tail(st, pos, len, data, te, si, bkt_stk, bkt_top);

    te[si] = {static_cast<uint32_t>(len), 0u};
    return si;
}

#define WRITE_IDX(base_idx, i) do { \
    tail[i] = (base_idx) + static_cast<uint32_t>(_tzcnt_u64(bits)); \
    bits = _blsr_u64(bits); \
} while(0)

[[gnu::always_inline]] inline void flatten_mask(uint32_t idx, uint64_t bits, uint32_t* __restrict__& tail) noexcept {
    if (!bits) return;
    const int cnt = static_cast<int>(__builtin_popcountll(bits));
    WRITE_IDX(idx, 0);
    WRITE_IDX(idx, 1);
    if (2 < cnt) {
        WRITE_IDX(idx, 2);
        WRITE_IDX(idx, 3);
        if (4 < cnt) {
            WRITE_IDX(idx, 4);
            WRITE_IDX(idx, 5);
            if (6 < cnt) {
                WRITE_IDX(idx, 6);
                WRITE_IDX(idx, 7);
                if (8 < cnt) {
                    WRITE_IDX(idx, 8);
                    WRITE_IDX(idx, 9);
                    if (10 < cnt) {
                        WRITE_IDX(idx, 10);
                        WRITE_IDX(idx, 11);
                        if (__builtin_expect(12 < cnt, 0)) {
                            for (int i = 12; i < cnt; ++i) {
                                WRITE_IDX(idx, i);
                            }
                        }
                    }
                }
            }
        }
    }
    tail += cnt;
}

#undef WRITE_IDX

// ── OnDemand driver: direct streaming interleaved mask scan / index store (AVX-512) ────
__attribute__((target("avx512f,avx512bw,pclmul")))
inline size_t build_indexes_avx512(
        std::string_view input,
        uint32_t* __restrict__ indexes) noexcept
{
    const char*  data = input.data();
    const size_t len  = input.size();
    uint64_t prev_escaped     = 0;
    uint64_t prev_string_mask = 0;
    uint64_t prev_scalar_bit  = 0;
    uint64_t prev_bits        = 0;
    uint32_t* tail = indexes;

    const __m512i vquote512 = _mm512_set1_epi8('"');
    const __m512i vbs512    = _mm512_set1_epi8('\\');
    const __m512i v0x20_512 = _mm512_set1_epi8(0x20);
    const __m128i vall_ones = _mm_set1_epi8('\xFF');

    const __m512i whitespace_table = _mm512_broadcast_i32x4(
        _mm_setr_epi8(' ', 100, 100, 100, 17, 100, 113, 2, 100, '\t', '\n', 112, 100, '\r', 100, 100));
    const __m512i op_table = _mm512_broadcast_i32x4(
        _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ':', '{', ',', '}', 0, 0));

#define STEP64_AVX512(PTR, BASE_IDX) do { \
    const __m512i _c = _mm512_loadu_si512(static_cast<const void*>(PTR)); \
    uint64_t _uq = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(_c, vquote512)); \
    const uint64_t _qm = _uq; \
    const uint64_t _bm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(_c, vbs512)); \
    if (__builtin_expect((_bm | prev_escaped) != 0ULL, 0)) { \
        uint64_t _em = prev_escaped, _tmp_bm = _bm; \
        while (_tmp_bm) { uint64_t _b = _tmp_bm & -_tmp_bm; if (!(_em & _b)) _em |= (_b << 1); _tmp_bm &= _tmp_bm - 1; } \
        prev_escaped = (_bm >> 63) & (1ULL - ((_em >> 63) & 1ULL)); \
        _uq &= ~_em; \
    } \
    const uint64_t _wsm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(_c, _mm512_shuffle_epi8(whitespace_table, _c))); \
    const uint64_t _stm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(_mm512_or_si512(_c, v0x20_512), _mm512_shuffle_epi8(op_table, _c))); \
    const uint64_t _sm = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128( \
                             _mm_set_epi64x(0LL, static_cast<int64_t>(_uq)), \
                             vall_ones, 0))) ^ prev_string_mask; \
    prev_string_mask = static_cast<uint64_t>(static_cast<int64_t>(_sm) >> 63); \
    const uint64_t _scalar   = ~(_qm | _stm | _wsm); \
    const uint64_t _sc_start = _scalar & ~((_scalar << 1) | prev_scalar_bit); \
    prev_scalar_bit = _scalar >> 63; \
    uint64_t _next_bits = (_uq & _sm) | ((_stm | _sc_start) & ~_sm); \
    asm volatile("" : "+r"(_next_bits)); \
    flatten_mask(static_cast<uint32_t>((BASE_IDX) - 64), prev_bits, tail); \
    prev_bits = _next_bits; \
} while(0)

    size_t pos = 0;
    const size_t blocks_end = len & ~static_cast<size_t>(63);
    while (__builtin_expect(pos + 255 < blocks_end, 1)) {
        STEP64_AVX512(data + pos,       pos);
        STEP64_AVX512(data + pos + 64,  pos + 64);
        STEP64_AVX512(data + pos + 128, pos + 128);
        STEP64_AVX512(data + pos + 192, pos + 192);
        pos += 256;
    }
    while (pos < blocks_end) {
        STEP64_AVX512(data + pos, pos);
        pos += 64;
    }
    if (pos < len) {
        alignas(64) char tail_buf[64];
        _mm512_store_si512(static_cast<void*>(tail_buf), v0x20_512);
        std::memcpy(tail_buf, data + pos, len - pos);
        STEP64_AVX512(tail_buf, pos);
        pos += 64;
    }
    flatten_mask(static_cast<uint32_t>(pos - 64), prev_bits, tail);
#undef STEP64_AVX512

    size_t si = static_cast<size_t>(tail - indexes);
    indexes[si] = static_cast<uint32_t>(len);
    return si;
}

// ── Phase 1 over one chunk (AVX2): pure SIMD mask generation (512 uint64 masks = 32KB tile)
__attribute__((target("avx2,pclmul")))
inline size_t scan_masks_avx2_chunk(
        const char* __restrict__ data, size_t pos, size_t end,
        scan_state& st, uint64_t* __restrict__ masks) noexcept
{
    size_t bi = 0;

    const __m256i vquote    = _mm256_set1_epi8('"');
    const __m256i vbs       = _mm256_set1_epi8('\\');
    const __m256i lower_tbl = _mm256_setr_epi8(
        16,0,0,0,0,0,0,0,0,32,36,1,8,34,0,0,
        16,0,0,0,0,0,0,0,0,32,36,1,8,34,0,0);
    const __m256i upper_tbl = _mm256_setr_epi8(
        32,0,24,4,0,3,0,3,0,0,0,0,0,0,0,0,
        32,0,24,4,0,3,0,3,0,0,0,0,0,0,0,0);
    const __m256i nibmask   = _mm256_set1_epi8(0x0F);
    const __m256i sc_bit    = _mm256_set1_epi8(0x0F);
    const __m256i ws_bit    = _mm256_set1_epi8(0x30);
    const __m256i vzero     = _mm256_setzero_si256();

    auto cl32 = [&](__m256i ch) __attribute__((always_inline)) -> std::pair<uint32_t, uint32_t> {
        __m256i lo = _mm256_shuffle_epi8(lower_tbl, _mm256_and_si256(ch, nibmask));
        __m256i hi = _mm256_shuffle_epi8(upper_tbl, _mm256_and_si256(_mm256_srli_epi16(ch, 4), nibmask));
        __m256i cl = _mm256_and_si256(lo, hi);
        return {
            static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpgt_epi8(_mm256_and_si256(cl, sc_bit), vzero))),
            static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpgt_epi8(_mm256_and_si256(cl, ws_bit), vzero)))
        };
    };
    auto qm64 = [&](__m256i a, __m256i b, __m256i v) __attribute__((always_inline)) -> uint64_t {
        return static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, v))) |
               (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(b, v)))) << 32);
    };

#define CM64_AVX2(c0, c1) do { \
    const uint64_t _qm  = qm64(c0, c1, vquote); \
    const uint64_t _bm  = qm64(c0, c1, vbs); \
    const auto [_s0,_w0] = cl32(c0); const auto [_s1,_w1] = cl32(c1); \
    const uint64_t _stm = static_cast<uint64_t>(_s0) | (static_cast<uint64_t>(_s1) << 32); \
    const uint64_t _wsm = static_cast<uint64_t>(_w0) | (static_cast<uint64_t>(_w1) << 32); \
    masks[bi++] = compute_mask64_common(st, _qm, _bm, _stm, _wsm); \
} while(0)

    while (__builtin_expect(pos + 255 < end, 1)) {
        const __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos +  32));
        const __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos +  64));
        const __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos +  96));
        const __m256i c4 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 128));
        const __m256i c5 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 160));
        const __m256i c6 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 192));
        const __m256i c7 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 224));
        CM64_AVX2(c0, c1); CM64_AVX2(c2, c3);
        CM64_AVX2(c4, c5); CM64_AVX2(c6, c7);
        pos += 256;
    }
    while (pos + 63 < end) {
        const __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
        CM64_AVX2(c0, c1);
        pos += 64;
    }
#undef CM64_AVX2

    return bi;
}

// ── Decoupled driver for AVX2: L1-tiled phase-1 mask scan / phase-2 tape build ────
inline size_t build_tape_decoupled_avx2(
        std::string_view input,
        uint32_t* __restrict__ tokens,
        tape_entry* __restrict__ te) noexcept
{
    constexpr size_t chunk_size = decoupled_chunk_size;
    constexpr size_t max_blocks_per_chunk = chunk_size / 64;

    const char*  data = input.data();
    const size_t len  = input.size();
    scan_state st{};
    uint32_t bkt_stk[4096]; int32_t bkt_top = -1;
    uint64_t tile_masks[max_blocks_per_chunk];

    size_t si  = 0;
    size_t pos = 0;
    const size_t blocks_end = len & ~static_cast<size_t>(63);
    while (pos < blocks_end) {
        const size_t chunk_end = (blocks_end - pos > chunk_size)
                                     ? pos + chunk_size : blocks_end;
        const size_t num_blocks = scan_masks_avx2_chunk(data, pos, chunk_end, st, tile_masks);
        
        // Phase 2: Extract tokens from tile_masks, load char from L1-resident data, build tape
        for (size_t b = 0; b < num_blocks; ++b) {
            uint64_t mask = tile_masks[b];
            const uint32_t base = static_cast<uint32_t>(pos + b * 64);
            while (mask) {
                const uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(mask));
                mask &= mask - 1ULL;
                const uint32_t p  = base + bit;
                const uint8_t  ch = static_cast<uint8_t>(data[p]);
                const uint32_t tok = (static_cast<uint32_t>(ch) << 24) | p;
                const uint32_t bch = ch | 0x20u;
                if (__builtin_expect(bch == 0x7Bu, 0)) {
                    te[si] = {tok, 0u};
                    bkt_stk[++bkt_top] = static_cast<uint32_t>(si);
                } else if (__builtin_expect(bch == 0x7Du, 0)) {
                    if (bkt_top >= 0) {
                        const uint32_t open = bkt_stk[bkt_top--];
                        te[open].partner    = static_cast<uint32_t>(si);
                        te[si]              = {tok, open};
                    } else {
                        te[si] = {tok, 0u};
                    }
                } else {
                    te[si] = {tok, 0u};
                }
                ++si;
            }
        }
        pos = chunk_end;
    }
    process_scalar_tail(st, pos, len, data, te, si, bkt_stk, bkt_top);

    te[si] = {static_cast<uint32_t>(len), 0u};
    return si;
}

// ── OnDemand driver: direct streaming interleaved mask scan / index store (AVX2) ────
__attribute__((target("avx2,pclmul")))
inline size_t build_indexes_avx2(
        std::string_view input,
        uint32_t* __restrict__ indexes) noexcept
{
    const char*  data = input.data();
    const size_t len  = input.size();
    uint64_t prev_escaped     = 0;
    uint64_t prev_string_mask = 0;
    uint64_t prev_scalar_bit  = 0;
    uint64_t prev_bits        = 0;
    uint32_t* tail = indexes;

    const __m256i vquote    = _mm256_set1_epi8('"');
    const __m256i vbs       = _mm256_set1_epi8('\\');
    const __m256i v0x20     = _mm256_set1_epi8(0x20);
    const __m128i vall_ones = _mm_set1_epi8('\xFF');

    const __m256i whitespace_table = _mm256_setr_epi8(
        ' ', 100, 100, 100, 17, 100, 113, 2, 100, '\t', '\n', 112, 100, '\r', 100, 100,
        ' ', 100, 100, 100, 17, 100, 113, 2, 100, '\t', '\n', 112, 100, '\r', 100, 100
    );
    const __m256i op_table = _mm256_setr_epi8(
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ':', '{', ',', '}', 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ':', '{', ',', '}', 0, 0
    );

#define STEP64_AVX2(PTR, BASE_IDX) do { \
    const __m256i _c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(PTR)); \
    const __m256i _c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>((PTR) + 32)); \
    const __m256i _q0 = _mm256_cmpeq_epi8(_c0, vquote); \
    const __m256i _q1 = _mm256_cmpeq_epi8(_c1, vquote); \
    uint64_t _uq = static_cast<uint32_t>(_mm256_movemask_epi8(_q0)) | \
                   (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_q1))) << 32); \
    const uint32_t _any_bs_m = static_cast<uint32_t>(_mm256_movemask_epi8( \
        _mm256_or_si256(_mm256_cmpeq_epi8(_c0, vbs), _mm256_cmpeq_epi8(_c1, vbs)))); \
    if (__builtin_expect((static_cast<uint64_t>(_any_bs_m) | prev_escaped) != 0ULL, 0)) { \
        const uint64_t _bm = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(_c0, vbs))) | \
                             (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(_c1, vbs)))) << 32); \
        uint64_t _em = prev_escaped, _tmp_bm = _bm; \
        while (_tmp_bm) { uint64_t _b = _tmp_bm & -_tmp_bm; if (!(_em & _b)) _em |= (_b << 1); _tmp_bm &= _tmp_bm - 1; } \
        prev_escaped = (_bm >> 63) & (1ULL - ((_em >> 63) & 1ULL)); \
        _uq &= ~_em; \
    } \
    const __m256i _w0 = _mm256_cmpeq_epi8(_c0, _mm256_shuffle_epi8(whitespace_table, _c0)); \
    const __m256i _w1 = _mm256_cmpeq_epi8(_c1, _mm256_shuffle_epi8(whitespace_table, _c1)); \
    const __m256i _op0 = _mm256_cmpeq_epi8(_mm256_or_si256(_c0, v0x20), _mm256_shuffle_epi8(op_table, _c0)); \
    const __m256i _op1 = _mm256_cmpeq_epi8(_mm256_or_si256(_c1, v0x20), _mm256_shuffle_epi8(op_table, _c1)); \
    const uint64_t _stm = static_cast<uint32_t>(_mm256_movemask_epi8(_op0)) | \
                          (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_op1))) << 32); \
    const uint64_t _non_scalar = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_or_si256(_q0, _mm256_or_si256(_w0, _op0)))) | \
                                 (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_or_si256(_q1, _mm256_or_si256(_w1, _op1))))) << 32); \
    const uint64_t _sm = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128( \
                             _mm_set_epi64x(0LL, static_cast<int64_t>(_uq)), \
                             vall_ones, 0))) ^ prev_string_mask; \
    prev_string_mask = static_cast<uint64_t>(static_cast<int64_t>(_sm) >> 63); \
    const uint64_t _scalar   = ~_non_scalar; \
    const uint64_t _sc_start = _scalar & ~((_scalar << 1) | prev_scalar_bit); \
    prev_scalar_bit = _scalar >> 63; \
    uint64_t _next_bits = (_uq & _sm) | ((_stm | _sc_start) & ~_sm); \
    asm volatile("" : "+r"(_next_bits)); \
    flatten_mask(static_cast<uint32_t>((BASE_IDX) - 64), prev_bits, tail); \
    prev_bits = _next_bits; \
} while(0)

    size_t pos = 0;
    const size_t blocks_end = len & ~static_cast<size_t>(63);
    while (__builtin_expect(pos + 255 < blocks_end, 1)) {
        STEP64_AVX2(data + pos,       pos);
        STEP64_AVX2(data + pos + 64,  pos + 64);
        STEP64_AVX2(data + pos + 128, pos + 128);
        STEP64_AVX2(data + pos + 192, pos + 192);
        pos += 256;
    }
    while (pos < blocks_end) {
        STEP64_AVX2(data + pos, pos);
        pos += 64;
    }
    if (pos < len) {
        alignas(64) char tail_buf[64];
        _mm256_store_si256(reinterpret_cast<__m256i*>(tail_buf), v0x20);
        _mm256_store_si256(reinterpret_cast<__m256i*>(tail_buf + 32), v0x20);
        std::memcpy(tail_buf, data + pos, len - pos);
        STEP64_AVX2(tail_buf, pos);
        pos += 64;
    }
    flatten_mask(static_cast<uint32_t>(pos - 64), prev_bits, tail);
#undef STEP64_AVX2

    size_t si = static_cast<size_t>(tail - indexes);
    indexes[si] = static_cast<uint32_t>(len);
    return si;
}

inline size_t build_indexes_scalar(
        std::string_view input,
        uint32_t* __restrict__ indexes) noexcept
{
    scan_state st{};
    size_t si = 0;
    process_scalar_tail_indexes(st, 0, input.size(), input.data(), indexes, si);
    indexes[si] = static_cast<uint32_t>(input.size());
    return si;
}

// ── CPUID: detect AVX-512F + AVX-512BW at runtime ─────────────────────────
[[nodiscard]] inline bool cpu_has_avx512bw() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __cpuid_count(7, 0, eax, ebx, ecx, edx);
    // Bit 16 of EBX = AVX-512F, bit 30 of EBX = AVX-512BW
    return ((ebx >> 16) & 1u) != 0u && ((ebx >> 30) & 1u) != 0u;
#else
    return false;
#endif
}

// ── Runtime dispatch [uint32_t*]: lean IndexOnly — AVX-512BW → AVX2 ──────
inline size_t build_structural_index(
        std::string_view input, uint32_t* sp) noexcept
{
    static const bool have_avx512bw = cpu_has_avx512bw();
    if (__builtin_expect(have_avx512bw, 1))
        return build_structural_index_avx512_index(input, sp);
    return build_structural_index_avx2_index(input, sp);
}

// ── Runtime dispatch [tape_entry*]: single-pass AVX-512BW → AVX2 ──────────
// Two-pass variants (bitmask intermediate, position intermediate) were
// benchmarked and found slower:
//
//   single-pass (current):            836 MB/s  ← best
//   bitmask two-pass:                 649 MB/s  ← data[p] re-read from L2/L3
//   position-array two-pass:          474 MB/s  ← 4.3 MB total traffic vs 2.4 MB
//
// Root cause: in single-pass, data[base+bit] is read from the zmm block
// already in L1.  Any two-pass approach forces a second touch of the data at
// L2/L3 latency for the char lookup.  The OOO processor naturally overlaps
// the SIMD mask computation with bracket-stack updates inside the same loop
// iteration, hiding most of the sequential dependency.
inline size_t build_structural_index(
        std::string_view input, tape_entry* te) noexcept
{
    static const bool have_avx512bw = cpu_has_avx512bw();
    if (__builtin_expect(have_avx512bw, 1))
        return build_structural_index_avx512(input, te);
    return build_structural_index_avx2(input, te);
}

// ── Runtime dispatch [tokens + tape_entry*]: decoupled two-phase build ────
[[gnu::always_inline]] inline size_t build_structural_index_decoupled(
        std::string_view input, uint32_t* tokens, tape_entry* te) noexcept
{
    static int8_t state = -1;
    if (__builtin_expect(state < 0, 0)) state = cpu_has_avx512bw() ? 1 : 0;
    if (state != 0)
        return build_tape_decoupled_avx512(input, tokens, te);
    return build_tape_decoupled_avx2(input, tokens, te);
}

// ── Runtime dispatch [uint32_t*]: OnDemand pure index build ───────────────
[[gnu::always_inline]] inline size_t build_structural_indexes(
        std::string_view input, uint32_t* indexes) noexcept
{
    static int8_t state = -1;
    if (__builtin_expect(state < 0, 0)) state = cpu_has_avx512bw() ? 1 : 0;
    if (state != 0)
        return build_indexes_avx512(input, indexes);
    return build_indexes_avx2(input, indexes);
}

} // namespace detail

// ── Exported: which SIMD tier will be used at runtime ─────────────────────
[[nodiscard]] inline bool has_avx512() noexcept { return detail::cpu_has_avx512bw(); }

[[nodiscard, gnu::always_inline]] inline auto parse(std::string_view input, turbo_parser& parser)
    -> result<turbo_document>
{
    turbo_document doc(input, parser);
    size_t count = detail::build_structural_index_decoupled(
        input, parser.tokens(), parser.tape());
    doc.set_structurals_count(count);
    return doc;
}

[[nodiscard, gnu::always_inline]] inline auto iterate(std::string_view input, turbo_parser& parser)
    -> result<turbo_document>
{
    parser.allocate_indexes(input.size());
    size_t count = detail::build_structural_indexes(
        input, parser.indexes());
    return turbo_document(input, parser.indexes(), count);
}

} // namespace fastjson::turbo
