#include "logging_service.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>

namespace utilist::app::core {

namespace fs = std::filesystem;

LoggingService& LoggingService::instance() {
    static LoggingService s_instance;
    return s_instance;
}

LoggingService::~LoggingService() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fileStream.is_open()) {
        m_fileStream.flush();
        m_fileStream.close();
    }
}

bool LoggingService::initialize(const fs::path& logDirectory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::error_code ec;
    fs::create_directories(logDirectory, ec);

    m_logFilePath = logDirectory / "utilist.log";
    if (fs::exists(m_logFilePath)) {
        m_currentBytesWritten = fs::file_size(m_logFilePath, ec);
    }

    m_fileStream.open(m_logFilePath, std::ios::out | std::ios::app);
    return m_fileStream.is_open();
}

void LoggingService::rotateIfNeeded() {
    if (m_currentBytesWritten > MAX_LOG_SIZE) {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        std::error_code ec;
        fs::path backupPath = m_logFilePath.parent_path() / (m_logFilePath.stem().string() + ".old.log");
        fs::rename(m_logFilePath, backupPath, ec);
        m_fileStream.open(m_logFilePath, std::ios::out | std::ios::trunc);
        m_currentBytesWritten = 0;
    }
}

void LoggingService::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const char* levelStr = "INFO";
    switch (level) {
        case LogLevel::Debug: levelStr = "DEBUG"; break;
        case LogLevel::Info: levelStr = "INFO"; break;
        case LogLevel::Warning: levelStr = "WARN"; break;
        case LogLevel::Error: levelStr = "ERROR"; break;
        case LogLevel::Critical: levelStr = "CRITICAL"; break;
    }

    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tmNow{};
    localtime_s(&tmNow, &timeT);

    std::ostringstream ss;
    ss << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S")
       << "." << std::setfill('0') << std::setw(3) << ms.count()
       << " [" << levelStr << "] "
       << message << "\n";

    std::string formatted = ss.str();

    // Print to stdout/stderr as well
    if (level == LogLevel::Error || level == LogLevel::Critical) {
        std::cerr << formatted;
    } else {
        std::cout << formatted;
    }

    if (m_fileStream.is_open()) {
        rotateIfNeeded();
        m_fileStream << formatted;
        m_fileStream.flush();
        m_currentBytesWritten += formatted.size();
    }
}

fs::path LoggingService::getLogFilePath() const {
    return m_logFilePath;
}

} // namespace utilist::app::core
