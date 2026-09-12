// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <sqlite3.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "global/language_manager.h"
#include "global/logger.h"
#include "global/runtime_paths.h"
#include "model/injury.h"
#include "model/match.h"
#include "model/match_report.h"
#include "model/world_generation.h"
#include "model/world_rng.h"
#include "model/world_simulation.h"

namespace
{
constexpr std::uint64_t WORLD_SEED = 20250702;

/**
 * Save slot unique to this process: ctest runs tests (and the unit_tests and
 * core_unit_tests copies of this file) in parallel, and saves are files.
 */
int uniqueSlot(int offset)
{
  return 100'000 + static_cast<int>(getpid() % 100'000) * 10 + offset;
}

/** Deletes the save of a slot when the test ends. */
struct SlotCleanup
{
  int slot;
  ~SlotCleanup() { RuntimePaths::removeSave(slot); }
};

std::unique_ptr<GameController> makeWorld(int slot,
                                          std::uint64_t seed = WORLD_SEED)
{
  Logger::init();
  auto controller = std::make_unique<GameController>();
  controller->newGame(slot, seed);
  return controller;
}

double overallOf(const GameController& controller, const Player& player)
{
  return player.getOverall(controller.getStatsConfig());
}

float statOf(const Player& player, const char* stat)
{
  const auto found = player.getStats().find(stat);
  return found == player.getStats().end() ? 0.0f : found->second;
}

bool isWinger(PlayerRole role)
{
  return role == PlayerRole::LW || role == PlayerRole::RW;
}

void execSql(sqlite3* db, const char* sql)
{
  char* error = nullptr;
  ASSERT_EQ(sqlite3_exec(db, sql, nullptr, nullptr, &error), SQLITE_OK)
      << (error ? error : "") << " in " << sql;
}
}  // namespace

// ---------------------------------------------------------------------------
// Deterministic RNG
// ---------------------------------------------------------------------------

TEST(WorldRngTest, SameSeedSameSequenceAndIndependentStreams)
{
  WorldRng a(99);
  WorldRng b(99);
  for (int i = 0; i < 100; ++i) ASSERT_EQ(a.next(), b.next());

  WorldRng injuries = WorldRng::stream(7, RngDomain::MatchInjury, 1, 2);
  WorldRng injuries_again = WorldRng::stream(7, RngDomain::MatchInjury, 1, 2);
  WorldRng youth = WorldRng::stream(7, RngDomain::YouthIntake, 1, 2);
  const std::uint64_t first = injuries.next();
  EXPECT_EQ(first, injuries_again.next());
  EXPECT_NE(first, youth.next());
  EXPECT_EQ(WorldRng::hashUniform(7, RngDomain::Retirement, 3, 4),
            WorldRng::hashUniform(7, RngDomain::Retirement, 3, 4));
  EXPECT_NE(WorldRng::hashUniform(7, RngDomain::Retirement, 3, 4),
            WorldRng::hashUniform(8, RngDomain::Retirement, 3, 4));
}

TEST(WorldRngTest, DistributionsStayInRange)
{
  WorldRng rng(1234);
  bool saw_low = false;
  bool saw_high = false;
  double normal_total = 0.0;
  constexpr int DRAWS = 20'000;
  for (int i = 0; i < DRAWS; ++i)
  {
    const int value = rng.uniformInt(3, 8);
    ASSERT_GE(value, 3);
    ASSERT_LE(value, 8);
    saw_low |= value == 3;
    saw_high |= value == 8;
    const double unit = rng.uniform01();
    ASSERT_GE(unit, 0.0);
    ASSERT_LT(unit, 1.0);
    normal_total += static_cast<double>(rng.normal(10.0f, 2.0f));
  }
  EXPECT_TRUE(saw_low && saw_high);
  EXPECT_NEAR(normal_total / DRAWS, 10.0, 0.1);

  const std::array<float, 3> weights = {0.0f, 1.0f, 3.0f};
  std::array<int, 3> picks{};
  for (int i = 0; i < DRAWS; ++i) ++picks[rng.weightedIndex(weights)];
  EXPECT_EQ(picks[0], 0);
  EXPECT_NEAR(static_cast<double>(picks[2]) / picks[1], 3.0, 0.3);
}

