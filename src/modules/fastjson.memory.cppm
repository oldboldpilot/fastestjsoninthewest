// FastestJSONInTheWest Memory Module
export module fastjson.memory;

import fastjson.core;
import <memory>;
import <cstddef>;
import <cstdint>;

export namespace fastjson::memory {

class ArenaAllocator {
    std::unique_ptr<std::byte[]> buffer_;
    std::size_t capacity_;
    std::size_t used_;
    std::size_t alignment_;
    
public:
    explicit ArenaAllocator(std::size_t capacity = 1024 * 1024, std::size_t alignment = 64);
    ~ArenaAllocator();
    
    void* allocate(std::size_t size) noexcept;
    void reset() noexcept;
    std::size_t bytes_used() const noexcept { return used_; }
    std::size_t bytes_available() const noexcept { return capacity_ - used_; }
    
private:
    static std::size_t align_up(std::size_t size, std::size_t alignment) noexcept;
};

} // namespace fastjson::memory
