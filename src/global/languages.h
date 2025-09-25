// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

/* This is an enum used in the settings manager to set the
 * text language, to be supported in the future
 * It will ve used also as nationality of Players
*/
enum class Language : uint8_t {
  EN, IT, ES, FR, DE, MX, CN, RO,
  PT, NL, SE, PL, GR, TR, RU, AR,
  JP, BR, KR, BE, CH, HR, DK, FI,
  IE, NO, SK, CZ, HU, UA, US, 
};

const std::unordered_map<Language, std::string> languageToString{
    {Language::EN, "English"},    {Language::IT, "Italian"},
    {Language::ES, "Spanish"},    {Language::FR, "French"},
    {Language::DE, "German"},     {Language::MX, "Mexican"},
    {Language::CN, "Chinese"},    {Language::RO, "Romanian"},
    {Language::PT, "Portuguese"}, {Language::NL, "Dutch"},
    {Language::SE, "Swedish"},    {Language::PL, "Polish"},
    {Language::GR, "Greek"},      {Language::TR, "Turkish"},
    {Language::RU, "Russian"},    {Language::AR, "Arabic"},
    {Language::JP, "Japanese"},   {Language::BR, "Brazilian"},
    {Language::KR, "Korean"},     {Language::BE, "Belgian"},
    {Language::CH, "Swiss"},      {Language::HR, "Croatian"},
    {Language::DK, "Danish"},     {Language::FI, "Finnish"},
    {Language::IE, "Irish"},      {Language::NO, "Norwegian"},
    {Language::SK, "Slovak"},     {Language::CZ, "Czech"},
    {Language::HU, "Hungarian"},  {Language::UA, "Ukrainian"},
    {Language::US, "American"}};

const std::unordered_map<std::string, Language> stringToLanguage{
    {"English", Language::EN},    {"Italian", Language::IT},
    {"Spanish", Language::ES},    {"French", Language::FR},
    {"German", Language::DE},     {"Mexican", Language::MX},
    {"Chinese", Language::CN},    {"Romanian", Language::RO},
    {"Portuguese", Language::PT}, {"Dutch", Language::NL},
    {"Swedish", Language::SE},    {"Polish", Language::PL},
    {"Greek", Language::GR},      {"Turkish", Language::TR},
    {"Russian", Language::RU},    {"Arabic", Language::AR},
    {"Japanese", Language::JP},   {"Brazilian", Language::BR},
    {"Korean", Language::KR},     {"Belgian", Language::BE},
    {"Swiss", Language::CH},      {"Croatian", Language::HR},
    {"Danish", Language::DK},     {"Finnish", Language::FI},
    {"Irish", Language::IE},      {"Norwegian", Language::NO},
    {"Slovak", Language::SK},     {"Czech", Language::CZ},
    {"Hungarian", Language::HU},  {"Ukrainian", Language::UA},
    {"American", Language::US}};

