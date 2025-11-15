// FastestJSONInTheWest Core Module
export module fastjson.core;

import <memory>;
import <expected>;
import <concepts>;
import <string>;
import <string_view>;
import <span>;
import <array>;
import <vector>;
import <cstdint>;
import <cstddef>;
import <type_traits>;

#ifdef FASTJSON_HAVE_SOURCE_LOCATION
import <source_location>;
#endif

export namespace fastjson {
    
    // =============================================================================
    // Version Information
    // =============================================================================
    
    struct version_info {
        static constexpr int major = 1;
        static constexpr int minor = 0;
        static constexpr int patch = 0;
        static constexpr const char* string = "1.0.0";
    };
    
    // =============================================================================
    // Error Handling
    // =============================================================================
    
    enum class ErrorCode : std::uint32_t {
        // Parsing errors
        InvalidJson = 1000,
        UnexpectedEndOfInput,
        InvalidCharacter,
        InvalidString,
        InvalidNumber,
        InvalidKeyword,
        InvalidEscape,
        InvalidUnicode,
        MaxDepthExceeded,
        
        // Memory errors
        OutOfMemory = 2000,
        AllocationFailed,
        BufferOverflow,
        InvalidMemoryAccess,
        
        // I/O errors
        FileNotFound = 3000,
        FileReadError,
        FileWriteError,
        NetworkError,
        TimeoutError,
        
        // Threading errors
        ThreadCreationFailed = 4000,
        SynchronizationError,
        DeadlockDetected,
        
        // GPU errors
        GPUInitializationFailed = 5000,
        GPUMemoryError,
        KernelExecutionFailed,
        
        // System errors
        UnsupportedOperation = 6000,
        ConfigurationError,
        InternalError
    };
    
    class Error {
        ErrorCode code_;
        std::string message_;
        std::string context_;
        
#ifdef FASTJSON_HAVE_SOURCE_LOCATION
        std::source_location location_;
        
    public:
        Error(ErrorCode code, std::string_view message, std::string_view context = {},
              std::source_location loc = std::source_location::current()) noexcept
            : code_(code), message_(message), context_(context), location_(loc) {}
#else
        const char* file_;
        int line_;
        
    public:
        Error(ErrorCode code, std::string_view message, std::string_view context = {},
              const char* file = __FILE__, int line = __LINE__) noexcept
            : code_(code), message_(message), context_(context), file_(file), line_(line) {}
#endif
        
        ErrorCode code() const noexcept { return code_; }
        const std::string& message() const noexcept { return message_; }
        const std::string& context() const noexcept { return context_; }
        
#ifdef FASTJSON_HAVE_SOURCE_LOCATION
        const std::source_location& location() const noexcept { return location_; }
        std::string location_string() const {
            return std::string(location_.file_name()) + ":" + std::to_string(location_.line());
        }
#else
        std::string location_string() const {
            return std::string(file_) + ":" + std::to_string(line_);
        }
#endif
        
        std::string full_message() const {
            std::string result = "[" + std::to_string(static_cast<std::uint32_t>(code_)) + "] ";
            result += message_;
            if (!context_.empty()) {
                result += " (Context: " + context_ + ")";
            }
            result += " at " + location_string();
            return result;
        }
    };
    
    template<typename T>
    using Result = std::expected<T, Error>;
    
    // Helper macros for error creation
#ifdef FASTJSON_HAVE_SOURCE_LOCATION
    #define FASTJSON_ERROR(code, msg) \
        fastjson::Error(fastjson::ErrorCode::code, msg)
    
    #define FASTJSON_ERROR_WITH_CONTEXT(code, msg, ctx) \
        fastjson::Error(fastjson::ErrorCode::code, msg, ctx)
#else
    #define FASTJSON_ERROR(code, msg) \
        fastjson::Error(fastjson::ErrorCode::code, msg, {}, __FILE__, __LINE__)
    
