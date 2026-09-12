// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/transfer_market_scene.h"

#include <fmt/format.h>
#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <utility>
#include <array>
#include <cmath>
#include <limits>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/game.h"
#include "model/role_utils.h"
#include "model/scouting.h"
#include "model/transfer_market.h"
#include "model/transfer_tuning.h"

namespace
{
using TransferNegotiation::ClubResponse;
using TransferNegotiation::ContractKind;
using Tuning = TransferMarketSceneTuning;

enum class TargetColumn : ImGuiID
{
  SHORTLIST = 1,
  NAME,
  CLUB,
  ROLE,
  AGE,
  OVERALL,
  POTENTIAL,
  VALUE,
  WAGE,
  CONTRACT,
  ACTION
};

constexpr std::array<PlayerRole, 12> FILTER_ROLES = {
    PlayerRole::GK,  PlayerRole::CB, PlayerRole::LB,  PlayerRole::RB,
    PlayerRole::CDM, PlayerRole::CM, PlayerRole::CAM, PlayerRole::LM,
    PlayerRole::RM,  PlayerRole::LW, PlayerRole::RW,  PlayerRole::ST};

constexpr std::array<const char*, 5> AVAILABILITY_KEYS = {
    "TRANSFER_AVAIL_ANY", "TRANSFER_AVAIL_LISTED", "TRANSFER_AVAIL_LOAN",
    "TRANSFER_AVAIL_FREE", "TRANSFER_AVAIL_PRE_CONTRACT"};

constexpr std::array<const char*, 3> CONTRACT_KEYS = {
    "TRANSFER_CONTRACT_ANY", "TRANSFER_CONTRACT_EXPIRING",
    "TRANSFER_CONTRACT_TWO_YEARS"};

constexpr std::array<SquadRole, 5> PROMISE_ROLES = {
    SquadRole::KeyPlayer, SquadRole::FirstTeam, SquadRole::Rotation,
    SquadRole::Backup, SquadRole::Fringe};

float scale() { return Theme::scale(); }

/** Currency field with steps, followed by the amount in compact form. */
bool moneyInput(const char* label, uint32_t& value, uint32_t step,
                uint32_t fast)
{
  const bool changed =
      ImGui::InputScalar(label, ImGuiDataType_U32, &value, &step, &fast, "%u");
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(Theme::palette().muted, "%s",
                     Format::money(value).c_str());
  return changed;
}

bool countInput(const char* label, uint16_t& value)
{
  const uint16_t step = Tuning::MoneyInput::TARGET_STEP;
  const uint16_t fast = Tuning::MoneyInput::TARGET_LARGE_STEP;
  return ImGui::InputScalar(label, ImGuiDataType_U16, &value, &step, &fast,
                            "%u");
}

/** Label on the left, widget on the right, one row per field. */
void fieldLabel(const char* text)
{
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(Theme::palette().muted, "%s", text);
  ImGui::SameLine(Tuning::Layout::LABEL_WIDTH * scale());
  ImGui::SetNextItemWidth(Tuning::Layout::INPUT_WIDTH * scale());
}

const char* decisionKey(ClubResponse::Decision decision)
{
  switch (decision)
  {
    case ClubResponse::Decision::Accept:
      return "TRANSFER_RESPONSE_ACCEPTED";
    case ClubResponse::Decision::Counter:
      return "TRANSFER_RESPONSE_COUNTER";
    case ClubResponse::Decision::Reject:
      return "TRANSFER_RESPONSE_REJECTED";
  }
  return "";
}

ImVec4 decisionColor(ClubResponse::Decision decision)
{
  const Theme::Palette& palette = Theme::palette();
  switch (decision)
  {
    case ClubResponse::Decision::Accept:
      return palette.positive;
    case ClubResponse::Decision::Counter:
      return palette.warning;
    case ClubResponse::Decision::Reject:
      return palette.negative;
  }
  return palette.text;
}

std::string teamName(const GameController& controller, TeamID team_id)
{
  if (team_id == FREE_AGENTS_TEAM_ID) return LOC("TRANSFER_FREE_AGENT_LABEL");
  const auto team = controller.getTeamById(team_id);
  return team ? team->get().getName() : std::string();
}

std::string playerName(const GameData& data, PlayerID player_id)
{
  const auto player = data.getPlayer(player_id);
  return player ? player->get().getName()
                : std::string(LOC("TRANSFER_RETIRED"));
}

std::string loanTermsText(const TransferNegotiation::LoanTerms& terms)
{
  std::string text = fmt::sprintf(
      LOC("TRANSFER_LOAN_TERMS"), static_cast<int>(terms.wage_share),
      LOC(terms.duration == TransferNegotiation::LoanDuration::SeasonEnd
              ? "TRANSFER_LOAN_SEASON"
              : "TRANSFER_LOAN_SIX_MONTHS"));
  if (terms.loan_fee > 0)
    text += fmt::sprintf(LOC("TRANSFER_LOAN_FEE_SUFFIX"),
                         Format::money(terms.loan_fee));
  return text;
}

std::string offerTermsText(const TransferNegotiation::OfferTerms& terms)
{
  if (terms.instalment_years == 0) return LOC("TRANSFER_TERMS_CASH");
  return fmt::sprintf(LOC("TRANSFER_TERMS_INSTALMENTS"),
                      static_cast<int>(terms.upfront_percent),
                      static_cast<int>(terms.instalment_years));
}
}  // namespace

TransferMarketScene::TransferMarketScene(GUIView* parent)
    : ManagementScene(parent)
{
}

TransferMarketScene::TransferMarketScene(GUIView* parent, PlayerID player)
    : ManagementScene(parent), pending_deal_player(player)
{
}

void TransferMarketScene::openPendingDeal()
{
  if (pending_deal_player == 0) return;
  const PlayerID player_id = std::exchange(pending_deal_player, 0);
  const auto scouted = guiView->getController().getScoutedRow(player_id);
  if (!scouted) return;
  const TargetRow row = makeTargetRow(*scouted);
  if (row.free_agent)
    openContractDialog(row.id, row.name);
  else
    openOfferDialog(row);
}

void TransferMarketScene::update(float /*deltaTime*/) {}

SceneID TransferMarketScene::getID() const { return SceneID::TRANSFER_MARKET; }

// ---------------------------------------------------------------------------
// View models
// ---------------------------------------------------------------------------

void TransferMarketScene::refreshData()
{
  const auto& controller = guiView->getController();
  const Game* game = controller.getGame();
  if (!game || !controller.getManagedTeam()) return;
  const TeamID managed = game->getManagedTeamId();

  window = controller.getTransferWindow();
  available_budget = controller.transferBudgetForTeam(managed);
  wage_room = controller.getManagedTeam()->get().getFinances().getWageBudget() -
              controller.getWeeklyWageBill(managed);

  league_ids.clear();
  league_names.clear();
  for (const auto& league : controller.getLeagues())
  {
    league_ids.push_back(league.get().getId());
    league_names.push_back(league.get().getName());
  }

  if (search_dirty)
    refreshTargets();
  else
    refreshTargetFlags();
  refreshShortlist();
  refreshSquad();
  refreshOffers();
  refreshLoans();
  refreshHistory();
}

TransferMarketScene::TargetRow TransferMarketScene::makeTargetRow(
    const ScoutedPlayerRow& scouted) const
{
  const auto& controller = guiView->getController();
  const auto data = controller.getGameData();
  const TransferMarket& market = controller.getGame()->getTransfers();
  TargetRow row;
  row.id = scouted.player_id;
  row.team_id = scouted.team_id;
  row.name = playerName(*data, row.id);
  row.name_lower = PlayerView::toLower(row.name);
  row.club = teamName(controller, row.team_id);
  row.role_id = scouted.role;
  row.role = RoleUtils::toString(scouted.role);
  row.age = scouted.age;
  row.overall = scouted.overall;
  row.potential_text = fmt::format("{:.0f}-{:.0f}", scouted.potential_low,
                                   scouted.potential_high);
  row.knowledge = scouted.knowledge;
  row.value = scouted.estimated_value;
  row.value_text = Format::money(row.value);
  row.wage = scouted.wage;
  row.wage_text = Format::money(row.wage);
  row.contract_years = scouted.contract_years;
  row.free_agent = row.team_id == FREE_AGENTS_TEAM_ID;
  if (const auto listing = controller.getAllListings().find(row.id);
      listing != controller.getAllListings().end() &&
      listing->second.seller_team_id == row.team_id)
    row.asking_price = listing->second.asking_price;
  row.loan_listed = market.isLoanListed(row.id);
  row.pre_contract = !row.free_agent && market.canBeTraded(row.id) &&
                     TransferNegotiation::canSignPreContract(
                         static_cast<uint8_t>(row.contract_years),
                         controller.getCurrentDate());
  row.shortlisted = scouted.shortlisted;
  return row;
}

void TransferMarketScene::refreshTargets()
{
  if (!search_dirty) return;
  search_dirty = false;
  applied_filters = filters;
  const auto& controller = guiView->getController();
  ScoutSearchFilter search;
  if (filters.role_index > 0)
    search.role = FILTER_ROLES[static_cast<size_t>(filters.role_index - 1)];
  search.min_age = static_cast<uint8_t>(filters.min_age);
  search.max_age = static_cast<uint8_t>(filters.max_age);
  search.min_overall = static_cast<float>(filters.min_overall);
  search.max_value = filters.max_value;
  if (filters.league_index > 0 &&
      static_cast<size_t>(filters.league_index) <= league_ids.size())
    search.league_id =
        league_ids[static_cast<size_t>(filters.league_index - 1)];
  search.free_agents_only = filters.availability == Availability::FREE_AGENTS;
  search.limit = Tuning::Filters::RESULT_LIMIT;

  targets.clear();
  for (const ScoutedPlayerRow& scouted :
       controller.searchScoutedPlayers(search))
    targets.push_back(makeTargetRow(scouted));
  applyTargetFilter();
}

