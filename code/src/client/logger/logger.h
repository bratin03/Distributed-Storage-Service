#include <iostream>

#define DEBUG 1

#ifndef LOGGER_H
#define LOGGER_H

#ifdef DEBUG
class Logger {
public:
  static void info(const std::string &msg) {
    std::cout << "\033[1;32m[INFO] " << msg << "\033[0m" << std::endl; // Light green
  }
  static void error(const std::string &msg) {
    std::cerr << "\033[1;35m[ERROR] " << msg << "\033[0m" << std::endl; // Magenta
  }
};
#else
class Logger {
public:
  static void info(const std::string &) {}
  static void error(const std::string &) {}
};
#endif

#endif // LOGGER_H