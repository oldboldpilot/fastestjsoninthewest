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
#include "fastjson_pow5_128.h"

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
    out_of_bounds,
    // Appended (existing enumerator values unchanged):
    number_out_of_range,   // number is valid JSON but not representable (e.g. 1e400, int64 overflow)
    utf8_error,            // input is not valid UTF-8
    unescaped_control,     // raw control character (< 0x20) inside a string
    unclosed_string,       // input ends inside a string
    depth_exceeded,        // nesting deeper than the parser's maximum depth
    empty_document,        // input contains no JSON value
    trailing_content       // a complete value is followed by more non-whitespace
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
// Number parsing (shared by the OnDemand accessors and the tape DOM).
//
// Correctness contract (verified bit-exact against strtod in
// tests/test_turbo_correctness.cpp):
//   • Strict RFC 8259 grammar: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
//     followed by whitespace, ',', '}', ']' or end of input.
//   • Doubles are correctly rounded (round-half-even), matching strtod:
//       1. Clinger fast path   — w ≤ 2^53, |q| ≤ 22: one exact IEEE op.
//       2. Eisel–Lemire        — 128-bit power-of-five product (table in
//                                fastjson_pow5_128.h, tools/gen_pow5_table.py).
//       3. std::from_chars     — >19 significant digits or the (never observed)
//                                undecidable Eisel–Lemire case.
//   • Overflow to ±inf is an error (number_out_of_range); underflow → ±0.
// -------------------------------------------------------------------------
namespace detail {

[[gnu::always_inline]] inline bool is_digit(char c) noexcept {
    return static_cast<uint8_t>(static_cast<uint8_t>(c) - static_cast<uint8_t>('0')) < 10u;
}

// Bytes that may legally follow a JSON scalar value: whitespace , } ]
inline constexpr std::array<bool, 256> value_terminator_tbl = [] {
    std::array<bool, 256> t{};
    for (unsigned char c : {' ', '\t', '\n', '\r', ',', '}', ']'}) t[c] = true;
    return t;
}();
[[gnu::always_inline]] inline bool is_value_terminator(char c) noexcept {
    return value_terminator_tbl[static_cast<uint8_t>(c)];
}

[[gnu::always_inline]] inline bool is_json_ws(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

struct number_scan {
    uint64_t    mantissa   = 0;     // decimal significand, digits concatenated (exact if ≤19 sig. digits)
    int64_t     exp10      = 0;     // value = ±mantissa × 10^exp10
    const char* start      = nullptr;
    const char* end        = nullptr;
    uint32_t    digits     = 0;     // integer + fraction digits (including leading zeros)
    bool        negative   = false;
    bool        is_integer = true;  // no fraction and no exponent
};

enum class num_status : uint8_t { ok, invalid, out_of_range, not_integer };

// SWAR digit helpers (8 bytes at a time, little-endian; byte 0 = first char).
// Bytes before the first non-digit are digits, so carries out of non-digit
// bytes (which only propagate upward) never disturb the lowest-set-bit answer.
[[gnu::always_inline]] inline uint32_t swar_digit_count(uint64_t x /* v ^ 0x30.. */) noexcept {
    const uint64_t nondigit = ((x + 0x7676767676767676ULL) | x) & 0x8080808080808080ULL;
    return nondigit ? static_cast<uint32_t>(__builtin_ctzll(nondigit)) >> 3 : 8u;
}
// x holds digit VALUES (0..9) in bytes 0..7, byte 0 most significant.
[[gnu::always_inline]] inline uint64_t swar_parse8(uint64_t x) noexcept {
    x = (x * 10) + (x >> 8);
    x = (((x & 0x000000FF000000FFULL) * 0x000F424000000064ULL) +
         (((x >> 16) & 0x000000FF000000FFULL) * 0x0000271000000001ULL)) >> 32;
    return x;
}
[[gnu::always_inline]] inline uint64_t load8(const char* p) noexcept {
    uint64_t w;
    std::memcpy(&w, p, 8);
    return w;
}
inline constexpr uint64_t pow10_u64[9] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};

// Accumulates a run of digits starting at p into i (i = i*10^k + run).
// Returns the end of the run. SWAR when 8 bytes are readable before `limit`.
template <bool Bounded>
[[gnu::always_inline]] inline const char* scan_digit_run(const char* p, const char* limit, uint64_t& i) noexcept {
    while (__builtin_expect(p + 8 <= limit, 1)) {
        uint64_t v;
        std::memcpy(&v, p, 8);
        const uint64_t x = v ^ 0x3030303030303030ULL;
        const uint32_t n = swar_digit_count(x);
        if (__builtin_expect(n == 0, 0)) return p;
        i = i * pow10_u64[n] + swar_parse8(x << (8 * (8 - n)));
        p += n;
        if (n < 8) return p;
    }
    // Byte tail near the end of the input.
    for (;;) {
        if constexpr (Bounded) { if (p >= limit) return p; }
        const char c = *p;
        if (!is_digit(c)) return p;
        i = 10 * i + static_cast<uint64_t>(c - '0');
        ++p;
    }
}

// Loop-free fast path for the dominant number shape: -?D{1,8}(.D{1,7})?
// followed by a value terminator (no exponent). Requires p + 24 <= end of
// input (max byte read: p + 18). Returns false for anything else — including
// every malformed input — and the caller then runs the general scanner,
// which decides validity, so this path can never accept what scan_number
// would reject. (≤ 15 digits: mantissa < 10^15 < 2^53 is guaranteed.)
[[gnu::always_inline]] inline bool scan_number_fast(const char* p, number_scan& s) noexcept {
    const bool neg = (*p == '-');
    const char* q = p + neg;
    const uint64_t x1 = load8(q) ^ 0x3030303030303030ULL;
    const uint32_t n1 = swar_digit_count(x1);
    if (n1 == 0) return false;
    if (n1 == 8 && is_digit(q[8])) return false;                        // > 8 digits → general
    if (*q == '0' && n1 > 1) return false;                               // leading zero → general (rejects)
    uint64_t w = swar_parse8(x1 << (64 - 8 * n1));                       // n1 ∈ [1, 8]: shift ∈ [0, 56]
    const char* r = q + n1;
    int64_t e = 0;
    uint32_t digits = n1;
    bool integer = true;
    if (*r == '.') {
        const uint64_t x2 = load8(r + 1) ^ 0x3030303030303030ULL;
        const uint32_t n2 = swar_digit_count(x2);
        if (n2 - 1u >= 7u) return false;                                 // 1..7 frac digits → n1+n2 ≤ 15
        w = w * pow10_u64[n2] + swar_parse8(x2 << (64 - 8 * n2));
        e = -static_cast<int64_t>(n2);
        digits += n2;
        integer = false;
        r += 1 + n2;
    }
    if (!is_value_terminator(*r)) return false;                          // exponent, junk, …
    s.start = p; s.end = r; s.mantissa = w; s.exp10 = e; s.digits = digits;
    s.negative = neg; s.is_integer = integer;
    return true;
}

// Scans one JSON number starting at p; `limit` is the end of the input.
// Bounded=false additionally guarantees the bytes from p are readable up to
// the first byte outside [0-9.eE+-] (true for every token that has a
// following structural index); Bounded=true treats `limit` as end of input.
// Validates the terminator. Returns false on any grammar error.
template <bool Bounded>
[[gnu::always_inline]] inline bool scan_number(const char* p, const char* limit, number_scan& s) noexcept {
    auto at = [limit](const char* q) noexcept -> char {
        if constexpr (Bounded) { return q < limit ? *q : ' '; }
        else { (void)limit; return *q; }
    };
    s.start = p;
    const bool neg = (at(p) == '-');
    p += neg;
    const char* const dstart = p;
    uint64_t i = 0;
    char c = at(p);
    if (c == '0') {
        ++p;
        if (__builtin_expect(is_digit(at(p)), 0)) return false;          // leading zero
    } else {
        if (__builtin_expect(!is_digit(c), 0)) return false;
        p = scan_digit_run<Bounded>(p, limit, i);
    }
    int64_t exp10 = 0;
    bool integer = true;
    if (at(p) == '.') {
        ++p;
        const char* const frac_start = p;
        if (__builtin_expect(!is_digit(at(p)), 0)) return false;          // "1." / "1.e5"
        p = scan_digit_run<Bounded>(p, limit, i);
        exp10 = frac_start - p;
        integer = false;
    }
    const uint32_t digits = static_cast<uint32_t>(p - dstart) - (integer ? 0u : 1u);
    if ((at(p) | 0x20) == 'e') {
        integer = false;
        ++p;
        const bool eneg = (at(p) == '-');
        if (at(p) == '-' || at(p) == '+') ++p;
        c = at(p);
        if (__builtin_expect(!is_digit(c), 0)) return false;             // "1e" / "1e+"
        int64_t e = 0;
        do {
            if (e < 0x10000000) e = 10 * e + (c - '0');                    // saturate: no overflow UB
            ++p; c = at(p);
        } while (is_digit(c));
        exp10 += eneg ? -e : e;
    }
    if (__builtin_expect(!is_value_terminator(at(p)), 0)) return false; // "12a", "1.2.3", "1-"
    s.mantissa = i; s.exp10 = exp10; s.end = p; s.digits = digits;
    s.negative = neg; s.is_integer = integer;
    return true;
}

inline constexpr double pow10_exact[23] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
};

// Eisel–Lemire (binary64). Returns false when the result cannot be decided
// from the 128-bit product (caller falls back to from_chars). w != 0,
// q in [pow5_smallest_q, pow5_largest_q].
[[gnu::always_inline]] inline bool eisel_lemire(uint64_t w, int64_t q, bool neg, uint64_t& bits_out) noexcept {
    using fastjson_turbo_tables::pow5_128;
    using fastjson_turbo_tables::pow5_smallest_q;
    const int lz = __builtin_clzll(w);
    w <<= lz;
    const uint64_t* t = &pow5_128[2 * static_cast<size_t>(q - pow5_smallest_q)];
    const unsigned __int128 p1 = static_cast<unsigned __int128>(w) * t[0];
    uint64_t hi = static_cast<uint64_t>(p1 >> 64);
    uint64_t lo = static_cast<uint64_t>(p1);
    constexpr uint64_t precision_mask = 0xFFFFFFFFFFFFFFFFULL >> 55;      // 52 + 3 bits
    if ((hi & precision_mask) == precision_mask) {
        const unsigned __int128 p2 = static_cast<unsigned __int128>(w) * t[1];
        const uint64_t p2hi = static_cast<uint64_t>(p2 >> 64);
        lo += p2hi;
        if (p2hi > lo) ++hi;
    }
    // Conservative guard (original Lemire paper): outside the safe exponent
    // window an all-ones low word may hide a carry — defer to from_chars.
    if (__builtin_expect(lo == 0xFFFFFFFFFFFFFFFFULL && (q < -27 || q > 55), 0)) return false;

    const int upperbit = static_cast<int>(hi >> 63);
    const int shift    = upperbit + 64 - 52 - 3;
    uint64_t mantissa  = hi >> shift;
    int32_t power2 = static_cast<int32_t>((((152170 + 65536) * q) >> 16) + 63) + upperbit - lz + 1023;
    const uint64_t sign = static_cast<uint64_t>(neg) << 63;

    if (power2 <= 0) {                                                    // subnormal / zero
        if (-power2 + 1 >= 64) { bits_out = sign; return true; }
        mantissa >>= -power2 + 1;
        mantissa += (mantissa & 1);
        mantissa >>= 1;
        power2 = (mantissa < (uint64_t{1} << 52)) ? 0 : 1;
        bits_out = mantissa | (static_cast<uint64_t>(power2) << 52) | sign;
        return true;
    }
    if (lo <= 1 && q >= -4 && q <= 23 && (mantissa & 3) == 1) {         // exact halfway → even
        if ((mantissa << shift) == hi) mantissa &= ~uint64_t{1};
    }
    mantissa += (mantissa & 1);
    mantissa >>= 1;
    if (mantissa >= (uint64_t{2} << 52)) { mantissa = uint64_t{1} << 52; ++power2; }
    mantissa &= ~(uint64_t{1} << 52);
    if (power2 >= 0x7FF) { bits_out = sign | (uint64_t{0x7FF} << 52); return true; }   // ±inf
    bits_out = mantissa | (static_cast<uint64_t>(power2) << 52) | sign;
    return true;
}

