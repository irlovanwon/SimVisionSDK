/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Thread-safe leveled logger (stderr + optional file, timestamped)
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/common/Logger.h"
#include "sim_vision/common/Types.h"

#include <cstdio>

namespace sim_vision {

namespace {
const char* level_name(LogLevel l) {
    switch (l) {
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::DEBUG: return "DEBUG";
    }
    return "INFO";
}

LogLevel parse_level(const std::string& s) {
    if (s == "fatal") return LogLevel::FATAL;
    if (s == "error") return LogLevel::ERROR;
    if (s == "warn")  return LogLevel::WARN;
    if (s == "debug") return LogLevel::DEBUG;
    return LogLevel::INFO;
}
}  // namespace

Logger::Logger() = default;

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lk(mtx_);
    level_.store(level);
}

void Logger::set_file(const std::string& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (file_.is_open()) file_.close();
    if (!path.empty()) {
        file_.open(path, std::ios::out | std::ios::app);
    }
}

LogLevel Logger::level() const {
    return level_.load();
}

void Logger::log(LogLevel level, const char* module, const std::string& message) {
    if (static_cast<int>(level) > static_cast<int>(level_.load())) return;
    std::string line = format_log_timestamp() + " | " + level_name(level) +
                       " | " + (module ? module : "") + " | " + message + "\n";
    std::lock_guard<std::mutex> lk(mtx_);
    std::fwrite(line.c_str(), 1, line.size(), stderr);
    if (file_.is_open()) {
        file_ << line;
        if (level == LogLevel::FATAL || level == LogLevel::ERROR) file_.flush();
    }
}

void log_init(const std::string& level_str, const std::string& file_path) {
    Logger::instance().set_level(parse_level(level_str));
    if (!file_path.empty()) Logger::instance().set_file(file_path);
}

}  // namespace sim_vision
