#include "calendar.h"
#include <algorithm>
#include <vector>

void Calendar::generate(const std::vector<Team>& teams) {
   weeks.clear();
   if (teams.size() < 2) {
      return;
   }

   std::vector<int> team_ids;
   for (const auto& team : teams) {
      team_ids.push_back(team.getId());
   }

   if (team_ids.size() % 2 != 0) {
      team_ids.push_back(-1); // Dummy team for scheduling
   }

   int num_teams = team_ids.size();
   int num_rounds = (num_teams - 1) * 2; // Double for home and away

   for (int round = 0; round < num_rounds; ++round) {
      Week week(round);
      for (int i = 0; i < num_teams / 2; ++i) {
         int home_id = team_ids[i];
         int away_id = team_ids[num_teams - 1 - i];

         if (home_id != -1 && away_id != -1) {
            // Determine home/away based on round number for double round-robin
            if (round < (num_teams - 1)) { // First half of the season (original schedule)
               if (round % 2 == 1) {
                  std::swap(home_id, away_id);
               }
            } else { // Second half of the season (reversed fixtures)
               if (round % 2 == 0) { // Reverse home/away for second half
                  std::swap(home_id, away_id);
               }
            }
            week.addMatch({home_id, away_id});
         }
      }
      weeks.push_back(week);

      // Rotate team IDs
      int last_id = team_ids.back();
      team_ids.pop_back();
      team_ids.insert(team_ids.begin() + 1, last_id);
   }
}

const std::vector<Week>& Calendar::getWeeks() const {
   return weeks;
}

void Calendar::setWeeks(const std::vector<Week>& weeks) {
   this->weeks = weeks;
}
