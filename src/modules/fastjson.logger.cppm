// FastestJSONInTheWest Logger Module
export module fastjson.logger;

import fastjson.core;
import <string>;
import <string_view>;
import <memory>;

export namespace fastjson::logging {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5
};

class Logger {
    std::string name_;
    LogLevel min_level_;
    
public:
    explicit Logger(std::string_view name, LogLevel min_level = LogLevel::Info);
    ~Logger();
    
    void log(LogLevel level, std::string_view message) noexcept;
    
    void trace(std::string_view message) noexcept { log(LogLevel::Trace, message); }
    void debug(std::string_view message) noexcept { log(LogLevel::Debug, message); }
    void info(std::string_view message) noexcept { log(LogLevel::Info, message); }
    void warning(std::string_view message) noexcept { log(LogLevel::Warning, message); }
    void error(std::string_view message) noexcept { log(LogLevel::Error, message); }
    void critical(std::string_view message) noexcept { log(LogLevel::Critical, message); }
    
    void set_level(LogLevel level) noexcept { min_level_ = level; }
    LogLevel get_level() const noexcept { return min_level_; }
    
private:
    bool should_log(LogLevel level) const noexcept;
    const char* level_name(LogLevel level) const noexcept;
};

Logger& get_global_logger();

} // namespace fastjson::logging
