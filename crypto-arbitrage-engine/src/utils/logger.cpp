#include "logger.h"
#include "core/constants.h"
#include <iostream>
#include <filesystem>

namespace arbitrage {

std::shared_ptr<spdlog::logger> Logger::logger_;
std::once_flag Logger::init_flag_;

void Logger::initialize() {
    // Default initialization
    std::vector<spdlog::sink_ptr> sinks;
    
    // Console sink with color
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);
    sinks.push_back(console_sink);
    
    // Create logger with sinks
    logger_ = std::make_shared<spdlog::logger>("arbitrage", sinks.begin(), sinks.end());
    logger_->set_level(spdlog::level::trace);
    logger_->set_pattern(constants::logging::DEFAULT_LOG_PATTERN);
    
    // Register as default logger
    spdlog::register_logger(logger_);
}

void Logger::init(const std::string& log_file, const std::string& log_level) {
    std::call_once(init_flag_, [&log_file, &log_level]() {
        try {
            std::vector<spdlog::sink_ptr> sinks;
            
            // Console sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);
            sinks.push_back(console_sink);
            
            // File sink with rotation
            if (!log_file.empty()) {
                // Create log directory if it doesn't exist
                std::filesystem::path log_path(log_file);
                std::filesystem::create_directories(log_path.parent_path());
                
                auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    log_file, 
                    constants::logging::MAX_LOG_FILE_SIZE, 
                    constants::logging::MAX_LOG_FILES
                );
                file_sink->set_level(spdlog::level::trace);
                sinks.push_back(file_sink);
            }
            
            // Create logger
            logger_ = std::make_shared<spdlog::logger>("arbitrage", sinks.begin(), sinks.end());
            logger_->set_pattern(constants::logging::DEFAULT_LOG_PATTERN);
            
            // Set level
            set_level(log_level);
            
            // Enable backtrace
            logger_->enable_backtrace(32);
            
            // Register as default logger
            spdlog::register_logger(logger_);
            
            logger_->info("Logger initialized - Level: {}, File: {}", 
                         log_level, log_file.empty() ? "console only" : log_file);
            
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
            // Fall back to console only
            logger_ = spdlog::stdout_color_mt("arbitrage");
            logger_->error("Failed to initialize file logger, using console only");
        }
    });
}

void Logger::set_level(const std::string& level) {
    if (!logger_) {
        initialize();
    }
    
    spdlog::level::level_enum log_level = spdlog::level::info;
    
    if (level == "trace") {
        log_level = spdlog::level::trace;
    } else if (level == "debug") {
        log_level = spdlog::level::debug;
    } else if (level == "info") {
        log_level = spdlog::level::info;
    } else if (level == "warn" || level == "warning") {
        log_level = spdlog::level::warn;
    } else if (level == "error") {
        log_level = spdlog::level::err;
    } else if (level == "critical") {
        log_level = spdlog::level::critical;
    } else if (level == "off") {
        log_level = spdlog::level::off;
    }
    
    logger_->set_level(log_level);
    logger_->flush_on(log_level);
}

void Logger::flush() {
    if (logger_) {
        logger_->flush();
    }
}

} // namespace arbitrage