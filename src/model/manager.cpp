#include "manager.h"

Manager::Manager(const Team& team) : team(team) {}

const Team& Manager::getTeam() const {
  return team;
}
