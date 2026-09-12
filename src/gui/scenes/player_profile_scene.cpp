// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/player_profile_scene.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <format>
#include <map>

#include "database/gamedata.h"
#include "global/global.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/scenes/lineup_scene.h"
#include "gui/view_models/competition_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/injury.h"
#include "model/transfer_tuning.h"
#include "model/world_simulation.h"

namespace
{
constexpr float THREE_COLUMN_MIN_WIDTH = 900.0f;
constexpr float HEADER_HEIGHT = 112.0f;
constexpr float BIO_KEY_WIDTH = 130.0f;
constexpr float ATTRIBUTE_LABEL_WIDTH = 118.0f;
constexpr const char* LIST_CONFIRM_ID = "##confirm_transfer_list";
constexpr const char* RENEW_DIALOG_ID = "###renew_contract";

std::string nationalityName(Language nationality)
{
  const auto name = languageToString.find(nationality);
  if (name == languageToString.end()) return {};
  const std::string key = "NATION_" + name->second;
  const char* localized = LOC(key.c_str());
  // LOC returns the key itself when a translation is missing.
  return key == localized ? name->second : std::string(localized);
}

ImVec4 fitnessColor(float value)
{
  const Theme::Palette& palette = Theme::palette();
  if (value >= 85.0f) return palette.positive;
  if (value >= 65.0f) return palette.warning;
  return palette.negative;
}
}  // namespace

PlayerProfileScene::PlayerProfileScene(GUIView* parent, PlayerID playerId)
    : ManagementScene(parent), player_id(playerId)
{
}

void PlayerProfileScene::update(float /*deltaTime*/) {}

NavSection PlayerProfileScene::navSection() const
{
  return isOwnPlayer() ? NavSection::SQUAD : NavSection::NONE;
}

const Player* PlayerProfileScene::player() const
{
  const auto data = guiView->getController().getGameData();
  if (!data) return nullptr;
  const auto found = data->getPlayer(player_id);
  return found ? &found->get() : nullptr;
}

bool PlayerProfileScene::isOwnPlayer() const
{
  const auto managed = guiView->getController().getManagedTeam();
  return managed && row.team_id == managed->get().getId() && row.id != 0;
}

void PlayerProfileScene::refresh()
{
  const Player* current = player();
  sections.clear();
  fits.clear();
  season_rows.clear();
  career_rows.clear();
  if (current == nullptr) return;
  GameController& controller = guiView->getController();
  row = PlayerView::makeRow(controller, *current);
  const auto club = controller.getTeamById(row.team_id);
  club_name = row.team_id == FREE_AGENTS_TEAM_ID || !club
                  ? std::string(LOC("TRANSFER_FREE_AGENT_LABEL"))
                  : club->get().getName();
  nationality = nationalityName(current->getNationality());
  fits = PlayerView::roleFits(*current, controller.getStatsConfig());

  dynamics = current->getDynamics();
  form = current->getForm();
  league_ban = controller.getSuspensionMatches(player_id, MatchType::LEAGUE);
  cup_ban = controller.getSuspensionMatches(player_id, MatchType::CUP);
  squad_role_key = isOwnPlayer()
                       ? squadRoleKey(controller.getSquadRole(player_id))
                       : nullptr;
  const auto potential = controller.getPotentialEstimate(player_id);
  potential_low = potential.low;
  potential_high = potential.high;

  const auto& stats = current->getStats();
  std::vector<std::string_view> grouped;
  for (const PlayerView::AttributeGroup& group : PlayerView::ATTRIBUTE_GROUPS)
  {
    AttributeSection section{group.title_key, {}};
    for (const std::string_view statName : group.stats)
    {
      grouped.push_back(statName);
      if (const auto stat = stats.find(std::string(statName));
          stat != stats.end())
        section.lines.push_back(
            {PlayerView::statLabel(stat->first), stat->second});
    }
    if (!section.lines.empty()) sections.push_back(std::move(section));
  }
  // Attributes added by future systems still show up, in their own group.
  AttributeSection other{"PROFILE_GROUP_OTHER", {}};
  for (const auto& [name, value] : stats)
    if (std::ranges::find(grouped, name) == grouped.end())
      other.lines.push_back({PlayerView::statLabel(name), value});
  if (!other.lines.empty()) sections.push_back(std::move(other));

  for (const PlayerSeasonStats& season :
       controller.getPlayerSeasonStats(player_id))
    season_rows.push_back(
        {LOC(CompetitionView::matchTypeKey(season.competition_type)), season});

  // Career: one line per season and club, all competitions combined.
  std::map<std::pair<uint16_t, TeamID>, PlayerSeasonStats> bySeason;
  for (const PlayerSeasonStats& season : controller.getPlayerCareer(player_id))
  {
    PlayerSeasonStats& total = bySeason[{season.season, season.team_id}];
    total.season = season.season;
    total.team_id = season.team_id;
    total.appearances += season.appearances;
    total.starts += season.starts;
    total.minutes += season.minutes;
    total.goals += season.goals;
    total.assists += season.assists;
    total.yellow_cards += season.yellow_cards;
    total.red_cards += season.red_cards;
    total.rating_total += season.rating_total;
    total.rated_matches += season.rated_matches;
  }
  for (auto entry = bySeason.rbegin(); entry != bySeason.rend(); ++entry)
  {
    const auto team = controller.getTeamById(entry->first.second);
    career_rows.push_back(
        {fmt::sprintf(LOC("PROFILE_CAREER_SEASON"), entry->first.first,
                      team ? team->get().getName() : std::string()),
         entry->second});
  }
}