    #define FASTJSON_ERROR_WITH_CONTEXT(code, msg, ctx) \
        fastjson::Error(fastjson::ErrorCode::code, msg, ctx, __FILE__, __LINE__)
#endif
    
    // =============================================================================
    // Core Concepts
    // =============================================================================
    
    template<typename T>
    concept JsonSerializable = requires(T t) {
        { t.to_json() } -> std::convertible_to<std::string>;
        { T::from_json(std::string_view{}) } -> std::same_as<Result<T>>;
    };
    
    template<typename T>
    concept Reflectable = requires {
        typename T::json_reflection_info;
    };
    
    template<typename T>
    concept SIMDOptimizable = requires {
        typename T::simd_type;
        { T::simd_width } -> std::convertible_to<std::size_t>;
    };
    
    template<typename T>
    concept Arithmetic = std::integral<T> || std::floating_point<T>;
    
    template<typename T>
    concept StringLike = std::convertible_to<T, std::string_view>;
    
    template<typename T>
    concept Container = requires(T t) {
        typename T::value_type;
        typename T::iterator;
        t.begin();
        t.end();
        t.size();
    };
    
    // =============================================================================
    // Forward Declarations
    // =============================================================================
    
    class JsonValue;
    class JsonDocument;
    class JsonArray;
    class JsonObject;
    
    // =============================================================================
    // JSON Value Types
    // =============================================================================
    
    enum class JsonType : std::uint8_t {
        Null,
        Boolean,
        Integer,
        Double,
        String,
        Array,
        Object
    };
    
    namespace detail {
        // Type-safe union for JSON values
        union JsonValueData {
            bool boolean;
            std::int64_t integer;
            double floating_point;
            std::string* string;
            JsonArray* array;
            JsonObject* object;
            
            JsonValueData() noexcept : integer(0) {}
            ~JsonValueData() noexcept {} // Destruction handled by JsonValue
        };
    }
    
    class JsonValue {
        JsonType type_;
        detail::JsonValueData data_;
        
    public:
        JsonValue() noexcept : type_(JsonType::Null) {}
        
        explicit JsonValue(std::nullptr_t) noexcept : type_(JsonType::Null) {}
        
        explicit JsonValue(bool value) noexcept : type_(JsonType::Boolean) {
            data_.boolean = value;
        }
        
        template<std::integral T>
        explicit JsonValue(T value) noexcept : type_(JsonType::Integer) {
            data_.integer = static_cast<std::int64_t>(value);
        }
        
        template<std::floating_point T>
        explicit JsonValue(T value) noexcept : type_(JsonType::Double) {
            data_.floating_point = static_cast<double>(value);
        }
        
        explicit JsonValue(StringLike auto&& value);
        explicit JsonValue(JsonArray array);
        explicit JsonValue(JsonObject object);
        
        // Copy and move constructors/operators
        JsonValue(const JsonValue& other);
        JsonValue(JsonValue&& other) noexcept;
        JsonValue& operator=(const JsonValue& other);
        JsonValue& operator=(JsonValue&& other) noexcept;
        
        ~JsonValue() noexcept;
        
        // Type queries
        JsonType type() const noexcept { return type_; }
        bool is_null() const noexcept { return type_ == JsonType::Null; }
        bool is_boolean() const noexcept { return type_ == JsonType::Boolean; }
        bool is_integer() const noexcept { return type_ == JsonType::Integer; }
        bool is_double() const noexcept { return type_ == JsonType::Double; }
        bool is_number() const noexcept { return is_integer() || is_double(); }
        bool is_string() const noexcept { return type_ == JsonType::String; }
        bool is_array() const noexcept { return type_ == JsonType::Array; }
        bool is_object() const noexcept { return type_ == JsonType::Object; }
        
        // Value accessors with type checking
        bool as_boolean() const;
        std::int64_t as_integer() const;
        double as_double() const;
        const std::string& as_string() const;
        const JsonArray& as_array() const;
        const JsonObject& as_object() const;
        
