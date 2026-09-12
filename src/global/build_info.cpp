// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "global/build_info.h"

#include <format>

#include "global/version.h"

// Set by src/CMakeLists.txt on this file only, so a new commit rebuilds
// nothing else.
#ifndef FM_BUILD_COMMIT
#define FM_BUILD_COMMIT "unknown"
#endif
#ifndef FM_BUILD_CONFIG
#define FM_BUILD_CONFIG "unknown"
#endif
#ifndef FM_BUILD_COMPILER
#define FM_BUILD_COMPILER "unknown"
#endif
#ifndef FM_BUILD_PLATFORM
#define FM_BUILD_PLATFORM "unknown"
#endif

namespace BuildInfo
{
std::string_view version() { return GameVersion::STRING; }
std::string_view commit() { return FM_BUILD_COMMIT; }
std::string_view configuration() { return FM_BUILD_CONFIG; }
std::string_view compiler() { return FM_BUILD_COMPILER; }
std::string_view platform() { return FM_BUILD_PLATFORM; }

std::string summary()
{
  return std::format("Football Management {} (commit {}, {}, {}, {})",
                     version(), commit(), configuration(), compiler(),
                     platform());
}
}  // namespace BuildInfo
