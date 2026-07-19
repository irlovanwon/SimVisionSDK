/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Thread-safe leveled logger with timestamped output and rotation-ready files
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

namespace sim_vision {

enum class LogLevel : int {
    FATAL = 0,
    ERROR = 1,
    WARN  = 2,
    INFO  = 3,
    DEBUG = 4,
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    void set_file(const std::string& path);
    LogLevel level() const;

    void log(LogLevel level, const char* module, const std::string& message);

private:
    Logger();
    std::atomic<LogLevel> level_{LogLevel::INFO};
    std::ofstream file_;
    std::mutex mtx_;
};

void log_init(const std::string& level_str, const std::string& file_path);

#define SIM_LOG(lvl, mod, msg) \
    ::sim_vision::Logger::instance().log(lvl, mod, msg)

}  // namespace sim_vision