TEST(WorldRngTest, DayOrdinalMatchesTheCivilCalendar)
{
  EXPECT_EQ(dayOrdinal(GameDateValue(1970, 1, 1)), 0);
  EXPECT_EQ(dayOrdinal(GameDateValue(2025, 7, 2)), 20271);
  EXPECT_EQ(dayOrdinal(GameDateValue(2000, 2, 29)), 11016);
  EXPECT_EQ(dayOrdinal(GameDateValue(2026, 3, 1)) -
                dayOrdinal(GameDateValue(2026, 2, 28)),
            1);
  const GameDateValue date(2031, 11, 5);
  EXPECT_EQ(dateFromInt(dateToInt(date)), date);
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

TEST(WorldGenerationTest, SameSeedGeneratesTheSameWorld)
{
  const SlotCleanup slot_a{uniqueSlot(1)};
  const SlotCleanup slot_b{uniqueSlot(2)};
  const SlotCleanup slot_c{uniqueSlot(3)};
  const auto first = makeWorld(slot_a.slot);
  const auto second = makeWorld(slot_b.slot);
  const auto other = makeWorld(slot_c.slot, WORLD_SEED + 1);

  const auto& players_a = first->getGameData()->getPlayers();
  const auto& players_b = second->getGameData()->getPlayers();
  ASSERT_EQ(players_a.size(), players_b.size());
  for (const auto& [id, player] : players_a)
  {
    const auto twin = players_b.find(id);
    ASSERT_NE(twin, players_b.end());
    ASSERT_EQ(player.getName(), twin->second.getName());
    ASSERT_EQ(player.getWage(), twin->second.getWage());
    ASSERT_EQ(player.getStats(), twin->second.getStats());
    ASSERT_FLOAT_EQ(player.getPotential(), twin->second.getPotential());
    ASSERT_EQ(player.getTraits().professionalism,
              twin->second.getTraits().professionalism);
  }
  for (const auto& team : first->getTeams())
  {
    const Team& twin = second->getTeamById(team.get().getId())->get();
    EXPECT_EQ(team.get().getReputation(), twin.getReputation());
    EXPECT_EQ(team.get().getStadiumCapacity(), twin.getStadiumCapacity());
    EXPECT_EQ(team.get().getFinances().getBalance(),
              twin.getFinances().getBalance());
  }

  const auto& players_other = other->getGameData()->getPlayers();
  std::size_t differences = 0;
  for (const auto& [id, player] : players_a)
  {
    const auto found = players_other.find(id);
    if (found == players_other.end() ||
        found->second.getStats() != player.getStats())
      ++differences;
  }
  EXPECT_GT(differences, players_a.size() / 2);
}

TEST(WorldGenerationTest, RolesLeaguesAndReputationShapeTheWorld)
{
  const SlotCleanup slot{uniqueSlot(0)};
  const auto controller = makeWorld(slot.slot);
  const auto gamedata = controller->getGameData();

  double cb_defending = 0.0;
  double cb_shooting = 0.0;
  double cb_pace = 0.0;
  double winger_pace = 0.0;
  double gk_keeping = 0.0;
  double gk_shooting = 0.0;
  int cbs = 0;
  int wingers = 0;
  int keepers = 0;
  std::set<Language> nationalities;
  for (const auto& [id, player] : gamedata->getPlayers())
  {
    ASSERT_GE(player.getContractYears(), 1);
    ASSERT_LE(player.getContractYears(), 5);
    ASSERT_GE(player.getAge(), 16);
    ASSERT_LE(player.getAge(), 37);
    ASSERT_GE(player.getPotential() + 0.01,
              static_cast<float>(overallOf(*controller, player)));
    nationalities.insert(player.getNationality());
    if (player.getRole() == PlayerRole::CB)
    {
      cb_defending += statOf(player, "Defending");
      cb_shooting += statOf(player, "Shooting");
      cb_pace += statOf(player, "Pace");
      ++cbs;
    }
    else if (isWinger(player.getRole()))
    {
      winger_pace += statOf(player, "Pace");
      ++wingers;
    }
    else if (player.getRole() == PlayerRole::GK)
    {
      gk_keeping += statOf(player, "Goalkeeping");
      gk_shooting += statOf(player, "Shooting");
      ++keepers;
    }
  }
  ASSERT_GT(cbs, 0);
  ASSERT_GT(wingers, 0);
  ASSERT_GT(keepers, 0);
  EXPECT_GT(cb_defending / cbs, cb_shooting / cbs + 15.0);
  EXPECT_GT(winger_pace / wingers, cb_pace / cbs + 8.0);
  EXPECT_GT(gk_keeping / keepers, gk_shooting / keepers + 30.0);
  EXPECT_GE(nationalities.size(), 10u);

  // Reputation drives quality, stadiums and payroll; top leagues pay more.
  std::map<LeagueID, std::vector<const Team*>> by_league;
  for (const auto& team : controller->getTeams())
    by_league[team.get().getLeagueId()].push_back(&team.get());
  const auto payroll = [&](const Team* team)
  { return controller->getWeeklyWageBill(team->getId()); };
  const auto mean_payroll = [&](LeagueID league)
  {
    double total = 0.0;
    for (const Team* team : by_league[league])
      total += static_cast<double>(payroll(team));
    return total / static_cast<double>(by_league[league].size());
  };
  EXPECT_GT(mean_payroll(3), 5.0 * mean_payroll(6))
      << "English top flight must out-pay the Italian second tier";

  const WorldSimulation& world = controller->getGame()->getWorld();
  for (auto& [league_id, teams] : by_league)
  {
    std::ranges::sort(teams, [](const Team* a, const Team* b)
                      { return a->getReputation() > b->getReputation(); });
    const Team* giant = teams.front();
    const Team* minnow = teams.back();
    EXPECT_GT(giant->getReputation(), minnow->getReputation());
    EXPECT_GT(world.lineupStrength(giant->getId()),
              world.lineupStrength(minnow->getId()));
    EXPECT_GT(payroll(giant), payroll(minnow));
    EXPECT_GT(giant->getStadiumCapacity(), minnow->getStadiumCapacity());
    for (const Team* team : teams)
    {
      const Finances& finances = team->getFinances();
      EXPECT_GE(finances.getWageBudget(), payroll(team));
      EXPECT_LE(finances.getTransferBudget(), finances.getBalance());
      EXPECT_GE(team->getStadiumCapacity(), 2'500u);
      EXPECT_GT(team->getProfile().ticket_price, 0u);
    }
  }
}

// ---------------------------------------------------------------------------
// Injuries
// ---------------------------------------------------------------------------

TEST(WorldGenerationTest, NamesFollowNationalityAndAreUnique)
{
  const SlotCleanup slot{uniqueSlot(0)};
  const auto controller = makeWorld(slot.slot);
  auto gamedata = controller->getGameData();
  const NamePool& pool = NamePool::instance();

  std::map<std::string, int> full_names;
  std::map<LeagueID, std::pair<int, int>> domestic;  // domestic, total
  int matching_first_names = 0;
  for (const auto& [id, player] : gamedata->getPlayers())
  {
    ++full_names[player.getName()];
    EXPECT_FALSE(pool.excluded_full_names.contains(player.getName()));
    const auto& firsts = pool.firstNames(player.getNationality());
    if (std::ranges::contains(firsts, player.getFirstName()))
      ++matching_first_names;
    const auto team = gamedata->getTeam(player.getTeamId());
    if (!team || player.getTeamId() == FREE_AGENTS_TEAM_ID) continue;
    auto& [home_grown, total] = domestic[team->get().getLeagueId()];
    ++total;
    if (player.getNationality() ==
        leagueProfile(team->get().getLeagueId()).domestic_nationality)
      ++home_grown;
  }
  int duplicates = 0;
  for (const auto& [name, count] : full_names) duplicates += count - 1;
  EXPECT_EQ(duplicates, 0);
  EXPECT_GT(matching_first_names,
            static_cast<int>(0.98 * gamedata->getPlayers().size()));

  for (const auto& [league_id, counts] : domestic)
  {
    const double share = static_cast<double>(counts.first) / counts.second;
    EXPECT_GE(share, 0.62) << "league " << league_id;
    EXPECT_LE(share, 0.92) << "league " << league_id;
  }

  // At most one surname appears twice in a squad, none three times.
  for (const auto& team : controller->getTeams())
  {
    std::map<std::string, int> surnames;
    for (const auto& player : controller->getPlayersForTeam(team.get().getId()))
      ++surnames[player.get().getLastName()];
    int repeated = 0;
    for (const auto& [surname, count] : surnames)
    {
      EXPECT_LE(count, 2) << team.get().getName() << " " << surname;
      if (count > 1) ++repeated;
    }
    EXPECT_LE(repeated, 1) << team.get().getName();
  }
}

TEST(WorldGenerationTest, VeteransHaveNoHiddenGrowth)
{
  const SlotCleanup slot{uniqueSlot(1)};
  const auto controller = makeWorld(slot.slot);
  int veterans = 0;
  for (const auto& [id, player] : controller->getGameData()->getPlayers())
  {
    if (player.getAge() < WorldTuning::Generation::VETERAN_AGE) continue;
    ++veterans;
    EXPECT_LE(player.getPotential() - overallOf(*controller, player),
              WorldTuning::Generation::VETERAN_HEADROOM + 0.01)
        << player.getName() << " age " << player.getAge();
  }
  EXPECT_GT(veterans, 500);
  EXPECT_FLOAT_EQ(WorldGeneration::maxPotential(34, 70.0f),
                  70.0f + WorldTuning::Generation::VETERAN_HEADROOM);
  EXPECT_GT(WorldGeneration::maxPotential(19, 60.0f), 90.0f);
}

TEST(WorldGenerationTest, PreseasonFriendliesStayInTheRegion)
{
  const SlotCleanup slot{uniqueSlot(2)};
  const auto controller = makeWorld(slot.slot);
  auto gamedata = controller->getGameData();
  int friendlies = 0;
  for (const auto& [date, matches] :
       controller->getGame()->getCalendar().getFullCalendar())
  {
    for (const Match& match : matches)
    {
      if (match.getMatchType() != MatchType::FRIENDLY) continue;
      ++friendlies;
      const LeagueID home = gamedata->getTeam(match.getHomeTeamId())
                                ->get()
                                .getLeagueId();
      const LeagueID away = gamedata->getTeam(match.getAwayTeamId())
                                ->get()
                                .getLeagueId();
      EXPECT_EQ(leagueProfile(home).region, leagueProfile(away).region)
          << static_cast<int>(home) << " vs " << static_cast<int>(away);
    }
  }
  EXPECT_GT(friendlies, 100);
}

TEST(InjuryModelTest, DiagnosisMixAndLayoffsFollowTheStudies)
{
  WorldRng rng(77);
  constexpr int DRAWS = 40'000;
  int hamstrings = 0;
  int minor = 0;
  int match_acl = 0;
  std::vector<int> acl_days;
  for (int i = 0; i < DRAWS; ++i)
  {
    const Injury injury =
        InjuryModel::draw(rng, InjuryContext::Match, InjuryType::None, false);
    ASSERT_NE(injury.type, InjuryType::None);
    ASSERT_GE(injury.days, 1);
    if (injury.type == InjuryType::HamstringStrain ||
        injury.type == InjuryType::HamstringTightness)
      ++hamstrings;
    if (InjuryModel::severity(injury.days) == InjurySeverity::Minor) ++minor;
    if (injury.type == InjuryType::KneeAcl)
    {
      ++match_acl;
      acl_days.push_back(injury.days);
    }
  }
  const double hamstring_share = static_cast<double>(hamstrings) / DRAWS;
  EXPECT_GT(hamstring_share, 0.18);
  EXPECT_LT(hamstring_share, 0.30);
  const double minor_share = static_cast<double>(minor) / DRAWS;
  EXPECT_GT(minor_share, 0.30);
  EXPECT_LT(minor_share, 0.65);
  ASSERT_GT(acl_days.size(), 100u);
  std::ranges::nth_element(
      acl_days,
      acl_days.begin() + static_cast<std::ptrdiff_t>(acl_days.size() / 2));
  const int acl_median = acl_days[acl_days.size() / 2];
  EXPECT_GT(acl_median, 170);
  EXPECT_LT(acl_median, 240);

  int training_acl = 0;
  for (int i = 0; i < DRAWS; ++i)
  {
    if (InjuryModel::draw(rng, InjuryContext::Training, InjuryType::None, false)
            .type == InjuryType::KneeAcl)
      ++training_acl;
  }
  EXPECT_LT(training_acl * 5, match_acl);
}

TEST(WorldSimulationTest, MatchConsequencesDrainRecoverAndInjure)
{
  const SlotCleanup slot{uniqueSlot(0)};
  const auto controller = makeWorld(slot.slot);
  const auto gamedata = controller->getGameData();
  const TeamID managed = controller->getTeams().front().get().getId();
  controller->selectManagedTeam(managed);
  const PlayerID player_id =
      controller->getPlayersForTeam(managed).front().get().getId();
  const Player& player = gamedata->getPlayers().at(player_id);

  ASSERT_TRUE(controller->applyMatchConsequences(player_id, 90, 68.0f, false));
  EXPECT_FLOAT_EQ(player.getDynamics().condition, 68.0f);
  EXPECT_EQ(player.getDynamics().season_minutes, 90);
  EXPECT_FALSE(controller->applyMatchConsequences(player_id, 90, 50.0f, false))
      << "consequences apply once per player and day";

  // Recovery follows a 48 h time constant: ~94% after three days.
  WorldSimulation world(gamedata);
  GameDateValue date = controller->getCurrentDate();
  for (int day = 0; day < 3; ++day)
  {
    date = date + 1;
    world.onDayAdvanced(date, managed);
  }
  if (player.isAvailable())
  {
    EXPECT_GT(player.getDynamics().condition, 88.0f);
    EXPECT_LT(player.getDynamics().condition, 100.0f);
  }

  // An engine-reported injury makes the player unavailable.
  const PlayerID injured_id =
      controller->getPlayersForTeam(managed).back().get().getId();
  ASSERT_TRUE(controller->applyMatchConsequences(injured_id, 30, 80.0f, true));
  EXPECT_FALSE(controller->isPlayerAvailable(injured_id));
  const auto injured = controller->getInjuredPlayers(managed);
  EXPECT_TRUE(std::ranges::contains(injured, injured_id));
  EXPECT_GT(controller->getUnreadInboxCount(), 0u);
  const auto& inbox = controller->getInbox();
  EXPECT_TRUE(std::ranges::any_of(inbox,
                                  [&](const InboxMessage& message)
                                  {
                                    return message.category ==
                                               InboxCategory::Injury &&
                                           message.player_id == injured_id;
                                  }));
}

TEST(WorldSimulationTest, InjuredPlayersAreLeftOutOfAiLineups)
{
  const SlotCleanup slot{uniqueSlot(0)};
  const auto controller = makeWorld(slot.slot);
  auto gamedata = controller->getGameData();
  Team& team =
      gamedata->getTeams().at(controller->getTeams().back().get().getId());
  const Player* goalkeeper = team.getLineup().getGoalkeeper();
  ASSERT_NE(goalkeeper, nullptr);
  const PlayerID keeper_id = goalkeeper->getId();

  PlayerDynamics& dynamics =
      gamedata->getPlayers().at(keeper_id).mutableDynamics();
  dynamics.injury = InjuryType::KneeMcl;
  dynamics.injury_days = 20;
  team.generateStartingXI(*gamedata, gamedata->getStatsConfig());

  ASSERT_NE(team.getLineup().getGoalkeeper(), nullptr);
  EXPECT_NE(team.getLineup().getGoalkeeper()->getId(), keeper_id);
  for (const auto& positioned : team.getLineup().getOutfieldPlayers())
    EXPECT_NE(positioned.player->getId(), keeper_id);
  for (const Player* reserve : team.getLineup().getReserves())
    EXPECT_NE(reserve->getId(), keeper_id);
}

// ---------------------------------------------------------------------------
// A year of world events (synthetic results, no match engine)
// ---------------------------------------------------------------------------

namespace
{
struct YearDigest
{
  std::int64_t balances = 0;
  double overall = 0.0;
  std::size_t injured_players = 0;
  std::size_t players = 0;
};

/** Prints revenue, wage ratio and net result per club of each league over
 * the first season (only visible with ctest -V). */
void printLeagueFinances(const GameController& controller)
{
  struct LeagueTotals
  {
    double income = 0.0;
    double wages = 0.0;
    double net = 0.0;
    double transfers = 0.0;
    std::vector<double> net_ratios;
    double first_month_wages = 0.0;
    double last_month_wages = 0.0;
    std::size_t squad_players = 0;
    int negative_cash = 0;
    int clubs = 0;
  };
  std::map<LeagueID, LeagueTotals> leagues;
  for (const auto& team : controller.getTeams())
  {
    const FinanceSummary season = team.get().getFinances().summarize(
        GameDateValue(2025, 7, 3), GameDateValue(2026, 7, 2));
    const auto category = [&](FinanceCategory value)
    {
      return static_cast<double>(
          season.by_category[static_cast<std::size_t>(value)]);
    };
    LeagueTotals& totals = leagues[team.get().getLeagueId()];
    const double transfers = category(FinanceCategory::TransferFeeIn) +
                             category(FinanceCategory::TransferFeeOut);
    const double income = static_cast<double>(season.income) -
                          category(FinanceCategory::TransferFeeIn);
    const double net = static_cast<double>(season.net()) - transfers;
    totals.income += income;
    totals.wages -= category(FinanceCategory::Wages);
    totals.net += net;
    totals.transfers += transfers;
    totals.net_ratios.push_back(income > 0.0 ? net / income : 0.0);
    const auto monthWages = [&](const GameDateValue& from,
                                const GameDateValue& to)
    {
      return -static_cast<double>(
          team.get().getFinances().summarize(from, to).by_category
              [static_cast<std::size_t>(FinanceCategory::Wages)]);
    };
    totals.first_month_wages +=
        monthWages(GameDateValue(2025, 7, 3), GameDateValue(2025, 8, 2));
    totals.last_month_wages +=
        monthWages(GameDateValue(2026, 6, 1), GameDateValue(2026, 6, 30));
    totals.squad_players += team.get().getPlayerIDs().size();
    if (team.get().getFinances().getBalance() < 0) ++totals.negative_cash;
    ++totals.clubs;
  }
  for (auto& [league_id, totals] : leagues)
  {
    std::ranges::sort(totals.net_ratios);
    std::cout << "[world-calibration] league " << static_cast<int>(league_id)
              << " revenue/club=" << totals.income / totals.clubs / 1e6
              << "M wages/revenue=" << totals.wages / totals.income
              << " operating net/club=" << totals.net / totals.clubs / 1e6
              << "M median net=" << totals.net_ratios[totals.net_ratios.size() / 2]
              << " transfers/club=" << totals.transfers / totals.clubs / 1e6
              << "M negative cash=" << totals.negative_cash << "/"
              << totals.clubs << " payroll growth="
              << totals.last_month_wages /
                     std::max(1.0, totals.first_month_wages)
              << " squad=" << totals.squad_players / totals.clubs << "\n";
  }
}

/** Plays a full year: daily world processing plus weekly synthetic results
 * for the managed club's league. */
YearDigest playYear(GameController& controller, WorldSimulation& world,
                    TeamID managed, int days)
{
  auto gamedata = controller.getGameData();
  const LeagueID league_id = gamedata->getTeam(managed)->get().getLeagueId();
  std::vector<TeamID> league_teams =
      gamedata->getLeague(league_id)->get().getTeamIDs();
  std::ranges::sort(league_teams);
  WorldRng scores(99);
  GameDateValue date = controller.getCurrentDate();
  int round = 0;
  for (int day = 0; day < days; ++day)
  {
    date = date + 1;
    world.onDayAdvanced(date, managed);
    if (date.month == 7 && date.day == 1)
    {
      world.onSeasonEnd(date, managed);
      world.onSeasonStart(date, managed);
    }
    const bool in_season = date.month >= 9 || date.month <= 5;
    if (!in_season || dayOrdinal(date) % 7 != 3) continue;
    // Round robin pairing by rotation.
    ++round;
    const std::size_t size = league_teams.size();
    for (std::size_t i = 0; i < size / 2; ++i)
    {
      const TeamID home =
          league_teams[(i + static_cast<std::size_t>(round)) % size];
      const TeamID away =
          league_teams[(size - 1 - i + static_cast<std::size_t>(round)) % size];
      if (home == away) continue;
      Match match(home, away, date, MatchType::LEAGUE, league_id);
      const auto home_goals =
          static_cast<std::uint8_t>(scores.uniformInt(0, 3));
      const auto away_goals =
          static_cast<std::uint8_t>(scores.uniformInt(0, 2));
      match.setPlayedResult(home_goals, away_goals);
      auto& league = gamedata->getLeagues().at(league_id);
      if (home_goals > away_goals)
        league.addPoints(home, 3);
      else if (home_goals < away_goals)
        league.addPoints(away, 3);
      else
      {
        league.addPoints(home, 1);
        league.addPoints(away, 1);
      }
      MatchReport report;
      world.onMatchPlayed(match, report, managed);
    }
  }
  YearDigest digest;
  for (const auto& team : controller.getTeams())
    digest.balances += team.get().getFinances().getBalance();
  for (const auto& [id, player] : gamedata->getPlayers())
  {
    digest.overall += overallOf(controller, player);
    if (player.getDynamics().last_injury != InjuryType::None)
      ++digest.injured_players;
  }
  digest.players = gamedata->getPlayers().size();
  return digest;
}
}  // namespace

TEST(WorldSimulationTest, AYearOfDevelopmentInjuriesYouthFinanceAndNews)
{
  const SlotCleanup slot{uniqueSlot(0)};
  const auto controller = makeWorld(slot.slot);
  auto gamedata = controller->getGameData();
  const TeamID managed = controller->getTeams().front().get().getId();
  const PlayerID first_new_id = gamedata->peekNextPlayerId();

  // Young, high-potential players should improve over a year.
  std::map<PlayerID, double> prospects;
  for (const auto& [id, player] : gamedata->getPlayers())
  {
    if (player.getAge() <= 20 &&
        player.getPotential() > overallOf(*controller, player) + 8.0)
      prospects.emplace(id, overallOf(*controller, player));
  }
  ASSERT_GT(prospects.size(), 50u);

  WorldSimulation world(gamedata);
  const auto started = std::chrono::steady_clock::now();
  const YearDigest digest = playYear(*controller, world, managed, 365);
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();
  std::cout << "[world] 365 days of world processing for "
            << controller->getTeams().size() << " clubs: " << seconds << " s\n";
  EXPECT_LT(seconds, 20.0);

  // Development: prospects still at the club grew on average.
  double growth = 0.0;
  int tracked = 0;
  for (const auto& [id, start] : prospects)
  {
    const auto player = gamedata->getPlayer(id);
    if (!player) continue;
    growth += overallOf(*controller, player->get()) - start;
    ++tracked;
  }
  ASSERT_GT(tracked, 0);
  EXPECT_GT(growth / tracked, 2.0);

  // Injuries: most players pick up at least one knock over a year.
  const double injured_share = static_cast<double>(digest.injured_players) /
                               static_cast<double>(digest.players);
  EXPECT_GT(injured_share, 0.30);
  EXPECT_LT(injured_share, 0.98);

  // Youth intake in March and retirements at the season end.
  std::size_t youth = 0;
  for (const auto& [id, player] : gamedata->getPlayers())
  {
    if (id < first_new_id) continue;
    ++youth;
    EXPECT_GE(player.getAge(), 15);
    EXPECT_LE(player.getAge(), 18);
    EXPECT_NE(player.getTeamId(), FREE_AGENTS_TEAM_ID);
  }
  EXPECT_GE(youth, 3u * controller->getTeams().size());
  EXPECT_FALSE(gamedata->getRemovedPlayerIds().empty()) << "no retirements";

  // Graduates get names nobody else carries.
  std::set<std::string> names;
  for (const auto& [id, player] : gamedata->getPlayers())
    EXPECT_TRUE(names.insert(player.getName()).second) << player.getName();

  // Finances: ledgers reconcile and every revenue stream was paid.
  for (const auto& team : controller->getTeams())
  {
    const Finances& finances = team.get().getFinances();
    ASSERT_EQ(finances.getBalance(), finances.ledgerTotal());
  }
  printLeagueFinances(*controller);
  std::cout << "[world-calibration] injured share=" << injured_share
            << " youth=" << youth
            << " retired=" << gamedata->getRemovedPlayerIds().size()
            << " prospect growth=" << growth / tracked << "\n";
  const Finances& managed_finances =
      gamedata->getTeam(managed)->get().getFinances();
  const FinanceSummary year = managed_finances.summarize(
      controller->getCurrentDate(), GameDateValue(2026, 7, 2));
  const auto total = [&](FinanceCategory category)
  { return year.by_category[static_cast<std::size_t>(category)]; };
  EXPECT_LT(total(FinanceCategory::Wages), 0);
  EXPECT_GT(total(FinanceCategory::Matchday), 0);
  EXPECT_GT(total(FinanceCategory::Broadcasting), 0);
  EXPECT_GT(total(FinanceCategory::Sponsorship), 0);
  EXPECT_GT(total(FinanceCategory::PrizeMoney), 0);
  EXPECT_LT(total(FinanceCategory::Staff), 0);
  EXPECT_LT(total(FinanceCategory::Facilities), 0);

  // Bounded dynamic state.
  for (const auto& [id, player] : gamedata->getPlayers())
  {
    const PlayerDynamics& dynamics = player.getDynamics();
    ASSERT_GE(dynamics.morale, 0.0f);
    ASSERT_LE(dynamics.morale, 100.0f);
    ASSERT_GE(dynamics.condition, 0.0f);
    ASSERT_LE(dynamics.condition, 100.0f);
  }

  // News for the managed club.
  std::set<InboxCategory> categories;
  for (const InboxMessage& message : world.getInbox().getMessages())
  {
    categories.insert(message.category);
    EXPECT_FALSE(message.formatTitle().empty());
  }
  for (const InboxCategory expected :
       {InboxCategory::Match, InboxCategory::Finance, InboxCategory::Board,
        InboxCategory::Youth, InboxCategory::Injury})
    EXPECT_TRUE(categories.contains(expected)) << inboxCategoryKey(expected);
  EXPECT_EQ(world.getBoardState().team_id, managed);
}

TEST(WorldSimulationTest, TakingChargeFillsTheDayOneInbox)
{
  const SlotCleanup slot{uniqueSlot(3)};
  ASSERT_TRUE(LanguageManager::instance().loadLanguage(Language::EN));
  auto controller = makeWorld(slot.slot);
  const TeamID managed = controller->getTeams().front().get().getId();
  controller->selectManagedTeam(managed);

  // Objective and budgets are known before the first day is played.
  const BoardState board = controller->getBoardState();
  EXPECT_EQ(board.team_id, managed);
  EXPECT_GT(board.expected_position, 0);
  EXPECT_GT(board.target_position, 0);

  std::set<std::string> titles;
  for (const InboxMessage& message : controller->getInbox())
  {
    titles.insert(message.title_key);
    std::cout << "TMPDUMP " << message.formatTitle() << "\n" << message.formatBody() << "\n";
    EXPECT_FALSE(message.read);
    EXPECT_EQ(message.formatBody().find('{'), std::string::npos)
        << message.body_key;
    EXPECT_EQ(message.formatBody().find("#0"), std::string::npos);
  }
  for (const char* expected :
       {"INBOX_BOARD_WELCOME_TITLE", "INBOX_SQUAD_REPORT_TITLE",
        "INBOX_PRESEASON_TITLE", "INBOX_SCOUT_SUGGESTION_TITLE"})
    EXPECT_TRUE(titles.contains(expected)) << expected;
  EXPECT_EQ(controller->getUnreadInboxCount(), 4u);

  // Re-selecting the same club does not repeat the news.
  controller->selectManagedTeam(managed);
  EXPECT_EQ(controller->getInbox().size(), 4u);

  // The board survives a reload.
  controller->saveGame();
  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(slot.slot));
  EXPECT_EQ(controller->getBoardState().expected_position,
            board.expected_position);
  EXPECT_EQ(controller->getInbox().size(), 4u);
}

TEST(WorldSimulationTest, InboxStaysQuietThroughTheFirstMonths)
{
  const SlotCleanup slot{uniqueSlot(4)};
  const auto controller = makeWorld(slot.slot);
  const TeamID managed = controller->getTeams().front().get().getId();
  controller->selectManagedTeam(managed);
  while (controller->getCurrentDate() < GameDateValue(2025, 10, 1))
    controller->advanceDay();

  std::map<std::string, std::pair<int, int>> by_title;  // unread, read
  std::map<int, int> unread_by_month;
  for (const InboxMessage& message : controller->getInbox())
  {
    auto& counts = by_title[message.title_key];
    (message.read ? counts.second : counts.first) += 1;
    if (!message.read) ++unread_by_month[message.date.month];
  }
  for (const auto& [title, counts] : by_title)
    std::cout << "[inbox] " << title << " unread=" << counts.first
              << " read=" << counts.second << "\n";
  for (const auto& [month, unread] : unread_by_month)
  {
    std::cout << "[inbox] month " << month << " unread " << unread << "\n";
    // July also holds the four day-one messages.
    EXPECT_LE(unread, month == 7 ? 19 : 15) << "month " << month;
  }
}

TEST(WorldSimulationTest, NegativeCashWarnsThenFreezesTransfers)
{
  const SlotCleanup slot{uniqueSlot(5)};
  auto controller = makeWorld(slot.slot);
  const TeamID managed = controller->getTeams().front().get().getId();
  controller->selectManagedTeam(managed);
  Finances& finances =
      controller->getGameData()->getTeams().at(managed).getFinances();
  EXPECT_GT(controller->transferBudgetForTeam(managed), 0u);
  // The spendable budget keeps twelve weeks of payroll in the bank.
  const std::int64_t payroll = controller->getWeeklyWageBill(managed);
  EXPECT_LE(static_cast<std::int64_t>(controller->transferBudgetForTeam(managed)),
            finances.getBalance() - 12 * payroll);

  const float confidence = controller->getBoardState().confidence;
  finances.addBalance(-finances.getBalance() - 300'000'000);
  EXPECT_EQ(controller->transferBudgetForTeam(managed), 0u);

  // The first weekly review warns; the next monthly review freezes.
  const auto count = [&](const char* title)
  {
    return std::ranges::count(controller->getInbox(), std::string(title),
                              &InboxMessage::title_key);
  };
  while (count("INBOX_BOARD_CASH_WARNING_TITLE") == 0)
    controller->advanceDay();
  EXPECT_FALSE(controller->isTransferEmbargoed());
  EXPECT_LT(controller->getBoardState().confidence, confidence);
  while (!controller->isTransferEmbargoed()) controller->advanceDay();
  EXPECT_EQ(controller->getCurrentDate().day, 1);
  EXPECT_EQ(count("INBOX_BOARD_EMBARGO_TITLE"), 1);

  // The embargo is rebuilt from the ledger after a reload.
  controller->saveGame();
  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(slot.slot));
  EXPECT_TRUE(controller->isTransferEmbargoed());

  // New money lifts it at the next weekly review.
  controller->getGameData()->getTeams().at(managed).getFinances().addBalance(
      600'000'000);
  EXPECT_FALSE(controller->isTransferEmbargoed());
  for (int day = 0; day < 7; ++day) controller->advanceDay();
  EXPECT_EQ(count("INBOX_BOARD_EMBARGO_LIFTED_TITLE"), 1);
}

