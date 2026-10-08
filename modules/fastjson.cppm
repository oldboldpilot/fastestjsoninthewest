// FastestJSONInTheWest - Comprehensive Thread-Safe SIMD Implementation
// Author: Olumuyiwa Oluwasanmi
// Advanced instruction set waterfall with complete thread safety

module;

#if defined(SENSEN_NO_IMPORT_STD)
#include <vector>
#include <string>
#include <string_view>
#include <iostream>
#include <algorithm>
#include <memory>
#include <type_traits>
#include <concepts>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <utility>
#include <optional>
#include <variant>
#include <format>
#include <source_location>
#include <execution>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <future>
#include <exception>
#include <stdexcept>
#include <chrono>
#include <expected>
#include <unordered_map>
#include <array>
#include <span>
#include <memory_resource>
#include <cmath>
#if defined(_LIBCPP_HAS_NO_MONOTONIC_BUFFER_RESOURCE) || defined(_LIBCPP_HAS_NO_INCOMPLETE_SHARED_LIBRARIES)
namespace std::pmr {
    class monotonic_buffer_resource : public std::pmr::memory_resource {
        struct Block {
            void* ptr;
            size_t size;
        };
        std::vector<Block> blocks_;
        size_t initial_size_;
        
    protected:
        void* do_allocate(size_t bytes, size_t alignment) override {
            #if defined(_MSC_VER) || defined(__MINGW32__)
            void* ptr = _aligned_malloc(bytes, alignment);
            #else
            void* ptr = std::aligned_alloc(alignment, (bytes + alignment - 1) & ~(alignment - 1));
            #endif
            if (!ptr) throw std::bad_alloc();
            blocks_.push_back({ptr, bytes});
            return ptr;
        }
        
        void do_deallocate(void* p, size_t bytes, size_t alignment) override {
            (void)p; (void)bytes; (void)alignment;
        }
        
        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
            return this == &other;
        }
        
    public:
        explicit monotonic_buffer_resource(size_t initial_size = 0) : initial_size_(initial_size) {}
        ~monotonic_buffer_resource() override {
            for (auto& b : blocks_) {
                #if defined(_MSC_VER) || defined(__MINGW32__)
                _aligned_free(b.ptr);
                #else
                std::free(b.ptr);
                #endif
            }
        }
    };
}
#endif
#endif

#if defined(_MSC_VER) && !defined(__clang__)
/* uint64_t is needed in the global module fragment before import std; runs */
#include <cstdint>
#include <type_traits>
struct alignas(16) msvc_uint128;
struct alignas(16) msvc_int128;
struct alignas(16) msvc_float128;

// Helper Functions for Carry Math (Declarations)
constexpr auto mul64_to_128(uint64_t u, uint64_t v) noexcept -> msvc_uint128;
constexpr auto divmod128(msvc_uint128 dividend, msvc_uint128 divisor, msvc_uint128& remainder) noexcept -> msvc_uint128;

struct alignas(16) msvc_uint128 {
    uint64_t low{0};
    uint64_t high{0};

    constexpr msvc_uint128() noexcept = default;
    constexpr msvc_uint128(uint64_t l, uint64_t h) noexcept : low(l), high(h) {}
    
    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    constexpr msvc_uint128(T val) noexcept {
        if constexpr (std::is_signed_v<T>) {
            low = static_cast<uint64_t>(val);
            high = val < 0 ? 0xFFFFFFFFFFFFFFFFULL : 0ULL;
        } else {
            low = static_cast<uint64_t>(val);
            high = 0ULL;
        }
    }

    constexpr msvc_uint128(msvc_int128 v) noexcept;

    constexpr explicit operator uint64_t() const noexcept { return low; }
    constexpr explicit operator int64_t() const noexcept { return static_cast<int64_t>(low); }
    constexpr explicit operator bool() const noexcept { return low != 0 || high != 0; }
    constexpr explicit operator double() const noexcept {
        return static_cast<double>(high) * 18446744073709551616.0 + static_cast<double>(low);
    }
    constexpr explicit msvc_uint128(double val) noexcept : low(val >= 1.0 ? static_cast<uint64_t>(val) : 0ULL), high(0ULL) {}
    constexpr explicit msvc_uint128(msvc_float128 v) noexcept;
    constexpr explicit operator msvc_float128() const noexcept;

