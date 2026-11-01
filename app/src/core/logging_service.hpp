#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <filesystem>

namespace utilist::app::core {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class LoggingService {
public:
    static LoggingService& instance();

    bool initialize(const std::filesystem::path& logDirectory);
    void log(LogLevel level, const std::string& message);

    void debug(const std::string& message) { log(LogLevel::Debug, message); }
    void info(const std::string& message) { log(LogLevel::Info, message); }
    void warn(const std::string& message) { log(LogLevel::Warning, message); }
    void error(const std::string& message) { log(LogLevel::Error, message); }
    void critical(const std::string& message) { log(LogLevel::Critical, message); }

    [[nodiscard]] std::filesystem::path getLogFilePath() const;

private:
    LoggingService() = default;
    ~LoggingService();

    std::mutex m_mutex;
    std::ofstream m_fileStream;
    std::filesystem::path m_logFilePath;
    size_t m_currentBytesWritten{0};
    static constexpr size_t MAX_LOG_SIZE = 5 * 1024 * 1024; // 5 MB

    void rotateIfNeeded();
};

} // namespace utilist::app::core