void TransferMarketScene::refreshTargetFlags()
{
  // Cheap after an action: re-derive the cached rows (club, listing, loan
  // list, shortlist) without estimating every player again.
  const auto& controller = guiView->getController();
  const TeamID managed = controller.getGame()->getManagedTeamId();
  std::vector<TargetRow> updated;
  updated.reserve(targets.size());
  for (const TargetRow& row : targets)
  {
    const auto scouted = controller.getScoutedRow(row.id);
    if (scouted && scouted->team_id != managed)
      updated.push_back(makeTargetRow(*scouted));
  }
  targets = std::move(updated);
  applyTargetFilter();
}

void TransferMarketScene::applyTargetFilter()
{
  const std::string query = PlayerView::toLower(filters.name.data());
  applied_filters.name = filters.name;
  visible_targets.clear();
  for (size_t index = 0; index < targets.size(); ++index)
  {
    const TargetRow& row = targets[index];
    if (!query.empty() && !row.name_lower.contains(query)) continue;
    switch (filters.availability)
    {
      case Availability::LISTED:
        if (row.asking_price == 0) continue;
        break;
      case Availability::LOAN_LISTED:
        if (!row.loan_listed) continue;
        break;
      case Availability::PRE_CONTRACT:
        if (!row.pre_contract) continue;
        break;
      default:
        break;
    }
    if (filters.contract == ContractFilter::EXPIRING && row.contract_years > 1)
      continue;
    if (filters.contract == ContractFilter::TWO_YEARS && row.contract_years > 2)
      continue;
    visible_targets.push_back(index);
  }
  sortTargets(targets, visible_targets, search_sort);
}

void TransferMarketScene::sortTargets(const std::vector<TargetRow>& rows,
                                      std::vector<size_t>& visible,
                                      const SortState& sort) const
{
  const auto column = static_cast<TargetColumn>(sort.column);
  std::ranges::sort(visible,
                    [&](size_t left, size_t right)
                    {
                      const TargetRow& a = rows[left];
                      const TargetRow& b = rows[right];
                      int cmp = 0;
                      switch (column)
                      {
                        case TargetColumn::SHORTLIST:
                          cmp = UI::compare(a.shortlisted, b.shortlisted);
                          break;
                        case TargetColumn::NAME:
                          cmp = a.name.compare(b.name);
                          break;
                        case TargetColumn::CLUB:
                          cmp = a.club.compare(b.club);
                          break;
                        case TargetColumn::ROLE:
                          cmp = UI::compare(a.role_id, b.role_id);
                          break;
                        case TargetColumn::AGE:
                          cmp = UI::compare(a.age, b.age);
                          break;
                        case TargetColumn::POTENTIAL:
                        case TargetColumn::OVERALL:
                        case TargetColumn::ACTION:
                          cmp = UI::compare(a.overall, b.overall);
                          break;
                        case TargetColumn::VALUE:
                          cmp = UI::compare(a.value, b.value);
                          break;
                        case TargetColumn::WAGE:
                          cmp = UI::compare(a.wage, b.wage);
                          break;
                        case TargetColumn::CONTRACT:
                          cmp = UI::compare(a.contract_years, b.contract_years);
                          break;
                      }
                      if (cmp == 0) cmp = UI::compare(a.id, b.id);
                      return sort.ascending ? cmp < 0 : cmp > 0;
                    });
}

void TransferMarketScene::refreshShortlist()
{
  const auto& controller = guiView->getController();
  shortlist.clear();
  for (const ShortlistEntry& entry : controller.getShortlist())
  {
    if (const auto scouted = controller.getScoutedRow(entry.player_id))
      shortlist.push_back(makeTargetRow(*scouted));
  }
  visible_shortlist.resize(shortlist.size());
  for (size_t index = 0; index < shortlist.size(); ++index)
    visible_shortlist[index] = index;
  sortTargets(shortlist, visible_shortlist, shortlist_sort);
}

void TransferMarketScene::refreshSquad()
{
  const auto& controller = guiView->getController();
  const TransferMarket& market = controller.getGame()->getTransfers();
  const TeamID managed = controller.getGame()->getManagedTeamId();
  squad.clear();
  for (const auto& reference : controller.getPlayersForTeam(managed))
  {
    SquadRow row;
    row.player = PlayerView::makeRow(controller, reference.get());
    row.loan_listed = market.isLoanListed(row.player.id);
    row.borrowed = market.findLoan(row.player.id) != nullptr;
    row.committed = market.findPreContract(row.player.id) != nullptr;
    if (const auto listing = controller.getAllListings().find(row.player.id);
        listing != controller.getAllListings().end())
    {
      row.asking_price = listing->second.asking_price;
      row.asking_text = Format::money(row.asking_price);
    }
    squad.push_back(std::move(row));
  }
  std::ranges::sort(squad, [](const SquadRow& a, const SquadRow& b)
                    { return a.player.overall > b.player.overall; });
}

void TransferMarketScene::refreshOffers()
{
  const auto& controller = guiView->getController();
  const auto data = controller.getGameData();
  const TransferMarket& market = controller.getGame()->getTransfers();
  const TeamID managed = controller.getGame()->getManagedTeamId();
  offers.clear();
  for (const IncomingOffer& incoming : controller.getIncomingOffers())
  {
    OfferRow row;
    row.offer_id = incoming.id;
    row.player_id = incoming.player_id;
    row.player = playerName(*data, incoming.player_id);
    row.club = teamName(controller, incoming.buyer);
    row.loan = incoming.loan;
    row.fee = incoming.terms.fee;
    row.amount_text = incoming.loan ? LOC("TRANSFER_KIND_LOAN")
                                    : Format::money(incoming.terms.fee);
    row.terms_text = incoming.loan ? loanTermsText(incoming.loan_terms)
                                   : offerTermsText(incoming.terms);
    const uint32_t value = controller.getPlayerMarketValue(incoming.player_id);
    row.value_ratio = !incoming.loan && value > 0
                          ? static_cast<float>(incoming.terms.fee) /
                                static_cast<float>(value) *
                                Tuning::PERCENT_SCALE
                          : 0.0f;
    row.expires_text = Format::dayMonth(incoming.expires);
    offers.push_back(std::move(row));
  }
  for (const auto& [player_id, listing] : controller.getIncomingBids())
  {
    OfferRow row;
    row.player_id = player_id;
    row.player = playerName(*data, player_id);
    row.club = teamName(controller, *listing.highest_bidder_id);
    row.fee = listing.highest_bid;
    row.amount_text = Format::money(listing.highest_bid);
    row.terms_text = fmt::sprintf(LOC("TRANSFER_TERMS_LISTED"),
                                  Format::money(listing.asking_price));
    const uint32_t value = controller.getPlayerMarketValue(player_id);
    row.value_ratio = value > 0 ? static_cast<float>(listing.highest_bid) /
                                      static_cast<float>(value) *
                                      Tuning::PERCENT_SCALE
                                : 0.0f;
    offers.push_back(std::move(row));
  }

  talks.clear();
  for (const auto& [player_id, talk] : market.talks())
  {
    TalkRow row;
    row.player_id = player_id;
    row.player = playerName(*data, player_id);
    row.club = teamName(controller, talk.seller);
    row.stage = LOC(talk.club_agreed ? "TRANSFER_STAGE_FEE_AGREED"
                                     : "TRANSFER_STAGE_TALKING");
    row.fee_text =
        talk.club_agreed ? Format::money(talk.agreed.fee) : std::string("-");
    row.date_text = Format::dayMonth(talk.expires);
    row.can_continue = controller.getContractTalkKind(player_id).has_value();
    talks.push_back(std::move(row));
  }
  for (const auto& [player_id, deal] : market.preContracts())
  {
    if (deal.to_team != managed && deal.from_team != managed) continue;
    TalkRow row;
    row.player_id = player_id;
    row.player = playerName(*data, player_id);
    row.club = teamName(
        controller, deal.to_team == managed ? deal.from_team : deal.to_team);
    row.stage = LOC(deal.to_team == managed ? "TRANSFER_STAGE_PRE_IN"
                                            : "TRANSFER_STAGE_PRE_OUT");
    row.fee_text = Format::money(deal.terms.weekly_wage);
    row.date_text = Format::date(deal.agreed);
    row.pre_contract = true;
    talks.push_back(std::move(row));
  }
  std::ranges::sort(talks, {}, &TalkRow::player);
}

void TransferMarketScene::refreshLoans()
{
  const auto& controller = guiView->getController();
  const auto data = controller.getGameData();
  const TransferMarket& market = controller.getGame()->getTransfers();
  const TeamID managed = controller.getGame()->getManagedTeamId();
  loans.clear();
  loans_in = 0;
  loans_out = 0;
  for (const auto& [player_id, loan] : market.loans())
  {
    if (loan.parent != managed && loan.borrower != managed) continue;
    LoanRow row;
    row.player_id = player_id;
    row.player = playerName(*data, player_id);
    row.incoming = loan.borrower == managed;
    row.club = teamName(controller, row.incoming ? loan.parent : loan.borrower);
    row.until_text = Format::date(loan.end);
    row.share_text = fmt::format("{}%", static_cast<int>(loan.wage_share));
    row.option_text =
        loan.option_fee == 0
            ? std::string("-")
            : fmt::sprintf(LOC(loan.obligation ? "TRANSFER_LOAN_OBLIGATION"
                                               : "TRANSFER_LOAN_OPTION"),
                           Format::money(loan.option_fee));
    row.can_recall = !row.incoming && loan.recall_clause && window.open;
    row.can_buy = row.incoming && loan.option_fee > 0;
    (row.incoming ? loans_in : loans_out) += 1;
    loans.push_back(std::move(row));
  }
  std::ranges::sort(loans, {}, &LoanRow::player);
}

