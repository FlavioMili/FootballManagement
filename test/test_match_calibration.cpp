// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "global/stats_config.h"
#include "model/match_engine.h"
#include "model/player.h"
#include "model/team.h"

namespace
{
StatsConfig createStatsConfig()
{
  StatsConfig config;
  config.possible_stats = {"Pace",      "Shooting",  "Passing",
                           "Dribbling", "Defending", "Physicality",
                           "Stamina",   "Vision",    "Goalkeeping"};
  config.role_focus["Goalkeeper"] = {{"Goalkeeping", "Vision", "Physicality"},
                                     {0.7, 0.2, 0.1}};
  config.role_focus["Defender"] = {
      {"Defending", "Physicality", "Pace", "Vision"}, {0.4, 0.3, 0.15, 0.15}};
  config.role_focus["Midfielder"] = {
      {"Passing", "Vision", "Stamina", "Dribbling"}, {0.3, 0.3, 0.2, 0.2}};
  config.role_focus["Striker"] = {
      {"Shooting", "Pace", "Dribbling", "Physicality"}, {0.4, 0.2, 0.2, 0.2}};
  return config;
}

/** Role-shaped attribute offsets so calibration squads resemble real ones. */
std::map<std::string, float> roleStats(PlayerRole role, float base,
                                       std::uint32_t salt)
{
  // Small deterministic per-player spread keeps squads from being clones.
  const float jitter = static_cast<float>((salt * 2654435761U) % 11U) - 5.0f;
  const auto value = [&](float offset)
  { return std::clamp(base + offset + jitter, 1.0f, 99.0f); };
  std::map<std::string, float> stats = {
      {"Pace", value(0.0f)},      {"Shooting", value(-8.0f)},
      {"Passing", value(0.0f)},   {"Dribbling", value(-4.0f)},
      {"Defending", value(0.0f)}, {"Physicality", value(0.0f)},
      {"Stamina", value(0.0f)},   {"Vision", value(0.0f)},
      {"Goalkeeping", 15.0f}};
  switch (role)
  {
    case PlayerRole::GK:
      stats["Goalkeeping"] = value(4.0f);
      stats["Shooting"] = value(-40.0f);
      stats["Dribbling"] = value(-30.0f);
      stats["Pace"] = value(-20.0f);
      break;
    case PlayerRole::CB:
      stats["Defending"] = value(8.0f);
      stats["Physicality"] = value(8.0f);
      stats["Shooting"] = value(-25.0f);
      stats["Dribbling"] = value(-15.0f);
      break;
    case PlayerRole::LB:
    case PlayerRole::RB:
      stats["Defending"] = value(4.0f);
      stats["Pace"] = value(4.0f);
      stats["Shooting"] = value(-20.0f);
      break;
    case PlayerRole::LM:
    case PlayerRole::RM:
    case PlayerRole::LW:
    case PlayerRole::RW:
      stats["Pace"] = value(6.0f);
      stats["Dribbling"] = value(4.0f);
      stats["Defending"] = value(-15.0f);
      stats["Shooting"] = value(-4.0f);
      break;
    case PlayerRole::CM:
    case PlayerRole::CDM:
    case PlayerRole::CAM:
      stats["Passing"] = value(6.0f);
      stats["Vision"] = value(6.0f);
      stats["Stamina"] = value(6.0f);
      stats["Defending"] = value(-6.0f);
      break;
    case PlayerRole::ST:
      stats["Shooting"] = value(8.0f);
      stats["Defending"] = value(-30.0f);
      stats["Dribbling"] = value(2.0f);
      break;
    case PlayerRole::UNKNOWN:
      break;
  }
  return stats;
}

/** A 4-4-2 squad with seven substitutes, matching the auto-lineup shape. */
Team createCalibrationTeam(TeamID id, float rating,
                           std::vector<std::unique_ptr<Player>>& pool)
{
  Team team(id, 1, "Calibration " + std::to_string(id), 50'000'000, {},
            Strategy{}, Lineup{});
  static constexpr PlayerRole ROLES[11] = {
      PlayerRole::GK, PlayerRole::LB, PlayerRole::CB, PlayerRole::CB,
      PlayerRole::RB, PlayerRole::LM, PlayerRole::CM, PlayerRole::CM,
      PlayerRole::RM, PlayerRole::ST, PlayerRole::ST};
  static constexpr Vector2F POSITIONS[11] = {
      {0.04f, 0.50f}, {0.20f, 0.12f}, {0.20f, 0.38f}, {0.20f, 0.62f},
      {0.20f, 0.88f}, {0.43f, 0.12f}, {0.43f, 0.38f}, {0.43f, 0.62f},
      {0.43f, 0.88f}, {0.78f, 0.38f}, {0.78f, 0.62f}};
  static constexpr PlayerRole BENCH[7] = {
      PlayerRole::GK, PlayerRole::CB, PlayerRole::RB, PlayerRole::CM,
      PlayerRole::LM, PlayerRole::ST, PlayerRole::ST};

  const auto makePlayer = [&](std::uint32_t index, PlayerRole role)
  {
    const auto playerId = static_cast<PlayerID>(id) * 100U + index;
    const auto height = static_cast<std::uint8_t>(
        role == PlayerRole::GK || role == PlayerRole::CB ? 188 : 178);
    auto player = std::make_unique<Player>(
        playerId, id, "Calib", std::to_string(playerId), role, Language::EN,
        100'000, 0, 26, 3, height, Foot::Right,
        roleStats(role, rating, playerId));
    Player* raw = player.get();
    pool.push_back(std::move(player));
    return raw;
  };

  for (std::uint32_t index = 0; index < 11; ++index)
  {
    const Player* player = makePlayer(index, ROLES[index]);
    if (ROLES[index] == PlayerRole::GK)
      team.getLineup().setGoalkeeper(player);
    else
      team.getLineup().addOutfieldPlayer(player, POSITIONS[index]);
  }
  std::vector<const Player*> reserves;
  for (std::uint32_t index = 0; index < 7; ++index)
    reserves.push_back(makePlayer(11 + index, BENCH[index]));
  team.getLineup().setReserves(reserves);
  return team;
}

struct CalibrationTotals
{
  int matches = 0;
  int homeWins = 0;
  int draws = 0;
  int awayWins = 0;
  int goallessDraws = 0;
  int homeGoals = 0;
  int awayGoals = 0;
  int shots = 0;
  int onTarget = 0;
  int shotsInsideBox = 0;
  int headedShots = 0;
  int setPieceShots = 0;
  int fouls = 0;
  int yellows = 0;
  int reds = 0;
  int corners = 0;
  int offsides = 0;
  int penalties = 0;
  int injuries = 0;
  int substitutions = 0;
  int advantages = 0;
  int passesAttempted = 0;
  int passesCompleted = 0;
  int tackleAttempts = 0;
  int maxSubstitutions = 0;
  int maxWindows = 0;
  int addedFirstHalf = 0;
  int addedSecondHalf = 0;
  std::map<MatchEventType, int> eventCounts;
  int crosses = 0;
  int interceptions = 0;
  int clearances = 0;
  double ballInPlay = 0.0;
  int headedGoals = 0;
  int setPieceGoals = 0;
  int penaltyGoals = 0;
  int ownGoals = 0;
  double substitutionMinuteSum = 0.0;
  double xg = 0.0;
  double possessionSpread = 0.0;
  // Outfield starters who played the whole match.
  int fullMatchPlayers = 0;
  double fullMatchDistance = 0.0;
  double fullMatchSecondHalfDistance = 0.0;
  double fullMatchCondition = 0.0;
  // Minutes of each half, to compare per-minute work rates.
  double firstHalfMinutes = 0.0;
  double secondHalfMinutes = 0.0;
  double firstHalfDistancePerMinute = 0.0;
  double secondHalfDistancePerMinute = 0.0;
  float minimumCondition = 1.0f;
  float minimumRating = 10.0f;
  float maximumRating = 0.0f;
  double ratingSum = 0.0;
  int ratedPlayers = 0;
  int statGoalMismatches = 0;
  double seconds = 0.0;

