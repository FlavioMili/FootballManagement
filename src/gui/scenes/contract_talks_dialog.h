// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "controller/game_controller.h"
#include "model/player_agent.h"
#include "model/transfer_negotiation.h"

/**
 * @brief Modal contract talks with a player and his agent: a transfer
 * target whose fee is agreed, a free agent, a pre-contract or the renewal
 * of a managed player's contract.
 *
 * Self-contained like OfferNegotiationDialog: the transfer market and the
 * player profile each keep one, call open() and render() every frame. The
 * club proposes the wage, the contract's end, a signing-on fee, a yearly
 * rise, appearance money, a release clause, the agent's fee and a
 * playing-time promise; the agent's demands, his line for every round and
 * the budget impact are shown beside. Text is built on open and after each
 * proposal, never per frame.
 */
class ContractTalksDialog
{
 public:
  /** Opens talks with @p player_id; false when no talks are possible. */
  bool open(GameController& controller, PlayerID player_id);

  enum class Event : std::uint8_t
  {
    None,
    Proposed, /*!< A proposal was answered (refresh views). */
    Signed    /*!< He signed: signedMessage() says what. */
  };
  Event render(GameController& controller);

  [[nodiscard]] bool isOpen() const { return visible; }
  [[nodiscard]] const std::string& signedMessage() const
  {
    return signed_message;
  }

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;

  struct Line
  {
    std::string label;
    std::string value;
  };

  void refreshDemands(GameController& controller);
  void refreshYearLabels(const GameController& controller);
  void refreshSummary();
  void propose(GameController& controller);
  void renderTerms();
  void renderSummary() const;

  bool open_requested = false;
  bool visible = false;
  PlayerID player_id = 0;
  std::string player;
  TransferNegotiation::ContractKind kind =
      TransferNegotiation::ContractKind::Transfer;
  std::uint8_t current_years = 0; /**< Renewal: seasons left now. */
  std::uint8_t first_year = 1;    /**< Shortest contract on offer. */
  std::uint8_t last_year = 1;     /**< Longest contract allowed. */
  int first_season_end = 0;       /**< Year of the current season's end. */
  std::int64_t wage_room = 0;     /**< Before this contract. */
  std::int64_t budget = 0;        /**< Transfer money for fees. */

  TransferNegotiation::ContractDemand demand;
  PlayerAgent::Demands agent;
  SquadRole projected{};
  TransferNegotiation::ContractOffer offer;
  std::int64_t agent_fee = 0;
  int promise_index = 0; /**< 0 = no promise. */
  int rise_index = 0;
  std::optional<TransferNegotiation::ContractResponse> response;
  GameController::PlayerActionBlock block =
      GameController::PlayerActionBlock::None;
  bool over_budget = false;
  int rounds_left = 0;
  bool signed_up = false;

  // Built by refreshDemands() / refreshSummary().
  std::string title;
  std::string kind_text;
  std::vector<Line> demand_lines;
  std::vector<std::string> agent_lines; /**< Oldest first. */
  std::vector<std::string> year_texts;
  std::vector<const char*> year_labels;
  std::string yearly_text;
  std::string total_text;
  std::string rounds_text;
  std::string signed_message;
};
