#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

namespace arbitrage {

class Logger {
private:
    static std::shared_ptr<spdlog::logger> logger_;
    static std::once_flag init_flag_;
    
    static void initialize();
    
public:
    static void init(const std::string& log_file, const std::string& log_level);
    
    static std::shared_ptr<spdlog::logger> get() {
        std::call_once(init_flag_, &Logger::initialize);
        return logger_;
    }
    
    template<typename... Args>
    static void trace(const std::string& fmt, Args&&... args) {
        get()->trace(fmt, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void debug(const std::string& fmt, Args&&... args) {
        get()->debug(fmt, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void info(const std::string& fmt, Args&&... args) {
        get()->info(fmt, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void warn(const std::string& fmt, Args&&... args) {
        get()->warn(fmt, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void error(const std::string& fmt, Args&&... args) {
        get()->error(fmt, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void critical(const std::string& fmt, Args&&... args) {
        get()->critical(fmt, std::forward<Args>(args)...);
    }
    
    static void set_level(const std::string& level);
    static void flush();
};

// Convenience macros
#define LOG_TRACE(...) arbitrage::Logger::trace(__VA_ARGS__)
#define LOG_DEBUG(...) arbitrage::Logger::debug(__VA_ARGS__)
#define LOG_INFO(...) arbitrage::Logger::info(__VA_ARGS__)
#define LOG_WARN(...) arbitrage::Logger::warn(__VA_ARGS__)
#define LOG_ERROR(...) arbitrage::Logger::error(__VA_ARGS__)
#define LOG_CRITICAL(...) arbitrage::Logger::critical(__VA_ARGS__)

} // namespace arbitrage