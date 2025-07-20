#include "league.h"
#include "match.h"
#include <iostream>
#include <algorithm>
#include <vector>

League::League(std::string name) : name(name) {}

League::~League() {}

void League::setName(std::string name) {
    this->name = name;
}

std::string League::getName() const {
    return name;
}

const std::vector<Team>& League::getTeams() const {
    return teams;
}

void League::overrideTeams(const std::vector<Team>& newTeams) {
    this->teams = newTeams;
    leaderboard.clear();
    for (const auto& team : this->teams) {
        leaderboard[team.getName()] = 0;
    }
}

void League::simulateSeason() {
    if (teams.empty()) {
        std::cout << "Cannot simulate season for an empty league." << std::endl;
        return;
    }
    std::cout << "\n--- Simulating season for " << name << " ---" << std::endl;
    
    calendar.generateSeasonSchedule(teams);
    const auto& schedule = calendar.getSchedule();

    for (size_t i = 0; i < schedule.size(); ++i) {
        std::cout << "\n--- Week " << i + 1 << " ---" << std::endl;
        const auto& week_matches = schedule[i];
        for (const auto& match_indices : week_matches) {
            Team& home_team = teams[match_indices.first];
            Team& away_team = teams[match_indices.second];
            Match match(home_team, away_team);
            MatchResult result = match.simulate();

            switch (result) {
                case MatchResult::HOME_WIN:
                    leaderboard[home_team.getName()] += 3;
                    break;
                case MatchResult::AWAY_WIN:
                    leaderboard[away_team.getName()] += 3;
                    break;
                case MatchResult::TIE:
                    leaderboard[home_team.getName()] += 1;
                    leaderboard[away_team.getName()] += 1;
                    break;
            }
        }
    }
    std::cout << "\n--- Season simulation for " << name << " finished ---\n" << std::endl;
    printLeaderboard();
}

void League::printLeaderboard() const {
    // Create a vector of pairs to sort
    std::vector<std::pair<std::string, int>> sorted_leaderboard;
    for (const auto& pair : leaderboard) {
        sorted_leaderboard.push_back(pair);
    }

    // Sort by points (descending)
    std::sort(sorted_leaderboard.begin(), sorted_leaderboard.end(), 
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    std::cout << "--- Final Leaderboard: " << name << " ---" << std::endl;
    for (const auto& pair : sorted_leaderboard) {
        std::cout << pair.first << ": " << pair.second << " pts" << std::endl;
    }
    std::cout << "------------------------------------" << std::endl;
}
