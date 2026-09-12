// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>

#include "model/transfer_negotiation.h"

/**
 * @namespace PlayerAgent
 * @brief The player's agent in the managed club's purchase and contract
 * talks: what he asks for and what he says each round.
 *
 * His demands are the player's (TransferNegotiation::contractDemand) put
 * as a package: a base wage with a yearly rise, appearance money and a
 * signing-on fee worth his flat ask, the release clause and playing time
 * the player wants, and his own fee. He opens above the standard agent fee
 * and comes down to it by the last round; less than AGENT_FEE_FLOOR of the
 * standard and he blocks the deal. Every answer comes with a short line in
 * his words (a language key taking the player's name). Pure rules.
 */
namespace PlayerAgent
{
using TransferNegotiation::ClubResponse;
using TransferNegotiation::ContractDemand;
using TransferNegotiation::ContractKind;
using TransferNegotiation::ContractOffer;
using TransferNegotiation::ContractResponse;
using TransferNegotiation::PlayerContext;

/** What the agent asks for in the current round. */
struct Demands
{
  std::uint32_t flat_wage = 0;   /*!< His ask as a plain weekly wage. */
  std::uint32_t asking_wage = 0; /*!< Base wage of the package he asks. */
  std::uint8_t yearly_rise = 0;  /*!< Percent a year. */
  std::uint32_t appearance_bonus = 0;
  std::uint32_t signing_bonus = 0;
  std::uint32_t agent_fee = 0;          /*!< His fee as he asks it now. */
  std::uint32_t standard_agent_fee = 0; /*!< What clubs usually pay. */
  bool wants_release_clause = false;
  std::uint32_t max_release_clause = 0;
  SquadRole desired_role{};
  bool wants_promise = false; /*!< Unsure of a place: wants it promised. */
};

/** The usual agent fee: a share of @p transfer_fee, else weeks of wage. */
std::uint32_t standardAgentFee(ContractKind kind, std::uint32_t transfer_fee,
                               std::uint32_t weekly_wage);
/** The least he accepts for himself. */
std::uint32_t lowestAgentFee(std::uint32_t standard);
/** His fee after @p round rejected proposals. */
std::uint32_t agentFeeAsk(const PlayerContext& context, std::uint32_t standard,
                          std::uint8_t round);

/**
 * Demands after @p round rejected proposals, the package priced for a
 * contract of @p years. @p transfer_fee is the agreed fee (0 on a free
 * move).
 */
Demands demands(const PlayerContext& context, std::uint32_t transfer_fee,
                std::uint8_t round, std::uint8_t years);

/** The package he asks for as an offer of @p years (no promise). */
ContractOffer askedOffer(const Demands& demands, std::uint8_t years);

/** The player's and the agent's answer, with the agent's line. */
struct Reply
{
  ContractResponse response;
  const char* line_key = "";
};

/** Answer to @p offer after @p round rejected proposals. */
Reply respond(const PlayerContext& context, const ContractOffer& offer,
              std::uint8_t round, std::uint32_t transfer_fee);

/** What he says as the contract talks open. */
const char* openingLine(const PlayerContext& context);

/**
 * What he says after the selling club's answer to the managed club's offer
 * (empty when the offer never reached the club); @p would_join: his client
 * accepts terms with the managed club.
 */
const char* purchaseLine(const ClubResponse& response, bool would_join,
                         const PlayerContext& context);

}  // namespace PlayerAgent
