#include "logger.hpp"
#include "object.hpp"

#include <chrono>
#include <memory>
#include <thread>

int main() {
    fmt::print("=== Logger Example ===\n\n");

    // Create a logger with module name
    auto logger = std::make_shared<Logger>("MainApp");

    // Register it with the ObjectRegistry
    ObjectRegistry::getInstance().registerObject("main_logger", logger);

    // Configure endpoints
    // 1. Add stdout endpoint
    logger->addEndpoint(std::make_shared<StdoutEndpoint>());

    // 2. Add file endpoint
    try {
        logger->addEndpoint(std::make_shared<FileEndpoint>("app.log"));
        fmt::print("Logging to both stdout and app.log\n\n");
    } catch (const std::exception& e) {
        fmt::print("Warning: Could not open log file: {}\n", e.what());
    }

    // Configure format (default is already set, but we can customize)
    logger->setFormat("{} - {} - [{}] {}\n");

    // Configure flush settings
    logger->setFlushByteLimit(512);  // Flush every 512 bytes
    logger->setFlushTimeInterval(std::chrono::milliseconds(500));  // Or every 500ms

    // Set initial log level to INFO (level 0)
    logger->setLevel(0);

    fmt::print("--- Testing different log levels ---\n");

    // Test different log levels
    logger->info("Application started");
    logger->log("Processing initialization");
    logger->warn("Configuration file missing, using defaults");
    logger->error("Failed to connect to database");

    // Debug messages (only visible in debug builds)
    logger->debug("This is a debug message - only in debug builds");

    // Test formatted logging
    fmt::print("\n--- Testing formatted logging ---\n");
    int userId = 42;
    std::string username = "alice";
    logger->info("User {} ({}) logged in", username, userId);
    logger->warn("Disk space low: {}% remaining", 15);

    // Test dynamic level change
    fmt::print("\n--- Testing dynamic log level change ---\n");
    fmt::print("Setting level to WARN (2) - INFO and LOG won't show\n");
    logger->setLevel(2);

    logger->info("This INFO won't be logged (level too low)");
    logger->log("This LOG won't be logged (level too low)");
    logger->warn("This WARN will be logged");
    logger->error("This ERROR will be logged");

    // Reset level
    logger->setLevel(0);

    // Test with multiple threads
    fmt::print("\n--- Testing multi-threaded logging ---\n");
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([logger, i]() {
            for (int j = 0; j < 3; ++j) {
                logger->info("Thread {} - message {}", i, j);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Test Logger chaining (Logger endpoint)
    fmt::print("\n--- Testing Logger chaining ---\n");
    auto secondaryLogger = std::make_shared<Logger>("SecondaryModule");
    secondaryLogger->addEndpoint(std::make_shared<StdoutEndpoint>());
    secondaryLogger->setFormat("[SECONDARY] {} - {} - [{}] {}");

    // Chain the secondary logger to the main logger
    logger->addEndpoint(std::make_shared<LoggerEndpoint>(secondaryLogger));
    logger->info("This message will appear in both loggers");

    // Ensure all logs are flushed before exiting
    fmt::print("\n--- Flushing all logs ---\n");
    logger->flush();
    secondaryLogger->flush();

    fmt::print("\n=== Logger example completed ===\n");
    fmt::print("Check app.log for file output\n");

    return 0;
}