void PlayerProfileScene::renderContent()
{
  const Player* current = player();
  if (current == nullptr)
  {
    UI::emptyState(LOC("PROFILE_MISSING_TITLE"), LOC("PROFILE_MISSING_BODY"));
    return;
  }
  renderHeader();

  const float available = ImGui::GetContentRegionAvail().x;
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float spacing = ImGui::GetStyle().ItemSpacing.y;
  const float availableHeight = ImGui::GetContentRegionAvail().y;
  const float topHeight =
      std::max(320.0f * Theme::scale(), std::floor(availableHeight * 0.6f));
  const float bottomHeight =
      std::max(190.0f * Theme::scale(), availableHeight - topHeight - spacing);
  if (available >= THREE_COLUMN_MIN_WIDTH * Theme::scale())
  {
    const float side = std::floor((available - 2.0f * gap) * 0.29f);
    const float middle = available - 2.0f * gap - 2.0f * side;
    renderBio(*current, side, topHeight);
    ImGui::SameLine();
    renderAttributes(middle, topHeight);
    ImGui::SameLine();
    renderStatus(side, topHeight);
    renderStatistics(side + gap + middle, bottomHeight);
    ImGui::SameLine();
    renderSuitability(side, bottomHeight);
  }
  else
  {
    const float half = std::floor((available - gap) * 0.5f);
    renderBio(*current, half, topHeight);
    ImGui::SameLine();
    renderAttributes(available - gap - half, topHeight);
    renderStatus(half, bottomHeight);
    ImGui::SameLine();
    renderSuitability(available - gap - half, bottomHeight);
    renderStatistics(available, bottomHeight);
  }
  renderDialogs();
}

