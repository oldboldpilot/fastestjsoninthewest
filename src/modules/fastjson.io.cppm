// FastestJSONInTheWest I/O Module
export module fastjson.io;

import fastjson.core;
import <string>;
import <string_view>;
import <memory>;

export namespace fastjson::io {

Result<std::string> read_file(std::string_view filename) noexcept;
Result<void> write_file(std::string_view filename, std::string_view content) noexcept;

class MemoryMappedFile {
    void* mapped_memory_;
    std::size_t size_;
    
public:
    explicit MemoryMappedFile(std::string_view filename);
    ~MemoryMappedFile();
    
    const void* data() const noexcept { return mapped_memory_; }
    std::size_t size() const noexcept { return size_; }
    bool is_valid() const noexcept { return mapped_memory_ != nullptr; }
};

} // namespace fastjson::io