    // Inline Friend Operators
    friend constexpr auto operator+(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> msvc_uint128 {
        uint64_t low = lhs.low + rhs.low;
        uint64_t high = lhs.high + rhs.high + (low < lhs.low);
        return {low, high};
    }

    friend constexpr auto operator-(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> msvc_uint128 {
        uint64_t low = lhs.low - rhs.low;
        uint64_t high = lhs.high - rhs.high - (lhs.low < rhs.low);
        return {low, high};
    }

    friend constexpr auto operator*(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> msvc_uint128 {
        msvc_uint128 res = mul64_to_128(lhs.low, rhs.low);
        res.high += lhs.high * rhs.low + lhs.low * rhs.high;
        return res;
    }

    friend constexpr auto operator/(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> msvc_uint128 {
        msvc_uint128 remainder;
        return divmod128(lhs, rhs, remainder);
    }

    friend constexpr auto operator%(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> msvc_uint128 {
        msvc_uint128 remainder;
        divmod128(lhs, rhs, remainder);
        return remainder;
    }

    friend constexpr auto operator-(msvc_uint128 val) noexcept -> msvc_uint128 {
        uint64_t low = ~val.low + 1;
        uint64_t high = ~val.high + (low == 0);
        return {low, high};
    }

    friend constexpr auto operator&(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> msvc_uint128 {
        return {lhs.low & rhs.low, lhs.high & rhs.high};
    }
    friend constexpr auto operator|(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> msvc_uint128 {
        return {lhs.low | rhs.low, lhs.high | rhs.high};
    }
    friend constexpr auto operator^(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> msvc_uint128 {
        return {lhs.low ^ rhs.low, lhs.high ^ rhs.high};
    }
    friend constexpr auto operator~(msvc_uint128 val) noexcept -> msvc_uint128 {
        return {~val.low, ~val.high};
    }

    friend constexpr auto operator<<(msvc_uint128 lhs, int count) noexcept -> msvc_uint128 {
        if (count <= 0) return lhs;
        if (count >= 128) return {0, 0};
        if (count >= 64) {
            return {0, lhs.low << (count - 64)};
        } else {
            return {lhs.low << count, (lhs.high << count) | (lhs.low >> (64 - count))};
        }
    }

    friend constexpr auto operator>>(msvc_uint128 lhs, int count) noexcept -> msvc_uint128 {
        if (count <= 0) return lhs;
        if (count >= 128) return {0, 0};
        if (count >= 64) {
            return {lhs.high >> (count - 64), 0};
        } else {
            return {(lhs.low >> count) | (lhs.high << (64 - count)), lhs.high >> count};
        }
    }

    friend constexpr auto operator==(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> bool {
        return lhs.low == rhs.low && lhs.high == rhs.high;
    }
    friend constexpr auto operator!=(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> bool {
        return !(lhs == rhs);
    }
    friend constexpr auto operator<(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> bool {
        if (lhs.high != rhs.high) return lhs.high < rhs.high;
        return lhs.low < rhs.low;
    }
    friend constexpr auto operator>(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> bool { return rhs < lhs; }
    friend constexpr auto operator<=(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> bool { return !(rhs < lhs); }
    friend constexpr auto operator>=(msvc_uint128 lhs, msvc_uint128 rhs) noexcept -> bool { return !(lhs < rhs); }

    constexpr auto operator+=(msvc_uint128 other) noexcept -> msvc_uint128& { *this = *this + other; return *this; }
    constexpr auto operator-=(msvc_uint128 other) noexcept -> msvc_uint128& { *this = *this - other; return *this; }
    constexpr auto operator*=(msvc_uint128 other) noexcept -> msvc_uint128& { *this = *this * other; return *this; }
    constexpr auto operator/=(msvc_uint128 other) noexcept -> msvc_uint128& { *this = *this / other; return *this; }
    constexpr auto operator%=(msvc_uint128 other) noexcept -> msvc_uint128& { *this = *this % other; return *this; }
    constexpr auto operator&=(msvc_uint128 other) noexcept -> msvc_uint128& { *this = *this & other; return *this; }
    constexpr auto operator|=(msvc_uint128 other) noexcept -> msvc_uint128& { *this = *this | other; return *this; }
    constexpr auto operator^=(msvc_uint128 other) noexcept -> msvc_uint128& { *this = *this ^ other; return *this; }
    constexpr auto operator<<=(int count) noexcept -> msvc_uint128& { *this = *this << count; return *this; }
    constexpr auto operator>>=(int count) noexcept -> msvc_uint128& { *this = *this >> count; return *this; }
    
    constexpr auto operator++() noexcept -> msvc_uint128& { *this += 1; return *this; }
    constexpr auto operator++(int) noexcept -> msvc_uint128 { msvc_uint128 tmp = *this; *this += 1; return tmp; }
    constexpr auto operator--() noexcept -> msvc_uint128& { *this -= 1; return *this; }
    constexpr auto operator--(int) noexcept -> msvc_uint128 { msvc_uint128 tmp = *this; *this -= 1; return tmp; }
};

struct alignas(16) msvc_int128 {
    uint64_t low{0};
    int64_t high{0};

    constexpr msvc_int128() noexcept = default;
    constexpr msvc_int128(uint64_t l, int64_t h) noexcept : low(l), high(h) {}
    
    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    constexpr msvc_int128(T val) noexcept {
        if constexpr (std::is_signed_v<T>) {
            low = static_cast<uint64_t>(val);
            high = val < 0 ? -1 : 0;
        } else {
            low = static_cast<uint64_t>(val);
            high = 0;
        }
    }

    constexpr msvc_int128(msvc_uint128 v) noexcept;

    constexpr explicit operator uint64_t() const noexcept { return low; }
    constexpr explicit operator int64_t() const noexcept { return static_cast<int64_t>(low); }
    constexpr explicit operator bool() const noexcept { return low != 0 || high != 0; }
    constexpr explicit operator double() const noexcept {
        return static_cast<double>(high) * 18446744073709551616.0 + static_cast<double>(low);
    }
    constexpr explicit msvc_int128(double val) noexcept : low(static_cast<uint64_t>(static_cast<int64_t>(val))), high(val < 0.0 ? int64_t{-1} : int64_t{0}) {}
    constexpr explicit msvc_int128(msvc_float128 v) noexcept;
    constexpr explicit operator msvc_float128() const noexcept;

    // Inline Friend Operators
    friend constexpr auto operator+(msvc_int128 lhs, msvc_int128 rhs) noexcept -> msvc_int128 {
        uint64_t low = lhs.low + rhs.low;
        int64_t high = lhs.high + rhs.high + (low < lhs.low);
        return {low, high};
    }

    friend constexpr auto operator-(msvc_int128 lhs, msvc_int128 rhs) noexcept -> msvc_int128 {
        uint64_t low = lhs.low - rhs.low;
        int64_t high = lhs.high - rhs.high - (lhs.low < rhs.low);
        return {low, high};
    }

    friend constexpr auto operator*(msvc_int128 lhs, msvc_int128 rhs) noexcept -> msvc_int128 {
        msvc_uint128 u_lhs{lhs.low, static_cast<uint64_t>(lhs.high)};
        msvc_uint128 u_rhs{rhs.low, static_cast<uint64_t>(rhs.high)};
        msvc_uint128 u_res = u_lhs * u_rhs;
        return {u_res.low, static_cast<int64_t>(u_res.high)};
    }

    friend constexpr auto operator/(msvc_int128 lhs, msvc_int128 rhs) noexcept -> msvc_int128 {
        bool neg_lhs = lhs.high < 0;
        bool neg_rhs = rhs.high < 0;

        msvc_uint128 u_lhs = neg_lhs ? static_cast<msvc_uint128>(-lhs) : msvc_uint128{lhs.low, static_cast<uint64_t>(lhs.high)};
        msvc_uint128 u_rhs = neg_rhs ? static_cast<msvc_uint128>(-rhs) : msvc_uint128{rhs.low, static_cast<uint64_t>(rhs.high)};

        msvc_uint128 u_rem;
        msvc_uint128 u_quot = divmod128(u_lhs, u_rhs, u_rem);

        msvc_int128 quot{u_quot.low, static_cast<int64_t>(u_quot.high)};
        return (neg_lhs ^ neg_rhs) ? -quot : quot;
    }

    friend constexpr auto operator%(msvc_int128 lhs, msvc_int128 rhs) noexcept -> msvc_int128 {
        bool neg_lhs = lhs.high < 0;
        bool neg_rhs = rhs.high < 0;

        msvc_uint128 u_lhs = neg_lhs ? static_cast<msvc_uint128>(-lhs) : msvc_uint128{lhs.low, static_cast<uint64_t>(lhs.high)};
        msvc_uint128 u_rhs = neg_rhs ? static_cast<msvc_uint128>(-rhs) : msvc_uint128{rhs.low, static_cast<uint64_t>(rhs.high)};

        msvc_uint128 u_rem;
        divmod128(u_lhs, u_rhs, u_rem);

        msvc_int128 rem{u_rem.low, static_cast<int64_t>(u_rem.high)};
        return neg_lhs ? -rem : rem;
    }

    friend constexpr auto operator-(msvc_int128 val) noexcept -> msvc_int128 {
        uint64_t low = ~val.low + 1;
        int64_t high = ~val.high + (low == 0);
        return {low, high};
    }

    friend constexpr auto operator&(msvc_int128 lhs, msvc_int128 rhs) noexcept -> msvc_int128 {
        return {lhs.low & rhs.low, lhs.high & rhs.high};
    }
    friend constexpr auto operator|(msvc_int128 lhs, msvc_int128 rhs) noexcept -> msvc_int128 {
        return {lhs.low | rhs.low, lhs.high | rhs.high};
    }
    friend constexpr auto operator^(msvc_int128 lhs, msvc_int128 rhs) noexcept -> msvc_int128 {
        return {lhs.low ^ rhs.low, lhs.high ^ rhs.high};
    }
    friend constexpr auto operator~(msvc_int128 val) noexcept -> msvc_int128 {
        return {~val.low, ~val.high};
    }

    friend constexpr auto operator<<(msvc_int128 lhs, int count) noexcept -> msvc_int128 {
        if (count <= 0) return lhs;
        if (count >= 128) return {0, 0};
        if (count >= 64) {
            return {0, static_cast<int64_t>(lhs.low << (count - 64))};
        } else {
            return {lhs.low << count, static_cast<int64_t>((static_cast<uint64_t>(lhs.high) << count) | (lhs.low >> (64 - count)))};
        }
    }

    friend constexpr auto operator>>(msvc_int128 lhs, int count) noexcept -> msvc_int128 {
        if (count <= 0) return lhs;
        if (count >= 128) {
            return lhs.high < 0 ? msvc_int128{0xFFFFFFFFFFFFFFFFULL, -1} : msvc_int128{0, 0};
        }
        if (count >= 64) {
            int64_t new_high = lhs.high < 0 ? -1 : 0;
            return {static_cast<uint64_t>(lhs.high >> (count - 64)), new_high};
        } else {
            return {(lhs.low >> count) | (static_cast<uint64_t>(lhs.high) << (64 - count)), lhs.high >> count};
        }
    }

    friend constexpr auto operator==(msvc_int128 lhs, msvc_int128 rhs) noexcept -> bool {
        return lhs.low == rhs.low && lhs.high == rhs.high;
    }
    friend constexpr auto operator!=(msvc_int128 lhs, msvc_int128 rhs) noexcept -> bool {
        return !(lhs == rhs);
    }
    friend constexpr auto operator<(msvc_int128 lhs, msvc_int128 rhs) noexcept -> bool {
        if (lhs.high != rhs.high) return lhs.high < rhs.high;
        return lhs.low < rhs.low;
    }
    friend constexpr auto operator>(msvc_int128 lhs, msvc_int128 rhs) noexcept -> bool { return rhs < lhs; }
    friend constexpr auto operator<=(msvc_int128 lhs, msvc_int128 rhs) noexcept -> bool { return !(rhs < lhs); }
    friend constexpr auto operator>=(msvc_int128 lhs, msvc_int128 rhs) noexcept -> bool { return !(lhs < rhs); }

    constexpr auto operator+=(msvc_int128 other) noexcept -> msvc_int128& { *this = *this + other; return *this; }
    constexpr auto operator-=(msvc_int128 other) noexcept -> msvc_int128& { *this = *this - other; return *this; }
    constexpr auto operator*=(msvc_int128 other) noexcept -> msvc_int128& { *this = *this * other; return *this; }
    constexpr auto operator/=(msvc_int128 other) noexcept -> msvc_int128& { *this = *this / other; return *this; }
    constexpr auto operator%=(msvc_int128 other) noexcept -> msvc_int128& { *this = *this % other; return *this; }
    constexpr auto operator&=(msvc_int128 other) noexcept -> msvc_int128& { *this = *this & other; return *this; }
    constexpr auto operator|=(msvc_int128 other) noexcept -> msvc_int128& { *this = *this | other; return *this; }
    constexpr auto operator^=(msvc_int128 other) noexcept -> msvc_int128& { *this = *this ^ other; return *this; }
    constexpr auto operator<<=(int count) noexcept -> msvc_int128& { *this = *this << count; return *this; }
    constexpr auto operator>>=(int count) noexcept -> msvc_int128& { *this = *this >> count; return *this; }

    constexpr auto operator++() noexcept -> msvc_int128& { *this += 1; return *this; }
    constexpr auto operator++(int) noexcept -> msvc_int128 { msvc_int128 tmp = *this; *this += 1; return tmp; }
    constexpr auto operator--() noexcept -> msvc_int128& { *this -= 1; return *this; }
    constexpr auto operator--(int) noexcept -> msvc_int128 { msvc_int128 tmp = *this; *this -= 1; return tmp; }
};

// Cross conversion implementations
constexpr msvc_uint128::msvc_uint128(msvc_int128 v) noexcept : low(v.low), high(static_cast<uint64_t>(v.high)) {}
constexpr msvc_int128::msvc_int128(msvc_uint128 v) noexcept : low(v.low), high(static_cast<int64_t>(v.high)) {}

// Helper math implementations
constexpr auto mul64_to_128(uint64_t u, uint64_t v) noexcept -> msvc_uint128 {
    uint64_t u_lo = u & 0xFFFFFFFFULL;
    uint64_t u_hi = u >> 32;
    uint64_t v_lo = v & 0xFFFFFFFFULL;
    uint64_t v_hi = v >> 32;

    uint64_t w0 = u_lo * v_lo;
    uint64_t w1 = u_hi * v_lo;
    uint64_t w2 = u_lo * v_hi;
    uint64_t w3 = u_hi * v_hi;

    uint64_t w1_lo = w1 & 0xFFFFFFFFULL;
    uint64_t w1_hi = w1 >> 32;
    uint64_t w2_lo = w2 & 0xFFFFFFFFULL;
    uint64_t w2_hi = w2 >> 32;

    uint64_t low_part = w0 + (w1_lo << 32);
    uint64_t carry = (low_part < w0);
    uint64_t low = low_part + (w2_lo << 32);
    carry += (low < low_part);

    uint64_t high = w3 + w1_hi + w2_hi + carry;
    return {low, high};
}

constexpr auto divmod128(msvc_uint128 dividend, msvc_uint128 divisor, msvc_uint128& remainder) noexcept -> msvc_uint128 {
    if (divisor.low == 0 && divisor.high == 0) {
        remainder = {0, 0};
        return {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL};
    }
    if (dividend < divisor) {
        remainder = dividend;
        return {0, 0};
    }
    if (dividend == divisor) {
        remainder = {0, 0};
        return {1, 0};
    }

    msvc_uint128 quotient = {0, 0};
    remainder = {0, 0};

    for (int i = 127; i >= 0; --i) {
        remainder = (remainder << 1) | ((dividend >> i) & 1ULL);
        if (remainder >= divisor) {
            remainder = remainder - divisor;
            quotient = quotient | (msvc_uint128{1, 0} << i);
        }
    }
    return quotient;
}

struct alignas(16) msvc_float128 {
    double head{0.0};
    double tail{0.0};

    constexpr msvc_float128() noexcept = default;
    constexpr msvc_float128(double h) noexcept : head(h), tail(0.0) {}
    constexpr msvc_float128(double h, double t) noexcept : head(h), tail(t) {}
    
    constexpr explicit msvc_float128(float v) noexcept : head(v), tail(0.0) {}
    constexpr explicit msvc_float128(long double v) noexcept : head(static_cast<double>(v)), tail(0.0) {}

    constexpr explicit operator double() const noexcept { return head; }
    constexpr explicit operator float() const noexcept { return static_cast<float>(head); }
    constexpr explicit operator long double() const noexcept { return static_cast<long double>(head); }
    constexpr explicit operator int64_t() const noexcept { return static_cast<int64_t>(head); }
    constexpr explicit operator uint64_t() const noexcept { return static_cast<uint64_t>(head); }
    constexpr explicit msvc_float128(msvc_int128 v) noexcept : head(static_cast<double>(v)), tail(0.0) {}
    constexpr explicit msvc_float128(msvc_uint128 v) noexcept : head(static_cast<double>(v)), tail(0.0) {}

    constexpr auto operator+=(msvc_float128 other) noexcept -> msvc_float128&;
    constexpr auto operator-=(msvc_float128 other) noexcept -> msvc_float128&;
    constexpr auto operator*=(msvc_float128 other) noexcept -> msvc_float128&;
    constexpr auto operator/=(msvc_float128 other) noexcept -> msvc_float128&;
};

constexpr auto msvc_two_sum(double a, double b, double& err) noexcept -> double {
    double s = a + b;
    double bb = s - a;
    err = (a - (s - bb)) + (b - bb);
    return s;
}

constexpr auto msvc_split(double a, double& hi, double& lo) noexcept -> void {
    double c = 134217729.0 * a;
    double ab = c - a;
    hi = c - ab;
    lo = a - hi;
}

constexpr auto msvc_two_prod(double a, double b, double& err) noexcept -> double {
    double p = a * b;
    double a_hi, a_lo;
    msvc_split(a, a_hi, a_lo);
    double b_hi, b_lo;
    msvc_split(b, b_hi, b_lo);
    err = ((a_hi * b_hi - p) + a_hi * b_lo + a_lo * b_hi) + a_lo * b_lo;
    return p;
}

constexpr auto operator+(msvc_float128 a, msvc_float128 b) noexcept -> msvc_float128 {
    double s1, s2, t1, t2;
    s1 = msvc_two_sum(a.head, b.head, t1);
    s2 = msvc_two_sum(a.tail, b.tail, t2);
    s2 += t1;
    s1 = msvc_two_sum(s1, s2, t1);
    s2 = t1 + t2;
    double head, tail;
    head = msvc_two_sum(s1, s2, tail);
    return {head, tail};
}

constexpr auto operator-(msvc_float128 a) noexcept -> msvc_float128 {
    return {-a.head, -a.tail};
}

constexpr auto operator-(msvc_float128 a, msvc_float128 b) noexcept -> msvc_float128 {
    return a + (-b);
}

constexpr auto operator*(msvc_float128 a, msvc_float128 b) noexcept -> msvc_float128 {
    double p1, p2;
    p1 = msvc_two_prod(a.head, b.head, p2);
    p2 += a.head * b.tail + a.tail * b.head;
    double head, tail;
    head = msvc_two_sum(p1, p2, tail);
    return {head, tail};
}

constexpr auto operator/(msvc_float128 a, msvc_float128 b) noexcept -> msvc_float128 {
    double q1 = a.head / b.head;
    msvc_float128 q1_b = msvc_float128(q1) * b;
    msvc_float128 r = a - q1_b;
    double q2 = r.head / b.head;
    double head, tail;
    head = msvc_two_sum(q1, q2, tail);
    return {head, tail};
}

constexpr auto operator==(msvc_float128 a, msvc_float128 b) noexcept -> bool {
    return a.head == b.head && a.tail == b.tail;
}
constexpr auto operator!=(msvc_float128 a, msvc_float128 b) noexcept -> bool {
    return !(a == b);
}
constexpr auto operator<(msvc_float128 a, msvc_float128 b) noexcept -> bool {
    if (a.head != b.head) return a.head < b.head;
    return a.tail < b.tail;
}
constexpr auto operator>(msvc_float128 a, msvc_float128 b) noexcept -> bool { return b < a; }
constexpr auto operator<=(msvc_float128 a, msvc_float128 b) noexcept -> bool { return !(b < a); }
constexpr auto operator>=(msvc_float128 a, msvc_float128 b) noexcept -> bool { return !(a < b); }

constexpr auto msvc_float128::operator+=(msvc_float128 other) noexcept -> msvc_float128& { *this = *this + other; return *this; }
constexpr auto msvc_float128::operator-=(msvc_float128 other) noexcept -> msvc_float128& { *this = *this - other; return *this; }
constexpr auto msvc_float128::operator*=(msvc_float128 other) noexcept -> msvc_float128& { *this = *this * other; return *this; }
constexpr auto msvc_float128::operator/=(msvc_float128 other) noexcept -> msvc_float128& { *this = *this / other; return *this; }

// Out-of-line cross-conversion definitions (msvc_float128 now complete)
constexpr msvc_uint128::msvc_uint128(msvc_float128 v) noexcept : low(v.head >= 1.0 ? static_cast<uint64_t>(v.head) : 0ULL), high(0ULL) {}
constexpr msvc_int128::msvc_int128(msvc_float128 v) noexcept : low(static_cast<uint64_t>(static_cast<int64_t>(v.head))), high(v.head < 0.0 ? int64_t{-1} : int64_t{0}) {}
constexpr msvc_uint128::operator msvc_float128() const noexcept { return msvc_float128{static_cast<double>(*this)}; }
constexpr msvc_int128::operator msvc_float128() const noexcept { return msvc_float128{static_cast<double>(*this)}; }

using float128_compat = msvc_float128;
using int128_compat = msvc_int128;
using uint128_compat = msvc_uint128;
#elif defined(_WIN32)
struct alignas(16) msvc_float128 {
    double head{0.0};
    double tail{0.0};

    constexpr msvc_float128() noexcept = default;
    constexpr msvc_float128(double h) noexcept : head(h), tail(0.0) {}
    constexpr msvc_float128(double h, double t) noexcept : head(h), tail(t) {}
    
    constexpr explicit msvc_float128(float v) noexcept : head(v), tail(0.0) {}
    constexpr explicit msvc_float128(long double v) noexcept : head(static_cast<double>(v)), tail(0.0) {}
    
    constexpr explicit msvc_float128(int64_t v) noexcept : head(static_cast<double>(v)), tail(0.0) {}
    constexpr explicit msvc_float128(uint64_t v) noexcept : head(static_cast<double>(v)), tail(0.0) {}
    #if defined(__SIZEOF_INT128__) || defined(__clang__)
    constexpr explicit msvc_float128(__int128 v) noexcept : head(static_cast<double>(v)), tail(0.0) {}
    constexpr explicit msvc_float128(unsigned __int128 v) noexcept : head(static_cast<double>(v)), tail(0.0) {}
    #endif

    constexpr explicit operator double() const noexcept { return head; }
    constexpr explicit operator float() const noexcept { return static_cast<float>(head); }
    constexpr explicit operator long double() const noexcept { return static_cast<long double>(head); }
    
    constexpr explicit operator int64_t() const noexcept { return static_cast<int64_t>(head); }
    constexpr explicit operator uint64_t() const noexcept { return static_cast<uint64_t>(head); }
    #if defined(__SIZEOF_INT128__) || defined(__clang__)
    constexpr explicit operator __int128() const noexcept { return static_cast<__int128>(head); }
    constexpr explicit operator unsigned __int128() const noexcept { return static_cast<unsigned __int128>(head); }
    #endif

    constexpr auto operator+=(msvc_float128 other) noexcept -> msvc_float128&;
    constexpr auto operator-=(msvc_float128 other) noexcept -> msvc_float128&;
    constexpr auto operator*=(msvc_float128 other) noexcept -> msvc_float128&;
    constexpr auto operator/=(msvc_float128 other) noexcept -> msvc_float128&;
};

constexpr auto msvc_two_sum(double a, double b, double& err) noexcept -> double {
    double s = a + b;
    double bb = s - a;
    err = (a - (s - bb)) + (b - bb);
    return s;
}

constexpr auto msvc_split(double a, double& hi, double& lo) noexcept -> void {
    double c = 134217729.0 * a;
    double ab = c - a;
    hi = c - ab;
    lo = a - hi;
}

constexpr auto msvc_two_prod(double a, double b, double& err) noexcept -> double {
    double p = a * b;
    double a_hi, a_lo;
    msvc_split(a, a_hi, a_lo);
    double b_hi, b_lo;
    msvc_split(b, b_hi, b_lo);
    err = ((a_hi * b_hi - p) + a_hi * b_lo + a_lo * b_hi) + a_lo * b_lo;
    return p;
}

constexpr auto operator+(msvc_float128 a, msvc_float128 b) noexcept -> msvc_float128 {
    double s1, s2, t1, t2;
    s1 = msvc_two_sum(a.head, b.head, t1);
    s2 = msvc_two_sum(a.tail, b.tail, t2);
    s2 += t1;
    s1 = msvc_two_sum(s1, s2, t1);
    s2 = t1 + t2;
    double head, tail;
    head = msvc_two_sum(s1, s2, tail);
    return {head, tail};
}

constexpr auto operator-(msvc_float128 a) noexcept -> msvc_float128 {
    return {-a.head, -a.tail};
}

constexpr auto operator-(msvc_float128 a, msvc_float128 b) noexcept -> msvc_float128 {
    return a + (-b);
}

constexpr auto operator*(msvc_float128 a, msvc_float128 b) noexcept -> msvc_float128 {
    double p1, p2;
    p1 = msvc_two_prod(a.head, b.head, p2);
    p2 += a.head * b.tail + a.tail * b.head;
    double head, tail;
    head = msvc_two_sum(p1, p2, tail);
    return {head, tail};
}

constexpr auto operator/(msvc_float128 a, msvc_float128 b) noexcept -> msvc_float128 {
    double q1 = a.head / b.head;
    msvc_float128 q1_b = msvc_float128(q1) * b;
    msvc_float128 r = a - q1_b;
    double q2 = r.head / b.head;
    double head, tail;
    head = msvc_two_sum(q1, q2, tail);
    return {head, tail};
}

constexpr auto operator==(msvc_float128 a, msvc_float128 b) noexcept -> bool {
    return a.head == b.head && a.tail == b.tail;
}
constexpr auto operator!=(msvc_float128 a, msvc_float128 b) noexcept -> bool {
    return !(a == b);
}
constexpr auto operator<(msvc_float128 a, msvc_float128 b) noexcept -> bool {
    if (a.head != b.head) return a.head < b.head;
    return a.tail < b.tail;
}
constexpr auto operator>(msvc_float128 a, msvc_float128 b) noexcept -> bool { return b < a; }
constexpr auto operator<=(msvc_float128 a, msvc_float128 b) noexcept -> bool { return !(b < a); }
constexpr auto operator>=(msvc_float128 a, msvc_float128 b) noexcept -> bool { return !(a < b); }

constexpr auto msvc_float128::operator+=(msvc_float128 other) noexcept -> msvc_float128& { *this = *this + other; return *this; }
constexpr auto msvc_float128::operator-=(msvc_float128 other) noexcept -> msvc_float128& { *this = *this - other; return *this; }
constexpr auto msvc_float128::operator*=(msvc_float128 other) noexcept -> msvc_float128& { *this = *this * other; return *this; }
constexpr auto msvc_float128::operator/=(msvc_float128 other) noexcept -> msvc_float128& { *this = *this / other; return *this; }

using float128_compat = msvc_float128;
using int128_compat = __int128;
using uint128_compat = unsigned __int128;
#else
using float128_compat = __float128;
using int128_compat = __int128;
using uint128_compat = unsigned __int128;
#endif


// SIMD intrinsics MUST be in global module fragment to avoid declaration conflicts
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <array>
#include <type_traits>
#if defined(__x86_64__) || defined(_M_X64)
    #if defined(_MSC_VER) && !defined(__clang__)
        // Native MSVC (cl.exe) has no <cpuid.h>/<x86intrin.h>; the CPUID
        // intrinsic and the SSE/AVX intrinsics live in <intrin.h>/<immintrin.h>.
        // MSVC's __cpuid/__cpuidex are 2-/3-argument intrinsics writing into an
        // int[4] array, NOT the GCC/clang 5-argument <cpuid.h> macros. Provide
        // GCC-compatible __cpuid/__cpuid_count wrappers (defined here, before the
        // shadowing macros) so detect_simd_capabilities() — written against the
        // GCC signature — compiles unchanged under cl.exe (was C2660).
        #include <intrin.h>
        #include <immintrin.h>
        namespace fastjson_detail_cpuid {
            inline void cpuid_msvc(unsigned leaf, unsigned& a, unsigned& b,
                                   unsigned& c, unsigned& d) noexcept {
                int regs[4];
                __cpuid(regs, static_cast<int>(leaf));  // real MSVC intrinsic (pre-macro)
                a = static_cast<unsigned>(regs[0]);
                b = static_cast<unsigned>(regs[1]);
                c = static_cast<unsigned>(regs[2]);
                d = static_cast<unsigned>(regs[3]);
            }
            inline void cpuidex_msvc(unsigned leaf, unsigned subleaf, unsigned& a,
                                     unsigned& b, unsigned& c, unsigned& d) noexcept {
                int regs[4];
                __cpuidex(regs, static_cast<int>(leaf), static_cast<int>(subleaf));
                a = static_cast<unsigned>(regs[0]);
                b = static_cast<unsigned>(regs[1]);
                c = static_cast<unsigned>(regs[2]);
                d = static_cast<unsigned>(regs[3]);
            }
        }
        // Shadow the GCC <cpuid.h> macro spellings with the wrapper calls. Defined
        // AFTER the wrappers so their bodies bind the genuine cl.exe intrinsics.
        #define __cpuid(leaf, a, b, c, d) \
            ::fastjson_detail_cpuid::cpuid_msvc((leaf), (a), (b), (c), (d))
        #define __cpuid_count(leaf, sub, a, b, c, d) \
            ::fastjson_detail_cpuid::cpuidex_msvc((leaf), (sub), (a), (b), (c), (d))
    #else
        #include <cpuid.h>
        #include <immintrin.h>
        #include <x86intrin.h>
    #endif
#endif

#ifdef __ARM_NEON
    #include <arm_neon.h>
#endif

#ifdef _OPENMP
    #include <omp.h>
#endif

#ifdef FASTJSON_USE_PARALLEL_STL
    #include <tbb/scalable_allocator.h>
#endif

#include "gpu/json_gpu.h"

// C++ headers needed for module purview
#include <sstream>

// Structural indexing for ondemand parsing (SIMD tape builder)
#include "fastjson_simd_index.h"

// \uXXXX / surrogate-pair decoding and UTF-8 encoding: the ONE implementation (fastjson_parallel uses it too).
#include "unicode.h"

// ============================================================================
// SIMD Implementations in Global Module Fragment
// All functions using SIMD intrinsic types (__m256i, __m512i, __m128i, etc.)
// MUST live here to avoid Clang 21 TemplateInstantiator segfault in module purview.
// The module purview exports thin wrappers that delegate to these detail:: functions.
// ============================================================================

namespace fastjson::detail {

#ifdef FASTJSON_ENABLE_SIMD

#if defined(__x86_64__) || defined(_M_X64)

// SIMD capability flags
static constexpr uint32_t SIMD_SSE2        = 0x002;
static constexpr uint32_t SIMD_SSE3        = 0x004;
static constexpr uint32_t SIMD_SSSE3       = 0x008;
static constexpr uint32_t SIMD_SSE41       = 0x010;
static constexpr uint32_t SIMD_SSE42       = 0x020;
static constexpr uint32_t SIMD_AVX         = 0x040;
static constexpr uint32_t SIMD_AVX2        = 0x080;
static constexpr uint32_t SIMD_AVX512F     = 0x100;
static constexpr uint32_t SIMD_AVX512BW    = 0x200;
static constexpr uint32_t SIMD_AVX512VBMI  = 0x400;
static constexpr uint32_t SIMD_AVX512VBMI2 = 0x800;
static constexpr uint32_t SIMD_AVX512VNNI  = 0x1000;
static constexpr uint32_t SIMD_AMX_TILE    = 0x2000;
static constexpr uint32_t SIMD_AMX_INT8    = 0x4000;

// XCR0 (XGETBV with ECX = 0): which register states the OPERATING SYSTEM saves on a context switch.
inline auto read_xcr0() noexcept -> uint64_t {
#if defined(_MSC_VER) && !defined(__clang__)
    return _xgetbv(0);
#else
    uint32_t lo = 0, hi = 0;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#endif
}

// Thread-safe SIMD capability detection. ONE atomic carries both the result and a READY bit: the previous
// `static bool initialized` was a plain flag read and written by every thread (a data race).
inline auto detect_simd_capabilities() noexcept -> uint32_t {
    static constexpr uint32_t kReady = 0x80000000U;
    static std::atomic<uint32_t> cached_caps{0};
    if (const uint32_t v = cached_caps.load(std::memory_order_acquire); (v & kReady) != 0) {
        return v & ~kReady;
    }

    uint32_t caps = 0;
    bool os_saves_ymm = false;   // XCR0 bits 1-2: SSE + AVX state
    bool os_saves_zmm = false;   // XCR0 bits 5-7: opmask, ZMM_Hi256, Hi16_ZMM
    bool os_saves_tiles = false; // XCR0 bits 17-18: AMX TILECFG + TILEDATA
    uint32_t eax, ebx, ecx, edx;

    __cpuid(0, eax, ebx, ecx, edx);
    if (eax >= 1) {
        __cpuid(1, eax, ebx, ecx, edx);
        if (edx & (1 << 26)) caps |= SIMD_SSE2;
        if (ecx & (1 << 0))  caps |= SIMD_SSE3;
        if (ecx & (1 << 9))  caps |= SIMD_SSSE3;
        if (ecx & (1 << 19)) caps |= SIMD_SSE41;
        if (ecx & (1 << 20)) caps |= SIMD_SSE42;
        if (ecx & (1 << 28)) caps |= SIMD_AVX;
        // A CPU that HAS AVX/AVX-512 but whose OS does not save that state (a VM or kernel that masks it)
        // faults on the first such instruction: CPUID alone is not permission. OSXSAVE (bit 27) says XGETBV
        // may be executed at all.
        if (ecx & (1 << 27)) {
            const uint64_t xcr0 = read_xcr0();
            os_saves_ymm = (xcr0 & 0x6) == 0x6;
            os_saves_zmm = (xcr0 & 0xE6) == 0xE6;
            os_saves_tiles = (xcr0 & 0x60000) == 0x60000;
        }
    }

    __cpuid(0, eax, ebx, ecx, edx);
    if (eax >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        if (ebx & (1 << 5))  caps |= SIMD_AVX2;
        if (ebx & (1 << 16)) caps |= SIMD_AVX512F;
        if (ebx & (1 << 30)) caps |= SIMD_AVX512BW;
        if (ecx & (1 << 1))  caps |= SIMD_AVX512VBMI;
        if (ecx & (1 << 6))  caps |= SIMD_AVX512VBMI2;
        if (ecx & (1 << 11)) caps |= SIMD_AVX512VNNI;
        if (edx & (1 << 24)) caps |= SIMD_AMX_TILE;
        if (edx & (1 << 25)) caps |= SIMD_AMX_INT8;
    }

    if (!os_saves_ymm) caps &= ~(SIMD_AVX | SIMD_AVX2);
    if (!os_saves_zmm) caps &= ~(SIMD_AVX512F | SIMD_AVX512BW | SIMD_AVX512VBMI | SIMD_AVX512VBMI2 | SIMD_AVX512VNNI);
    if (!os_saves_tiles) caps &= ~(SIMD_AMX_TILE | SIMD_AMX_INT8);

    // FASTJSON_SIMD caps the tier, so ONE host can exercise every dispatch path (tests, A/B timing):
    // `scalar` (no vector tier), `avx2` (no AVX-512), unset or anything else = the best the CPU and OS allow.
    // It can only LOWER the tier: it never claims a feature the hardware lacks.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)  // getenv: one read-only lookup of a tier name; _dupenv_s would allocate
#endif
    const char* const cap = std::getenv("FASTJSON_SIMD");
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    if (cap != nullptr) {
        if (std::strcmp(cap, "scalar") == 0) {
            caps = 0;
        } else if (std::strcmp(cap, "avx2") == 0) {
            caps &= ~(SIMD_AVX512F | SIMD_AVX512BW | SIMD_AVX512VBMI | SIMD_AVX512VBMI2 | SIMD_AVX512VNNI | SIMD_AMX_TILE |
                      SIMD_AMX_INT8);
        }
    }

    cached_caps.store(caps | kReady, std::memory_order_release);
    return caps;
}

// --------------------------------------------------------------------------
// AVX-512 tier: COMPILED IN WHEREVER THE COMPILER CAN TARGET IT, chosen at RUN TIME by CPUID.
// The build baseline is x86-64-v3 (no -mavx512f, so a binary built here runs on an AVX2-only CPU), which
// left HAVE_AVX512F undefined and these kernels compiled OUT: an AVX-512 host ran the AVX2 path. A
// per-function target attribute needs no global flag, and the dispatcher only calls the kernel when CPUID
// reports AVX-512F+BW. cl.exe has no target attribute and takes the scalar path, as before.
// No lambdas inside: a lambda does not inherit its enclosing function's target attribute, so an AVX-512
// intrinsic in one fails to compile under the x86-64-v3 baseline.
// --------------------------------------------------------------------------
#if (defined(__clang__) || defined(__GNUC__)) && (defined(__x86_64__) || defined(_M_X64))
#define FASTJSON_TARGET_AVX512 1
#endif

// --------------------------------------------------------------------------
// AVX-512 Whitespace Skip — 4x zmm registers (256 bytes per iteration)
// --------------------------------------------------------------------------
#ifdef FASTJSON_TARGET_AVX512
__attribute__((target("avx512f,avx512bw")))
inline auto ws_mask_avx512(__m512i chunk) -> __mmask64 {
    return _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8(' ')) | _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\t')) |
           _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\n')) | _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\r'));
}

__attribute__((target("avx512f,avx512bw")))
inline auto skip_whitespace_avx512(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;

    // 4x zmm multi-register: 256 bytes per iteration
    while (end - ptr >= 256) {
        __m512i c0 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
        __m512i c1 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 64));
        __m512i c2 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 128));
        __m512i c3 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 192));

        __mmask64 ws0 = ws_mask_avx512(c0);
        if (ws0 != 0xFFFFFFFFFFFFFFFF) return ptr + __builtin_ctzll(~ws0);

        __mmask64 ws1 = ws_mask_avx512(c1);
        if (ws1 != 0xFFFFFFFFFFFFFFFF) return ptr + 64 + __builtin_ctzll(~ws1);

        __mmask64 ws2 = ws_mask_avx512(c2);
        if (ws2 != 0xFFFFFFFFFFFFFFFF) return ptr + 128 + __builtin_ctzll(~ws2);

        __mmask64 ws3 = ws_mask_avx512(c3);
        if (ws3 != 0xFFFFFFFFFFFFFFFF) return ptr + 192 + __builtin_ctzll(~ws3);

        ptr += 256;
    }

    // Single zmm tail loop
    while (end - ptr >= 64) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
        __mmask64 ws = ws_mask_avx512(chunk);
        if (ws != 0xFFFFFFFFFFFFFFFF) return ptr + __builtin_ctzll(~ws);
        ptr += 64;
    }

    // Scalar tail
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}
#endif // FASTJSON_TARGET_AVX512

// --------------------------------------------------------------------------
// AVX2 Whitespace Skip — 8x ymm registers (256 bytes per iteration)
// --------------------------------------------------------------------------
#ifdef HAVE_AVX2
__attribute__((target("avx2")))
inline auto skip_whitespace_avx2(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;

    const __m256i ws_space = _mm256_set1_epi8(' ');
    const __m256i ws_tab   = _mm256_set1_epi8('\t');
    const __m256i ws_nl    = _mm256_set1_epi8('\n');
    const __m256i ws_cr    = _mm256_set1_epi8('\r');

    auto check_ws = [&](__m256i chunk) -> __m256i {
        __m256i m0 = _mm256_cmpeq_epi8(chunk, ws_space);
        __m256i m1 = _mm256_cmpeq_epi8(chunk, ws_tab);
        __m256i m2 = _mm256_cmpeq_epi8(chunk, ws_nl);
        __m256i m3 = _mm256_cmpeq_epi8(chunk, ws_cr);
        return _mm256_or_si256(_mm256_or_si256(m0, m1), _mm256_or_si256(m2, m3));
    };

    // 8x ymm multi-register: 256 bytes per iteration
    while (end - ptr >= 256) {
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 32));
        __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 64));
        __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 96));
        __m256i c4 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 128));
        __m256i c5 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 160));
        __m256i c6 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 192));
        __m256i c7 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 224));

        uint32_t m0 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c0)));
        if (m0) return ptr + __builtin_ctz(m0);
        uint32_t m1 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c1)));
        if (m1) return ptr + 32 + __builtin_ctz(m1);
        uint32_t m2 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c2)));
        if (m2) return ptr + 64 + __builtin_ctz(m2);
        uint32_t m3 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c3)));
        if (m3) return ptr + 96 + __builtin_ctz(m3);
        uint32_t m4 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c4)));
        if (m4) return ptr + 128 + __builtin_ctz(m4);
        uint32_t m5 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c5)));
        if (m5) return ptr + 160 + __builtin_ctz(m5);
        uint32_t m6 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c6)));
        if (m6) return ptr + 192 + __builtin_ctz(m6);
        uint32_t m7 = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(c7)));
        if (m7) return ptr + 224 + __builtin_ctz(m7);

        ptr += 256;
    }

    // Single ymm tail loop
    while (end - ptr >= 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        uint32_t mask = ~static_cast<uint32_t>(_mm256_movemask_epi8(check_ws(chunk)));
        if (mask) return ptr + __builtin_ctz(mask);
        ptr += 32;
    }

    // Scalar tail
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}
#endif // HAVE_AVX2

