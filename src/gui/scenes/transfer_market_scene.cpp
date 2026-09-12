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
#include "model/competition.h"
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
  FIT,
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

/** Estimated ability: a rating chip, or a range chip ("64-72") coloured by
 * its centre when the scouts know too little for a single number. */
void estimateChip(float overall, bool ranged, const std::string& text)
{
  if (!ranged)
  {
    UI::ratingChip(overall);
    return;
  }
  const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
  const float padX = 6.0f * scale();
  const ImVec2 size(textSize.x + 2.0f * padX, textSize.y + 2.0f * scale());
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const ImVec4 color = Theme::ratingColor(overall);
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRect(start, ImVec2(start.x + size.x, start.y + size.y),
                    Theme::toU32(color, 0.55f), 4.0f * scale());
  drawList->AddText(ImVec2(start.x + padX, start.y + scale()),
                    Theme::toU32(color), text.c_str());
  ImGui::Dummy(size);
}

/** Centres a deal dialog that fits its content up to the viewport height;
 * taller content (responses, reasons) scrolls instead of leaving the
 * screen. */
void placeDialog(float width)
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(width, 0.0f),
      ImVec2(width,
             viewport->Size.y * Tuning::Layout::DIALOG_VIEWPORT_SHARE));
}

std::string fitText(const TransferNegotiation::SquadFit& fit)
{
  using TransferNegotiation::FitKind;
  switch (fit.kind)
  {
    case FitKind::Starter:
      return LOC("TRANSFER_FIT_STARTER");
    case FitKind::Upgrade:
      return fmt::sprintf(LOC("TRANSFER_FIT_UPGRADE"),
                          static_cast<int>(std::lround(fit.gain)));
    case FitKind::Depth:
      return LOC("TRANSFER_FIT_DEPTH");
    case FitKind::None:
      break;
  }
  return "-";
}

ImVec4 fitColor(TransferNegotiation::FitKind kind)
{
  const Theme::Palette& palette = Theme::palette();
  switch (kind)
  {
    case TransferNegotiation::FitKind::Starter:
    case TransferNegotiation::FitKind::Upgrade:
      return palette.positive;
    case TransferNegotiation::FitKind::Depth:
      return palette.info;
    case TransferNegotiation::FitKind::None:
      break;
  }
  return palette.faint;
}

constexpr int64_t BID_ROUNDING = 10'000;
constexpr int64_t WAGE_ROUNDING = 100;
constexpr uint8_t DEFAULT_INSTALMENT_YEARS = 2;
constexpr int64_t WEEKS_PER_YEAR =
    static_cast<int64_t>(TransferTuning::Contract::WEEKS_PER_YEAR);

constexpr std::array<uint8_t, 5> UPFRONT_OPTIONS = {100, 80, 60, 40, 20};
constexpr std::array<const char*, 5> UPFRONT_LABELS = {"100%", "80%", "60%",
                                                       "40%", "20%"};
constexpr std::array<const char*, 5> YEAR_LABELS = {"1", "2", "3", "4", "5"};
constexpr std::array<uint16_t, 4> APPEARANCE_TARGETS = {10, 20, 30, 50};
constexpr std::array<uint16_t, 4> GOAL_TARGETS = {5, 10, 15, 20};
constexpr std::array<uint8_t, 5> SELL_ON_OPTIONS = {0, 10, 15, 20, 30};
constexpr std::array<const char*, 5> SELL_ON_LABELS = {"0%", "10%", "15%",
                                                       "20%", "30%"};
constexpr std::array<uint8_t, 5> WAGE_SHARE_OPTIONS = {0, 25, 50, 75, 100};
constexpr std::array<const char*, 5> WAGE_SHARE_LABELS = {"0%", "25%", "50%",
                                                          "75%", "100%"};

/** Index of @p value among @p options (the nearest one if absent). */
template <typename T, typename V>
int optionIndex(std::span<const T> options, V value)
{
  size_t best = 0;
  for (size_t index = 1; index < options.size(); ++index)
  {
    if (std::abs(static_cast<int>(options[index]) - static_cast<int>(value)) <
        std::abs(static_cast<int>(options[best]) - static_cast<int>(value)))
      best = index;
  }
  return static_cast<int>(best);
}

template <typename T, size_t N, typename V>
int optionIndex(const std::array<T, N>& options, V value)
{
  return optionIndex(std::span<const T>(options), value);
}

uint32_t clampFee(int64_t fee)
{
  return static_cast<uint32_t>(std::clamp<int64_t>(
      fee, BID_ROUNDING, std::numeric_limits<uint32_t>::max()));
}

/** @p amount times @p factor, rounded to a sensible fee. */
int64_t scaledAmount(int64_t amount, double factor)
{
  return std::llround(static_cast<double>(amount) * factor /
                      static_cast<double>(BID_ROUNDING)) *
         BID_ROUNDING;
}

int64_t scaledWage(int64_t wage, double factor)
{
  return std::llround(static_cast<double>(wage) * factor /
                      static_cast<double>(WAGE_ROUNDING)) *
         WAGE_ROUNDING;
}

/** Small muted caption above a dialog field. */
void formLabel(const char* text)
{
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * scale()));
  Theme::ScopedText caption(Theme::Text::SMALL);
  ImGui::TextColored(Theme::palette().muted, "%s", text);
}

/** Moves the cursor so the buttons labelled @p labels end at the right
 * edge (null labels are skipped). */
void alignActions(std::initializer_list<const char*> labels)
{
  float total = 0.0f;
  int count = 0;
  for (const char* label : labels)
  {
    if (label == nullptr) continue;
    total += UI::buttonWidth(label);
    ++count;
  }
  total += ImGui::GetStyle().ItemSpacing.x * static_cast<float>(count - 1);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0f, ImGui::GetContentRegionAvail().x - total));
}

/**
 * Card-looking panel for dialogs, built on a one-cell table instead of a
 * child window so an auto-sized dialog fits its content on the first frame.
 * Pair with endPanel() when it returns true.
 */
bool beginPanel(const char* id, const char* title)
{
  const float pad = Theme::Space::M * scale();
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(pad, pad));
  ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, Theme::palette().border);
  const bool open = ImGui::BeginTable(
      id, 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_PadOuterX,
      ImVec2(-FLT_MIN, 0.0f));
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  if (!open) return false;
  ImGui::TableSetupColumn("##panel", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                         Theme::toU32(Theme::palette().surface));
  if (title != nullptr && *title != '\0') UI::sectionLabel(title);
  return true;
}

void endPanel() { ImGui::EndTable(); }

/** An offer structure that fits the budget, if any. */
struct Suggestion
{
  std::optional<TransferNegotiation::OfferTerms> terms;
};

int64_t signingCash(const TransferNegotiation::OfferTerms& terms)
{
  TransferMarket::Deal deal;
  deal.terms = terms;
  return static_cast<int64_t>(TransferNegotiation::upfrontAmount(terms)) +
         static_cast<int64_t>(TransferMarket::agentFee(deal));
}

