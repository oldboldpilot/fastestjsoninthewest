module;

#include <cstdint>
#include <string_view>
#include <memory>
#include <expected>
#include <charconv>
#include <immintrin.h>
#include <cstring>
#if defined(__x86_64__) || defined(_M_X64)
#  include <cpuid.h>
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
//   • Inline bracket matching during the SIMD scan (no second O(n) pass)
//   • Supports JSON up to 16 777 215 bytes (~16 MB)
//
// v5.1 perf fix:
//   • tape_entry stores use __builtin_memcpy(8) → single QWORD MOV
//     (struct aggregate init {tok, 0u} was generating 2× DWORD stores)
//   • Added uint32_t* overloads of process_group64 / process_scalar_tail
//     for pure IndexOnly path: zero bracket stack, 1 DWORD store per token
//   • Added build_structural_index(string_view, uint32_t*) dispatch
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

// -------------------------------------------------------------------------
// turbo_parser
// -------------------------------------------------------------------------
class turbo_parser {
    std::unique_ptr<tape_entry[]> tape_;
    size_t capacity_ = 0;

public:
    turbo_parser() = default;

    void allocate(size_t capacity) {
        if (capacity > capacity_) {
            tape_     = std::make_unique<tape_entry[]>(capacity + 32);
            capacity_ = capacity;
        }
    }

    tape_entry* tape() noexcept { return tape_.get(); }
};

// -------------------------------------------------------------------------
// turbo_document
// -------------------------------------------------------------------------
class turbo_document {
    std::string_view   input_;
    const tape_entry*  tape_;              // immutable after parse() completes
    size_t             count_ = 0;

public:
    explicit turbo_document(std::string_view input, turbo_parser& parser)
        : input_(input), tape_(nullptr)
    {
        parser.allocate(input.size());
        tape_ = parser.tape();
    }

    // Byte position of structural token i
    [[nodiscard]] uint32_t get_structural(size_t idx) const noexcept {
        return tape_[idx].token & 0x00FF'FFFFu;
    }

    // Character type of structural token i
    [[nodiscard]] char get_structural_char(size_t idx) const noexcept {
        if (idx >= count_) return '\0';
        return static_cast<char>(tape_[idx].token >> 24);
    }

    // O(1) skip: reads token+partner from the SAME 8-byte cache line slot.
    [[nodiscard]] size_t skip_value(size_t idx) const noexcept {
        const tape_entry& e = tape_[idx];
        const char c = static_cast<char>(e.token >> 24);
        if (c == '"') return idx + 2;
        if (c == '{' || c == '[') return e.partner + 1;
        return idx + 1;
    }

    [[nodiscard]] auto root()              const noexcept -> turbo_value;
    [[nodiscard]] size_t structurals_count() const noexcept { return count_; }
    void set_structurals_count(size_t n)   noexcept { count_ = n; }
    [[nodiscard]] std::string_view input() const noexcept { return input_; }
    const tape_entry* tape()               const noexcept { return tape_; }
    void set_tape(const tape_entry* t)     noexcept { tape_ = t; }
};

// -------------------------------------------------------------------------
// turbo_value
// -------------------------------------------------------------------------
class turbo_value {
    const turbo_document* doc_;
    size_t                idx_;

public:
    constexpr turbo_value(const turbo_document* doc, size_t idx) noexcept
        : doc_(doc), idx_(idx) {}