void PlayerProfileScene::renderHeader()
{
  const Theme::Palette& palette = Theme::palette();
  GameController& controller = guiView->getController();
  UI::beginCard("profile_header", nullptr,
                ImVec2(0.0f, HEADER_HEIGHT * Theme::scale()));
  const float startX = ImGui::GetCursorPosX();
  const float rightEdge = startX + ImGui::GetContentRegionAvail().x;

  // Overall and value on the right edge.
  const std::string overall = std::format("{:.0f}", row.overall);
  float overallWidth = 0.0f;
  {
    Theme::ScopedText display(Theme::Text::DISPLAY);
    overallWidth = std::max(ImGui::CalcTextSize(overall.c_str()).x,
                            ImGui::CalcTextSize("00").x);
  }
  float valueWidth = 0.0f;
  {
    Theme::ScopedText title(Theme::Text::TITLE);
    valueWidth = ImGui::CalcTextSize(row.value_text.c_str()).x;
  }
  valueWidth =
      std::max(valueWidth, ImGui::CalcTextSize(LOC("PROFILE_MARKET_VALUE")).x);
  const float blockX =
      rightEdge - overallWidth - valueWidth - Theme::Space::XL * Theme::scale();

  ImGui::BeginGroup();
  {
    Theme::ScopedText heading(Theme::Text::HEADING);
    ImGui::TextUnformatted(row.name.c_str());
  }
  UI::badge(row.role.c_str(), palette.accent);
  ImGui::SameLine();
  ImGui::TextColored(palette.muted, "%s", LOC(PlayerView::groupKey(row.group)));
  ImGui::SameLine(0.0f, Theme::Space::L * Theme::scale());
  if (row.team_id != FREE_AGENTS_TEAM_ID && !isOwnPlayer())
  {
    if (UI::link(club_name.c_str(), "profile_club"))
      Navigation::openClub(guiView, row.team_id);
  }
  else
  {
    ImGui::TextUnformatted(club_name.c_str());
  }
  ImGui::SameLine(0.0f, Theme::Space::L * Theme::scale());
  ImGui::TextColored(
      palette.muted, "%s",
      fmt::sprintf(LOC("PROFILE_AGE_NATION"), row.age, nationality.c_str())
          .c_str());
  if (row.listed)
  {
    ImGui::SameLine(0.0f, Theme::Space::L * Theme::scale());
    UI::badge(LOC("ROSTER_BADGE_LISTED"), palette.warning);
  }

  // Context actions.
  if (isOwnPlayer())
  {
    if (row.listed)
    {
      if (ImGui::SmallButton(LOC("TRANSFER_UNLIST")))
      {
        controller.removePlayerFromTransfer(player_id);
        showToast(LOC("PROFILE_UNLISTED_TOAST"));
        refresh();
      }
    }
    else if (ImGui::SmallButton(LOC("PROFILE_TRANSFER_LIST")))
    {
      list_confirm_requested = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(LOC("PROFILE_OPEN_LINEUP")))
      guiView->navigateTo(std::make_unique<LineupScene>(guiView, player_id));
    ImGui::SameLine();
    if (ImGui::SmallButton(LOC("PROFILE_RENEW")))
    {
      const auto demand = controller.getContractDemand(player_id, false);
      renew_wage = static_cast<float>(demand.weekly_wage);
      renew_years = std::max<int>(demand.years, 1);
      renew_status.clear();
      renew_requested = true;
    }
  }
  else
  {
    if (row.team_id != FREE_AGENTS_TEAM_ID)
    {
      if (ImGui::SmallButton(LOC("PROFILE_VIEW_CLUB")))
        Navigation::openClub(guiView, row.team_id);
      ImGui::SameLine();
    }
    if (ImGui::SmallButton(LOC("FINANCE_OPEN_MARKET")))
      Navigation::open(guiView, NavSection::TRANSFERS);
  }
  ImGui::EndGroup();

  const float groupRight = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x;
  ImGui::SetCursorPos(
      ImVec2(std::max(blockX, groupRight + Theme::Space::L * Theme::scale()),
             ImGui::GetStyle().WindowPadding.y));
  ImGui::BeginGroup();
  UI::sectionLabel(LOC("PROFILE_OVERALL"));
  {
    Theme::ScopedText display(Theme::Text::DISPLAY);
    ImGui::TextColored(Theme::ratingColor(row.overall), "%s", overall.c_str());
  }
  ImGui::EndGroup();
  ImGui::SameLine(0.0f, Theme::Space::XL * Theme::scale());
  ImGui::BeginGroup();
  UI::sectionLabel(LOC("PROFILE_MARKET_VALUE"));
  {
    Theme::ScopedText title(Theme::Text::TITLE);
    ImGui::TextUnformatted(row.value_text.c_str());
  }
  ImGui::EndGroup();
  UI::endCard();
}