// --------------------------------------------------------------------------
// SSE4.2 Whitespace Skip — 4x xmm registers (64 bytes per iteration)
// --------------------------------------------------------------------------
#ifdef HAVE_SSE42
__attribute__((target("sse4.2")))
inline auto skip_whitespace_sse42(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;

    const __m128i whitespace_chars =
        _mm_setr_epi8(' ', '\t', '\n', '\r', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    // 4x xmm multi-register: 64 bytes per iteration
    while (end - ptr >= 64) {
        __m128i c0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        __m128i c1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 16));
        __m128i c2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 32));
        __m128i c3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 48));

        int r0 = _mm_cmpestri(whitespace_chars, 4, c0, 16,
                               _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
        if (r0 < 16) return ptr + r0;

        int r1 = _mm_cmpestri(whitespace_chars, 4, c1, 16,
                               _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
        if (r1 < 16) return ptr + 16 + r1;

        int r2 = _mm_cmpestri(whitespace_chars, 4, c2, 16,
                               _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
        if (r2 < 16) return ptr + 32 + r2;

        int r3 = _mm_cmpestri(whitespace_chars, 4, c3, 16,
                               _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
        if (r3 < 16) return ptr + 48 + r3;

        ptr += 64;
    }

    // Single xmm tail loop
    while (end - ptr >= 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        int result = _mm_cmpestri(whitespace_chars, 4, chunk, 16,
                                   _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
        if (result < 16) return ptr + result;
        ptr += 16;
    }

    // Scalar tail
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}
#endif // HAVE_SSE42

// --------------------------------------------------------------------------
// SSE2 Whitespace Skip — 4x xmm registers (64 bytes per iteration)
// --------------------------------------------------------------------------
#ifdef HAVE_SSE2
__attribute__((target("sse2")))
inline auto skip_whitespace_sse2(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;

    const __m128i ws_space = _mm_set1_epi8(' ');
    const __m128i ws_tab   = _mm_set1_epi8('\t');
    const __m128i ws_nl    = _mm_set1_epi8('\n');
    const __m128i ws_cr    = _mm_set1_epi8('\r');

    auto check_ws = [&](__m128i chunk) -> __m128i {
        __m128i m0 = _mm_cmpeq_epi8(chunk, ws_space);
        __m128i m1 = _mm_cmpeq_epi8(chunk, ws_tab);
        __m128i m2 = _mm_cmpeq_epi8(chunk, ws_nl);
        __m128i m3 = _mm_cmpeq_epi8(chunk, ws_cr);
        return _mm_or_si128(_mm_or_si128(m0, m1), _mm_or_si128(m2, m3));
    };

    // 4x xmm multi-register: 64 bytes per iteration
    while (end - ptr >= 64) {
        __m128i c0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        __m128i c1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 16));
        __m128i c2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 32));
        __m128i c3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 48));

        uint32_t m0 = ~_mm_movemask_epi8(check_ws(c0)) & 0xFFFF;
        if (m0) return ptr + __builtin_ctz(m0);
        uint32_t m1 = ~_mm_movemask_epi8(check_ws(c1)) & 0xFFFF;
        if (m1) return ptr + 16 + __builtin_ctz(m1);
        uint32_t m2 = ~_mm_movemask_epi8(check_ws(c2)) & 0xFFFF;
        if (m2) return ptr + 32 + __builtin_ctz(m2);
        uint32_t m3 = ~_mm_movemask_epi8(check_ws(c3)) & 0xFFFF;
        if (m3) return ptr + 48 + __builtin_ctz(m3);

        ptr += 64;
    }

    // Single xmm tail loop
    while (end - ptr >= 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        uint32_t mask = ~_mm_movemask_epi8(check_ws(chunk)) & 0xFFFF;
        if (mask) return ptr + __builtin_ctz(mask);
        ptr += 16;
    }

    // Scalar tail
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}
#endif // HAVE_SSE2

// --------------------------------------------------------------------------
// Runtime SIMD Dispatcher for Whitespace Skipping
// --------------------------------------------------------------------------
inline auto skip_whitespace_simd_impl(const char* data, size_t size) -> const char* {
    static const uint32_t caps = detect_simd_capabilities();

#ifdef FASTJSON_TARGET_AVX512
    if ((caps & SIMD_AVX512F) && (caps & SIMD_AVX512BW))
        return skip_whitespace_avx512(data, size);
#endif
#ifdef HAVE_AVX2
    if (caps & SIMD_AVX2)
        return skip_whitespace_avx2(data, size);
#endif
#ifdef HAVE_SSE42
    if (caps & SIMD_SSE42)
        return skip_whitespace_sse42(data, size);
#endif
#ifdef HAVE_SSE2
    if (caps & SIMD_SSE2)
        return skip_whitespace_sse2(data, size);
#endif

    // Scalar fallback
    const char* ptr = data;
    const char* end = data + size;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}

// --------------------------------------------------------------------------
// AVX2 String End Detection — 8x ymm registers (256 bytes per iteration)
// Finds the first quote, backslash, or control char in [start, end).
// --------------------------------------------------------------------------
#ifdef HAVE_AVX2
__attribute__((target("avx2")))
inline auto find_string_end_avx2(const char* start, const char* end) -> const char* {
    const char* ptr = start;

    const __m256i quote     = _mm256_set1_epi8('"');
    const __m256i backslash = _mm256_set1_epi8('\\');
    // Unsigned "byte < 0x20" with SIGNED compares: flip the sign bit of BOTH sides. The limit used to stay
    // 0x20 unflipped, so (b ^ 0x80) < 0x20 held for every printable byte (0x61 'a' -> 0xE1 = -31 < 32):
    // the scan stopped at the first character of every string and parse_string_simd's fast path never ran.
    const __m256i ctrl_limit = _mm256_set1_epi8(static_cast<char>(0x20 ^ 0x80));
    const __m256i sign_flip = _mm256_set1_epi8(static_cast<char>(0x80));

    auto check_special = [&](__m256i chunk) -> uint32_t {
        __m256i q  = _mm256_cmpeq_epi8(chunk, quote);
        __m256i bs = _mm256_cmpeq_epi8(chunk, backslash);
        // Control chars: unsigned < 0x20, use signed comparison with XOR trick
        __m256i ctrl = _mm256_cmpgt_epi8(ctrl_limit,
                       _mm256_xor_si256(chunk, sign_flip));
        return static_cast<uint32_t>(_mm256_movemask_epi8(
            _mm256_or_si256(_mm256_or_si256(q, bs), ctrl)));
    };

    // 8x ymm multi-register: 256 bytes per iteration
    while (end - ptr >= 256) {
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 32));
        __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 64));
        __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 96));
        __m256i c4 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 128));
        __m256i c5 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 160));
        __m256i c6 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 192));
        __m256i c7 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 224));

        uint32_t m0 = check_special(c0); if (m0) return ptr + __builtin_ctz(m0);
        uint32_t m1 = check_special(c1); if (m1) return ptr + 32 + __builtin_ctz(m1);
        uint32_t m2 = check_special(c2); if (m2) return ptr + 64 + __builtin_ctz(m2);
        uint32_t m3 = check_special(c3); if (m3) return ptr + 96 + __builtin_ctz(m3);
        uint32_t m4 = check_special(c4); if (m4) return ptr + 128 + __builtin_ctz(m4);
        uint32_t m5 = check_special(c5); if (m5) return ptr + 160 + __builtin_ctz(m5);
        uint32_t m6 = check_special(c6); if (m6) return ptr + 192 + __builtin_ctz(m6);
        uint32_t m7 = check_special(c7); if (m7) return ptr + 224 + __builtin_ctz(m7);

        ptr += 256;
    }

    // Single ymm tail loop
    while (end - ptr >= 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        uint32_t mask = check_special(chunk);
        if (mask) return ptr + __builtin_ctz(mask);
        ptr += 32;
    }

    // Scalar tail
    while (ptr < end) {
        if (*ptr == '"' || *ptr == '\\' || static_cast<unsigned char>(*ptr) < 0x20)
            return ptr;
        ++ptr;
    }
    return end;
}
#endif // HAVE_AVX2

// --------------------------------------------------------------------------
// AVX-512 String End Detection — 8x zmm registers (512 bytes per iteration)
// First '"', '\\' or control byte (< 0x20). The control test is UNSIGNED (_mm512_cmplt_epu8_mask):
// a signed compare reads every byte >= 0x80 -- each UTF-8 lead/continuation byte -- as negative and
// therefore "< 0x20", which stopped the scan at the first non-ASCII character.
// --------------------------------------------------------------------------
#ifdef FASTJSON_TARGET_AVX512
__attribute__((target("avx512f,avx512bw")))
inline auto string_special_mask_avx512(__m512i chunk) -> __mmask64 {
    return _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('"')) | _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('\\')) |
           _mm512_cmplt_epu8_mask(chunk, _mm512_set1_epi8(0x20));
}

__attribute__((target("avx512f,avx512bw")))
inline auto find_string_end_avx512(const char* start, const char* end) -> const char* {
    const char* ptr = start;
    while (end - ptr >= 512) {
        const __m512i c0 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
        const __m512i c1 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 64));
        const __m512i c2 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 128));
        const __m512i c3 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 192));
        const __m512i c4 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 256));
        const __m512i c5 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 320));
        const __m512i c6 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 384));
        const __m512i c7 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 448));
        if (const __mmask64 m = string_special_mask_avx512(c0)) return ptr + __builtin_ctzll(m);
        if (const __mmask64 m = string_special_mask_avx512(c1)) return ptr + 64 + __builtin_ctzll(m);
        if (const __mmask64 m = string_special_mask_avx512(c2)) return ptr + 128 + __builtin_ctzll(m);
        if (const __mmask64 m = string_special_mask_avx512(c3)) return ptr + 192 + __builtin_ctzll(m);
        if (const __mmask64 m = string_special_mask_avx512(c4)) return ptr + 256 + __builtin_ctzll(m);
        if (const __mmask64 m = string_special_mask_avx512(c5)) return ptr + 320 + __builtin_ctzll(m);
        if (const __mmask64 m = string_special_mask_avx512(c6)) return ptr + 384 + __builtin_ctzll(m);
        if (const __mmask64 m = string_special_mask_avx512(c7)) return ptr + 448 + __builtin_ctzll(m);
        ptr += 512;
    }
    while (end - ptr >= 64) {
        if (const __mmask64 m = string_special_mask_avx512(_mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr)))) {
            return ptr + __builtin_ctzll(m);
        }
        ptr += 64;
    }
    // The last < 64 bytes in ONE masked load: the mask stops the load at `end`, so nothing past it is read.
    if (ptr < end) {
        const __mmask64 live = (end - ptr) >= 64 ? ~__mmask64{0} : ((__mmask64{1} << (end - ptr)) - 1);
        const __m512i tail = _mm512_maskz_loadu_epi8(live, ptr);
        if (const __mmask64 m = string_special_mask_avx512(tail) & live) return ptr + __builtin_ctzll(m);
    }
    return end;
}
#endif // FASTJSON_TARGET_AVX512

// --------------------------------------------------------------------------
// String End Detection Dispatcher
// --------------------------------------------------------------------------
inline auto find_string_end_simd_impl(const char* start, const char* end) -> const char* {
    [[maybe_unused]] static const uint32_t caps = detect_simd_capabilities();

#ifdef FASTJSON_TARGET_AVX512
    if ((caps & SIMD_AVX512F) && (caps & SIMD_AVX512BW))
        return find_string_end_avx512(start, end);
#endif
#ifdef HAVE_AVX2
    if (caps & SIMD_AVX2)
        return find_string_end_avx2(start, end);
#endif

    // Scalar fallback
    const char* ptr = start;
    while (ptr < end && *ptr != '"' && *ptr != '\\' && static_cast<unsigned char>(*ptr) >= 0x20)
        ++ptr;
    return ptr;
}

// --------------------------------------------------------------------------
// First-of-a-byte-SET scan: the first byte in [start, end) equal to ANY of up to 8 needles.
// Built like the string-end kernels (8 registers per iteration, runtime dispatch, an exact masked/bounded
// tail) but with the needle count a TEMPLATE parameter: a runtime count would leave the per-chunk compare
// loop rolled, and a 2-byte set would pay for eight compares. A set of n needles is padded to the next
// kernel width (1, 2, 4, 8) by repeating its last byte, which cannot change the answer.
// Needles are compared for EQUALITY (cmpeq), which is sign-agnostic: a byte >= 0x80 matches only itself.
// No lambdas (they lose the target attribute); every loop is bounded by `end - ptr` so no pointer is ever
// formed past one-past-the-end; the AVX-512 tail is ONE masked load, the AVX2 tail a bounded byte loop.
// --------------------------------------------------------------------------
inline constexpr size_t kFindFirstOfMaxSet = 8;

template <size_t N>
inline auto find_first_of_scalar_n(const char* start, const char* end, const unsigned char* set) -> const char* {
    for (const char* ptr = start; ptr < end; ++ptr) {
        const auto b = static_cast<unsigned char>(*ptr);
        for (size_t k = 0; k < N; ++k) {
            if (b == set[k]) return ptr;
        }
    }
    return end;
}

#ifdef HAVE_AVX2
template <size_t N>
struct NeedlesAvx2 {
    std::array<__m256i, N> v;
};

template <size_t N>
__attribute__((target("avx2")))
inline auto first_of_mask_avx2(__m256i chunk, const NeedlesAvx2<N>& nd) -> uint32_t {
    __m256i hit = _mm256_cmpeq_epi8(chunk, nd.v[0]);
    for (size_t k = 1; k < N; ++k) hit = _mm256_or_si256(hit, _mm256_cmpeq_epi8(chunk, nd.v[k]));
    return static_cast<uint32_t>(_mm256_movemask_epi8(hit));
}

template <size_t N>
__attribute__((target("avx2")))
inline auto find_first_of_avx2_n(const char* start, const char* end, const unsigned char* set) -> const char* {
    NeedlesAvx2<N> nd;
    for (size_t k = 0; k < N; ++k) nd.v[k] = _mm256_set1_epi8(static_cast<char>(set[k]));
    const char* ptr = start;
    while (end - ptr >= 256) {
        for (size_t j = 0; j < 8; ++j) {
            const uint32_t m = first_of_mask_avx2<N>(
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + 32 * j)), nd);
            if (m) return ptr + 32 * j + __builtin_ctz(m);
        }
        ptr += 256;
    }
    while (end - ptr >= 32) {
        const uint32_t m = first_of_mask_avx2<N>(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr)), nd);
        if (m) return ptr + __builtin_ctz(m);
        ptr += 32;
    }
    return find_first_of_scalar_n<N>(ptr, end, set);
}
#endif // HAVE_AVX2

#ifdef FASTJSON_TARGET_AVX512
template <size_t N>
struct NeedlesAvx512 {
    std::array<__m512i, N> v;
};

template <size_t N>
__attribute__((target("avx512f,avx512bw")))
inline auto first_of_mask_avx512(__m512i chunk, const NeedlesAvx512<N>& nd) -> __mmask64 {
    __mmask64 hit = _mm512_cmpeq_epi8_mask(chunk, nd.v[0]);
    for (size_t k = 1; k < N; ++k) hit |= _mm512_cmpeq_epi8_mask(chunk, nd.v[k]);
    return hit;
}

template <size_t N>
__attribute__((target("avx512f,avx512bw")))
inline auto find_first_of_avx512_n(const char* start, const char* end, const unsigned char* set) -> const char* {
    NeedlesAvx512<N> nd;
    for (size_t k = 0; k < N; ++k) nd.v[k] = _mm512_set1_epi8(static_cast<char>(set[k]));
    const char* ptr = start;
    while (end - ptr >= 512) {
        for (size_t j = 0; j < 8; ++j) {
            if (const __mmask64 m = first_of_mask_avx512<N>(
                    _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr + 64 * j)), nd)) {
                return ptr + 64 * j + __builtin_ctzll(m);
            }
        }
        ptr += 512;
    }
    while (end - ptr >= 64) {
        if (const __mmask64 m = first_of_mask_avx512<N>(_mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr)), nd)) {
            return ptr + __builtin_ctzll(m);
        }
        ptr += 64;
    }
    if (ptr < end) {
        const __mmask64 live = (__mmask64{1} << (end - ptr)) - 1;  // 0 < end - ptr < 64 here
        const __m512i tail = _mm512_maskz_loadu_epi8(live, ptr);
        if (const __mmask64 m = first_of_mask_avx512<N>(tail, nd) & live) return ptr + __builtin_ctzll(m);
    }
    return end;
}
#endif // FASTJSON_TARGET_AVX512

// Dispatcher. `set` holds exactly 8 bytes: the needles padded to width N by the caller (only N are read).
template <size_t N>
inline auto find_first_of_dispatch_n(const char* start, const char* end, const unsigned char* set) -> const char* {
    [[maybe_unused]] static const uint32_t caps = detect_simd_capabilities();
#ifdef FASTJSON_TARGET_AVX512
    if ((caps & SIMD_AVX512F) && (caps & SIMD_AVX512BW)) return find_first_of_avx512_n<N>(start, end, set);
#endif
#ifdef HAVE_AVX2
    if (caps & SIMD_AVX2) return find_first_of_avx2_n<N>(start, end, set);
#endif
    return find_first_of_scalar_n<N>(start, end, set);
}

inline auto find_first_of_simd_impl(const char* start, const char* end, const unsigned char* needles, size_t count)
    -> const char* {
    if (count == 0 || start >= end) return end;
    if (count > kFindFirstOfMaxSet) {
        // More than 8 needles is answered by the exact scalar scan, never truncated.
        for (const char* ptr = start; ptr < end; ++ptr) {
            const auto b = static_cast<unsigned char>(*ptr);
            for (size_t k = 0; k < count; ++k) {
                if (b == needles[k]) return ptr;
            }
        }
        return end;
    }
    std::array<unsigned char, kFindFirstOfMaxSet> padded{};
    for (size_t k = 0; k < kFindFirstOfMaxSet; ++k) padded[k] = needles[k < count ? k : count - 1];
    if (count == 1) return find_first_of_dispatch_n<1>(start, end, padded.data());
    if (count == 2) return find_first_of_dispatch_n<2>(start, end, padded.data());
    if (count <= 4) return find_first_of_dispatch_n<4>(start, end, padded.data());
    return find_first_of_dispatch_n<8>(start, end, padded.data());
}

