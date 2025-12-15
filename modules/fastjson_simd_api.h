#pragma once

#include <memory>
#include <vector>
#include <span>
#include <optional>
#include <functional>
#include <algorithm>
#include <concepts>
#include <chrono>

import fastjson_simd_multiregister;

namespace fastjson::simd {

// ============================================================================
// RAII SIMD Context Management
// ============================================================================

class alignas(64) SIMDContext {
public:
    // Factory methods following Core Guidelines
    [[nodiscard]] static auto create() -> std::unique_ptr<SIMDContext>;
    [[nodiscard]] static auto create_with_capabilities(std::span<const char> required_features) 
        -> std::optional<std::unique_ptr<SIMDContext>>;

    // Non-copyable, moveable (Rule of 5)
    SIMDContext(const SIMDContext&) = delete;
    SIMDContext& operator=(const SIMDContext&) = delete;
    SIMDContext(SIMDContext&&) = default;
    SIMDContext& operator=(SIMDContext&&) = default;
    ~SIMDContext() = default;

    // Fluent API for configuration
    [[nodiscard]] auto with_avx512(bool enable = true) & -> SIMDContext& {
        capabilities_.avx512_enabled = enable;
        return *this;
    }
    
    [[nodiscard]] auto with_avx512(bool enable = true) && -> SIMDContext {
        capabilities_.avx512_enabled = enable;
        return std::move(*this);
    }
    
    [[nodiscard]] auto with_multi_register(bool enable = true) & -> SIMDContext& {
        capabilities_.multi_register_enabled = enable;
        return *this;
    }
    
    [[nodiscard]] auto with_multi_register(bool enable = true) && -> SIMDContext {
        capabilities_.multi_register_enabled = enable;
        return std::move(*this);
    }

    // Query capabilities
    [[nodiscard]] auto supports_avx512() const noexcept -> bool;
    [[nodiscard]] auto supports_avx2() const noexcept -> bool;
    [[nodiscard]] auto supports_multi_register() const noexcept -> bool;
    [[nodiscard]] auto get_optimal_chunk_size() const noexcept -> size_t;

private:
    struct Capabilities {
        bool avx512_detected{false};
        bool avx2_detected{false};
        bool avx512_enabled{true};
        bool multi_register_enabled{true};
        size_t optimal_chunk_size{128};
    };

    explicit SIMDContext(Capabilities caps) : capabilities_(std::move(caps)) {}
    
    Capabilities capabilities_;
    
    static auto detect_capabilities() -> Capabilities;
};

// ============================================================================
// RAII Memory-Aligned Buffer Management
// ============================================================================

template<size_t Alignment = 64>
class AlignedBuffer {
public:
    explicit AlignedBuffer(size_t size) 
        : size_(size)
        , data_(std::aligned_alloc(Alignment, ((size + Alignment - 1) / Alignment) * Alignment), 
                std::free) {
        if (!data_) {
            throw std::bad_alloc{};
        }
    }

    // Non-copyable, moveable
    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;
    AlignedBuffer(AlignedBuffer&&) = default;
    AlignedBuffer& operator=(AlignedBuffer&&) = default;
    ~AlignedBuffer() = default;

    [[nodiscard]] auto data() noexcept -> std::byte* {
        return static_cast<std::byte*>(data_.get());
    }
    
    [[nodiscard]] auto data() const noexcept -> const std::byte* {
        return static_cast<const std::byte*>(data_.get());
    }
    
    [[nodiscard]] auto as_span() const noexcept -> std::span<const std::byte> {
        return {data(), size_};
    }
    
    [[nodiscard]] auto as_char_span() const noexcept -> std::span<const char> {
        return {reinterpret_cast<const char*>(data()), size_};
    }
    
    [[nodiscard]] auto size() const noexcept -> size_t { return size_; }

private:
    size_t size_;
    std::unique_ptr<void, decltype(&std::free)> data_;
};

// ============================================================================
// Fluent API SIMD Operations Builder
// ============================================================================

class SIMDOperations {
public:
    explicit SIMDOperations(std::shared_ptr<SIMDContext> context)
        : context_(std::move(context)) {}

    // Fluent API for chaining operations
    [[nodiscard]] auto skip_whitespace(std::span<const char> data, size_t start_pos = 0) const -> size_t;
    
    [[nodiscard]] auto find_string_end(std::span<const char> data, size_t start_pos = 0) const -> size_t;
    
    [[nodiscard]] auto validate_number_chars(std::span<const char> data, 
                                            size_t start_pos = 0, 
                                            std::optional<size_t> end_pos = std::nullopt) const -> bool;
    
    [[nodiscard]] auto find_structural_chars(std::span<const char> data, 
                                            size_t start_pos = 0) const -> multi::StructuralChars;

    // Batch operations for better cache utilization
    template<typename Container>
    [[nodiscard]] auto batch_skip_whitespace(const Container& chunks) const 
        -> std::vector<size_t> requires std::ranges::range<Container>;

    // Performance monitoring
    struct PerformanceStats {
        size_t bytes_processed{0};
        size_t operations_count{0};
        std::chrono::nanoseconds total_time{0};
        
        [[nodiscard]] auto throughput_mbps() const -> double {
            if (total_time.count() == 0) return 0.0;
            return (static_cast<double>(bytes_processed) * 1000.0) / total_time.count();
        }
    };
    
    [[nodiscard]] auto get_stats() const -> const PerformanceStats& { return stats_; }
    auto reset_stats() -> void { stats_ = {}; }

private:
    std::shared_ptr<SIMDContext> context_;
    mutable PerformanceStats stats_;
    
