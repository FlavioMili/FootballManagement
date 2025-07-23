#include "cli_view.h"
#include <iostream>
#include <algorithm>

CliView::CliView(GameController& controller) : controller(controller) {}

void CliView::run() {
    while (true) {
        displayMenu();
        processInput();
    }
}

void CliView::displayMenu() {
    std::cout << "\n--- Football Manager ---" << std::endl;
    std::cout << "1. View Roster" << std::endl;
    std::cout << "2. View Leaderboard" << std::endl;
    std::cout << "3. Advance to next week" << std::endl;
    std::cout << "4. Exit" << std::endl;
    std::cout << "Enter your choice: ";
}

void CliView::processInput() {
    int choice;
    std::cin >> choice;

    switch (choice) {
        case 1:
            viewRoster();
            break;
        case 2:
            viewLeaderboard(controller.getManagedTeam().getLeagueId()); // Pass managed league ID
            break;
        case 3:
            controller.advanceWeek();
            break;
        case 4:
            exit(0);
        default:
            std::cout << "Invalid choice. Please try again." << std::endl;
    }
}

void CliView::viewRoster() {
    Team team = controller.getManagedTeam();
    std::vector<Player> players = controller.getPlayersForTeam(team.getId());
    std::cout << "\n--- Roster for " << team.getName() << " ---" << std::endl;
    for (const auto& player : players) {
        std::cout << player.getName() << " (Age: " << player.getAge() << ", Role: " << player.getRole() << ") - Overall: " << player.getOverall() << std::endl;
    }
}

void CliView::viewLeaderboard(int league_id) {
    std::vector<Team> teams = controller.getTeamsInLeague(league_id);
    std::cout << "\n--- Leaderboard ---" << std::endl;
    std::sort(teams.begin(), teams.end(), [](const Team& a, const Team& b) {
        return a.getPoints() > b.getPoints();
    });

    for (const auto& team : teams) {
        std::cout << team.getName() << ": " << team.getPoints() << " points" << std::endl;
    }
}