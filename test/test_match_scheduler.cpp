// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/logger.h"
#include "global/runtime_paths.h"
#include "global/thread_pool.h"
#include "model/calendar.h"
#include "model/discipline.h"
#include "model/match.h"
#include "model/match_engine.h"
#include "model/match_scheduler.h"

namespace
{
constexpr std::uint64_t WORLD_SEED = 20250702;
constexpr unsigned PARALLEL_THREADS = 4;

/** Save slot unique to this process (the test binaries run in parallel). */
int uniqueSlot(int offset)
{
  return 300'000 + static_cast<int>(getpid() % 100'000) * 10 + offset;
}

struct SlotCleanup
{
  int slot;
  ~SlotCleanup() { RuntimePaths::removeSave(slot); }
};

std::unique_ptr<GameController> makeWorld(int slot)
{
  Logger::init();
  auto controller = std::make_unique<GameController>();
  controller->newGame(slot, WORLD_SEED);
  return controller;
}

/** Restores an environment variable when the test ends. */
class ScopedEnv
{
 public:
  ScopedEnv(const char* name, const char* value) : name(name)
  {
    if (const char* previous_value = std::getenv(name))
      previous = previous_value;
    if (value)
      setenv(name, value, 1);
    else
      unsetenv(name);
  }
  ~ScopedEnv()
  {
    if (previous)
      setenv(name, previous->c_str(), 1);
    else
      unsetenv(name);
  }
  ScopedEnv(const ScopedEnv&) = delete;
  ScopedEnv& operator=(const ScopedEnv&) = delete;

 private:
  const char* name;
  std::optional<std::string> previous;
};

/** First day with at least @p minimum fixtures (a full league matchday). */
std::optional<GameDateValue> firstBusyDay(const GameController& controller,
                                          std::size_t minimum)
{
  for (const auto& [date, matches] :
       controller.getGame()->getCalendar().getFullCalendar())
  {
    if (matches.size() >= minimum) return date;
  }
  return std::nullopt;
}

void advanceUntil(GameController& controller, const GameDateValue& date)
{
  while (controller.getCurrentDate() < date) controller.advanceDay();
}

std::vector<MatchSimulationInput> prepareDay(const GameController& controller,
                                             const GameDateValue& date)
{
  std::vector<MatchSimulationInput> inputs;
  for (const Match& match :
       controller.getGame()->getCalendar().getMatchesForDate(date))
  {
    if (auto input = match.prepareSimulation(*controller.getGameData()))
      inputs.push_back(std::move(*input));
  }
  return inputs;
}

std::string describe(const MatchSimulationResult& result)
{
  std::string text =
      fmt::format("{}-{} et{} pens{} {} {} {}", result.home_goals,
                  result.away_goals, result.extra_time,
                  result.penalties.has_value(), result.report.eventsToJson(),
                  result.report.playersToJson(), result.report.statsToJson());
  for (const PlayerMatchConsequence& consequence : result.consequences)
    text += fmt::format(" {}:{}:{}:{}", consequence.player_id,
                        consequence.minutes_played, consequence.end_condition,
                        consequence.injured);
  return text;
}

/**
 * One line per standing row, played fixture (with its full report), club and
 * player: everything a matchday changes. Floats print in shortest round-trip
 * form, so equal lines mean bit-identical values.
 */