void PlayerProfileScene::renderBio(const Player& current, float width,
                                   float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("profile_bio", LOC("PROFILE_BIO"), ImVec2(width, height), true);
  const float keyWidth = BIO_KEY_WIDTH * Theme::scale();
  const std::string age = std::to_string(current.getAge());
  const std::string heightText = std::format("{} cm", current.getHeight());
  const char* foot = current.getFoot() == Foot::Right ? LOC("PLAYER_FOOT_RIGHT")
                                                      : LOC("PLAYER_FOOT_LEFT");
  UI::keyValue(LOC("PLAYER_AGE"), age.c_str(), keyWidth);
  UI::keyValue(LOC("PROFILE_NATIONALITY"), nationality.c_str(), keyWidth);
  UI::keyValue(LOC("PROFILE_HEIGHT"), heightText.c_str(), keyWidth);
  UI::keyValue(LOC("PROFILE_FOOT"), foot, keyWidth);
  UI::keyValue(LOC("PROFILE_POSITION"), row.role.c_str(), keyWidth);
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
  UI::sectionLabel(LOC("PROFILE_CONTRACT"));
  const std::string wage =
      fmt::sprintf(LOC("PROFILE_WAGE_VALUE"), row.wage_text.c_str());
  const std::string contract =
      fmt::sprintf(LOC("PROFILE_CONTRACT_VALUE"), row.contract_years);
  UI::keyValue(LOC("PLAYER_WEEKLY_WAGE"), wage.c_str(), keyWidth);
  ImGui::PushStyleColor(
      ImGuiCol_Text, row.contract_years <= 1 ? palette.negative : palette.text);
  UI::keyValue(LOC("PLAYER_CONTRACT"), contract.c_str(), keyWidth);
  ImGui::PopStyleColor();
  UI::keyValue(LOC("PROFILE_MARKET_VALUE_LABEL"), row.value_text.c_str(),
               keyWidth);
  UI::keyValue(
      LOC("PROFILE_TRANSFER_STATUS"),
      LOC(row.listed ? "PROFILE_STATUS_LISTED" : "PROFILE_STATUS_NOT_LISTED"),
      keyWidth);
  UI::endCard();
}

void PlayerProfileScene::renderStatus(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("profile_status", LOC("PROFILE_STATUS"), ImVec2(width, height),
                true);
  // Availability first: it is what decides selection.
  if (dynamics.injury_days > 0)
  {
    UI::badge(LOC("PROFILE_INJURED"), palette.negative);
    ImGui::SameLine();
    ImGui::TextUnformatted(
        fmt::sprintf(LOC("PROFILE_INJURY_DETAIL"),
                     LOC(InjuryModel::nameKey(dynamics.injury)),
                     dynamics.injury_days)
            .c_str());
  }
  else if (league_ban > 0 || cup_ban > 0)
  {
    UI::badge(LOC("PROFILE_SUSPENDED"), palette.warning);
    ImGui::SameLine();
    ImGui::TextUnformatted(fmt::sprintf(LOC("PLAYER_SUSPENDED_MATCHES"),
                                        std::max(league_ban, cup_ban))
                               .c_str());
  }
  else
  {
    UI::badge(LOC("PROFILE_AVAILABLE"), palette.positive);
  }
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));

  const float labelWidth = ATTRIBUTE_LABEL_WIDTH * Theme::scale();
  const auto percentMeter = [labelWidth](const char* label, float value)
  {
    const std::string text = std::format("{:.0f}%", static_cast<double>(value));
    UI::meter(label, value / 100.0f, labelWidth, fitnessColor(value),
              text.c_str());
  };
  percentMeter(LOC("PROFILE_CONDITION"), dynamics.condition);
  percentMeter(LOC("PROFILE_SHARPNESS"), dynamics.sharpness);
  percentMeter(LOC("PROFILE_MORALE"), dynamics.morale);

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
  const float keyWidth = BIO_KEY_WIDTH * Theme::scale();
  const std::string formText =
      dynamics.rating_count > 0
          ? fmt::sprintf(LOC("PROFILE_FORM_VALUE"), static_cast<double>(form),
                         dynamics.rating_count)
          : std::string(LOC("PROFILE_FORM_NONE"));
  UI::keyValue(LOC("PROFILE_FORM"), formText.c_str(), keyWidth);
  if (squad_role_key != nullptr)
    UI::keyValue(LOC("PROFILE_SQUAD_ROLE"), LOC(squad_role_key), keyWidth);
  if (potential_high > 0.0f)
  {
    const std::string range =
        std::format("{:.0f} – {:.0f}", static_cast<double>(potential_low),
                    static_cast<double>(potential_high));
    ImGui::PushStyleColor(
        ImGuiCol_Text,
        Theme::ratingColor(static_cast<double>(potential_low + potential_high) *
                           0.5));
    UI::keyValue(LOC("PROFILE_POTENTIAL"), range.c_str(), keyWidth);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", LOC(isOwnPlayer() ? "PROFILE_POTENTIAL_OWN"
                                                : "PROFILE_POTENTIAL_SCOUTED"));
  }
  UI::endCard();
}

