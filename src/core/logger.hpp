#pragma once

#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace CORE {

    enum class LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3
    };

    class Logger {
    public:
        static Logger& instance() {
            static Logger logger;
            return logger;
        }

        void set_level(LogLevel level) {
            std::lock_guard<std::mutex> lock(mutex_);
            current_level_ = level;
        }

        template<typename... Args>
        void log(LogLevel level, Args&&... args) {
            if (level < current_level_) return;

            std::lock_guard<std::mutex> lock(mutex_);
            
            // Get current time
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            // Format timestamp
            std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            std::cout << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
            
            // Add log level
            std::cout << "[" << level_to_string(level) << "] ";
            
            // Print all arguments
            ((std::cout << args << " "), ...);
            std::cout << std::endl;
        }

        template<typename... Args>
        void debug(Args&&... args) { log(LogLevel::DEBUG, std::forward<Args>(args)...); }
        
        template<typename... Args>
        void info(Args&&... args) { log(LogLevel::INFO, std::forward<Args>(args)...); }
        
        template<typename... Args>
        void warn(Args&&... args) { log(LogLevel::WARN, std::forward<Args>(args)...); }
        
        template<typename... Args>
        void error(Args&&... args) { log(LogLevel::ERROR, std::forward<Args>(args)...); }

    private:
        std::mutex mutex_;
        LogLevel current_level_ = LogLevel::INFO;

        const char* level_to_string(LogLevel level) {
            switch (level) {
                case LogLevel::DEBUG: return "DEBUG";
                case LogLevel::INFO:  return "INFO ";
                case LogLevel::WARN:  return "WARN ";
                case LogLevel::ERROR: return "ERROR";
                default: return "UNKNOWN";
            }
        }
    };

    // Convenience macros
    #define LOG_DEBUG(...) CORE::Logger::instance().debug(__VA_ARGS__)
    #define LOG_INFO(...)  CORE::Logger::instance().info(__VA_ARGS__)
    #define LOG_WARN(...)  CORE::Logger::instance().warn(__VA_ARGS__)
    #define LOG_ERROR(...) CORE::Logger::instance().error(__VA_ARGS__)

} // namespace CORE