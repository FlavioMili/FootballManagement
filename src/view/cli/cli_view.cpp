#include "cli_view.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <limits>
#include <iomanip>

CliView::CliView(GameController& controller) : controller(controller) {}

void CliView::run() {
  if (!controller.hasSelectedTeam()) {
    chooseManagedTeamMenu();
  }

  bool running = true;
  while (running) {
    displayMenu();
    running = processInput();
  }
}

void CliView::chooseManagedTeamMenu() {
  std::vector<Team> available = controller.getAvailableTeams();

  if (available.empty()) {
    throw std::runtime_error("No teams available to manage.");
  }

  std::cout << "\n--- Choose Your Team ---\n";
  for (size_t i = 0; i < available.size(); ++i) {
    std::cout << (i + 1) << ". " << available[i].getName() << "\n";
  }

  int choice = -1;
  while (true) {
    std::cout << "Enter team number: ";
    std::cin >> choice;

    if (!std::cin.fail() && choice >= 1 && choice <= static_cast<int>(available.size())) {
      controller.selectManagedTeam(available[choice - 1].getId());
      break;
    } else {
      std::cin.clear();
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << "Invalid input. Try again.\n";
    }
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
bool CliView::processInput() {
  int choice = -1;
  std::cout << "Enter your choice: ";
  std::cin >> choice;

  if (std::cin.fail() || choice < 1 || choice > 4) {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << "Invalid input. Please enter a number between 1 and 4.\n";
    return true;
  }

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
      return false;
  }
  return true;
}

void CliView::viewRoster() {
  Team team = controller.getManagedTeam();
  std::vector<Player> players = controller.getPlayersForTeam(team.getId());

  std::cout << " PLAYERS SIZE IN ROSTER: " << players.size() << "\n";

  std::cout << "\n--- Roster for " << team.getName() << " ---\n";
  for (const auto& p : players) {
    std::cout << p.getName() << " (Age: " << p.getAge() 
      << ", Role: " << p.getRole() << ") - Overall: "
      << std::fixed << std::setprecision(2)
      << p.getOverall(controller.getStatsConfig()) << "\n";
  }
}

void CliView::viewLeaderboard(int league_id) {
  League& league = controller.getLeagueById(league_id);
  std::vector<Team> teams = controller.getTeamsInLeague(league_id);

  std::cout << "\n--- Leaderboard ---\n";
  std::sort(teams.begin(), teams.end(), [&](const Team& a, const Team& b) {
    return league.getPoints(a.getId()) > league.getPoints(b.getId());
  });

  for (const auto& t : teams) {
    std::cout << t.getName() << ": " 
      << league.getPoints(t.getId()) << " points\n";
  }
}
