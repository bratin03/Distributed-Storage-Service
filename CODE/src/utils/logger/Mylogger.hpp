#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>

enum class LogLevel { DEBUG, INFO, WARNING, ERROR };

class MyLogger {
public:
    static void init(const std::string& filename = "");
    static void log(LogLevel level, const std::string& msg);
    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warning(const std::string& msg);
    static void error(const std::string& msg);

private:
    static std::mutex log_mutex;
    static std::ofstream log_file;
    static bool file_output;
    static std::string get_time_string();
    static std::string get_level_string(LogLevel level);
    static std::string get_color_code(LogLevel level);
};
