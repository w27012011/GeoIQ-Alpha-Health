#ifdef _MSC_VER
#pragma warning(disable: 4996) // Suppress MSVC deprecation warning for std::localtime
#endif

#include "utils/Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace GeoIQ {

    Logger& Logger::getInstance() {
        static Logger instance;
        return instance;
    }

    Logger::~Logger() {
        std::lock_guard<std::mutex> lock(log_mutex);
        if (log_stream.is_open()) {
            log_stream.close();
        }
    }

    void Logger::setLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(log_mutex);
        current_level = level;
    }

    void Logger::setLogFile(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(log_mutex);
        if (log_stream.is_open()) {
            log_stream.close();
        }
        // Truncate existing log file and open a new session
        log_stream.open(filepath, std::ios::out | std::ios::trunc);
    }

    void Logger::log(LogLevel level, const std::string& module, const std::string& message) {
        // Guard checking if the log matches the active threshold
        if (static_cast<int>(level) < static_cast<int>(current_level)) {
            return;
        }

        std::lock_guard<std::mutex> lock(log_mutex);

        std::string lvl_str = getLevelString(level);
        std::string timestamp = getTimestamp();

        // Print to console
        std::cout << "[" << lvl_str << "] " << timestamp << " [" << module << "] " << message << std::endl;

        // Print to file stream if active
        if (log_stream.is_open()) {
            log_stream << "[" << lvl_str << "] " << timestamp << " [" << module << "] " << message << std::endl;
        }
    }

    std::string Logger::getLevelString(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG_LVL: return "DEBUG";
            case LogLevel::INFO_LVL:  return "INFO ";
            case LogLevel::WARN_LVL:  return "WARN ";
            case LogLevel::ERROR_LVL: return "ERROR";
            default:                  return "UNKN ";
        }
    }

    std::string Logger::getTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm* timeinfo = std::localtime(&in_time_t);
        
        std::stringstream ss;
        if (timeinfo) {
            ss << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S");
        } else {
            ss << "0000-00-00 00:00:00";
        }
        return ss.str();
    }
}
