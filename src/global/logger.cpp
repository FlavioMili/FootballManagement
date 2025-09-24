// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "logger.h"
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "paths.h"

namespace {
std::shared_ptr<spdlog::logger> logger;
}

void Logger::init() {
  if (!logger) {
    auto file_sink    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(LOGGER_PATH, true);
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    logger = std::make_shared<spdlog::logger>(
      "main_logger", spdlog::sinks_init_list{console_sink, file_sink}
    );

    logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
    logger->set_level(spdlog::level::debug);
    spdlog::register_logger(logger);
  }
}

void Logger::info(const std::string& msg)  { logger->info(msg); }
void Logger::error(const std::string& msg) { logger->error(msg); }
void Logger::warn(const std::string& msg)  { logger->warn(msg); }

#ifdef DEBUG
  void Logger::debug(const std::string& msg) { logger->debug(msg); }
#else 
  void Logger::debug(const std::string& msg) { (void)msg; }
#endif
