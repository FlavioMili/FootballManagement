#pragma once

#include "team.h"

class Match {
public:
    Match(const Team& teamA, const Team& teamB);
    void simulate();

private:
    Team teamA;
    Team teamB;
};