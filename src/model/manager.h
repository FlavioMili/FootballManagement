#pragma once

#include "team.h"

class Manager {
public:
   Manager(const Team& team);
   const Team& getTeam() const;

private:
   const Team& team;
};
