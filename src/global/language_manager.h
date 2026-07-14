// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

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

/**
 * @class LanguageManager
 * @brief Singleton class that manages localization and translations.
 *
 * This class handles loading language files and providing translated strings
 * based on a key.
 */
class LanguageManager
{
 public:
  /**
   * @brief Gets the singleton instance of the LanguageManager.
   * @return A reference to the LanguageManager instance.
   */
  static LanguageManager& instance()
  {
    static LanguageManager instance;
    return instance;
  }

  /**
   * @brief Loads the JSON file for the specific language into memory.
   * @param lang The Language enum value to load.
   * @return True if the language was successfully loaded, false otherwise.
   */
  bool loadLanguage(Language lang);

  /**
   * @brief Retrieves the localized string. Returns the key itself if not found.
   * @param key The key to look up the translation for.
   * @return The localized string, or the key if no translation exists.
   */
  const char* get(const char* key) const;

 private:
  LanguageManager() = default;
  std::unordered_map<std::string, std::string> translations;
};

/**
 * @def LOC
 * @brief Global helper macro for concise UI code localization.
 * @param key The key to translate.
 */
#define LOC(key) LanguageManager::instance().get(key)

#endif  // LANGUAGE_MANAGER_H