TEST(WorldSimulationTest, SameSeedSameHistory)
{
  const SlotCleanup slot_a{uniqueSlot(1)};
  const SlotCleanup slot_b{uniqueSlot(2)};
  const auto first = makeWorld(slot_a.slot);
  const auto second = makeWorld(slot_b.slot);
  const TeamID managed = first->getTeams().front().get().getId();
  WorldSimulation world_a(first->getGameData());
  WorldSimulation world_b(second->getGameData());
  const YearDigest a = playYear(*first, world_a, managed, 90);
  const YearDigest b = playYear(*second, world_b, managed, 90);
  EXPECT_EQ(a.balances, b.balances);
  EXPECT_DOUBLE_EQ(a.overall, b.overall);
  EXPECT_EQ(a.injured_players, b.injured_players);
  EXPECT_EQ(world_a.getInbox().getMessages().size(),
            world_b.getInbox().getMessages().size());
}

TEST(WorldSimulationTest, BoardSetsObjectivesAndLosesPatienceOnlySlowly)
{
  BoardState board;
  board.objective = BoardModel::objectiveFor(3, 20);
  EXPECT_EQ(board.objective, BoardObjective::TopFour);
  board.target_position = BoardModel::targetPosition(board.objective, 20);
  EXPECT_EQ(board.target_position, 4);
  EXPECT_EQ(BoardModel::objectiveFor(1, 20), BoardObjective::WinLeague);
  EXPECT_EQ(BoardModel::objectiveFor(19, 20), BoardObjective::AvoidRelegation);

  // Favourites are expected to take more points at home.
  EXPECT_GT(BoardModel::expectedPoints(75.0f, 65.0f, true),
            BoardModel::expectedPoints(65.0f, 75.0f, true));
  EXPECT_GT(BoardModel::expectedPoints(70.0f, 70.0f, true),
            BoardModel::expectedPoints(70.0f, 70.0f, false));

  // A few bad months only warn; a sustained collapse ends in dismissal.
  int reviews_until_dismissal = 0;
  bool warned = false;
  for (int month = 0; month < 12 && !board.dismissed; ++month)
  {
    for (int match = 0; match < 4; ++match)
      BoardModel::recordMatch(board, 0.0f, 2.0f);
    const BoardReviewOutcome outcome =
        BoardModel::monthlyReview(board, 20, 20, true, true);
    warned |= outcome == BoardReviewOutcome::Warning;
    ++reviews_until_dismissal;
  }
  EXPECT_TRUE(warned);
  EXPECT_TRUE(board.dismissed);
  EXPECT_GE(reviews_until_dismissal, 4);

  BoardState happy;
  happy.target_position = 10;
  for (int month = 0; month < 10; ++month)
  {
    for (int match = 0; match < 4; ++match)
      BoardModel::recordMatch(happy, 3.0f, 1.3f);
    EXPECT_EQ(BoardModel::monthlyReview(happy, 3, 20, false, false),
              BoardReviewOutcome::Satisfied);
  }
  EXPECT_GT(happy.confidence, 70.0f);
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

TEST(WorldPersistenceTest, WorldStateSurvivesSaveAndLoad)
{
  const SlotCleanup slot{uniqueSlot(0)};
  auto controller = makeWorld(slot.slot);
  auto gamedata = controller->getGameData();
  const TeamID managed = controller->getTeams().front().get().getId();
  controller->selectManagedTeam(managed);
  controller->advanceDay();
  ASSERT_FALSE(controller->getInbox().empty()) << "board objective expected";
  const std::uint32_t read_id = controller->getInbox().front().id;
  ASSERT_TRUE(controller->markInboxMessageRead(read_id));

  const PlayerID player_id =
      controller->getPlayersForTeam(managed).front().get().getId();
  Player& player = gamedata->getPlayers().at(player_id);
  PlayerDynamics& dynamics = player.mutableDynamics();
  dynamics.injury = InjuryType::HamstringStrain;
  dynamics.injury_days = 12;
  dynamics.morale = 33.5f;
  player.pushMatchRating(7.4f);
  player.pushMatchRating(6.1f);
  ASSERT_TRUE(controller->setTicketPrice(77));

  const Player before = player;
  const Team& team = controller->getManagedTeam()->get();
  const ClubProfile profile = team.getProfile();
  const std::string form = team.getRecentForm();
  const auto balance = team.getFinances().getBalance();
  const auto ledger = team.getFinances().getLedger().size();
  const auto inbox_size = controller->getInbox().size();
  const auto unread = controller->getUnreadInboxCount();
  const BoardState board = controller->getBoardState();
  const auto seed = controller->getWorldSeed();
  const auto next_id = gamedata->peekNextPlayerId();
  controller->saveGame();

  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(slot.slot));
  gamedata = controller->getGameData();
  const Player& after = gamedata->getPlayers().at(player_id);
  EXPECT_FLOAT_EQ(after.getPotential(), before.getPotential());
  EXPECT_EQ(after.getTraits().ambition, before.getTraits().ambition);
  EXPECT_EQ(after.getTraits().injury_proneness,
            before.getTraits().injury_proneness);
  EXPECT_EQ(after.getDynamics().injury, InjuryType::HamstringStrain);
  EXPECT_EQ(after.getDynamics().injury_days, 12);
  EXPECT_NEAR(after.getDynamics().morale, 33.5f, 0.01f);
  EXPECT_NEAR(after.getDynamics().condition, before.getDynamics().condition,
              0.01f);
  EXPECT_EQ(after.getDynamics().rating_count, 2);
  EXPECT_NEAR(after.getForm(), before.getForm(), 0.01f);
  EXPECT_FALSE(controller->isPlayerAvailable(player_id));

  const Team& reloaded = controller->getManagedTeam()->get();
  EXPECT_EQ(reloaded.getProfile().ticket_price, 77u);
  EXPECT_EQ(reloaded.getReputation(), profile.reputation);
  EXPECT_EQ(reloaded.getStadiumCapacity(), profile.stadium_capacity);
  EXPECT_EQ(reloaded.getProfile().youth_facilities, profile.youth_facilities);
  EXPECT_EQ(reloaded.getRecentForm(), form);
  EXPECT_EQ(reloaded.getFinances().getBalance(), balance);
  EXPECT_EQ(reloaded.getFinances().getLedger().size(), ledger);

  EXPECT_EQ(controller->getInbox().size(), inbox_size);
  EXPECT_EQ(controller->getUnreadInboxCount(), unread);
  EXPECT_TRUE(controller->getInbox().front().read);
  EXPECT_EQ(controller->getBoardState().team_id, board.team_id);
  EXPECT_EQ(controller->getBoardState().objective, board.objective);
  EXPECT_NEAR(controller->getBoardState().confidence, board.confidence, 0.01f);
  EXPECT_EQ(controller->getWorldSeed(), seed);
  EXPECT_EQ(controller->getGameData()->peekNextPlayerId(), next_id);
}