// Which string-scan kernel the dispatcher above selects on this host: "avx512", "avx2" or "scalar". It mirrors
// the dispatcher's conditions INCLUDING what was compiled in, so a kernel compiled out shows here even when
// CPUID reports the feature (the x86-64-v3 baseline once compiled the AVX-512 tier out on AVX-512 hosts).
inline auto string_scan_tier_impl() -> const char* {
    [[maybe_unused]] static const uint32_t caps = detect_simd_capabilities();
#ifdef FASTJSON_TARGET_AVX512
    if ((caps & SIMD_AVX512F) && (caps & SIMD_AVX512BW)) return "avx512";
#endif
#ifdef HAVE_AVX2
    if (caps & SIMD_AVX2) return "avx2";
#endif
    return "scalar";
}

// --------------------------------------------------------------------------
// Serialization Escape Position Finder
// The first character that needs JSON escaping ('"', '\\', control < 0x20) is EXACTLY the parser's string-end
// predicate, so it delegates to that dispatcher (AVX-512 8x zmm -> AVX2 8x ymm -> scalar). The copies this
// replaced compared SIGNED -- every UTF-8 byte stopped the vector loop, so UTF-8 text was escaped one SIMD
// restart per byte -- and its AVX-512 branch was gated on HAVE_AVX512BW in a function with no target, i.e. dead.
// --------------------------------------------------------------------------
inline auto find_escape_position_simd_impl(const char* ptr, const char* end) -> const char* {
    return find_string_end_simd_impl(ptr, end);
}

// --------------------------------------------------------------------------
// AMX / VNNI Implementations (kept for specialized use cases)
// --------------------------------------------------------------------------
#ifdef HAVE_AMX_TILE
class amx_context {
    static thread_local bool tiles_configured;
public:
    static auto configure_tiles() noexcept -> bool {
        if (!tiles_configured) {
            struct tile_config {
                uint8_t palette_id;
                uint8_t start_row;
                uint8_t reserved[14];
                uint16_t colsb[16];
                uint8_t rows[16];
            };
            alignas(64) tile_config config = {};
            config.palette_id = 1;
            for (int i = 0; i < 8; ++i) {
                config.colsb[i] = 64;
                config.rows[i] = 16;
            }
            _tile_loadconfig(&config);
            tiles_configured = true;
        }
        return tiles_configured;
    }
    ~amx_context() {
        if (tiles_configured) {
            _tile_release();
            tiles_configured = false;
        }
    }
};

thread_local bool amx_context::tiles_configured = false;

__attribute__((target("amx-tile,amx-int8")))
inline auto classify_json_chars_amx(const char* data, size_t size, uint8_t* classifications)
    -> size_t {
    if (!amx_context::configure_tiles()) return 0;
    const size_t chunk_size = 1024;
    size_t processed = 0;
    alignas(64) uint8_t lookup_table[256] = {};
    lookup_table[' '] = 1; lookup_table['\t'] = 1;
    lookup_table['\n'] = 1; lookup_table['\r'] = 1;
    lookup_table['{'] = 2; lookup_table['}'] = 2;
    lookup_table['['] = 2; lookup_table[']'] = 2;
    lookup_table[':'] = 2; lookup_table[','] = 2;
    lookup_table['"'] = 3;
    for (size_t i = 0; i + chunk_size <= size; i += chunk_size) {
        const char* chunk = data + i;
        for (size_t j = 0; j < chunk_size; ++j)
            classifications[processed + j] = lookup_table[static_cast<uint8_t>(chunk[j])];
        processed += chunk_size;
    }
    for (size_t i = processed; i < size; ++i)
        classifications[i] = lookup_table[static_cast<uint8_t>(data[i])];
    return size;
}
#endif // HAVE_AMX_TILE

#ifdef HAVE_AVX512VNNI
__attribute__((target("avx512f,avx512vnni")))
inline auto process_json_tokens_vnni(const char* data, size_t size)
    -> std::array<uint32_t, 256> {
    std::array<uint32_t, 256> token_counts = {};
    const size_t chunk_size = 64;
    size_t processed = 0;
    for (size_t i = 0; i + chunk_size <= size; i += chunk_size) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(data + i));
        __mmask64 brace_open  = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('{'));
        __mmask64 brace_close = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('}'));
        __mmask64 brack_open  = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8('['));
        __mmask64 brack_close = _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8(']'));
        token_counts['{'] += __builtin_popcountll(brace_open);
        token_counts['}'] += __builtin_popcountll(brace_close);
        token_counts['['] += __builtin_popcountll(brack_open);
        token_counts[']'] += __builtin_popcountll(brack_close);
        processed += chunk_size;
    }
    for (size_t i = processed; i < size; ++i)
        token_counts[static_cast<uint8_t>(data[i])]++;
    return token_counts;
}
#endif // HAVE_AVX512VNNI

#endif // x86_64

#else // !FASTJSON_ENABLE_SIMD

// SIMD capability constants (still needed for API compatibility when SIMD disabled)
constexpr uint32_t SIMD_SSE2        = 0x002;
constexpr uint32_t SIMD_SSE3        = 0x004;
constexpr uint32_t SIMD_SSSE3       = 0x008;
constexpr uint32_t SIMD_SSE41       = 0x010;
constexpr uint32_t SIMD_SSE42       = 0x020;
constexpr uint32_t SIMD_AVX         = 0x040;
constexpr uint32_t SIMD_AVX2        = 0x080;
constexpr uint32_t SIMD_AVX512F     = 0x100;
constexpr uint32_t SIMD_AVX512BW    = 0x200;
constexpr uint32_t SIMD_AVX512VBMI  = 0x400;
constexpr uint32_t SIMD_AVX512VBMI2 = 0x800;
constexpr uint32_t SIMD_AVX512VNNI  = 0x1000;
constexpr uint32_t SIMD_AMX_TILE    = 0x2000;
constexpr uint32_t SIMD_AMX_INT8    = 0x4000;

// Scalar-only implementations when SIMD is disabled at compile time
inline auto detect_simd_capabilities() noexcept -> uint32_t { return 0; }

inline auto skip_whitespace_simd_impl(const char* data, size_t size) -> const char* {
    const char* ptr = data;
    const char* end = data + size;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ++ptr;
    return ptr;
}

inline auto find_string_end_simd_impl(const char* start, const char* end) -> const char* {
    const char* ptr = start;
    while (ptr < end && *ptr != '"' && *ptr != '\\' && static_cast<unsigned char>(*ptr) >= 0x20)
        ++ptr;
    return ptr;
}

inline auto string_scan_tier_impl() -> const char* { return "scalar"; }

inline auto find_first_of_simd_impl(const char* start, const char* end, const unsigned char* needles, size_t count)
    -> const char* {
    for (const char* ptr = start; ptr < end; ++ptr) {
        const auto b = static_cast<unsigned char>(*ptr);
        for (size_t k = 0; k < count; ++k) {
            if (b == needles[k]) return ptr;
        }
    }
    return end;
}

inline auto find_escape_position_simd_impl(const char* ptr, const char* end) -> const char* {
    while (ptr < end && *ptr != '"' && *ptr != '\\'
           && static_cast<unsigned char>(*ptr) >= 32)
        ++ptr;
    return ptr;
}

#endif // FASTJSON_ENABLE_SIMD

// ============================================================================
// Text helpers shared by the parser, json_value serialisation, fastjson::writer and escape_string. One
// implementation each (DRY): the escaping that to_string() applies is the escaping the streaming writer applies.
// ============================================================================

/// Length of the well-formed UTF-8 sequence starting at `p` (1..4), or 0 when it is not one (RFC 3629: no
/// overlongs, no UTF-16 surrogates, nothing above U+10FFFF, no truncated sequence).
[[nodiscard]] inline auto utf8_sequence_length(std::string_view s, size_t i) noexcept -> size_t {
    const auto b0 = static_cast<unsigned char>(s[i]);
    if (b0 < 0x80) { return 1; }
    const size_t left = s.size() - i;
    const auto cont = [&](size_t k) { return k < left && (static_cast<unsigned char>(s[i + k]) & 0xC0U) == 0x80U; };
    if (b0 >= 0xC2 && b0 <= 0xDF) { return cont(1) ? 2 : 0; }
    if (b0 >= 0xE0 && b0 <= 0xEF) {
        if (!cont(1) || !cont(2)) { return 0; }
        const auto b1 = static_cast<unsigned char>(s[i + 1]);
        if (b0 == 0xE0 && b1 < 0xA0) { return 0; }  // overlong
        if (b0 == 0xED && b1 > 0x9F) { return 0; }  // UTF-16 surrogate
        return 3;
    }
    if (b0 >= 0xF0 && b0 <= 0xF4) {
        if (!cont(1) || !cont(2) || !cont(3)) { return 0; }
        const auto b1 = static_cast<unsigned char>(s[i + 1]);
        if (b0 == 0xF0 && b1 < 0x90) { return 0; }  // overlong
        if (b0 == 0xF4 && b1 > 0x8F) { return 0; }  // above U+10FFFF
        return 4;
    }
    return 0;  // 0x80..0xC1 lead, 0xF5..0xFF
}

/// True when every byte of `s` belongs to a well-formed UTF-8 sequence. ASCII is skipped eight bytes at a time.
[[nodiscard]] inline auto utf8_valid(std::string_view s) noexcept -> bool {
    size_t i = 0;
    while (i < s.size()) {
        if (s.size() - i >= 8) {
            std::uint64_t word = 0;
            std::memcpy(&word, s.data() + i, sizeof word);
            if ((word & 0x8080808080808080ULL) == 0) {
                i += 8;
                continue;
            }
        }
        const size_t n = utf8_sequence_length(s, i);
        if (n == 0) { return false; }
        i += n;
    }
    return true;
}

/// Appends `s` as a quoted JSON string: `"` and `\` escaped, \n \t \r by name, every other byte below 0x20 as
/// \u00XX, everything else (including UTF-8) verbatim. The caller decides whether invalid UTF-8 is acceptable.
inline auto append_json_string(std::string& out, std::string_view s) -> void {
    static constexpr std::array<char, 16> kHex{'0', '1', '2', '3', '4', '5', '6', '7',
                                              '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    out += '"';
    const char* ptr = s.data();
    const char* const end = s.data() + s.size();
    while (ptr < end) {
        const char* const stop = find_escape_position_simd_impl(ptr, end);
        out.append(ptr, stop);
        ptr = stop;
        if (ptr == end) { break; }
        switch (*ptr) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default: {
                const auto b = static_cast<unsigned char>(*ptr);
                out += "\\u00";
                out += kHex[b >> 4U];
                out += kHex[b & 0x0FU];
                break;
            }
        }
        ++ptr;
    }
    out += '"';
}

/// Exact decimal digits of a binary floating value m * 2^e, as a digit string and the power of ten of its first
/// digit. Base-1e9 limbs; exact for every finite binary128 (at most ~11,500 digits, built only for the rare
/// value outside double range or precision).
struct exact_decimal {
    std::string digits;   // no leading zeros; "0" for zero
    long exponent10 = 0;  // value = digits[0].digits[1..] * 10^exponent10
};

#if !defined(_WIN32)
[[nodiscard]] inline auto exact_decimal_of(unsigned __int128 m, int e) -> exact_decimal {
    std::vector<std::uint32_t> limbs;  // little-endian, base 1e9
    while (m != 0) {
        limbs.push_back(static_cast<std::uint32_t>(m % 1000000000U));
        m /= 1000000000U;
    }
    if (limbs.empty()) { return {"0", 0}; }
    const auto mul_small = [&](std::uint32_t k) {
        std::uint64_t carry = 0;
        for (auto& limb : limbs) {
            const std::uint64_t cur = static_cast<std::uint64_t>(limb) * k + carry;
            limb = static_cast<std::uint32_t>(cur % 1000000000U);
            carry = cur / 1000000000U;
        }
        while (carry != 0) {
            limbs.push_back(static_cast<std::uint32_t>(carry % 1000000000U));
            carry /= 1000000000U;
        }
    };
    int pending = e >= 0 ? e : -e;
    const std::uint32_t big = e >= 0 ? (1U << 29U) : 1220703125U;  // 2^29 or 5^13
    const int big_step = e >= 0 ? 29 : 13;
    while (pending >= big_step) {
        mul_small(big);
        pending -= big_step;
    }
    std::uint32_t rest = 1;
    for (int k = 0; k < pending; ++k) { rest *= e >= 0 ? 2U : 5U; }
    if (rest != 1) { mul_small(rest); }
    exact_decimal out;
    out.digits = std::to_string(limbs.back());
    for (size_t i = limbs.size() - 1; i-- > 0;) {
        const std::string part = std::to_string(limbs[i]);
        out.digits.append(9 - part.size(), '0');
        out.digits += part;
    }
    // e < 0: the integer N = m * 5^-e carries the value N * 10^e.
    out.exponent10 = static_cast<long>(out.digits.size()) - 1 + (e < 0 ? e : 0);
    return out;
}
#endif

/// `x` rounded half-to-even to `p` significant digits, written as a JSON number (positional for exponents in
/// [-7, 21), scientific otherwise), with trailing zeros removed.
[[nodiscard]] inline auto decimal_text(const exact_decimal& x, size_t p, bool negative) -> std::string {
    std::string d = x.digits;
    long exp10 = x.exponent10;
    if (d.size() > p) {
        const char next = d[p];
        const bool rest_nonzero = d.find_first_not_of('0', p + 1) != std::string::npos;
        const bool round_up = next > '5' || (next == '5' && (rest_nonzero || ((d[p - 1] - '0') % 2 != 0)));
        d.resize(p);
        if (round_up) {
            size_t i = p;
            while (i > 0 && d[i - 1] == '9') {
                d[i - 1] = '0';
                --i;
            }
            if (i == 0) {
                d.insert(d.begin(), '1');
                d.pop_back();
                ++exp10;
            } else {
                ++d[i - 1];
            }
        }
    }
    while (d.size() > 1 && d.back() == '0') { d.pop_back(); }
    std::string out = negative ? "-" : "";
    if (exp10 >= -7 && exp10 < 21) {
        if (exp10 < 0) {
            out += "0.";
            out.append(static_cast<size_t>(-exp10 - 1), '0');
            out += d;
        } else if (static_cast<size_t>(exp10) + 1 >= d.size()) {
            out += d;
            out.append(static_cast<size_t>(exp10) + 1 - d.size(), '0');
        } else {
            out += d.substr(0, static_cast<size_t>(exp10) + 1);
            out += '.';
            out += d.substr(static_cast<size_t>(exp10) + 1);
        }
        return out;
    }
    out += d[0];
    if (d.size() > 1) {
        out += '.';
        out += d.substr(1);
    }
    out += 'e';
    out += std::to_string(exp10);
    return out;
}

#if !defined(_WIN32)
/// Shortest decimal that reads back (through strtold, the parser's own reader) to exactly `v`; 36 significant
/// digits -- enough to identify any binary128 -- when no shorter one does. `v` must be finite. libc++'s
/// to_chars(long double) formats at double precision (1e999 printed "inf"), so the digits are built exactly here.
[[nodiscard]] inline auto float128_text(__float128 v) -> std::string {
    unsigned __int128 bits = 0;
    static_assert(sizeof bits == sizeof v);
    std::memcpy(&bits, &v, sizeof v);
    const bool negative = (bits >> 127U) != 0;
    if ((bits << 1U) == 0) { return negative ? "-0" : "0"; }
    const auto biased = static_cast<int>((bits >> 112U) & 0x7FFFU);
    const unsigned __int128 frac = bits & ((static_cast<unsigned __int128>(1) << 112U) - 1U);
    const unsigned __int128 mant = biased == 0 ? frac : (frac | (static_cast<unsigned __int128>(1) << 112U));
    const int e2 = (biased == 0 ? 1 : biased) - 16383 - 112;
    const exact_decimal x = exact_decimal_of(mant, e2);
    for (size_t p = 17; p <= 36; ++p) {
        std::string text = decimal_text(x, p, negative);
        char* endp = nullptr;
        const long double back = std::strtold(text.c_str(), &endp);
        if (static_cast<__float128>(back) == v) { return text; }
    }
    return decimal_text(x, 36, negative);
}
#endif

} // namespace fastjson::detail

export module fastjson;

#if !defined(SENSEN_NO_IMPORT_STD)
import std;
#endif

// Module-internal (not exported). In the purview because the GMF's <charconv> exposes no floating to_chars here.
namespace fastjson::detail {
/// Appends the shortest round-trip decimal of a FINITE double (std::to_chars); the caller handles non-finite.
/// Returns false, appending nothing, if the text could not be formed (the caller decides what that means).
[[nodiscard]] inline auto append_double(std::string& out, double v) -> bool {
    std::array<char, 64> buf{};  // the longest shortest-round-trip double is 24 characters
    const auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (ec != std::errc{}) {
        return false;
    }
    out.append(buf.data(), end);
    return true;
}
}  // namespace fastjson::detail

export namespace fastjson {

namespace gpu {
    using fastjson::gpu::gpu_backend;
    using fastjson::gpu::gpu_info;
    using fastjson::gpu::gpu_parse_config;
    using fastjson::gpu::gpu_parse_result;
    using fastjson::gpu::gpu_buffer;
    using fastjson::gpu::detect_gpu_backend;
    using fastjson::gpu::get_gpu_info;
    using fastjson::gpu::is_gpu_available;
    using fastjson::gpu::parse_on_gpu;
    using fastjson::gpu::gpu_find_whitespace;
    using fastjson::gpu::gpu_find_strings;
    using fastjson::gpu::gpu_find_numbers;
    using fastjson::gpu::gpu_find_structural_chars;
    using fastjson::gpu::gpu_matrix_multiply;
    using fastjson::gpu::gpu_launch_triton_ptx;
}

// SIMD Wrappers — delegate to detail:: implementations in global module fragment
// All actual SIMD intrinsic usage lives in the GMF to avoid Clang 21 module BMI segfault.
// ============================================================================
// SIMD capability constants for the public API.
//
// These carry LITERAL values rather than aliasing detail::SIMD_* . The detail:: definitions
// live in the global module fragment and are `static`, i.e. internal linkage, which makes them
// TU-local. Initialising an exported constant from one exposes a TU-local entity in the module
// interface -- ill-formed per [basic.link]/17, and clang warns -WTU-local-entity-exposure on
// every one of the 14. Values are duplicated from the detail:: block above and must stay in
// sync; they are stable ABI bit flags, so that is a fixed cost, not a maintenance burden.
constexpr uint32_t SIMD_SSE2        = 0x002;
constexpr uint32_t SIMD_SSE3        = 0x004;
constexpr uint32_t SIMD_SSSE3       = 0x008;
constexpr uint32_t SIMD_SSE41       = 0x010;
constexpr uint32_t SIMD_SSE42       = 0x020;
constexpr uint32_t SIMD_AVX         = 0x040;
constexpr uint32_t SIMD_AVX2        = 0x080;
constexpr uint32_t SIMD_AVX512F     = 0x100;
constexpr uint32_t SIMD_AVX512BW    = 0x200;
constexpr uint32_t SIMD_AVX512VBMI  = 0x400;
constexpr uint32_t SIMD_AVX512VBMI2 = 0x800;
constexpr uint32_t SIMD_AVX512VNNI  = 0x1000;
constexpr uint32_t SIMD_AMX_TILE    = 0x2000;
constexpr uint32_t SIMD_AMX_INT8    = 0x4000;

// Thread-safe SIMD capability detection — delegates to GMF implementation
[[nodiscard]] inline auto detect_simd_capabilities() noexcept -> uint32_t {
    return detail::detect_simd_capabilities();
}

// Whitespace skip — runtime SIMD dispatch via GMF detail:: implementation
[[nodiscard]] inline auto skip_whitespace_simd(const char* data, size_t size) -> const char* {
    return detail::skip_whitespace_simd_impl(data, size);
}

// First '"', '\\' or control byte (< 0x20) in [data, data + size), or data + size: the parser's string-end
// scan (AVX-512 8x zmm -> AVX2 8x ymm -> scalar, chosen at run time; FASTJSON_SIMD caps it). Exported because
// any text scanner that stops at the same three classes can reuse it.
[[nodiscard]] inline auto find_string_end_simd(const char* data, size_t size) -> const char* {
    return detail::find_string_end_simd_impl(data, data + size);
}

// First byte in [data, data + size) equal to ANY byte of `set`, or data + size (also for an empty set).
// Same dispatch as find_string_end_simd (AVX-512 8x zmm -> AVX2 8x ymm -> scalar; FASTJSON_SIMD caps it).
// Needles compare for equality, so bytes >= 0x80 (UTF-8) are ordinary needles/haystack bytes. Up to 8 needles
// run on the vector kernels; a longer set is answered by the exact scalar scan. For ONE needle prefer
// std::string_view::find(char) (glibc's SIMD memchr).
[[nodiscard]] inline auto find_first_of_simd(const char* data, size_t size, std::string_view set) -> const char* {
    return detail::find_first_of_simd_impl(data, data + size,
                                           reinterpret_cast<const unsigned char*>(set.data()), set.size());
}

// The kernel `find_string_end_simd` (and the parser's string fast path) uses here: "avx512" | "avx2" | "scalar".
[[nodiscard]] inline auto string_scan_tier() -> std::string_view { return detail::string_scan_tier_impl(); }

// (All SIMD implementations removed from module purview — see GMF detail:: namespace)
// JSON Type Definitions - Standard Container Based
// ============================================================================

// Forward declarations
class json_value;
class parser;

// Error handling types
enum class json_error_code {
    empty_input,
    extra_tokens,
    max_depth_exceeded,
    unexpected_end,
    invalid_syntax,
    invalid_literal,
    invalid_number,
    invalid_string,
    invalid_escape,
    invalid_unicode,
    invalid_writer_sequence  ///< fastjson::writer: a call order that does not describe one JSON value
};

struct json_error {
    json_error_code code;
    std::string message;
    size_t line;
    size_t column;