// Slow path: >19 significant digits or undecidable Eisel–Lemire. On ERANGE
// the decimal magnitude (leading-digit position + exponent) tells overflow
// (error) from underflow (±0).
[[gnu::noinline]] inline num_status to_double_slow(const number_scan& s, double& out) noexcept {
    double v = 0.0;
    const auto [ptr, ec] = std::from_chars(s.start, s.end, v);
    if (ec == std::errc() && ptr == s.end) {
        if (__builtin_expect(std::isinf(v), 0)) return num_status::out_of_range;
        out = v;
        return num_status::ok;
    }
    if (ec == std::errc::result_out_of_range) {
        // Classify by the position of the leading significant digit. Exponent
        // part only (s.exp10 also folds in the fraction length, so recompute).
        const char* e = s.start;
        while (e < s.end && (*e | 0x20) != 'e') ++e;
        int64_t ex = 0;
        if (e < s.end) {
            const char* r = e + 1;
            const bool eneg = (*r == '-');
            if (*r == '-' || *r == '+') ++r;
            while (r < s.end && is_digit(*r)) { if (ex < 0x10000000) ex = ex * 10 + (*r - '0'); ++r; }
            if (eneg) ex = -ex;
        }
        const char* q = s.start + s.negative;
        int64_t mag = 0;
        if (*q == '0') {
            ++q;
            if (q < e && *q == '.') { ++q; while (q < e && *q == '0') { --mag; ++q; } }
        } else {
            while (q < e && is_digit(*q)) { ++mag; ++q; }
        }
        if (mag + ex > 0) return num_status::out_of_range;               // overflow
        out = s.negative ? -0.0 : 0.0;                                   // underflow → ±0
        return num_status::ok;
    }
    return num_status::invalid;
}

// General path: > 19 significant digits, |exp10| > 22, or mantissa > 2^53.
// Kept out of line so the Clinger fast path below stays small enough to
// inline into every accessor (perf: an outlined accessor lambda cost a call
// + vzeroupper per field in the OnDemand FieldAccess benchmark).
[[gnu::noinline]] inline num_status to_double_general(const number_scan& s, double& out) noexcept {
    uint64_t w = s.mantissa;
    const int64_t q = s.exp10;
    if (__builtin_expect(s.digits > 19, 0)) {
        // Leading zeros ("0.000…") are not significant and do not perturb w.
        const char* z = s.start + s.negative;
        uint32_t lz = 0;
        while (z < s.end && (*z == '0' || *z == '.')) { lz += (*z == '0'); ++z; }
        if (s.digits - lz > 19) return to_double_slow(s, out);
    }
    if (q >= -22 && q <= 22 && w <= (uint64_t{1} << 53)) {
        double d = static_cast<double>(w);                               // exact
        d = (q < 0) ? d / pow10_exact[-q] : d * pow10_exact[q];          // one correctly-rounded op
        out = s.negative ? -d : d;
        return num_status::ok;
    }
    if (w == 0 || q < fastjson_turbo_tables::pow5_smallest_q) {
        out = s.negative ? -0.0 : 0.0;
        return num_status::ok;
    }
    if (q > fastjson_turbo_tables::pow5_largest_q) return num_status::out_of_range;
    uint64_t bits = 0;
    if (__builtin_expect(!eisel_lemire(w, q, s.negative, bits), 0)) return to_double_slow(s, out);
    if (__builtin_expect((bits & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL, 0))
        return num_status::out_of_range;
    out = std::bit_cast<double>(bits);
    return num_status::ok;
}

[[gnu::always_inline]] inline num_status to_double(const number_scan& s, double& out) noexcept {
    const uint64_t w = s.mantissa;
    const int64_t q = s.exp10;
    // Clinger fast path: ≤ 19 digits (w exact), w ≤ 2^53, |q| ≤ 22 →
    // one correctly-rounded IEEE multiply/divide of two exact doubles.
    if (__builtin_expect(s.digits <= 19 && q >= -22 && q <= 22 && w <= (uint64_t{1} << 53), 1)) {
        double d = static_cast<double>(w);
        d = (q < 0) ? d / pow10_exact[-q] : d * pow10_exact[q];
        out = s.negative ? -d : d;
        return num_status::ok;
    }
    return to_double_general(s, out);
}

[[gnu::always_inline]] inline num_status to_int64(const number_scan& s, int64_t& out) noexcept {
    if (!s.is_integer) return num_status::not_integer;
    if (s.digits > 19) return num_status::out_of_range;                  // 19 digits always fit u64
    const uint64_t m = s.mantissa;
    if (s.negative) {
        if (m > (uint64_t{1} << 63)) return num_status::out_of_range;
        out = static_cast<int64_t>(uint64_t{0} - m);                     // two's complement, no UB
    } else {
        if (m > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) return num_status::out_of_range;
        out = static_cast<int64_t>(m);
    }
    return num_status::ok;
}

// Parses the number token that starts at byte `start`; `next` is the byte
// offset of the following structural index (== input size for the last token).
template <class Fn>
[[gnu::always_inline]] inline num_status with_number(std::string_view in, uint32_t start, size_t next,
                                                     number_scan& s, Fn&& fn) noexcept {
    const char* p = in.data() + start;
    const char* const end = in.data() + in.size();
    bool ok;
    if (__builtin_expect(p + 24 <= end && scan_number_fast(p, s), 1)) ok = true;
    else if (next < in.size()) ok = scan_number<false>(p, end, s);
    else                       ok = scan_number<true>(p, end, s);
    if (__builtin_expect(!ok, 0)) return num_status::invalid;
    return fn(s);
}

inline parse_error num_error(num_status st) noexcept {
    switch (st) {
        case num_status::out_of_range: return {error_code::number_out_of_range, "Number out of range"};
        case num_status::not_integer:  return {error_code::type_error, "Not an integer"};
        default:                       return {error_code::invalid_json, "Invalid number"};
    }
}

// Atom check for true/false/null with bounds: literal followed by a value
// terminator or end of input.
[[gnu::always_inline]] inline bool match_atom(std::string_view in, size_t pos, const char* lit, size_t n) noexcept {
    if (pos + n > in.size()) return false;
    if (std::memcmp(in.data() + pos, lit, n) != 0) return false;
    return pos + n == in.size() || is_value_terminator(in[pos + n]);
}

} // namespace detail

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
    const size_t total = doc_->structurals_count();
    if (__builtin_expect(idx_ >= total, 0))
        return std::unexpected(parse_error{error_code::type_error, "Not a number"});
    const std::string_view in = doc_->input();
    const uint32_t start = doc_->get_structural(idx_);
    const char* const p = in.data() + start;
    const char* const end = in.data() + in.size();
    detail::number_scan s;
    if (__builtin_expect(p + 24 <= end && detail::scan_number_fast(p, s), 1)) {
        // scan_number_fast guarantees ≤ 15 digits (mantissa < 10^15 < 2^53)
        // and exp10 ∈ [-7, 0]: integer needs only cvtsi2sd, fraction needs one
        // exact IEEE-754 division.
        double d = static_cast<double>(s.mantissa);
        if (!s.is_integer) d /= detail::pow10_exact[-s.exp10];
        return s.negative ? -d : d;
    }
    const char c0 = *p;
    if (__builtin_expect(c0 != '-' && !detail::is_digit(c0), 0))
        return std::unexpected(parse_error{error_code::type_error, "Not a number"});
    const size_t next = (idx_ + 1 < total) ? doc_->get_structural(idx_ + 1) : in.size();
    double out = 0.0;
    const detail::num_status st = detail::with_number(in, start, next, s,
        [&out](const detail::number_scan& sc) noexcept { return detail::to_double(sc, out); });
    if (__builtin_expect(st == detail::num_status::ok, 1)) return out;
    return std::unexpected(detail::num_error(st));
}
[[gnu::always_inline]] inline auto turbo_value::get_int64() const noexcept -> result<int64_t> {
    const size_t total = doc_->structurals_count();
    if (__builtin_expect(idx_ >= total, 0))
        return std::unexpected(parse_error{error_code::type_error, "Not a number"});
    const std::string_view in = doc_->input();
    const uint32_t start = doc_->get_structural(idx_);
    const char* const p = in.data() + start;
    const char* const end = in.data() + in.size();
    detail::number_scan s;
    if (__builtin_expect(p + 24 <= end && detail::scan_number_fast(p, s), 1)) {
        if (__builtin_expect(!s.is_integer, 0))
            return std::unexpected(parse_error{error_code::type_error, "Not an integer"});
        const auto m = static_cast<int64_t>(s.mantissa);
        return s.negative ? -m : m;
    }
    const char c0 = *p;
    if (__builtin_expect(c0 != '-' && !detail::is_digit(c0), 0))
        return std::unexpected(parse_error{error_code::type_error, "Not a number"});
    const size_t next = (idx_ + 1 < total) ? doc_->get_structural(idx_ + 1) : in.size();
    int64_t out = 0;
    const detail::num_status st = detail::with_number(in, start, next, s,
        [&out](const detail::number_scan& sc) noexcept { return detail::to_int64(sc, out); });
    if (__builtin_expect(st == detail::num_status::ok, 1)) return out;
    return std::unexpected(detail::num_error(st));
}
[[gnu::always_inline]] inline auto turbo_value::get_bool() const noexcept -> result<bool> {
    if (idx_ >= doc_->structurals_count())
        return std::unexpected(parse_error{error_code::type_error, "Not a boolean"});
    const uint32_t pos = doc_->get_structural(idx_);
    const auto inp = doc_->input();
    if (detail::match_atom(inp, pos, "true", 4))  return true;
    if (detail::match_atom(inp, pos, "false", 5)) return false;
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
                    // Bounds check FIRST: idxs[curr + 1] (the ':' after the key)
                    // bounds the key bytes, so a lookup key longer than the
                    // stored key never reads past it (or past the input end).
                    // Also verify the matched '"' is the real closing quote
                    // before ':' (not an escaped '\"' inside a longer key).
                    const uint32_t colon = idxs[curr + 1];
                    const size_t   ke    = static_cast<size_t>(ks) + key_len;
                    if (ke < colon &&
                        in[ks] == key_first && in[ke] == '"' &&
                        __builtin_expect(std::memcmp(in + ks, key.data(), key_len) == 0, 1)) {
                        uint32_t q = colon - 1;
                        if (__builtin_expect(q != ke, 0)) {
                            while (q > ke && detail::is_json_ws(in[q])) --q;
                        }
                        if (__builtin_expect(q == ke, 1)) {
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
                // On the tape path, curr + 1 is the key's closing quote token:
                // compare the exact key span (prevents matching an escaped '\"'
                // inside a key or spanning across keys).
                if (__builtin_expect(curr + 1 < total &&
                                     doc_->get_structural(curr + 1) - ks == key_len &&
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
    uint64_t ctrl_in_string   = 0;   // OR of (byte < 0x20) & in-string bits (validation)
    uint64_t non_ascii        = 0;   // non-zero if any byte >= 0x80 was seen
};

// Stage-1 validation flags (bitwise OR), reported by the indexers.
inline constexpr uint32_t s1_non_ascii      = 1u << 0;   // needs full UTF-8 validation
inline constexpr uint32_t s1_ctrl_in_string = 1u << 1;   // raw byte < 0x20 inside a string
inline constexpr uint32_t s1_unclosed_str   = 1u << 2;   // input ends inside a string
inline constexpr uint32_t s1_bad_brackets   = 1u << 3;   // unbalanced / mismatched {} []
inline constexpr uint32_t s1_too_deep       = 1u << 4;   // nesting deeper than 4096 (tape build)

// Carry state of the OnDemand indexer between consecutive input ranges, so
// the input can be indexed in tiles (index_range_avx2 / index_range_avx512).
struct s1_tile_state {
    uint64_t prev_escaped     = 0;
    uint64_t prev_string_mask = 0;
    uint64_t prev_scalar_bit  = 0;
    uint64_t non_ascii        = 0;
    uint64_t ctrl_err         = 0;
};
[[gnu::always_inline]] inline uint32_t s1_flags(const s1_tile_state& st) noexcept {
    return (st.non_ascii ? s1_non_ascii : 0u) | (st.ctrl_err ? s1_ctrl_in_string : 0u)
         | (st.prev_string_mask ? s1_unclosed_str : 0u);
}

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

        // Bracket stack is a fixed 4096-entry array: depth beyond it is still
        // counted in bkt_top (so pairing stays correct) but not stored; those
        // brackets keep partner 0 and skip_value falls back to the lazy scan.
        if (open_m & bbit) {
            te[si] = {tok, 0u};
            if (__builtin_expect(++bkt_top < 4096, 1)) bkt_stk[bkt_top] = static_cast<uint32_t>(si);
        } else if ((close_m & bbit) && (bkt_top >= 0)) {
            if (__builtin_expect(bkt_top < 4096, 1)) {
                const uint32_t open = bkt_stk[bkt_top];
                te[open].partner    = static_cast<uint32_t>(si);
                te[si]              = {tok, open};
            } else {
                te[si] = {tok, 0u};
            }
            --bkt_top;
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
                        if (++bkt_top < 4096) bkt_stk[bkt_top] = static_cast<uint32_t>(si);  // see process_group64
                    } else if ((c == '}' || c == ']') && bkt_top >= 0) {
                        if (bkt_top < 4096) {
                            uint32_t open = bkt_stk[bkt_top];
                            te[open].partner = static_cast<uint32_t>(si);
                            te[si].token   = tok;
                            te[si].partner = open;
                        } else {
                            uint64_t _q = static_cast<uint64_t>(tok);
                            te[si] = std::bit_cast<tape_entry>(_q);
                        }
                        --bkt_top;
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
                if (++bkt_top < 4096) bkt_stk[bkt_top] = si;   // depth > 4096: counted, not stored
            } else if (bkt_top >= 0) {
                if (bkt_top < 4096) {
                    const uint32_t open = bkt_stk[bkt_top];
                    te[open].partner    = si;
                    te[si].partner      = open;
                }
                --bkt_top;
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
            if (++bkt_top < 4096) bkt_stk[bkt_top] = static_cast<uint32_t>(si);
        } else if (ch == 0x7Du && bkt_top >= 0) {
            if (bkt_top < 4096) {
                const uint32_t open = bkt_stk[bkt_top];
                te[open].partner    = static_cast<uint32_t>(si);
                te[si].partner      = open;
            }
            --bkt_top;
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
                if (++bkt_top < 4096) bkt_stk[bkt_top] = si;   // depth > 4096: counted, not stored
            } else if (bkt_top >= 0) {
                if (bkt_top < 4096) {
                    const uint32_t open = bkt_stk[bkt_top];
                    te[open].partner    = si;
                    te[si].partner      = open;
                }
                --bkt_top;
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
            if (++bkt_top < 4096) bkt_stk[bkt_top] = static_cast<uint32_t>(si);
        } else if (ch == 0x7Du && bkt_top >= 0) {
            if (bkt_top < 4096) {
                const uint32_t open = bkt_stk[bkt_top];
                te[open].partner    = static_cast<uint32_t>(si);
                te[si].partner      = open;
            }
            --bkt_top;
        }
    }
    return base + count;
}

__attribute__((target("avx2,pclmul")))
inline uint64_t compute_mask64_common(
        scan_state& st,
        uint64_t qm, uint64_t bm, uint64_t stm, uint64_t wsm, uint64_t ctrl = 0) noexcept
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
    st.ctrl_in_string |= ctrl & sm;

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
    const __m512i v1f512    = _mm512_set1_epi8(0x1F);
    __m512i ascii_acc       = _mm512_setzero_si512();

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
    const uint64_t _ctl = static_cast<uint64_t>(_mm512_cmple_epu8_mask(zmm, v1f512)); \
    ascii_acc = _mm512_or_si512(ascii_acc, zmm); \
    masks[bi++] = compute_mask64_common(st, _qm, _bm, _stm, _wsm, _ctl); \
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

    st.non_ascii |= static_cast<uint64_t>(_mm512_movepi8_mask(ascii_acc));
    return bi;
}

// ── Phase 2 for one 64-byte block: emit tape entries; match and validate
//    brackets ('{'↔'}', '['↔']'; open+2 == close in ASCII). `src` points at
//    the block's bytes (input or the space-padded tail copy).
[[gnu::always_inline]] inline void emit_block_tape(
        uint64_t mask, uint32_t base, const char* __restrict__ src,
        tape_entry* __restrict__ te, size_t& si,
        uint32_t* __restrict__ bkt_stk, int32_t& bkt_top, uint32_t& bad) noexcept
{
    while (mask) {
        const uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(mask));
        mask &= mask - 1ULL;
        const uint8_t  ch  = static_cast<uint8_t>(src[bit]);
        const uint32_t tok = (static_cast<uint32_t>(ch) << 24) | (base + bit);
        te[si] = {tok, 0u};
        const uint32_t bch = ch | 0x20u;
        if (__builtin_expect(bch == 0x7Bu, 0)) {
            if (__builtin_expect(bkt_top < 4095, 1)) bkt_stk[++bkt_top] = static_cast<uint32_t>(si);
            else bad |= s1_too_deep;
        } else if (__builtin_expect(bch == 0x7Du, 0)) {
            if (__builtin_expect(bkt_top >= 0, 1)) {
                const uint32_t open = bkt_stk[bkt_top--];
                if (__builtin_expect((te[open].token >> 24) + 2u != ch, 0)) bad |= s1_bad_brackets;
                te[open].partner = static_cast<uint32_t>(si);
                te[si].partner   = open;
            } else {
                bad |= s1_bad_brackets;
            }
        }
        ++si;
    }
}

