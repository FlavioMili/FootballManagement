// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/competition.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include <string_view>
#include <tuple>
#include <unordered_set>

#include "database/gamedata.h"
#include "global/language_manager.h"
#include "global/stats_config.h"
#include "model/calendar.h"
#include "model/league.h"
#include "model/lineup.h"
#include "model/player.h"
#include "model/team.h"

namespace
{
constexpr int MAX_PYRAMID_DEPTH = 16;
constexpr size_t PENALTY_ROUNDS = 5;
constexpr float BASE_PENALTY_CONVERSION = 0.75f;
constexpr float PENALTY_SKILL_WEIGHT = 0.0035f;
constexpr float PENALTY_AVERAGE_SKILL = 50.0f;
constexpr float MIN_PENALTY_CONVERSION = 0.55f;
constexpr float MAX_PENALTY_CONVERSION = 0.92f;
// Tired legs: about 0.66 goals per 30 minutes of extra time for both teams.
constexpr double EXTRA_TIME_GOALS_PER_TEAM = 0.33;
constexpr double EXTRA_TIME_HOME_EDGE = 1.05;
constexpr double STRENGTH_EXPONENT = 2.0;
constexpr double MIN_STRENGTH_FACTOR = 0.5;
constexpr double MAX_STRENGTH_FACTOR = 2.0;
constexpr double DEFAULT_STRENGTH = 50.0;
constexpr uint32_t CUP_DRAW_SALT = 16;

std::vector<const Player*> starters(const Lineup& lineup)
{
  std::vector<const Player*> players;
  players.reserve(lineup.getOutfieldPlayers().size() + 1);
  for (const auto& positioned : lineup.getOutfieldPlayers())
    if (positioned.player) players.push_back(positioned.player);
  if (lineup.getGoalkeeper()) players.push_back(lineup.getGoalkeeper());
  return players;
}

float statOf(const Player& player, const char* name)
{
  const auto& stats = player.getStats();
  const auto it = stats.find(name);
  return it == stats.end() ? PENALTY_AVERAGE_SKILL : it->second;
}

double teamStrength(const Team& team, const StatsConfig& stats_config)
{
  const auto players = starters(team.getLineup());
  if (players.empty()) return DEFAULT_STRENGTH;
  double total = 0.0;
  for (const Player* player : players) total += player->getOverall(stats_config);
  return std::max(1.0, total / static_cast<double>(players.size()));
}

std::vector<float> penaltyTakers(const Team& shooters, const Team& keepers)
{
  std::vector<const Player*> order = starters(shooters.getLineup());
  std::ranges::stable_sort(order,
                           [](const Player* left, const Player* right)
                           {
                             return statOf(*left, "Shooting") >
                                    statOf(*right, "Shooting");
                           });
  const Player* goalkeeper = keepers.getLineup().getGoalkeeper();
  const float keeping =
      goalkeeper ? statOf(*goalkeeper, "Goalkeeping") : PENALTY_AVERAGE_SKILL;
  std::vector<float> chances;
  chances.reserve(order.size());
  for (const Player* player : order)
    chances.push_back(Competitions::penaltyConversionProbability(
        statOf(*player, "Shooting"), keeping));
  return chances;
}

/**
 * Day of a tie of a midweek round (@p date is its Wednesday): Tuesday,
 * Wednesday or Thursday, whichever rests both clubs best and is quietest, so
 * a round is spread over the three days. Weekend dates (the final) stay.
 */
GameDateValue tieDay(const Calendar& calendar, TeamID home_id, TeamID away_id,
                     const GameDateValue& date, const GameDateValue& after)
{
  if (SeasonCalendar::dayOfWeek(date) != SeasonCalendar::WEDNESDAY) return date;
  std::optional<GameDateValue> best;
  std::tuple<int, size_t, int> best_key{};
  for (const int offset : {0, -1, 1})
  {
    const GameDateValue day = SeasonCalendar::addDays(date, offset);
    if (!(after < day) ||
        (offset != 0 && (SeasonCalendar::isBlackout(day) ||
                         SeasonCalendar::isContinentalWeek(day))))
      continue;
    // Clashes with the clubs' other matches (league fixtures can still move
    // within their round, see Calendar::protectRest).
    int clashes = 0;
    for (int near = -SeasonCalendar::MIN_REST_BEFORE_TIE;
         near <= SeasonCalendar::MIN_REST_BEFORE_TIE; ++near)
    {
      for (const Match& match :
           calendar.getMatchesForDate(SeasonCalendar::addDays(day, near)))
      {
        const bool involved =
            match.getHomeTeamId() == home_id || match.getAwayTeamId() == home_id ||
            match.getHomeTeamId() == away_id || match.getAwayTeamId() == away_id;
        if (!involved) continue;
        if (near == 0)
          clashes += 10;
        else if (near < 0 &&
                 -near < SeasonCalendar::restDays(match.getMatchType(),
                                                  MatchType::CUP))
          ++clashes;
        else if (near > 0 &&
                 near < SeasonCalendar::restDays(MatchType::CUP,
                                                 match.getMatchType()))
          ++clashes;
      }
    }
    const std::tuple key{clashes, calendar.getMatchesForDate(day).size(),
                         std::abs(offset)};
    if (!best || key < best_key)
    {
      best = day;
      best_key = key;
    }
  }
  return best.value_or(date);
}

void addTies(Calendar& calendar, const GameData& gamedata,
             std::vector<TeamID> participants, std::mt19937& rng,
             const GameDateValue& date, LeagueID cup_id, uint8_t stage,
             const GameDateValue& after)
{
  std::ranges::shuffle(participants, rng);
  for (size_t i = 0; i + 1 < participants.size(); i += 2)
  {
    TeamID home_id = participants[i];
    TeamID away_id = participants[i + 1];
    // The lower-division club hosts when the tiers differ.
    const auto home_team = gamedata.getTeam(home_id);
    const auto away_team = gamedata.getTeam(away_id);
    if (home_team && away_team &&
        Competitions::leagueTier(gamedata, home_team->get().getLeagueId()) <
            Competitions::leagueTier(gamedata, away_team->get().getLeagueId()))
      std::swap(home_id, away_id);
    calendar.addMatch(Match(home_id, away_id,
                            tieDay(calendar, home_id, away_id, date, after),
                            MatchType::CUP, cup_id, stage));
  }
}

bool hasFixtureOn(const Calendar& calendar, const GameDateValue& date,
                  const std::vector<TeamID>& teams)
{
  return std::ranges::any_of(
      calendar.getMatchesForDate(date),
      [&teams](const Match& match)
      {
        return std::ranges::contains(teams, match.getHomeTeamId()) ||
               std::ranges::contains(teams, match.getAwayTeamId());
      });
}
}  // namespace

