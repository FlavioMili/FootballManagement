#include "match.h"
#include <iostream>

Match::Match(const Team& a, const Team& b) : teamA(a), teamB(b) {}

void Match::simulate() {
    std::cout << "Simulating match between " << teamA.getName() << " and " << teamB.getName() << std::endl;
    // Future logic for match simulation will go here.
}