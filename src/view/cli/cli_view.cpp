#include "cli_view.h"
#include <iostream>
#include <algorithm>
#include <vector>

CliView::CliView(GameController& controller) : controller(controller) {}

void CliView::run() {
   while (true) {
      displayMenu();
      processInput();
   }
}

void CliView::displayMenu() {
   std::cout << "\n--- Football Manager ---\n";
   std::cout << "1. View Roster\n";
   std::cout << "2. View Leaderboard\n";
   std::cout << "3. Advance to next week\n";
   std::cout << "4. Exit\n";
   std::cout << "Enter your choice: ";
}

/**
* Later on this function should be changed to process 
* other types of menus, for instance strategy or others.
* the idea might be using ncurses later on????
*/
void CliView::processInput() {
   int choice;
   std::cin >> choice;

   switch (choice) {
      case 1:
         viewRoster();
         break;
      case 2:
         viewLeaderboard(controller.getManagedTeam().getLeagueId());
         break;
      case 3:
         controller.advanceWeek();
         break;
      case 4:
         exit(0);
      default:
         std::cout << "Invalid choice. Please try again.\n";
   }
}

void CliView::viewRoster() {
   Team team = controller.getManagedTeam();
   std::vector<Player> players = controller.getPlayersForTeam(team.getId());

   std::cout << " PLAYERS SIZE IN ROSTER: " << players.size() << "\n";

   std::cout << "\n--- Roster for " << team.getName() << " ---\n";
   for (const auto& p : players) {
      std::cout << p.getName() << " (Age: " << p.getAge() 
         << ", Role: " << p.getRole() << ") - Overall: "
         << p.getOverall() << "\n";
   }
}

void CliView::viewLeaderboard(int league_id) {
   std::vector<Team> teams = controller.getTeamsInLeague(league_id);
   std::cout << "\n--- Leaderboard ---\n";
   std::sort(teams.begin(), teams.end(), [](const Team& a, const Team& b) {
      return a.getPoints() > b.getPoints();
   });

   for (const auto& t : teams) {
      std::cout << t.getName() << ": " 
         << t.getPoints() << " points\n";
   }
}
