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

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "global/languages.h"
#include "global/number_format.h"

/**
 * @class LanguageManager
 * @brief Singleton class that manages localization and translations.
 *
 * English is always kept loaded as the fallback: a key missing from the
 * current language, or a language file that cannot be read at all, shows
 * the English text instead of the raw key. status() tells the UI when that
 * happened so it can say so.
 */
class LanguageManager
{
 public:
  /** @brief What the last load left in place. */
  struct Status
  {
    Language requested = Language::EN;
    /** The requested file could not be read: everything is English. */
    bool file_failed = false;
    /** English keys the requested file lacks (shown in English). */
    std::size_t missing_keys = 0;

    [[nodiscard]] bool fallback() const
    {
      return file_failed || missing_keys > 0;
    }
  };

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
   * @return True if the language was loaded; false when it fell back to
   * English for the whole file (English itself failing as well leaves the
   * raw keys).
   */
  bool loadLanguage(Language lang);

  /**
   * @brief Loads @p path as the texts of @p lang, with the same English
   * fallback as loadLanguage() (which calls it with the packaged file).
   */
  bool loadFromFile(const std::string& path, Language lang);

  /**
   * @brief Retrieves the localized string: the current language's, else
   * the English one, else the key itself (the same pointer).
   * @param key The key to look up the translation for.
   */
  const char* get(const char* key) const;

  /** @brief The language last requested (even if it fell back). */
  [[nodiscard]] Language current() const { return status_.requested; }

  /** @brief Outcome of the last load, for a notice in the UI. */
  [[nodiscard]] const Status& status() const { return status_; }

  /** @brief Number and money conventions of the loaded texts. */
  [[nodiscard]] const NumberFormat::Locale& numberLocale() const
  {
    return number_locale;
  }

 private:
  struct StringHash
  {
    using is_transparent = void;
    std::size_t operator()(std::string_view text) const
    {
      return std::hash<std::string_view>{}(text);
    }
  };
  using Table =
      std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;

  LanguageManager() = default;
  /** Reads the English table once; false when it cannot be read. */
  bool ensureEnglish();
  void refreshNumberLocale();

  /** Texts of the current language; empty while English is current. */
  Table translations;
  Table english;
  bool english_loaded = false;
  Status status_;
  NumberFormat::Locale number_locale;
};

/**
 * @def LOC
 * @brief Global helper macro for concise UI code localization.
 * @param key The key to translate.
 */
#define LOC(key) LanguageManager::instance().get(key)

#endif  // LANGUAGE_MANAGER_H
