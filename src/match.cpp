#include "match.h"
#include <iostream>

void Match::simulateMatch(const Team& teamA, const Team& teamB) {
    std::cout << "Match: " << teamA.getName() << " vs " << teamB.getName() << "\n";
    std::cout << "Stats: " << teamA.getStats() << " vs " << teamB.getStats() << "\n";

    if (teamA.getStats() > teamB.getStats()) {
        std::cout << "Result: " << teamA.getName() << " wins!\n";
    } else {
        std::cout << "Result: " << teamB.getName() << " wins!\n";
    }
}

