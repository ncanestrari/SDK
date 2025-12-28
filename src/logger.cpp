#include "logger.hpp"

#include <fmt/chrono.h>
#include <iostream>

// StdoutEndpoint implementation
void StdoutEndpoint::write(const std::string& message) {
    std::cout << message;
}

void StdoutEndpoint::flush() {
    std::cout.flush();
}

// FileEndpoint implementation
FileEndpoint::FileEndpoint(const std::string& path) : filePath(path) {
    file.open(filePath, std::ios::out | std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filePath);
    }
}

FileEndpoint::~FileEndpoint() {
    if (file.is_open()) {
        file.flush();
        file.close();
    }
}

void FileEndpoint::write(const std::string& message) {
    std::lock_guard<std::mutex> lock(fileMutex);
    if (file.is_open()) {
        file << message;
    }
}

void FileEndpoint::flush() {
    std::lock_guard<std::mutex> lock(fileMutex);
    if (file.is_open()) {
        file.flush();
    }
}

// SocketEndpoint implementation (placeholder)
SocketEndpoint::SocketEndpoint(const std::string& host, int port)
    : host(host), port(port) {
    // Socket initialization would go here
    // For now, this is a placeholder
}

void SocketEndpoint::write(const std::string& message) {
    // Socket write implementation would go here
    // Placeholder: print to stderr to indicate it's not implemented
    std::cerr << "[SocketEndpoint] Not implemented: " << message;
}

void SocketEndpoint::flush() {
    // Socket flush implementation would go here
}

// LoggerEndpoint implementation
LoggerEndpoint::LoggerEndpoint(std::shared_ptr<Logger> targetLogger)
    : logger(targetLogger) {}

void LoggerEndpoint::write(const std::string& message) {
    if (logger) {
        // Forward the already formatted message
        logger->info(message);
    }
}

void LoggerEndpoint::flush() {
    if (logger) {
        logger->flush();
    }
}

// Logger implementation
Logger::Logger(const std::string& moduleName)
    : moduleName(moduleName),
      format("{} - {} - [{}] {}\n"),
      maxBufferBytes(1024 * 1024),  // 1MB default
      flushInterval(std::chrono::seconds(1)),  // 1 second default
      currentBufferBytes(0),
      lastFlushTime(std::chrono::steady_clock::now()),
      currentLevel(static_cast<int>(LogLevel::INFO)) {}

Logger::~Logger() {
    flush();
    scheduler.shutdown();
}

std::string Logger::getType() const {
    return "Logger";
}

void Logger::display() const {
    fmt::print("Logger [module: {}]", moduleName);
}

void Logger::setFormat(const std::string& fmt) {
    format = fmt;
}

void Logger::addEndpoint(std::shared_ptr<LogEndpoint> endpoint) {
    endpoints.push_back(endpoint);
}

void Logger::setFlushByteLimit(size_t bytes) {
    maxBufferBytes = bytes;
}

void Logger::setFlushTimeInterval(std::chrono::microseconds interval) {
    flushInterval = interval;
}

void Logger::setModuleName(const std::string& name) {
    moduleName = name;
}

void Logger::setLevel(int level) {
    currentLevel.store(level);
}

int Logger::getLevel() const {
    return currentLevel.load();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::LOG:   return "LOG";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

std::string Logger::formatMessage(LogLevel level, const std::string& message) {
    std::string result;

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // Format date/time
    std::string dateStr = fmt::format("{:%Y-%m-%d %H:%M:%S}.{:03d}", now, ms.count());

    result = fmt::format(format, dateStr, moduleName, levelToString(level), message);

    return result;
}

void Logger::addToBuffer(const std::string& message) {
    std::lock_guard<std::mutex> lock(bufferMutex);
    buffer.push_back(message);
    currentBufferBytes += message.size();
}

bool Logger::shouldFlush() {
    std::lock_guard<std::mutex> lock(bufferMutex);

    // Check byte limit
    if (currentBufferBytes >= maxBufferBytes) {
        return true;
    }

    // Check time interval
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastFlushTime);
    if (elapsed >= flushInterval) {
        return true;
    }

    return false;
}

void Logger::flushBuffer() {
    std::vector<std::string> localBuffer;
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        if (buffer.empty()) {
            return;
        }
        localBuffer.swap(buffer);
        currentBufferBytes = 0;
        lastFlushTime = std::chrono::steady_clock::now();
    }

    // Write to all endpoints
    for (const auto& endpoint : endpoints) {
        for (const auto& message : localBuffer) {
            endpoint->write(message);
        }
        endpoint->flush();
    }
}

void Logger::flush() {
    scheduler.waitForAll();
    flushBuffer();
}

void Logger::logInternal(LogLevel level, const std::string& message) {
    // Check if this message should be logged based on current level
    if (static_cast<int>(level) < currentLevel.load()) {
        return;
    }

    // Format the message IMMEDIATELY to capture current timestamp and thread ID
    std::string formattedMessage = formatMessage(level, message);

    // Schedule the buffering and flushing operation asynchronously
    scheduler.schedule([this, formattedMessage]() {
        addToBuffer(formattedMessage);

        if (shouldFlush()) {
            flushBuffer();
        }
    });
}

void Logger::debug(const std::string& message) {
#ifndef NDEBUG
    logInternal(LogLevel::DEBUG, message);
#endif
}

void Logger::info(const std::string& message) {
    logInternal(LogLevel::INFO, message);
}

void Logger::log(const std::string& message) {
    logInternal(LogLevel::LOG, message);
}

void Logger::warn(const std::string& message) {
    logInternal(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    logInternal(LogLevel::ERROR, message);
}