void TransferMarketScene::refreshHistory()
{
  const auto& controller = guiView->getController();
  const auto data = controller.getGameData();
  const TransferMarket& market = controller.getGame()->getTransfers();
  const TeamID managed = controller.getGame()->getManagedTeamId();
  const auto& records = market.history();
  history.clear();
  const size_t count = std::min(records.size(), Tuning::Tables::HISTORY_LIMIT);
  history.reserve(count);
  for (size_t index = 0; index < count; ++index)
  {
    const TransferRecord& record = records[records.size() - 1 - index];
    HistoryRow row;
    row.date = Format::date(record.date);
    row.player_id = record.player_id;
    row.player = playerName(*data, record.player_id);
    row.from = teamName(controller, record.from_team);
    row.to = teamName(controller, record.to_team);
    row.kind_key = transferKindKey(record.kind);
    row.fee_text = record.fee > 0 ? Format::money(record.fee) : "-";
    row.managed = record.from_team == managed || record.to_team == managed;
    history.push_back(std::move(row));
  }
  visible_history.clear();
  for (size_t index = 0; index < history.size(); ++index)
    if (!history_managed_only || history[index].managed)
      visible_history.push_back(index);
}

// ---------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------

void TransferMarketScene::renderContent()
{
  UI::pageHeader(LOC("TRANSFER_TITLE"), LOC("TRANSFER_SUBTITLE"));
  renderSummary();

  if (ImGui::BeginTabBar("TransferMarketTabs"))
  {
    if (ImGui::BeginTabItem(LOC("TRANSFER_TAB_SEARCH")))
    {
      renderSearchTab();
      ImGui::EndTabItem();
    }
    const std::string shortlistLabel = fmt::format(
        "{} ({})###shortlist", LOC("TRANSFER_TAB_SHORTLIST"), shortlist.size());
    if (ImGui::BeginTabItem(shortlistLabel.c_str()))
    {
      if (shortlist.empty())
        UI::emptyState(LOC("TRANSFER_SHORTLIST_EMPTY_TITLE"),
                       LOC("TRANSFER_SHORTLIST_EMPTY_BODY"));
      else
        renderTargetTable("ShortlistTable", shortlist, visible_shortlist,
                          shortlist_sort);
      ImGui::EndTabItem();
    }
    const std::string offersLabel =
        offers.empty() ? fmt::format("{}###offers", LOC("TRANSFER_TAB_OFFERS"))
                       : fmt::format("{} ({})###offers",
                                     LOC("TRANSFER_TAB_OFFERS"), offers.size());
    if (ImGui::BeginTabItem(offersLabel.c_str()))
    {
      renderOffersTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(LOC("TRANSFER_TAB_LOANS")))
    {
      renderLoansTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(LOC("TRANSFER_TAB_SQUAD")))
    {
      renderSquadTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(LOC("TRANSFER_TAB_HISTORY")))
    {
      renderHistoryTab();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  renderOfferDialog();
  renderContractDialog();
  renderLoanDialog();
  renderCounterDialog();
  renderListingDialog();
  renderReleaseDialog();
}

void TransferMarketScene::renderSummary()
{
  const Theme::Palette& palette = Theme::palette();
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float available = ImGui::GetContentRegionAvail().x;
  const int perRow =
      std::clamp(static_cast<int>(
                     (available + gap) /
                     (Tuning::Layout::SUMMARY_TILE_MIN_WIDTH * scale() + gap)),
                 1, Tuning::Layout::SUMMARY_TILES);
  const float width = (available - gap * static_cast<float>(perRow - 1)) /
                      static_cast<float>(perRow);
  int tile = 0;
  const auto next = [&]()
  {
    if (++tile % perRow != 0) ImGui::SameLine();
  };

  const std::string budget = Format::money(available_budget);
  UI::statTile("budget", LOC("TRANSFER_TILE_BUDGET"), budget.c_str(),
               LOC("TRANSFER_TILE_BUDGET_NOTE"), palette.accent, width);
  next();
  const std::string wages = Format::money(wage_room);
  UI::statTile("wages", LOC("TRANSFER_TILE_WAGE_ROOM"), wages.c_str(),
               LOC("TRANSFER_TILE_WAGE_ROOM_NOTE"),
               wage_room >= 0 ? palette.text : palette.negative, width);
  next();
  const std::string windowText =
      !window.open ? std::string(LOC("TRANSFER_WINDOW_CLOSED_SHORT"))
      : window.days_to_deadline == 0
          ? std::string(LOC("TRANSFER_DEADLINE_DAY"))
          : fmt::sprintf(LOC("TRANSFER_DAYS_LEFT"), window.days_to_deadline);
  UI::statTile("window", LOC("TRANSFER_WINDOW"), windowText.c_str(),
               LOC(window.open ? (window.winter ? "TRANSFER_WINDOW_WINTER"
                                                : "TRANSFER_WINDOW_SUMMER")
                               : "TRANSFER_WINDOW_NOTE"),
               window.open ? (window.days_to_deadline <=
                                      TransferTuning::Market::LATE_WINDOW_DAYS
                                  ? palette.warning
                                  : palette.positive)
                           : palette.negative,
               width);
  next();
  const std::string offerCount = std::to_string(offers.size());
  UI::statTile("offers", LOC("TRANSFER_TILE_OFFERS"), offerCount.c_str(),
               LOC("TRANSFER_BIDS_NOTE"),
               offers.empty() ? palette.text : palette.warning, width);
  next();
  const std::string loanCount = fmt::format("{} / {}", loans_in, loans_out);
  UI::statTile("loans", LOC("TRANSFER_TILE_LOANS"), loanCount.c_str(),
               LOC("TRANSFER_TILE_LOANS_NOTE"), palette.text, width);
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * scale()));
}

// ---------------------------------------------------------------------------
// Search and shortlist
// ---------------------------------------------------------------------------

void TransferMarketScene::renderSearchTab()
{
  renderFilters();
  refreshTargets();
  if (visible_targets.empty())
  {
    UI::emptyState(LOC("TRANSFER_EMPTY_TITLE"), LOC("TRANSFER_EMPTY_BODY"));
    return;
  }
  renderTargetTable("TargetTable", targets, visible_targets, search_sort);
}