  double perMatch(int value) const
  {
    return matches > 0 ? static_cast<double>(value) / matches : 0.0;
  }
  double perTeam(int value) const { return perMatch(value) / 2.0; }
};

void accumulate(CalibrationTotals& totals, const MatchEngine& engine)
{
  const MatchStats& stats = engine.getStats();
  ++totals.matches;
  const int home = engine.getHomeScore();
  const int away = engine.getAwayScore();
  totals.homeGoals += home;
  totals.awayGoals += away;
  if (home > away)
    ++totals.homeWins;
  else if (home < away)
    ++totals.awayWins;
  else
    ++totals.draws;
  if (home == 0 && away == 0) ++totals.goallessDraws;
  totals.shots += stats.homeShots + stats.awayShots;
  totals.onTarget += stats.homeOnTarget + stats.awayOnTarget;
  totals.shotsInsideBox += stats.homeShotsInsideBox + stats.awayShotsInsideBox;
  totals.headedShots += stats.homeHeadedShots + stats.awayHeadedShots;
  totals.setPieceShots += stats.homeSetPieceShots + stats.awaySetPieceShots;
  totals.fouls += stats.homeFouls + stats.awayFouls;
  totals.yellows += stats.homeYellowCards + stats.awayYellowCards;
  totals.reds += stats.homeRedCards + stats.awayRedCards;
  totals.corners += stats.homeCorners + stats.awayCorners;
  totals.offsides += stats.homeOffsides + stats.awayOffsides;
  totals.penalties += stats.homePenalties + stats.awayPenalties;
  totals.injuries += stats.homeInjuries + stats.awayInjuries;
  totals.substitutions += stats.homeSubstitutions + stats.awaySubstitutions;
  totals.advantages += stats.homeAdvantagesPlayed + stats.awayAdvantagesPlayed;
  totals.passesAttempted +=
      stats.homePassesAttempted + stats.awayPassesAttempted;
  totals.passesCompleted +=
      stats.homePassesCompleted + stats.awayPassesCompleted;
  totals.tackleAttempts += stats.homeTackleAttempts + stats.awayTackleAttempts;
  totals.xg += stats.homeShotXG + stats.awayShotXG;
  totals.possessionSpread += std::abs(stats.homePossession - 50.0f);
  totals.maxSubstitutions =
      std::max({totals.maxSubstitutions, engine.getSubstitutionsUsed(true),
                engine.getSubstitutionsUsed(false)});
  totals.maxWindows =
      std::max({totals.maxWindows, engine.getSubstitutionWindowsUsed(true),
                engine.getSubstitutionWindowsUsed(false)});
  totals.addedFirstHalf += engine.getAddedMinutes(1);
  totals.addedSecondHalf += engine.getAddedMinutes(2);
  for (const MatchSubstitution& change : engine.getSubstitutions())
    totals.substitutionMinuteSum += change.timeMinute;
  for (const MatchEvent& event : engine.getEvents())
  {
    if (event.type == MatchEventType::OWN_GOAL) ++totals.ownGoals;
    ++totals.eventCounts[event.type];
  }
  totals.crosses += stats.homeCrosses + stats.awayCrosses;
  totals.ballInPlay += stats.ballInPlayMinutes;
  totals.headedGoals += stats.homeHeadedGoals + stats.awayHeadedGoals;
  totals.setPieceGoals += stats.homeSetPieceGoals + stats.awaySetPieceGoals;
  totals.penaltyGoals += stats.homePenaltyGoals + stats.awayPenaltyGoals;

  int homeGoalsByPlayers = 0;
  int awayGoalsByPlayers = 0;
  for (const PlayerMatchStats& entry : engine.getPlayerStats())
  {
    (entry.isHomeTeam ? homeGoalsByPlayers : awayGoalsByPlayers) += entry.goals;
    (entry.isHomeTeam ? awayGoalsByPlayers : homeGoalsByPlayers) +=
        entry.ownGoals;
    totals.interceptions += entry.interceptions;
    totals.clearances += entry.clearances;
    totals.minimumRating = std::min(totals.minimumRating, entry.rating);
    totals.maximumRating = std::max(totals.maximumRating, entry.rating);
    totals.ratingSum += entry.rating;
    ++totals.ratedPlayers;
    const bool fullMatch = entry.started && !entry.substitutedOff &&
                           !entry.sentOff && !entry.injured;
    if (!fullMatch || entry.role == PlayerRole::GK) continue;
    ++totals.fullMatchPlayers;
    totals.fullMatchDistance += entry.distanceMetres;
    totals.fullMatchSecondHalfDistance += entry.secondHalfDistanceMetres;
    totals.fullMatchCondition += entry.condition;
    totals.minimumCondition =
        std::min(totals.minimumCondition, entry.condition);
  }
  if (homeGoalsByPlayers != home || awayGoalsByPlayers != away)
    ++totals.statGoalMismatches;
  const double secondHalf = engine.getMatchTimeMinutes() - 45.0;
  const double firstHalf = engine.getElapsedMatchMinutes() - secondHalf;
  totals.firstHalfMinutes += firstHalf;
  totals.secondHalfMinutes += secondHalf;
}

CalibrationTotals runCalibration(const Team& home, const Team& away,
                                 const StatsConfig& config, int matches,
                                 std::uint32_t firstSeed)
{
  CalibrationTotals totals;
  const auto started = std::chrono::steady_clock::now();
  for (int index = 0; index < matches; ++index)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config,
                       firstSeed + static_cast<std::uint32_t>(index) * 7919U);
    for (int frame = 0;
         frame < 20'000 && engine.getState() != MatchState::FULL_TIME; ++frame)
    {
      engine.update(0.25f);
    }
    accumulate(totals, engine);
  }
  totals.seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();
  return totals;
}

