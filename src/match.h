#pragma once

#include "team.h"

enum class MatchResult {
    HOME_WIN,
    AWAY_WIN,
    TIE
};

class Match {
public:
    Match(Team& teamA, Team& teamB);
    MatchResult simulate();

private:
    Team& teamA;
    Team& teamB;
};