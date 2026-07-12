// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2026 FlavioMili. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "language_manager.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "global/paths.h"

using json = nlohmann::json;

bool LanguageManager::loadLanguage(Language lang)
{
  translations.clear();

  // Map the Language enum to a filename
  auto it = languageToString.find(lang);
  if (it == languageToString.end()) return false;

  std::string filePath =
      std::string(PROJECT_ROOT) + "assets/lang/" + it->second + ".json";
  std::ifstream file(filePath);
  if (!file.is_open())
  {
    std::cerr << "Could not open language file: " << filePath << "\n";
    return false;
  }

  try
  {
    json j;
    file >> j;
    for (auto& [key, value] : j.items())
    {
      translations[key] = value.get<std::string>();
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error parsing language JSON: " << e.what() << "\n";
    return false;
  }
  return true;
}

const char* LanguageManager::get(const char* key) const
{
  if (auto it = translations.find(key); it != translations.end())
  {
    return it->second.c_str();
  }
  return key;  // Fallback to returning the literal key pointer if translation
               // is missing
}