// ── Decoupled driver: L1-tiled phase-1 mask scan / phase-2 tape build (AVX-512) ────
inline size_t build_tape_decoupled_avx512(
        std::string_view input,
        uint32_t* __restrict__ tokens,
        tape_entry* __restrict__ te,
        uint32_t& flags) noexcept
{
    (void)tokens;
    constexpr size_t chunk_size = decoupled_chunk_size;
    constexpr size_t max_blocks_per_chunk = chunk_size / 64;

    const char*  data = input.data();
    const size_t len  = input.size();
    scan_state st{};
    uint32_t bkt_stk[4096]; int32_t bkt_top = -1;
    uint64_t tile_masks[max_blocks_per_chunk];
    uint32_t bad = 0;

    size_t si  = 0;
    size_t pos = 0;
    const size_t blocks_end = len & ~static_cast<size_t>(63);
    while (pos < blocks_end) {
        const size_t chunk_end = (blocks_end - pos > chunk_size)
                                     ? pos + chunk_size : blocks_end;
        const size_t num_blocks = scan_masks_avx512_chunk(data, pos, chunk_end, st, tile_masks);
        for (size_t b = 0; b < num_blocks; ++b) {
            const uint32_t base = static_cast<uint32_t>(pos + b * 64);
            emit_block_tape(tile_masks[b], base, data + base, te, si, bkt_stk, bkt_top, bad);
        }
        pos = chunk_end;
    }
    if (pos < len) {   // < 64-byte tail: same SIMD path over a space-padded copy
        alignas(64) char tail[64];
        std::memset(tail, ' ', sizeof tail);
        std::memcpy(tail, data + pos, len - pos);
        scan_masks_avx512_chunk(tail, 0, 64, st, tile_masks);
        emit_block_tape(tile_masks[0], static_cast<uint32_t>(pos), tail, te, si, bkt_stk, bkt_top, bad);
    }
    te[si] = {static_cast<uint32_t>(len), 0u};
    if (bkt_top != -1) bad |= s1_bad_brackets;
    flags = bad | (st.non_ascii ? s1_non_ascii : 0u) | (st.ctrl_in_string ? s1_ctrl_in_string : 0u)
                | (st.prev_string_mask ? s1_unclosed_str : 0u);
    return si;
}

#define WRITE_IDX(base_idx, i) do { \
    tail[i] = (base_idx) + static_cast<uint32_t>(_tzcnt_u64(bits)); \
    bits = _blsr_u64(bits); \
} while(0)