    [[nodiscard]] bool is_object() const noexcept { return doc_->get_structural_char(idx_) == '{'; }
    [[nodiscard]] bool is_array()  const noexcept { return doc_->get_structural_char(idx_) == '['; }
    [[nodiscard]] bool is_string() const noexcept { return doc_->get_structural_char(idx_) == '"'; }
    [[nodiscard]] bool is_number() const noexcept {
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

public:
    constexpr turbo_object(const turbo_document* doc, size_t idx) noexcept
        : doc_(doc), idx_(idx) {}

    [[nodiscard]] result<turbo_value> find_field(std::string_view key) const noexcept;

    [[nodiscard]] turbo_value operator[](std::string_view key) const noexcept {
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

        turbo_value operator*()  const noexcept { return turbo_value(doc_, idx_); }
        iterator& operator++() noexcept {
            idx_ = doc_->skip_value(idx_);
            if (idx_ < doc_->structurals_count() && doc_->get_structural_char(idx_) == ',')
                ++idx_;
            return *this;
        }
        bool operator!=(const iterator& o) const noexcept { return idx_ < o.idx_; }
    };

    iterator begin() const noexcept { return {doc_, idx_ + 1}; }
    iterator end()   const noexcept {
        size_t close = doc_->skip_value(idx_);
        return {doc_, close > 0 ? close - 1 : 0};
    }
};

// -------------------------------------------------------------------------
// Deferred inline definitions
// -------------------------------------------------------------------------
inline auto turbo_document::root() const noexcept -> turbo_value {
    return turbo_value(this, 0);
}
inline auto turbo_value::get_object() const noexcept -> result<turbo_object> {
    if (!is_object())
        return std::unexpected(parse_error{error_code::type_error, "Not an object"});
    return turbo_object(doc_, idx_);
}
inline auto turbo_value::get_array() const noexcept -> result<turbo_array> {
    if (!is_array())
        return std::unexpected(parse_error{error_code::type_error, "Not an array"});
    return turbo_array(doc_, idx_);
}
inline auto turbo_value::get_string() const noexcept -> result<std::string_view> {
    if (!is_string())
        return std::unexpected(parse_error{error_code::type_error, "Not a string"});
    uint32_t start = doc_->get_structural(idx_) + 1;
    uint32_t end   = doc_->get_structural(idx_ + 1);
    return doc_->input().substr(start, end - start);
}
inline auto turbo_value::get_double() const noexcept -> result<double> {
    if (!is_number())
        return std::unexpected(parse_error{error_code::type_error, "Not a number"});
    uint32_t start = doc_->get_structural(idx_);
    uint32_t end   = doc_->get_structural(idx_ + 1);
    double val;
    auto [ptr, ec] = std::from_chars(doc_->input().data() + start, doc_->input().data() + end, val);
    if (ec == std::errc()) return val;
    return std::unexpected(parse_error{error_code::type_error, "Invalid number"});
}
inline auto turbo_value::get_int64() const noexcept -> result<int64_t> {
    if (!is_number())
        return std::unexpected(parse_error{error_code::type_error, "Not a number"});
    uint32_t start = doc_->get_structural(idx_);
    uint32_t end   = doc_->get_structural(idx_ + 1);
    int64_t val;
    auto [ptr, ec] = std::from_chars(doc_->input().data() + start, doc_->input().data() + end, val);
    if (ec == std::errc()) return val;
    return std::unexpected(parse_error{error_code::type_error, "Invalid integer"});
}
inline auto turbo_value::get_bool() const noexcept -> result<bool> {
    char c = doc_->get_structural_char(idx_);
    if (c == 't') return true;
    if (c == 'f') return false;
    return std::unexpected(parse_error{error_code::type_error, "Not a boolean"});
}
inline auto turbo_object::find_field(std::string_view key) const noexcept
    -> result<turbo_value>
{
    size_t curr  = idx_ + 1;
    auto   inp   = doc_->input();
    size_t total = doc_->structurals_count();

    while (curr < total && doc_->get_structural_char(curr) != '}') {
        if (doc_->get_structural_char(curr) == '"') {
            uint32_t ks = doc_->get_structural(curr) + 1;
            uint32_t ke = doc_->get_structural(curr + 1);
            curr += 2; // past opening + closing quote for key

            if (curr < total && doc_->get_structural_char(curr) == ':') ++curr;

            if (ke - ks == key.size() &&
                __builtin_memcmp(inp.data() + ks, key.data(), key.size()) == 0)
                return turbo_value(doc_, curr);

            curr = doc_->skip_value(curr);
            if (curr < total && doc_->get_structural_char(curr) == ',') ++curr;
        } else {
            ++curr;
        }
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

    // Emit tape entries.
    uint64_t mask = combined;
    while (mask) {
        const uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(mask));
        mask &= mask - 1ULL;
        const uint32_t  p   = base + bit;
        const uint8_t   ch  = static_cast<uint8_t>(data[p]);
        const uint32_t  tok = (static_cast<uint32_t>(ch) << 24) | p;

        if (ch == '{' || ch == '[') {
            te[si] = {tok, 0u};
            bkt_stk[++bkt_top] = static_cast<uint32_t>(si);
        } else if ((ch == '}' || ch == ']') & (bkt_top >= 0)) {
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
                __builtin_memcpy(&te[si++], &_q, 8);
                in_string = !in_string;
            } else if (!in_string) {
                const bool is_struct = (c=='{' || c=='}' || c=='[' || c==']' || c==':' || c==',');
                const bool is_scalar_start = !is_struct && c != ' ' && c != '\n' && c != '\r' && c != '\t'
                    && (pos == 0 || ({
                        char p = data[pos-1];
                        p==' '||p=='\n'||p=='\r'||p=='\t'||p=='{'||p=='}'||p=='['||p==']'||p==':'||p==','||p=='"';
                    }));
                if (is_struct || is_scalar_start) {
                    const uint32_t tok = (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 24)
                                       | static_cast<uint32_t>(pos);
                    if (c == '{' || c == '[') {
                        uint64_t _q = static_cast<uint64_t>(tok);
                        __builtin_memcpy(&te[si], &_q, 8);
                        bkt_stk[++bkt_top] = static_cast<uint32_t>(si);
                    } else if ((c == '}' || c == ']') && bkt_top >= 0) {
                        uint32_t open = bkt_stk[bkt_top--];
                        te[open].partner = static_cast<uint32_t>(si);
                        te[si].token   = tok;
                        te[si].partner = open;
                    } else {
                        uint64_t _q = static_cast<uint64_t>(tok);
                        __builtin_memcpy(&te[si], &_q, 8);
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
    __builtin_memcpy(&te[si], &_sentinel, 8);
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
                const bool is_scalar_start = !is_struct && c != ' ' && c != '\n' && c != '\r' && c != '\t'
                    && (pos == 0 || ({
                        char p = data[pos-1];
                        p==' '||p=='\n'||p=='\r'||p=='\t'||p=='{'||p=='}'||p=='['||p==']'||p==':'||p==','||p=='"';
                    }));
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
    const auto [_s0,_w0] = cl32(c0); const auto [_s1,_w1] = cl32(c1); \
    const uint64_t _stm = static_cast<uint64_t>(_s0) | (static_cast<uint64_t>(_s1) << 32); \
    const uint64_t _wsm = static_cast<uint64_t>(_w0) | (static_cast<uint64_t>(_w1) << 32); \
    process_group64(st, _qm, _bm, _stm, _wsm, static_cast<uint32_t>(pos + (OFF)), data, te, si, bkt_stk, bkt_top); \
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
__attribute__((target("avx512f,avx512bw,pclmul")))
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
    uint64_t _stm = 0, _wsm = 0; \
    CL64_AVX512(zmm, _stm, _wsm); \
    process_group64(st, _qm, _bm, _stm, _wsm, \
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

// ── Runtime dispatch [tape_entry*]: AVX-512BW → AVX2 ─────────────────────
inline size_t build_structural_index(
        std::string_view input, tape_entry* te) noexcept
{
    static const bool have_avx512bw = cpu_has_avx512bw();
    if (__builtin_expect(have_avx512bw, 1))
        return build_structural_index_avx512(input, te);
    return build_structural_index_avx2(input, te);
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

} // namespace detail

// ── Exported: which SIMD tier will be used at runtime ─────────────────────
[[nodiscard]] inline bool has_avx512() noexcept { return detail::cpu_has_avx512bw(); }

[[nodiscard]] auto parse(std::string_view input, turbo_parser& parser)
    -> result<turbo_document>
{
    turbo_document doc(input, parser);
    size_t count = detail::build_structural_index(input, parser.tape());
    doc.set_structurals_count(count);
    return doc;
}

} // namespace fastjson::turbo