/** Spread the payments if that is enough, else lower the fee. */
Suggestion budgetFix(const TransferNegotiation::OfferTerms& terms,
                     int64_t budget)
{
  Suggestion fix;
  if (budget <= 0) return fix;
  for (const uint8_t percent : UPFRONT_OPTIONS)
  {
    if (percent >= terms.upfront_percent) continue;
    TransferNegotiation::OfferTerms candidate = terms;
    candidate.upfront_percent = percent;
    candidate.instalment_years =
        std::max(terms.instalment_years, DEFAULT_INSTALMENT_YEARS);
    if (signingCash(candidate) <= budget)
    {
      fix.terms = candidate;
      return fix;
    }
  }
  TransferNegotiation::OfferTerms candidate = terms;
  const double share =
      terms.upfront_percent / 100.0 +
      TransferTuning::Offer::AGENT_FEE_PERCENT / 100.0;
  candidate.fee = static_cast<uint32_t>(std::clamp<int64_t>(
      static_cast<int64_t>(static_cast<double>(budget) / share) /
          BID_ROUNDING * BID_ROUNDING,
      0, std::numeric_limits<uint32_t>::max()));
  while (candidate.fee >= BID_ROUNDING && signingCash(candidate) > budget)
    candidate.fee -= static_cast<uint32_t>(BID_ROUNDING);
  if (candidate.fee >= BID_ROUNDING) fix.terms = candidate;
  return fix;
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
  embargo = game->getWorld().isTransferEmbargoed(managed);
  available_budget = embargo ? 0 : controller.transferBudgetForTeam(managed);
  wage_room = controller.getManagedTeam()->get().getFinances().getWageBudget() -
              controller.getWeeklyWageBill(managed);

  league_ids.clear();
  league_names.clear();
  for (const auto& league : controller.getLeagues())
  {
    league_ids.push_back(league.get().getId());
    league_names.push_back(Competitions::leagueName(league.get()));
  }

  refreshSquad();
  refreshNeeds();
  if (search_dirty)
  {
    refreshTargets();
    refreshRecommended();
  }
  else
  {
    refreshTargetFlags();
  }
  refreshShortlist();
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
  row.role = RoleUtils::shortName(scouted.role);
  row.age = scouted.age;
  row.overall = scouted.overall;
  row.overall_low = scouted.overall_low;
  row.overall_high = scouted.overall_high;
  row.knowledge = scouted.knowledge;
  row.ranged = scouted.knowledge < ScoutingTuning::RANGE_DISPLAY_KNOWLEDGE &&
               std::lround(row.overall_high) > std::lround(row.overall_low);
  row.overall_text =
      row.ranged ? fmt::format("{:.0f}-{:.0f}", row.overall_low,
                               row.overall_high)
                 : fmt::format("{:.0f}", row.overall);
  row.potential_text = fmt::format("{:.0f}-{:.0f}", scouted.potential_low,
                                   scouted.potential_high);
  row.fit = TransferNegotiation::squadFit(needs, row.role_id, row.overall);
  row.fit_text = fitText(row.fit);
  row.value = scouted.estimated_value;
  row.value_text = Format::money(row.value);
  row.wage = scouted.wage;
  row.wage_text = Format::money(row.wage);
  row.contract_years = scouted.contract_years;
  row.free_agent = row.team_id == FREE_AGENTS_TEAM_ID;
  const int64_t value_cap = affordableValueCap();
  row.affordable =
      (row.free_agent || (value_cap > 0 && row.value <= value_cap)) &&
      static_cast<double>(row.wage) *
              static_cast<double>(ScoutingTuning::EXPECTED_WAGE_RAISE) <=
          static_cast<double>(wage_room);
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
  ScoutSearchFilter search;
  if (filters.role_index > 0)
    search.role = FILTER_ROLES[static_cast<size_t>(filters.role_index - 1)];
  search.min_age = static_cast<uint8_t>(filters.min_age);
  search.max_age = static_cast<uint8_t>(filters.max_age);
  search.min_overall = static_cast<float>(filters.min_overall);
  if (filters.max_value_index > 0)
    search.max_value = Tuning::Filters::MAX_VALUE_STEPS[static_cast<size_t>(
        filters.max_value_index - 1)];
  if (filters.league_index > 0 &&
      static_cast<size_t>(filters.league_index) <= league_ids.size())
    search.league_id =
        league_ids[static_cast<size_t>(filters.league_index - 1)];
  search.free_agents_only = filters.availability == Availability::FREE_AGENTS;
  search.limit = Tuning::Filters::RESULT_LIMIT;
  targets = searchRows(search, filters.affordable_only);
  applyTargetFilter();
}

int64_t TransferMarketScene::affordableValueCap() const
{
  return static_cast<int64_t>(static_cast<double>(available_budget) *
                              Tuning::Filters::AFFORDABLE_VALUE_MULTIPLE);
}

std::vector<TransferMarketScene::TargetRow> TransferMarketScene::searchRows(
    ScoutSearchFilter search, bool affordable) const
{
  const auto& controller = guiView->getController();
  std::vector<TargetRow> rows;
  if (affordable)
  {
    const int64_t cap = affordableValueCap();
    // Free agents cost no fee: they come from their own search below.
    if (cap > 0 && !search.free_agents_only)
    {
      ScoutSearchFilter capped = search;
      capped.max_value =
          search.max_value > 0 ? std::min(search.max_value, cap) : cap;
      for (const ScoutedPlayerRow& scouted :
           controller.searchScoutedPlayers(capped))
      {
        if (scouted.team_id != FREE_AGENTS_TEAM_ID)
          rows.push_back(makeTargetRow(scouted));
      }
    }
    // A league filter excludes the free-agent pool.
    if (search.league_id == 0)
    {
      search.free_agents_only = true;
      search.max_value = 0;
      for (const ScoutedPlayerRow& scouted :
           controller.searchScoutedPlayers(search))
        rows.push_back(makeTargetRow(scouted));
    }
    return rows;
  }
  for (const ScoutedPlayerRow& scouted :
       controller.searchScoutedPlayers(search))
    rows.push_back(makeTargetRow(scouted));
  return rows;
}

void TransferMarketScene::refreshNeeds()
{
  // The managed squad is known exactly; players leaving on a pre-contract
  // do not count.
  std::vector<std::pair<PlayerRole, float>> shape;
  shape.reserve(squad.size());
  for (const SquadRow& row : squad)
  {
    if (!row.committed)
      shape.emplace_back(row.player.role_id,
                         static_cast<float>(row.player.overall));
  }
  needs = TransferNegotiation::squadNeeds(shape);
  needs_text.clear();
  for (const TransferNegotiation::PositionNeed& need : needs.positions)
  {
    if (need.count >= need.wanted) continue;
    if (!needs_text.empty()) needs_text += ", ";
    needs_text += fmt::sprintf(
        LOC(need.count < need.starters ? "TRANSFER_NEED_STARTER"
                                       : "TRANSFER_NEED_DEPTH"),
        RoleUtils::shortName(need.group));
  }
}