uint32_t Competitions::mixSeed(uint32_t a, uint32_t b, uint32_t c)
{
  uint32_t hash = 0x9E37'79B9U;
  for (const uint32_t value : {a, b, c})
    hash ^= value + 0x9E37'79B9U + (hash << 6U) + (hash >> 2U);
  hash ^= hash >> 16U;
  hash *= 0x85EB'CA6BU;
  hash ^= hash >> 13U;
  hash *= 0xC2B2'AE35U;
  hash ^= hash >> 16U;
  return hash;
}

// ---------------- Pyramid ----------------

LeagueID Competitions::rootLeague(const GameData& gamedata, LeagueID league_id)
{
  LeagueID current = league_id;
  for (int depth = 0; depth < MAX_PYRAMID_DEPTH; ++depth)
  {
    const auto league = gamedata.getLeague(current);
    if (!league) break;
    const auto parent = league->get().getParentLeagueID();
    if (!parent || !gamedata.getLeague(*parent)) break;
    current = *parent;
  }
  return current;
}

uint8_t Competitions::leagueTier(const GameData& gamedata, LeagueID league_id)
{
  uint8_t tier = 1;
  LeagueID current = league_id;
  for (int depth = 0; depth < MAX_PYRAMID_DEPTH; ++depth)
  {
    const auto league = gamedata.getLeague(current);
    if (!league) break;
    const auto parent = league->get().getParentLeagueID();
    if (!parent || !gamedata.getLeague(*parent)) break;
    current = *parent;
    ++tier;
  }
  return tier;
}

std::vector<LeagueID> Competitions::countryRoots(const GameData& gamedata)
{
  std::vector<LeagueID> roots;
  for (const auto& [id, league] : gamedata.getLeagues())
    if (rootLeague(gamedata, id) == id) roots.push_back(id);
  std::ranges::sort(roots);
  return roots;
}

