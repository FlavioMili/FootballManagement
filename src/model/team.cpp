// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "team.h"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

#include "finances.h"
#include "gamedata.h"

namespace
{
constexpr std::size_t RECENT_FORM_LENGTH = 5;
}

// Constructor
Team::Team(TeamID team_id, uint8_t team_league_id, std::string_view team_name,
           int64_t initial_balance,
           const std::vector<PlayerID>& initial_player_ids,
           const Strategy& strategy, const Lineup& lineup_data)
    : id(team_id),
      league_id(team_league_id),
      name(team_name),
      player_ids(initial_player_ids),
      team_strategy(strategy),
      lineup(lineup_data),
      finances(initial_balance)
{
}

// Accessors
uint16_t Team::getId() const { return id; }
uint8_t Team::getLeagueId() const { return league_id; }
void Team::setLeagueId(LeagueID new_league_id) { league_id = new_league_id; }
const std::string& Team::getName() const { return name; }

const std::vector<PlayerID>& Team::getPlayerIDs() const { return player_ids; }

void Team::addPlayerID(PlayerID player_id)
{
  if (!std::ranges::contains(player_ids, player_id))
  {
    player_ids.push_back(player_id);
  }
}

bool Team::removePlayerID(PlayerID player_id)
{
  if (std::erase(player_ids, player_id) > 0)
  {
    return true;
  }
  return false;
}

// Lineup access
Lineup& Team::getLineup() { return lineup; }
const Lineup& Team::getLineup() const { return lineup; }

// Strategy access
Strategy& Team::getStrategy() { return team_strategy; }
const Strategy& Team::getStrategy() const { return team_strategy; }
void Team::setStrategy(const Strategy& strategy) { team_strategy = strategy; }

// Generate best starting XI automatically
void Team::generateStartingXI(const class GameData& gamedata,
                              const StatsConfig& stats_config)
{
  // Academy players only fill in when the senior squad runs short.
  constexpr std::size_t MIN_SENIORS = 14;
  std::vector<PlayerID> available;
  std::vector<PlayerID> academy;
  available.reserve(player_ids.size());
  const auto& players = gamedata.getPlayers();
  for (const PlayerID player_id : player_ids)
  {
    const auto found = players.find(player_id);
    if (found == players.end() || !found->second.isAvailable()) continue;
    if (found->second.isAcademyPlayer())
      academy.push_back(player_id);
    else
      available.push_back(player_id);
  }
  if (available.size() < MIN_SENIORS)
    available.insert(available.end(), academy.begin(), academy.end());
  lineup.generateStartingXI(gamedata, available, stats_config);
}

// Finances access
Finances& Team::getFinances() noexcept { return finances; }
const Finances& Team::getFinances() const noexcept { return finances; }

// Club profile
const ClubProfile& Team::getProfile() const noexcept { return profile; }
void Team::setProfile(const ClubProfile& new_profile) { profile = new_profile; }
std::uint8_t Team::getReputation() const noexcept { return profile.reputation; }
std::uint32_t Team::getStadiumCapacity() const noexcept
{
  return profile.stadium_capacity;
}

const std::string& Team::getRecentForm() const noexcept { return recent_form; }

void Team::setRecentForm(std::string_view form)
{
  recent_form = form.substr(0, RECENT_FORM_LENGTH);
}

void Team::pushResult(MatchOutcome outcome)
{
  recent_form.insert(recent_form.begin(), static_cast<char>(outcome));
  if (recent_form.size() > RECENT_FORM_LENGTH)
    recent_form.resize(RECENT_FORM_LENGTH);
}