void TransferMarketScene::renderFilters()
{
  using L = Tuning::Layout;
  bool changed = false;
  ImGui::SetNextItemWidth(L::SEARCH_WIDTH * scale());
  if (ImGui::InputTextWithHint("##market_search", LOC("ROSTER_SEARCH_HINT"),
                               filters.name.data(), filters.name.size()))
    applyTargetFilter();

  UI::sameLineIfFits(L::COMBO_WIDTH * scale());
  ImGui::SetNextItemWidth(L::COMBO_WIDTH * scale());
  const std::string rolePreview =
      filters.role_index == 0
          ? std::string(LOC("TRANSFER_FILTER_ANY_POSITION"))
          : RoleUtils::toString(
                FILTER_ROLES[static_cast<size_t>(filters.role_index - 1)]);
  if (ImGui::BeginCombo("##market_role", rolePreview.c_str()))
  {
    if (ImGui::Selectable(LOC("TRANSFER_FILTER_ANY_POSITION"),
                          filters.role_index == 0))
    {
      filters.role_index = 0;
      changed = true;
    }
    for (size_t index = 0; index < FILTER_ROLES.size(); ++index)
    {
      const std::string role = RoleUtils::toString(FILTER_ROLES[index]);
      if (ImGui::Selectable(role.c_str(),
                            filters.role_index == static_cast<int>(index + 1)))
      {
        filters.role_index = static_cast<int>(index + 1);
        changed = true;
      }
    }
    ImGui::EndCombo();
  }

  UI::sameLineIfFits(L::COMBO_WIDTH * scale());
  ImGui::SetNextItemWidth(L::COMBO_WIDTH * scale());
  const char* leaguePreview =
      filters.league_index == 0
          ? LOC("TRANSFER_FILTER_ANY_LEAGUE")
          : league_names[static_cast<size_t>(filters.league_index - 1)].c_str();
  if (ImGui::BeginCombo("##market_league", leaguePreview))
  {
    if (ImGui::Selectable(LOC("TRANSFER_FILTER_ANY_LEAGUE"),
                          filters.league_index == 0))
    {
      filters.league_index = 0;
      changed = true;
    }
    for (size_t index = 0; index < league_names.size(); ++index)
    {
      ImGui::PushID(static_cast<int>(index));
      if (ImGui::Selectable(
              league_names[index].c_str(),
              filters.league_index == static_cast<int>(index + 1)))
      {
        filters.league_index = static_cast<int>(index + 1);
        changed = true;
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }

  UI::sameLineIfFits(L::COMBO_WIDTH * scale());
  ImGui::SetNextItemWidth(L::COMBO_WIDTH * scale());
  if (ImGui::BeginCombo(
          "##market_availability",
          LOC(AVAILABILITY_KEYS[static_cast<size_t>(filters.availability)])))
  {
    for (size_t index = 0; index < AVAILABILITY_KEYS.size(); ++index)
    {
      const auto option = static_cast<Availability>(index);
      if (ImGui::Selectable(LOC(AVAILABILITY_KEYS[index]),
                            filters.availability == option))
      {
        // Free agents are a search scope; the others filter the results.
        changed |= (filters.availability == Availability::FREE_AGENTS) !=
                   (option == Availability::FREE_AGENTS);
        filters.availability = option;
        applyTargetFilter();
      }
    }
    ImGui::EndCombo();
  }

  UI::sameLineIfFits(L::COMBO_WIDTH * scale());
  ImGui::SetNextItemWidth(L::COMBO_WIDTH * scale());
  if (ImGui::BeginCombo(
          "##market_contract",
          LOC(CONTRACT_KEYS[static_cast<size_t>(filters.contract)])))
  {
    for (size_t index = 0; index < CONTRACT_KEYS.size(); ++index)
    {
      const auto option = static_cast<ContractFilter>(index);
      if (ImGui::Selectable(LOC(CONTRACT_KEYS[index]),
                            filters.contract == option))
      {
        filters.contract = option;
        applyTargetFilter();
      }
    }
    ImGui::EndCombo();
  }

  // Sliders search when released, not on every drag step.
  UI::sameLineIfFits(L::SLIDER_WIDTH * scale());
  ImGui::SetNextItemWidth(L::SLIDER_WIDTH * scale());
  const std::string ageFormat =
      fmt::format("{} %d", LOC("TRANSFER_FILTER_MIN_AGE"));
  ImGui::SliderInt("##market_min_age", &filters.min_age,
                   Tuning::Filters::MINIMUM_AGE, filters.max_age,
                   ageFormat.c_str());
  changed |= ImGui::IsItemDeactivatedAfterEdit();
  UI::sameLineIfFits(L::SLIDER_WIDTH * scale());
  ImGui::SetNextItemWidth(L::SLIDER_WIDTH * scale());
  const std::string maxAgeFormat =
      fmt::format("{} %d", LOC("TRANSFER_FILTER_AGE"));
  ImGui::SliderInt("##market_max_age", &filters.max_age, filters.min_age,
                   Tuning::Filters::MAXIMUM_AGE, maxAgeFormat.c_str());
  changed |= ImGui::IsItemDeactivatedAfterEdit();
  UI::sameLineIfFits(L::SLIDER_WIDTH * scale());
  ImGui::SetNextItemWidth(L::SLIDER_WIDTH * scale());
  const std::string overallFormat =
      fmt::format("{} %d", LOC("TRANSFER_FILTER_MIN_OVERALL"));
  ImGui::SliderInt("##market_min_overall", &filters.min_overall, 0,
                   Tuning::Filters::MAXIMUM_OVERALL, overallFormat.c_str());
  changed |= ImGui::IsItemDeactivatedAfterEdit();
  UI::sameLineIfFits(L::INPUT_WIDTH * scale());
  ImGui::SetNextItemWidth(L::INPUT_WIDTH * scale());
  const std::string valueHint = LOC("TRANSFER_FILTER_MAX_VALUE");
  int64_t maxValue = filters.max_value;
  const int64_t step = Tuning::MoneyInput::LARGE_STEP;
  const int64_t fast = 10 * step;
  if (ImGui::InputScalar("##market_max_value", ImGuiDataType_S64, &maxValue,
                         &step, &fast,
                         maxValue > 0 ? "%lld" : valueHint.c_str()))
    filters.max_value = std::max<int64_t>(0, maxValue);
  changed |= ImGui::IsItemDeactivatedAfterEdit();
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", LOC("TRANSFER_FILTER_MAX_VALUE_TIP"));

  UI::sameLineIfFits(UI::buttonWidth(LOC("TRANSFER_FILTER_RESET")));
  if (ImGui::Button(LOC("TRANSFER_FILTER_RESET")))
  {
    filters = Filters{};
    changed = true;
  }
  UI::sameLineIfFits(120.0f * scale());
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(
      Theme::palette().muted, "%s",
      fmt::sprintf(LOC("TRANSFER_RESULTS"), visible_targets.size()).c_str());
  if (changed) search_dirty = true;
}

void TransferMarketScene::renderTargetTable(const char* id,
                                            const std::vector<TargetRow>& rows,
                                            std::vector<size_t>& visible,
                                            SortState& sort)
{
  const Theme::Palette& palette = Theme::palette();
  auto& controller = guiView->getController();
  const ImGuiTableFlags flags =
      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Hideable;
  if (!UI::beginDataTable(id, Tuning::Tables::TARGET_COLUMN_COUNT, flags,
                          Tuning::Layout::TARGET_TABLE_MIN_WIDTH * scale(),
                          ImVec2(0.0f, ImGui::GetContentRegionAvail().y), 2))
    return;
  const auto column = [](const char* key, TargetColumn columnId,
                         ImGuiTableColumnFlags extra = 0)
  {
    ImGui::TableSetupColumn(LOC(key), extra, 0.0f,
                            static_cast<ImGuiID>(columnId));
  };
  column("TRANSFER_COL_SHORTLIST", TargetColumn::SHORTLIST,
         ImGuiTableColumnFlags_NoHide);
  column("TRANSFER_COL_NAME", TargetColumn::NAME,
         ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide);
  column("TRANSFER_COL_TEAM", TargetColumn::CLUB);
  column("TRANSFER_COL_ROLE", TargetColumn::ROLE);
  column("TRANSFER_COL_AGE", TargetColumn::AGE);
  column("TRANSFER_COL_OVR", TargetColumn::OVERALL,
         ImGuiTableColumnFlags_PreferSortDescending |
             ImGuiTableColumnFlags_DefaultSort);
  column("TRANSFER_COL_POTENTIAL", TargetColumn::POTENTIAL,
         ImGuiTableColumnFlags_NoSort);
  column("TRANSFER_COL_VALUE", TargetColumn::VALUE,
         ImGuiTableColumnFlags_PreferSortDescending);
  column("TRANSFER_COL_WAGE", TargetColumn::WAGE);
  column("TRANSFER_COL_CONTRACT", TargetColumn::CONTRACT);
  column("TRANSFER_COL_ACTION", TargetColumn::ACTION,
         ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoHide);
  ImGui::TableHeadersRow();

  // The table keeps its sort state across scene instances: follow it.
  if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
      specs && specs->SpecsCount > 0)
  {
    const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
    const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
    if (specs->SpecsDirty || spec.ColumnUserID != sort.column ||
        ascending != sort.ascending)
    {
      sort.column = spec.ColumnUserID;
      sort.ascending = ascending;
      sortTargets(rows, visible, sort);
    }
    specs->SpecsDirty = false;
  }

  bool shortlist_changed = false;
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(visible.size()));
  while (clipper.Step())
  {
    for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line)
    {
      const TargetRow& row = rows[visible[static_cast<size_t>(line)]];
      ImGui::TableNextRow();
      ImGui::PushID(static_cast<int>(row.id));
      ImGui::TableNextColumn();
      bool shortlisted = row.shortlisted;
      // Compact box so rows keep the height of a text line.
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
      const bool toggled = ImGui::Checkbox("##shortlist", &shortlisted);
      ImGui::PopStyleVar();
      if (toggled)
      {
        if (shortlisted)
          controller.addToShortlist(row.id);
        else
          controller.removeFromShortlist(row.id);
        shortlist_changed = true;
      }
      ImGui::TableNextColumn();
      if (UI::link(row.name.c_str(), "name"))
        Navigation::openPlayer(guiView, row.id);
      if (row.asking_price > 0)
      {
        ImGui::SameLine();
        UI::badge(LOC("TRANSFER_BADGE_LISTED"), palette.info);
      }
      if (row.loan_listed)
      {
        ImGui::SameLine();
        UI::badge(LOC("TRANSFER_BADGE_LOAN"), palette.accent);
      }
      if (row.pre_contract)
      {
        ImGui::SameLine();
        UI::badge(LOC("TRANSFER_BADGE_EXPIRING"), palette.warning);
      }
      ImGui::TableNextColumn();
      ImGui::TextColored(row.free_agent ? palette.faint : palette.muted, "%s",
                         row.club.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.role.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.age);
      ImGui::TableNextColumn();
      UI::ratingChip(row.overall);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "%s",
            fmt::sprintf(LOC("TRANSFER_KNOWLEDGE_TIP"), row.knowledge).c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%s", row.potential_text.c_str());
      ImGui::TableNextColumn();
      UI::textRight(row.value_text.c_str());
      ImGui::TableNextColumn();
      UI::textRight(row.wage_text.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.contract_years);
      ImGui::TableNextColumn();
      renderTargetActions(row);
      ImGui::PopID();
    }
  }
  ImGui::EndTable();
  if (shortlist_changed)
  {
    for (TargetRow& row : targets)
      row.shortlisted = controller.isShortlisted(row.id);
    refreshShortlist();
  }
}

