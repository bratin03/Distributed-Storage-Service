#include "Mylogger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

std::mutex MyLogger::log_mutex;
std::ofstream MyLogger::log_file;
bool MyLogger::file_output = false;

void MyLogger::init(const std::string& filename) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!filename.empty()) {
        log_file.open(filename, std::ios::app);
        if (log_file.is_open()) {
            file_output = true;
        } else {
            std::cerr << "Failed to open log file: " << filename << std::endl;
        }
    }
}

void MyLogger::log(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::string time_str = get_time_string();
    std::string level_str = get_level_string(level);
    std::string color_code = get_color_code(level);
    std::string reset_code = "\033[0m";

    std::stringstream ss;
    ss << color_code << "[" << level_str << "] " << msg << reset_code;
    std::cout << ss.str() << std::endl;

    if (file_output) {
        log_file << "[" << time_str << "] [" << level_str << "] " << msg << std::endl;
    }
}

void MyLogger::debug(const std::string& msg) {
    log(LogLevel::DEBUG, msg);
}

void MyLogger::info(const std::string& msg) {
    log(LogLevel::INFO, msg);
}

void MyLogger::warning(const std::string& msg) {
    log(LogLevel::WARNING, msg);
}

void MyLogger::error(const std::string& msg) {
    log(LogLevel::ERROR, msg);
}

std::string MyLogger::get_time_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string MyLogger::get_level_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string MyLogger::get_color_code(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "\033[1;94m"; // Light blue
        case LogLevel::INFO: return "\033[1;32m";  // Light green
        case LogLevel::WARNING: return "\033[1;33m"; // Yellow
        case LogLevel::ERROR: return "\033[1;35m"; // Magenta
        default: return "\033[0m";
    }
}