    // String conversion for debugging
    auto to_string() const -> std::string {
        std::ostringstream oss;
        oss << "JSON Error at line " << line << ", column " << column << ": " << message;
        return oss.str();
    }
};

// Stream output operator for json_error
inline auto operator<<(std::ostream& os, const json_error& error) -> std::ostream& {
    return os << error.to_string();
}

// Result type for parsing operations (using std::expected for C++23)
template <typename T> using json_result = std::expected<T, json_error>;

// Zero-copy string with COW semantics: holds string_view into input buffer
// or owned std::string for escaped/mutated strings.
// Lifetime rule: string_view points into the original JSON input buffer.
// The caller must keep the input alive while json_value objects exist.
class json_string_data {
    std::variant<std::string_view, std::string> data_;

public:
    json_string_data() noexcept : data_(std::string_view{}) {}
    json_string_data(std::string_view sv) noexcept : data_(sv) {}
    json_string_data(std::string&& s) noexcept : data_(std::move(s)) {}
    json_string_data(const std::string& s) : data_(s) {}
    json_string_data(const char* s) noexcept : data_(std::string_view(s)) {}

    // Always returns a view (zero-copy for unescaped strings)
    [[nodiscard]] auto view() const noexcept -> std::string_view {
        return std::visit([](const auto& v) -> std::string_view { return v; }, data_);
    }

    // Materializes an owned string if needed
    [[nodiscard]] auto to_string() const -> std::string {
        return std::visit(
            [](const auto& v) -> std::string {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, json_string_data>) {
                    return v;
                } else {
                    return std::string(v);
                }
            },
            data_);
    }

    // COW: ensures the string is owned before mutation
    auto ensure_owned() -> std::string& {
        if (std::holds_alternative<std::string_view>(data_)) {
            auto sv = std::get<std::string_view>(data_);
            data_ = std::string(sv);
        }
        return std::get<std::string>(data_);
    }

    [[nodiscard]] auto is_view() const noexcept -> bool {
        return std::holds_alternative<std::string_view>(data_);
    }

    auto operator==(const json_string_data& other) const noexcept -> bool {
        return view() == other.view();
    }

    auto operator==(std::string_view sv) const noexcept -> bool {
        return view() == sv;
    }

    // Implicit conversion to string_view for seamless interop
    operator std::string_view() const noexcept { return view(); }

    // For STL container compat (e.g., unordered_map key hashing)
    [[nodiscard]] auto size() const noexcept -> size_t { return view().size(); }
    [[nodiscard]] auto length() const noexcept -> size_t { return view().length(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return view().empty(); }
    [[nodiscard]] auto data() const noexcept -> const char* { return view().data(); }

    auto clear() -> void { data_ = std::string_view{}; }
};

// JSON container type aliases using standard containers
using json_string = json_string_data;
using json_number = double;               // Default 64-bit float
using json_number_128 = float128_compat;       // Extended 128-bit float
using json_int_128 = int128_compat;            // 128-bit signed integer
using json_uint_128 = uint128_compat;  // 128-bit unsigned integer
using json_boolean = bool;
using json_null = std::nullptr_t;            // Use nullptr_t for direct equivalence with nullptr
#ifdef FASTJSON_USE_PARALLEL_STL
using json_array = std::vector<json_value, tbb::scalable_allocator<json_value>>;
using json_object = std::unordered_map<std::string, json_value, std::hash<std::string>, std::equal_to<std::string>, tbb::scalable_allocator<std::pair<const std::string, json_value>>>;
#else
using json_array = std::vector<json_value>;  // Array as std::vector
using json_object =
    std::unordered_map<std::string, json_value>;  // Object as unordered_map with string keys
#endif

// Precision information for adaptive number parsing
struct number_precision_info {
    int significant_digits = 0;  // Count of significant digits
    int exponent = 0;            // Exponent value (0 if no exponent)
    bool has_decimal = false;    // Whether number has decimal point
    bool has_exponent = false;   // Whether number has exponent
    bool is_negative = false;    // Whether number is negative
    bool needs_128bit = false;   // Whether 128-bit precision is required
    bool is_integer = false;     // Whether number is an integer (no decimal/exponent)
};

// Internal storage for complex types to support Copy-On-Write (COW)
using json_array_ptr = std::shared_ptr<json_array>;
using json_object_ptr = std::shared_ptr<json_object>;

// JSON value variant type with 128-bit support and COW pointers
using json_data = std::variant<json_null, json_boolean, json_number, json_number_128, json_int_128,
                               json_uint_128, json_string_data, json_array_ptr, json_object_ptr>;

// Main JSON value class with thread-safe operations
class json_value {
private:
    json_data data_;

    // COW helper to ensure unique ownership before modification
    template<typename T, typename Ptr>
    auto ensure_unique(Ptr& ptr) -> T& {
        if (ptr.use_count() > 1) {
            ptr = std::make_shared<T>(*ptr);
        }
        return *ptr;
    }

    // Private helper methods for serialization
    auto serialize_to_buffer(std::string& buffer, int indent) const -> void;
    auto serialize_string_to_buffer(std::string& buffer, std::string_view str) const -> void;
    auto serialize_array_to_buffer(std::string& buffer, const json_array& arr, int indent) const
        -> void;
    auto serialize_object_to_buffer(std::string& buffer, const json_object& obj, int indent) const
        -> void;

public:
    // Constructors
    json_value() : data_(nullptr) {}  // Default to nullptr for null JSON values

    json_value(std::nullptr_t) : data_(nullptr) {}

    json_value(bool value) : data_(value) {}

    json_value(int value) : data_(static_cast<int128_compat>(value)) {}

    json_value(int64_t value) : data_(static_cast<int128_compat>(value)) {}

    json_value(uint64_t value) : data_(static_cast<uint128_compat>(value)) {}

    json_value(double value) : data_(value) {}

    json_value(float128_compat value) : data_(value) {}

    json_value(int128_compat value) : data_(value) {}

    json_value(uint128_compat value) : data_(value) {}

    json_value(const char* value) : data_(json_string_data(std::string_view(value))) {}

    json_value(std::string_view value) : data_(json_string_data(value)) {}

    json_value(const std::string& value) : data_(json_string_data(value)) {}

    json_value(std::string&& value) : data_(json_string_data(std::move(value))) {}

    json_value(json_string_data value) : data_(std::move(value)) {}

    json_value(const json_array& array) : data_(std::make_shared<json_array>(array)) {}

    json_value(json_array&& array) : data_(std::make_shared<json_array>(std::move(array))) {}

    json_value(const json_object& object) : data_(std::make_shared<json_object>(object)) {}

    json_value(json_object&& object) : data_(std::make_shared<json_object>(std::move(object))) {}

    // Convenience COW constructors for pointers
    json_value(json_array_ptr array) : data_(std::move(array)) {}
    json_value(json_object_ptr object) : data_(std::move(object)) {}

    // Copy and move constructors
    json_value(const json_value&) = default;
    json_value(json_value&&) = default;
    json_value& operator=(const json_value&) = default;
    json_value& operator=(json_value&&) = default;

    // Type checking methods (declared here, implemented below)
    auto is_null() const noexcept -> bool;
    auto is_boolean() const noexcept -> bool;
    auto is_number() const noexcept -> bool;
    auto is_number_128() const noexcept -> bool;
    auto is_int_128() const noexcept -> bool;
    auto is_uint_128() const noexcept -> bool;
    auto is_string() const noexcept -> bool;
    auto is_array() const noexcept -> bool;
    auto is_object() const noexcept -> bool;

    // Value accessor methods (declared here, implemented below)
    auto as_boolean() const -> bool;
    auto as_number() const -> double;
    auto as_number_128() const -> float128_compat;
    auto as_int_128() const -> int128_compat;
    auto as_uint_128() const -> uint128_compat;
    auto as_string() const -> std::string_view;
    auto as_std_string() const -> std::string;
    auto as_string_data() const -> const json_string_data&;
    auto as_array() const -> const json_array&;
    auto as_object() const -> const json_object&;

    // Numeric conversion helpers with automatic type handling
    // These methods convert between numeric types intelligently
    auto as_int64() const -> int64_t;
    auto as_uint64() const -> uint64_t;
    auto as_float64() const -> double;
    auto as_int128() const -> int128_compat;
    auto as_uint128() const -> uint128_compat;
    auto as_float128() const -> float128_compat;

    // CHECKED reads, for values from outside (a provider's stream, a peer's frame, a config file): the exact value
    // or an error -- never a rounded, wrapped or substituted one, and never an exception. get_int64/get_uint64 take
    // any JSON number whose value is an integer in range (1, 1.0, 1e3, a 128-bit integer); get_double takes any
    // finite JSON number at its nearest double.
    [[nodiscard]] auto get_int64() const -> json_result<int64_t>;
    [[nodiscard]] auto get_uint64() const -> json_result<uint64_t>;
    [[nodiscard]] auto get_double() const -> json_result<double>;

    // Mutable accessor methods
    auto as_array() -> json_array&;
    auto as_object() -> json_object&;

    // Array operations
    auto push_back(json_value value) -> json_value&;
    auto pop_back() -> json_value&;
    auto clear() -> json_value&;
    auto size() const noexcept -> size_t;
    auto empty() const noexcept -> bool;
    auto operator[](size_t index) const -> const json_value&;
    auto operator[](size_t index) -> json_value&;

    // Object operations
    auto operator[](const std::string& key) -> json_value&;
    auto operator[](const std::string& key) const -> const json_value&;
    auto insert(const std::string& key, json_value value) -> json_value&;
    auto erase(const std::string& key) -> json_value&;
    auto contains(const std::string& key) const noexcept -> bool;

    // Serialization methods
    auto to_string() const -> std::string;
    auto to_pretty_string(int indent = 4) const -> std::string;

private:
    // Private helper methods for serialization
    auto serialize_pretty_to_buffer(std::string& buffer, int indent_size, int current_indent) const
        -> void;
    auto serialize_pretty_array(std::string& buffer, const json_array& arr, int indent_size,
                                int current_indent) const -> void;
    auto serialize_pretty_object(std::string& buffer, const json_object& obj, int indent_size,
                                 int current_indent) const -> void;
};

// Thread-safe JSON value implementation
// ============================================================================

// Thread-safe type checking functions
// Integer -> narrower integer: the plain cast wraps (a huge id read back as a small or negative one).
// Floating -> integer: the plain cast is undefined behaviour out of range. Both are checked here, ONCE, as optional
// results: the get_*() accessors report an empty one as an error, the as_*() accessors throw std::out_of_range.
template <class To>
inline constexpr bool kSignedIntegral = std::is_same_v<To, int64_t> || std::is_same_v<To, int128_compat>;

template <class To, class From>
[[nodiscard]] auto narrow_int(From v) -> std::optional<To> {
    if constexpr (std::is_same_v<From, int128_compat>) {
        if constexpr (!kSignedIntegral<To>) {
            if (v < static_cast<int128_compat>(int64_t{0})) { return std::nullopt; }
        }
        if constexpr (std::is_same_v<To, int64_t>) {
            if (v < static_cast<int128_compat>(std::numeric_limits<int64_t>::min()) ||
                v > static_cast<int128_compat>(std::numeric_limits<int64_t>::max())) {
                return std::nullopt;
            }
        } else if constexpr (std::is_same_v<To, uint64_t>) {
            if (static_cast<uint128_compat>(v) > static_cast<uint128_compat>(std::numeric_limits<uint64_t>::max())) {
                return std::nullopt;
            }
        }
    } else {  // uint128_compat
        if constexpr (std::is_same_v<To, int64_t>) {
            if (v > static_cast<uint128_compat>(static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))) {
                return std::nullopt;
            }
        } else if constexpr (std::is_same_v<To, uint64_t>) {
            if (v > static_cast<uint128_compat>(std::numeric_limits<uint64_t>::max())) { return std::nullopt; }
        } else if constexpr (std::is_same_v<To, int128_compat>) {
            if ((v >> 127) == static_cast<uint128_compat>(uint64_t{1})) { return std::nullopt; }
        }
    }
    return static_cast<To>(v);
}

// Truncates toward zero. NaN gives 0 (the accessors' documented value); out of range gives nothing.
template <class To, class From>
[[nodiscard]] auto narrow_float(From v) -> std::optional<To> {
    const auto ld = static_cast<long double>(v);
    if (ld != ld) {
        return To(0);
    }
#if defined(_MSC_VER) && !defined(__clang__)
    // MSVC's 128-bit compatibility types convert from a floating value through 64 bits: past 2^63 / 2^64 that
    // conversion truncates, so only the 64-bit range is accepted there.
    constexpr int bits = 64;
#else
    constexpr int bits = static_cast<int>(sizeof(To) * 8);
#endif
    const long double hi = std::ldexp(1.0L, kSignedIntegral<To> ? bits - 1 : bits);
    const bool in_range = kSignedIntegral<To> ? (ld >= -hi && ld < hi) : (ld > -1.0L && ld < hi);
    if (!in_range) {
        return std::nullopt;
    }
    return static_cast<To>(v);
}

template <class To>
[[nodiscard]] auto or_out_of_range(std::optional<To> v) -> To {
    if (!v.has_value()) {
        throw std::out_of_range("JSON number does not fit the requested integer type");
    }
    return v.value();
}

auto json_value::is_null() const noexcept -> bool {
    return std::holds_alternative<std::nullptr_t>(data_);
}

auto json_value::is_boolean() const noexcept -> bool {
    return std::holds_alternative<bool>(data_);
}

// "Is this a JSON number?" -- whatever representation the parser chose for it (double, or a 128-bit float or
// integer when a double would lose digits or range). Asking only about `double` here made a 16-digit integer or a
// 17-digit float "not a number" to every caller, which then had to know the representations (rule 66).
auto json_value::is_number() const noexcept -> bool {
    return std::holds_alternative<double>(data_) || std::holds_alternative<float128_compat>(data_) ||
           std::holds_alternative<int128_compat>(data_) || std::holds_alternative<uint128_compat>(data_);
}

auto json_value::is_number_128() const noexcept -> bool {
    return std::holds_alternative<float128_compat>(data_);
}

auto json_value::is_int_128() const noexcept -> bool {
    return std::holds_alternative<int128_compat>(data_);
}

auto json_value::is_uint_128() const noexcept -> bool {
    return std::holds_alternative<uint128_compat>(data_);
}

auto json_value::is_string() const noexcept -> bool {
    return std::holds_alternative<json_string_data>(data_);
}

auto json_value::is_array() const noexcept -> bool {
    return std::holds_alternative<json_array_ptr>(data_);
}

auto json_value::is_object() const noexcept -> bool {
    return std::holds_alternative<json_object_ptr>(data_);
}

// Thread-safe value accessor functions
auto json_value::as_boolean() const -> bool {
    if (!is_boolean()) {
        throw std::runtime_error("JSON value is not a boolean");
    }
    return std::get<bool>(data_);
}

auto json_value::as_number() const -> double {
    // Try 64-bit double first (fast path)
    if (std::holds_alternative<double>(data_)) {
        return std::get<double>(data_);
    }

    // Fallback: Convert from 128-bit types (with potential precision loss)
    if (is_number_128()) {
        return static_cast<double>(std::get<float128_compat>(data_));
    }
    if (is_int_128()) {
        return static_cast<double>(std::get<int128_compat>(data_));
    }
    if (is_uint_128()) {
        return static_cast<double>(std::get<uint128_compat>(data_));
    }

    // Not a numeric type at all
    return std::numeric_limits<double>::quiet_NaN();
}

auto json_value::as_number_128() const -> float128_compat {
    // Try 128-bit float first
    if (is_number_128()) {
        return std::get<float128_compat>(data_);
    }

    // Fallback: Convert from other numeric types (upcast or precision loss)
    if (std::holds_alternative<double>(data_)) {
        return static_cast<float128_compat>(std::get<double>(data_));
    }
    if (is_int_128()) {
        return static_cast<float128_compat>(std::get<int128_compat>(data_));
    }
    if (is_uint_128()) {
        return static_cast<float128_compat>(std::get<uint128_compat>(data_));
    }

    // Not a numeric type at all
    return static_cast<float128_compat>(std::numeric_limits<double>::quiet_NaN());
}

auto json_value::as_int_128() const -> int128_compat {
    // Try signed 128-bit first
    if (is_int_128()) {
        return std::get<int128_compat>(data_);
    }
    // Fallback: Convert from other numeric types
    if (is_uint_128()) {
        return or_out_of_range(narrow_int<int128_compat>(std::get<uint128_compat>(data_)));
    }
    if (std::holds_alternative<double>(data_)) {
        const double val = std::get<double>(data_);
        if (__builtin_isnan(val)) {
            return 0;
        }
        return or_out_of_range(narrow_float<int128_compat>(val));
    }
    if (is_number_128()) {
        return or_out_of_range(narrow_float<int128_compat>(std::get<float128_compat>(data_)));
    }
    return 0;
}

auto json_value::as_uint_128() const -> uint128_compat {
    // Try unsigned 128-bit first
    if (is_uint_128()) {
        return std::get<uint128_compat>(data_);
    }
    // Fallback: Convert from other numeric types
    if (is_int_128()) {
        return or_out_of_range(narrow_int<uint128_compat>(std::get<int128_compat>(data_)));
    }
    if (std::holds_alternative<double>(data_)) {
        double val = std::get<double>(data_);
        if (__builtin_isnan(val))
            return 0;
        return or_out_of_range(narrow_float<uint128_compat>(val));
    }
    if (is_number_128()) {
        return or_out_of_range(narrow_float<uint128_compat>(std::get<float128_compat>(data_)));
    }
    return 0;
}

auto json_value::as_string() const -> std::string_view {
    if (!is_string()) {
        throw std::runtime_error("JSON value is not a string");
    }
    return std::get<json_string_data>(data_).view();
}

auto json_value::as_std_string() const -> std::string {
    return std::string(as_string());
}

auto json_value::as_string_data() const -> const json_string_data& {
    if (!is_string()) {
        throw std::runtime_error("JSON value is not a string");
    }
    return std::get<json_string_data>(data_);
}

// Numeric conversion helpers with automatic type handling
// Returns NaN for non-numeric types instead of throwing
auto json_value::as_int64() const -> int64_t {
    if (std::holds_alternative<double>(data_)) {
        double val = std::get<double>(data_);
        if (__builtin_isnan(val))
            return 0;
        return or_out_of_range(narrow_float<int64_t>(val));
    } else if (is_int_128()) {
        return or_out_of_range(narrow_int<int64_t>(std::get<int128_compat>(data_)));
    } else if (is_uint_128()) {
        return or_out_of_range(narrow_int<int64_t>(std::get<uint128_compat>(data_)));
    } else if (is_number_128()) {
        return or_out_of_range(narrow_float<int64_t>(std::get<float128_compat>(data_)));
    }
    return 0;  // Non-numeric type returns 0
}

auto json_value::as_uint64() const -> uint64_t {
    if (std::holds_alternative<double>(data_)) {
        double val = std::get<double>(data_);
        if (__builtin_isnan(val))
            return 0;
        return or_out_of_range(narrow_float<uint64_t>(val));
    } else if (is_uint_128()) {
        return or_out_of_range(narrow_int<uint64_t>(std::get<uint128_compat>(data_)));
    } else if (is_int_128()) {
        return or_out_of_range(narrow_int<uint64_t>(std::get<int128_compat>(data_)));
    } else if (is_number_128()) {
        return or_out_of_range(narrow_float<uint64_t>(std::get<float128_compat>(data_)));
    }
    return 0;  // Non-numeric type returns 0
}

auto json_value::as_float64() const -> double {
    if (std::holds_alternative<double>(data_)) {
        return std::get<double>(data_);
    } else if (is_int_128()) {
        return static_cast<double>(std::get<int128_compat>(data_));
    } else if (is_uint_128()) {
        return static_cast<double>(std::get<uint128_compat>(data_));
    } else if (is_number_128()) {
        return static_cast<double>(std::get<float128_compat>(data_));
    }
    return std::numeric_limits<double>::quiet_NaN();
}

auto json_value::as_int128() const -> int128_compat {
    if (is_int_128()) {
        return std::get<int128_compat>(data_);
    } else if (is_uint_128()) {
        return or_out_of_range(narrow_int<int128_compat>(std::get<uint128_compat>(data_)));
    }
    if (std::holds_alternative<double>(data_)) {
        const double val = std::get<double>(data_);
        if (__builtin_isnan(val)) {
            return 0;
        }
        return or_out_of_range(narrow_float<int128_compat>(val));
    }
    if (is_number_128()) {
        return or_out_of_range(narrow_float<int128_compat>(std::get<float128_compat>(data_)));
    }
    return 0;  // Non-numeric type returns 0
}

auto json_value::as_uint128() const -> uint128_compat {
    if (is_uint_128()) {
        return std::get<uint128_compat>(data_);
    }
    if (is_int_128()) {
        return or_out_of_range(narrow_int<uint128_compat>(std::get<int128_compat>(data_)));
    }
    if (std::holds_alternative<double>(data_)) {
        const double val = std::get<double>(data_);
        if (__builtin_isnan(val)) {
            return 0;
        }
        return or_out_of_range(narrow_float<uint128_compat>(val));
    }
    if (is_number_128()) {
        return or_out_of_range(narrow_float<uint128_compat>(std::get<float128_compat>(data_)));
    }
    return 0;  // Non-numeric type returns 0
}

