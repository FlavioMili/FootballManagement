// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "global/runtime_paths.h"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace
{
std::filesystem::path ensureDirectory(std::filesystem::path path)
{
  std::filesystem::create_directories(path);
  return path;
}

void removeDatabaseFiles(const std::filesystem::path& database)
{
  std::error_code error;
  std::filesystem::remove(database, error);
  std::filesystem::remove(database.string() + "-wal", error);
  std::filesystem::remove(database.string() + "-shm", error);
}

unsigned long processId()
{
#if defined(_WIN32)
  return static_cast<unsigned long>(_getpid());
#else
  return static_cast<unsigned long>(getpid());
#endif
}
}  // namespace

std::filesystem::path RuntimePaths::root()
{
  if (const char* configured = std::getenv("FM_RUNTIME_ROOT");
      configured != nullptr && *configured != '\0')
  {
    return ensureDirectory(std::filesystem::absolute(configured));
  }

  if (const char* testRoot = std::getenv("FM_TEST_RUNTIME_ROOT");
      testRoot != nullptr && *testRoot != '\0')
  {
    return ensureDirectory(std::filesystem::absolute(testRoot) /
                           std::to_string(processId()));
  }

  char* prefPath = SDL_GetPrefPath("FlavioMili", "FootballManagement");
  if (prefPath != nullptr)
  {
    std::filesystem::path result = ensureDirectory(prefPath);
    SDL_free(prefPath);
    return result;
  }

  return ensureDirectory(std::filesystem::temp_directory_path() /
                         "FootballManagement");
}

std::filesystem::path RuntimePaths::saveDirectory()
{
  return ensureDirectory(root() / "saves");
}

std::filesystem::path RuntimePaths::savePath(int slot)
{
  if (slot < 0) throw std::invalid_argument("Save slot cannot be negative");
  return saveDirectory() / ("save_" + std::to_string(slot) + ".db");
}

std::filesystem::path RuntimePaths::settingsPath()
{
  return root() / "settings.json";
}

std::filesystem::path RuntimePaths::logPath()
{
  return ensureDirectory(root() / "logs") / "log.txt";
}

std::filesystem::path RuntimePaths::capturePath(const char* filename)
{
  return ensureDirectory(root() / "captures") / filename;
}

std::filesystem::path RuntimePaths::imguiIniPath()
{
  return root() / "imgui.ini";
}

void RuntimePaths::removeSave(int slot) { removeDatabaseFiles(savePath(slot)); }

void RuntimePaths::removeAllSaves()
{
  std::error_code error;
  std::filesystem::remove_all(saveDirectory(), error);
}