void TransferMarketScene::refreshRecommended()
{
  // Need-based and affordable, whatever the Search tab filters say. With
  // the default filters the Search rows are exactly that pool: reuse them
  // instead of estimating the world again.
  const Filters defaults;
  const bool default_search =
      filters.affordable_only && filters.role_index == 0 &&
      filters.league_index == 0 && filters.min_age == defaults.min_age &&
      filters.max_age == defaults.max_age && filters.min_overall == 0 &&
      filters.max_value_index == 0 &&
      filters.availability == Availability::ANY;
  std::vector<TargetRow> pool;
  if (default_search)
  {
    pool = targets;
  }
  else
  {
    ScoutSearchFilter search;
    search.limit = Tuning::Filters::RESULT_LIMIT;
    pool = searchRows(search, true);
  }
  std::erase_if(pool,
                [](const TargetRow& row)
                {
                  return !row.affordable || row.fit.score <= 0.0f ||
                         row.fit.kind == TransferNegotiation::FitKind::None;
                });
  const auto better = [](const TargetRow& a, const TargetRow& b)
  {
    if (a.fit.score != b.fit.score) return a.fit.score > b.fit.score;
    return a.value != b.value ? a.value < b.value : a.id < b.id;
  };
  const size_t keep = std::min(pool.size(), Tuning::Filters::RECOMMENDED_LIMIT);
  std::partial_sort(pool.begin(),
                    pool.begin() + static_cast<std::ptrdiff_t>(keep),
                    pool.end(), better);
  pool.resize(keep);
  recommended = std::move(pool);
  visible_recommended.resize(recommended.size());
  for (size_t index = 0; index < recommended.size(); ++index)
    visible_recommended[index] = index;
  sortTargets(recommended, visible_recommended, recommended_sort);
}

void TransferMarketScene::refreshTargetFlags()
{
  // Cheap after an action: re-derive the cached rows (club, listing, loan
  // list, shortlist) without estimating every player again.
  rederiveRows(targets);
  applyTargetFilter();
  rederiveRows(recommended);
  visible_recommended.resize(recommended.size());
  for (size_t index = 0; index < recommended.size(); ++index)
    visible_recommended[index] = index;
  sortTargets(recommended, visible_recommended, recommended_sort);
}

void TransferMarketScene::rederiveRows(std::vector<TargetRow>& rows) const
{
  const auto& controller = guiView->getController();
  const TeamID managed = controller.getGame()->getManagedTeamId();
  std::vector<TargetRow> updated;
  updated.reserve(rows.size());
  for (const TargetRow& row : rows)
  {
    const auto scouted = controller.getScoutedRow(row.id);
    if (scouted && scouted->team_id != managed)
      updated.push_back(makeTargetRow(*scouted));
  }
  rows = std::move(updated);
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
    if (filters.affordable_only && !row.affordable) continue;
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
  const auto column = sort.column == 0
                          ? TargetColumn::FIT
                          : static_cast<TargetColumn>(sort.column);
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
                          // Ranges sort by their midpoint.
                          cmp = UI::compare(a.overall_low + a.overall_high,
                                            b.overall_low + b.overall_high);
                          break;
                        case TargetColumn::FIT:
                          // Equal fit: the cheaper player first.
                          cmp = UI::compare(a.fit.score, b.fit.score);
                          if (cmp == 0)
                            cmp = sort.ascending
                                      ? UI::compare(a.value, b.value)
                                      : UI::compare(b.value, a.value);
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
    row.value = value;
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
    row.value = value;
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
    row.severance = record.kind == TransferKind::Release;
    row.fee_text =
        record.fee == 0 ? std::string("-")
        : row.severance ? fmt::sprintf(LOC("TRANSFER_HISTORY_SEVERANCE"),
                                       Format::money(record.fee))
                        : Format::money(record.fee);
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
    if (ImGui::BeginTabItem(LOC("TRANSFER_TAB_RECOMMENDED")))
    {
      renderRecommendedTab();
      ImGui::EndTabItem();
    }
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
               LOC(embargo ? "TRANSFER_TILE_EMBARGO_NOTE"
                           : "TRANSFER_TILE_BUDGET_NOTE"),
               embargo                ? palette.negative
               : available_budget > 0 ? palette.text
                                      : palette.warning,
               width);
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
          : fmt::sprintf(
                Format::plural("TRANSFER_DAYS_LEFT", window.days_to_deadline),
                window.days_to_deadline);
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
    UI::emptyState(LOC("TRANSFER_EMPTY_TITLE"),
                   LOC(filters.affordable_only ? "TRANSFER_EMPTY_AFFORDABLE_BODY"
                                               : "TRANSFER_EMPTY_BODY"));
    return;
  }
  renderTargetTable("TargetTable", targets, visible_targets, search_sort);
}

