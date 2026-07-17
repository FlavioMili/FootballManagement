// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "gui/gui_scene.h"
#include "model/player.h"
#include "model/transfer_listing.h"

/**
 * @class TransferMarketScene
 * @brief Full-screen ImGui scene for the transfer market.
 *
 * Layout: Top bar (budget + window status), tab bar, content area.
 * Tabs: Buy Players, Your Listings, Incoming Bids.
 * Dialogs: Confirmation (buy/sign), Counter-offer.
 */
class TransferMarketScene : public GUIScene
{
 public:
  explicit TransferMarketScene(GUIView* parent);
  ~TransferMarketScene() override = default;

  void onEnter() override;
  void update(float deltaTime) override;
  void render() override;
  [[nodiscard]] SceneID getID() const override;

 private:
  enum class Tab : uint8_t
  {
    BUY,
    LISTINGS,
    BIDS
  };

  Tab active_tab = Tab::BUY;

  // Filters for Buy tab
  int filter_role_index = 0;
  int filter_max_age = 40;
  float filter_max_price = 100000000.0f;

  // Confirm transfer dialog
  struct ConfirmState
  {
    bool open = false;
    PlayerID player_id = 0;
    TeamID seller_id = 0;
    uint32_t price = 0;
    bool is_free_agent = false;
  };
  ConfirmState confirm_state;

  // Counter-offer dialog
  struct CounterState
  {
    bool open = false;
    PlayerID player_id = 0;
  };
  CounterState counter_state;

  // Cached data
  std::vector<const TransferListing*> cached_listings;
  std::vector<std::pair<PlayerID, TransferListing>> cached_bids;
  std::vector<std::reference_wrapper<const Player>> cached_free_agents;

  // Listing input cache
  int list_player_index = 0;
  float list_price_input = 0.0f;
  float counter_price_input = 0.0f;

  void refreshData();
  void renderBuyTab();
  void renderListingsTab();
  void renderBidsTab();
  void renderConfirmDialog();
  void renderCounterDialog();
};