        // Mutable accessors
        bool& as_boolean();
        std::int64_t& as_integer();
        double& as_double();
        std::string& as_string();
        JsonArray& as_array();
        JsonObject& as_object();
        
        // Safe conversion with optional return
        template<typename T>
        std::optional<T> get() const noexcept;
        
        // Operator overloads for convenience
        bool operator==(const JsonValue& other) const noexcept;
        bool operator!=(const JsonValue& other) const noexcept {
            return !(*this == other);
        }
        
        // Array/Object access operators
        const JsonValue& operator[](std::size_t index) const;
        JsonValue& operator[](std::size_t index);
        const JsonValue& operator[](std::string_view key) const;
        JsonValue& operator[](std::string_view key);
    };
    
    // =============================================================================
    // JSON Array
    // =============================================================================
    
    class JsonArray {
        std::vector<JsonValue> values_;
        
    public:
        using iterator = std::vector<JsonValue>::iterator;
        using const_iterator = std::vector<JsonValue>::const_iterator;
        using value_type = JsonValue;
        using reference = JsonValue&;
        using const_reference = const JsonValue&;
        using size_type = std::size_t;
        
        JsonArray() = default;
        explicit JsonArray(std::size_t size) : values_(size) {}
        JsonArray(std::initializer_list<JsonValue> values) : values_(values) {}
        
        // Element access
        const_reference operator[](size_type index) const noexcept {
            return values_[index];
        }
        reference operator[](size_type index) noexcept {
            return values_[index];
        }
        
        const_reference at(size_type index) const {
            return values_.at(index);
        }
        reference at(size_type index) {
            return values_.at(index);
        }
        
        // Capacity
        bool empty() const noexcept { return values_.empty(); }
        size_type size() const noexcept { return values_.size(); }
        void reserve(size_type capacity) { values_.reserve(capacity); }
        
        // Modifiers
        void push_back(const JsonValue& value) { values_.push_back(value); }
        void push_back(JsonValue&& value) { values_.push_back(std::move(value)); }
        
        template<typename... Args>
        reference emplace_back(Args&&... args) {
            return values_.emplace_back(std::forward<Args>(args)...);
        }
        
        void pop_back() { values_.pop_back(); }
        void clear() noexcept { values_.clear(); }
        
        iterator insert(const_iterator pos, const JsonValue& value) {
            return values_.insert(pos, value);
        }
        iterator insert(const_iterator pos, JsonValue&& value) {
            return values_.insert(pos, std::move(value));
        }
        
        iterator erase(const_iterator pos) {
            return values_.erase(pos);
        }
        iterator erase(const_iterator first, const_iterator last) {
            return values_.erase(first, last);
        }
        
        // Iterators
        iterator begin() noexcept { return values_.begin(); }
        const_iterator begin() const noexcept { return values_.begin(); }
        const_iterator cbegin() const noexcept { return values_.cbegin(); }
        
        iterator end() noexcept { return values_.end(); }
        const_iterator end() const noexcept { return values_.end(); }
        const_iterator cend() const noexcept { return values_.cend(); }
    };
    
    // =============================================================================
    // JSON Object
    // =============================================================================
    
    class JsonObject {
        // Using vector of pairs for better cache locality than map
        std::vector<std::pair<std::string, JsonValue>> pairs_;
        
        // Helper to find key
        auto find_key(std::string_view key) const noexcept;
        auto find_key(std::string_view key) noexcept;
        
    public:
        using value_type = std::pair<std::string, JsonValue>;
        using iterator = std::vector<value_type>::iterator;
        using const_iterator = std::vector<value_type>::const_iterator;
        using size_type = std::size_t;
        
        JsonObject() = default;
        JsonObject(std::initializer_list<value_type> pairs) : pairs_(pairs) {}
        
        // Element access
        const JsonValue& operator[](std::string_view key) const;
        JsonValue& operator[](std::string_view key);
        
        const JsonValue& at(std::string_view key) const;
        JsonValue& at(std::string_view key);
        