std::vector<LeagueID> Competitions::countryLeagues(const GameData& gamedata,
                                                   LeagueID root)
{
  std::vector<LeagueID> leagues;
  for (const auto& [id, league] : gamedata.getLeagues())
    if (rootLeague(gamedata, id) == root) leagues.push_back(id);
  std::ranges::sort(leagues,
                    [&gamedata](LeagueID left, LeagueID right)
                    {
                      const uint8_t left_tier = leagueTier(gamedata, left);
                      const uint8_t right_tier = leagueTier(gamedata, right);
                      if (left_tier != right_tier) return left_tier < right_tier;
                      return left < right;
                    });
  return leagues;
}

// ---------------- Domestic cup ----------------

std::vector<TeamID> Competitions::cupEntrants(const GameData& gamedata,
                                              LeagueID root)
{
  std::vector<TeamID> entrants;
  for (const LeagueID league_id : countryLeagues(gamedata, root))
  {
    std::vector<TeamID> teams = gamedata.getLeagues().at(league_id).getTeamIDs();
    std::ranges::sort(teams);
    entrants.insert(entrants.end(), teams.begin(), teams.end());
  }
  return entrants;
}

namespace
{
// "Italian Lower League" -> "<prefix>ITALIAN_LOWER_LEAGUE".
std::string nameKey(std::string_view prefix, const std::string& name)
{
  std::string key(prefix);
  for (const char c : name)
  {
    const auto byte = static_cast<unsigned char>(c);
    key += std::isalnum(byte) != 0 ? static_cast<char>(std::toupper(byte))
                                   : '_';
  }
  return key;
}

bool isTranslated(const std::string& key)
{
  return LOC(key.c_str()) != key.c_str();
}
}  // namespace

std::string Competitions::leagueName(const League& league)
{
  const std::string key = nameKey("LEAGUE_NAME_", league.getName());
  return isTranslated(key) ? std::string(LOC(key.c_str())) : league.getName();
}

std::string Competitions::leagueNameArg(const League& league)
{
  const std::string key = nameKey("LEAGUE_NAME_", league.getName());
  return isTranslated(key) ? "@" + key : league.getName();
}

std::string Competitions::cupName(const GameData& gamedata, LeagueID root)
{
  const auto league = gamedata.getLeague(root);
  if (!league) return "Cup";
  if (const std::string key = nameKey("CUP_NAME_", league->get().getName());
      isTranslated(key))
    return LOC(key.c_str());
  std::string name = league->get().getName();
  static constexpr std::string_view LEAGUE_WORD = "League";
  if (const auto position = name.rfind(LEAGUE_WORD);
      position != std::string::npos)
    return name.replace(position, LEAGUE_WORD.size(), "Cup");
  return name + " Cup";
}

uint8_t Competitions::cupRoundCount(size_t entrants)
{
  uint8_t rounds = 0;
  while ((size_t{1} << rounds) < entrants) ++rounds;
  return rounds;
}

const char* Competitions::cupRoundLabelKey(uint8_t stage, uint8_t total_rounds)
{
  const int rounds_left = static_cast<int>(total_rounds) - stage;
  if (rounds_left == 0) return "CUP_ROUND_FINAL";
  if (rounds_left == 1) return "CUP_ROUND_SEMI_FINAL";
  if (rounds_left == 2) return "CUP_ROUND_QUARTER_FINAL";
  return "CUP_ROUND_NUMBER";
}

Competitions::CupStatus Competitions::cupStatus(const Calendar& calendar,
                                                const GameData& gamedata,
                                                LeagueID root)
{
  CupStatus status;
  status.cup_id = root;
  status.name = cupName(gamedata, root);
  const std::vector<TeamID> entrants = cupEntrants(gamedata, root);
  status.total_rounds = cupRoundCount(entrants.size());

  std::map<uint8_t, std::vector<Match>> by_stage;
  std::unordered_set<TeamID> eliminated;
  for (const auto& [date, matches] : calendar.getFullCalendar())
  {
    for (const Match& match : matches)
    {
      if (match.getMatchType() != MatchType::CUP ||
          match.getCompetitionId() != root)
        continue;
      by_stage[match.getStage()].push_back(match);
      if (const auto winner = match.getWinnerId())
        eliminated.insert(*winner == match.getHomeTeamId()
                              ? match.getAwayTeamId()
                              : match.getHomeTeamId());
    }
  }

  for (auto& [stage, ties] : by_stage)
  {
    CupRound round;
    round.stage = stage;
    round.complete = std::ranges::all_of(
        ties, [](const Match& tie) { return tie.getWinnerId().has_value(); });
    round.ties = std::move(ties);
    status.rounds.push_back(std::move(round));
  }
  for (const TeamID team : entrants)
    if (!eliminated.contains(team)) status.remaining.push_back(team);

  if (!status.rounds.empty() && status.rounds.back().complete &&
      status.remaining.size() == 1)
  {
    status.winner = status.remaining.front();
    const Match& final_tie = status.rounds.back().ties.front();
    status.runner_up = final_tie.getHomeTeamId() == *status.winner
                           ? final_tie.getAwayTeamId()
                           : final_tie.getHomeTeamId();
  }
  return status;
}

