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

#include "gui/scenes/management_scene.h"
#include "gui/scenes/transfer_market_scene_tuning.h"
#include "gui/view_models/player_view.h"
#include "model/transfer_negotiation.h"

struct ScoutedPlayerRow;
struct ScoutSearchFilter;

/**
 * @class TransferMarketScene
 * @brief Transfer market inside the management shell.
 *
 * Tabs: Recommended (affordable players who fit a squad need), Search
 * (scouted estimates with filters, affordable players by default), Shortlist,
 * Offers & talks
 * (incoming offers, open negotiations, pre-contracts), Loans, My squad
 * (list, loan-list, release) and History. Dialogs: structured transfer
 * offer, personal terms, loan offer, counter-offer, asking price and
 * release confirmation.
 *
 * Every table is built from cached view models in refresh(); rendering
 * only draws them (clipped), never queries SQL or sorts per frame.
 */
class TransferMarketScene : public ManagementScene
{
 public:
  explicit TransferMarketScene(GUIView* parent);
  /** Opens the market with the deal dialog for @p player already open: a
   * transfer offer, or contract talks for a free agent. */
  TransferMarketScene(GUIView* parent, PlayerID player);
  ~TransferMarketScene() override = default;

  void update(float deltaTime) override;
  [[nodiscard]] SceneID getID() const override;

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override
  {
    return NavSection::TRANSFERS;
  }
  /** Entering the screen re-runs the scouting search; actions only
   * refresh the rows (see refreshData()). */
  void refresh() override
  {
    search_dirty = true;
    refreshData();
    openPendingDeal();
  }

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;

  /** Another club's player or a free agent, from scouting estimates. */
  struct TargetRow
  {
    PlayerID id = 0;
    TeamID team_id = 0;
    std::string name;
    std::string name_lower;
    std::string club;
    std::string role;
    PlayerRole role_id = PlayerRole::UNKNOWN;
    int age = 0;
    float overall = 0.0f; /**< Centre of the estimated range. */
    float overall_low = 0.0f;
    float overall_high = 0.0f;
    bool ranged = false; /**< Too little knowledge for a point value. */
    std::string overall_text;
    std::string potential_text;
    int knowledge = 0;
    TransferNegotiation::SquadFit fit;
    std::string fit_text;
    bool affordable = false; /**< Fee and expected wage within means. */
    int64_t value = 0;
    std::string value_text;
    uint32_t wage = 0;
    std::string wage_text;
    int contract_years = 0;
    uint32_t asking_price = 0; /**< Listing price, 0 if not listed. */
    bool free_agent = false;
    bool loan_listed = false;
    bool pre_contract = false; /**< In the final six months (Jan-Jun). */
    bool shortlisted = false;
  };

  /** A managed player with his market state. */
  struct SquadRow
  {
    PlayerView::PlayerRow player;
    bool loan_listed = false;
    bool borrowed = false;  /**< On loan at the managed club. */
    bool committed = false; /**< Leaving on a pre-contract. */
    uint32_t asking_price = 0;
    std::string asking_text;
  };

  /** An AI offer for a managed player (structured or a listing bid). */
  struct OfferRow
  {
    uint32_t offer_id = 0; /**< 0 = bid on a listed player. */
    PlayerID player_id = 0;
    std::string player;
    std::string club;
    bool loan = false;
    uint32_t fee = 0;
    std::string amount_text;
    std::string terms_text;
    float value_ratio = 0.0f;
    std::string expires_text;
  };

  /** The managed club's open negotiation or agreed pre-contract. */
  struct TalkRow
  {
    PlayerID player_id = 0;
    std::string player;
    std::string club;
    std::string stage;
    std::string fee_text;
    std::string date_text;
    bool can_continue = false;
    bool pre_contract = false;
  };

  struct LoanRow
  {
    PlayerID player_id = 0;
    std::string player;
    std::string club;
    bool incoming = false; /**< Borrowed by the managed club. */
    std::string until_text;
    std::string share_text;
    std::string option_text;
    bool can_recall = false;
    bool can_buy = false;
  };

  struct HistoryRow
  {
    std::string date;
    PlayerID player_id = 0;
    std::string player;
    std::string from;
    std::string to;
    const char* kind_key = "";
    std::string fee_text;
    bool severance = false; /**< Release: wages paid off, not a fee. */
    bool managed = false;
  };

  enum class Availability : uint8_t
  {
    ANY,
    LISTED,
    LOAN_LISTED,
    FREE_AGENTS,
    PRE_CONTRACT,
    COUNT
  };

  enum class ContractFilter : uint8_t
  {
    ANY,
    EXPIRING,
    TWO_YEARS,
    COUNT
  };

  /** Sort column and direction of one target table. */
  struct SortState
  {
    ImGuiID column = 0;
    bool ascending = false;
  };

  struct Filters
  {
    std::array<char, TransferMarketSceneTuning::Filters::SEARCH_BUFFER_SIZE>
        name{};
    int role_index = 0;   /**< 0 = any, else FILTER_ROLES[index - 1]. */
    int league_index = 0; /**< 0 = any, else leagues[index - 1]. */
    int min_age = TransferMarketSceneTuning::Filters::MINIMUM_AGE;
    int max_age = TransferMarketSceneTuning::Filters::DEFAULT_MAXIMUM_AGE;
    int min_overall = 0;
    int max_value_index = 0; /**< 0 = no limit, else MAX_VALUE_STEPS. */
    bool affordable_only = true;
    Availability availability = Availability::ANY;
    ContractFilter contract = ContractFilter::ANY;
  };