void PlayerProfileScene::renderAttributes(float width, float height)
{
  UI::beginCard("profile_attributes", LOC("PROFILE_ATTRIBUTES"),
                ImVec2(width, height), true);
  const float labelWidth = ATTRIBUTE_LABEL_WIDTH * Theme::scale();
  for (const AttributeSection& section : sections)
  {
    {
      Theme::ScopedText title(Theme::Text::SMALL);
      ImGui::TextUnformatted(LOC(section.title_key));
    }
    for (const AttributeLine& line : section.lines)
      UI::attributeBar(line.name.c_str(), line.value, labelWidth);
    ImGui::Dummy(ImVec2(0.0f, 2.0f * Theme::scale()));
  }
  UI::endCard();
}

void PlayerProfileScene::renderSuitability(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("profile_fit", LOC("PROFILE_ROLE_FIT"), ImVec2(width, height),
                true);
  const float labelWidth = ATTRIBUTE_LABEL_WIDTH * Theme::scale();
  for (const PlayerView::RoleFit& fit : fits)
    UI::attributeBar(LOC(PlayerView::groupKey(fit.group)), fit.rating,
                     labelWidth);
  const std::string natural = fmt::sprintf(
      LOC("PROFILE_NATURAL_POSITION"), LOC(PlayerView::groupKey(row.group)));
  ImGui::TextColored(palette.faint, "%s", natural.c_str());
  UI::endCard();
}

