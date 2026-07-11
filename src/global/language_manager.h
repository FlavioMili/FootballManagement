// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2026 FlavioMili. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#ifndef LANGUAGE_MANAGER_H
#define LANGUAGE_MANAGER_H

#include <string>
#include <unordered_map>
#include "global/languages.h"

class LanguageManager
{
 public:
  static LanguageManager& instance() {
    static LanguageManager instance;
    return instance;
  }

  // Loads the JSON file for the specific language into memory
  bool loadLanguage(Language lang);

  // Retrieves the localized string. Returns the key itself if not found.
  const char* get(const char* key) const;

 private:
  LanguageManager() = default;
  std::unordered_map<std::string, std::string> translations;
};

// Global helper macro for concise UI code
#define LOC(key) LanguageManager::instance().get(key)

#endif // LANGUAGE_MANAGER_H