  struct OfferDialog
  {
    bool requested = false;
    PlayerID player_id = 0;
    std::string player;
    std::string club;
    int64_t value = 0;
    std::string estimate_text; /**< Estimated ability ("64-72"). */
    int knowledge = 0;
    TransferNegotiation::OfferTerms terms;
    std::optional<TransferNegotiation::ClubResponse> response;
  };

  struct ContractDialog
  {
    bool requested = false;
    PlayerID player_id = 0;
    std::string player;
    TransferNegotiation::ContractKind kind =
        TransferNegotiation::ContractKind::Transfer;
    TransferNegotiation::ContractDemand demand;
    SquadRole projected{};
    TransferNegotiation::ContractOffer offer;
    int promise_index = 0; /**< 0 = no promise. */
    std::optional<TransferNegotiation::ContractResponse> response;
    bool over_budget = false;
    int rounds_left = 0;
  };

  struct LoanDialog
  {
    bool requested = false;
    PlayerID player_id = 0;
    std::string player;
    std::string club;
    TransferNegotiation::LoanTerms terms;
    std::optional<TransferNegotiation::ClubResponse> response;
  };

  struct CounterDialog
  {
    bool requested = false;
    OfferRow offer;
    uint32_t fee = 0;
    std::optional<TransferNegotiation::ClubResponse> response;
  };

  struct ListingDialog
  {
    bool requested = false;
    PlayerID player_id = 0;
    std::string player;
    uint32_t price = 0;
  };

  struct ReleaseDialog
  {
    bool requested = false;
    PlayerID player_id = 0;
    std::string body;
  };

  void refreshData();
  void openPendingDeal();
  void refreshNeeds();
  void refreshTargets();
  void refreshRecommended();
  void refreshTargetFlags();
  /** Re-derives cached rows after an action (drops players who left). */
  void rederiveRows(std::vector<TargetRow>& rows) const;
  /** Highest estimated value the budget can reach (0 = nothing). */
  int64_t affordableValueCap() const;
  /** Scouted rows for @p search: capped to the budget when affordable,
   * plus every free agent (no fee) the filters allow. */
  std::vector<TargetRow> searchRows(ScoutSearchFilter search,
                                    bool affordable) const;
  void refreshShortlist();
  void refreshSquad();
  void refreshOffers();
  void refreshLoans();
  void refreshHistory();
  TargetRow makeTargetRow(const ScoutedPlayerRow& scouted) const;
  void applyTargetFilter();
  void sortTargets(const std::vector<TargetRow>& rows,
                   std::vector<size_t>& visible, const SortState& sort) const;

  void renderSummary();
  void renderSearchTab();
  void renderRecommendedTab();
  void renderFilters();
  void renderTargetTable(const char* id, const std::vector<TargetRow>& rows,
                         std::vector<size_t>& visible, SortState& sort);
  void renderTargetActions(const TargetRow& row);
  void renderOffersTab();
  void renderLoansTab();
  void renderSquadTab();
  void renderHistoryTab();

  void openOfferDialog(const TargetRow& row);
  void openContractDialog(PlayerID player_id, const std::string& name);
  void openLoanDialog(const TargetRow& row);
  void renderOfferDialog();
  void renderContractDialog();
  void renderLoanDialog();
  void renderCounterDialog();
  void renderListingDialog();
  void renderReleaseDialog();
  void renderReasons(const std::vector<TransferNegotiation::Reason>& reasons,
                     const ImVec4& color) const;
  float dialogWidth() const;

  Filters filters;
  Filters applied_filters;
  bool search_dirty = true;
  std::vector<LeagueID> league_ids;
  std::vector<std::string> league_names;

  TransferNegotiation::SquadNeeds needs;
  std::string needs_text;
  std::vector<TargetRow> targets;
  std::vector<size_t> visible_targets;
  std::vector<TargetRow> recommended;
  std::vector<size_t> visible_recommended;
  std::vector<TargetRow> shortlist;
  std::vector<size_t> visible_shortlist;
  std::vector<SquadRow> squad;
  std::vector<OfferRow> offers;
  std::vector<TalkRow> talks;
  std::vector<LoanRow> loans;
  std::vector<HistoryRow> history;
  bool history_managed_only = false;
  std::vector<size_t> visible_history;

  SortState search_sort;
  SortState shortlist_sort;
  SortState recommended_sort;

  int64_t available_budget = 0;
  bool embargo = false; /**< Board has frozen transfer spending. */
  int64_t wage_room = 0;
  TransferNegotiation::WindowInfo window;
  int loans_in = 0;
  int loans_out = 0;

  OfferDialog offer_dialog;
  ContractDialog contract_dialog;
  LoanDialog loan_dialog;
  CounterDialog counter_dialog;
  ListingDialog listing_dialog;
  ReleaseDialog release_dialog;
  /** Player whose deal dialog opens on entry (0 = none). */
  PlayerID pending_deal_player = 0;
};