        // Lookup
        bool contains(std::string_view key) const noexcept;
        const_iterator find(std::string_view key) const noexcept;
        iterator find(std::string_view key) noexcept;
        
        // Capacity
        bool empty() const noexcept { return pairs_.empty(); }
        size_type size() const noexcept { return pairs_.size(); }
        void reserve(size_type capacity) { pairs_.reserve(capacity); }
        
        // Modifiers
        std::pair<iterator, bool> insert(const value_type& pair);
        std::pair<iterator, bool> insert(value_type&& pair);
        
        template<typename... Args>
        std::pair<iterator, bool> emplace(Args&&... args) {
            // Implementation would check for duplicate keys
            auto pair = value_type(std::forward<Args>(args)...);
            auto it = find_key(pair.first);
            if (it != pairs_.end()) {
                return {it, false};
            }
            pairs_.push_back(std::move(pair));
            return {pairs_.end() - 1, true};
        }
        
        size_type erase(std::string_view key);
        iterator erase(const_iterator pos);
        
        void clear() noexcept { pairs_.clear(); }
        
        // Iterators
        iterator begin() noexcept { return pairs_.begin(); }
        const_iterator begin() const noexcept { return pairs_.begin(); }
        const_iterator cbegin() const noexcept { return pairs_.cbegin(); }
        
        iterator end() noexcept { return pairs_.end(); }
        const_iterator end() const noexcept { return pairs_.end(); }
        const_iterator cend() const noexcept { return pairs_.cend(); }
    };
    
    // =============================================================================
    // JSON Document
    // =============================================================================
    
    class JsonDocument {
        JsonValue root_;
        std::unique_ptr<class MemoryArena> arena_; // Forward declaration
        
    public:
        JsonDocument();
        explicit JsonDocument(JsonValue root);
        
        // Move-only type for performance
        JsonDocument(const JsonDocument&) = delete;
        JsonDocument& operator=(const JsonDocument&) = delete;
        JsonDocument(JsonDocument&&) noexcept = default;
        JsonDocument& operator=(JsonDocument&&) noexcept = default;
        
        ~JsonDocument() noexcept;
        
        // Root access
        const JsonValue& root() const noexcept { return root_; }
        JsonValue& root() noexcept { return root_; }
        
        // Convenience accessors (delegate to root)
        const JsonValue& operator[](std::size_t index) const { return root_[index]; }
        JsonValue& operator[](std::size_t index) { return root_[index]; }
        const JsonValue& operator[](std::string_view key) const { return root_[key]; }
        JsonValue& operator[](std::string_view key) { return root_[key]; }
        
        // Document-level operations
        std::string serialize(bool pretty_print = false) const;
        static Result<JsonDocument> parse(std::string_view json);
        static Result<JsonDocument> parse_file(std::string_view filename);
        
        // Memory management
        std::size_t memory_usage() const noexcept;
        void compact(); // Defragment internal memory
    };
    
    // =============================================================================
    // Utility Functions
    // =============================================================================
    
    // String conversion utilities
    std::string escape_string(std::string_view str);
    std::string unescape_string(std::string_view str);
    bool validate_utf8(std::string_view str) noexcept;
    
    // Number parsing utilities
    Result<std::int64_t> parse_integer(std::string_view str) noexcept;
    Result<double> parse_double(std::string_view str) noexcept;
    
    // Type name utilities
    const char* type_name(JsonType type) noexcept;
    
    // JSON path utilities
    class JsonPath; // Forward declaration
    
} // namespace fastjson

// =============================================================================
// Implementation Details (would be in separate implementation files)
// =============================================================================

namespace fastjson::detail {
    
    // Memory management forward declarations
    class MemoryArena;
    class ObjectPool;
    
    // SIMD backend forward declarations  
    class SIMDBackend;
    std::unique_ptr<SIMDBackend> create_optimal_simd_backend();
    
    // Threading utilities
    class ThreadPool;
    
    // I/O utilities
    class FileReader;
    class MemoryMappedFile;
    
} // namespace fastjson::detail