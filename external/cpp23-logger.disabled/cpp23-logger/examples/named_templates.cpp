/**
 * Named Templates Example for cpp23-logger
 *
 * Demonstrates:
 * - Mustache-style named templates {param_name}
 * - TemplateParams initialization
 * - Logger::named_args() helper
 * - Context-rich logging for complex systems
 */

import logger;

#include <iostream>
#include <string>

auto main() -> int {
    // Initialize logger
    auto& log = logger::Logger::getInstance()
        .initialize("logs/templates.log")
        .setLevel(logger::LogLevel::DEBUG)
        .enableColors(true);

    // =======================================================================
    // Method 1: Direct map initialization
    // =======================================================================

    log.info("User {user_id} logged in from {ip} at {time}",
        {
            {"user_id", 12345},
            {"ip", "192.168.1.100"},
            {"time", "14:23:45"}
        });

    // =======================================================================
    // Method 2: Using named_args helper (more concise)
    // =======================================================================

    std::string symbol = "AAPL";
    int quantity = 100;
    double price = 178.50;

    log.warn("Trade rejected: Symbol={symbol} Qty={qty} Price=${price}",
        logger::Logger::named_args("symbol", symbol, "qty", quantity, "price", price));

    // =======================================================================
    // Method 3: Complex simulation logging
    // =======================================================================

    int simulation_id = 42;
    int step = 150;
    double temperature = 298.15;
    double pressure = 101325.0;
    std::string status = "Running";

    log.debug("Simulation[{sim_id}] Step[{step}]: T={temp}K P={pressure}Pa Status={status}",
        {
            {"sim_id", simulation_id},
            {"step", step},
            {"temp", temperature},
            {"pressure", pressure},
            {"status", status}
        });

    // =======================================================================
    // Method 4: Pipeline execution tracking
    // =======================================================================

    std::string pipeline_name = "RiskAwareTrade";
    int current_step = 3;
    int total_steps = 5;
    std::string step_name = "PlaceOrder";
    std::string step_status = "Success";
    int duration_ms = 45;

    log.info("Pipeline[{pipeline}] Step[{current}/{total}] {step_name}: {status} ({duration}ms)",
        {
            {"pipeline", pipeline_name},
            {"current", current_step},
            {"total", total_steps},
            {"step_name", step_name},
            {"status", step_status},
            {"duration", duration_ms}
        });

    // =======================================================================
    // Method 5: Error tracking with context
    // =======================================================================

    std::string function_name = "connectToDatabase";
    std::string error_message = "Connection timeout";
    int error_code = -1;
    int retry_attempt = 2;

    log.error("Function {func} failed with error {code}: {message} (attempt {attempt})",
        logger::Logger::named_args(
            "func", function_name,
            "code", error_code,
            "message", error_message,
            "attempt", retry_attempt
        ));

    // =======================================================================
    // Method 6: Machine learning training progress
    // =======================================================================

    int epoch = 15;
    int total_epochs = 100;
    double train_loss = 0.0234;
    double val_loss = 0.0312;
    double accuracy = 95.7;
    double learning_rate = 0.0001;

    log.info("Training Epoch[{epoch}/{total}]: Loss={loss:.4f} ValLoss={val_loss:.4f} Acc={acc:.2f}% LR={lr:.6f}",
        {
            {"epoch", epoch},
            {"total", total_epochs},
            {"loss", train_loss},
            {"val_loss", val_loss},
            {"acc", accuracy},
            {"lr", learning_rate}
        });

    // =======================================================================
    // Method 7: Performance metrics logging
    // =======================================================================

    std::string component = "PricePredictor";
    int requests_processed = 5432;
    double avg_latency_ms = 12.3;
    double p95_latency_ms = 25.7;
    double p99_latency_ms = 45.2;
    double throughput_rps = 450.5;

    log.info("Metrics[{component}]: Requests={requests} AvgLatency={avg}ms P95={p95}ms P99={p99}ms Throughput={tps}rps",
        logger::Logger::named_args(
            "component", component,
            "requests", requests_processed,
            "avg", avg_latency_ms,
            "p95", p95_latency_ms,
            "p99", p99_latency_ms,
            "tps", throughput_rps
        ));

    log.info("Examples complete - check logs/templates.log for output");

    return 0;
}
