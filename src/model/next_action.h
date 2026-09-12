// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "global/types.h"
#include "model/match_analysis.h"

class GameController;

/** @brief Kinds of pending work the next-best-action engine knows. */
enum class NextActionKind : std::uint8_t
{
  UnavailableInLineup,
  IncomingOffer,
  PendingTalk,
  ContractExpiring,
  ScoutReportA,
  CongestedWeek,
  BoardWarning,
  WindowSquadHole,
  OppositionReport
};

/** @brief Where an action is taken care of. */
enum class ActionTarget : std::uint8_t
{
  Lineup,
  Inbox,
  Transfers,
  Scouting,
  Training,
  Club,
  Player,
  Opposition
};

/** @brief One ranked piece of pending work with the reason it matters. */
struct NextAction
{
  NextActionKind kind = NextActionKind::UnavailableInLineup;
  ActionTarget target = ActionTarget::Lineup;
  /** Player or offer the action is about (0 when none). */
  std::uint32_t ref = 0;
  /** 0-100; higher first. */
  int priority = 0;
  AnalysisLine title;
  AnalysisLine reason;
};

/** @brief The facts the ranking is based on (gathered once per refresh). */
struct NextActionFacts
{
  struct Offer
  {
    std::uint32_t id = 0;
    PlayerID player = 0;
    std::string player_name;
    std::string buyer;
    std::string fee; /*!< Formatted money. */
    bool loan = false;
    int days_left = 0;
  };
  struct Person
  {
    PlayerID player = 0;
    std::string name;
  };
  struct Report
  {
    PlayerID player = 0;
    std::string name;
    std::string scout;
  };

  /** Days until the next managed fixture (-1 without one). */
  int days_to_match = -1;
  std::string next_opponent;
  std::vector<std::string> unavailable_selected;
  /** Nobody fit can replace them: they play through it, nothing to fix. */
  bool no_fit_replacements = false;
  bool assistant_fixes_lineup = false;
  std::vector<Offer> offers;
  std::vector<Person> pending_talks;
  /** Key and first-team players in the final year of their contract. */
  std::vector<Person> expiring_contracts;
  /** From 1 January other clubs may agree pre-contracts with them. */
  bool pre_contract_period = false;
  std::vector<Report> unseen_grade_a;
  /** Match days in the next seven days (today included). */
  int matches_next_week = 0;
  int tired_players = 0; /*!< Fit players below 75 condition. */
  float average_condition = 100.0f;
  bool assistant_runs_training = false;
  float board_confidence = 60.0f;
  bool window_open = false;
  int window_days_left = -1;
  /** Positions without enough senior cover (display names). */
  std::vector<std::string> squad_holes;
  /** The opposition report of the next match was opened. */
  bool opposition_viewed = false;
};

namespace NextActionRules
{
inline constexpr int TIRED_CONDITION = 75;
inline constexpr int CONGESTED_TIRED_PLAYERS = 4;
inline constexpr float CONGESTED_AVERAGE = 82.0f;
inline constexpr float BOARD_WARNING_CONFIDENCE = 35.0f;
inline constexpr int WINDOW_WARNING_DAYS = 14;
inline constexpr int OPPOSITION_DAYS = 2;
/** Offers listed individually before they are folded into one action. */
inline constexpr std::size_t MAX_OFFER_ACTIONS = 2;
}  // namespace NextActionRules

/** Ranks the pending work, most important first (pure, deterministic). */
std::vector<NextAction> rankNextActions(
    const NextActionFacts& facts,
    std::size_t limit = std::numeric_limits<std::size_t>::max());

/** Reads the facts of the managed club from the controller. */
NextActionFacts gatherNextActionFacts(const GameController& controller);
