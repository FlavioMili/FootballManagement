#include "calendar.h"
#include <algorithm>
#include <iostream>

void Calendar::generateSeasonSchedule(const std::vector<Team>& teams) {
    schedule.clear();
    if (teams.size() < 2) {
        return;
    }

    std::vector<int> team_indices;
    for (size_t i = 0; i < teams.size(); ++i) {
        team_indices.push_back(i);
    }

    // If odd number of teams, add a dummy team
    bool dummy_added = false;
    if (team_indices.size() % 2 != 0) {
        team_indices.push_back(-1); // Dummy team
        dummy_added = true;
    }

    int num_teams = team_indices.size();
    int num_rounds = num_teams - 1;
    int half_size = num_teams / 2;

    for (int round = 0; round < num_rounds; ++round) {
        std::vector<std::pair<int, int>> week_matches;
        for (int i = 0; i < half_size; ++i) {
            int home = team_indices[i];
            int away = team_indices[num_teams - 1 - i];

            if (home != -1 && away != -1) {
                 // Alternate home/away
                if (round % 2 == 0) {
                    week_matches.push_back({home, away});
                } else {
                    week_matches.push_back({away, home});
                }
            }
        }
        schedule.push_back(week_matches);

        // Rotate teams
        int last_team = team_indices.back();
        team_indices.pop_back();
        team_indices.insert(team_indices.begin() + 1, last_team);
    }
    
    std::cout << "Generated a schedule with " << schedule.size() << " weeks." << std::endl;
}

const std::vector<std::vector<std::pair<int, int>>>& Calendar::getSchedule() const {
    return schedule;
}
