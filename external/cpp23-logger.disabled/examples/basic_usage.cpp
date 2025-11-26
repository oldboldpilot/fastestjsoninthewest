/**
 * Basic Usage Example for cpp23-logger
 *
 * Demonstrates:
 * - Logger initialization
 * - Fluent API
 * - Log levels
 * - Positional formatting
 */

import logger;

#include <iostream>

auto main() -> int {
    // Initialize logger with fluent API
    auto& log = logger::Logger::getInstance()
        .initialize("logs/example.log")
        .setLevel(logger::LogLevel::DEBUG)
        .enableColors(true);

    // Basic logging at different levels
    log.trace("This is a trace message (very detailed)");
    log.debug("This is a debug message");
    log.info("This is an info message");
    log.warn("This is a warning message");
    log.error("This is an error message");
    log.critical("This is a critical message!");

    // Positional formatting
    int record_count = 1000;
    double process_time = 2.5;

    log.info("Processed {} records in {} seconds", record_count, process_time);
    log.debug("Average time per record: {} ms", (process_time * 1000) / record_count);

    // Multiple parameters
    std::string operation = "database_connect";
    int retry_count = 3;
    int max_retries = 5;

    log.warn("Operation '{}' failed, retry {}/{}", operation, retry_count, max_retries);

    // Error handling example
    int error_code = 404;
    std::string error_msg = "Resource not found";

    log.error("HTTP Error {}: {}", error_code, error_msg);

    // Configuration changes
    log.info("Changing log level to WARN");
    log.setLevel(logger::LogLevel::WARN);

    log.debug("This debug message won't be shown");  // Below WARN level
    log.warn("This warning will be shown");           // At WARN level

    log.info("Application completed successfully");

    return 0;
}
