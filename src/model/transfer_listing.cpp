// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "transfer_listing.h"

TransferListing::TransferListing(PlayerID pid, TeamID seller, uint32_t price,
                                 GameDateValue date, float attention)
    : player_id(pid),
      seller_team_id(seller),
      asking_price(price),
      listing_date(date),
      highest_bidder_id(std::nullopt),
      highest_bid(0),
      attention_score(attention)
{
}