void TransferMarketScene::renderTargetActions(const TargetRow& row)
{
  const auto& controller = guiView->getController();
  if (ImGui::SmallButton(LOC("TRANSFER_ACTIONS"))) ImGui::OpenPopup("actions");
  if (!ImGui::BeginPopup("actions")) return;
  const auto talk = controller.getContractTalkKind(row.id);
  if (row.free_agent)
  {
    if (ImGui::Selectable(LOC("TRANSFER_ACTION_SIGN")))
      openContractDialog(row.id, row.name);
  }
  else
  {
    if (ImGui::Selectable(LOC("TRANSFER_ACTION_OFFER"), false,
                          window.open ? 0 : ImGuiSelectableFlags_Disabled))
      openOfferDialog(row);
    if (ImGui::Selectable(LOC("TRANSFER_ACTION_LOAN"), false,
                          window.open ? 0 : ImGuiSelectableFlags_Disabled))
      openLoanDialog(row);
    const bool agreed = talk == ContractKind::Transfer;
    if (agreed && ImGui::Selectable(LOC("TRANSFER_ACTION_TERMS")))
      openContractDialog(row.id, row.name);
    if (talk == ContractKind::PreContract &&
        ImGui::Selectable(LOC("TRANSFER_ACTION_PRE_CONTRACT")))
      openContractDialog(row.id, row.name);
    if (!window.open)
      ImGui::TextColored(Theme::palette().faint, "%s",
                         LOC("TRANSFER_WINDOW_CLOSED_HINT"));
  }
  ImGui::Separator();
  if (ImGui::Selectable(LOC("TRANSFER_ACTION_PROFILE")))
    Navigation::openPlayer(guiView, row.id);
  ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// Offers, loans, squad and history
// ---------------------------------------------------------------------------

void TransferMarketScene::renderOffersTab()
{
  const Theme::Palette& palette = Theme::palette();
  auto& controller = guiView->getController();
  UI::sectionLabel(LOC("TRANSFER_INCOMING_OFFERS"));
  bool changed = false;
  if (offers.empty())
  {
    ImGui::TextColored(palette.muted, "%s", LOC("TRANSFER_NO_BIDS"));
  }
  else if (UI::beginDataTable(
               "IncomingOffers", Tuning::Tables::OFFER_COLUMN_COUNT,
               ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                   ImGuiTableFlags_SizingFixedFit,
               Tuning::Layout::WIDE_TABLE_MIN_WIDTH * scale(), ImVec2(0, 0)))
  {
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_NAME"),
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_BIDDER"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_AMOUNT"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_TERMS"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_VS_VALUE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_EXPIRES"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_ACTION"));
    ImGui::TableHeadersRow();
    for (const OfferRow& row : offers)
    {
      ImGui::TableNextRow();
      ImGui::PushID(static_cast<int>(row.offer_id));
      ImGui::PushID(static_cast<int>(row.player_id));
      ImGui::TableNextColumn();
      if (UI::link(row.player.c_str(), "name"))
        Navigation::openPlayer(guiView, row.player_id);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.club.c_str());
      ImGui::TableNextColumn();
      UI::textRight(row.amount_text.c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%s", row.terms_text.c_str());
      ImGui::TableNextColumn();
      if (!row.loan)
        ImGui::TextColored(row.value_ratio >= Tuning::PERCENT_SCALE
                               ? palette.positive
                               : palette.warning,
                           "%.0f%%", static_cast<double>(row.value_ratio));
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.expires_text.c_str());
      ImGui::TableNextColumn();
      ImGui::BeginDisabled(!window.open);
      if (UI::primaryButton(LOC("TRANSFER_ACCEPT")))
      {
        const bool done = row.offer_id != 0
                              ? controller.acceptIncomingOffer(row.offer_id)
                              : controller.acceptBid(row.player_id);
        showToast(done ? fmt::sprintf(LOC("TRANSFER_SOLD_TOAST"), row.player)
                       : std::string(LOC("TRANSFER_DEAL_FAILED")),
                  !done);
        changed = true;
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button(LOC("TRANSFER_REJECT")))
      {
        if (row.offer_id != 0)
          controller.rejectIncomingOffer(row.offer_id);
        else
          controller.rejectBid(row.player_id);
        changed = true;
      }
      if (!row.loan)
      {
        ImGui::SameLine();
        if (ImGui::Button(LOC("TRANSFER_COUNTER")))
          counter_dialog = {true, row, row.fee, std::nullopt};
      }
      ImGui::PopID();
      ImGui::PopID();
      if (changed) break;
    }
    ImGui::EndTable();
  }
  if (changed)
  {
    refreshData();
    return;
  }

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::M * scale()));
  UI::sectionLabel(LOC("TRANSFER_YOUR_TALKS"));
  if (talks.empty())
  {
    ImGui::TextColored(palette.muted, "%s", LOC("TRANSFER_NO_TALKS"));
    return;
  }
  if (!UI::beginDataTable(
          "Talks", Tuning::Tables::TALK_COLUMN_COUNT,
          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY,
          Tuning::Layout::WIDE_TABLE_MIN_WIDTH * scale(),
          ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
    return;
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_NAME"),
                          ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_TEAM"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_STAGE"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_AGREED"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_DATE"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_ACTION"));
  ImGui::TableHeadersRow();
  for (const TalkRow& row : talks)
  {
    ImGui::TableNextRow();
    ImGui::PushID(static_cast<int>(row.player_id));
    ImGui::TableNextColumn();
    if (UI::link(row.player.c_str(), "name"))
      Navigation::openPlayer(guiView, row.player_id);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.club.c_str());
    ImGui::TableNextColumn();
    ImGui::TextColored(row.pre_contract ? palette.info : palette.muted, "%s",
                       row.stage.c_str());
    ImGui::TableNextColumn();
    UI::textRight(row.fee_text.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.date_text.c_str());
    ImGui::TableNextColumn();
    if (row.can_continue && UI::primaryButton(LOC("TRANSFER_ACTION_TERMS")))
      openContractDialog(row.player_id, row.player);
    ImGui::PopID();
  }
  ImGui::EndTable();
}

void TransferMarketScene::renderLoansTab()
{
  const Theme::Palette& palette = Theme::palette();
  auto& controller = guiView->getController();
  if (loans.empty())
  {
    UI::emptyState(LOC("TRANSFER_NO_LOANS_TITLE"),
                   LOC("TRANSFER_NO_LOANS_BODY"));
    return;
  }
  if (!UI::beginDataTable(
          "LoansTable", Tuning::Tables::LOAN_COLUMN_COUNT,
          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY,
          Tuning::Layout::WIDE_TABLE_MIN_WIDTH * scale(),
          ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
    return;
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_NAME"),
                          ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_DIRECTION"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_TEAM"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_UNTIL"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_WAGE_SHARE"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_OPTION"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_ACTION"));
  ImGui::TableHeadersRow();
  bool changed = false;
  for (const LoanRow& row : loans)
  {
    ImGui::TableNextRow();
    ImGui::PushID(static_cast<int>(row.player_id));
    ImGui::TableNextColumn();
    if (UI::link(row.player.c_str(), "name"))
      Navigation::openPlayer(guiView, row.player_id);
    ImGui::TableNextColumn();
    UI::badge(LOC(row.incoming ? "TRANSFER_LOAN_IN" : "TRANSFER_LOAN_OUT"),
              row.incoming ? palette.positive : palette.info);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.club.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.until_text.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.share_text.c_str());
    ImGui::TableNextColumn();
    ImGui::TextColored(palette.muted, "%s", row.option_text.c_str());
    ImGui::TableNextColumn();
    if (row.can_recall && ImGui::Button(LOC("TRANSFER_RECALL")))
    {
      const bool done = controller.recallLoan(row.player_id);
      showToast(LOC(done ? "TRANSFER_RECALLED_TOAST" : "TRANSFER_DEAL_FAILED"),
                !done);
      changed = true;
    }
    if (row.can_buy && UI::primaryButton(LOC("TRANSFER_EXERCISE_OPTION")))
    {
      const bool done = controller.exerciseLoanOption(row.player_id);
      showToast(done ? fmt::sprintf(LOC("TRANSFER_COMPLETED_TOAST"), row.player)
                     : std::string(LOC("TRANSFER_NOT_ENOUGH_BUDGET")),
                !done);
      changed = true;
    }
    ImGui::PopID();
    if (changed) break;
  }
  ImGui::EndTable();
  if (changed) refreshData();
}

void TransferMarketScene::renderSquadTab()
{
  const Theme::Palette& palette = Theme::palette();
  auto& controller = guiView->getController();
  ImGui::TextColored(palette.muted, "%s", LOC("TRANSFER_SQUAD_HINT"));
  if (!UI::beginDataTable(
          "SquadMarket", Tuning::Tables::SQUAD_COLUMN_COUNT,
          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY,
          Tuning::Layout::WIDE_TABLE_MIN_WIDTH * scale(),
          ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
    return;
  ImGui::TableSetupScrollFreeze(1, 1);
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_NAME"),
                          ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_ROLE"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_AGE"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_OVR"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_VALUE"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_WAGE"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_CONTRACT"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_STATUS"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_ACTION"));
  ImGui::TableHeadersRow();
  bool changed = false;
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(squad.size()));
  while (clipper.Step() && !changed)
  {
    for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line)
    {
      const SquadRow& row = squad[static_cast<size_t>(line)];
      const PlayerView::PlayerRow& player = row.player;
      ImGui::TableNextRow();
      ImGui::PushID(static_cast<int>(player.id));
      ImGui::TableNextColumn();
      if (UI::link(player.name.c_str(), "name"))
        Navigation::openPlayer(guiView, player.id);
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%s", player.role.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", player.age);
      ImGui::TableNextColumn();
      UI::ratingChip(player.overall);
      ImGui::TableNextColumn();
      UI::textRight(player.value_text.c_str());
      ImGui::TableNextColumn();
      UI::textRight(player.wage_text.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", player.contract_years);
      ImGui::TableNextColumn();
      if (row.borrowed)
        UI::badge(LOC("TRANSFER_BADGE_BORROWED"), palette.positive);
      else if (row.committed)
        UI::badge(LOC("TRANSFER_BADGE_LEAVING"), palette.negative);
      if (row.asking_price > 0)
      {
        UI::badge(row.asking_text.c_str(), palette.info);
        ImGui::SameLine();
      }
      if (row.loan_listed)
        UI::badge(LOC("TRANSFER_BADGE_LOAN"), palette.accent);
      ImGui::TableNextColumn();
      const bool tradable = !row.borrowed && !row.committed;
      ImGui::BeginDisabled(!tradable);
      if (row.asking_price > 0)
      {
        if (ImGui::SmallButton(LOC("TRANSFER_UNLIST")))
        {
          controller.removePlayerFromTransfer(player.id);
          showToast(LOC("PROFILE_UNLISTED_TOAST"));
          changed = true;
        }
      }
      else
      {
        ImGui::BeginDisabled(!window.open);
        if (ImGui::SmallButton(LOC("TRANSFER_LIST_SHORT")))
          listing_dialog = {true, player.id, player.name, player.market_value};
        ImGui::EndDisabled();
      }
      ImGui::SameLine();
      if (ImGui::SmallButton(LOC(row.loan_listed ? "TRANSFER_UNLIST_LOAN"
                                                 : "TRANSFER_LIST_LOAN")))
      {
        controller.setLoanListed(player.id, !row.loan_listed);
        changed = true;
      }
      ImGui::SameLine();
      if (ImGui::SmallButton(LOC("TRANSFER_RELEASE")))
      {
        release_dialog.requested = true;
        release_dialog.player_id = player.id;
        release_dialog.body = fmt::sprintf(
            LOC("TRANSFER_RELEASE_BODY"), player.name,
            Format::moneyFull(controller.getReleaseCost(player.id)));
      }
      ImGui::EndDisabled();
      ImGui::PopID();
      if (changed) break;
    }
  }
  ImGui::EndTable();
  if (changed) refreshData();
}