std::vector<std::string> worldFingerprint(const GameController& controller)
{
  std::vector<std::string> lines;
  for (const League& league : controller.getLeagues())
  {
    for (const StandingRow& row : controller.getStandings(league.getId()))
      lines.push_back(fmt::format(
          "table {} {} pos{} {}/{}/{}/{} {}:{} pts{} {}", league.getId(),
          row.team_id, row.position, row.played, row.won, row.drawn, row.lost,
          row.goals_for, row.goals_against, row.points, row.form));
  }
  for (const auto& [date, matches] :
       controller.getGame()->getCalendar().getFullCalendar())
  {
    for (const Match& match : matches)
    {
      if (!match.isPlayed()) continue;
      std::string line = fmt::format(
          "fixture {} {}-{} {}:{} et{} pens{}:{}", date.toString(),
          match.getHomeTeamId(), match.getAwayTeamId(), match.getHomeScore(),
          match.getAwayScore(), match.wentToExtraTime(),
          match.getHomePenalties(), match.getAwayPenalties());
      if (const auto report = controller.getMatchReport(
              date, match.getHomeTeamId(), match.getAwayTeamId()))
        line += fmt::format(" att{} {} {} {}", report->attendance,
                            report->eventsToJson(), report->playersToJson(),
                            report->statsToJson());
      lines.push_back(std::move(line));
    }
  }
  for (const Team& team : controller.getTeams())
  {
    const Lineup& lineup = team.getLineup();
    std::string line = fmt::format(
        "club {} bal{} ledger{} gk{}", team.getId(),
        team.getFinances().getBalance(), team.getFinances().ledgerTotal(),
        lineup.getGoalkeeper() ? lineup.getGoalkeeper()->getId() : 0);
    for (const auto& slot : lineup.getOutfieldPlayers())
      line += fmt::format(" {}", slot.player ? slot.player->getId() : 0);
    lines.push_back(std::move(line));
  }
  for (const auto& [id, player] : controller.getGameData()->getPlayers())
  {
    const PlayerDynamics& dynamics = player.getDynamics();
    std::string line = fmt::format(
        "player {} team{} cond{} sharp{} morale{} share{} inj{}/{} last{}"
        " apps{} mins{}",
        id, player.getTeamId(), dynamics.condition, dynamics.sharpness,
        dynamics.morale, dynamics.playing_share,
        static_cast<int>(dynamics.injury), dynamics.injury_days,
        dynamics.last_match_day, dynamics.season_appearances,
        dynamics.season_minutes);
    for (const float rating : dynamics.recent_ratings)
      line += fmt::format(" r{}", rating);
    for (const auto& [name, value] : player.getStats())
      line += fmt::format(" {}={}", name, value);
    lines.push_back(std::move(line));
  }
  return lines;
}

void expectSameWorld(const std::vector<std::string>& sequential,
                     const std::vector<std::string>& parallel)
{
  ASSERT_EQ(sequential.size(), parallel.size());
  for (std::size_t i = 0; i < sequential.size(); ++i)
  {
    if (sequential[i] != parallel[i])
    {
      constexpr std::size_t SHOWN = 400;
      FAIL() << "first difference at line " << i
             << "\n sequential: " << sequential[i].substr(0, SHOWN)
             << "\n parallel:   " << parallel[i].substr(0, SHOWN);
    }
  }
}
}  // namespace

// ---------------------------------------------------------------------------
// Thread pool
// ---------------------------------------------------------------------------

TEST(ThreadPoolTest, ParallelForRunsEveryIndexExactlyOnce)
{
  ThreadPool pool(3);
  EXPECT_EQ(pool.getWorkerCount(), 3U);
  constexpr std::size_t COUNT = 2000;
  std::vector<std::atomic<int>> calls(COUNT);
  pool.parallelFor(COUNT, [&](std::size_t index) { ++calls[index]; });
  for (std::size_t i = 0; i < COUNT; ++i) ASSERT_EQ(calls[i].load(), 1) << i;

  pool.parallelFor(0, [](std::size_t) { FAIL() << "no index to run"; });
}

TEST(ThreadPoolTest, RethrowsTheFirstTaskExceptionAfterTheBatch)
{
  ThreadPool pool(2);
  std::atomic<int> finished{0};
  EXPECT_THROW(pool.parallelFor(64,
                                [&](std::size_t index)
                                {
                                  if (index == 10)
                                    throw std::runtime_error("task failed");
                                  ++finished;
                                }),
               std::runtime_error);
  EXPECT_EQ(finished.load(), 63);
}

TEST(ThreadPoolTest, NestedBatchesOnBusyWorkersComplete)
{
  // Every worker blocks inside an outer task; the inner batches must still
  // finish because their callers work through them.
  ThreadPool pool(2);
  std::atomic<int> inner{0};
  pool.parallelFor(4, [&](std::size_t)
                   { pool.parallelFor(16, [&](std::size_t) { ++inner; }); });
  EXPECT_EQ(inner.load(), 64);
}

