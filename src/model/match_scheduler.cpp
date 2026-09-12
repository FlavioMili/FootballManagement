// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/match_scheduler.h"

#include <algorithm>
#include <limits>

#include "model/match.h"
#include "model/match_engine.h"

namespace
{
/** Headless step of the update() loop for engines without simulateToEnd(). */
constexpr float HEADLESS_STEP_SECONDS = 0.25f;

template <typename Engine>
void runToFullTime(Engine& engine)
{
  if constexpr (requires { engine.simulateToEnd(); })
  {
    engine.simulateToEnd();
  }
  else
  {
    while (engine.getState() != MatchState::FULL_TIME)
      engine.update(HEADLESS_STEP_SECONDS);
  }
}

uint8_t toGoals(int goals)
{
  return static_cast<uint8_t>(std::clamp(
      goals, 0, static_cast<int>(std::numeric_limits<uint8_t>::max())));
}
}  // namespace

MatchSimulationResult MatchSimulation::run(const MatchSimulationInput& input,
                                           const StatsConfig& config)
{
  MatchEngine engine(input.home_lineup, input.away_lineup, input.home_strategy,
                     input.away_strategy, config, input.seed);
  MatchdaySquad::carryCondition(engine, input.home_lineup);
  MatchdaySquad::carryCondition(engine, input.away_lineup);
  runToFullTime(engine);

  MatchSimulationResult result;
  result.home_goals = toGoals(engine.getHomeScore());
  result.away_goals = toGoals(engine.getAwayScore());
  if (input.knockout && result.home_goals == result.away_goals)
  {
    const Competitions::KnockoutResolution& resolution = *input.knockout;
    result.home_goals =
        static_cast<uint8_t>(result.home_goals + resolution.home_extra_goals);
    result.away_goals =
        static_cast<uint8_t>(result.away_goals + resolution.away_extra_goals);
    result.extra_time = true;
    if (resolution.penalties)
      result.penalties.emplace(resolution.home_penalties,
                               resolution.away_penalties);
  }

  result.report.fillFromEngine(engine, input.home_id, input.away_id);
  if (result.report.players.empty())
  {
    result.report.addLineupAppearances(input.home_lineup, input.home_id);
    result.report.addLineupAppearances(input.away_lineup, input.away_id);
  }
  result.consequences = MatchdaySquad::consequences(engine);
  return result;
}

MatchScheduler::MatchScheduler(unsigned thread_count)
{
  setThreadCount(thread_count);
}

void MatchScheduler::setThreadCount(unsigned thread_count)
{
  const unsigned clamped =
      std::clamp(thread_count, 1U, ThreadPool::MAX_THREADS);
  if (clamped == threads) return;
  threads = clamped;
  pool.reset();  // Recreated with the new size by the next parallel batch.
}

std::vector<MatchSimulationResult> MatchScheduler::run(
    const std::vector<MatchSimulationInput>& inputs, const StatsConfig& config)
{
  std::vector<MatchSimulationResult> results(inputs.size());
  total.fetch_add(static_cast<uint32_t>(inputs.size()),
                  std::memory_order_relaxed);
  const auto simulate = [&](std::size_t index)
  {
    results[index] = MatchSimulation::run(inputs[index], config);
    completed.fetch_add(1, std::memory_order_relaxed);
  };

  if (threads <= 1 || inputs.size() <= 1)
  {
    for (std::size_t index = 0; index < inputs.size(); ++index) simulate(index);
    return results;
  }
  if (!pool) pool = std::make_unique<ThreadPool>(threads - 1);
  pool->parallelFor(inputs.size(), simulate);
  return results;
}
