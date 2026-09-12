// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

/**
 * Local crash reports. A fatal signal, an uncaught exception or an error
 * reaching main() writes <logs>/crash-<utc>.txt (version, build, reason and,
 * where available, a backtrace) and marks it pending; the next start shows a
 * notice and keeps the crashed session's log next to it as crash-<utc>.log.
 * Reports stay on this computer: nothing is ever sent anywhere.
 */
namespace CrashReport
{
/** A report the player has not seen yet. */
struct Pending
{
  std::filesystem::path report;
  std::filesystem::path log;  ///< Empty when the session log was not kept.
};

/**
 * Installs the fatal signal and std::terminate handlers. Call once at
 * startup, after Logger::init() (which keeps the previous session's log).
 */
void install();

/**
 * Writes a report for a fatal error the program caught itself (not for use
 * in signal handlers). Only the first report of a process is written.
 * @return The report file.
 */
std::filesystem::path write(std::string_view reason);

/** The report left by a crashed earlier session, if not acknowledged. */
std::optional<Pending> pending();

/** Marks the pending report as seen and keeps only the newest reports. */
void acknowledge();

/** Folder holding the reports and logs. */
std::filesystem::path directory();
}  // namespace CrashReport
