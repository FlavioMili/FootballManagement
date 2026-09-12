// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "global/stats_config.h"
#include "global/thread_pool.h"
#include "global/types.h"
#include "model/competition.h"
#include "model/lineup.h"
#include "model/match_report.h"
#include "model/strategy.h"
#include "model/world_simulation.h"

/**
 * Everything a headless match needs, captured on the thread that owns the
 * game state. Simulating it only reads the Player objects the lineups point
 * to (attributes and persistent condition), which nothing modifies while a
 * batch runs, so inputs with distinct players can run concurrently.
 */
struct MatchSimulationInput
{
  TeamID home_id = 0;
  TeamID away_id = 0;
  uint32_t seed = 0;
  Lineup home_lineup;
  Lineup away_lineup;
  Strategy home_strategy;
  Strategy away_strategy;
  /**
   * Extra time and shootout of a knockout tie, used if it ends level (on
   * aggregate, for the second leg of a two-legged tie).
   */
  std::optional<Competitions::KnockoutResolution> knockout;
  /** Aggregate lead of the home side from the first leg (0 otherwise). */
  int knockout_lead = 0;
};

/** Final score, engine summary and physical outcome of one match. */
struct MatchSimulationResult
{
  uint8_t home_goals = 0;
  uint8_t away_goals = 0;
  bool extra_time = false;
  std::optional<std::pair<uint8_t, uint8_t>> penalties;
  MatchReport report;
  std::vector<PlayerMatchConsequence> consequences;
};

namespace MatchSimulation
{
/** Runs one match to full time. Thread-safe for distinct inputs. */
MatchSimulationResult run(const MatchSimulationInput& input,
                          const StatsConfig& config);
}  // namespace MatchSimulation

/** Matches simulated and scheduled since the last progress reset. */
struct SimulationProgress
{
  uint32_t completed = 0;
  uint32_t total = 0;
};

/**
 * @class MatchScheduler
 * @brief Simulates a batch of independent matches in parallel.
 *
 * Results come back in input order and each match is fully determined by its
 * input, so a batch gives identical results for any thread count.
 */
class MatchScheduler
{
 public:
  explicit MatchScheduler(
      unsigned thread_count = ThreadPool::defaultThreadCount());

  /** Threads per batch including the caller; 1 simulates on the caller. */
  void setThreadCount(unsigned thread_count);
  unsigned getThreadCount() const { return threads; }

  /** Simulates every input; result i belongs to inputs[i]. */
  std::vector<MatchSimulationResult> run(
      const std::vector<MatchSimulationInput>& inputs,
      const StatsConfig& config);

  /** Starts counting afresh (e.g. at the start of a day). */
  void resetProgress()
  {
    completed.store(0, std::memory_order_relaxed);
    total.store(0, std::memory_order_relaxed);
  }
  /** Safe to call from any thread while a batch runs. */
  SimulationProgress getProgress() const
  {
    return {completed.load(std::memory_order_relaxed),
            total.load(std::memory_order_relaxed)};
  }

 private:
  unsigned threads = 1;
  std::unique_ptr<ThreadPool> pool;
  std::atomic<uint32_t> completed{0};
  std::atomic<uint32_t> total{0};
};
