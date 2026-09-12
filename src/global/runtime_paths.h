// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <filesystem>

/** Runtime locations for files that must never be written into game assets. */
class RuntimePaths
{
 public:
  /**
   * Returns the writable root. FM_RUNTIME_ROOT is an explicit test/automation
   * override; otherwise SDL's platform-specific preference location is used.
   * FM_TEST_RUNTIME_ROOT gives each process its own <root>/<pid> directory,
   * removed at process exit unless FM_KEEP_TEST_ARTIFACTS is set (non-"0").
   */
  static std::filesystem::path root();
  static std::filesystem::path saveDirectory();
  static std::filesystem::path savePath(int slot);
  static std::filesystem::path settingsPath();
  static std::filesystem::path logPath();
  static std::filesystem::path capturePath(const char* filename);
  static std::filesystem::path imguiIniPath();

  /** Removes only save files owned by this runtime root, including WAL files. */
  static void removeSave(int slot);
  static void removeAllSaves();
};