// Writes are unconditional in groups of 4 (slots past `cnt` receive harmless
// garbage — tzcnt(0) == 64 — and are overwritten by the next block). perf:
// the previous 2-at-a-time ladder mispredicted on number-dense input where
// the per-block count varies. Every caller's buffer has >= 8 slots of slack
// beyond the maximum structural count (indexes: +128, DOM window: +1024).
// Granularity 4 keeps sparse blocks (~12 structurals) from paying for 16.
[[gnu::always_inline]] inline void flatten_mask(uint32_t idx, uint64_t bits, uint32_t* __restrict__& tail) noexcept {
    if (!bits) return;
    const int cnt = static_cast<int>(__builtin_popcountll(bits));
    WRITE_IDX(idx, 0); WRITE_IDX(idx, 1); WRITE_IDX(idx, 2); WRITE_IDX(idx, 3);
    WRITE_IDX(idx, 4); WRITE_IDX(idx, 5); WRITE_IDX(idx, 6); WRITE_IDX(idx, 7);
    if (cnt > 8) {
        WRITE_IDX(idx, 8);  WRITE_IDX(idx, 9);  WRITE_IDX(idx, 10); WRITE_IDX(idx, 11);
        if (cnt > 12) {
            WRITE_IDX(idx, 12); WRITE_IDX(idx, 13); WRITE_IDX(idx, 14); WRITE_IDX(idx, 15);
            if (cnt > 16) {
                WRITE_IDX(idx, 16); WRITE_IDX(idx, 17); WRITE_IDX(idx, 18); WRITE_IDX(idx, 19);
                if (__builtin_expect(cnt > 20, 0)) {
                    for (int i = 20; i < cnt; ++i) {
                        WRITE_IDX(idx, i);
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
// Validation folded into the same pass: OR of all bytes (UTF-8 needed?),
// raw control bytes inside strings, and unterminated final string. When
// bs_bits is non-null, bit (pos/64) is set for every 64-byte block that
// contains a backslash (used by the DOM to unescape only those strings).
// Range form: indexes blocks [pos, end) (pos a multiple of 64; end a
// multiple of 64, or == len for the final range), carrying `st` across
// calls. Every structural in the range is emitted before returning.
inline uint32_t* index_range_avx512(
        const char* __restrict__ data, size_t len, size_t pos, size_t end,
        s1_tile_state& st,
        uint32_t* __restrict__ tail,
        uint64_t* __restrict__ bs_bits) noexcept
{
    uint64_t prev_escaped     = st.prev_escaped;
    uint64_t prev_string_mask = st.prev_string_mask;
    uint64_t prev_scalar_bit  = st.prev_scalar_bit;
    uint64_t prev_bits        = 0;

    const __m512i vquote512 = _mm512_set1_epi8('"');
    const __m512i vbs512    = _mm512_set1_epi8('\\');
    const __m512i v0x20_512 = _mm512_set1_epi8(0x20);
    const __m128i vall_ones = _mm_set1_epi8('\xFF');

    const __m512i whitespace_table = _mm512_broadcast_i32x4(
        _mm_setr_epi8(' ', 100, 100, 100, 17, 100, 113, 2, 100, '\t', '\n', 112, 100, '\r', 100, 100));
    const __m512i op_table = _mm512_broadcast_i32x4(
        _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ':', '{', ',', '}', 0, 0));
    const __m512i v1f_512 = _mm512_set1_epi8(0x1F);
    __m512i ascii_acc = _mm512_setzero_si512();
    uint64_t ctrl_err = 0;

#define STEP64_AVX512(PTR, BASE_IDX) do { \
    const __m512i _c = _mm512_loadu_si512(static_cast<const void*>(PTR)); \
    ascii_acc = _mm512_or_si512(ascii_acc, _c); \
    uint64_t _uq = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(_c, vquote512)); \
    const uint64_t _qm = _uq; \
    const uint64_t _bm = static_cast<uint64_t>(_mm512_cmpeq_epi8_mask(_c, vbs512)); \
    if (__builtin_expect((_bm | prev_escaped) != 0ULL, 0)) { \
        if (bs_bits) bs_bits[(BASE_IDX) >> 12] |= 1ULL << (((BASE_IDX) >> 6) & 63); \
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
    ctrl_err |= static_cast<uint64_t>(_mm512_cmple_epu8_mask(_c, v1f_512)) & _sm; \
    const uint64_t _scalar   = ~(_qm | _stm | _wsm); \
    const uint64_t _sc_start = _scalar & ~((_scalar << 1) | prev_scalar_bit); \
    prev_scalar_bit = _scalar >> 63; \
    uint64_t _next_bits = (_uq & _sm) | ((_stm | _sc_start) & ~_sm); \
    asm volatile("" : "+r"(_next_bits)); \
    flatten_mask(static_cast<uint32_t>((BASE_IDX) - 64), prev_bits, tail); \
    prev_bits = _next_bits; \
} while(0)

    const size_t blocks_end = (end == len) ? (len & ~static_cast<size_t>(63)) : end;
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
    if (end == len && pos < len) {
        alignas(64) char tail_buf[64];
        _mm512_store_si512(static_cast<void*>(tail_buf), v0x20_512);
        std::memcpy(tail_buf, data + pos, len - pos);
        STEP64_AVX512(tail_buf, pos);
        pos += 64;
    }
    flatten_mask(static_cast<uint32_t>(pos - 64), prev_bits, tail);
#undef STEP64_AVX512

    st.prev_escaped = prev_escaped;
    st.prev_string_mask = prev_string_mask;
    st.prev_scalar_bit = prev_scalar_bit;
    st.non_ascii |= static_cast<uint64_t>(_mm512_movepi8_mask(ascii_acc));
    st.ctrl_err |= ctrl_err;
    return tail;
}

// Whole-input OnDemand index: one range plus the end sentinel.
__attribute__((target("avx512f,avx512bw,pclmul")))
inline size_t build_indexes_avx512(
        std::string_view input,
        uint32_t* __restrict__ indexes,
        uint64_t* __restrict__ bs_bits,
        uint32_t& flags) noexcept
{
    s1_tile_state st{};
    uint32_t* tail = index_range_avx512(input.data(), input.size(), 0, input.size(), st, indexes, bs_bits);
    flags = s1_flags(st);
    const size_t si = static_cast<size_t>(tail - indexes);
    indexes[si] = static_cast<uint32_t>(input.size());
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
    const __m256i v1f       = _mm256_set1_epi8(0x1F);
    __m256i ascii_acc       = _mm256_setzero_si256();

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
    const uint64_t _ctl = qm64(_mm256_max_epu8(c0, v1f), _mm256_max_epu8(c1, v1f), v1f); \
    ascii_acc = _mm256_or_si256(ascii_acc, _mm256_or_si256(c0, c1)); \
    masks[bi++] = compute_mask64_common(st, _qm, _bm, _stm, _wsm, _ctl); \
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

    st.non_ascii |= static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(ascii_acc)));
    return bi;
}

// ── Decoupled driver for AVX2: L1-tiled phase-1 mask scan / phase-2 tape build ────
inline size_t build_tape_decoupled_avx2(
        std::string_view input,
        uint32_t* __restrict__ tokens,
        tape_entry* __restrict__ te,
        uint32_t& flags) noexcept
{
    (void)tokens;
    constexpr size_t chunk_size = decoupled_chunk_size;
    constexpr size_t max_blocks_per_chunk = chunk_size / 64;

    const char*  data = input.data();
    const size_t len  = input.size();
    scan_state st{};
    uint32_t bkt_stk[4096]; int32_t bkt_top = -1;
    uint64_t tile_masks[max_blocks_per_chunk];
    uint32_t bad = 0;

    size_t si  = 0;
    size_t pos = 0;
    const size_t blocks_end = len & ~static_cast<size_t>(63);
    while (pos < blocks_end) {
        const size_t chunk_end = (blocks_end - pos > chunk_size)
                                     ? pos + chunk_size : blocks_end;
        const size_t num_blocks = scan_masks_avx2_chunk(data, pos, chunk_end, st, tile_masks);
        // Phase 2: Extract tokens from tile_masks, load char from L1-resident data, build tape
        for (size_t b = 0; b < num_blocks; ++b) {
            const uint32_t base = static_cast<uint32_t>(pos + b * 64);
            emit_block_tape(tile_masks[b], base, data + base, te, si, bkt_stk, bkt_top, bad);
        }
        pos = chunk_end;
    }
    if (pos < len) {   // < 64-byte tail: same SIMD path over a space-padded copy
        alignas(64) char tail[64];
        std::memset(tail, ' ', sizeof tail);
        std::memcpy(tail, data + pos, len - pos);
        scan_masks_avx2_chunk(tail, 0, 64, st, tile_masks);
        emit_block_tape(tile_masks[0], static_cast<uint32_t>(pos), tail, te, si, bkt_stk, bkt_top, bad);
    }
    te[si] = {static_cast<uint32_t>(len), 0u};
    if (bkt_top != -1) bad |= s1_bad_brackets;
    flags = bad | (st.non_ascii ? s1_non_ascii : 0u) | (st.ctrl_in_string ? s1_ctrl_in_string : 0u)
                | (st.prev_string_mask ? s1_unclosed_str : 0u);
    return si;
}

// ── OnDemand driver: direct streaming interleaved mask scan / index store (AVX2) ────
__attribute__((target("avx2,pclmul")))
// Same validation / backslash-bitmap contract as build_indexes_avx512.
// Range form: indexes blocks [pos, end) (pos a multiple of 64; end a
// multiple of 64, or == len for the final range), carrying `st` across
// calls. Every structural in the range is emitted before returning.
inline uint32_t* index_range_avx2(
        const char* __restrict__ data, size_t len, size_t pos, size_t end,
        s1_tile_state& st,
        uint32_t* __restrict__ tail,
        uint64_t* __restrict__ bs_bits) noexcept
{
    uint64_t prev_escaped     = st.prev_escaped;
    uint64_t prev_string_mask = st.prev_string_mask;
    uint64_t prev_scalar_bit  = st.prev_scalar_bit;
    uint64_t prev_bits        = 0;

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
    const __m256i v1f = _mm256_set1_epi8(0x1F);
    __m256i ascii_acc = _mm256_setzero_si256();
    uint64_t ctrl_err = 0;

#define STEP64_AVX2(PTR, BASE_IDX) do { \
    const __m256i _c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(PTR)); \
    const __m256i _c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>((PTR) + 32)); \
    ascii_acc = _mm256_or_si256(ascii_acc, _mm256_or_si256(_c0, _c1)); \
    const __m256i _q0 = _mm256_cmpeq_epi8(_c0, vquote); \
    const __m256i _q1 = _mm256_cmpeq_epi8(_c1, vquote); \
    uint64_t _uq = static_cast<uint32_t>(_mm256_movemask_epi8(_q0)) | \
                   (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_q1))) << 32); \
    const uint32_t _any_bs_m = static_cast<uint32_t>(_mm256_movemask_epi8( \
        _mm256_or_si256(_mm256_cmpeq_epi8(_c0, vbs), _mm256_cmpeq_epi8(_c1, vbs)))); \
    if (__builtin_expect((static_cast<uint64_t>(_any_bs_m) | prev_escaped) != 0ULL, 0)) { \
        if (bs_bits) bs_bits[(BASE_IDX) >> 12] |= 1ULL << (((BASE_IDX) >> 6) & 63); \
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
    const uint64_t _ctl = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_max_epu8(_c0, v1f), v1f))) | \
                          (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_max_epu8(_c1, v1f), v1f)))) << 32); \
    ctrl_err |= _ctl & _sm; \
    const uint64_t _scalar   = ~_non_scalar; \
    const uint64_t _sc_start = _scalar & ~((_scalar << 1) | prev_scalar_bit); \
    prev_scalar_bit = _scalar >> 63; \
    uint64_t _next_bits = (_uq & _sm) | ((_stm | _sc_start) & ~_sm); \
    asm volatile("" : "+r"(_next_bits)); \
    flatten_mask(static_cast<uint32_t>((BASE_IDX) - 64), prev_bits, tail); \
    prev_bits = _next_bits; \
} while(0)

    const size_t blocks_end = (end == len) ? (len & ~static_cast<size_t>(63)) : end;
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
    if (end == len && pos < len) {
        alignas(64) char tail_buf[64];
        _mm256_store_si256(reinterpret_cast<__m256i*>(tail_buf), v0x20);
        _mm256_store_si256(reinterpret_cast<__m256i*>(tail_buf + 32), v0x20);
        std::memcpy(tail_buf, data + pos, len - pos);
        STEP64_AVX2(tail_buf, pos);
        pos += 64;
    }
    flatten_mask(static_cast<uint32_t>(pos - 64), prev_bits, tail);
#undef STEP64_AVX2

    st.prev_escaped = prev_escaped;
    st.prev_string_mask = prev_string_mask;
    st.prev_scalar_bit = prev_scalar_bit;
    st.non_ascii |= static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(ascii_acc)));
    st.ctrl_err |= ctrl_err;
    return tail;
}

// Whole-input OnDemand index: one range plus the end sentinel.
__attribute__((target("avx2,pclmul")))
inline size_t build_indexes_avx2(
        std::string_view input,
        uint32_t* __restrict__ indexes,
        uint64_t* __restrict__ bs_bits,
        uint32_t& flags) noexcept
{
    s1_tile_state st{};
    uint32_t* tail = index_range_avx2(input.data(), input.size(), 0, input.size(), st, indexes, bs_bits);
    flags = s1_flags(st);
    const size_t si = static_cast<size_t>(tail - indexes);
    indexes[si] = static_cast<uint32_t>(input.size());
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

// ── UTF-8 validation ──────────────────────────────────────────────────────
// Stage 1 OR-accumulates every input byte; only documents containing a byte
// ≥ 0x80 pay for full validation (validate_utf8 below), so pure-ASCII input —
// the common case — costs two vector ORs per 64 bytes.
//
// Scalar reference (RFC 3629): rejects overlongs, surrogates (U+D800–DFFF),
// code points > U+10FFFF, stray continuations and truncated sequences.
inline bool validate_utf8_scalar(const char* data, size_t len) noexcept {
    const auto* s = reinterpret_cast<const uint8_t*>(data);
    size_t i = 0;
    while (i < len) {
        const uint8_t b = s[i];
        if (b < 0x80) { ++i; continue; }
        auto cont = [&](size_t k) noexcept { return i + k < len && (s[i + k] & 0xC0u) == 0x80u; };
        if (b < 0xC2) return false;                                  // continuation / overlong C0,C1
        if (b < 0xE0) { if (!cont(1)) return false; i += 2; continue; }
        if (b < 0xF0) {
            if (!cont(1) || !cont(2)) return false;
            if (b == 0xE0 && s[i + 1] < 0xA0) return false;          // overlong 3-byte
            if (b == 0xED && s[i + 1] > 0x9F) return false;          // surrogates
            i += 3; continue;
        }
        if (b < 0xF5) {
            if (!cont(1) || !cont(2) || !cont(3)) return false;
            if (b == 0xF0 && s[i + 1] < 0x90) return false;          // overlong 4-byte
            if (b == 0xF4 && s[i + 1] > 0x8F) return false;          // > U+10FFFF
            i += 4; continue;
        }
        return false;                                                // F5..FF
    }
    return true;
}

// AVX2 lookup validator (Keiser & Lemire, "Validating UTF-8 In Less Than One
// Instruction Per Byte", SPE 2021): three 16-entry nibble lookups classify
// every (prev byte, byte) pair; a saturating-subtract check catches missing
// 3rd/4th-byte continuations. ASCII 32-byte chunks are skipped.
__attribute__((target("avx2")))
inline bool validate_utf8_avx2(const char* data, size_t len) noexcept {
    constexpr uint8_t TOO_SHORT = 1u << 0, TOO_LONG = 1u << 1, OVERLONG_3 = 1u << 2,
                      TOO_LARGE = 1u << 3, SURROGATE = 1u << 4, OVERLONG_2 = 1u << 5,
                      TOO_LARGE_1000 = 1u << 6, OVERLONG_4 = 1u << 6, TWO_CONTS = 1u << 7;
    constexpr uint8_t CARRY = TOO_SHORT | TOO_LONG | TWO_CONTS;
    auto tbl = [](uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4, uint8_t a5, uint8_t a6, uint8_t a7,
                  uint8_t a8, uint8_t a9, uint8_t aA, uint8_t aB, uint8_t aC, uint8_t aD, uint8_t aE, uint8_t aF)
        __attribute__((always_inline, target("avx2"))) {
        return _mm256_setr_epi8(
            static_cast<char>(a0), static_cast<char>(a1), static_cast<char>(a2), static_cast<char>(a3),
            static_cast<char>(a4), static_cast<char>(a5), static_cast<char>(a6), static_cast<char>(a7),
            static_cast<char>(a8), static_cast<char>(a9), static_cast<char>(aA), static_cast<char>(aB),
            static_cast<char>(aC), static_cast<char>(aD), static_cast<char>(aE), static_cast<char>(aF),
            static_cast<char>(a0), static_cast<char>(a1), static_cast<char>(a2), static_cast<char>(a3),
            static_cast<char>(a4), static_cast<char>(a5), static_cast<char>(a6), static_cast<char>(a7),
            static_cast<char>(a8), static_cast<char>(a9), static_cast<char>(aA), static_cast<char>(aB),
            static_cast<char>(aC), static_cast<char>(aD), static_cast<char>(aE), static_cast<char>(aF));
    };
    const __m256i byte_1_high = tbl(
        TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
        TWO_CONTS, TWO_CONTS, TWO_CONTS, TWO_CONTS,
        TOO_SHORT | OVERLONG_2,
        TOO_SHORT,
        TOO_SHORT | OVERLONG_3 | SURROGATE,
        TOO_SHORT | TOO_LARGE | TOO_LARGE_1000 | OVERLONG_4);
    const __m256i byte_1_low = tbl(
        CARRY | OVERLONG_3 | OVERLONG_2 | OVERLONG_4,
        CARRY | OVERLONG_2,
        CARRY, CARRY,
        CARRY | TOO_LARGE,
        CARRY | TOO_LARGE | TOO_LARGE_1000,
        CARRY | TOO_LARGE | TOO_LARGE_1000, CARRY | TOO_LARGE | TOO_LARGE_1000,
        CARRY | TOO_LARGE | TOO_LARGE_1000, CARRY | TOO_LARGE | TOO_LARGE_1000,
        CARRY | TOO_LARGE | TOO_LARGE_1000, CARRY | TOO_LARGE | TOO_LARGE_1000,
        CARRY | TOO_LARGE | TOO_LARGE_1000,
        CARRY | TOO_LARGE | TOO_LARGE_1000 | SURROGATE,
        CARRY | TOO_LARGE | TOO_LARGE_1000, CARRY | TOO_LARGE | TOO_LARGE_1000);
    const __m256i byte_2_high = tbl(
        TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,
        TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE_1000 | OVERLONG_4,
        TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE,
        TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE | TOO_LARGE,
        TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE | TOO_LARGE,
        TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT);
    const __m256i nib = _mm256_set1_epi8(0x0F);
    // Bytes that leave a sequence incomplete when they sit in the last 1/2/3
    // positions of a chunk (≥ C0 / ≥ E0 / ≥ F0 respectively).
    const __m256i max_incomplete = _mm256_setr_epi8(
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        static_cast<char>(0xF0 - 1), static_cast<char>(0xE0 - 1), static_cast<char>(0xC0 - 1));
    const __m256i v_e0 = _mm256_set1_epi8(static_cast<char>(0xE0 - 0x80));
    const __m256i v_f0 = _mm256_set1_epi8(static_cast<char>(0xF0 - 0x80));
    const __m256i v_80 = _mm256_set1_epi8(static_cast<char>(0x80));

    __m256i error = _mm256_setzero_si256();
    __m256i prev_input = _mm256_setzero_si256();
    __m256i prev_incomplete = _mm256_setzero_si256();

    auto step = [&](__m256i input) __attribute__((always_inline, target("avx2"))) {
        if (_mm256_movemask_epi8(input) == 0) {                       // ASCII chunk
            error = _mm256_or_si256(error, prev_incomplete);
            prev_incomplete = _mm256_setzero_si256();
            prev_input = input;
            return;
        }
        const __m256i shifted = _mm256_permute2x128_si256(prev_input, input, 0x21);
        const __m256i prev1 = _mm256_alignr_epi8(input, shifted, 16 - 1);
        const __m256i prev2 = _mm256_alignr_epi8(input, shifted, 16 - 2);
        const __m256i prev3 = _mm256_alignr_epi8(input, shifted, 16 - 3);
        const __m256i b1h = _mm256_shuffle_epi8(byte_1_high, _mm256_and_si256(_mm256_srli_epi16(prev1, 4), nib));
        const __m256i b1l = _mm256_shuffle_epi8(byte_1_low,  _mm256_and_si256(prev1, nib));
        const __m256i b2h = _mm256_shuffle_epi8(byte_2_high, _mm256_and_si256(_mm256_srli_epi16(input, 4), nib));
        const __m256i sc  = _mm256_and_si256(_mm256_and_si256(b1h, b1l), b2h);
        const __m256i is_third  = _mm256_subs_epu8(prev2, v_e0);
        const __m256i is_fourth = _mm256_subs_epu8(prev3, v_f0);
        const __m256i must23_80 = _mm256_and_si256(_mm256_or_si256(is_third, is_fourth), v_80);
        error = _mm256_or_si256(error, _mm256_xor_si256(must23_80, sc));
        prev_incomplete = _mm256_subs_epu8(input, max_incomplete);
        prev_input = input;
    };

    size_t pos = 0;
    for (; pos + 32 <= len; pos += 32)
        step(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos)));
    if (pos < len) {
        alignas(32) char tail[32] = {};
        std::memcpy(tail, data + pos, len - pos);
        step(_mm256_load_si256(reinterpret_cast<const __m256i*>(tail)));
    }
    error = _mm256_or_si256(error, prev_incomplete);
    return _mm256_testz_si256(error, error) != 0;
}