void TransferMarketScene::renderHistoryTab()
{
  const Theme::Palette& palette = Theme::palette();
  if (ImGui::Checkbox(LOC("TRANSFER_HISTORY_MY_CLUB"), &history_managed_only))
  {
    visible_history.clear();
    for (size_t index = 0; index < history.size(); ++index)
      if (!history_managed_only || history[index].managed)
        visible_history.push_back(index);
  }
  if (visible_history.empty())
  {
    UI::emptyState(LOC("TRANSFER_HISTORY_EMPTY_TITLE"),
                   LOC("TRANSFER_HISTORY_EMPTY_BODY"));
    return;
  }
  if (!UI::beginDataTable(
          "HistoryTable", Tuning::Tables::HISTORY_COLUMN_COUNT,
          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY,
          Tuning::Layout::WIDE_TABLE_MIN_WIDTH * scale(),
          ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
    return;
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_DATE"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_NAME"),
                          ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_FROM"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_TO"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_TYPE"));
  ImGui::TableSetupColumn(LOC("TRANSFER_COL_FEE"));
  ImGui::TableHeadersRow();
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(visible_history.size()));
  while (clipper.Step())
  {
    for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line)
    {
      const HistoryRow& row =
          history[visible_history[static_cast<size_t>(line)]];
      ImGui::TableNextRow();
      ImGui::PushID(line);
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%s", row.date.c_str());
      ImGui::TableNextColumn();
      if (UI::link(row.player.c_str(), "name"))
        Navigation::openPlayer(guiView, row.player_id);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.from.c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(row.managed ? palette.accent : palette.text, "%s",
                         row.to.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(LOC(row.kind_key));
      ImGui::TableNextColumn();
      UI::textRight(row.fee_text.c_str());
      ImGui::PopID();
    }
  }
  ImGui::EndTable();
}

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

float TransferMarketScene::dialogWidth() const
{
  return std::min(
      Tuning::Layout::DIALOG_WIDTH * scale(),
      ImGui::GetMainViewport()->Size.x * Tuning::Layout::DIALOG_VIEWPORT_SHARE);
}

void TransferMarketScene::renderReasons(
    const std::vector<TransferNegotiation::Reason>& reasons,
    const ImVec4& color) const
{
  for (const TransferNegotiation::Reason reason : reasons)
  {
    ImGui::Bullet();
    ImGui::TextColored(color, "%s",
                       LOC(TransferNegotiation::reasonKey(reason)));
  }
}

void TransferMarketScene::openOfferDialog(const TargetRow& row)
{
  offer_dialog = {};
  offer_dialog.requested = true;
  offer_dialog.player_id = row.id;
  offer_dialog.player = row.name;
  offer_dialog.club = row.club;
  offer_dialog.value = row.value;
  offer_dialog.terms.fee =
      row.asking_price > 0
          ? row.asking_price
          : static_cast<uint32_t>(std::max<int64_t>(
                row.value, TransferTuning::Contract::MINIMUM_WEEKLY_WAGE));
}

void TransferMarketScene::openContractDialog(PlayerID player_id,
                                             const std::string& name)
{
  const auto& controller = guiView->getController();
  const auto kind = controller.getContractTalkKind(player_id);
  if (!kind) return;
  contract_dialog = {};
  contract_dialog.requested = true;
  contract_dialog.player_id = player_id;
  contract_dialog.player = name;
  contract_dialog.kind = *kind;
  contract_dialog.demand = controller.getPlayerDemand(player_id, *kind);
  contract_dialog.projected =
      controller.getGame()->getTransfers().projectedRole(
          player_id, controller.getGame()->getManagedTeamId());
  contract_dialog.offer =
      TransferNegotiation::demandedOffer(contract_dialog.demand);
  contract_dialog.offer.release_clause = 0;
  contract_dialog.rounds_left = controller.getContractRoundsLeft(player_id);
}

void TransferMarketScene::openLoanDialog(const TargetRow& row)
{
  loan_dialog = {};
  loan_dialog.requested = true;
  loan_dialog.player_id = row.id;
  loan_dialog.player = row.name;
  loan_dialog.club = row.club;
  loan_dialog.terms.wage_share = TransferTuning::Loan::LISTED_WAGE_SHARE;
}

