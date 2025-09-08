#pragma once
#include <string>

class Logger {
public:
  static void init();

  static void info(const std::string& msg);
  static void error(const std::string& msg);
  static void debug(const std::string& msg);
  static void warn(const std::string& msg);
};