    template<typename Func>
    auto timed_operation(std::span<const char> data, Func&& func) const -> decltype(func());
};

// ============================================================================
// High-Level JSON Scanner with SIMD
// ============================================================================

class JSONScanner {
public:
    enum class TokenType : uint8_t {
        Whitespace,
        String,
        Number,
        OpenBrace,
        CloseBrace,
        OpenBracket,
        CloseBracket,
        Colon,
        Comma,
        BooleanTrue,
        BooleanFalse,
        Null,
        Unknown
    };

    struct Token {
        TokenType type;
        size_t position;
        size_t length;
        
        [[nodiscard]] auto extract_text(std::span<const char> source) const -> std::string_view {
            return {source.data() + position, length};
        }
    };

    explicit JSONScanner(std::shared_ptr<SIMDContext> context)
        : operations_(std::move(context)) {}

    // Fluent API for configuration
    [[nodiscard]] auto with_structural_detection(bool enable = true) & -> JSONScanner& {
        enable_structural_detection_ = enable;
        return *this;
    }
    
    [[nodiscard]] auto with_structural_detection(bool enable = true) && -> JSONScanner {
        enable_structural_detection_ = enable;
        return std::move(*this);
    }
    
    [[nodiscard]] auto with_validation(bool enable = true) & -> JSONScanner& {
        enable_validation_ = enable;
        return *this;
    }
    
    [[nodiscard]] auto with_validation(bool enable = true) && -> JSONScanner {
        enable_validation_ = enable;
        return std::move(*this);
    }

    // Main scanning operations
    [[nodiscard]] auto scan_tokens(std::span<const char> json_data) -> std::vector<Token>;
    
    [[nodiscard]] auto scan_next_token(std::span<const char> json_data, 
                                      size_t& position) -> std::optional<Token>;

    // Specialized scanning operations
    [[nodiscard]] auto find_all_strings(std::span<const char> json_data) -> std::vector<Token>;
    
    [[nodiscard]] auto find_all_numbers(std::span<const char> json_data) -> std::vector<Token>;
    
    [[nodiscard]] auto find_structural_layout(std::span<const char> json_data) 
        -> std::vector<Token>;

    // Performance and diagnostics
    [[nodiscard]] auto get_operations_stats() const -> const SIMDOperations::PerformanceStats& {
        return operations_.get_stats();
    }
    
    auto reset_stats() -> void { operations_.reset_stats(); }

private:
    SIMDOperations operations_;
    bool enable_structural_detection_{true};
    bool enable_validation_{true};
    
    [[nodiscard]] auto classify_token_at(std::span<const char> data, size_t position) -> TokenType;
    
    [[nodiscard]] auto scan_string_token(std::span<const char> data, size_t start_pos) -> Token;
    
    [[nodiscard]] auto scan_number_token(std::span<const char> data, size_t start_pos) -> Token;
    
    [[nodiscard]] auto scan_literal_token(std::span<const char> data, size_t start_pos) -> Token;
};

// ============================================================================
// Factory Functions and Convenience APIs
// ============================================================================

namespace factory {

[[nodiscard]] inline auto create_optimized_context() -> std::unique_ptr<SIMDContext> {
    return std::make_unique<SIMDContext>(
        std::move(*SIMDContext::create())
            .with_avx512(true)
            .with_multi_register(true)
    );
}

[[nodiscard]] inline auto create_scanner() -> JSONScanner {
    auto context = std::make_shared<SIMDContext>(std::move(*create_optimized_context()));
    return JSONScanner{std::move(context)}
        .with_structural_detection(true)
        .with_validation(true);
}

[[nodiscard]] inline auto create_operations() -> SIMDOperations {
    auto context = std::make_shared<SIMDContext>(std::move(*create_optimized_context()));
    return SIMDOperations{std::move(context)};
}

} // namespace factory

// ============================================================================
// Template Implementations
// ============================================================================

template<typename Container>
[[nodiscard]] auto SIMDOperations::batch_skip_whitespace(const Container& chunks) const 
    -> std::vector<size_t> requires std::ranges::range<Container> {
    
    std::vector<size_t> results;
    results.reserve(std::ranges::size(chunks));
    
    for (const auto& chunk : chunks) {
        if constexpr (requires { chunk.data(); chunk.size(); }) {
            std::span<const char> span{chunk.data(), chunk.size()};
            results.push_back(skip_whitespace(span));
        } else {
            static_assert(std::convertible_to<decltype(chunk), std::span<const char>>, 
                         "Container elements must be convertible to span<const char>");
            results.push_back(skip_whitespace(chunk));
        }
    }
    
    return results;
}

template<typename Func>
auto SIMDOperations::timed_operation(std::span<const char> data, Func&& func) const 
    -> decltype(func()) {
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = std::forward<Func>(func)();
    auto end = std::chrono::high_resolution_clock::now();
    
    stats_.bytes_processed += data.size();
    stats_.operations_count++;
    stats_.total_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    return result;
}

// ============================================================================
// Concepts for Template Constraints
// ============================================================================

template<typename T>
concept JSONDataSource = requires(T t) {
    { t.data() } -> std::convertible_to<const char*>;
    { t.size() } -> std::convertible_to<size_t>;
} || std::convertible_to<T, std::span<const char>>;

template<typename T>
concept SIMDCapableBuffer = requires(T t) {
    requires std::same_as<T, AlignedBuffer<64>> || std::same_as<T, AlignedBuffer<32>>;
    { t.as_char_span() } -> std::convertible_to<std::span<const char>>;
};

} // namespace fastjson::simd