void Competitions::scheduleCupFirstRounds(Calendar& calendar,
                                          const GameData& gamedata,
                                          uint16_t season_year)
{
  for (const LeagueID root : countryRoots(gamedata))
  {
    const std::vector<LeagueID> leagues = countryLeagues(gamedata, root);
    size_t entrant_count = 0;
    for (const LeagueID league_id : leagues)
      entrant_count += gamedata.getLeagues().at(league_id).getTeamIDs().size();
    const uint8_t rounds = cupRoundCount(entrant_count);
    if (rounds == 0) continue;

    // Preliminary round for the lowest-tier clubs; the rest get a bye.
    const size_t first_round_ties =
        entrant_count - (size_t{1} << (rounds - 1));
    std::mt19937 rng(mixSeed(season_year, root, CUP_DRAW_SALT));
    std::vector<TeamID> participants;
    for (auto league = leagues.rbegin();
         league != leagues.rend() && participants.size() < first_round_ties * 2;
         ++league)
    {
      std::vector<TeamID> teams = gamedata.getLeagues().at(*league).getTeamIDs();
      std::ranges::sort(teams);
      std::ranges::shuffle(teams, rng);
      const size_t wanted = first_round_ties * 2 - participants.size();
      teams.resize(std::min(wanted, teams.size()));
      participants.insert(participants.end(), teams.begin(), teams.end());
    }
    const auto dates = SeasonCalendar::cupRoundDates(season_year, rounds);
    addTies(calendar, gamedata, std::move(participants), rng, dates.front(),
            root, 1, SeasonCalendar::addDays(dates.front(), -2));
  }
}

size_t Competitions::drawPendingCupRounds(Calendar& calendar,
                                          const GameData& gamedata,
                                          uint16_t season_year,
                                          const GameDateValue& today)
{
  size_t added = 0;
  for (const LeagueID root : countryRoots(gamedata))
  {
    const CupStatus status = cupStatus(calendar, gamedata, root);
    if (status.rounds.empty() || status.winner ||
        !status.rounds.back().complete || status.remaining.size() < 2)
      continue;

    const auto next_stage = static_cast<uint8_t>(status.rounds.back().stage + 1);
    const auto dates =
        SeasonCalendar::cupRoundDates(season_year, status.total_rounds);
    GameDateValue date = next_stage <= dates.size() ? dates[next_stage - 1]
                                                    : SeasonCalendar::nextFreeMidweek(today);
    if (!(today < date)) date = SeasonCalendar::nextFreeMidweek(today);
    while (hasFixtureOn(calendar, date, status.remaining))
      date = SeasonCalendar::nextFreeMidweek(date);

    std::mt19937 rng(mixSeed(season_year, root, CUP_DRAW_SALT + next_stage));
    addTies(calendar, gamedata, status.remaining, rng, date, root, next_stage,
            today);
    added += status.remaining.size() / 2;
  }
  return added;
}

// ---------------- Knockout resolution ----------------

float Competitions::penaltyConversionProbability(float shooting,
                                                 float goalkeeping)
{
  const float chance = BASE_PENALTY_CONVERSION +
                       PENALTY_SKILL_WEIGHT * (shooting - PENALTY_AVERAGE_SKILL) -
                       PENALTY_SKILL_WEIGHT * (goalkeeping - PENALTY_AVERAGE_SKILL);
  return std::clamp(chance, MIN_PENALTY_CONVERSION, MAX_PENALTY_CONVERSION);
}

