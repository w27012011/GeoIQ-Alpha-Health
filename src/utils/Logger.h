#ifndef GEOIQ_LOGGER_H
#define GEOIQ_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>

namespace GeoIQ {

    class Logger {
    public:
        enum class LogLevel {
            DEBUG_LVL,
            INFO_LVL,
            WARN_LVL,
            ERROR_LVL
        };

        // Singleton instance accessor (thread-safe Meyers Singleton)
        static Logger& getInstance();

        // Disable copy and assignment semantics
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(Logger&&) = delete;

        ~Logger();

        // Configures logger parameters
        void setLevel(LogLevel level);
        void setLogFile(const std::string& filepath);

        // Core logging execution method
        void log(LogLevel level, const std::string& module, const std::string& message);

        // Inline convenience wrappers for logging levels
        void debug(const std::string& module, const std::string& msg) { log(LogLevel::DEBUG_LVL, module, msg); }
        void info(const std::string& module, const std::string& msg)  { log(LogLevel::INFO_LVL, module, msg); }
        void warn(const std::string& module, const std::string& msg)  { log(LogLevel::WARN_LVL, module, msg); }
        void error(const std::string& module, const std::string& msg) { log(LogLevel::ERROR_LVL, module, msg); }

    private:
        Logger() = default; // Private constructor for singleton

        LogLevel current_level = LogLevel::INFO_LVL;
        std::ofstream log_stream;
        std::mutex log_mutex; // Mutual exclusion guard for concurrent safe stream writes

        std::string getLevelString(LogLevel level) const;
        std::string getTimestamp() const;
    };
}

#endif // GEOIQ_LOGGER_H
