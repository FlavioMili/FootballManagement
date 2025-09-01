// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "team.h"

/**
  * The idea of this class is to later
  * support different managers in the same
  * game save, so that there is more variety
  * 
  * TODO improve manager class with traits ??? 
  * I think it would be a good idea to create a 
  * manager_traits.json with some strategy_parameters
  * so basically some Managers might have traits to use 
  * 3-5-2 or 4-3-3 and Gegenpress or Tiki-Taka
  * This would make simpler the managment and "coherency"
  * of an AI team's strategy.
*/
class Manager {
 public:
  explicit Manager(const Team& team);
  const Team& getTeam() const;

 private:
  const Team& team;
};
