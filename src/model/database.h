#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "player.h"
#include "team.h"
#include "league.h"
#include "calendar.h"

class Database {
 public:
  explicit Database(const std::string& db_path);
  ~Database();

  void initialize() const;
  bool isFirstRun() const;

  void addLeague(const std::string& name) const;
  std::vector<League> getLeagues() const;

  void addTeam(int league_id, const std::string& name, uint64_t balance) const;
  std::vector<Team> getTeams(const int league_id) const;
  void updateTeam(const Team& team) const;

  void addPlayer(int team_id, const Player& player) const;
  std::vector<Player> getPlayers(int team_id) const;

  void saveCalendar(const Calendar& calendar, int season, int league_id) const;
  Calendar loadCalendar(int season, int league_id) const;

  void saveManagedTeamId(int team_id) const;
  int loadManagedTeamId();

  void updateGameState(int season, int week, int managed_team_id) const;
  void loadGameState(int& season, int& week, int& managed_team_id) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> pImpl;
};
