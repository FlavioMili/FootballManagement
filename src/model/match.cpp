// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "match.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "database/gamedata.h"
#include "lineup.h"
#include "model/competition.h"
#include "model/match_engine.h"
#include "model/match_report.h"
#include "model/match_scheduler.h"
#include "model/world_simulation.h"
#include "player.h"

Match::Match(TeamID home_id, TeamID away_id, GameDateValue date, MatchType type,
             LeagueID competition, uint8_t round_stage)
    : home_team_id(home_id),
      away_team_id(away_id),
      match_date(date),
      match_type(type),
      home_score(0),
      away_score(0),
      competition_id(competition),
      stage(round_stage)
{
}

uint32_t Match::getSeed() const
{
  return (static_cast<uint32_t>(home_team_id) << 16U) ^
         static_cast<uint32_t>(away_team_id) ^
         (static_cast<uint32_t>(match_date.year) << 8U) ^
         (static_cast<uint32_t>(match_date.month) << 4U) ^ match_date.day;
}

void Match::simulate(const GameData& game_data, MatchReport* report,
                     std::vector<PlayerMatchConsequence>* consequences)
{
  const std::optional<MatchSimulationInput> input =
      prepareSimulation(game_data);
  if (!input) return;
  MatchSimulationResult result =
      MatchSimulation::run(*input, game_data.getStatsConfig());
  if (consequences) *consequences = std::move(result.consequences);
  MatchReport simulated = applySimulation(std::move(result));
  if (report) *report = std::move(simulated);
}

std::optional<MatchSimulationInput> Match::prepareSimulation(
    const GameData& game_data) const
{
  if (_played) return std::nullopt;
  const auto home_team = game_data.getTeam(home_team_id);
  const auto away_team = game_data.getTeam(away_team_id);
  if (!home_team || !away_team) return std::nullopt;

  MatchSimulationInput input;
  input.home_id = home_team_id;
  input.away_id = away_team_id;
  input.seed = getSeed();
  input.home_lineup = home_team->get().getLineup();
  input.away_lineup = away_team->get().getLineup();
  input.home_strategy = home_team->get().getStrategy();
  input.away_strategy = away_team->get().getStrategy();
  // Seeded by the fixture alone, so drawing it up front for every tie gives
  // the same extra time as drawing it after a level 90 minutes.
  if (isKnockout())
    input.knockout = Competitions::resolveDrawnKnockout(
        home_team->get(), away_team->get(), game_data.getStatsConfig(),
        input.seed);
  return input;
}

MatchReport Match::applySimulation(MatchSimulationResult result)
{
  if (result.extra_time)
    setKnockoutResult(result.home_goals, result.away_goals, true,
                      result.penalties);
  else
    setPlayedResult(result.home_goals, result.away_goals);
  writeResultTo(result.report);
  return std::move(result.report);
}

void Match::setPlayedResult(uint8_t h, uint8_t a)
{
  home_score = h;
  away_score = a;
  _played = true;
}

void Match::setKnockoutResult(
    uint8_t h, uint8_t a, bool went_to_extra_time,
    std::optional<std::pair<uint8_t, uint8_t>> shootout)
{
  setPlayedResult(h, a);
  extra_time = went_to_extra_time;
  penalties = shootout.has_value();
  home_penalties = shootout ? shootout->first : 0;
  away_penalties = shootout ? shootout->second : 0;
}

std::optional<TeamID> Match::getWinnerId() const
{
  if (!_played) return std::nullopt;
  if (home_score != away_score)
    return home_score > away_score ? home_team_id : away_team_id;
  if (penalties && home_penalties != away_penalties)
    return home_penalties > away_penalties ? home_team_id : away_team_id;
  return std::nullopt;
}

void Match::writeResultTo(MatchReport& report) const
{
  constexpr uint8_t EXTRA_TIME_MINUTES = 120;
  report.date = match_date;
  report.home_team_id = home_team_id;
  report.away_team_id = away_team_id;
  report.match_type = match_type;
  report.competition_id = competition_id;
  report.stage = stage;
  report.home_goals = home_score;
  report.away_goals = away_score;
  report.extra_time = extra_time;
  report.penalties = penalties;
  report.home_penalties = home_penalties;
  report.away_penalties = away_penalties;
  if (extra_time)
  {
    for (PlayerMatchLine& line : report.players)
      if (line.started) line.minutes = EXTRA_TIME_MINUTES;
  }
}

uint16_t Match::getHomeTeamId() const { return home_team_id; }
uint16_t Match::getAwayTeamId() const { return away_team_id; }
uint8_t Match::getHomeScore() const { return home_score; }
uint8_t Match::getAwayScore() const { return away_score; }
MatchType Match::getMatchType() const { return match_type; }
const GameDateValue& Match::getDate() const { return match_date; }

bool Match::isPlayed() const { return _played; }

// ---------------------------------------------------------------------------
// Matchday squad
// ---------------------------------------------------------------------------

namespace
{
bool isSelected(const Lineup& lineup, const Player* player)
{
  return lineup.getGoalkeeper() == player ||
         std::ranges::any_of(lineup.getOutfieldPlayers(),
                             [player](const Lineup::PositionedPlayer& slot)
                             { return slot.player == player; }) ||
         std::ranges::contains(lineup.getReserves(), player);
}
}  // namespace

std::vector<PlayerID> MatchdaySquad::ineligible(const Lineup& lineup,
                                                const Eligibility& eligible)
{
  std::vector<PlayerID> result;
  const auto check = [&](const Player* player)
  {
    if (player && !eligible(*player)) result.push_back(player->getId());
  };
  check(lineup.getGoalkeeper());
  for (const Lineup::PositionedPlayer& slot : lineup.getOutfieldPlayers())
    check(slot.player);
  for (const Player* reserve : lineup.getReserves()) check(reserve);
  return result;
}