TEST(ThreadPoolTest, DefaultThreadCountHonoursTheEnvironment)
{
  {
    const ScopedEnv env("FM_SIM_THREADS", "3");
    EXPECT_EQ(ThreadPool::defaultThreadCount(), 3U);
  }
  {
    const ScopedEnv env("FM_SIM_THREADS", "64");
    EXPECT_EQ(ThreadPool::defaultThreadCount(), ThreadPool::MAX_THREADS);
  }
  for (const char* invalid : {"0", "two", "", "-1"})
  {
    const ScopedEnv env("FM_SIM_THREADS", invalid);
    const unsigned threads = ThreadPool::defaultThreadCount();
    EXPECT_GE(threads, 1U) << invalid;
    EXPECT_LE(threads, ThreadPool::MAX_THREADS) << invalid;
  }
}

// ---------------------------------------------------------------------------
// Parallel matchday simulation
// ---------------------------------------------------------------------------

TEST(MatchSchedulerTest, BatchResultsDoNotDependOnTheThreadCount)
{
  const SlotCleanup slot{uniqueSlot(0)};
  const auto controller = makeWorld(slot.slot);
  const auto matchday = firstBusyDay(*controller, 50);
  ASSERT_TRUE(matchday);
  const std::vector<MatchSimulationInput> inputs =
      prepareDay(*controller, *matchday);
  ASSERT_GE(inputs.size(), 50U);

  const StatsConfig& config = controller->getStatsConfig();
  MatchScheduler sequential(1);
  MatchScheduler parallel(PARALLEL_THREADS);
  const auto expected = sequential.run(inputs, config);
  const auto actual = parallel.run(inputs, config);
  ASSERT_EQ(actual.size(), inputs.size());
  for (std::size_t i = 0; i < inputs.size(); ++i)
    ASSERT_EQ(describe(actual[i]), describe(expected[i])) << "match " << i;

  const SimulationProgress progress = parallel.getProgress();
  EXPECT_EQ(progress.completed, inputs.size());
  EXPECT_EQ(progress.total, inputs.size());
  parallel.resetProgress();
  EXPECT_EQ(parallel.getProgress().total, 0U);
}

TEST(MatchSchedulerTest, ParallelCareerMatchesSequentialCareer)
{
  const SlotCleanup sequential_slot{uniqueSlot(1)};
  const SlotCleanup parallel_slot{uniqueSlot(2)};
  const auto sequential = makeWorld(sequential_slot.slot);
  const auto parallel = makeWorld(parallel_slot.slot);
  sequential->setSimulationThreads(1);
  parallel->setSimulationThreads(PARALLEL_THREADS);
  ASSERT_EQ(parallel->getGame()->getSimulationThreads(), PARALLEL_THREADS);

  // One full league matchday first, then eight more weeks (friendlies,
  // league rounds and the first cup round with extra time and penalties).
  const auto matchday = firstBusyDay(*sequential, 50);
  ASSERT_TRUE(matchday);
  advanceUntil(*sequential, *matchday);
  advanceUntil(*parallel, *matchday);
  expectSameWorld(worldFingerprint(*sequential), worldFingerprint(*parallel));

  const GameDateValue end = SeasonCalendar::addDays(*matchday, 56);
  advanceUntil(*sequential, end);
  advanceUntil(*parallel, end);
  expectSameWorld(worldFingerprint(*sequential), worldFingerprint(*parallel));

  std::size_t cup_ties = 0;
  for (const auto& [date, matches] :
       parallel->getGame()->getCalendar().getFullCalendar())
  {
    for (const Match& match : matches)
      if (match.isPlayed() && match.isKnockout()) ++cup_ties;
  }
  EXPECT_GT(cup_ties, 0U);
}