void PlayerProfileScene::renderStatistics(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("profile_stats", nullptr, ImVec2(width, height), true);
  UI::sectionLabel(
      LOC(show_career ? "PROFILE_CAREER" : "PROFILE_SEASON_STATS"));
  ImGui::SameLine();
  const char* toggleLabel =
      LOC(show_career ? "PROFILE_SHOW_SEASON" : "PROFILE_SHOW_CAREER");
  const float toggleWidth = ImGui::CalcTextSize(toggleLabel).x +
                            ImGui::GetStyle().FramePadding.x * 2.0f;
  ImGui::SetCursorPosX(
      std::max(ImGui::GetCursorPosX(),
               ImGui::GetWindowContentRegionMax().x - toggleWidth));
  if (ImGui::SmallButton(toggleLabel)) show_career = !show_career;

  const std::vector<StatsRow>& rows = show_career ? career_rows : season_rows;
  if (rows.empty())
  {
    ImGui::TextColored(palette.faint, "%s", LOC("PROFILE_NO_STATS"));
    UI::endCard();
    return;
  }
  if (UI::beginDataTable("profile_stats_table", 8,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                             ImGuiTableFlags_ScrollY |
                             ImGuiTableFlags_SizingFixedFit,
                         560.0f, ImVec2(0.0f, 0.0f), 1))
  {
    ImGui::TableSetupColumn(
        LOC(show_career ? "CLUB_COL_SEASON" : "CLUB_COL_COMPETITION"),
        ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(LOC("STATS_COL_APPS"));
    ImGui::TableSetupColumn(LOC("STATS_COL_MINUTES"));
    ImGui::TableSetupColumn(LOC("STATS_COL_GOALS"));
    ImGui::TableSetupColumn(LOC("STATS_COL_ASSISTS"));
    ImGui::TableSetupColumn(LOC("STATS_COL_YELLOW"));
    ImGui::TableSetupColumn(LOC("STATS_COL_RED"));
    ImGui::TableSetupColumn(LOC("STATS_COL_RATING"));
    ImGui::TableHeadersRow();
    for (const StatsRow& line : rows)
    {
      const PlayerSeasonStats& stats = line.stats;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(line.label.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%u (%u)", stats.appearances, stats.starts);
      ImGui::TableNextColumn();
      ImGui::Text("%u", stats.minutes);
      ImGui::TableNextColumn();
      ImGui::Text("%u", stats.goals);
      ImGui::TableNextColumn();
      ImGui::Text("%u", stats.assists);
      ImGui::TableNextColumn();
      ImGui::TextColored(
          stats.yellow_cards > 0 ? palette.warning : palette.muted, "%u",
          stats.yellow_cards);
      ImGui::TableNextColumn();
      ImGui::TextColored(stats.red_cards > 0 ? palette.negative : palette.muted,
                         "%u", stats.red_cards);
      ImGui::TableNextColumn();
      if (stats.rated_matches > 0)
        ImGui::Text("%.2f", static_cast<double>(stats.averageRating()));
      else
        ImGui::TextColored(palette.faint, "–");
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void PlayerProfileScene::renderDialogs()
{
  GameController& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  if (list_confirm_requested)
  {
    list_confirm_requested = false;
    ImGui::OpenPopup(LIST_CONFIRM_ID);
  }
  if (ImGui::IsPopupOpen(LIST_CONFIRM_ID))
  {
    const std::string body =
        fmt::sprintf(LOC("PROFILE_LIST_CONFIRM_BODY"), row.name.c_str(),
                     Format::moneyFull(row.market_value).c_str());
    if (UI::confirmDialog(LIST_CONFIRM_ID, LOC("PROFILE_TRANSFER_LIST"),
                          body.c_str(), LOC("PROFILE_TRANSFER_LIST"),
                          LOC("TRANSFER_CANCEL")) == UI::DialogResult::CONFIRM)
    {
      controller.listPlayerForTransfer(row.id, row.market_value);
      showToast(LOC("PROFILE_LISTED_TOAST"));
      refresh();
    }
  }

  if (renew_requested)
  {
    renew_requested = false;
    ImGui::OpenPopup(RENEW_DIALOG_ID);
  }
  if (!ImGui::IsPopupOpen(RENEW_DIALOG_ID)) return;
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  const std::string title =
      std::string(LOC("PROFILE_RENEW_TITLE")) + RENEW_DIALOG_ID;
  if (!ImGui::BeginPopupModal(title.c_str(), nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize))
    return;
  const auto demand = controller.getContractDemand(player_id, false);
  ImGui::TextColored(palette.muted, "%s",
                     fmt::sprintf(LOC("TRANSFER_PLAYER_DEMANDS"),
                                  demand.weekly_wage, demand.years)
                         .c_str());
  ImGui::SetNextItemWidth(240.0f * Theme::scale());
  ImGui::InputFloat(LOC("TRANSFER_OFFER_WAGE"), &renew_wage, 100.0f, 1000.0f,
                    "%.0f");
  renew_wage = std::max(0.0f, renew_wage);
  ImGui::SetNextItemWidth(240.0f * Theme::scale());
  ImGui::SliderInt(LOC("TRANSFER_OFFER_YEARS"), &renew_years,
                   TransferTuning::Contract::MINIMUM_YEARS,
                   TransferTuning::Contract::MAXIMUM_YEARS);
  const GameController::ContractTerms offer{static_cast<uint32_t>(renew_wage),
                                            static_cast<uint8_t>(renew_years)};
  const bool acceptable =
      controller.isContractOfferAcceptable(player_id, false, offer);
  if (!acceptable)
    ImGui::TextColored(palette.negative, "%s",
                       LOC("TRANSFER_CONTRACT_REJECTED"));
  if (!renew_status.empty())
    ImGui::TextColored(palette.negative, "%s", renew_status.c_str());
  const ImVec2 buttonSize(150.0f * Theme::scale(), 0.0f);
  if (ImGui::Button(LOC("TRANSFER_CANCEL"), buttonSize) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    ImGui::CloseCurrentPopup();
  ImGui::SameLine();
  ImGui::BeginDisabled(!acceptable);
  if (UI::primaryButton(LOC("PROFILE_RENEW_CONFIRM"), buttonSize))
  {
    if (controller.renewContract(player_id, offer))
    {
      showToast(LOC("PROFILE_RENEWED_TOAST"));
      refresh();
      ImGui::CloseCurrentPopup();
    }
    else
    {
      renew_status = LOC("PROFILE_RENEW_OVER_BUDGET");
    }
  }
  ImGui::EndDisabled();
  ImGui::EndPopup();
}
