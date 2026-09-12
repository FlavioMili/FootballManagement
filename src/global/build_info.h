// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <string_view>

/** What was built and how: shown in the main menu and in crash reports. */
namespace BuildInfo
{
/** Game version without a leading "v", e.g. "1.0.0". */
std::string_view version();
/** Short commit hash the build was configured from, or "unknown". */
std::string_view commit();
/** Build configuration, e.g. "Release". */
std::string_view configuration();
/** Compiler name and version, e.g. "GCC 14.2.0". */
std::string_view compiler();
/** Target platform, e.g. "linux-x86_64". */
std::string_view platform();
/** One line with everything above, for logs and crash reports. */
std::string summary();
}  // namespace BuildInfo
