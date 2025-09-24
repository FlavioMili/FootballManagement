// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

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