void TransferMarketScene::renderRecommendedTab()
{
  const Theme::Palette& palette = Theme::palette();
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(
      palette.muted, "%s",
      (needs_text.empty()
           ? std::string(LOC("TRANSFER_RECOMMENDED_NO_GAPS"))
           : fmt::sprintf(LOC("TRANSFER_RECOMMENDED_NEEDS"), needs_text))
          .c_str());
  ImGui::PopTextWrapPos();
  if (recommended.empty())
  {
    UI::emptyState(LOC("TRANSFER_RECOMMENDED_EMPTY_TITLE"),
                   LOC("TRANSFER_RECOMMENDED_EMPTY_BODY"));
    return;
  }
  renderTargetTable("RecommendedTable", recommended, visible_recommended,
                    recommended_sort);
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
          : RoleUtils::shortName(
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
      const char* const role = RoleUtils::longName(FILTER_ROLES[index]);
      if (ImGui::Selectable(role,
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
  UI::sameLineIfFits(L::COMBO_WIDTH * scale());
  ImGui::SetNextItemWidth(L::COMBO_WIDTH * scale());
  const auto valueLabel = [](int index)
  {
    return index == 0
               ? std::string(LOC("TRANSFER_FILTER_MAX_VALUE"))
               : fmt::sprintf(LOC("TRANSFER_FILTER_MAX_VALUE_AT"),
                              Format::money(Tuning::Filters::MAX_VALUE_STEPS
                                                [static_cast<size_t>(index - 1)]));
  };
  if (ImGui::BeginCombo("##market_max_value",
                        valueLabel(filters.max_value_index).c_str()))
  {
    for (int index = 0;
         index <= static_cast<int>(Tuning::Filters::MAX_VALUE_STEPS.size());
         ++index)
    {
      if (ImGui::Selectable(valueLabel(index).c_str(),
                            filters.max_value_index == index))
      {
        filters.max_value_index = index;
        changed = true;
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", LOC("TRANSFER_FILTER_MAX_VALUE_TIP"));

  const std::string affordableTip =
      fmt::sprintf(LOC("TRANSFER_FILTER_AFFORDABLE_TIP"),
                   Format::money(affordableValueCap()),
                   Format::money(std::max<int64_t>(wage_room, 0)));
  UI::sameLineIfFits(UI::buttonWidth(LOC("TRANSFER_FILTER_AFFORDABLE")) +
                     ImGui::GetFrameHeight());
  if (ImGui::Checkbox(LOC("TRANSFER_FILTER_AFFORDABLE"),
                      &filters.affordable_only))
    changed = true;
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", affordableTip.c_str());

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
                          Tuning::Layout::TARGET_TABLE_MIN_WIDTH,
                          ImVec2(0.0f, ImGui::GetContentRegionAvail().y), 2))
    return;
  const auto column = [](const char* key, TargetColumn columnId,
                         ImGuiTableColumnFlags extra = 0, float weight = 0.0f)
  {
    ImGui::TableSetupColumn(LOC(key), extra, weight,
                            static_cast<ImGuiID>(columnId));
  };
  column("TRANSFER_COL_SHORTLIST", TargetColumn::SHORTLIST,
         ImGuiTableColumnFlags_NoHide);
  column("TRANSFER_COL_NAME", TargetColumn::NAME,
         ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide,
         Tuning::Layout::NAME_COLUMN_WEIGHT);
  column("TRANSFER_COL_TEAM", TargetColumn::CLUB,
         ImGuiTableColumnFlags_WidthStretch,
         Tuning::Layout::CLUB_COLUMN_WEIGHT);
  column("TRANSFER_COL_ROLE", TargetColumn::ROLE);
  column("TRANSFER_COL_AGE", TargetColumn::AGE);
  column("TRANSFER_COL_EST_OVR", TargetColumn::OVERALL,
         ImGuiTableColumnFlags_PreferSortDescending);
  column("TRANSFER_COL_POTENTIAL", TargetColumn::POTENTIAL,
         ImGuiTableColumnFlags_NoSort);
  column("TRANSFER_COL_FIT", TargetColumn::FIT,
         ImGuiTableColumnFlags_PreferSortDescending |
             ImGuiTableColumnFlags_DefaultSort);
  column("TRANSFER_COL_EST_VALUE", TargetColumn::VALUE,
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
      if (ImGui::IsItemHovered() &&
          ImGui::CalcTextSize(row.name.c_str()).x >
              ImGui::GetColumnWidth())
        ImGui::SetTooltip("%s", row.name.c_str());
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
      UI::textFitted(row.club, ImGui::GetContentRegionAvail().x,
                     row.free_agent ? palette.faint : palette.muted);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.role.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.age);
      ImGui::TableNextColumn();
      estimateChip(row.overall, row.ranged, row.overall_text);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", fmt::sprintf(LOC("TRANSFER_KNOWLEDGE_TIP"),
                                             row.overall_low, row.overall_high,
                                             row.knowledge)
                                    .c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%s", row.potential_text.c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(fitColor(row.fit.kind), "%s", row.fit_text.c_str());
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
    for (auto* list : {&targets, &recommended})
    {
      for (TargetRow& row : *list)
        row.shortlisted = controller.isShortlisted(row.id);
    }
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
    const ImGuiSelectableFlags dealFlags =
        window.open && !embargo ? 0 : ImGuiSelectableFlags_Disabled;
    if (ImGui::Selectable(LOC("TRANSFER_ACTION_OFFER"), false, dealFlags))
      openOfferDialog(row);
    if (ImGui::Selectable(LOC("TRANSFER_ACTION_LOAN"), false, dealFlags))
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
    else if (embargo)
      ImGui::TextColored(Theme::palette().negative, "%s",
                         LOC("NEG_REASON_EMBARGO"));
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
               Tuning::Layout::WIDE_TABLE_MIN_WIDTH, ImVec2(0, 0)))
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
        // A counter can only be accepted while the window is open.
        ImGui::SameLine();
        ImGui::BeginDisabled(!window.open);
        if (ImGui::Button(LOC("TRANSFER_COUNTER")))
          counter_dialog = {true, row, row.fee, std::nullopt};
        ImGui::EndDisabled();
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
          Tuning::Layout::WIDE_TABLE_MIN_WIDTH,
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
          Tuning::Layout::WIDE_TABLE_MIN_WIDTH,
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
          Tuning::Layout::WIDE_TABLE_MIN_WIDTH,
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
          listing_dialog = {true, player.id, player.name,
                            player.market_value, player.market_value};
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
          Tuning::Layout::WIDE_TABLE_MIN_WIDTH,
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
      if (row.severance)
      {
        UI::textRightColored(palette.muted, row.fee_text.c_str());
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s", LOC("TRANSFER_HISTORY_SEVERANCE_TIP"));
      }
      else
      {
        UI::textRight(row.fee_text.c_str());
      }
      ImGui::PopID();
    }
  }
  ImGui::EndTable();
}

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

bool TransferMarketScene::wideDialogs() const
{
  return ImGui::GetMainViewport()->Size.x >=
         Tuning::Layout::TWO_COLUMN_MIN_VIEWPORT * scale();
}

float TransferMarketScene::dialogWidth() const
{
  return std::min((wideDialogs() ? Tuning::Layout::WIDE_DIALOG_WIDTH
                                 : Tuning::Layout::DIALOG_WIDTH) *
                      scale(),
                  ImGui::GetMainViewport()->Size.x *
                      Tuning::Layout::DIALOG_VIEWPORT_SHARE);
}

void TransferMarketScene::renderDialogColumns(
    const std::function<void()>& terms,
    const std::function<void()>& summary) const
{
  // Terms on the left, a summary card on the right; stacked when narrow.
  if (wideDialogs() &&
      ImGui::BeginTable("##dialog_columns", 2, ImGuiTableFlags_SizingStretchProp))
  {
    ImGui::TableSetupColumn("##terms", ImGuiTableColumnFlags_WidthStretch,
                            Tuning::Layout::TERMS_COLUMN_WEIGHT);
    ImGui::TableSetupColumn("##summary", ImGuiTableColumnFlags_WidthStretch,
                            Tuning::Layout::SUMMARY_COLUMN_WEIGHT);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    terms();
    ImGui::TableNextColumn();
    summary();
    ImGui::EndTable();
    return;
  }
  terms();
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * scale()));
  summary();
}

void TransferMarketScene::renderReasons(
    const std::vector<TransferNegotiation::Reason>& reasons,
    const ImVec4& color) const
{
  ImGui::PushTextWrapPos(0.0f);
  for (const TransferNegotiation::Reason reason : reasons)
  {
    ImGui::Bullet();
    ImGui::TextColored(color, "%s",
                       LOC(TransferNegotiation::reasonKey(reason)));
  }
  ImGui::PopTextWrapPos();
}

void TransferMarketScene::renderClubResponse(const ClubResponse& response,
                                             const std::string& detail) const
{
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * scale()));
  {
    Theme::ScopedText heading(Theme::Text::TITLE);
    ImGui::TextColored(decisionColor(response.decision), "%s",
                       LOC(decisionKey(response.decision)));
  }
  if (!detail.empty())
  {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(detail.c_str());
    ImGui::PopTextWrapPos();
  }
  renderReasons(response.reasons, Theme::palette().muted);
}

void TransferMarketScene::openOfferDialog(const TargetRow& row)
{
  offer_dialog = {};
  offer_dialog.requested = true;
  offer_dialog.player_id = row.id;
  offer_dialog.player = row.name;
  offer_dialog.club = row.club;
  offer_dialog.value = row.value;
  offer_dialog.asking_price = row.asking_price;
  offer_dialog.wage = row.wage;
  offer_dialog.estimate_text = row.overall_text;
  offer_dialog.knowledge = row.knowledge;
  // Opening bid: the asking price, else the estimated value rounded to a
  // bid a club would make.
  offer_dialog.terms.fee =
      row.asking_price > 0
          ? row.asking_price
          : clampFee((row.value + BID_ROUNDING / 2) / BID_ROUNDING *
                     BID_ROUNDING);
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
  contract_dialog.offer.weekly_wage = contract_dialog.demand.asking_wage;
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
  loan_dialog.wage = row.wage;
  loan_dialog.value = row.value;
  loan_dialog.terms.wage_share = TransferTuning::Loan::LISTED_WAGE_SHARE;
}