auto json_value::as_float128() const -> float128_compat {
    if (is_number_128()) {
        return std::get<float128_compat>(data_);
    } else if (std::holds_alternative<double>(data_)) {
        return static_cast<float128_compat>(std::get<double>(data_));
    } else if (is_int_128()) {
        return static_cast<float128_compat>(std::get<int128_compat>(data_));
    } else if (is_uint_128()) {
        return static_cast<float128_compat>(std::get<uint128_compat>(data_));
    }
    return static_cast<float128_compat>(std::numeric_limits<double>::quiet_NaN());
}

namespace checked_read {
[[nodiscard]] inline auto number_error(std::string_view what) -> json_error {
    return json_error{json_error_code::invalid_number, std::string{what}, 0, 0};
}

// The exact integer a JSON number holds, as To, or why not.
template <class To, class Data>
[[nodiscard]] auto exact_integer(const Data& data) -> json_result<To> {
    return std::visit(
        [](const auto& v) -> json_result<To> {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float128_compat>) {
                const auto ld = static_cast<long double>(v);
                if (!std::isfinite(ld)) { return std::unexpected(number_error("the number is not finite")); }
                if (ld != std::truncl(ld)) { return std::unexpected(number_error("the number is not an integer")); }
                if (auto n = narrow_float<To>(v)) { return n.value(); }
                return std::unexpected(number_error("the integer is outside the requested range"));
            } else if constexpr (std::is_same_v<T, int128_compat> || std::is_same_v<T, uint128_compat>) {
                if (auto n = narrow_int<To>(v)) { return n.value(); }
                return std::unexpected(number_error("the integer is outside the requested range"));
            } else {
                return std::unexpected(number_error("the value is not a number"));
            }
        },
        data);
}
}  // namespace checked_read

auto json_value::get_int64() const -> json_result<int64_t> { return checked_read::exact_integer<int64_t>(data_); }

auto json_value::get_uint64() const -> json_result<uint64_t> { return checked_read::exact_integer<uint64_t>(data_); }

auto json_value::get_double() const -> json_result<double> {
    if (!is_number()) {
        return std::unexpected(checked_read::number_error("the value is not a number"));
    }
    const double d = as_float64();
    if (!std::isfinite(d)) {
        return std::unexpected(checked_read::number_error("the number is outside double range"));
    }
    return d;
}

auto json_value::as_array() const -> const json_array& {
    if (!is_array()) {
        throw std::runtime_error("JSON value is not an array");
    }
    return *std::get<json_array_ptr>(data_);
}

auto json_value::as_object() const -> const json_object& {
    if (!is_object()) {
        throw std::runtime_error("JSON value is not an object");
    }
    return *std::get<json_object_ptr>(data_);
}

// Thread-safe mutable accessor functions with Copy-On-Write (COW)
auto json_value::as_array() -> json_array& {
    if (!is_array()) {
        data_ = std::make_shared<json_array>();
    }
    return ensure_unique<json_array>(std::get<json_array_ptr>(data_));
}

auto json_value::as_object() -> json_object& {
    if (!is_object()) {
        data_ = std::make_shared<json_object>();
    }
    return ensure_unique<json_object>(std::get<json_object_ptr>(data_));
}

// Thread-safe array operations with trail calling syntax
auto json_value::push_back(json_value value) -> json_value& {
    as_array().emplace_back(std::move(value));
    return *this;
}

auto json_value::pop_back() -> json_value& {
    auto& arr = as_array();
    if (arr.empty()) {
        throw std::runtime_error("Cannot pop_back on empty array");
    }
    arr.pop_back();
    return *this;
}

auto json_value::clear() -> json_value& {
    std::visit(
        [this](auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_array_ptr> || std::is_same_v<T, json_object_ptr>) {
                if (v.use_count() > 1) {
                    v = std::make_shared<typename T::element_type>();
                } else {
                    v->clear();
                }
            } else if constexpr (std::is_same_v<T, json_string_data>) {
                v.clear();
            }
        },
        data_);
    return *this;
}

auto json_value::size() const noexcept -> size_t {
    return std::visit(
        [](const auto& v) -> size_t {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_array_ptr> || std::is_same_v<T, json_object_ptr>) {
                return v->size();
            } else if constexpr (std::is_same_v<T, json_string_data>) {
                return v.length();
            } else {
                return 1;
            }
        },
        data_);
}

auto json_value::empty() const noexcept -> bool {
    return std::visit(
        [](const auto& v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, json_array_ptr> || std::is_same_v<T, json_object_ptr>) {
                return v->empty();
            } else if constexpr (std::is_same_v<T, json_string_data>) {
                return v.empty();
            } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return true;
            } else {
                return false;
            }
        },
        data_);
}

// Thread-safe object operations
auto json_value::insert(const std::string& key, json_value value) -> json_value& {
    as_object()[key] = std::move(value);
    return *this;
}

auto json_value::erase(const std::string& key) -> json_value& {
    as_object().erase(key);
    return *this;
}

auto json_value::contains(const std::string& key) const noexcept -> bool {
    if (!is_object()) {
        return false;
    }
    const auto& obj = as_object();
    return obj.find(key) != obj.end();
}

// Thread-safe indexing operators
auto json_value::operator[](size_t index) -> json_value& {
    auto& arr = as_array();
    if (index >= arr.size()) {
        throw std::out_of_range("Array index out of range");
    }
    return arr[index];
}

auto json_value::operator[](size_t index) const -> const json_value& {
    const auto& arr = as_array();
    if (index >= arr.size()) {
        throw std::out_of_range("Array index out of range");
    }
    return arr[index];
}

auto json_value::operator[](const std::string& key) -> json_value& {
    return as_object()[key];
}

auto json_value::operator[](const std::string& key) const -> const json_value& {
    const auto& obj = as_object();
    auto it = obj.find(key);
    if (it == obj.end()) {
        throw std::out_of_range("Object key not found: " + key);
    }
    return it->second;
}

// Thread-safe serialization with SIMD optimization
auto json_value::to_string() const -> std::string {
    thread_local std::string buffer;  // Thread-local buffer for performance
    buffer.clear();
    buffer.reserve(1024);  // Pre-allocate reasonable size

    serialize_to_buffer(buffer, 0);
    return buffer;
}

auto json_value::to_pretty_string(int indent) const -> std::string {
    thread_local std::string buffer;  // Thread-local buffer
    buffer.clear();
    buffer.reserve(2048);  // Larger buffer for pretty printing

    serialize_pretty_to_buffer(buffer, indent, 0);
    return buffer;
}

// Thread-safe internal serialization methods
auto json_value::serialize_to_buffer(std::string& buffer, int indent) const -> void {
    std::visit(
        [this, &buffer, indent](const auto& v) {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                buffer += "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                buffer += v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, double>) {
                // JSON has no NaN or infinity: one cannot be written as a number, so to_string() writes `null`
                // (the JSON.stringify rule). fastjson::to_json() refuses such a value instead.
                if (!std::isfinite(v) || !detail::append_double(buffer, v)) {
                    buffer += "null";
                }
            } else if constexpr (std::is_same_v<T, float128_compat>) {
#if defined(_WIN32)
                const auto d = static_cast<double>(v);
                if (!std::isfinite(d) || !detail::append_double(buffer, d)) {
                    buffer += "null";
                }
#else
                if (std::isfinite(static_cast<long double>(v))) {
                    buffer += detail::float128_text(v);
                } else {
                    buffer += "null";
                }
#endif
            } else if constexpr (std::is_same_v<T, int128_compat>) {
                // Convert int128_compat to string manually
                bool is_negative = v < 0;
                uint128_compat abs_val = is_negative ? -static_cast<uint128_compat>(v)
                                                        : static_cast<uint128_compat>(v);
                thread_local std::array<char, 64> num_buffer;
                char* ptr = num_buffer.data() + num_buffer.size();
                *--ptr = '\0';
                do {
                    *--ptr = static_cast<char>('0' + static_cast<char>(static_cast<uint64_t>(abs_val % 10)));
                    abs_val /= 10;
                } while (abs_val > 0);
                if (is_negative) {
                    *--ptr = '-';
                }
                buffer += ptr;
            } else if constexpr (std::is_same_v<T, uint128_compat>) {
                // Convert uint128_compat to string manually
                uint128_compat val = v;
                thread_local std::array<char, 64> num_buffer;
                char* ptr = num_buffer.data() + num_buffer.size();
                *--ptr = '\0';
                do {
                    *--ptr = static_cast<char>('0' + static_cast<char>(static_cast<uint64_t>(val % 10)));
                    val /= 10;
                } while (val > 0);
                buffer += ptr;
            } else if constexpr (std::is_same_v<T, json_string_data>) {
                serialize_string_to_buffer(buffer, v.view());
            } else if constexpr (std::is_same_v<T, json_array_ptr>) {
                serialize_array_to_buffer(buffer, *v, indent);
            } else if constexpr (std::is_same_v<T, json_object_ptr>) {
                serialize_object_to_buffer(buffer, *v, indent);
            }
        },
        data_);
}

auto json_value::serialize_string_to_buffer(std::string& buffer, std::string_view str) const
    -> void {
    detail::append_json_string(buffer, str);
}

auto json_value::serialize_array_to_buffer(std::string& buffer, const json_array& arr,
                                           int indent) const -> void {
    buffer += '[';

    for (size_t i = 0; i < arr.size(); ++i) {
        if (i > 0)
            buffer += ',';
        arr[i].serialize_to_buffer(buffer, indent);
    }

    buffer += ']';
}

auto json_value::serialize_object_to_buffer(std::string& buffer, const json_object& obj,
                                            int indent) const -> void {
    buffer += '{';

    bool first = true;
    for (const auto& [key, value] : obj) {
        if (!first)
            buffer += ',';
        first = false;

        serialize_string_to_buffer(buffer, key);
        buffer += ':';
        value.serialize_to_buffer(buffer, indent);
    }

    buffer += '}';
}

// Thread-safe pretty printing implementation
// Stream output operator for json_value
auto operator<<(std::ostream& os, const json_value& value) -> std::ostream& {
    return os << value.to_string();
}

auto json_value::serialize_pretty_to_buffer(std::string& buffer, int indent_size,
                                            int current_indent) const -> void {
    std::visit(
        [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, json_array_ptr>) {
                serialize_pretty_array(buffer, *v, indent_size, current_indent);
            } else if constexpr (std::is_same_v<T, json_object_ptr>) {
                serialize_pretty_object(buffer, *v, indent_size, current_indent);
            } else {
                // For non-container types, use regular serialization
                serialize_to_buffer(buffer, current_indent);
            }
        },
        data_);
}

auto json_value::serialize_pretty_array(std::string& buffer, const json_array& arr, int indent_size,
                                        int current_indent) const -> void {
    if (arr.empty()) {
        buffer += "[]";
        return;
    }

    buffer += "[\n";
    current_indent += indent_size;

    for (size_t i = 0; i < arr.size(); ++i) {
        buffer += std::string(current_indent, ' ');
        arr[i].serialize_pretty_to_buffer(buffer, indent_size, current_indent);

        if (i < arr.size() - 1) {
            buffer += ',';
        }
        buffer += '\n';
    }

    current_indent -= indent_size;
    buffer += std::string(current_indent, ' ') + ']';
}

auto json_value::serialize_pretty_object(std::string& buffer, const json_object& obj,
                                         int indent_size, int current_indent) const -> void {
    if (obj.empty()) {
        buffer += "{}";
        return;
    }

    buffer += "{\n";
    current_indent += indent_size;

    auto it = obj.begin();
    while (it != obj.end()) {
        buffer += std::string(current_indent, ' ');
        serialize_string_to_buffer(buffer, it->first);
        buffer += ": ";
        it->second.serialize_pretty_to_buffer(buffer, indent_size, current_indent);

        ++it;
        if (it != obj.end()) {
            buffer += ',';
        }
        buffer += '\n';
    }

    current_indent -= indent_size;
    buffer += std::string(current_indent, ' ') + '}';
}

// Number Precision Analysis for Adaptive Parsing
// ============================================================================

// Analyze a JSON number string to determine precision requirements
inline auto analyze_number_precision(const char* start, const char* end) -> number_precision_info {
    number_precision_info info;
    const char* ptr = start;

    // Check for negative sign
    if (ptr < end && *ptr == '-') {
        info.is_negative = true;
        ++ptr;
    }

    // Count significant digits
    bool leading_zero = false;
    bool after_decimal = false;
    int digits_before_decimal = 0;
    int digits_after_decimal = 0;

    while (ptr < end && (*ptr >= '0' && *ptr <= '9')) {
        if (*ptr == '0' && digits_before_decimal == 0 && !after_decimal) {
            leading_zero = true;
        } else {
            ++digits_before_decimal;
        }
        ++ptr;
    }

    // Check for decimal point
    if (ptr < end && *ptr == '.') {
        info.has_decimal = true;
        after_decimal = true;
        ++ptr;

        while (ptr < end && (*ptr >= '0' && *ptr <= '9')) {
            ++digits_after_decimal;
            ++ptr;
        }
    }

    info.significant_digits = digits_before_decimal + digits_after_decimal;

    // Check for exponent
    if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
        info.has_exponent = true;
        ++ptr;

        bool exp_negative = false;
        if (ptr < end && (*ptr == '+' || *ptr == '-')) {
            exp_negative = (*ptr == '-');
            ++ptr;
        }

        // Saturates: only "past +-308" matters here, and an exponent of 99999999999 must not overflow an int.
        int exp_value = 0;
        constexpr int kExponentCap = 1000000;
        while (ptr < end && (*ptr >= '0' && *ptr <= '9')) {
            exp_value = exp_value >= kExponentCap ? kExponentCap : exp_value * 10 + (*ptr - '0');
            ++ptr;
        }

        info.exponent = exp_negative ? -exp_value : exp_value;
    }

    // Determine if this is an integer (no decimal point, no exponent)
    info.is_integer = !info.has_decimal && !info.has_exponent;

    // Determine if 128-bit precision is needed
    // For floats: > 15 significant digits (double precision limit)
    // For exponents: outside ±308 range (double range)
    // For integers: > 2^63 - 1 (int64_t limit) or > 2^64 - 1 (uint64_t limit)
    if (info.is_integer) {
        // Check if integer fits in 64-bit
        if (digits_before_decimal > 19) {
            // Definitely needs 128-bit
            info.needs_128bit = true;
        } else if (digits_before_decimal == 19) {
            // Might need 128-bit, check exact value later
            info.needs_128bit = true;
        }
    } else {
        // Float precision check. 17 significant digits identify any double uniquely, and every shortest
        // round-trip printer (Python repr, JS, std::to_chars) emits up to 17 -- e.g. 0.30000000000000004 -- so
        // only MORE digits than that carry precision a double cannot hold.
        if (info.significant_digits > 17) {
            info.needs_128bit = true;
        }
        if (info.has_exponent && (info.exponent > 308 || info.exponent < -308)) {
            info.needs_128bit = true;
        }
    }

    return info;
}

// Parse float128_compat from string using Clang's native support
inline auto parse_float128(const char* str, size_t length) -> std::optional<float128_compat> {
    if (length == 0 || length > 100) {
        return std::nullopt;
    }

    // Use strtold for parsing and convert to float128_compat
    // This preserves more precision than double
    thread_local std::array<char, 128> buffer;
    std::memcpy(buffer.data(), str, length);
    buffer[length] = '\0';

    char* endptr = nullptr;
    long double value = std::strtold(buffer.data(), &endptr);

    if (endptr != buffer.data() + length || !std::isfinite(value)) {
        return std::nullopt;
    }

    return static_cast<float128_compat>(value);
}

// Parse int128_compat from string manually
inline auto parse_int128(const char* str, size_t length, bool is_negative)
    -> std::optional<int128_compat> {
    if (length == 0 || length > 40) {  // Max 39 digits for int128_compat
        return std::nullopt;
    }

    uint128_compat value = 0;
    const char* ptr = str;
    const char* end = str + length;

    // Skip leading zeros
    while (ptr < end && *ptr == '0') {
        ++ptr;
    }

    // Parse digits
    while (ptr < end) {
        if (*ptr < '0' || *ptr > '9') {
            return std::nullopt;
        }

        uint128_compat digit = *ptr - '0';

        // Check for overflow
        uint128_compat old_value = value;
        value = value * 10 + digit;

        if (value < old_value) {
            return std::nullopt;  // Overflow
        }

        ++ptr;
    }

    if (is_negative) {
        // Check if value fits in signed int128_compat
        constexpr uint128_compat max_neg = static_cast<uint128_compat>(1) << 127;
        if (value > max_neg) {
            return std::nullopt;
        }
        // Negate in the UNSIGNED type, then convert (modular, well defined): negating the signed value overflows
        // for exactly -2^127, whose magnitude has no positive int128 form.
        return static_cast<int128_compat>(-value);
    } else {
        // Check if value fits in signed int128_compat
        constexpr uint128_compat max_pos = (static_cast<uint128_compat>(1) << 127) - 1;
        if (value > max_pos) {
            return std::nullopt;
        }
        return static_cast<int128_compat>(value);
    }
}

// Parse uint128_compat from string manually
inline auto parse_uint128(const char* str, size_t length) -> std::optional<uint128_compat> {
    if (length == 0 || length > 40) {  // Max 39 digits for uint128_compat
        return std::nullopt;
    }

    uint128_compat value = 0;
    const char* ptr = str;
    const char* end = str + length;

    // Skip leading zeros
    while (ptr < end && *ptr == '0') {
        ++ptr;
    }

    // Parse digits
    while (ptr < end) {
        if (*ptr < '0' || *ptr > '9') {
            return std::nullopt;
        }

        uint128_compat digit = *ptr - '0';

        // Check for overflow
        uint128_compat old_value = value;
        value = value * 10 + digit;

        if (value < old_value) {
            return std::nullopt;  // Overflow
        }

        ++ptr;
    }

    return value;
}

// Parser Class Definition
// ============================================================================

class parser {
public:
    explicit parser(std::string_view input);
    parser(std::string_view input, std::pmr::memory_resource* arena);
    auto parse() -> json_result<json_value>;

private:
    // Parsing methods
    auto parse_value() -> json_result<json_value>;
    auto parse_null() -> json_result<json_value>;
    auto parse_boolean() -> json_result<json_value>;
    auto parse_number() -> json_result<json_value>;
    auto parse_string() -> json_result<json_value>;
    auto parse_array() -> json_result<json_value>;
    auto parse_object() -> json_result<json_value>;

    // Helper methods
    auto skip_whitespace() -> void;
    auto skip_whitespace_simd() -> const char*;
    auto find_string_end_simd(const char* start) -> const char*;
    auto parse_string_simd() -> json_result<json_string_data>;
    auto peek() const noexcept -> char;
    auto advance() noexcept -> char;
    auto match(char expected) noexcept -> bool;
    auto is_at_end() const noexcept -> bool;
    auto make_error(json_error_code code, std::string message) const -> json_error;

    // Arena-aware allocation helpers
    [[nodiscard]] auto make_array_ptr(json_array&& arr) -> json_array_ptr;
    [[nodiscard]] auto make_object_ptr(json_object&& obj) -> json_object_ptr;

    // Member variables
    const char* data_;
    const char* end_;
    const char* current_;
    size_t line_;
    size_t column_;
    size_t depth_;
    std::pmr::memory_resource* arena_ = nullptr;  // nullptr = use default heap
    static constexpr size_t max_depth_ = 1000;
};

// ============================================================================
// json_document: owns input buffer + arena + root value
// Ensures string_view lifetimes are valid as long as the document exists.
// ============================================================================

class json_document {
    std::string input_;                                                  // Owned input (string_view lifetime source)
    std::unique_ptr<std::pmr::monotonic_buffer_resource> arena_;         // Heap-allocated arena (movable)
    json_value root_;

public:
    explicit json_document(std::string input, size_t arena_hint = 0)
        : input_(std::move(input)),
          arena_(std::make_unique<std::pmr::monotonic_buffer_resource>(
              arena_hint > 0 ? arena_hint : input_.size() * 2)),
          root_(nullptr) {}

    json_document(json_document&&) = default;
    json_document& operator=(json_document&&) = default;
    json_document(const json_document&) = delete;
    json_document& operator=(const json_document&) = delete;

    [[nodiscard]] auto root() const noexcept -> const json_value& { return root_; }
    [[nodiscard]] auto root() noexcept -> json_value& { return root_; }
    [[nodiscard]] auto input() const noexcept -> std::string_view { return input_; }
    [[nodiscard]] auto arena() noexcept -> std::pmr::memory_resource* { return arena_.get(); }

    // Allow parse_arena to set root_
    friend auto parse_arena(std::string input) -> json_result<json_document>;
};

// Thread-safe JSON Parser Implementation
// ============================================================================

parser::parser(std::string_view input)
    : data_(input.data()), end_(input.data() + input.size()), current_(input.data()), line_(1),
      column_(1), depth_(0), arena_(nullptr) {}

parser::parser(std::string_view input, std::pmr::memory_resource* arena)
    : data_(input.data()), end_(input.data() + input.size()), current_(input.data()), line_(1),
      column_(1), depth_(0), arena_(arena) {}

auto parser::make_array_ptr(json_array&& arr) -> json_array_ptr {
    if (arena_) {
        return std::allocate_shared<json_array>(
            std::pmr::polymorphic_allocator<json_array>(arena_), std::move(arr));
    }
    return std::make_shared<json_array>(std::move(arr));
}

