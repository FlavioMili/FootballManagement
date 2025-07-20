#include "match.h"
#include <iostream>
#include <cmath>
#include <random>

Match::Match(Team& a, Team& b) : teamA(a), teamB(b) {}

// Helper to get a random number
int get_random_goals(double strength_factor) {
    std::random_device rd;
    std::mt19937 gen(rd());
    // The higher the strength factor, the more likely to score more goals
    std::poisson_distribution<> d(strength_factor);
    return d(gen);
}

MatchResult Match::simulate() {
    // Calculate team strengths based on key stats
    double strengthA = (teamA.getAverageStat("Shooting") * 0.4) +
                       (teamA.getAverageStat("Passing") * 0.3) +
                       (teamA.getAverageStat("Dribbling") * 0.1) +
                       (teamA.getAverageStat("Vision") * 0.2);

    double strengthB = (teamB.getAverageStat("Shooting") * 0.4) +
                       (teamB.getAverageStat("Passing") * 0.3) +
                       (teamB.getAverageStat("Dribbling") * 0.1) +
                       (teamB.getAverageStat("Vision") * 0.2);

    // Normalize strength and add some randomness
    double strengthFactorA = (strengthA / 100.0) * 2.5 + (get_random_goals(0.5) / 2.0);
    double strengthFactorB = (strengthB / 100.0) * 2.5 + (get_random_goals(0.5) / 2.0);

    int goalsA = get_random_goals(strengthFactorA);
    int goalsB = get_random_goals(strengthFactorB);

    std::cout << "  " << teamA.getName() << " " << goalsA << " - " << goalsB << " " << teamB.getName() << std::endl;

    if (goalsA > goalsB) {
        return MatchResult::HOME_WIN;
    } else if (goalsB > goalsA) {
        return MatchResult::AWAY_WIN;
    } else {
        return MatchResult::TIE;
    }
}
