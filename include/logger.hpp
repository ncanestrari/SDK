#pragma once

#include "object.hpp"
#include "scheduler.hpp"

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations
class Logger;

// Endpoint interface for log destinations
class LogEndpoint {
public:
    virtual ~LogEndpoint() = default;
    virtual void write(const std::string& message) = 0;
    virtual void flush() = 0;
};

// Stdout endpoint
class StdoutEndpoint : public LogEndpoint {
public:
    void write(const std::string& message) override;
    void flush() override;
};

// File endpoint
class FileEndpoint : public LogEndpoint {
private:
    std::ofstream file;
    std::string filePath;
    mutable std::mutex fileMutex;

public:
    explicit FileEndpoint(const std::string& path);
    ~FileEndpoint() override;
    void write(const std::string& message) override;
    void flush() override;
};

// Socket endpoint (placeholder for future implementation)
class SocketEndpoint : public LogEndpoint {
private:
    std::string host;
    int port;
    // Socket implementation would go here

public:
    SocketEndpoint(const std::string& host, int port);
    void write(const std::string& message) override;
    void flush() override;
};

// Logger endpoint - forwards to another logger
class LoggerEndpoint : public LogEndpoint {
private:
    std::shared_ptr<Logger> logger;

public:
    explicit LoggerEndpoint(std::shared_ptr<Logger> targetLogger);
    void write(const std::string& message) override;
    void flush() override;
};

// Main Logger class
class Logger : public Object {
public:
    enum class LogLevel {
        DEBUG = -1,  // Only in debug builds
        INFO = 0,
        LOG = 1,
        WARN = 2,
        ERROR = 3
    };

private:
    Scheduler scheduler;
    std::vector<std::shared_ptr<LogEndpoint>> endpoints;
    std::string moduleName;
    std::string format;

    // Buffering and flushing configuration
    std::vector<std::string> buffer;
    size_t maxBufferBytes;
    std::chrono::microseconds flushInterval;
    size_t currentBufferBytes;
    std::chrono::steady_clock::time_point lastFlushTime;

    // Current log level
    std::atomic<int> currentLevel;

    // Mutex for buffer access
    mutable std::mutex bufferMutex;

    // Helper methods
    std::string formatMessage(LogLevel level, const std::string& message);
    void addToBuffer(const std::string& message);
    void flushBuffer();
    bool shouldFlush();
    std::string levelToString(LogLevel level);

public:
    // Constructor
    explicit Logger(const std::string& moduleName = "");

    // Destructor - ensures buffer is flushed
    ~Logger() override;

    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Override Object methods
    std::string getType() const override;
    void display() const override;

    // Configuration methods
    void setFormat(const std::string& fmt);
    void addEndpoint(std::shared_ptr<LogEndpoint> endpoint);
    void setFlushByteLimit(size_t bytes);
    void setFlushTimeInterval(std::chrono::microseconds interval);
    void setModuleName(const std::string& name);

    // Level control
    void setLevel(int level);
    int getLevel() const;

    // Logging methods
    void debug(const std::string& message);
    void info(const std::string& message);
    void log(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

    // Template versions for formatted logging
    template<typename... Args>
    void debug(fmt::format_string<Args...> fmt, Args&&... args) {
#ifndef NDEBUG
        std::string message = fmt::format(fmt, std::forward<Args>(args)...);
        logInternal(LogLevel::DEBUG, message);
#endif
    }

    template<typename... Args>
    void info(fmt::format_string<Args...> fmt, Args&&... args) {
        std::string message = fmt::format(fmt, std::forward<Args>(args)...);
        logInternal(LogLevel::INFO, message);
    }

    template<typename... Args>
    void log(fmt::format_string<Args...> fmt, Args&&... args) {
        std::string message = fmt::format(fmt, std::forward<Args>(args)...);
        logInternal(LogLevel::LOG, message);
    }

    template<typename... Args>
    void warn(fmt::format_string<Args...> fmt, Args&&... args) {
        std::string message = fmt::format(fmt, std::forward<Args>(args)...);
        logInternal(LogLevel::WARN, message);
    }

    template<typename... Args>
    void error(fmt::format_string<Args...> fmt, Args&&... args) {
        std::string message = fmt::format(fmt, std::forward<Args>(args)...);
        logInternal(LogLevel::ERROR, message);
    }

    // Manual flush
    void flush();

private:
    void logInternal(LogLevel level, const std::string& message);
};