auto parser::make_object_ptr(json_object&& obj) -> json_object_ptr {
    if (arena_) {
        return std::allocate_shared<json_object>(
            std::pmr::polymorphic_allocator<json_object>(arena_), std::move(obj));
    }
    return std::make_shared<json_object>(std::move(obj));
}

auto parser::parse() -> json_result<json_value> {
    skip_whitespace();
    if (is_at_end()) {
        return std::unexpected(make_error(json_error_code::empty_input, "Empty input"));
    }

    auto result = parse_value();
    if (!result) {
        return result;
    }

    skip_whitespace();
    if (!is_at_end()) {
        return std::unexpected(
            make_error(json_error_code::extra_tokens, "Unexpected characters after JSON value"));
    }

    return result;
}

auto parser::parse_value() -> json_result<json_value> {
    if (depth_ >= max_depth_) {
        return std::unexpected(
            make_error(json_error_code::max_depth_exceeded, "Maximum nesting depth exceeded"));
    }

    skip_whitespace();

    if (is_at_end()) {
        return std::unexpected(
            make_error(json_error_code::unexpected_end, "Unexpected end of input"));
    }

    char c = peek();

    switch (c) {
        case 'n':
            return parse_null();
        case 't':
        case 'f':
            return parse_boolean();
        case '"':
            return parse_string();
        case '[':
            return parse_array();
        case '{':
            return parse_object();
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return parse_number();
        default:
            return std::unexpected(make_error(json_error_code::invalid_syntax,
                                              "Unexpected character: " + std::string(1, c)));
    }
}

auto parser::parse_null() -> json_result<json_value> {
    if (!match('n') || !match('u') || !match('l') || !match('l')) {
        return std::unexpected(
            make_error(json_error_code::invalid_literal, "Invalid null literal"));
    }
    return json_value{};
}

auto parser::parse_boolean() -> json_result<json_value> {
    if (match('t')) {
        if (!match('r') || !match('u') || !match('e')) {
            return std::unexpected(
                make_error(json_error_code::invalid_literal, "Invalid true literal"));
        }
        return json_value{true};
    } else if (match('f')) {
        if (!match('a') || !match('l') || !match('s') || !match('e')) {
            return std::unexpected(
                make_error(json_error_code::invalid_literal, "Invalid false literal"));
        }
        return json_value{false};
    }

    return std::unexpected(make_error(json_error_code::invalid_literal, "Invalid boolean literal"));
}

auto parser::parse_number() -> json_result<json_value> {
    const char* start = current_;

    // Handle negative sign
    if (peek() == '-') {
        advance();
    }

    // Handle integer part
    if (peek() == '0') {
        advance();
    } else if (peek() >= '1' && peek() <= '9') {
        advance();
        while (peek() >= '0' && peek() <= '9') {
            advance();
        }
    } else {
        return std::unexpected(
            make_error(json_error_code::invalid_number, "Invalid number format"));
    }

    // Handle fractional part
    bool plain_integer = true;
    if (peek() == '.') {
        plain_integer = false;
        advance();
        if (!(peek() >= '0' && peek() <= '9')) {
            return std::unexpected(
                make_error(json_error_code::invalid_number, "Invalid decimal number"));
        }
        while (peek() >= '0' && peek() <= '9') {
            advance();
        }
    }

    // Handle exponent
    if (peek() == 'e' || peek() == 'E') {
        plain_integer = false;
        advance();
        if (peek() == '+' || peek() == '-') {
            advance();
        }
        if (!(peek() >= '0' && peek() <= '9')) {
            return std::unexpected(make_error(json_error_code::invalid_number, "Invalid exponent"));
        }
        while (peek() >= '0' && peek() <= '9') {
            advance();
        }
    }

    // INTEGER FAST PATH. A token of at most 15 digits (and an optional '-') with no fraction or exponent is below 2^53,
    // so the double is EXACT and the strtod + to_chars round trip below can only ever hand back the same value: the
    // grammar above forbids leading zeros, so the digits are what `to_chars(..., fixed)` would print, and "-0" yields
    // -0.0 from both routes. Streams carry such numbers on every delta (`created`, `index`, token counts), and the
    // round trip was ~4% of parse time. Anything longer takes the unchanged path below.
    if (plain_integer) {
        const char* digits = start;
        const bool negative = (*digits == '-');
        if (negative) ++digits;
        const size_t digit_count = static_cast<size_t>(current_ - digits);
        if (digit_count <= 15) {
            std::uint64_t acc = 0;
            for (const char* d = digits; d < current_; ++d) {
                acc = acc * 10U + static_cast<std::uint64_t>(*d - '0');
            }
            const double exact = static_cast<double>(acc);
            return json_value{negative ? -exact : exact};
        }
    }

    // Analyze precision requirements
    size_t length = current_ - start;
    auto precision_info = analyze_number_precision(start, current_);

    // Adaptive precision parsing strategy:
    // 1. Try 64-bit parsing first (fast path)
    // 2. If precision info indicates need for 128-bit, or if 64-bit parsing loses precision,
    // upgrade
    // 3. If 128-bit parsing also fails, return NaN

    if (!precision_info.needs_128bit) {
        // Fast path: Try 64-bit parsing first
        thread_local std::array<char, 64> buffer;
        if (length < buffer.size()) {
            std::memcpy(buffer.data(), start, length);
            buffer[length] = '\0';

            char* end_ptr;
            double value = std::strtod(buffer.data(), &end_ptr);

            // An overflow is not a value: strtod's +-HUGE_VAL goes to the 128-bit reader below (which holds
            // exponents far past 308) instead of being stored as infinity.
            if (end_ptr == buffer.data() + length && std::isfinite(value)) {
                // Verify no precision loss for integers
                if (precision_info.is_integer) {
                    // For integers, check if value exactly represents the parsed number
                    // by converting back to string and comparing
                    thread_local std::array<char, 32> verify_buffer;
                    auto [ptr, ec] = std::to_chars(verify_buffer.data(),
                                                   verify_buffer.data() + verify_buffer.size(),
                                                   value, std::chars_format::fixed);

                    if (ec == std::errc{}) {
                        std::string_view original(start, length);
                        std::string_view converted(verify_buffer.data(),
                                                   ptr - verify_buffer.data());

                        // If they match, no precision loss
                        if (original == converted) {
                            return json_value{value};
                        }
                        // Precision loss detected, upgrade to 128-bit
                    } else {
                        // Conversion failed, upgrade to 128-bit
                    }
                } else {
                    // For floats with <= 17 significant digits and exponent in range,
                    // 64-bit is sufficient
                    return json_value{value};
                }
            }
        }
    }

    // Slow path: 128-bit parsing required
    if (precision_info.is_integer) {
        // Try 128-bit integer parsing
        const char* num_start = start;
        if (*num_start == '-') {
            ++num_start;
        }

        auto result = parse_int128(num_start, current_ - num_start, precision_info.is_negative);
        if (result) {
            return json_value{*result};
        }

        // If signed parsing failed, try unsigned for positive numbers
        if (!precision_info.is_negative) {
            auto uresult = parse_uint128(num_start, current_ - num_start);
            if (uresult) {
                return json_value{*uresult};
            }
        }
    } else {
        // Try 128-bit float parsing
        auto result = parse_float128(start, length);
        if (result) {
            return json_value{*result};
        }
    }

    // All parsing attempts failed. A number no representation can hold is an ERROR -- never a NaN or an
    // infinity standing in for it (JSON has neither, and a caller cannot tell the substitute from data).
    if (precision_info.is_integer) {
        return std::unexpected(
            make_error(json_error_code::invalid_number, "Integer value exceeds 128-bit range"));
    }
    return std::unexpected(
        make_error(json_error_code::invalid_number, "Number is outside the representable range"));
}

auto parser::parse_string() -> json_result<json_value> {
    if (!match('"')) {
        return std::unexpected(
            make_error(json_error_code::invalid_string, "Expected opening quote"));
    }

    // Use SIMD-optimized string parsing if available
    auto result = parse_string_simd();
    if (result) {
        return json_value{std::move(*result)};
    }

    // Fallback to scalar parsing
    std::string value;
    value.reserve(64);  // Pre-allocate reasonable size

    while (!is_at_end() && peek() != '"') {
        // BULK-COPY THE RUN UP TO THE NEXT QUOTE, BACKSLASH OR CONTROL BYTE. `parse_string_simd` above only takes a string
        // with NO escape at all, so every string that has even one (a tool call's arguments are JSON inside JSON: a
        // backslash every few bytes; source code and diffs carry `\n` throughout) used to be decoded one `push_back` per
        // byte, ~10 ns/byte -- 91% of the time of parsing a 1 MiB argument delta in `perf`. The vector scan that already
        // found the string end finds the end of each clean run, and the run is appended in one call. A clean run holds no
        // newline (`\n` < 0x20 is a stop byte), so the column advances by its length and the line not at all, exactly what
        // `advance()` would have done byte by byte; the stop byte itself is still handled by the per-byte code below, so
        // every escape and every error is decoded and reported where it was.
        if (const char* stop = find_string_end_simd(current_); stop != current_) {
            // RFC 8259 section 8.1: JSON text is UTF-8. A run ends only at an ASCII stop byte, so a multi-byte
            // sequence never straddles two runs and each run can be checked on its own.
            if (!detail::utf8_valid(std::string_view{current_, static_cast<size_t>(stop - current_)})) {
                return std::unexpected(
                    make_error(json_error_code::invalid_unicode, "Invalid UTF-8 in string"));
            }
            value.append(current_, static_cast<size_t>(stop - current_));
            column_ += static_cast<size_t>(stop - current_);
            current_ = stop;
            continue;
        }
        char c = advance();

        if (c == '\\') {
            if (is_at_end()) {
                return std::unexpected(
                    make_error(json_error_code::invalid_string, "Unterminated escape sequence"));
            }

            char escaped = advance();
            switch (escaped) {
                case '"':
                    value += '"';
                    break;
                case '\\':
                    value += '\\';
                    break;
                case '/':
                    value += '/';
                    break;
                case 'b':
                    value += '\b';
                    break;
                case 'f':
                    value += '\f';
                    break;
                case 'n':
                    value += '\n';
                    break;
                case 'r':
                    value += '\r';
                    break;
                case 't':
                    value += '\t';
                    break;
                case 'u': {
                    // \uXXXX, or a surrogate pair \uD83D\uDE00 as ONE code point (4-byte UTF-8). A lone or
                    // mismatched surrogate has no UTF-8 form and is an error (it used to become CESU-8 bytes).
                    if (current_ + 4 > end_) {
                        return std::unexpected(make_error(json_error_code::invalid_string,
                                                          "Incomplete Unicode escape"));
                    }
                    const auto decoded = unicode::parse_unicode_escape(
                        current_, static_cast<size_t>(end_ - current_));
                    if (!decoded.success && decoded.bytes_consumed == 0) {
                        // A bad hex digit: reported where it always was -- just past the first non-hex byte.
                        for (int k = 0; k < 4; ++k) {
                            if (unicode::parse_hex_digit(advance()) < 0) {
                                break;
                            }
                        }
                        return std::unexpected(make_error(json_error_code::invalid_string,
                                                          "Invalid Unicode escape"));
                    }
                    for (int k = 0; k < 4; ++k) {
                        advance();
                    }
                    if (!decoded.success || !unicode::encode_utf8(decoded.codepoint, value)) {
                        return std::unexpected(make_error(json_error_code::invalid_unicode, decoded.error));
                    }
                    for (int k = 4; k < decoded.bytes_consumed; ++k) {
                        advance();
                    }
                    break;
                }
                default:
                    return std::unexpected(
                        make_error(json_error_code::invalid_string, "Invalid escape sequence"));
            }
        } else if (static_cast<unsigned char>(c) < 0x20) {
            return std::unexpected(
                make_error(json_error_code::invalid_string, "Control character in string"));
        } else if (static_cast<unsigned char>(c) >= 0x80) {
            // Not reached through a scan run (no tier stops on a byte >= 0x80), but never appended unchecked.
            const char* const lead = current_ - 1;
            const std::string_view rest{lead, static_cast<size_t>(end_ - lead)};
            const size_t n = detail::utf8_sequence_length(rest, 0);
            if (n == 0) {
                return std::unexpected(
                    make_error(json_error_code::invalid_unicode, "Invalid UTF-8 in string"));
            }
            value.append(lead, n);
            for (size_t k = 1; k < n; ++k) {
                advance();
            }
        } else {
            value += c;
        }
    }

    if (!match('"')) {
        return std::unexpected(make_error(json_error_code::invalid_string, "Unterminated string"));
    }

    return json_value{std::move(value)};
}

auto parser::parse_array() -> json_result<json_value> {
    if (!match('[')) {
        return std::unexpected(make_error(json_error_code::invalid_syntax, "Expected '['"));
    }

    ++depth_;
    json_array array;

    skip_whitespace();

    // Handle empty array
    if (match(']')) {
        --depth_;
        return json_value{make_array_ptr(std::move(array))};
    }

    // Parse array elements
    while (true) {
        auto element = parse_value();
        if (!element) {
            --depth_;
            return std::unexpected(element.error());
        }

        array.emplace_back(std::move(*element));

        skip_whitespace();

        if (match(']')) {
            break;
        } else if (match(',')) {
            skip_whitespace();
            continue;
        } else {
            --depth_;
            return std::unexpected(
                make_error(json_error_code::invalid_syntax, "Expected ',' or ']' in array"));
        }
    }

    --depth_;
    return json_value{make_array_ptr(std::move(array))};
}

auto parser::parse_object() -> json_result<json_value> {
    if (!match('{')) {
        return std::unexpected(make_error(json_error_code::invalid_syntax, "Expected '{'"));
    }

    ++depth_;
    json_object object;

    skip_whitespace();

    // Handle empty object
    if (match('}')) {
        --depth_;
        return json_value{make_object_ptr(std::move(object))};
    }

    // Parse object members
    while (true) {
        skip_whitespace();

        // Parse key
        if (peek() != '"') {
            --depth_;
            return std::unexpected(
                make_error(json_error_code::invalid_syntax, "Expected string key in object"));
        }

        auto key_result = parse_string();
        if (!key_result) {
            --depth_;
            return std::unexpected(key_result.error());
        }

        std::string key(key_result->as_string());

        skip_whitespace();

        // Expect colon
        if (!match(':')) {
            --depth_;
            return std::unexpected(
                make_error(json_error_code::invalid_syntax, "Expected ':' after object key"));
        }

        // Parse value
        auto value_result = parse_value();
        if (!value_result) {
            --depth_;
            return std::unexpected(value_result.error());
        }

        object[std::move(key)] = std::move(*value_result);

        skip_whitespace();

        if (match('}')) {
            break;
        } else if (match(',')) {
            skip_whitespace();
            continue;
        } else {
            --depth_;
            return std::unexpected(
                make_error(json_error_code::invalid_syntax, "Expected ',' or '}' in object"));
        }
    }

    --depth_;
    return json_value{make_object_ptr(std::move(object))};
}

auto parser::skip_whitespace() -> void {
    // THE COMMON CASE IS NO WHITESPACE AT ALL. Compact JSON (every API stream, every wire delta) calls this between
    // every pair of tokens and finds nothing to skip; a vector kernel dispatched through a runtime-capability branch
    // and a PLT stub costs an order of magnitude more than the one byte test that decides there is no work. Measured
    // on OpenAI-shaped stream deltas this call was 9.6% of total parse time (perf, AVX-512 host). A byte that is not
    // one of the four JSON whitespace characters leaves `current_` where it is, exactly as the kernel's result would.
    if (current_ >= end_) return;
    const auto first = static_cast<unsigned char>(*current_);
    if (first != ' ' && first != '\t' && first != '\n' && first != '\r') return;
    size_t remaining = end_ - current_;
    const char* new_pos = ::fastjson::skip_whitespace_simd(current_, remaining);

    // Update line and column tracking
    while (current_ < new_pos) {
        if (*current_ == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        ++current_;
    }
}

auto parser::peek() const noexcept -> char {
    return is_at_end() ? '\0' : *current_;
}

auto parser::advance() noexcept -> char {
    if (is_at_end()) {
        return '\0';
    }

    char c = *current_++;
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }

    return c;
}

auto parser::match(char expected) noexcept -> bool {
    if (peek() == expected) {
        advance();
        return true;
    }
    return false;
}

auto parser::is_at_end() const noexcept -> bool {
    return current_ >= end_;
}

auto parser::make_error(json_error_code code, std::string message) const -> json_error {
    return json_error{
        .code = code, .message = std::move(message), .line = line_, .column = column_};
}

auto parser::skip_whitespace_simd() -> const char* {
    size_t remaining = end_ - current_;
    const char* new_pos = ::fastjson::skip_whitespace_simd(current_, remaining);
    return new_pos;
}

// find_string_end_simd and parse_string_simd — delegate to GMF detail:: implementation
auto parser::find_string_end_simd(const char* start) -> const char* {
    return detail::find_string_end_simd_impl(start, end_);
}

auto parser::parse_string_simd() -> json_result<json_string_data> {
    const char* start = current_;
    const char* string_end = find_string_end_simd(start);

    // Fast path: found closing quote with no escapes -- ONE bulk copy instead of the per-character loop.
    // OWNED, never a view into the input: `parse(std::string_view)` cannot know how long the caller keeps the
    // buffer, and `auto v = parse(readFile(p));` destroys it at the end of the statement. This path never ran
    // while the AVX2 scan's control limit was wrong; a view here made every unescaped string dangle the moment
    // it did. Zero-copy views belong to the explicit ondemand API, whose caller owns the buffer's lifetime.
    if (string_end < end_ && *string_end == '"'
        && std::find(start, string_end, '\\') == string_end
        && detail::utf8_valid(std::string_view{start, static_cast<size_t>(string_end - start)})) {
        json_string_data result(std::string(start, static_cast<size_t>(string_end - start)));
        // Skip the string and its closing quote. The column counts them as advance() would have (a run holds no
        // newline: control bytes stop the scan), so an error after the string reports the column it is at.
        column_ += static_cast<size_t>(string_end - start) + 1;
        current_ = string_end + 1;
        return result;
    }

    // Slow path: has escapes or control characters, fall back to regular parsing
    return std::unexpected(json_error{});
}

// Convenience Functions Implementation
// ============================================================================

auto parse(std::string_view input) -> json_result<json_value> {
    parser p(input);
    return p.parse();
}

// Arena-based parsing: all allocations from a single monotonic arena.
// Returns json_document that owns the arena, input buffer, and root value.
// string_view lifetimes valid as long as json_document exists.
auto parse_arena(std::string input) -> json_result<json_document> {
    json_document doc(std::move(input));
    parser p(doc.input(), doc.arena());
    auto result = p.parse();
    if (!result) {
        return std::unexpected(result.error());
    }
    doc.root_ = std::move(*result);
    return doc;
}

// ============================================================================
// Ondemand/Lazy JSON Parsing — Two-Stage: SIMD structural index + lazy access
// Only materializes values on explicit get_*() calls. Zero-copy for strings.
// ============================================================================

enum class json_type : uint8_t {
    null_value, boolean_value, number_value,
    string_value, array_value, object_value
};

class ondemand_value;
class ondemand_object;
class ondemand_array;

class ondemand_document {
    std::string input_;
    std::vector<structural_index> tape_;
    size_t tape_pos_ = 0;

public:
    static auto parse(std::string input) -> json_result<ondemand_document>;
    [[nodiscard]] auto root() -> ondemand_value;
    [[nodiscard]] auto materialize() -> json_result<json_value>;
    [[nodiscard]] auto input() const noexcept -> std::string_view { return input_; }
    [[nodiscard]] auto tape_size() const noexcept -> size_t { return tape_.size(); }

    // Internal access for ondemand_value/object/array
    [[nodiscard]] auto raw_input() const noexcept -> const char* { return input_.data(); }
    [[nodiscard]] auto tape() const noexcept -> const std::vector<structural_index>& { return tape_; }
    auto set_tape_pos(size_t pos) noexcept -> void { tape_pos_ = pos; }
    [[nodiscard]] auto tape_pos() const noexcept -> size_t { return tape_pos_; }
    [[nodiscard]] auto input_size() const noexcept -> size_t { return input_.size(); }
};

class ondemand_value {
    ondemand_document* doc_;
    size_t tape_start_;

public:
    ondemand_value(ondemand_document* doc, size_t tape_pos) noexcept
        : doc_(doc), tape_start_(tape_pos) {}

    [[nodiscard]] auto type() const noexcept -> json_type;
    [[nodiscard]] auto get_string() const -> json_result<std::string_view>;
    [[nodiscard]] auto get_double() const -> json_result<double>;
    [[nodiscard]] auto get_int64() const -> json_result<int64_t>;
    [[nodiscard]] auto get_bool() const -> json_result<bool>;
    [[nodiscard]] auto is_null() const noexcept -> bool;
    [[nodiscard]] auto get_object() const -> json_result<ondemand_object>;
    [[nodiscard]] auto get_array() const -> json_result<ondemand_array>;

    // Skip past this value in the tape (for iteration)
    [[nodiscard]] auto skip() const -> size_t;
};

class ondemand_object {
    ondemand_document* doc_;
    size_t tape_start_;  // Points to '{'

public:
    ondemand_object(ondemand_document* doc, size_t tape_pos) noexcept
        : doc_(doc), tape_start_(tape_pos) {}

    [[nodiscard]] auto find_field(std::string_view key) const -> json_result<ondemand_value>;
    [[nodiscard]] auto operator[](std::string_view key) const -> json_result<ondemand_value>;
};