void TransferMarketScene::renderOfferDialog()
{
  if (offer_dialog.requested)
  {
    ImGui::OpenPopup("###transfer_offer");
    offer_dialog.requested = false;
  }
  placeDialog(dialogWidth());
  const std::string title =
      fmt::sprintf(LOC("TRANSFER_OFFER_TITLE"), offer_dialog.player) +
      "###transfer_offer";
  if (!ImGui::BeginPopupModal(
          title.c_str(), nullptr,
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    return;
  auto& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  TransferNegotiation::OfferTerms& terms = offer_dialog.terms;
  const std::optional<ClubResponse>& response = offer_dialog.response;
  const bool accepted =
      response && response->decision == ClubResponse::Decision::Accept;

  // Figures shared by the terms and the summary.
  TransferMarket::Deal probe;
  probe.terms = terms;
  const int64_t upfront = TransferNegotiation::upfrontAmount(terms);
  const int64_t agent = TransferMarket::agentFee(probe);
  const int64_t signing = upfront + agent;
  const auto instalments = TransferNegotiation::instalmentAmounts(terms);
  const int64_t add_ons = static_cast<int64_t>(terms.appearance_bonus) +
                          static_cast<int64_t>(terms.goal_bonus);
  const int64_t expected_wage = static_cast<int64_t>(std::llround(
      static_cast<double>(offer_dialog.wage) *
      static_cast<double>(ScoutingTuning::EXPECTED_WAGE_RAISE)));
  const bool over_budget = signing > available_budget;

  const auto termsColumn = [&]
  {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(
        palette.muted, "%s",
        fmt::sprintf(LOC("TRANSFER_OFFER_HEADER"), offer_dialog.club,
                     Format::money(offer_dialog.value))
            .c_str());
    ImGui::TextColored(palette.faint, "%s",
                       fmt::sprintf(LOC("TRANSFER_OFFER_ESTIMATE"),
                                    offer_dialog.estimate_text,
                                    offer_dialog.knowledge)
                           .c_str());
    ImGui::PopTextWrapPos();

    formLabel(LOC("TRANSFER_FIELD_FEE"));
    int64_t fee = terms.fee;
    std::vector<UI::MoneyChip> chips;
    if (offer_dialog.asking_price > 0)
      chips.push_back({LOC("TRANSFER_CHIP_ASKING"), offer_dialog.asking_price});
    chips.push_back({LOC("TRANSFER_CHIP_VALUE"), offer_dialog.value});
    if (response && response->counter_fee > 0 && !accepted)
      chips.push_back({LOC("TRANSFER_CHIP_COUNTER"), response->counter_fee});
    chips.push_back({LOC("TRANSFER_CHIP_MINUS_10"), scaledAmount(fee, 0.9)});
    chips.push_back({LOC("TRANSFER_CHIP_PLUS_10"), scaledAmount(fee, 1.1)});
    if (UI::moneyInput("##fee", fee,
                       {.minimum = BID_ROUNDING,
                        .maximum = std::numeric_limits<uint32_t>::max(),
                        .chips = chips}))
      terms.fee = clampFee(fee);

    formLabel(LOC("TRANSFER_FIELD_UPFRONT"));
    int upfrontIndex = optionIndex(UPFRONT_OPTIONS, terms.upfront_percent);
    if (UI::segmented("##upfront", upfrontIndex, UPFRONT_LABELS))
    {
      terms.upfront_percent = UPFRONT_OPTIONS[static_cast<size_t>(upfrontIndex)];
      if (terms.upfront_percent < 100 && terms.instalment_years == 0)
        terms.instalment_years = DEFAULT_INSTALMENT_YEARS;
    }
    if (terms.upfront_percent < 100)
    {
      formLabel(LOC("TRANSFER_FIELD_INSTALMENTS"));
      int yearIndex = std::max(0, terms.instalment_years - 1);
      if (UI::segmented("##years", yearIndex,
                        std::span(YEAR_LABELS).first(
                            TransferTuning::Offer::MAX_INSTALMENT_YEARS)))
        terms.instalment_years = static_cast<uint8_t>(yearIndex + 1);
    }
    else
    {
      terms.instalment_years = 0;
    }

    ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * scale()));
    const bool addOns = beginPanel("##offer_add_ons", LOC("TRANSFER_ADD_ONS"));
    const auto bonusRow = [&](const char* label, const char* id,
                              uint32_t& bonus, uint16_t& target,
                              std::span<const uint16_t> options,
                              const char* target_key)
    {
      formLabel(label);
      int64_t amount = bonus;
      if (UI::moneyInput(id, amount,
                         {.maximum = std::numeric_limits<uint32_t>::max()}))
        bonus = static_cast<uint32_t>(amount);
      if (bonus == 0)
      {
        target = 0;
        return;
      }
      std::array<std::string, 4> texts;
      std::array<const char*, 4> labels{};
      for (size_t index = 0; index < options.size(); ++index)
      {
        texts[index] = fmt::sprintf(LOC(target_key), options[index]);
        labels[index] = texts[index].c_str();
      }
      int targetIndex = optionIndex(options, target);
      if (target == 0) target = options[static_cast<size_t>(targetIndex)];
      ImGui::PushID(id);
      if (UI::segmented("##target", targetIndex,
                        std::span(labels).first(options.size())))
        target = options[static_cast<size_t>(targetIndex)];
      ImGui::PopID();
    };
    bonusRow(LOC("TRANSFER_FIELD_APPEARANCE_BONUS"), "##app_bonus",
             terms.appearance_bonus, terms.appearance_target,
             APPEARANCE_TARGETS, "TRANSFER_APPS_SHORT");
    bonusRow(LOC("TRANSFER_FIELD_GOAL_BONUS"), "##goal_bonus",
             terms.goal_bonus, terms.goal_target, GOAL_TARGETS,
             "TRANSFER_GOALS_SHORT");
    formLabel(LOC("TRANSFER_FIELD_SELL_ON"));
    int sellOnIndex = optionIndex(SELL_ON_OPTIONS, terms.sell_on_percent);
    if (UI::segmented("##sell_on", sellOnIndex, SELL_ON_LABELS))
      terms.sell_on_percent = SELL_ON_OPTIONS[static_cast<size_t>(sellOnIndex)];
    if (addOns) endPanel();
  };

  const auto summaryColumn = [&]
  {
    const bool panel = beginPanel("##offer_summary", LOC("TRANSFER_SUMMARY_TITLE"));
    UI::summaryRow(LOC("TRANSFER_SUMMARY_FEE"),
                   Format::moneyFull(terms.fee).c_str());
    UI::summaryRow(LOC("TRANSFER_SUMMARY_UPFRONT"),
                   Format::moneyFull(upfront).c_str());
    if (!instalments.empty())
      UI::summaryRow(LOC("TRANSFER_SUMMARY_INSTALMENTS"),
                     fmt::sprintf(LOC("TRANSFER_SUMMARY_INSTALMENTS_VALUE"),
                                  static_cast<int>(instalments.size()),
                                  Format::money(instalments.front()))
                         .c_str());
    UI::summaryRow(LOC("TRANSFER_SUMMARY_AGENT"),
                   Format::moneyFull(agent).c_str());
    if (add_ons > 0)
      UI::summaryRow(LOC("TRANSFER_SUMMARY_ADD_ONS"),
                     Format::moneyFull(add_ons).c_str());
    UI::summaryRow(LOC("TRANSFER_SUMMARY_TOTAL"),
                   Format::moneyFull(static_cast<int64_t>(terms.fee) + agent +
                                     add_ons)
                       .c_str(),
                   nullptr, true);
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * scale()));
    UI::budgetImpact(LOC("TRANSFER_BUDGET_IMPACT"), available_budget,
                     available_budget - signing);
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * scale()));
    UI::summaryRow(
        LOC("TRANSFER_WAGE_EXPECTED"),
        fmt::sprintf(LOC("TRANSFER_WAGE_EXPECTED_VALUE"),
                     Format::money(expected_wage))
            .c_str());
    UI::budgetImpact(LOC("TRANSFER_WAGE_IMPACT"), wage_room,
                     wage_room - expected_wage, LOC("TRANSFER_WAGE_WARNING"));
    if (panel) endPanel();

    if (over_budget && !accepted)
    {
      // Say what would fit: spread the payments, else lower the fee.
      const Suggestion fix = budgetFix(terms, available_budget);
      ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * scale()));
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextColored(palette.negative, "%s",
                         fmt::sprintf(LOC("TRANSFER_OVER_BUDGET_BY"),
                                      Format::money(signing - available_budget))
                             .c_str());
      if (fix.terms)
      {
        const std::string text =
            fix.terms->fee == terms.fee
                ? fmt::sprintf(LOC("TRANSFER_SUGGEST_SPREAD"),
                               static_cast<int>(fix.terms->upfront_percent),
                               static_cast<int>(fix.terms->instalment_years))
                : fmt::sprintf(LOC("TRANSFER_SUGGEST_LOWER"),
                               Format::money(fix.terms->fee));
        ImGui::TextColored(palette.muted, "%s", text.c_str());
        ImGui::PopTextWrapPos();
        if (UI::secondaryButton(LOC("TRANSFER_APPLY_SUGGESTION")))
          terms = *fix.terms;
      }
      else
      {
        ImGui::TextColored(palette.muted, "%s",
                           LOC("TRANSFER_NOT_ENOUGH_BUDGET"));
        ImGui::PopTextWrapPos();
      }
    }
    if (response)
      renderClubResponse(
          *response,
          response->counter_fee > 0 && !accepted
              ? fmt::sprintf(LOC("TRANSFER_COUNTER_FEE"),
                             Format::moneyFull(response->counter_fee))
              : std::string());
  };
  renderDialogColumns(termsColumn, summaryColumn);

  const bool valid = TransferNegotiation::isValid(terms) && terms.fee > 0;
  ImGui::Separator();
  const char* submit =
      LOC(accepted ? "TRANSFER_ACTION_TERMS" : "TRANSFER_SUBMIT_OFFER");
  const bool counter = response && response->counter_fee > 0 && !accepted;
  alignActions({LOC("TRANSFER_CANCEL"),
                counter ? LOC("TRANSFER_MATCH_COUNTER") : nullptr, submit});
  if (UI::secondaryButton(LOC("TRANSFER_CANCEL")) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  if (counter)
  {
    ImGui::SameLine();
    if (UI::secondaryButton(LOC("TRANSFER_MATCH_COUNTER")))
      terms.fee = response->counter_fee;
  }
  ImGui::SameLine();
  if (accepted)
  {
    if (UI::primaryButton(submit))
    {
      ImGui::CloseCurrentPopup();
      openContractDialog(offer_dialog.player_id, offer_dialog.player);
    }
  }
  else
  {
    ImGui::BeginDisabled(!valid || over_budget);
    if (UI::primaryButton(submit))
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
  placeDialog(dialogWidth());
  const std::string title =
      fmt::sprintf(LOC("TRANSFER_CONTRACT_TITLE"), contract_dialog.player) +
      "###transfer_contract";
  if (!ImGui::BeginPopupModal(
          title.c_str(), nullptr,
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    return;
  auto& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  const TransferNegotiation::ContractDemand& demand = contract_dialog.demand;
  TransferNegotiation::ContractOffer& offer = contract_dialog.offer;

  const auto termsColumn = [&]
  {
    const char* kindKey = contract_dialog.kind == ContractKind::PreContract
                              ? "TRANSFER_TALK_PRE_CONTRACT"
                          : contract_dialog.kind == ContractKind::FreeAgent
                              ? "TRANSFER_TALK_FREE"
                              : "TRANSFER_TALK_TRANSFER";
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(palette.muted, "%s", LOC(kindKey));
    ImGui::PopTextWrapPos();

    formLabel(LOC("TRANSFER_FIELD_WAGE"));
    int64_t wage = offer.weekly_wage;
    const std::array<UI::MoneyChip, 3> wageChips = {
        {{LOC("TRANSFER_CHIP_AGENT_ASK"), demand.asking_wage},
         {LOC("TRANSFER_CHIP_MINUS_5"), scaledWage(demand.asking_wage, 0.95)},
         {LOC("TRANSFER_CHIP_MINUS_10"),
          scaledWage(demand.asking_wage, 0.90)}}};
    if (UI::moneyInput("##wage", wage,
                       {.maximum = std::numeric_limits<uint32_t>::max(),
                        .chips = wageChips}))
      offer.weekly_wage = static_cast<uint32_t>(wage);

    formLabel(LOC("TRANSFER_FIELD_YEARS"));
    int yearIndex = std::max(0, offer.years - 1);
    if (UI::segmented("##contract_years", yearIndex,
                      std::span(YEAR_LABELS).first(demand.max_years)))
      offer.years = static_cast<uint8_t>(yearIndex + 1);

    formLabel(LOC("TRANSFER_FIELD_SIGNING_BONUS"));
    int64_t bonus = offer.signing_bonus;
    std::vector<UI::MoneyChip> bonusChips = {{LOC("TRANSFER_CHIP_NONE"), 0}};
    if (demand.signing_bonus > 0)
      bonusChips.push_back(
          {LOC("TRANSFER_CHIP_ASKED_BONUS"), demand.signing_bonus});
    if (UI::moneyInput("##bonus", bonus,
                       {.maximum = std::numeric_limits<uint32_t>::max(),
                        .chips = bonusChips}))
      offer.signing_bonus = static_cast<uint32_t>(bonus);

    formLabel(LOC("TRANSFER_FIELD_RELEASE_CLAUSE"));
    int64_t clause = offer.release_clause;
    std::vector<UI::MoneyChip> clauseChips = {{LOC("TRANSFER_CHIP_NONE"), 0}};
    if (demand.wants_release_clause)
      clauseChips.push_back(
          {LOC("TRANSFER_CHIP_MAX_CLAUSE"), demand.max_release_clause});
    if (UI::moneyInput("##clause", clause,
                       {.maximum = std::numeric_limits<uint32_t>::max(),
                        .chips = clauseChips}))
      offer.release_clause = static_cast<uint32_t>(clause);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", LOC("TRANSFER_CLAUSE_TIP"));

    formLabel(LOC("TRANSFER_FIELD_PROMISE"));
    ImGui::SetNextItemWidth(-FLT_MIN);
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
  };

  const auto summaryColumn = [&]
  {
    const bool demands = beginPanel("##agent_demands", LOC("TRANSFER_AGENT_DEMANDS"));
    UI::summaryRow(LOC("TRANSFER_FIELD_AGENT_ASK"),
                   fmt::sprintf(LOC("TRANSFER_PER_WEEK"),
                                Format::moneyFull(demand.asking_wage))
                       .c_str());
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", LOC("TRANSFER_AGENT_ASK_TIP"));
    UI::summaryRow(LOC("TRANSFER_FIELD_YEARS"),
                   fmt::format("{}-{}", static_cast<int>(demand.min_years),
                               static_cast<int>(demand.max_years))
                       .c_str());
    if (demand.signing_bonus > 0)
      UI::summaryRow(LOC("TRANSFER_FIELD_SIGNING_BONUS"),
                     Format::moneyFull(demand.signing_bonus).c_str());
    if (demand.wants_release_clause)
      UI::summaryRow(LOC("TRANSFER_FIELD_RELEASE_CLAUSE"),
                     fmt::sprintf(LOC("TRANSFER_CLAUSE_AT_MOST"),
                                  Format::money(demand.max_release_clause))
                         .c_str());
    UI::summaryRow(LOC("TRANSFER_FIELD_DESIRED_ROLE"),
                   LOC(squadRoleKey(demand.desired_role)));
    UI::summaryRow(LOC("TRANSFER_FIELD_PROJECTED_ROLE"),
                   LOC(squadRoleKey(contract_dialog.projected)));
    if (demands) endPanel();

    ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * scale()));
    const bool panel = beginPanel("##contract_summary", LOC("TRANSFER_SUMMARY_TITLE"));
    const int64_t yearly = static_cast<int64_t>(offer.weekly_wage) *
                           static_cast<int64_t>(WEEKS_PER_YEAR);
    UI::summaryRow(LOC("TRANSFER_SUMMARY_YEARLY"),
                   Format::moneyFull(yearly).c_str());
    UI::summaryRow(LOC("TRANSFER_SUMMARY_CONTRACT_TOTAL"),
                   Format::moneyFull(yearly * offer.years +
                                     static_cast<int64_t>(offer.signing_bonus))
                       .c_str(),
                   nullptr, true);
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * scale()));
    UI::budgetImpact(LOC("TRANSFER_WAGE_IMPACT"), wage_room,
                     wage_room - static_cast<int64_t>(offer.weekly_wage),
                     LOC("TRANSFER_WAGE_OVER_ROOM"));
    ImGui::TextColored(
        palette.faint, "%s",
        fmt::sprintf(LOC("TRANSFER_ROUNDS_LEFT"), contract_dialog.rounds_left)
            .c_str());
    if (panel) endPanel();

    if (contract_dialog.response)
    {
      const bool agreed = contract_dialog.response->accepted;
      ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * scale()));
      {
        Theme::ScopedText heading(Theme::Text::TITLE);
        ImGui::TextColored(agreed ? palette.positive : palette.negative, "%s",
                           LOC(agreed ? "TRANSFER_PLAYER_AGREES"
                                      : "TRANSFER_PLAYER_REFUSES"));
      }
      renderReasons(contract_dialog.response->reasons, palette.muted);
      if (!agreed && contract_dialog.rounds_left > 0)
        ImGui::TextUnformatted(
            fmt::sprintf(LOC("TRANSFER_AGENT_NOW_ASKS"),
                         Format::moneyFull(demand.asking_wage))
                .c_str());
      if (contract_dialog.over_budget)
        ImGui::TextColored(palette.negative, "%s",
                           LOC("TRANSFER_NOT_ENOUGH_BUDGET"));
    }
  };
  renderDialogColumns(termsColumn, summaryColumn);

  ImGui::Separator();
  alignActions({LOC("TRANSFER_CANCEL"), LOC("TRANSFER_PROPOSE")});
  if (UI::secondaryButton(LOC("TRANSFER_CANCEL")) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  ImGui::SameLine();
  ImGui::BeginDisabled(contract_dialog.rounds_left == 0 || offer.years == 0);
  if (UI::primaryButton(LOC("TRANSFER_PROPOSE")))
  {
    const auto result =
        controller.proposeContract(contract_dialog.player_id, offer);
    contract_dialog.response = result.response;
    contract_dialog.over_budget = result.over_budget;
    contract_dialog.rounds_left = result.rounds_left;
    if (!result.response.accepted)
      contract_dialog.demand.asking_wage = result.response.demand.asking_wage;
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
  placeDialog(dialogWidth());
  const std::string title =
      fmt::sprintf(LOC("TRANSFER_LOAN_TITLE"), loan_dialog.player) +
      "###transfer_loan";
  if (!ImGui::BeginPopupModal(
          title.c_str(), nullptr,
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    return;
  auto& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  TransferNegotiation::LoanTerms& terms = loan_dialog.terms;
  const GameDateValue today = controller.getCurrentDate();
  const int weeks = TransferNegotiation::weeksBetween(
      today, TransferNegotiation::loanEndDate(today, terms.duration));
  const int64_t weekly = static_cast<int64_t>(loan_dialog.wage) *
                         terms.wage_share / 100;

  const auto termsColumn = [&]
  {
    ImGui::TextColored(
        palette.muted, "%s",
        fmt::sprintf(LOC("TRANSFER_LOAN_HEADER"), loan_dialog.club).c_str());
    formLabel(LOC("TRANSFER_FIELD_DURATION"));
    int duration =
        terms.duration == TransferNegotiation::LoanDuration::SeasonEnd ? 0 : 1;
    const std::array<const char*, 2> durations = {
        LOC("TRANSFER_LOAN_SEASON"), LOC("TRANSFER_LOAN_SIX_MONTHS")};
    if (UI::segmented("##duration", duration, durations))
      terms.duration = duration == 0
                           ? TransferNegotiation::LoanDuration::SeasonEnd
                           : TransferNegotiation::LoanDuration::SixMonths;
    formLabel(LOC("TRANSFER_FIELD_WAGE_SHARE"));
    int shareIndex = optionIndex(WAGE_SHARE_OPTIONS, terms.wage_share);
    if (UI::segmented("##share", shareIndex, WAGE_SHARE_LABELS))
      terms.wage_share = WAGE_SHARE_OPTIONS[static_cast<size_t>(shareIndex)];
    formLabel(LOC("TRANSFER_FIELD_LOAN_FEE"));
    int64_t fee = terms.loan_fee;
    if (UI::moneyInput("##loan_fee", fee,
                       {.maximum = std::numeric_limits<uint32_t>::max()}))
      terms.loan_fee = static_cast<uint32_t>(fee);
    formLabel(LOC("TRANSFER_FIELD_OPTION_FEE"));
    int64_t option = terms.option_fee;
    const std::array<UI::MoneyChip, 2> optionChips = {
        {{LOC("TRANSFER_CHIP_NONE"), 0},
         {LOC("TRANSFER_CHIP_OPTION_VALUE"),
          scaledAmount(loan_dialog.value,
                       TransferTuning::Loan::OPTION_VALUE_MULTIPLE)}}};
    if (UI::moneyInput("##option_fee", option,
                       {.maximum = std::numeric_limits<uint32_t>::max(),
                        .chips = optionChips}))
      terms.option_fee = static_cast<uint32_t>(option);
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * scale()));
    ImGui::Checkbox(LOC("TRANSFER_FIELD_OBLIGATION"), &terms.obligation);
    ImGui::SameLine();
    ImGui::Checkbox(LOC("TRANSFER_FIELD_RECALL"), &terms.recall_clause);
  };

  const auto summaryColumn = [&]
  {
    const bool panel = beginPanel("##loan_summary", LOC("TRANSFER_SUMMARY_TITLE"));
    UI::summaryRow(LOC("TRANSFER_SUMMARY_WEEKLY_SHARE"),
                   fmt::sprintf(LOC("TRANSFER_PER_WEEK"),
                                Format::moneyFull(weekly))
                       .c_str());
    UI::summaryRow(
        LOC("TRANSFER_SUMMARY_LOAN_TOTAL"),
        Format::moneyFull(weekly * weeks + static_cast<int64_t>(terms.loan_fee))
            .c_str(),
        nullptr, true);
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * scale()));
    UI::budgetImpact(LOC("TRANSFER_BUDGET_IMPACT"), available_budget,
                     available_budget - static_cast<int64_t>(terms.loan_fee),
                     LOC("TRANSFER_NOT_ENOUGH_BUDGET"));
    UI::budgetImpact(LOC("TRANSFER_WAGE_IMPACT"), wage_room,
                     wage_room - weekly, LOC("TRANSFER_WAGE_OVER_ROOM"));
    if (panel) endPanel();
    if (loan_dialog.response)
    {
      const ClubResponse& response = *loan_dialog.response;
      renderClubResponse(
          response,
          response.decision == ClubResponse::Decision::Counter
              ? fmt::sprintf(LOC("TRANSFER_LOAN_COUNTER"),
                             static_cast<int>(response.counter_wage_share),
                             response.counter_option_fee > 0
                                 ? Format::moneyFull(response.counter_option_fee)
                                 : std::string("-"))
              : std::string());
    }
  };
  renderDialogColumns(termsColumn, summaryColumn);

  ImGui::Separator();
  const bool counter =
      loan_dialog.response &&
      loan_dialog.response->decision == ClubResponse::Decision::Counter;
  alignActions({LOC("TRANSFER_CANCEL"),
                counter ? LOC("TRANSFER_MATCH_COUNTER") : nullptr,
                LOC("TRANSFER_SUBMIT_OFFER")});
  if (UI::secondaryButton(LOC("TRANSFER_CANCEL")) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  if (counter)
  {
    ImGui::SameLine();
    if (UI::secondaryButton(LOC("TRANSFER_MATCH_COUNTER")))
    {
      terms.wage_share = loan_dialog.response->counter_wage_share;
      terms.option_fee = loan_dialog.response->counter_option_fee;
    }
  }
  ImGui::SameLine();
  if (UI::primaryButton(LOC("TRANSFER_SUBMIT_OFFER")))
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
  placeDialog(std::min(Tuning::Layout::DIALOG_WIDTH * scale(),
                       ImGui::GetMainViewport()->Size.x *
                           Tuning::Layout::DIALOG_VIEWPORT_SHARE));
  const std::string title =
      std::string(LOC("TRANSFER_COUNTER_TITLE")) + "###transfer_counter";
  if (!ImGui::BeginPopupModal(
          title.c_str(), nullptr,
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    return;
  auto& controller = guiView->getController();
  const OfferRow& offer = counter_dialog.offer;
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(fmt::sprintf(LOC("TRANSFER_COUNTER_HEADER"),
                                      offer.club, offer.player,
                                      offer.amount_text)
                             .c_str());
  ImGui::PopTextWrapPos();
  formLabel(LOC("TRANSFER_NEW_PRICE"));
  int64_t fee = counter_dialog.fee;
  const std::array<UI::MoneyChip, 4> chips = {
      {{LOC("TRANSFER_CHIP_THEIR_OFFER"), offer.fee},
       {LOC("TRANSFER_CHIP_VALUE"), offer.value},
       {LOC("TRANSFER_CHIP_PLUS_10"), scaledAmount(offer.fee, 1.1)},
       {LOC("TRANSFER_CHIP_PLUS_25"), scaledAmount(offer.fee, 1.25)}}};
  if (UI::moneyInput("##counter_fee", fee,
                     {.maximum = std::numeric_limits<uint32_t>::max(),
                      .chips = chips}))
    counter_dialog.fee = static_cast<uint32_t>(fee);
  if (counter_dialog.response)
  {
    const ClubResponse& response = *counter_dialog.response;
    renderClubResponse(
        response, response.decision == ClubResponse::Decision::Counter
                      ? fmt::sprintf(LOC("TRANSFER_COUNTER_IMPROVED"),
                                     Format::moneyFull(response.counter_fee))
                      : std::string());
  }
  ImGui::Separator();
  alignActions({LOC("TRANSFER_CANCEL"), LOC("TRANSFER_SUBMIT_COUNTER")});
  if (UI::secondaryButton(LOC("TRANSFER_CANCEL")) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  ImGui::SameLine();
  ImGui::BeginDisabled(counter_dialog.fee == 0);
  if (UI::primaryButton(LOC("TRANSFER_SUBMIT_COUNTER")))
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
  placeDialog(std::min(Tuning::Layout::DIALOG_WIDTH * scale(),
                       ImGui::GetMainViewport()->Size.x *
                           Tuning::Layout::DIALOG_VIEWPORT_SHARE));
  const std::string title =
      fmt::sprintf(LOC("TRANSFER_LIST_TITLE"), listing_dialog.player) +
      "###transfer_listing";
  if (!ImGui::BeginPopupModal(
          title.c_str(), nullptr,
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    return;
  formLabel(LOC("TRANSFER_ASKING_PRICE"));
  int64_t price = listing_dialog.price;
  const std::array<UI::MoneyChip, 3> chips = {
      {{LOC("TRANSFER_CHIP_MARKET_VALUE"), listing_dialog.value},
       {LOC("TRANSFER_CHIP_PLUS_25"), scaledAmount(listing_dialog.value, 1.25)},
       {LOC("TRANSFER_CHIP_PLUS_50"),
        scaledAmount(listing_dialog.value, 1.5)}}};
  if (UI::moneyInput("##asking", price,
                     {.maximum = std::numeric_limits<uint32_t>::max(),
                      .chips = chips}))
    listing_dialog.price = static_cast<uint32_t>(price);
  ImGui::Separator();
  alignActions({LOC("TRANSFER_CANCEL"), LOC("TRANSFER_LIST_PLAYER")});
  if (UI::secondaryButton(LOC("TRANSFER_CANCEL")) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  ImGui::SameLine();
  ImGui::BeginDisabled(listing_dialog.price == 0);
  if (UI::primaryButton(LOC("TRANSFER_LIST_PLAYER")))
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