void report(const char* label, const CalibrationTotals& t)
{
  const int goals = t.homeGoals + t.awayGoals;
  const double firstHalfDistance =
      t.fullMatchDistance - t.fullMatchSecondHalfDistance;
  std::printf(
      "[calibration] %s matches=%d goals/match=%.2f (home %.2f away %.2f) "
      "home/draw/away=%.1f/%.1f/%.1f%% 0-0=%.1f%%\n"
      "[calibration] %s shots/team=%.2f onTarget/team=%.2f insideBox=%.0f%% "
      "headed=%.0f%% setPiece=%.0f%% conversion=%.1f%% "
      "onTargetConversion=%.1f%% "
      "xG/shot=%.3f goals/xG=%.2f headedGoals=%.0f%% setPieceGoals=%.0f%% "
      "penaltyConversion=%.0f%% ownGoals=%d\n"
      "[calibration] %s fouls/team=%.2f yellows/match=%.2f reds/match=%.3f "
      "penalties/match=%.3f advantage=%.0f%% corners/team=%.2f "
      "offsides/team=%.2f injuries/team=%.3f tackles/team=%.1f\n"
      "[calibration] %s passes/team=%.0f passCompletion=%.1f%% "
      "possessionSpread=%.1f subs/team=%.2f subMinute=%.1f maxSubs=%d "
      "maxWindows=%d added=%.2f+%.2f\n"
      "[calibration] %s fullMatchDistance=%.0fm (1st %.0f / 2nd %.0f) "
      "perMinuteDrop=%.1f%% condition=%.2f (min %.2f) rating mean=%.2f [%.1f, "
      "%.1f] "
      "goalStatMismatches=%d ms/match=%.2f\n",
      label, t.matches, t.perMatch(goals), t.perMatch(t.homeGoals),
      t.perMatch(t.awayGoals), 100.0 * t.perMatch(t.homeWins),
      100.0 * t.perMatch(t.draws), 100.0 * t.perMatch(t.awayWins),
      100.0 * t.perMatch(t.goallessDraws), label, t.perTeam(t.shots),
      t.perTeam(t.onTarget),
      t.shots > 0 ? 100.0 * t.shotsInsideBox / t.shots : 0.0,
      t.shots > 0 ? 100.0 * t.headedShots / t.shots : 0.0,
      t.shots > 0 ? 100.0 * t.setPieceShots / t.shots : 0.0,
      t.shots > 0 ? 100.0 * goals / t.shots : 0.0,
      t.onTarget > 0 ? 100.0 * goals / t.onTarget : 0.0,
      t.shots > 0 ? t.xg / t.shots : 0.0, t.xg > 0.0 ? goals / t.xg : 0.0,
      goals > 0 ? 100.0 * t.headedGoals / goals : 0.0,
      goals > 0 ? 100.0 * t.setPieceGoals / goals : 0.0,
      t.penalties > 0 ? 100.0 * t.penaltyGoals / t.penalties : 0.0, t.ownGoals,
      label, t.perTeam(t.fouls), t.perMatch(t.yellows), t.perMatch(t.reds),
      t.perMatch(t.penalties),
      t.fouls > 0 ? 100.0 * t.advantages / t.fouls : 0.0, t.perTeam(t.corners),
      t.perTeam(t.offsides), t.perTeam(t.injuries), t.perTeam(t.tackleAttempts),
      label, t.perTeam(t.passesAttempted),
      t.passesAttempted > 0 ? 100.0 * t.passesCompleted / t.passesAttempted
                            : 0.0,
      t.matches > 0 ? t.possessionSpread / t.matches : 0.0,
      t.perTeam(t.substitutions),
      t.substitutions > 0 ? t.substitutionMinuteSum / t.substitutions : 0.0,
      t.maxSubstitutions, t.maxWindows, t.perMatch(t.addedFirstHalf),
      t.perMatch(t.addedSecondHalf), label,
      t.fullMatchPlayers > 0 ? t.fullMatchDistance / t.fullMatchPlayers : 0.0,
      t.fullMatchPlayers > 0 ? firstHalfDistance / t.fullMatchPlayers : 0.0,
      t.fullMatchPlayers > 0
          ? t.fullMatchSecondHalfDistance / t.fullMatchPlayers
          : 0.0,
      t.secondHalfMinutes > 0.0 && firstHalfDistance > 0.0
          ? 100.0 *
                (1.0 - (t.fullMatchSecondHalfDistance / t.secondHalfMinutes) /
                           (firstHalfDistance / t.firstHalfMinutes))
          : 0.0,
      t.fullMatchPlayers > 0 ? t.fullMatchCondition / t.fullMatchPlayers : 0.0,
      t.minimumCondition,
      t.ratedPlayers > 0 ? t.ratingSum / t.ratedPlayers : 0.0, t.minimumRating,
      t.maximumRating, t.statGoalMismatches,
      t.matches > 0 ? 1000.0 * t.seconds / t.matches : 0.0);
  std::printf(
      "[calibration] %s ballInPlay=%.1f min crosses/team=%.2f "
      "interceptions/team=%.2f clearances/team=%.2f events/match:",
      label, t.matches > 0 ? t.ballInPlay / t.matches : 0.0,
      t.perTeam(t.crosses), t.perTeam(t.interceptions),
      t.perTeam(t.clearances));
  for (const auto& [type, count] : t.eventCounts)
  {
    std::printf(" %s=%.2f", std::string(matchEventTypeName(type)).c_str(),
                t.perMatch(count));
  }
  std::printf("\n");
}
}  // namespace