void TransferMarketScene::renderOfferDialog()
{
  if (offer_dialog.requested)
  {
    ImGui::OpenPopup("###transfer_offer");
    offer_dialog.requested = false;
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(dialogWidth(), 0.0f));
  const std::string title =
      fmt::sprintf(LOC("TRANSFER_OFFER_TITLE"), offer_dialog.player) +
      "###transfer_offer";
  if (!ImGui::BeginPopupModal(title.c_str(), nullptr,
                              ImGuiWindowFlags_NoSavedSettings))
    return;
  auto& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  TransferNegotiation::OfferTerms& terms = offer_dialog.terms;
  ImGui::TextColored(
      palette.muted, "%s",
      fmt::sprintf(LOC("TRANSFER_OFFER_HEADER"), offer_dialog.club,
                   Format::money(offer_dialog.value))
          .c_str());
  ImGui::Separator();

  using MI = Tuning::MoneyInput;
  fieldLabel(LOC("TRANSFER_FIELD_FEE"));
  moneyInput("##fee", terms.fee, MI::SMALL_STEP, MI::LARGE_STEP);
  int upfront = terms.upfront_percent;
  fieldLabel(LOC("TRANSFER_FIELD_UPFRONT"));
  if (ImGui::SliderInt("##upfront", &upfront,
                       TransferTuning::Offer::MIN_UPFRONT_PERCENT, 100, "%d%%"))
    terms.upfront_percent = static_cast<uint8_t>(upfront);
  int years = terms.instalment_years;
  fieldLabel(LOC("TRANSFER_FIELD_INSTALMENTS"));
  if (ImGui::SliderInt("##years", &years, 0,
                       TransferTuning::Offer::MAX_INSTALMENT_YEARS))
    terms.instalment_years = static_cast<uint8_t>(years);
  // Keep the structure consistent: deferred money needs instalment years.
  if (terms.instalment_years == 0) terms.upfront_percent = 100;
  if (terms.upfront_percent == 100) terms.instalment_years = 0;
  fieldLabel(LOC("TRANSFER_FIELD_APPEARANCE_BONUS"));
  moneyInput("##app_bonus", terms.appearance_bonus, MI::SMALL_STEP,
             MI::LARGE_STEP);
  fieldLabel(LOC("TRANSFER_FIELD_APPEARANCES"));
  countInput("##app_target", terms.appearance_target);
  fieldLabel(LOC("TRANSFER_FIELD_GOAL_BONUS"));
  moneyInput("##goal_bonus", terms.goal_bonus, MI::SMALL_STEP, MI::LARGE_STEP);
  fieldLabel(LOC("TRANSFER_FIELD_GOALS"));
  countInput("##goal_target", terms.goal_target);
  int sellOn = terms.sell_on_percent;
  fieldLabel(LOC("TRANSFER_FIELD_SELL_ON"));
  if (ImGui::SliderInt("##sell_on", &sellOn, 0,
                       TransferTuning::Offer::MAX_SELL_ON_PERCENT, "%d%%"))
    terms.sell_on_percent = static_cast<uint8_t>(sellOn);

  TransferMarket::Deal probe;
  probe.terms = terms;
  const uint32_t upfrontFee = TransferNegotiation::upfrontAmount(terms);
  const uint32_t agent = TransferMarket::agentFee(probe);
  const auto instalments = TransferNegotiation::instalmentAmounts(terms);
  ImGui::Separator();
  UI::keyValue(LOC("TRANSFER_SUMMARY_UPFRONT"),
               Format::moneyFull(upfrontFee).c_str(),
               Tuning::Layout::LABEL_WIDTH * scale());
  if (!instalments.empty())
    UI::keyValue(LOC("TRANSFER_SUMMARY_INSTALMENTS"),
                 fmt::sprintf(LOC("TRANSFER_SUMMARY_INSTALMENTS_VALUE"),
                              static_cast<int>(instalments.size()),
                              Format::money(instalments.front()))
                     .c_str(),
                 Tuning::Layout::LABEL_WIDTH * scale());
  UI::keyValue(LOC("TRANSFER_SUMMARY_AGENT"), Format::moneyFull(agent).c_str(),
               Tuning::Layout::LABEL_WIDTH * scale());
  const int64_t signing = static_cast<int64_t>(upfrontFee) + agent;
  UI::keyValue(LOC("TRANSFER_SUMMARY_BUDGET_AFTER"),
               Format::moneyFull(available_budget - signing).c_str(),
               Tuning::Layout::LABEL_WIDTH * scale());
  if (signing > available_budget)
    ImGui::TextColored(palette.negative, "%s",
                       LOC("TRANSFER_NOT_ENOUGH_BUDGET"));
  const bool valid = TransferNegotiation::isValid(terms) && terms.fee > 0;

  if (offer_dialog.response)
  {
    const ClubResponse& response = *offer_dialog.response;
    ImGui::Separator();
    {
      Theme::ScopedText heading(Theme::Text::TITLE);
      ImGui::TextColored(decisionColor(response.decision), "%s",
                         LOC(decisionKey(response.decision)));
    }
    if (response.counter_fee > 0 &&
        response.decision != ClubResponse::Decision::Accept)
      ImGui::TextUnformatted(
          fmt::sprintf(LOC("TRANSFER_COUNTER_FEE"),
                       Format::moneyFull(response.counter_fee))
              .c_str());
    renderReasons(response.reasons, palette.muted);
  }

  ImGui::Separator();
  const ImVec2 buttonSize(Tuning::Layout::STANDARD_BUTTON_WIDTH * scale(),
                          0.0f);
  if (ImGui::Button(LOC("TRANSFER_CANCEL"), buttonSize) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  const bool accepted =
      offer_dialog.response &&
      offer_dialog.response->decision == ClubResponse::Decision::Accept;
  if (offer_dialog.response && offer_dialog.response->counter_fee > 0 &&
      !accepted)
  {
    ImGui::SameLine();
    if (ImGui::Button(LOC("TRANSFER_MATCH_COUNTER")))
      terms.fee = offer_dialog.response->counter_fee;
  }
  ImGui::SameLine();
  if (accepted)
  {
    if (UI::primaryButton(LOC("TRANSFER_ACTION_TERMS")))
    {
      ImGui::CloseCurrentPopup();
      openContractDialog(offer_dialog.player_id, offer_dialog.player);
    }
  }
  else
  {
    ImGui::BeginDisabled(!valid || signing > available_budget);
    if (UI::primaryButton(LOC("TRANSFER_SUBMIT_OFFER"), buttonSize))
    {
      offer_dialog.response =
          controller.makeTransferOffer(offer_dialog.player_id, terms);
      refreshOffers();
    }
    ImGui::EndDisabled();
  }
  ImGui::EndPopup();
}

void TransferMarketScene::renderContractDialog()
{
  if (contract_dialog.requested)
  {
    ImGui::OpenPopup("###transfer_contract");
    contract_dialog.requested = false;
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(dialogWidth(), 0.0f));
  const std::string title =
      fmt::sprintf(LOC("TRANSFER_CONTRACT_TITLE"), contract_dialog.player) +
      "###transfer_contract";
  if (!ImGui::BeginPopupModal(title.c_str(), nullptr,
                              ImGuiWindowFlags_NoSavedSettings))
    return;
  auto& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  const TransferNegotiation::ContractDemand& demand = contract_dialog.demand;
  TransferNegotiation::ContractOffer& offer = contract_dialog.offer;
  const float keyWidth = Tuning::Layout::LABEL_WIDTH * scale();

  const char* kindKey = contract_dialog.kind == ContractKind::PreContract
                            ? "TRANSFER_TALK_PRE_CONTRACT"
                        : contract_dialog.kind == ContractKind::FreeAgent
                            ? "TRANSFER_TALK_FREE"
                            : "TRANSFER_TALK_TRANSFER";
  ImGui::TextColored(palette.muted, "%s", LOC(kindKey));
  UI::sectionLabel(LOC("TRANSFER_AGENT_DEMANDS"));
  UI::keyValue(LOC("TRANSFER_FIELD_WAGE"),
               fmt::sprintf(LOC("TRANSFER_PER_WEEK"),
                            Format::moneyFull(demand.weekly_wage))
                   .c_str(),
               keyWidth);
  UI::keyValue(LOC("TRANSFER_FIELD_YEARS"),
               fmt::format("{}-{}", static_cast<int>(demand.min_years),
                           static_cast<int>(demand.max_years))
                   .c_str(),
               keyWidth);
  if (demand.signing_bonus > 0)
    UI::keyValue(LOC("TRANSFER_FIELD_SIGNING_BONUS"),
                 Format::moneyFull(demand.signing_bonus).c_str(), keyWidth);
  if (demand.wants_release_clause)
    UI::keyValue(LOC("TRANSFER_FIELD_RELEASE_CLAUSE"),
                 fmt::sprintf(LOC("TRANSFER_CLAUSE_AT_MOST"),
                              Format::moneyFull(demand.max_release_clause))
                     .c_str(),
                 keyWidth);
  UI::keyValue(LOC("TRANSFER_FIELD_DESIRED_ROLE"),
               LOC(squadRoleKey(demand.desired_role)), keyWidth);
  UI::keyValue(LOC("TRANSFER_FIELD_PROJECTED_ROLE"),
               LOC(squadRoleKey(contract_dialog.projected)), keyWidth);

  UI::sectionLabel(LOC("TRANSFER_YOUR_OFFER"));
  using MI = Tuning::MoneyInput;
  fieldLabel(LOC("TRANSFER_FIELD_WAGE"));
  moneyInput("##wage", offer.weekly_wage, MI::WAGE_SMALL_STEP,
             MI::WAGE_LARGE_STEP);
  int years = offer.years;
  fieldLabel(LOC("TRANSFER_FIELD_YEARS"));
  if (ImGui::SliderInt("##contract_years", &years,
                       TransferTuning::Contract::MINIMUM_YEARS,
                       demand.max_years))
    offer.years = static_cast<uint8_t>(years);
  fieldLabel(LOC("TRANSFER_FIELD_SIGNING_BONUS"));
  moneyInput("##bonus", offer.signing_bonus, MI::SMALL_STEP, MI::LARGE_STEP);
  fieldLabel(LOC("TRANSFER_FIELD_RELEASE_CLAUSE"));
  moneyInput("##clause", offer.release_clause, MI::LARGE_STEP,
             10 * MI::LARGE_STEP);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", LOC("TRANSFER_CLAUSE_TIP"));
  fieldLabel(LOC("TRANSFER_FIELD_PROMISE"));
  const char* promisePreview =
      contract_dialog.promise_index == 0
          ? LOC("TRANSFER_PROMISE_NONE")
          : LOC(squadRoleKey(PROMISE_ROLES[static_cast<size_t>(
                contract_dialog.promise_index - 1)]));
  if (ImGui::BeginCombo("##promise", promisePreview))
  {
    if (ImGui::Selectable(LOC("TRANSFER_PROMISE_NONE"),
                          contract_dialog.promise_index == 0))
      contract_dialog.promise_index = 0;
    for (size_t index = 0; index < PROMISE_ROLES.size(); ++index)
    {
      if (ImGui::Selectable(
              LOC(squadRoleKey(PROMISE_ROLES[index])),
              contract_dialog.promise_index == static_cast<int>(index + 1)))
        contract_dialog.promise_index = static_cast<int>(index + 1);
    }
    ImGui::EndCombo();
  }
  offer.promised_role =
      contract_dialog.promise_index == 0
          ? std::nullopt
          : std::optional<SquadRole>(PROMISE_ROLES[static_cast<size_t>(
                contract_dialog.promise_index - 1)]);

  ImGui::TextColored(
      palette.faint, "%s",
      fmt::sprintf(LOC("TRANSFER_ROUNDS_LEFT"), contract_dialog.rounds_left)
          .c_str());
  if (contract_dialog.response)
  {
    ImGui::Separator();
    const bool accepted = contract_dialog.response->accepted;
    {
      Theme::ScopedText heading(Theme::Text::TITLE);
      ImGui::TextColored(
          accepted ? palette.positive : palette.negative, "%s",
          LOC(accepted ? "TRANSFER_PLAYER_AGREES" : "TRANSFER_PLAYER_REFUSES"));
    }
    renderReasons(contract_dialog.response->reasons, palette.muted);
    if (contract_dialog.over_budget)
      ImGui::TextColored(palette.negative, "%s",
                         LOC("TRANSFER_NOT_ENOUGH_BUDGET"));
  }

  ImGui::Separator();
  const ImVec2 buttonSize(Tuning::Layout::STANDARD_BUTTON_WIDTH * scale(),
                          0.0f);
  if (ImGui::Button(LOC("TRANSFER_CANCEL"), buttonSize) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  ImGui::SameLine();
  ImGui::BeginDisabled(contract_dialog.rounds_left == 0 || offer.years == 0);
  if (UI::primaryButton(LOC("TRANSFER_PROPOSE"), buttonSize))
  {
    const auto result =
        controller.proposeContract(contract_dialog.player_id, offer);
    contract_dialog.response = result.response;
    contract_dialog.over_budget = result.over_budget;
    contract_dialog.rounds_left = result.rounds_left;
    if (result.completed)
    {
      showToast(
          fmt::sprintf(LOC(contract_dialog.kind == ContractKind::PreContract
                               ? "TRANSFER_PRE_CONTRACT_TOAST"
                               : "TRANSFER_COMPLETED_TOAST"),
                       contract_dialog.player));
      refreshData();
      ImGui::CloseCurrentPopup();
    }
    else
    {
      refreshOffers();
    }
  }
  ImGui::EndDisabled();
  ImGui::EndPopup();
}

void TransferMarketScene::renderLoanDialog()
{
  if (loan_dialog.requested)
  {
    ImGui::OpenPopup("###transfer_loan");
    loan_dialog.requested = false;
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(dialogWidth(), 0.0f));
  const std::string title =
      fmt::sprintf(LOC("TRANSFER_LOAN_TITLE"), loan_dialog.player) +
      "###transfer_loan";
  if (!ImGui::BeginPopupModal(title.c_str(), nullptr,
                              ImGuiWindowFlags_NoSavedSettings))
    return;
  auto& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  TransferNegotiation::LoanTerms& terms = loan_dialog.terms;
  ImGui::TextColored(
      palette.muted, "%s",
      fmt::sprintf(LOC("TRANSFER_LOAN_HEADER"), loan_dialog.club).c_str());
  ImGui::Separator();
  fieldLabel(LOC("TRANSFER_FIELD_DURATION"));
  if (ImGui::BeginCombo(
          "##duration",
          LOC(terms.duration == TransferNegotiation::LoanDuration::SeasonEnd
                  ? "TRANSFER_LOAN_SEASON"
                  : "TRANSFER_LOAN_SIX_MONTHS")))
  {
    if (ImGui::Selectable(
            LOC("TRANSFER_LOAN_SEASON"),
            terms.duration == TransferNegotiation::LoanDuration::SeasonEnd))
      terms.duration = TransferNegotiation::LoanDuration::SeasonEnd;
    if (ImGui::Selectable(
            LOC("TRANSFER_LOAN_SIX_MONTHS"),
            terms.duration == TransferNegotiation::LoanDuration::SixMonths))
      terms.duration = TransferNegotiation::LoanDuration::SixMonths;
    ImGui::EndCombo();
  }
  int share = terms.wage_share;
  fieldLabel(LOC("TRANSFER_FIELD_WAGE_SHARE"));
  if (ImGui::SliderInt("##share", &share, 0, 100, "%d%%"))
    terms.wage_share = static_cast<uint8_t>(share);
  using MI = Tuning::MoneyInput;
  fieldLabel(LOC("TRANSFER_FIELD_LOAN_FEE"));
  moneyInput("##loan_fee", terms.loan_fee, MI::SMALL_STEP, MI::LARGE_STEP);
  fieldLabel(LOC("TRANSFER_FIELD_OPTION_FEE"));
  moneyInput("##option_fee", terms.option_fee, MI::SMALL_STEP, MI::LARGE_STEP);
  ImGui::Checkbox(LOC("TRANSFER_FIELD_OBLIGATION"), &terms.obligation);
  ImGui::SameLine();
  ImGui::Checkbox(LOC("TRANSFER_FIELD_RECALL"), &terms.recall_clause);

  if (loan_dialog.response)
  {
    const ClubResponse& response = *loan_dialog.response;
    ImGui::Separator();
    {
      Theme::ScopedText heading(Theme::Text::TITLE);
      ImGui::TextColored(decisionColor(response.decision), "%s",
                         LOC(decisionKey(response.decision)));
    }
    if (response.decision == ClubResponse::Decision::Counter)
      ImGui::TextUnformatted(
          fmt::sprintf(LOC("TRANSFER_LOAN_COUNTER"),
                       static_cast<int>(response.counter_wage_share),
                       response.counter_option_fee > 0
                           ? Format::moneyFull(response.counter_option_fee)
                           : std::string("-"))
              .c_str());
    renderReasons(response.reasons, palette.muted);
  }

  ImGui::Separator();
  const ImVec2 buttonSize(Tuning::Layout::STANDARD_BUTTON_WIDTH * scale(),
                          0.0f);
  if (ImGui::Button(LOC("TRANSFER_CANCEL"), buttonSize) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  if (loan_dialog.response &&
      loan_dialog.response->decision == ClubResponse::Decision::Counter)
  {
    ImGui::SameLine();
    if (ImGui::Button(LOC("TRANSFER_MATCH_COUNTER")))
    {
      terms.wage_share = loan_dialog.response->counter_wage_share;
      terms.option_fee = loan_dialog.response->counter_option_fee;
    }
  }
  ImGui::SameLine();
  if (UI::primaryButton(LOC("TRANSFER_SUBMIT_OFFER"), buttonSize))
  {
    loan_dialog.response =
        controller.makeLoanOffer(loan_dialog.player_id, terms);
    if (loan_dialog.response->decision == ClubResponse::Decision::Accept)
    {
      showToast(fmt::sprintf(LOC("TRANSFER_LOAN_TOAST"), loan_dialog.player));
      refreshData();
      ImGui::CloseCurrentPopup();
    }
  }
  ImGui::EndPopup();
}

void TransferMarketScene::renderCounterDialog()
{
  if (counter_dialog.requested)
  {
    ImGui::OpenPopup("###transfer_counter");
    counter_dialog.requested = false;
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  const std::string title =
      std::string(LOC("TRANSFER_COUNTER_TITLE")) + "###transfer_counter";
  if (!ImGui::BeginPopupModal(title.c_str(), nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize))
    return;
  auto& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  const OfferRow& offer = counter_dialog.offer;
  ImGui::TextUnformatted(fmt::sprintf(LOC("TRANSFER_COUNTER_HEADER"),
                                      offer.club, offer.player,
                                      offer.amount_text)
                             .c_str());
  fieldLabel(LOC("TRANSFER_NEW_PRICE"));
  moneyInput("##counter_fee", counter_dialog.fee,
             Tuning::MoneyInput::SMALL_STEP, Tuning::MoneyInput::LARGE_STEP);
  if (counter_dialog.response)
  {
    const ClubResponse& response = *counter_dialog.response;
    ImGui::TextColored(decisionColor(response.decision), "%s",
                       LOC(decisionKey(response.decision)));
    if (response.decision == ClubResponse::Decision::Counter)
      ImGui::TextUnformatted(
          fmt::sprintf(LOC("TRANSFER_COUNTER_IMPROVED"),
                       Format::moneyFull(response.counter_fee))
              .c_str());
    renderReasons(response.reasons, palette.muted);
  }
  const ImVec2 buttonSize(Tuning::Layout::STANDARD_BUTTON_WIDTH * scale(),
                          0.0f);
  if (ImGui::Button(LOC("TRANSFER_CANCEL"), buttonSize) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  ImGui::SameLine();
  ImGui::BeginDisabled(counter_dialog.fee == 0);
  if (UI::primaryButton(LOC("TRANSFER_SUBMIT_COUNTER"), buttonSize))
  {
    if (offer.offer_id == 0)
    {
      // A bid on a listed player: the new price becomes the asking price.
      controller.counterOffer(offer.player_id, counter_dialog.fee);
      refreshData();
      ImGui::CloseCurrentPopup();
    }
    else
    {
      counter_dialog.response =
          controller.counterIncomingOffer(offer.offer_id, counter_dialog.fee);
      if (counter_dialog.response->decision == ClubResponse::Decision::Accept)
      {
        showToast(fmt::sprintf(LOC("TRANSFER_SOLD_TOAST"), offer.player));
        ImGui::CloseCurrentPopup();
      }
      refreshData();
    }
  }
  ImGui::EndDisabled();
  ImGui::EndPopup();
}

void TransferMarketScene::renderListingDialog()
{
  if (listing_dialog.requested)
  {
    ImGui::OpenPopup("###transfer_listing");
    listing_dialog.requested = false;
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  const std::string title =
      fmt::sprintf(LOC("TRANSFER_LIST_TITLE"), listing_dialog.player) +
      "###transfer_listing";
  if (!ImGui::BeginPopupModal(title.c_str(), nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize))
    return;
  fieldLabel(LOC("TRANSFER_ASKING_PRICE"));
  moneyInput("##asking", listing_dialog.price, Tuning::MoneyInput::SMALL_STEP,
             Tuning::MoneyInput::LARGE_STEP);
  const ImVec2 buttonSize(Tuning::Layout::STANDARD_BUTTON_WIDTH * scale(),
                          0.0f);
  if (ImGui::Button(LOC("TRANSFER_CANCEL"), buttonSize) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  ImGui::SameLine();
  ImGui::BeginDisabled(listing_dialog.price == 0);
  if (UI::primaryButton(LOC("TRANSFER_LIST_PLAYER"), buttonSize))
  {
    auto& controller = guiView->getController();
    controller.listPlayerForTransfer(listing_dialog.player_id,
                                     listing_dialog.price);
    const bool listed = controller.isPlayerListed(listing_dialog.player_id);
    showToast(LOC(listed ? "PROFILE_LISTED_TOAST" : "TRANSFER_DEAL_FAILED"),
              !listed);
    refreshData();
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  ImGui::EndPopup();
}

void TransferMarketScene::renderReleaseDialog()
{
  if (release_dialog.requested)
  {
    ImGui::OpenPopup("###transfer_release");
    release_dialog.requested = false;
  }
  const UI::DialogResult result =
      UI::confirmDialog("###transfer_release", LOC("TRANSFER_RELEASE_TITLE"),
                        release_dialog.body.c_str(), LOC("TRANSFER_RELEASE"),
                        LOC("TRANSFER_CANCEL"));
  if (result != UI::DialogResult::CONFIRM) return;
  const bool released =
      guiView->getController().releasePlayer(release_dialog.player_id);
  showToast(LOC(released ? "TRANSFER_RELEASED_TOAST" : "TRANSFER_DEAL_FAILED"),
            !released);
  refreshData();
}