TEST(WorldPersistenceTest, RetiredAndNewPlayersArePersisted)
{
  const SlotCleanup slot{uniqueSlot(0)};
  auto controller = makeWorld(slot.slot);
  auto gamedata = controller->getGameData();
  const TeamID team_id = controller->getTeams().front().get().getId();
  Team& team = gamedata->getTeams().at(team_id);

  // A youth player created after generation is inserted on save.
  Player youth(gamedata->allocatePlayerId(), team_id, "New", "Talent",
               PlayerRole::CM, Language::IT, 500, 0, 16, 3, 175, Foot::Right,
               controller->getPlayersForTeam(team_id).front().get().getStats());
  youth.setPotential(80.0f);
  const PlayerID youth_id = youth.getId();
  gamedata->addPlayer(youth_id, youth);
  team.addPlayerID(youth_id);

  // A retired player is deleted on save.
  const PlayerID retired_id =
      controller->getPlayersForTeam(team_id).front().get().getId();
  ASSERT_NE(retired_id, youth_id);
  team.removePlayerID(retired_id);
  team.generateStartingXI(*gamedata, gamedata->getStatsConfig());
  ASSERT_TRUE(gamedata->removePlayer(retired_id));
  controller->saveGame();

  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(slot.slot));
  EXPECT_TRUE(controller->getGameData()->getPlayer(youth_id).has_value());
  EXPECT_FALSE(controller->getGameData()->getPlayer(retired_id).has_value());
  EXPECT_GT(controller->getGameData()->peekNextPlayerId(), youth_id);
}