// Headless calibration against broad real-football bands (top-five-league
// averages: ~2.8 goals, ~25% draws, ~12.8 shots and ~11.5 fouls per team,
// ~4 yellows per match). Fixed seeds keep the test deterministic.
TEST(MatchEngineCalibration, HeadlessSeasonMatchesRealisticBands)
{
  std::vector<std::unique_ptr<Player>> pool;
  const StatsConfig config = createStatsConfig();
  const Team equalHome = createCalibrationTeam(1, 65.0f, pool);
  const Team equalAway = createCalibrationTeam(2, 65.0f, pool);
  const Team strong = createCalibrationTeam(3, 78.0f, pool);
  const Team weak = createCalibrationTeam(4, 55.0f, pool);

  const CalibrationTotals equal =
      runCalibration(equalHome, equalAway, config, 200, 1);
  const CalibrationTotals strongHome =
      runCalibration(strong, weak, config, 40, 50'001);
  const CalibrationTotals weakHome =
      runCalibration(weak, strong, config, 40, 90'001);
  report("equal", equal);
  report("strong-home", strongHome);
  report("weak-home", weakHome);

  const int goals = equal.homeGoals + equal.awayGoals;
  EXPECT_GE(equal.perMatch(goals), 2.2);
  EXPECT_LE(equal.perMatch(goals), 3.3);
  EXPECT_GE(equal.perMatch(equal.draws), 0.20);
  EXPECT_LE(equal.perMatch(equal.draws), 0.32);
  EXPECT_GT(equal.homeWins, equal.awayWins) << "home advantage";
  // The engine condenses ~57 minutes of live play into about a minute of
  // simulated action (the clock runs one match minute per simulated second),
  // so it produces roughly a quarter of the real number of possessions.
  // Shot volume is therefore below the real ~12.8 per team and conversion
  // above the real ~10.5%, which keeps goals, results and scorelines
  // realistic. These bands pin that trade-off.
  EXPECT_GE(equal.perTeam(equal.shots), 4.0);
  EXPECT_LE(equal.perTeam(equal.shots), 17.0);
  const double onTargetShare =
      static_cast<double>(equal.onTarget) / equal.shots;
  EXPECT_GE(onTargetShare, 0.30);
  EXPECT_LE(onTargetShare, 0.55);
  const double conversion = static_cast<double>(goals) / equal.shots;
  EXPECT_GE(conversion, 0.08);
  EXPECT_LE(conversion, 0.35);
  EXPECT_GE(equal.xg / equal.shots, 0.08);
  EXPECT_LE(equal.xg / equal.shots, 0.25);
  const double headedShare =
      static_cast<double>(equal.headedShots) / equal.shots;
  EXPECT_GE(headedShare, 0.05);
  EXPECT_LE(headedShare, 0.30);
  EXPECT_GE(static_cast<double>(equal.penaltyGoals) / equal.penalties, 0.6);
  EXPECT_LE(static_cast<double>(equal.penaltyGoals) / equal.penalties, 0.95);
  EXPECT_GE(equal.perTeam(equal.fouls), 8.0);
  EXPECT_LE(equal.perTeam(equal.fouls), 15.0);
  EXPECT_GE(equal.perMatch(equal.yellows), 2.0);
  EXPECT_LE(equal.perMatch(equal.yellows), 5.0);
  EXPECT_GE(equal.perMatch(equal.reds), 0.03);
  EXPECT_LE(equal.perMatch(equal.reds), 0.40);
  EXPECT_GE(equal.perMatch(equal.penalties), 0.08);
  EXPECT_LE(equal.perMatch(equal.penalties), 0.50);
  EXPECT_GE(equal.perTeam(equal.corners), 3.0);
  EXPECT_LE(equal.perTeam(equal.corners), 7.5);
  EXPECT_GE(equal.perTeam(equal.offsides), 0.6);
  EXPECT_LE(equal.perTeam(equal.offsides), 3.0);
  EXPECT_GE(equal.perTeam(equal.injuries), 0.05);
  EXPECT_LE(equal.perTeam(equal.injuries), 0.40);
  EXPECT_GE(equal.perTeam(equal.substitutions), 3.0);
  EXPECT_LE(equal.maxSubstitutions,
            MatchTuning::Rules::MAX_SUBSTITUTIONS_PER_TEAM);
  EXPECT_LE(equal.maxWindows, MatchTuning::Substitution::MAX_WINDOWS);
  EXPECT_GE(equal.perMatch(equal.addedFirstHalf), 1.0);
  EXPECT_LE(equal.perMatch(equal.addedFirstHalf), 5.0);
  EXPECT_GE(equal.perMatch(equal.addedSecondHalf), 3.0);
  EXPECT_LE(equal.perMatch(equal.addedSecondHalf), 8.0);
  ASSERT_GT(equal.fullMatchPlayers, 0);
  const double distance = equal.fullMatchDistance / equal.fullMatchPlayers;
  EXPECT_GE(distance, 8'500.0);
  EXPECT_LE(distance, 12'500.0);
  // Per minute played, fatigue must show in the second half.
  const double firstHalfRate =
      (equal.fullMatchDistance - equal.fullMatchSecondHalfDistance) /
      equal.firstHalfMinutes;
  const double secondHalfRate =
      equal.fullMatchSecondHalfDistance / equal.secondHalfMinutes;
  EXPECT_LT(secondHalfRate, firstHalfRate * 0.98)
      << "second-half work rate must drop";
  const double condition = equal.fullMatchCondition / equal.fullMatchPlayers;
  EXPECT_GE(condition, 0.45);
  EXPECT_LE(condition, 0.85);
  EXPECT_GE(equal.minimumRating, MatchTuning::Rating::MINIMUM);
  EXPECT_LE(equal.maximumRating, MatchTuning::Rating::MAXIMUM);
  EXPECT_EQ(equal.statGoalMismatches, 0)
      << "per-player goals must add up to the score";

  // The stronger side wins most games wherever it plays.
  EXPECT_GT(strongHome.homeWins, strongHome.awayWins * 2);
  EXPECT_GT(weakHome.awayWins, weakHome.homeWins * 2);
  EXPECT_LT(equal.seconds / equal.matches, 0.05) << "a match must stay fast";
}
