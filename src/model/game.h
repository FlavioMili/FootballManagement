#pragma once

#include <cstdint>
#include <vector>
#include <memory>

#include "model/gamedate.h"
#include "model/calendar.h"
#include "model/match.h"
#include "database/database.h"

class Game {
public:
  Game();

  // Simulation
  void advanceDay();

  // Accessors
  const GameDateValue &getCurrentDate() const;
  const Calendar &getCalendar() const;
  int getCurrentSeason() const;
  uint16_t getManagedTeamId() const;
  void setManagedTeamId(uint16_t id);

  // Save / load game state
  void saveGame();

private:
  void loadGame();
  void endSeason();
  void handleSeasonTransition();
  void startNewSeason();

  // Player training
  void trainPlayers(const std::vector<uint32_t> &player_ids);

  // Matchday simulation helper
  void simulateMatches(std::vector<Match> &matches);
  void updateStandings(const Match &match);

  std::shared_ptr<Database> db;
  Calendar calendar;
  GameDateValue currentDate;
  uint8_t current_season = 1;
  uint16_t managed_team_id;
};