[[nodiscard]] inline bool validate_utf8(const char* data, size_t len) noexcept {
    return validate_utf8_avx2(data, len);
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
        std::string_view input, uint32_t* tokens, tape_entry* te, uint32_t& flags) noexcept
{
    static int8_t state = -1;
    if (__builtin_expect(state < 0, 0)) state = cpu_has_avx512bw() ? 1 : 0;
    if (state != 0)
        return build_tape_decoupled_avx512(input, tokens, te, flags);
    return build_tape_decoupled_avx2(input, tokens, te, flags);
}

// ── Runtime dispatch [uint32_t*]: OnDemand pure index build ───────────────
[[gnu::always_inline]] inline size_t build_structural_indexes(
        std::string_view input, uint32_t* indexes, uint64_t* bs_bits, uint32_t& flags) noexcept
{
    static int8_t state = -1;
    if (__builtin_expect(state < 0, 0)) state = cpu_has_avx512bw() ? 1 : 0;
    if (state != 0)
        return build_indexes_avx512(input, indexes, bs_bits, flags);
    return build_indexes_avx2(input, indexes, bs_bits, flags);
}

// Maps stage-1 flags to a deterministic error (fixed priority order).
// UTF-8 is only validated when a non-ASCII byte was seen.
[[gnu::noinline]] inline std::optional<parse_error> stage1_error(uint32_t flags, std::string_view input) noexcept {
    if ((flags & s1_non_ascii) && !validate_utf8(input.data(), input.size()))
        return parse_error{error_code::utf8_error, "Invalid UTF-8"};
    if (flags & s1_unclosed_str)
        return parse_error{error_code::unclosed_string, "Unterminated string"};
    if (flags & s1_ctrl_in_string)
        return parse_error{error_code::unescaped_control, "Unescaped control character in string"};
    if (flags & s1_too_deep)
        return parse_error{error_code::depth_exceeded, "Nesting too deep"};
    if (flags & s1_bad_brackets)
        return parse_error{error_code::invalid_json, "Unbalanced or mismatched brackets"};
    return std::nullopt;
}

} // namespace detail

// ── Exported: which SIMD tier will be used at runtime ─────────────────────
[[nodiscard]] inline bool has_avx512() noexcept { return detail::cpu_has_avx512bw(); }

// Structural tape (bracket partners pre-matched). Validates: UTF-8, string
// termination, no raw control bytes in strings, balanced and correctly paired
// brackets, nesting ≤ 4096, non-empty input. Scalars are validated lazily by
// the typed accessors (strict grammar, see detail::scan_number).
[[nodiscard, gnu::always_inline]] inline auto parse(std::string_view input, turbo_parser& parser)
    -> result<turbo_document>
{
    turbo_document doc(input, parser);
    uint32_t flags = 0;
    size_t count = detail::build_structural_index_decoupled(
        input, parser.tokens(), parser.tape(), flags);
    if (__builtin_expect(flags != 0, 0)) {
        if (auto e = detail::stage1_error(flags, input)) return std::unexpected(*e);
    }
    if (__builtin_expect(count == 0, 0))
        return std::unexpected(parse_error{error_code::empty_document, "Empty document"});
    doc.set_structurals_count(count);
    return doc;
}

// OnDemand (lazy) index. Validates in stage 1 exactly what simdjson OnDemand
// validates up front: UTF-8, string termination, no raw control bytes in
// strings, non-empty input. Structure and scalars are validated as accessed.
[[nodiscard, gnu::always_inline]] inline auto iterate(std::string_view input, turbo_parser& parser)
    -> result<turbo_document>
{
    parser.allocate_indexes(input.size());
    uint32_t flags = 0;
    size_t count = detail::build_structural_indexes(
        input, parser.indexes(), nullptr, flags);
    if (__builtin_expect(flags != 0, 0)) {
        if (auto e = detail::stage1_error(flags, input)) return std::unexpected(*e);
    }
    if (__builtin_expect(count == 0, 0))
        return std::unexpected(parse_error{error_code::empty_document, "Empty document"});
    return turbo_document(input, parser.indexes(), count);
}

// =========================================================================
// Tape DOM — fully validated, materialized document on the Turbo stage 1.
//
//   dom_parser p;                       // reuse across documents (buffers grow, never shrink)
//   auto root = p.parse(json);          // result<dom_element>
//   double c = (*root)["records"]->at(0)->operator[]("close")->get_double().value();
//
// Design (vs simdjson DOM):
//   • Stage 1 = build_indexes_avx512 / build_indexes_avx2 (the OnDemand
//     indexer) plus a per-64-byte-block backslash bitmap, UTF-8 / control /
//     termination validation in the same pass.
//   • Stage 2 = one goto-driven state machine over the structural indexes that
//     validates the full RFC 8259 grammar and emits 16-byte tape nodes,
//     parsing numbers (correctly rounded, see detail::to_double) in the same
//     loop.
//   • Zero-copy strings: a string is a (offset, length) view into the input
//     unless its 64-byte block(s) contain a backslash and memchr confirms an
//     escape — only then is it unescaped into a side buffer. simdjson copies
//     every string (keys included).
//   • Inline short strings: content ≤ 8 bytes is stored in the node itself
//     (zero-padded), so typical keys compare as one length check plus one
//     64-bit compare and lookups never touch the input bytes (perf: the
//     input reads were the dominant L2-miss source in array iteration).
//   • Every node stores `next` (index of its next sibling), so skipping any
//     container is one load: O(1). Navigation uses dom_skip (predictable
//     container/scalar branch) so scalar runs are not a serial load chain.
//   • Object lookup compares key length first, then bytes.
//   • 128-bit integers are kept exactly (side table); beyond ±2^128 → double.
//
// Lifetime: elements reference the parser's tape AND the caller's input bytes
// (zero-copy). Both must outlive every dom_element / string_view obtained;
// the next parse() on the same parser invalidates previous results.
// =========================================================================

enum class dom_type : uint8_t {
    null_value, boolean, int64, uint64, int128, uint128, double_value, string, array, object
};

struct dom_node {
    uint8_t  tag;       // dom_tag::*
    uint8_t  flags;     // string: dom_str_in_sbuf | dom_str_inline; int64: dom_neg_zero
    uint16_t reserved;
    uint32_t next;      // containers: index one past the last descendant (next sibling)
                        // strings:    byte length          other scalars: 0
    uint64_t payload;   // i64/u64/double bits | string: byte offset, or the bytes
                        // themselves if inline | container: element count |
                        // 128-bit integer: side-table index
};
inline constexpr uint8_t dom_str_in_sbuf = 1;   // offset is into the parser's unescape buffer
inline constexpr uint8_t dom_str_inline  = 2;   // ≤ 8 bytes stored in payload (zero-padded)
inline constexpr uint8_t dom_neg_zero    = 4;   // int64 node from the literal "-0": get_double() → -0.0
static_assert(sizeof(dom_node) == 16, "dom_node must be 16 bytes");

namespace dom_tag {
inline constexpr uint8_t null_v  = 'n';
inline constexpr uint8_t true_v  = 't';
inline constexpr uint8_t false_v = 'f';
inline constexpr uint8_t i64     = 'l';
inline constexpr uint8_t u64     = 'u';
inline constexpr uint8_t i128    = 'L';
inline constexpr uint8_t u128    = 'U';
inline constexpr uint8_t dbl     = 'd';
inline constexpr uint8_t str     = '"';
inline constexpr uint8_t arr     = '[';
inline constexpr uint8_t obj     = '{';
} // namespace dom_tag

// Next sibling of p. A *predictable branch* (container vs scalar) instead of
// the data-dependent `tape + p->next`: the CPU speculates p + 1 for scalars
// and keeps loading ahead, rather than serializing on the `next` load (perf:
// the data-dependent chase made array-of-objects iteration ~3x slower).
[[gnu::always_inline]] inline const dom_node* dom_skip(const dom_node* tape, const dom_node* p) noexcept {
    if (__builtin_expect((p->tag | 0x20u) == 0x7Bu, 0)) return tape + p->next;   // '{' or '['
    return p + 1;
}

// Read-only view of one parsed document (owned by dom_parser).
struct dom_document_view {
    const dom_node*          tape  = nullptr;
    const char*              input = nullptr;
    const char*              sbuf  = nullptr;   // unescaped string bytes
    const unsigned __int128* big   = nullptr;   // 128-bit integer side table
};