class ondemand_array {
    ondemand_document* doc_;
    size_t tape_start_;  // Points to '['

public:
    ondemand_array(ondemand_document* doc, size_t tape_pos) noexcept
        : doc_(doc), tape_start_(tape_pos) {}

    [[nodiscard]] auto count_elements() const -> size_t;

    // Simple indexed access
    [[nodiscard]] auto at(size_t index) const -> json_result<ondemand_value>;
};

// ============================================================================
// Ondemand Implementation
// ============================================================================

auto ondemand_document::parse(std::string input) -> json_result<ondemand_document> {
    ondemand_document doc;
    doc.input_ = std::move(input);
    // The lazy accessors walk a structural tape that assumes a well-formed document; indexing a malformed one
    // ("[1,", "[1]]", an unterminated string, invalid UTF-8) used to succeed and hand out values from it. The
    // input is validated by THE grammar (the same parser, into a scratch arena that is dropped at once).
    {
        std::pmr::monotonic_buffer_resource scratch;
        parser check(doc.input_, &scratch);
        if (auto valid = check.parse(); !valid) {
            return std::unexpected(valid.error());
        }
    }
    doc.tape_ = build_structural_index(
        std::span<const char>(doc.input_.data(), doc.input_.size()));
    if (doc.tape_.empty()) {
        return std::unexpected(json_error{json_error_code::empty_input, "Empty or whitespace-only input", 0, 0});
    }
    return doc;
}

auto ondemand_document::root() -> ondemand_value {
    return ondemand_value(this, 0);
}

auto ondemand_document::materialize() -> json_result<json_value> {
    return fastjson::parse(input_);
}

// Helper: skip to the matching closing bracket/brace in the structural tape
inline auto skip_container(const std::vector<structural_index>& tape,
                           size_t start_pos) -> size_t {
    auto open_type = tape[start_pos].type;
    structural_type close_type;
    if (open_type == structural_type::left_brace)
        close_type = structural_type::right_brace;
    else if (open_type == structural_type::left_bracket)
        close_type = structural_type::right_bracket;
    else
        return start_pos + 1;  // Not a container

    int depth = 1;
    size_t pos = start_pos + 1;
    while (pos < tape.size() && depth > 0) {
        if (tape[pos].type == open_type) ++depth;
        else if (tape[pos].type == close_type) --depth;
        ++pos;
    }
    return pos;  // One past the closing bracket
}

auto ondemand_value::type() const noexcept -> json_type {
    if (tape_start_ >= doc_->tape().size()) return json_type::null_value;
    auto t = doc_->tape()[tape_start_].type;
    switch (t) {
        case structural_type::left_brace: return json_type::object_value;
        case structural_type::left_bracket: return json_type::array_value;
        case structural_type::quote: return json_type::string_value;
        case structural_type::null_start: return json_type::null_value;
        case structural_type::true_start:
        case structural_type::false_start: return json_type::boolean_value;
        case structural_type::number_start: return json_type::number_value;
        default: break;
    }
    // Check if the character at the position is a number
    size_t pos = doc_->tape()[tape_start_].position;
    if (pos < doc_->input_size()) {
        char c = doc_->raw_input()[pos];
        if (c == '-' || (c >= '0' && c <= '9')) return json_type::number_value;
    }
    return json_type::null_value;
}

auto ondemand_value::get_string() const -> json_result<std::string_view> {
    if (tape_start_ >= doc_->tape().size()) {
        return std::unexpected(json_error{json_error_code::invalid_string, "Invalid tape position", 0, 0});
    }
    if (doc_->tape()[tape_start_].type != structural_type::quote) {
        return std::unexpected(json_error{json_error_code::invalid_string, "Value is not a string", 0, 0});
    }
    size_t start = doc_->tape()[tape_start_].position + 1;  // Skip opening quote
    // Find closing quote in tape
    if (tape_start_ + 1 < doc_->tape().size() &&
        doc_->tape()[tape_start_ + 1].type == structural_type::quote) {
        size_t end = doc_->tape()[tape_start_ + 1].position;
        return std::string_view(doc_->raw_input() + start, end - start);
    }
    // Fallback: scan for closing quote
    const char* ptr = doc_->raw_input() + start;
    const char* input_end = doc_->raw_input() + doc_->input_size();
    while (ptr < input_end && *ptr != '"') {
        if (*ptr == '\\') ++ptr;  // Skip escaped char
        ++ptr;
    }
    return std::string_view(doc_->raw_input() + start, ptr - (doc_->raw_input() + start));
}

auto ondemand_value::get_double() const -> json_result<double> {
    if (tape_start_ >= doc_->tape().size()) {
        return std::unexpected(json_error{json_error_code::invalid_number, "Invalid tape position", 0, 0});
    }
    size_t pos = doc_->tape()[tape_start_].position;
    const char* start = doc_->raw_input() + pos;
    char* end_ptr = nullptr;
    double val = std::strtod(start, &end_ptr);
    if (end_ptr == start) {
        return std::unexpected(json_error{json_error_code::invalid_number, "Not a number", 0, 0});
    }
    return val;
}

auto ondemand_value::get_int64() const -> json_result<int64_t> {
    if (tape_start_ >= doc_->tape().size()) {
        return std::unexpected(json_error{json_error_code::invalid_number, "Invalid tape position", 0, 0});
    }
    size_t pos = doc_->tape()[tape_start_].position;
    const char* start = doc_->raw_input() + pos;
    char* end_ptr = nullptr;
    long long val = std::strtoll(start, &end_ptr, 10);
    if (end_ptr == start) {
        return std::unexpected(json_error{json_error_code::invalid_number, "Not an integer", 0, 0});
    }
    return static_cast<int64_t>(val);
}

auto ondemand_value::get_bool() const -> json_result<bool> {
    if (tape_start_ >= doc_->tape().size()) {
        return std::unexpected(json_error{json_error_code::invalid_syntax, "Invalid tape position", 0, 0});
    }
    auto t = doc_->tape()[tape_start_].type;
    if (t == structural_type::true_start) return true;
    if (t == structural_type::false_start) return false;
    return std::unexpected(json_error{json_error_code::invalid_syntax, "Value is not a boolean", 0, 0});
}

auto ondemand_value::is_null() const noexcept -> bool {
    if (tape_start_ >= doc_->tape().size()) return true;
    return doc_->tape()[tape_start_].type == structural_type::null_start;
}

auto ondemand_value::get_object() const -> json_result<ondemand_object> {
    if (tape_start_ >= doc_->tape().size() ||
        doc_->tape()[tape_start_].type != structural_type::left_brace) {
        return std::unexpected(json_error{json_error_code::invalid_syntax, "Value is not an object", 0, 0});
    }
    return ondemand_object(doc_, tape_start_);
}

auto ondemand_value::get_array() const -> json_result<ondemand_array> {
    if (tape_start_ >= doc_->tape().size() ||
        doc_->tape()[tape_start_].type != structural_type::left_bracket) {
        return std::unexpected(json_error{json_error_code::invalid_syntax, "Value is not an array", 0, 0});
    }
    return ondemand_array(doc_, tape_start_);
}

auto ondemand_value::skip() const -> size_t {
    if (tape_start_ >= doc_->tape().size()) return tape_start_;
    auto t = doc_->tape()[tape_start_].type;
    if (t == structural_type::left_brace || t == structural_type::left_bracket) {
        return skip_container(doc_->tape(), tape_start_);
    }
    if (t == structural_type::quote) {
        // Skip opening + closing quote pair
        return tape_start_ + 2;
    }
    // Primitive (null, true, false, number) = 1 tape entry
    return tape_start_ + 1;
}

auto ondemand_object::find_field(std::string_view key) const -> json_result<ondemand_value> {
    const auto& tape = doc_->tape();
    const char* input = doc_->raw_input();
    size_t input_size = doc_->input_size();

    // Start after the opening brace
    size_t pos = tape_start_ + 1;

    while (pos < tape.size() && tape[pos].type != structural_type::right_brace) {
        // Expect a string key (quote)
        if (tape[pos].type != structural_type::quote) {
            ++pos;
            continue;
        }

        // Extract the key
        size_t key_start = tape[pos].position + 1;
        size_t key_end = key_start;
        if (pos + 1 < tape.size() && tape[pos + 1].type == structural_type::quote) {
            key_end = tape[pos + 1].position;
        } else {
            // Scan for closing quote
            const char* ptr = input + key_start;
            const char* end = input + input_size;
            while (ptr < end && *ptr != '"') {
                if (*ptr == '\\') ++ptr;
                ++ptr;
            }
            key_end = ptr - input;
        }

        std::string_view field_key(input + key_start, key_end - key_start);

        // Skip past closing quote + colon
        pos += 2;  // Past both quote markers
        // Skip colon
        while (pos < tape.size() && tape[pos].type == structural_type::colon) ++pos;

        // Now pos points to the value
        if (field_key == key) {
            return ondemand_value(doc_, pos);
        }

        // Skip the value to get to the next field
        ondemand_value val(doc_, pos);
        pos = val.skip();

        // Skip comma
        while (pos < tape.size() && tape[pos].type == structural_type::comma) ++pos;
    }

    return std::unexpected(json_error{json_error_code::invalid_syntax,
                                       "Field not found: " + std::string(key), 0, 0});
}

auto ondemand_object::operator[](std::string_view key) const -> json_result<ondemand_value> {
    return find_field(key);
}

auto ondemand_array::count_elements() const -> size_t {
    const auto& tape = doc_->tape();
    size_t count = 0;
    size_t pos = tape_start_ + 1;

    if (pos >= tape.size() || tape[pos].type == structural_type::right_bracket) {
        return 0;
    }

    while (pos < tape.size() && tape[pos].type != structural_type::right_bracket) {
        ++count;
        ondemand_value val(doc_, pos);
        pos = val.skip();
        // Skip comma
        while (pos < tape.size() && tape[pos].type == structural_type::comma) ++pos;
    }
    return count;
}

auto ondemand_array::at(size_t index) const -> json_result<ondemand_value> {
    const auto& tape = doc_->tape();
    size_t pos = tape_start_ + 1;
    size_t current = 0;

    while (pos < tape.size() && tape[pos].type != structural_type::right_bracket) {
        if (current == index) {
            return ondemand_value(doc_, pos);
        }
        ondemand_value val(doc_, pos);
        pos = val.skip();
        // Skip comma
        while (pos < tape.size() && tape[pos].type == structural_type::comma) ++pos;
        ++current;
    }

    return std::unexpected(json_error{json_error_code::invalid_syntax, "Array index out of range", 0, 0});
}

// Convenience function for ondemand parsing
auto parse_ondemand(std::string input) -> json_result<ondemand_document> {
    return ondemand_document::parse(std::move(input));
}

auto object() -> json_value {
    return json_value{json_object{}};
}

auto array() -> json_value {
    return json_value{json_array{}};
}

auto null() -> json_value {
    return json_value{};
}

auto stringify(const json_value& value) -> std::string {
    return value.to_string();
}

auto prettify(const json_value& value, int indent) -> std::string {
    return value.to_pretty_string(indent);
}

// ============================================================================
// Writing JSON text without a json_value: escape_string, to_json, writer.
// Everything that WRITES JSON uses these (or json_value::to_string), never hand-built text: the escaping and the
// number formatting below are the ones to_string() uses (detail::append_json_string / append_double).
// ============================================================================

/// `text` as a quoted, escaped JSON string. Refuses text that is not UTF-8 (JSON text must be).
[[nodiscard]] auto escape_string(std::string_view text) -> json_result<std::string> {
    if (!detail::utf8_valid(text)) {
        return std::unexpected(json_error{json_error_code::invalid_unicode, "escape_string: text is not UTF-8", 0, 0});
    }
    std::string out;
    out.reserve(text.size() + 2);
    detail::append_json_string(out, text);
    return out;
}

namespace writer_detail {
// A value is writable as JSON only if every number is finite and every string and key is UTF-8.
[[nodiscard]] inline auto writable(const json_value& v) -> std::optional<json_error> {
    if (v.is_string()) {
        if (!detail::utf8_valid(v.as_string())) {
            return json_error{json_error_code::invalid_unicode, "to_json: a string is not UTF-8", 0, 0};
        }
    } else if (v.is_number()) {
        if (!std::isfinite(static_cast<long double>(v.as_float128()))) {
            return json_error{json_error_code::invalid_number, "to_json: a number is NaN or infinite", 0, 0};
        }
    } else if (v.is_array()) {
        for (const auto& item : v.as_array()) {
            if (auto bad = writable(item)) { return bad; }
        }
    } else if (v.is_object()) {
        for (const auto& [key, item] : v.as_object()) {
            if (!detail::utf8_valid(key)) {
                return json_error{json_error_code::invalid_unicode, "to_json: a key is not UTF-8", 0, 0};
            }
            if (auto bad = writable(item)) { return bad; }
        }
    }
    return std::nullopt;
}
}  // namespace writer_detail

/// The checked form of to_string(): the same text, or an error when the value has no JSON form (a NaN or
/// infinite number, which to_string() writes as `null`; a string or key that is not UTF-8).
[[nodiscard]] auto to_json(const json_value& value) -> json_result<std::string> {
    if (auto bad = writer_detail::writable(value)) {
        return std::unexpected(std::move(bad).value());
    }
    return value.to_string();
}

/// A streaming JSON writer: members and elements are written in CALL ORDER (a json_object is a hash map and
/// keeps none), straight into one string, with the escaping and number formatting to_string() uses.
///
///     auto text = fastjson::writer{}
///                     .begin_object()
///                     .key("model").value(name)
///                     .key("messages").begin_array().raw(message.to_string()).end_array()
///                     .end_object()
///                     .finish();          // json_result<std::string>
///
/// Fail fast, no partial output: the first call that cannot be part of ONE well-formed JSON value (a value in
/// an object without a key, two keys in a row, a mismatched or missing close, a second top-level value, a NaN or
/// infinite number, text that is not UTF-8, raw() text that is not exactly one JSON value) is recorded, every
/// later call is ignored, and finish() returns that error instead of text.
class writer {
public:
    writer() = default;

    template <class Self> auto begin_object(this Self&& self) -> Self&& {
        self.open(frame_kind::object, '{');
        return std::forward<Self>(self);
    }
    template <class Self> auto end_object(this Self&& self) -> Self&& {
        self.close(frame_kind::object, '}');
        return std::forward<Self>(self);
    }
    template <class Self> auto begin_array(this Self&& self) -> Self&& {
        self.open(frame_kind::array, '[');
        return std::forward<Self>(self);
    }
    template <class Self> auto end_array(this Self&& self) -> Self&& {
        self.close(frame_kind::array, ']');
        return std::forward<Self>(self);
    }
    /// The next member's name; must be followed by exactly one value (or container).
    template <class Self> auto key(this Self&& self, std::string_view name) -> Self&& {
        self.write_key(name);
        return std::forward<Self>(self);
    }
    template <class Self> auto value(this Self&& self, std::string_view text) -> Self&& {
        self.write_string(text);
        return std::forward<Self>(self);
    }
    /// A C string. A null pointer is refused (it is not text; write value(nullptr) for JSON null).
    template <class Self> auto value(this Self&& self, const char* text) -> Self&& {
        if (text == nullptr) {
            self.fail("writer: a null const char* is not a string (use value(nullptr) for null)");
        } else {
            self.write_string(std::string_view{text});
        }
        return std::forward<Self>(self);
    }
    /// A char is ambiguous (a one-character string or a code unit as a number): say which, value(std::string_view
    /// {&c, 1}) or value(int{c}). Refused at compile time rather than guessed.
    template <class Self> auto value(this Self&& self, char) -> Self&& = delete;
    template <class Self> auto value(this Self&& self, std::nullptr_t) -> Self&& {
        self.write_scalar("null");
        return std::forward<Self>(self);
    }
    template <class Self, class B>
        requires std::same_as<B, bool>
    auto value(this Self&& self, B flag) -> Self&& {
        self.write_scalar(flag ? "true" : "false");
        return std::forward<Self>(self);
    }
    template <class Self, std::integral I>
        requires(!std::same_as<I, bool> && !std::same_as<I, char> && !std::same_as<I, char8_t>)
    auto value(this Self&& self, I number) -> Self&& {
        std::array<char, 48> buf{};  // a 128-bit integer has at most 40 characters
        const auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), number);
        if (ec != std::errc{}) {
            self.fail("writer: an integer could not be formatted", json_error_code::invalid_number);
        } else {
            self.write_scalar(std::string_view{buf.data(), static_cast<size_t>(end - buf.data())});
        }
        return std::forward<Self>(self);
    }
    template <class Self, std::floating_point F>
    auto value(this Self&& self, F number) -> Self&& {
        // Written as a double (the format to_string uses). A finite long double past double range is refused as
        // such, not reported as an infinity it never was.
        if (std::isfinite(number) && !std::isfinite(static_cast<double>(number))) {
            self.fail("writer: a number is outside double range", json_error_code::invalid_number);
        } else {
            self.write_double(static_cast<double>(number));
        }
        return std::forward<Self>(self);
    }
    /// One value already in JSON form (e.g. a json_value's to_string()); it must parse as exactly one value.
    template <class Self> auto raw(this Self&& self, std::string_view json_text) -> Self&& {
        self.write_raw(json_text);
        return std::forward<Self>(self);
    }

    /// The finished document, or the first error. A writer that holds no complete value is an error.
    [[nodiscard]] auto finish() && -> json_result<std::string> {
        if (auto bad = final_error()) { return std::unexpected(std::move(bad).value()); }
        return std::move(out_);
    }
    [[nodiscard]] auto finish() const& -> json_result<std::string> {
        if (auto bad = final_error()) { return std::unexpected(std::move(bad).value()); }
        return out_;
    }

private:
    enum class frame_kind : std::uint8_t { object, array };
    struct frame {
        frame_kind kind;
        bool has_items = false;
        bool awaiting_value = false;  // object only: a key was written, its value has not been
    };

    std::string out_;
    std::vector<frame> stack_;
    std::optional<json_error> error_;
    bool root_written_ = false;

    auto fail(std::string message, json_error_code code = json_error_code::invalid_writer_sequence) -> void {
        if (!error_) { error_ = json_error{code, std::move(message), 0, 0}; }
    }
    // Called before every value or container: is a value allowed here, and does it need a comma?
    [[nodiscard]] auto begin_value() -> bool {
        if (error_) { return false; }
        if (stack_.empty()) {
            if (root_written_) {
                fail("writer: a second top-level value");
                return false;
            }
            root_written_ = true;
            return true;
        }
        frame& top = stack_.back();
        if (top.kind == frame_kind::object) {
            if (!top.awaiting_value) {
                fail("writer: a value inside an object needs a key first");
                return false;
            }
            top.awaiting_value = false;
            return true;
        }
        if (top.has_items) { out_ += ','; }
        top.has_items = true;
        return true;
    }
    auto open(frame_kind kind, char bracket) -> void {
        if (!begin_value()) { return; }
        out_ += bracket;
        stack_.push_back(frame{kind});
    }
    auto close(frame_kind kind, char bracket) -> void {
        if (error_) { return; }
        if (stack_.empty() || stack_.back().kind != kind) {
            fail(kind == frame_kind::object ? "writer: end_object() closes no open object"
                                            : "writer: end_array() closes no open array");
            return;
        }
        if (stack_.back().awaiting_value) {
            fail("writer: a key without a value");
            return;
        }
        stack_.pop_back();
        out_ += bracket;
    }
    auto write_key(std::string_view name) -> void {
        if (error_) { return; }
        if (stack_.empty() || stack_.back().kind != frame_kind::object) {
            fail("writer: key() outside an object");
            return;
        }
        frame& top = stack_.back();
        if (top.awaiting_value) {
            fail("writer: two keys in a row");
            return;
        }
        if (!detail::utf8_valid(name)) {
            fail("writer: a key is not UTF-8", json_error_code::invalid_unicode);
            return;
        }
        if (top.has_items) { out_ += ','; }
        top.has_items = true;
        top.awaiting_value = true;
        detail::append_json_string(out_, name);
        out_ += ':';
    }
    auto write_string(std::string_view text) -> void {
        if (!detail::utf8_valid(text)) {
            fail("writer: a string is not UTF-8", json_error_code::invalid_unicode);
            return;
        }
        if (!begin_value()) { return; }
        detail::append_json_string(out_, text);
    }
    auto write_scalar(std::string_view literal) -> void {
        if (!begin_value()) { return; }
        out_ += literal;
    }
    auto write_double(double number) -> void {
        if (!std::isfinite(number)) {
            fail("writer: a number is NaN or infinite", json_error_code::invalid_number);
            return;
        }
        if (!begin_value()) { return; }
        if (!detail::append_double(out_, number)) {
            fail("writer: a number could not be formatted", json_error_code::invalid_number);
        }
    }
    auto write_raw(std::string_view json_text) -> void {
        if (error_) { return; }
        if (auto parsed = parse(json_text); !parsed) {
            fail("writer: raw() text is not one JSON value: " + parsed.error().message);
            return;
        }
        if (!begin_value()) { return; }
        out_ += json_text;
    }
    [[nodiscard]] auto final_error() const -> std::optional<json_error> {
        if (error_) { return error_; }
        if (!stack_.empty()) {
            return json_error{json_error_code::invalid_writer_sequence, "writer: an object or array is still open", 0, 0};
        }
        if (!root_written_) {
            return json_error{json_error_code::invalid_writer_sequence, "writer: nothing was written", 0, 0};
        }
        return std::nullopt;
    }
};

// Literals implementation
inline namespace literals {
auto operator""_json(const char* str, std::size_t len) -> json_result<json_value> {
    return parse(std::string_view{str, len});
}
}  // namespace literals

}  // namespace fastjson