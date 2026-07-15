// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "role_utils.h"

std::string RoleUtils::toString(PlayerRole role)
{
  switch (role)
  {
    case PlayerRole::GK:
      return "GK";
    case PlayerRole::CB:
      return "CB";
    case PlayerRole::LB:
      return "LB";
    case PlayerRole::RB:
      return "RB";
    case PlayerRole::CDM:
      return "CDM";
    case PlayerRole::CM:
      return "CM";
    case PlayerRole::CAM:
      return "CAM";
    case PlayerRole::LM:
      return "LM";
    case PlayerRole::RM:
      return "RM";
    case PlayerRole::LW:
      return "LW";
    case PlayerRole::RW:
      return "RW";
    case PlayerRole::ST:
      return "ST";
    default:
      return "UNKNOWN";
  }
}

PlayerRole RoleUtils::fromString(std::string_view role_str)
{
  if (role_str == "GK") return PlayerRole::GK;
  if (role_str == "CB") return PlayerRole::CB;
  if (role_str == "LB") return PlayerRole::LB;
  if (role_str == "RB") return PlayerRole::RB;
  if (role_str == "CDM") return PlayerRole::CDM;
  if (role_str == "CM") return PlayerRole::CM;
  if (role_str == "CAM") return PlayerRole::CAM;
  if (role_str == "LM") return PlayerRole::LM;
  if (role_str == "RM") return PlayerRole::RM;
  if (role_str == "LW") return PlayerRole::LW;
  if (role_str == "RW") return PlayerRole::RW;
  if (role_str == "ST") return PlayerRole::ST;

  // Handle legacy strings for backwards compatibility if needed
  if (role_str == "Goalkeeper") return PlayerRole::GK;
  if (role_str == "Defender") return PlayerRole::CB;
  if (role_str == "Midfielder") return PlayerRole::CM;
  if (role_str == "Striker") return PlayerRole::ST;

  return PlayerRole::UNKNOWN;
}

Vector2F RoleUtils::getBaseCoordinate(PlayerRole role)
{
  // Assume Home team plays from Left (0.0) to Right (1.0).
  // Y goes from Top (0.0) to Bottom (1.0).
  switch (role)
  {
    case PlayerRole::GK:
      return {0.05f, 0.50f};
    case PlayerRole::CB:
      return {0.15f, 0.50f};
    case PlayerRole::LB:
      return {0.15f, 0.15f};
    case PlayerRole::RB:
      return {0.15f, 0.85f};
    case PlayerRole::CDM:
      return {0.30f, 0.50f};
    case PlayerRole::CM:
      return {0.45f, 0.50f};
    case PlayerRole::CAM:
      return {0.60f, 0.50f};
    case PlayerRole::LM:
      return {0.45f, 0.15f};
    case PlayerRole::RM:
      return {0.45f, 0.85f};
    case PlayerRole::LW:
      return {0.75f, 0.15f};
    case PlayerRole::RW:
      return {0.75f, 0.85f};
    case PlayerRole::ST:
      return {0.85f, 0.50f};
    default:
      return {0.50f, 0.50f};
  }
}

std::string RoleUtils::getBroadCategory(PlayerRole role)
{
  switch (role)
  {
    case PlayerRole::GK:
      return "Goalkeeper";
    case PlayerRole::CB:
    case PlayerRole::LB:
    case PlayerRole::RB:
      return "Defender";
    case PlayerRole::CDM:
    case PlayerRole::CM:
    case PlayerRole::CAM:
    case PlayerRole::LM:
    case PlayerRole::RM:
      return "Midfielder";
    case PlayerRole::LW:
    case PlayerRole::RW:
    case PlayerRole::ST:
      return "Striker";
    default:
      return "Unknown";
  }
}
