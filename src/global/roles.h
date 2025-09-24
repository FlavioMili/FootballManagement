// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

enum class Role : uint8_t {
  GKP, CB, LB, RB, LWB, RWB,
  CDM, CM, CAM, LM, RM, LWM, RWM,
  ST, CF, LW, RW, LF, RF
};

// These are used to parse the input easily
inline const std::unordered_map<std::string_view, Role> strToRole{
  {"GKP", Role::GKP}, {"CB", Role::CB},   {"LB", Role::LB},
  {"RB", Role::RB},   {"LWB", Role::LWB}, {"RWB", Role::RWB},
  {"CDM", Role::CDM}, {"CM", Role::CM},   {"CAM", Role::CAM},
  {"LM", Role::LM},   {"RM", Role::RM},   {"LWM", Role::LWM},
  {"RWM", Role::RWM}, {"ST", Role::ST},   {"CF", Role::CF},
  {"LW", Role::LW},   {"RW", Role::RW},   {"LF", Role::LF},
  {"RF", Role::RF}
};

inline const std::unordered_map<Role, std::string_view> roleToStr{
  {Role::GKP, "GKP"}, {Role::CB, "CB"},   {Role::LB, "LB"},
  {Role::RB, "RB"},   {Role::LWB, "LWB"}, {Role::RWB, "RWB"},
  {Role::CDM, "CDM"}, {Role::CM, "CM"},   {Role::CAM, "CAM"},
  {Role::LM, "LM"},   {Role::RM, "RM"},   {Role::LWM, "LWM"},
  {Role::RWM, "RWM"}, {Role::ST, "ST"},   {Role::CF, "CF"},
  {Role::LW, "LW"},   {Role::RW, "RW"},   {Role::LF, "LF"},
  {Role::RF, "RF"}
};