TEST(WorldPersistenceTest, LegacySavesAreMigrated)
{
  const SlotCleanup slot{uniqueSlot(0)};
  auto controller = makeWorld(slot.slot);
  const TeamID team_id = controller->getTeams().front().get().getId();
  const PlayerID player_id =
      controller->getPlayersForTeam(team_id).front().get().getId();
  controller->saveGame();
  const std::int64_t balance =
      controller->getTeamById(team_id)->get().getFinances().getBalance();
  controller.reset();

  // Strip everything the world simulation added to the schema.
  sqlite3* db = nullptr;
  ASSERT_EQ(sqlite3_open(RuntimePaths::savePath(slot.slot).c_str(), &db),
            SQLITE_OK);
  for (const char* sql :
       {"DROP TABLE FinanceLedger;", "DROP TABLE InboxMessages;",
        "DROP TABLE BoardState;", "DROP TABLE WorldState;",
        "ALTER TABLE Players DROP COLUMN potential;",
        "ALTER TABLE Players DROP COLUMN traits;",
        "ALTER TABLE Players DROP COLUMN dynamics;",
        "ALTER TABLE Teams DROP COLUMN reputation;",
        "ALTER TABLE Teams DROP COLUMN stadium_capacity;",
        "ALTER TABLE Teams DROP COLUMN ticket_price;",
        "ALTER TABLE Teams DROP COLUMN training_facilities;",
        "ALTER TABLE Teams DROP COLUMN youth_facilities;",
        "ALTER TABLE Teams DROP COLUMN transfer_budget;",
        "ALTER TABLE Teams DROP COLUMN wage_budget;",
        "ALTER TABLE Teams DROP COLUMN recent_form;"})
    execSql(db, sql);
  sqlite3_close(db);

  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(slot.slot));
  const Team& team = controller->getTeamById(team_id)->get();
  EXPECT_GT(team.getReputation(), 0);
  EXPECT_GT(team.getStadiumCapacity(), 0u);
  EXPECT_GT(team.getFinances().getWageBudget(), 0);
  EXPECT_EQ(team.getFinances().getBalance(), balance);
  EXPECT_EQ(team.getFinances().ledgerTotal(), balance);
  EXPECT_GT(
      controller->getGameData()->getPlayer(player_id)->get().getPotential(),
      0.0f);
  const std::uint64_t migrated_seed = controller->getWorldSeed();
  controller->advanceDay();
  controller->saveGame();

  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(slot.slot));
  EXPECT_EQ(controller->getWorldSeed(), migrated_seed);
  const Finances& finances =
      controller->getTeamById(team_id)->get().getFinances();
  EXPECT_EQ(finances.getBalance(), finances.ledgerTotal());
}

