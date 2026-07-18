// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <optional>

#include "global/types.h"
#include "model/gamedate.h"

/**
 * @struct TransferListing
 * @brief Represents a player listed on the transfer market.
 *
 * Tracks the asking price, listing date, and the current highest bid (if any).
 * Owned by GameController as part of the transfer_listings map.
 */
struct TransferListing
{
  PlayerID player_id;          // The player being sold
  TeamID seller_team_id;       // The team selling the player
  uint32_t asking_price;       // The price the seller wants (in currency units)
  GameDateValue listing_date;  // When the player was first listed

  // Bid state — set when another team makes a bid
  std::optional<TeamID> highest_bidder_id;  // Team that made the highest bid
                                            // (std::nullopt if none)
  uint32_t highest_bid = 0;  // Amount of the highest bid (0 if none)

  // Cached for performance
  float attention_score = 0.0f;  // Calculated when listed, dictates AI interest

  /**
   * @brief Constructs a new TransferListing with bid state cleared.
   * @param pid Player ID.
   * @param seller ID of the selling team.
   * @param price Asking price in currency units.
   * @param date Date the listing was created.
   * @param attention The pre-calculated attention score.
   */
  TransferListing(PlayerID pid, TeamID seller, uint32_t price,
                  GameDateValue date, float attention = 0.0f);

  /** @brief Default constructor for map/serialization use. */
  TransferListing() = default;
};