[[gnu::always_inline]] inline std::string_view dom_str(const dom_document_view* d, const dom_node* n) noexcept {
    const char* p;
    if (n->flags & dom_str_inline) p = reinterpret_cast<const char*>(&n->payload);
    else p = ((n->flags & dom_str_in_sbuf) ? d->sbuf : d->input) + static_cast<uint32_t>(n->payload);
    return {p, static_cast<size_t>(n->next)};
}

class dom_element;
class dom_array;
class dom_object;

struct dom_field;

class dom_element {
    const dom_document_view* doc_ = nullptr;
    const dom_node*          n_   = nullptr;
public:
    constexpr dom_element() noexcept = default;
    constexpr dom_element(const dom_document_view* d, const dom_node* n) noexcept : doc_(d), n_(n) {}

    [[nodiscard]] dom_type type() const noexcept;
    [[nodiscard, gnu::always_inline]] inline bool is_null()   const noexcept { return n_->tag == dom_tag::null_v; }
    [[nodiscard, gnu::always_inline]] inline bool is_bool()   const noexcept { return n_->tag == dom_tag::true_v || n_->tag == dom_tag::false_v; }
    [[nodiscard, gnu::always_inline]] inline bool is_string() const noexcept { return n_->tag == dom_tag::str; }
    [[nodiscard, gnu::always_inline]] inline bool is_array()  const noexcept { return n_->tag == dom_tag::arr; }
    [[nodiscard, gnu::always_inline]] inline bool is_object() const noexcept { return n_->tag == dom_tag::obj; }
    [[nodiscard, gnu::always_inline]] inline bool is_number() const noexcept {
        const uint8_t t = n_->tag;
        return t == dom_tag::dbl || t == dom_tag::i64 || t == dom_tag::u64 || t == dom_tag::i128 || t == dom_tag::u128;
    }
    [[nodiscard, gnu::always_inline]] inline bool is_integer() const noexcept {
        const uint8_t t = n_->tag;
        return t == dom_tag::i64 || t == dom_tag::u64 || t == dom_tag::i128 || t == dom_tag::u128;
    }

    [[nodiscard]] result<bool>              get_bool()    const noexcept;
    [[nodiscard]] result<int64_t>           get_int64()   const noexcept;
    [[nodiscard]] result<uint64_t>          get_uint64()  const noexcept;
    [[nodiscard]] result<__int128>          get_int128()  const noexcept;
    [[nodiscard]] result<unsigned __int128> get_uint128() const noexcept;
    [[nodiscard]] result<double>            get_double()  const noexcept;   // any number → double
    [[nodiscard]] result<std::string_view>  get_string()  const noexcept;
    [[nodiscard]] result<dom_array>         get_array()   const noexcept;
    [[nodiscard]] result<dom_object>        get_object()  const noexcept;

    // Object field lookup (type_error if not an object, key_not_found if absent).
    [[nodiscard]] result<dom_element> operator[](std::string_view key) const noexcept;
    // Array element (type_error if not an array, out_of_bounds if i >= size).
    [[nodiscard]] result<dom_element> at(size_t i) const noexcept;

    [[nodiscard]] const dom_node* node() const noexcept { return n_; }
};

struct dom_field {
    std::string_view key;
    dom_element      value;
};

class dom_array {
    const dom_document_view* doc_;
    const dom_node*          n_;
public:
    constexpr dom_array(const dom_document_view* d, const dom_node* n) noexcept : doc_(d), n_(n) {}
    struct iterator {
        const dom_document_view* doc_;
        const dom_node*          p_;
        [[gnu::always_inline]] inline dom_element operator*() const noexcept { return dom_element(doc_, p_); }
        [[gnu::always_inline]] inline iterator& operator++() noexcept { p_ = dom_skip(doc_->tape, p_); return *this; }
        [[gnu::always_inline]] inline bool operator!=(const iterator& o) const noexcept { return p_ != o.p_; }
        [[gnu::always_inline]] inline bool operator==(const iterator& o) const noexcept { return p_ == o.p_; }
    };
    [[nodiscard, gnu::always_inline]] inline iterator begin() const noexcept { return {doc_, n_ + 1}; }
    [[nodiscard, gnu::always_inline]] inline iterator end()   const noexcept { return {doc_, doc_->tape + n_->next}; }
    [[nodiscard, gnu::always_inline]] inline size_t size() const noexcept { return static_cast<size_t>(n_->payload); }
    [[nodiscard]] result<dom_element> at(size_t i) const noexcept {
        if (i >= size()) return std::unexpected(parse_error{error_code::out_of_bounds, "Array index out of bounds"});
        const dom_node* p = n_ + 1;
        for (; i; --i) p = dom_skip(doc_->tape, p);
        return dom_element(doc_, p);
    }
};

class dom_object {
    const dom_document_view* doc_;
    const dom_node*          n_;
public:
    constexpr dom_object(const dom_document_view* d, const dom_node* n) noexcept : doc_(d), n_(n) {}
    struct iterator {
        const dom_document_view* doc_;
        const dom_node*          p_;   // key node
        [[gnu::always_inline]] inline dom_field operator*() const noexcept {
            return {dom_str(doc_, p_), dom_element(doc_, p_ + 1)};
        }
        [[gnu::always_inline]] inline iterator& operator++() noexcept { p_ = dom_skip(doc_->tape, p_ + 1); return *this; }
        [[gnu::always_inline]] inline bool operator!=(const iterator& o) const noexcept { return p_ != o.p_; }
        [[gnu::always_inline]] inline bool operator==(const iterator& o) const noexcept { return p_ == o.p_; }
    };
    [[nodiscard, gnu::always_inline]] inline iterator begin() const noexcept { return {doc_, n_ + 1}; }
    [[nodiscard, gnu::always_inline]] inline iterator end()   const noexcept { return {doc_, doc_->tape + n_->next}; }
    [[nodiscard, gnu::always_inline]] inline size_t size() const noexcept { return static_cast<size_t>(n_->payload); }

    // Linear scan: length compare first, then bytes. First match wins
    // (duplicate keys are legal JSON; RFC 8259 leaves semantics open).
    [[nodiscard, gnu::always_inline]] inline result<dom_element> find_field(std::string_view key) const noexcept {
        const dom_node* const tape = doc_->tape;
        const dom_node* p   = n_ + 1;
        const dom_node* end = tape + n_->next;
        const size_t klen = key.size();
        if (__builtin_expect(klen <= 8, 1)) {
            // Keys ≤ 8 bytes are always inline: compare length + one word.
            uint64_t kw = 0;
            std::memcpy(&kw, key.data(), klen);
            while (p < end) {
                if (p->next == klen && p->payload == kw) return dom_element(doc_, p + 1);
                p = dom_skip(tape, p + 1);
            }
        } else {
            while (p < end) {
                if (p->next == klen && std::memcmp(dom_str(doc_, p).data(), key.data(), klen) == 0)
                    return dom_element(doc_, p + 1);
                p = dom_skip(tape, p + 1);
            }
        }
        return std::unexpected(parse_error{error_code::key_not_found, "Key not found"});
    }
    [[nodiscard, gnu::always_inline]] inline result<dom_element> operator[](std::string_view key) const noexcept {
        return find_field(key);
    }
};

// ---- dom_element out-of-line inline definitions --------------------------
[[gnu::always_inline]] inline dom_type dom_element::type() const noexcept {
    switch (n_->tag) {
        case dom_tag::null_v:  return dom_type::null_value;
        case dom_tag::true_v:
        case dom_tag::false_v: return dom_type::boolean;
        case dom_tag::i64:     return dom_type::int64;
        case dom_tag::u64:     return dom_type::uint64;
        case dom_tag::i128:    return dom_type::int128;
        case dom_tag::u128:    return dom_type::uint128;
        case dom_tag::dbl:     return dom_type::double_value;
        case dom_tag::str:     return dom_type::string;
        case dom_tag::arr:     return dom_type::array;
        default:               return dom_type::object;
    }
}
[[gnu::always_inline]] inline result<bool> dom_element::get_bool() const noexcept {
    if (n_->tag == dom_tag::true_v)  return true;
    if (n_->tag == dom_tag::false_v) return false;
    return std::unexpected(parse_error{error_code::type_error, "Not a boolean"});
}
[[gnu::always_inline]] inline result<int64_t> dom_element::get_int64() const noexcept {
    switch (n_->tag) {
        case dom_tag::i64: return static_cast<int64_t>(n_->payload);
        case dom_tag::u64: return std::unexpected(parse_error{error_code::number_out_of_range, "Number out of range"});
        case dom_tag::i128: case dom_tag::u128:
            return std::unexpected(parse_error{error_code::number_out_of_range, "Number out of range"});
        default: return std::unexpected(parse_error{error_code::type_error, "Not an integer"});
    }
}
[[gnu::always_inline]] inline result<uint64_t> dom_element::get_uint64() const noexcept {
    switch (n_->tag) {
        case dom_tag::i64:
            if (static_cast<int64_t>(n_->payload) < 0)
                return std::unexpected(parse_error{error_code::number_out_of_range, "Number out of range"});
            return n_->payload;
        case dom_tag::u64: return n_->payload;
        case dom_tag::i128: case dom_tag::u128:
            return std::unexpected(parse_error{error_code::number_out_of_range, "Number out of range"});
        default: return std::unexpected(parse_error{error_code::type_error, "Not an integer"});
    }
}
[[gnu::always_inline]] inline result<__int128> dom_element::get_int128() const noexcept {
    switch (n_->tag) {
        case dom_tag::i64:  return static_cast<__int128>(static_cast<int64_t>(n_->payload));
        case dom_tag::u64:  return static_cast<__int128>(n_->payload);
        case dom_tag::i128: return static_cast<__int128>(doc_->big[n_->payload]);
        case dom_tag::u128: {
            const unsigned __int128 v = doc_->big[n_->payload];
            if (v > static_cast<unsigned __int128>(std::numeric_limits<__int128>::max()))
                return std::unexpected(parse_error{error_code::number_out_of_range, "Number out of range"});
            return static_cast<__int128>(v);
        }
        default: return std::unexpected(parse_error{error_code::type_error, "Not an integer"});
    }
}
[[gnu::always_inline]] inline result<unsigned __int128> dom_element::get_uint128() const noexcept {
    switch (n_->tag) {
        case dom_tag::i64:
            if (static_cast<int64_t>(n_->payload) < 0)
                return std::unexpected(parse_error{error_code::number_out_of_range, "Number out of range"});
            return static_cast<unsigned __int128>(n_->payload);
        case dom_tag::u64:  return static_cast<unsigned __int128>(n_->payload);
        case dom_tag::u128: return doc_->big[n_->payload];
        case dom_tag::i128: return std::unexpected(parse_error{error_code::number_out_of_range, "Number out of range"});
        default: return std::unexpected(parse_error{error_code::type_error, "Not an integer"});
    }
}
[[gnu::always_inline]] inline result<double> dom_element::get_double() const noexcept {
    const uint8_t t = n_->tag;
    if (__builtin_expect(t == dom_tag::dbl, 1)) return std::bit_cast<double>(n_->payload);
    switch (t) {
        case dom_tag::i64:
            if (__builtin_expect(n_->flags & dom_neg_zero, 0)) return -0.0;   // literal "-0"
            return static_cast<double>(static_cast<int64_t>(n_->payload));
        case dom_tag::u64:  return static_cast<double>(n_->payload);
        case dom_tag::i128: return static_cast<double>(static_cast<__int128>(doc_->big[n_->payload]));
        case dom_tag::u128: return static_cast<double>(doc_->big[n_->payload]);
        default: return std::unexpected(parse_error{error_code::type_error, "Not a number"});
    }
}
[[gnu::always_inline]] inline result<std::string_view> dom_element::get_string() const noexcept {
    if (__builtin_expect(n_->tag != dom_tag::str, 0))
        return std::unexpected(parse_error{error_code::type_error, "Not a string"});
    return dom_str(doc_, n_);
}
[[gnu::always_inline]] inline result<dom_array> dom_element::get_array() const noexcept {
    if (__builtin_expect(n_->tag != dom_tag::arr, 0))
        return std::unexpected(parse_error{error_code::type_error, "Not an array"});
    return dom_array(doc_, n_);
}
[[gnu::always_inline]] inline result<dom_object> dom_element::get_object() const noexcept {
    if (__builtin_expect(n_->tag != dom_tag::obj, 0))
        return std::unexpected(parse_error{error_code::type_error, "Not an object"});
    return dom_object(doc_, n_);
}
[[gnu::always_inline]] inline result<dom_element> dom_element::operator[](std::string_view key) const noexcept {
    if (__builtin_expect(n_->tag != dom_tag::obj, 0))
        return std::unexpected(parse_error{error_code::type_error, "Not an object"});
    return dom_object(doc_, n_).find_field(key);
}
[[gnu::always_inline]] inline result<dom_element> dom_element::at(size_t i) const noexcept {
    if (__builtin_expect(n_->tag != dom_tag::arr, 0))
        return std::unexpected(parse_error{error_code::type_error, "Not an array"});
    return dom_array(doc_, n_).at(i);
}