std::size_t MatchdaySquad::replaceIneligible(
    Lineup& lineup, std::span<const Player* const> squad,
    const Eligibility& eligible, const StatsConfig& config)
{
  // Unselected eligible players, best first.
  std::vector<std::pair<double, const Player*>> ranked;
  for (const Player* player : squad)
  {
    if (player && eligible(*player) && !isSelected(lineup, player))
      ranked.emplace_back(player->getOverall(config), player);
  }
  std::ranges::sort(ranked,
                    [](const auto& left, const auto& right)
                    {
                      if (left.first != right.first)
                        return left.first > right.first;
                      return left.second->getId() < right.second->getId();
                    });
  std::vector<const Player*> pool;
  pool.reserve(ranked.size());
  for (const auto& entry : ranked) pool.push_back(entry.second);

  const std::size_t before = ineligible(lineup, eligible).size();
  const std::size_t reserveCount = lineup.getReserves().size();

  std::vector<const Player*> starters;
  if (lineup.getGoalkeeper()) starters.push_back(lineup.getGoalkeeper());
  for (const Lineup::PositionedPlayer& slot : lineup.getOutfieldPlayers())
    if (slot.player) starters.push_back(slot.player);

  for (const Player* starter : starters)
  {
    if (eligible(*starter)) continue;
    const bool keeperSlot = starter == lineup.getGoalkeeper();
    const auto isKeeper = [](const Player* player)
    { return player->getRole() == PlayerRole::GK; };
    const std::array<std::function<bool(const Player*)>, 3> preferences = {
        [&](const Player* player)
        { return player->getRole() == starter->getRole(); },
        [&](const Player* player) { return isKeeper(player) == keeperSlot; },
        // Last resort: an outfield player in goal beats an injured keeper.
        [&](const Player*) { return keeperSlot; }};

    const Player* replacement = nullptr;
    bool fromPool = false;
    for (const auto& preferred : preferences)
    {
      const auto& reserves = lineup.getReserves();
      if (const auto reserve = std::ranges::find_if(
              reserves, [&](const Player* player)
              { return player && eligible(*player) && preferred(player); });
          reserve != reserves.end())
      {
        replacement = *reserve;
        break;
      }
      if (const auto candidate = std::ranges::find_if(pool, preferred);
          candidate != pool.end())
      {
        replacement = *candidate;
        fromPool = true;
        pool.erase(candidate);
        break;
      }
    }
    if (replacement == nullptr) continue;
    if (fromPool)
    {
      std::vector<const Player*> reserves = lineup.getReserves();
      reserves.push_back(replacement);
      lineup.setReserves(reserves);
    }
    lineup.swapPlayers(replacement->getId(), starter->getId());
  }

  // Unavailable reserves (including the starters just benched) leave the
  // squad; the best remaining players fill the bench back up.
  std::vector<const Player*> bench;
  bench.reserve(reserveCount);
  for (const Player* reserve : lineup.getReserves())
  {
    if (reserve && eligible(*reserve)) bench.push_back(reserve);
  }
  if (bench.size() != lineup.getReserves().size())
  {
    for (auto candidate = pool.begin();
         candidate != pool.end() && bench.size() < reserveCount; ++candidate)
      bench.push_back(*candidate);
    lineup.setReserves(bench);
  }
  return before - ineligible(lineup, eligible).size();
}

std::vector<std::pair<PlayerID, PlayerID>> MatchdaySquad::replacements(
    const Lineup& before, const Lineup& after)
{
  std::vector<std::pair<PlayerID, PlayerID>> result;
  const auto idOf = [](const Player* player)
  { return player ? player->getId() : PlayerID{}; };
  if (idOf(before.getGoalkeeper()) != idOf(after.getGoalkeeper()))
    result.emplace_back(idOf(before.getGoalkeeper()),
                        idOf(after.getGoalkeeper()));
  const auto& oldSlots = before.getOutfieldPlayers();
  const auto& newSlots = after.getOutfieldPlayers();
  for (std::size_t slot = 0; slot < std::min(oldSlots.size(), newSlots.size());
       ++slot)
  {
    if (idOf(oldSlots[slot].player) != idOf(newSlots[slot].player))
      result.emplace_back(idOf(oldSlots[slot].player),
                          idOf(newSlots[slot].player));
  }
  for (const Player* reserve : before.getReserves())
  {
    if (reserve && !isSelected(after, reserve))
      result.emplace_back(reserve->getId(), PlayerID{});
  }
  return result;
}

void MatchdaySquad::carryCondition(MatchEngine& engine, const Lineup& lineup)
{
  const auto carry = [&engine](const Player* player)
  {
    if (player)
      engine.setPlayerCondition(player->getId(),
                                player->getDynamics().condition / 100.0f);
  };
  carry(lineup.getGoalkeeper());
  for (const Lineup::PositionedPlayer& slot : lineup.getOutfieldPlayers())
    carry(slot.player);
  // Substitutes come on with their own condition.
  for (const Player* reserve : lineup.getReserves()) carry(reserve);
}

std::vector<PlayerMatchConsequence> MatchdaySquad::consequences(
    const MatchEngine& engine)
{
  std::vector<PlayerMatchConsequence> result;
  result.reserve(engine.getPlayerStats().size());
  for (const PlayerMatchStats& stats : engine.getPlayerStats())
  {
    const long minutes = std::lround(stats.minutesPlayed);
    if (minutes <= 0) continue;
    result.push_back({stats.playerId,
                      static_cast<std::uint8_t>(std::min(minutes, 255L)),
                      std::clamp(stats.condition, 0.0f, 1.0f) * 100.0f,
                      stats.injured});
  }
  return result;
}
