#pragma once

#include "team.h"

/**
* The idea of this class is to later
* support different managers in the same
* game save, so that there is more variety
*/
class Manager {
public:
   Manager(const Team& team);
   const Team& getTeam() const;

private:
   const Team& team;
};