namespace detail {

struct dom_frame { uint32_t tape_idx; uint32_t count; };

[[gnu::always_inline]] inline bool hex4(const char* p, uint32_t& out) noexcept {
    uint32_t v = 0;
    for (int k = 0; k < 4; ++k) {
        const uint8_t c = static_cast<uint8_t>(p[k]);
        uint32_t d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if ((c | 0x20u) >= 'a' && (c | 0x20u) <= 'f') d = (c | 0x20u) - 'a' + 10;
        else return false;
        v = (v << 4) | d;
    }
    out = v;
    return true;
}

// Unescapes JSON string content src[0, n) into dst. Returns the output length,
// or -1 for an invalid escape (unknown escape char, bad hex, lone surrogate).
// Output is never longer than the input.
inline int64_t unescape_json_string(const char* src, size_t n, char* dst) noexcept {
    char* d = dst;
    size_t i = 0;
    while (i < n) {
        const void* bs = std::memchr(src + i, '\\', n - i);
        const size_t run = bs ? static_cast<size_t>(static_cast<const char*>(bs) - (src + i)) : n - i;
        std::memcpy(d, src + i, run);
        d += run; i += run;
        if (i >= n) break;
        if (i + 1 >= n) return -1;
        const char e = src[i + 1];
        switch (e) {
            case '"':  *d++ = '"';  i += 2; break;
            case '\\': *d++ = '\\'; i += 2; break;
            case '/':  *d++ = '/';  i += 2; break;
            case 'b':  *d++ = '\b'; i += 2; break;
            case 'f':  *d++ = '\f'; i += 2; break;
            case 'n':  *d++ = '\n'; i += 2; break;
            case 'r':  *d++ = '\r'; i += 2; break;
            case 't':  *d++ = '\t'; i += 2; break;
            case 'u': {
                uint32_t cp;
                if (i + 6 > n || !hex4(src + i + 2, cp)) return -1;
                i += 6;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    uint32_t lo;
                    if (i + 6 > n || src[i] != '\\' || src[i + 1] != 'u' || !hex4(src + i + 2, lo)) return -1;
                    if (lo < 0xDC00 || lo > 0xDFFF) return -1;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i += 6;
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return -1;                                            // lone low surrogate
                }
                if (cp < 0x80) {
                    *d++ = static_cast<char>(cp);
                } else if (cp < 0x800) {
                    *d++ = static_cast<char>(0xC0 | (cp >> 6));
                    *d++ = static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    *d++ = static_cast<char>(0xE0 | (cp >> 12));
                    *d++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    *d++ = static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    *d++ = static_cast<char>(0xF0 | (cp >> 18));
                    *d++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    *d++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    *d++ = static_cast<char>(0x80 | (cp & 0x3F));
                }
                break;
            }
            default: return -1;
        }
    }
    return d - dst;
}

struct dom_build_ctx {
    const char*                      in;
    size_t                           len;
    const uint64_t*                  bs;      // backslash-block bitmap (filled tile by tile)
    char*                            sbuf;
    std::vector<unsigned __int128>*  big;
    dom_frame*                       stack;
    size_t                           max_depth;
};

// Tiled stage 1 feeding stage 2: the index window is refilled one input
// tile at a time, so the indexes (and the input bytes stage 2 re-reads) are
// still L1/L2-resident when consumed. simdjson runs stage 1 over the whole
// document first; for multi-MB inputs its index array and the input are
// re-read from L3/DRAM by stage 2.
struct s1_window { const uint32_t* ip; const uint32_t* iend; };
struct s1_stream {
    const char*   data;
    size_t        len;
    size_t        pos;        // input bytes indexed so far
    size_t        tile;       // bytes per refill (multiple of 64)
    uint32_t*     buf;        // index window (tile + slack entries)
    uint64_t*     bs_bits;
    bool          avx512;
    bool          done = false;
    bool          any_bs = false;
    s1_tile_state st{};

    // Moves the unconsumed entries [ip, iend) to the front of the window and
    // appends the next tile's indexes; writes the end sentinel (== len) after
    // the final tile.
    [[gnu::noinline]] s1_window refill(const uint32_t* ip, const uint32_t* iend) noexcept {
        const size_t keep = static_cast<size_t>(iend - ip);
        if (keep && ip != buf) std::memmove(buf, ip, keep * sizeof(uint32_t));
        uint32_t* tail = buf + keep;
        const size_t end = (len - pos > tile) ? pos + tile : len;
        tail = avx512 ? index_range_avx512(data, len, pos, end, st, tail, bs_bits)
                      : index_range_avx2(data, len, pos, end, st, tail, bs_bits);
        if (!any_bs && end > pos) {
            for (size_t w = pos >> 12, we = (end - 1) >> 12; w <= we; ++w) any_bs |= (bs_bits[w] != 0);
        }
        pos = end;
        if (pos == len) {
            // Unclosed final string: its opening quote is necessarily the last
            // structural (everything after it is string content), and it is
            // still in the window (FJ_ENSURE never consumes a token without
            // its successor). Drop it, so stage 2 never sees a string with no
            // closing quote (its length would underflow → over-read). The
            // grammar then fails at end of input and stage 1's higher-priority
            // unclosed_string is reported.
            if (st.prev_string_mask && tail > buf) --tail;
            *tail = static_cast<uint32_t>(len); done = true;
        }
        return {buf, tail};
    }
    void drain() noexcept { while (!done) refill(buf, buf); }
};

// Two 64-bit stores per node.
[[gnu::always_inline]] inline void put_node(dom_node* nd, uint8_t tag, uint8_t flags, uint32_t next, uint64_t payload) noexcept {
    const uint64_t w0 = static_cast<uint64_t>(tag) | (static_cast<uint64_t>(flags) << 8) | (static_cast<uint64_t>(next) << 32);
    std::memcpy(reinterpret_cast<char*>(nd), &w0, 8);
    std::memcpy(reinterpret_cast<char*>(nd) + 8, &payload, 8);
}

// Up to 8 bytes that end at the end of the input (cold path).
[[gnu::noinline]] inline uint64_t load_tail8(const char* p, uint32_t n) noexcept {
    uint64_t w = 0;
    std::memcpy(&w, p, n < 8 ? n : 8);
    return w;
}

// Escaped string (rare): unescape into the side buffer; ≤ 8 bytes go inline.
// Returns the new side-buffer fill, or -1 for an invalid escape.
[[gnu::noinline]] inline int64_t dom_emit_escaped_string(const char* src, uint32_t slen, char* sbuf,
                                                         uint32_t sb, dom_node* tp) noexcept {
    const int64_t out = unescape_json_string(src, slen, sbuf + sb);
    if (__builtin_expect(out < 0, 0)) return -1;
    if (out <= 8) {
        uint64_t w = 0;
        std::memcpy(&w, sbuf + sb, static_cast<size_t>(out));
        put_node(tp, dom_tag::str, dom_str_inline, static_cast<uint32_t>(out), w);
        return sb;
    }
    put_node(tp, dom_tag::str, dom_str_in_sbuf, static_cast<uint32_t>(out), sb);
    return static_cast<int64_t>(sb) + out;
}

// String token at ip[0] (opening quote) → node *tp.
[[gnu::always_inline]] inline error_code dom_emit_string(const dom_build_ctx& cx, const uint32_t* ip, dom_node* tp,
                                                         uint32_t& sb, bool any_bs) noexcept {
    const char* const in = cx.in;
    const uint32_t start = ip[0] + 1;
    // Closing quote = last non-whitespace byte before the next structural
    // (stage 1 guarantees nothing but whitespace can sit in between); almost
    // always the byte immediately before it.
    uint32_t e = ip[1] - 1;
    if (__builtin_expect(in[e] != '"', 0)) { do { --e; } while (is_json_ws(in[e])); }
    // e >= start always: an unclosed final string (the only way the scan
    // could reach the opening quote) is removed by s1_stream::refill.
    const uint32_t slen = e - start;
    if (__builtin_expect(any_bs, 0)) {
        for (uint32_t b = start >> 6, be = e >> 6; b <= be; ++b) {
            if ((cx.bs[b >> 6] >> (b & 63)) & 1ULL) {
                if (std::memchr(in + start, '\\', slen) != nullptr) {
                    const int64_t r = dom_emit_escaped_string(in + start, slen, cx.sbuf, sb, tp);
                    if (r < 0) return error_code::invalid_json;
                    sb = static_cast<uint32_t>(r);
                    return error_code::success;
                }
                break;
            }
        }
    }
    // Branch-free inline (≤ 8 bytes, zero-padded) vs zero-copy offset: string
    // lengths straddling 8 are common and unpredictable.
    const uint64_t w = (__builtin_expect(start + 8 <= cx.len, 1)) ? load8(in + start) : load_tail8(in + start, slen);
    const bool inl = slen <= 8;
    put_node(tp, dom_tag::str, inl ? dom_str_inline : uint8_t{0}, slen, inl ? _bzhi_u64(w, slen << 3) : start);
    return error_code::success;
}

[[gnu::noinline]] inline error_code dom_emit_bigint(std::vector<unsigned __int128>* big, const number_scan& s, dom_node* tp) {
    // 20..39 digits: exact 128-bit if it fits, else correctly-rounded double.
    unsigned __int128 v = 0;
    bool overflow = false;
    constexpr unsigned __int128 umax = ~static_cast<unsigned __int128>(0);
    for (const char* p = s.start + s.negative; p < s.end; ++p) {
        const uint32_t d = static_cast<uint32_t>(*p - '0');
        if (v > (umax - d) / 10) { overflow = true; break; }
        v = v * 10 + d;
    }
    if (!overflow) {
        if (!s.negative) {
            if (v <= std::numeric_limits<uint64_t>::max()) {
                put_node(tp, dom_tag::u64, 0, 0, static_cast<uint64_t>(v));
            } else {
                big->push_back(v);
                put_node(tp, dom_tag::u128, 0, 0, big->size() - 1);
            }
            return error_code::success;
        }
        constexpr unsigned __int128 i128_min_mag = static_cast<unsigned __int128>(1) << 127;
        if (v <= i128_min_mag) {
            big->push_back(static_cast<unsigned __int128>(0) - v);   // two's complement of -v
            put_node(tp, dom_tag::i128, 0, 0, big->size() - 1);
            return error_code::success;
        }
    }
    double dv = 0.0;
    const num_status st = to_double(s, dv);
    if (st != num_status::ok) return st == num_status::out_of_range ? error_code::number_out_of_range : error_code::invalid_json;
    put_node(tp, dom_tag::dbl, 0, 0, std::bit_cast<uint64_t>(dv));
    return error_code::success;
}

[[gnu::noinline]] inline error_code dom_emit_neg_i128(std::vector<unsigned __int128>* big, uint64_t m, dom_node* tp) {
    big->push_back(static_cast<unsigned __int128>(0) - static_cast<unsigned __int128>(m));
    put_node(tp, dom_tag::i128, 0, 0, big->size() - 1);
    return error_code::success;
}