TEST(MatchSchedulerTest, ContinueReportsMatchdayProgress)
{
  const SlotCleanup slot{uniqueSlot(3)};
  const auto controller = makeWorld(slot.slot);
  EXPECT_EQ(controller->getContinueProgress().fraction(), 0.0f);
  const auto matchday = firstBusyDay(*controller, 50);
  ASSERT_TRUE(matchday);
  advanceUntil(*controller, SeasonCalendar::addDays(*matchday, -1));

  controller->advanceDay();
  const GameController::ContinueProgress progress =
      controller->getContinueProgress();
  EXPECT_EQ(progress.days_done, 0);
  EXPECT_EQ(progress.days_total, 1);
  EXPECT_GE(progress.matches_total, 50U);
  EXPECT_EQ(progress.matches_done, progress.matches_total);
  EXPECT_FLOAT_EQ(progress.fraction(), 1.0f);
}

// The scheduler must hand the engine the fixture's real home side: its
// result for a fixture equals a direct engine run with the home line-up
// first, and swapping the venue swaps the sides the engine sees. (Home
// advantage itself is the engine's job and is calibrated there.)
TEST(MatchSchedulerTest, SchedulerKeepsTheFixtureVenue)
{
  const SlotCleanup slot{uniqueSlot(6)};
  const auto controller = makeWorld(slot.slot);
  const auto matchday = firstBusyDay(*controller, 50);
  ASSERT_TRUE(matchday);
  const std::vector<MatchSimulationInput> inputs =
      prepareDay(*controller, *matchday);
  ASSERT_GE(inputs.size(), 8U);
  const StatsConfig& config = controller->getStatsConfig();
  for (std::size_t i = 0; i < 8; ++i)
  {
    const MatchSimulationInput& input = inputs[i];
    const auto team = controller->getGameData()->getTeam(input.home_id);
    ASSERT_TRUE(team);
    EXPECT_EQ(input.home_lineup.getGoalkeeper(),
              team->get().getLineup().getGoalkeeper());

    for (const bool swapped : {false, true})
    {
      MatchSimulationInput venue = input;
      venue.knockout.reset();
      if (swapped)
      {
        std::swap(venue.home_id, venue.away_id);
        std::swap(venue.home_lineup, venue.away_lineup);
        std::swap(venue.home_strategy, venue.away_strategy);
      }
      const MatchSimulationResult result = MatchSimulation::run(venue, config);
      MatchEngine engine(venue.home_lineup, venue.away_lineup,
                         venue.home_strategy, venue.away_strategy, config,
                         venue.seed);
      MatchdaySquad::carryCondition(engine, venue.home_lineup);
      MatchdaySquad::carryCondition(engine, venue.away_lineup);
      while (engine.getState() != MatchState::FULL_TIME) engine.update(0.25f);
      EXPECT_EQ(result.home_goals, engine.getHomeScore()) << "match " << i;
      EXPECT_EQ(result.away_goals, engine.getAwayScore()) << "match " << i;
    }
  }
}

// ---------------------------------------------------------------------------
// Serving bans only visits outstanding bans (it ran after every match)
// ---------------------------------------------------------------------------