Competitions::ShootoutResult Competitions::simulateShootout(
    const std::vector<float>& home_takers,
    const std::vector<float>& away_takers, std::mt19937& rng)
{
  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  const auto chance = [](const std::vector<float>& takers, size_t kick)
  {
    // Capped below certainty so sudden death always terminates.
    return takers.empty()
               ? BASE_PENALTY_CONVERSION
               : std::min(takers[kick % takers.size()], MAX_PENALTY_CONVERSION);
  };
  int home = 0;
  int away = 0;
  for (size_t kick = 0; kick < PENALTY_ROUNDS; ++kick)
  {
    const auto left = static_cast<int>(PENALTY_ROUNDS - kick);
    if (roll(rng) < chance(home_takers, kick)) ++home;
    if (home > away + left || away > home + left - 1) break;
    if (roll(rng) < chance(away_takers, kick)) ++away;
    if (home > away + left - 1 || away > home + left - 1) break;
  }
  for (size_t kick = PENALTY_ROUNDS; home == away; ++kick)
  {
    if (roll(rng) < chance(home_takers, kick)) ++home;
    if (roll(rng) < chance(away_takers, kick)) ++away;
  }
  return {static_cast<uint8_t>(home), static_cast<uint8_t>(away)};
}

Competitions::KnockoutResolution Competitions::resolveDrawnKnockout(
    const Team& home, const Team& away, const StatsConfig& stats_config,
    uint32_t seed)
{
  std::mt19937 rng(mixSeed(seed, home.getId(), away.getId()));
  const double home_strength = teamStrength(home, stats_config);
  const double away_strength = teamStrength(away, stats_config);
  const auto rate = [](double own, double other, double edge)
  {
    const double factor = std::clamp(std::pow(own / other, STRENGTH_EXPONENT),
                                     MIN_STRENGTH_FACTOR, MAX_STRENGTH_FACTOR);
    return EXTRA_TIME_GOALS_PER_TEAM * factor * edge;
  };
  std::poisson_distribution<int> home_goals(
      rate(home_strength, away_strength, EXTRA_TIME_HOME_EDGE));
  std::poisson_distribution<int> away_goals(
      rate(away_strength, home_strength, 1.0 / EXTRA_TIME_HOME_EDGE));

  KnockoutResolution resolution;
  resolution.home_extra_goals =
      static_cast<uint8_t>(std::min(home_goals(rng), 9));
  resolution.away_extra_goals =
      static_cast<uint8_t>(std::min(away_goals(rng), 9));
  if (resolution.home_extra_goals == resolution.away_extra_goals)
  {
    const ShootoutResult shootout = simulateShootout(
        penaltyTakers(home, away), penaltyTakers(away, home), rng);
    resolution.penalties = true;
    resolution.home_penalties = shootout.home;
    resolution.away_penalties = shootout.away;
  }
  return resolution;
}

// ---------------- Promotion / relegation ----------------

std::vector<Competitions::LeagueMovement> Competitions::computeLeagueMovements(
    const GameData& gamedata,
    const std::unordered_map<LeagueID, std::vector<StandingRow>>& standings)
{
  std::map<LeagueID, std::vector<LeagueID>> children_of;
  for (const auto& [id, league] : gamedata.getLeagues())
  {
    const auto parent = league.getParentLeagueID();
    if (parent && gamedata.getLeague(*parent)) children_of[*parent].push_back(id);
  }

  std::vector<LeagueMovement> movements;
  for (auto& [parent_id, children] : children_of)
  {
    std::ranges::sort(children);
    const auto parent_rows = standings.find(parent_id);
    if (parent_rows == standings.end() || parent_rows->second.empty()) continue;
    const auto& parent_table = parent_rows->second;
    size_t relegation_index = parent_table.size();
    for (const LeagueID child_id : children)
    {
      const auto child_rows = standings.find(child_id);
      if (child_rows == standings.end()) continue;
      const auto& child_table = child_rows->second;
      const size_t slots =
          std::min({PROMOTION_SLOTS, child_table.size() / 4,
                    parent_table.size() / (4 * children.size())});
      for (size_t i = 0; i < slots && relegation_index > 0; ++i)
      {
        movements.push_back({child_table[i].team_id, child_id, parent_id});
        --relegation_index;
        movements.push_back(
            {parent_table[relegation_index].team_id, parent_id, child_id});
      }
    }
  }
  return movements;
}

void Competitions::applyLeagueMovements(
    GameData& gamedata, const std::vector<LeagueMovement>& movements)
{
  auto& leagues = gamedata.getLeagues();
  for (const LeagueMovement& movement : movements)
  {
    const auto from = leagues.find(movement.from);
    const auto to = leagues.find(movement.to);
    const auto team = gamedata.getTeam(movement.team_id);
    if (from == leagues.end() || to == leagues.end() || !team) continue;
    from->second.removeTeamID(movement.team_id);
    to->second.addTeamID(movement.team_id);
    team->get().setLeagueId(movement.to);
  }
}