[[gnu::always_inline]] inline error_code dom_emit_number(const dom_build_ctx& cx, const uint32_t* ip, dom_node* tp) {
    const char* p = cx.in + ip[0];
    const char* const end = cx.in + cx.len;
    number_scan s;
    if (__builtin_expect(p + 24 <= end && scan_number_fast(p, s), 1)) {
        const uint64_t m = s.mantissa;
        if (s.is_integer) {
            // ≤ 8 digits: always fits in signed int64_t.
            put_node(tp, dom_tag::i64, (s.negative && m == 0) ? dom_neg_zero : uint8_t{0}, 0,
                     s.negative ? (uint64_t{0} - m) : m);
            return error_code::success;
        }
        // ≤ 15 digits and exp10 ∈ [-7, -1]: Clinger fast path is guaranteed.
        double d = static_cast<double>(m) / pow10_exact[-s.exp10];
        if (s.negative) d = -d;
        put_node(tp, dom_tag::dbl, 0, 0, std::bit_cast<uint64_t>(d));
        return error_code::success;
    }
    const bool ok = (ip[1] < cx.len) ? scan_number<false>(p, end, s) : scan_number<true>(p, end, s);
    if (__builtin_expect(!ok, 0)) return error_code::invalid_json;
    if (s.is_integer) {
        if (__builtin_expect(s.digits <= 19, 1)) {
            const uint64_t m = s.mantissa;
            if (!s.negative) {
                put_node(tp, m <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) ? dom_tag::i64 : dom_tag::u64,
                         0, 0, m);
                return error_code::success;
            }
            if (__builtin_expect(m <= (uint64_t{1} << 63), 1)) {
                put_node(tp, dom_tag::i64, m == 0 ? dom_neg_zero : uint8_t{0}, 0, uint64_t{0} - m);
                return error_code::success;
            }
            return dom_emit_neg_i128(cx.big, m, tp);
        }
        return dom_emit_bigint(cx.big, s, tp);
    }
    double dv = 0.0;
    const num_status st = to_double(s, dv);
    if (__builtin_expect(st != num_status::ok, 0))
        return st == num_status::out_of_range ? error_code::number_out_of_range : error_code::invalid_json;
    put_node(tp, dom_tag::dbl, 0, 0, std::bit_cast<uint64_t>(dv));
    return error_code::success;
}

[[gnu::always_inline]] inline error_code dom_emit_scalar(const dom_build_ctx& cx, const uint32_t* ip, dom_node* tp,
                                                         uint32_t& sb, bool any_bs) {
    const char c = cx.in[ip[0]];
    switch (c) {
        case '"': return dom_emit_string(cx, ip, tp, sb, any_bs);
        case '-': case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return dom_emit_number(cx, ip, tp);
        case 't':
            if (!match_atom({cx.in, cx.len}, ip[0], "true", 4)) return error_code::invalid_json;
            put_node(tp, dom_tag::true_v, 0, 0, 0);
            return error_code::success;
        case 'f':
            if (!match_atom({cx.in, cx.len}, ip[0], "false", 5)) return error_code::invalid_json;
            put_node(tp, dom_tag::false_v, 0, 0, 0);
            return error_code::success;
        case 'n':
            if (!match_atom({cx.in, cx.len}, ip[0], "null", 4)) return error_code::invalid_json;
            put_node(tp, dom_tag::null_v, 0, 0, 0);
            return error_code::success;
        default:
            return error_code::invalid_json;                              // ',' ':' '}' ']' or junk
    }
}

// Stage 2: grammar-validating tape builder (RFC 8259 value grammar), fed by
// the tiled stage 1 in `S`.
//
// Invariant after every advance: either ip + 1 < iend (ip[0] and ip[1] are
// real indexes), or stage 1 is done and ip + 1 == iend (ip[1] is the end
// sentinel), or ip >= iend (no tokens left). Scalars read ip[1] to bound
// themselves, which this invariant makes safe.
//
// The context is a const local and the hot cursors are plain locals, so they
// stay in registers: the tape stores are uint8_t/uint64_t typed and would
// otherwise alias (and force reloads of) anything whose address escaped.
// Requires BMI2 (_bzhi_u64): every CPU that runs the AVX2/AVX-512 stage 1 has it.
__attribute__((target("bmi,bmi2,lzcnt,popcnt")))
inline error_code dom_build(const dom_build_ctx& cx_in, s1_stream& S, dom_node* const tape, uint32_t& sbuf_used) {
    const dom_build_ctx   cx    = cx_in;
    const char* const     in    = cx.in;
    dom_frame* const      stack = cx.stack;
    const uint32_t*       ip    = S.buf;
    const uint32_t*       iend  = S.buf;
    dom_node*             tp    = tape;
    size_t   depth     = 0;       // number of open containers
    uint32_t cur_open  = 0;       // tape index of the innermost open container
    uint32_t cur_count = 0;       // its element count so far
    uint32_t sb        = 0;
    bool     any_bs    = false;
    char     c;
    error_code ec = error_code::success;

#define FJ_ENSURE()                                                                     \
    do {                                                                                \
        if (__builtin_expect(ip + 1 >= iend, 0)) {                                      \
            while (ip + 1 >= iend && !S.done) {                                         \
                const s1_window w_ = S.refill(ip, iend);                                \
                ip = w_.ip; iend = w_.iend; any_bs = S.any_bs;                          \
            }                                                                           \
            if (ip >= iend) return error_code::invalid_json;                            \
        }                                                                               \
    } while (0)
#define FJ_NEXT() do { ++ip; FJ_ENSURE(); } while (0)

    while (ip + 1 >= iend && !S.done) {
        const s1_window w0 = S.refill(ip, iend);
        ip = w0.ip; iend = w0.iend; any_bs = S.any_bs;
    }
    if (ip >= iend) return error_code::empty_document;
    c = in[*ip];
    if (c == '{') goto object_begin;
    if (c == '[') goto array_begin;
    ec = dom_emit_scalar(cx, ip, tp, sb, any_bs);
    if (ec != error_code::success) return ec;
    ++tp; ++ip;
    goto document_end;

object_begin:
    if (__builtin_expect(depth >= cx.max_depth, 0)) return error_code::depth_exceeded;
    stack[depth++] = {cur_open, cur_count};
    cur_open = static_cast<uint32_t>(tp - tape); cur_count = 0;
    tp->tag = dom_tag::obj;
    ++tp;
    FJ_NEXT();
    c = in[*ip];
    if (c == '}') goto scope_end;
object_key:
    if (__builtin_expect(c != '"', 0)) return error_code::invalid_json;   // key must be a string
    ec = dom_emit_string(cx, ip, tp, sb, any_bs);
    if (__builtin_expect(ec != error_code::success, 0)) return ec;
    ++tp;
    FJ_NEXT();
    if (__builtin_expect(in[*ip] != ':', 0)) return error_code::invalid_json;
    FJ_NEXT();
    ++cur_count;
    c = in[*ip];
    if (c == '{') goto object_begin;
    if (c == '[') goto array_begin;
    ec = dom_emit_scalar(cx, ip, tp, sb, any_bs);
    if (__builtin_expect(ec != error_code::success, 0)) return ec;
    ++tp; ++ip;
object_continue:
    FJ_ENSURE();
    c = in[*ip];
    if (__builtin_expect(c == ',', 1)) {
        FJ_NEXT();
        c = in[*ip];
        goto object_key;
    }
    if (c == '}') goto scope_end;
    return error_code::invalid_json;

array_begin:
    if (__builtin_expect(depth >= cx.max_depth, 0)) return error_code::depth_exceeded;
    stack[depth++] = {cur_open, cur_count};
    cur_open = static_cast<uint32_t>(tp - tape); cur_count = 0;
    tp->tag = dom_tag::arr;
    ++tp;
    FJ_NEXT();
    c = in[*ip];
    if (c == ']') goto scope_end;
array_value:
    ++cur_count;
    if (c == '{') goto object_begin;
    if (c == '[') goto array_begin;
    ec = dom_emit_scalar(cx, ip, tp, sb, any_bs);
    if (__builtin_expect(ec != error_code::success, 0)) return ec;
    ++tp; ++ip;
array_continue:
    FJ_ENSURE();
    c = in[*ip];
    if (__builtin_expect(c == ',', 1)) {
        FJ_NEXT();
        c = in[*ip];
        goto array_value;
    }
    if (c == ']') goto scope_end;
    return error_code::invalid_json;

scope_end:
    {
        dom_node* const open = tape + cur_open;
        const uint8_t open_tag = open->tag;
        if (__builtin_expect(static_cast<uint8_t>(open_tag + 2) != static_cast<uint8_t>(c), 0))
            return error_code::invalid_json;                              // '{' closed by ']' etc.
        put_node(open, open_tag, 0, static_cast<uint32_t>(tp - tape), cur_count);
        const dom_frame f = stack[--depth];
        cur_open = f.tape_idx; cur_count = f.count;
        ++ip;
        if (depth == 0) goto document_end;
        if (tape[cur_open].tag == dom_tag::obj) goto object_continue;
        goto array_continue;
    }

document_end:
    // Anything but whitespace after the root value is an error; this also
    // runs stage 1 to the end so its validation flags are complete.
    while (ip >= iend && !S.done) {
        const s1_window w1 = S.refill(ip, iend);
        ip = w1.ip; iend = w1.iend;
    }
    if (__builtin_expect(ip < iend, 0)) return error_code::trailing_content;
    sbuf_used = sb;
    return error_code::success;
#undef FJ_NEXT
#undef FJ_ENSURE
}

inline std::string_view dom_error_message(error_code ec) noexcept {
    switch (ec) {
        case error_code::invalid_json:        return "Invalid JSON";
        case error_code::number_out_of_range: return "Number out of range";
        case error_code::depth_exceeded:      return "Nesting too deep";
        case error_code::trailing_content:    return "Trailing content after JSON value";
        case error_code::capacity_exceeded:   return "Document too large";
        case error_code::empty_document:      return "Empty document";
        default:                              return "Parse error";
    }
}

[[nodiscard]] inline bool use_avx512() noexcept {
    static const bool v = cpu_has_avx512bw();
    return v;
}

} // namespace detail

// -------------------------------------------------------------------------
// dom_parser — owns all buffers; reuse it across documents.
// -------------------------------------------------------------------------
class dom_parser {
    std::unique_ptr<uint32_t[]>          window_;    // tiled stage-1 index window
    std::unique_ptr<uint64_t[]>          bs_bits_;
    std::unique_ptr<dom_node[]>          tape_;
    std::unique_ptr<char[]>              sbuf_;
    std::unique_ptr<detail::dom_frame[]> stack_;
    std::vector<unsigned __int128>       big_;
    size_t cap_input_ = 0;
    size_t max_depth_;
    dom_document_view doc_{};

public:
    static constexpr size_t default_max_depth = 1024;
    // Inputs are indexed with 32-bit offsets.
    static constexpr size_t max_input_size = 0xFFFF'FF00u;
    // Stage-1 tile: input bytes indexed per refill of the stage-2 window.
    static constexpr size_t tile_bytes = 64 * 1024;

    explicit dom_parser(size_t max_depth = default_max_depth)
        : stack_(std::make_unique_for_overwrite<detail::dom_frame[]>(max_depth ? max_depth : 1)),
          max_depth_(max_depth ? max_depth : 1) {}

    dom_parser(const dom_parser&) = delete;
    dom_parser& operator=(const dom_parser&) = delete;

    [[nodiscard]] size_t max_depth() const noexcept { return max_depth_; }

    // Parses and fully validates `json`. Throws only std::bad_alloc.
    // Memory: the tape is reserved at its proven upper bound (one 16-byte
    // node per input byte — every node starts at a distinct structural
    // byte); untouched pages are never committed by the OS.
    [[nodiscard]] result<dom_element> parse(std::string_view json) {
        const size_t len = json.size();
        if (__builtin_expect(len > max_input_size, 0))
            return std::unexpected(parse_error{error_code::capacity_exceeded, "Document too large"});
        if (__builtin_expect(len > cap_input_ || !tape_, 0)) {
            tape_    = std::make_unique_for_overwrite<dom_node[]>(len + 2);
            bs_bits_ = std::make_unique_for_overwrite<uint64_t[]>((len >> 12) + 2);
            sbuf_    = std::make_unique_for_overwrite<char[]>(len + 64);
            cap_input_ = len;
        }
        if (!window_) window_ = std::make_unique_for_overwrite<uint32_t[]>(tile_bytes + 1024);
        std::memset(bs_bits_.get(), 0, ((len >> 12) + 2) * sizeof(uint64_t));
        big_.clear();

        detail::s1_stream S{json.data(), len, 0, tile_bytes, window_.get(), bs_bits_.get(), detail::use_avx512()};
        const detail::dom_build_ctx cx{json.data(), len, bs_bits_.get(), sbuf_.get(), &big_, stack_.get(), max_depth_};
        uint32_t sbuf_used = 0;
        const error_code ec = detail::dom_build(cx, S, tape_.get(), sbuf_used);
        if (ec != error_code::success) S.drain();          // complete stage-1 validation
        // Stage-1 (encoding / string) errors take priority over grammar
        // errors, so the reported code does not depend on the tiling.
        if (const uint32_t flags = detail::s1_flags(S.st)) {
            if (auto e = detail::stage1_error(flags, json)) return std::unexpected(*e);
        }
        if (__builtin_expect(ec != error_code::success, 0))
            return std::unexpected(parse_error{ec, detail::dom_error_message(ec)});
        doc_ = {tape_.get(), json.data(), sbuf_.get(), big_.data()};
        return dom_element(&doc_, tape_.get());
    }
};

} // namespace fastjson::turbo
