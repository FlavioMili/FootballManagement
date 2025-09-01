// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "manager.h"

Manager::Manager(const Team& team) : team(team) {}

const Team& Manager::getTeam() const {
  return team;
}