TEST(DisciplineBansTest, RestoredBansAreServedOnlyByTheirTeamAndScope)
{
  GameData gamedata;
  for (const TeamID team : {10, 20, 30, 40})
    gamedata.addTeam(team, Team(team, 1, "Club", 0));
  const auto addPlayer = [&](PlayerID id, TeamID team)
  {
    gamedata.addPlayer(
        id, Player(id, team, "Test", "Player", PlayerRole::CM, Language::EN,
                   1000, 0, 25, 2, 180, Foot::Right, {}));
  };
  addPlayer(1, 10);
  addPlayer(2, 10);
  addPlayer(3, 20);

  Discipline discipline;
  discipline.restore({{1, MatchType::LEAGUE, 5, 0, 2},
                      {2, MatchType::LEAGUE, 3, 0, 0},
                      {3, MatchType::CUP, 0, 1, 1}});
  const auto play = [&](MatchType type, TeamID home, TeamID away)
  {
    MatchReport report;
    report.match_type = type;
    report.home_team_id = home;
    report.away_team_id = away;
    discipline.processMatch(report, gamedata);
  };

  play(MatchType::LEAGUE, 30, 40);  // Neither club involved.
  play(MatchType::LEAGUE, 20, 30);  // League match does not serve a cup ban.
  EXPECT_EQ(discipline.banMatches(1, MatchType::LEAGUE), 2);
  EXPECT_EQ(discipline.banMatches(3, MatchType::CUP), 1);
  ASSERT_EQ(discipline.suspendedPlayers(10, gamedata).size(), 1U);

  play(MatchType::LEAGUE, 40, 10);
  play(MatchType::CUP, 20, 40);
  EXPECT_EQ(discipline.banMatches(1, MatchType::LEAGUE), 1);
  EXPECT_FALSE(discipline.isSuspended(3, MatchType::CUP));
  EXPECT_TRUE(discipline.suspendedPlayers(20, gamedata).empty());

  play(MatchType::LEAGUE, 10, 30);
  play(MatchType::LEAGUE, 10, 30);  // Nothing left to serve.
  EXPECT_FALSE(discipline.isSuspended(1, MatchType::LEAGUE));
  EXPECT_TRUE(discipline.suspendedPlayers(10, gamedata).empty());
  EXPECT_EQ(discipline.records().size(), 3U);
  discipline.resetSeason();
  EXPECT_TRUE(discipline.records().empty());
}

// ---------------------------------------------------------------------------
// Opt-in timing. FM_SEASON_TIMING=1 [FM_SEASON_THREADS=n] fm-test -R Timing
// (the test environment pins FM_SIM_THREADS=2; FM_SEASON_THREADS overrides).
// ---------------------------------------------------------------------------

namespace
{
unsigned timingThreads()
{
  if (const char* configured = std::getenv("FM_SEASON_THREADS"))
    return static_cast<unsigned>(std::max(1, std::atoi(configured)));
  return ThreadPool::defaultThreadCount();
}
}  // namespace

TEST(MatchSchedulerTest, MatchdayTiming)
{
  if (!std::getenv("FM_SEASON_TIMING"))
    GTEST_SKIP() << "set FM_SEASON_TIMING=1 to time a matchday";
  const SlotCleanup slot{uniqueSlot(4)};
  const auto controller = makeWorld(slot.slot);
  const auto matchday = firstBusyDay(*controller, 50);
  ASSERT_TRUE(matchday);
  advanceUntil(*controller, SeasonCalendar::addDays(*matchday, -1));
  const std::vector<MatchSimulationInput> inputs =
      prepareDay(*controller, *matchday);

  const auto time = [&](unsigned threads)
  {
    MatchScheduler scheduler(threads);
    const auto started = std::chrono::steady_clock::now();
    const auto results = scheduler.run(inputs, controller->getStatsConfig());
    EXPECT_EQ(results.size(), inputs.size());
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - started)
        .count();
  };
  const double sequential_ms = time(1);
  const unsigned threads = timingThreads();
  const double parallel_ms = time(threads);
  std::cout << "[matchday] " << inputs.size() << " matches: 1 thread "
            << sequential_ms << " ms, " << threads << " threads " << parallel_ms
            << " ms\n";

  controller->setSimulationThreads(threads);
  const auto started = std::chrono::steady_clock::now();
  controller->advanceDay();
  std::cout << "[matchday] full advanceDay with " << threads << " threads: "
            << std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - started)
                   .count()
            << " ms\n";
}

TEST(MatchSchedulerTest, SeasonTiming)
{
  if (!std::getenv("FM_SEASON_TIMING"))
    GTEST_SKIP() << "set FM_SEASON_TIMING=1 to time a full season";
  const SlotCleanup slot{uniqueSlot(5)};
  const auto controller = makeWorld(slot.slot);
  const unsigned threads = timingThreads();
  controller->setSimulationThreads(threads);
  const auto started = std::chrono::steady_clock::now();
  for (int day = 0; day < 365; ++day) controller->advanceDay();
  std::cout << "[season] 365 days with " << threads << " threads: "
            << std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                             started)
                   .count()
            << " s\n";
}
