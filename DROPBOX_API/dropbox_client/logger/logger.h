#ifndef LOGGER_H
#define LOGGER_H

#define DEBUG

#include <iostream>
#include <string>

#ifdef DEBUG
class Logger {
public:
  static void debug(const std::string &msg) {
    std::cout << "\033[1;94m[DEBUG] " << msg << "\033[0m" << std::endl; // Light blue
  }
  static void info(const std::string &msg) {
    std::cout << "\033[1;32m[INFO] " << msg << "\033[0m" << std::endl; // Light green
  }
  static void warning(const std::string &msg) {
    std::cout << "\033[1;33m[WARNING] " << msg << "\033[0m" << std::endl; // Yellow
  }
  static void error(const std::string &msg) {
    std::cerr << "\033[1;35m[ERROR] " << msg << "\033[0m" << std::endl; // Magenta
  }
};
#else
class Logger {
public:
  static void debug(const std::string &) {}
  static void info(const std::string &) {}
  static void warning(const std::string &) {}
  static void error(const std::string &) {}
};
#endif

#endif // LOGGER_H