// ---------------------------------------------------------------------------
// Opt-in timing of a complete headless season (match engine included).
// Run with FM_SEASON_TIMING=1 .local-tools/bin/fm-test -R FullSeasonTiming
// ---------------------------------------------------------------------------

TEST(WorldSimulationTest, FullSeasonTiming)
{
  const SlotCleanup slot{uniqueSlot(0)};
  if (!std::getenv("FM_SEASON_TIMING"))
    GTEST_SKIP() << "set FM_SEASON_TIMING=1 to time a full headless season";
  const auto controller = makeWorld(slot.slot);
  const auto started = std::chrono::steady_clock::now();
  for (int day = 0; day < 365; ++day) controller->advanceDay();
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();
  std::cout << "[season] 365 days, all leagues, AI only: " << seconds << " s\n";
  printLeagueFinances(*controller);
  std::size_t injured = 0;
  for (const auto& [id, player] : controller->getGameData()->getPlayers())
  {
    if (player.getDynamics().last_injury != InjuryType::None) ++injured;
  }
  std::cout << "[season] players injured at least once: " << injured << " of "
            << controller->getGameData()->getPlayers().size() << "\n";
  for (const auto& team : controller->getTeams())
  {
    const Finances& finances = team.get().getFinances();
    EXPECT_EQ(finances.getBalance(), finances.ledgerTotal());
  }
}
