// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/staff_scene.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/inbox.h"

namespace
{
constexpr float TWO_COLUMN_MIN_WIDTH = 1000.0f;
constexpr float TOP_CARD_HEIGHT = 330.0f;
constexpr float KEY_WIDTH = 150.0f;
constexpr const char* RELEASE_POPUP_ID = "##release_staff";
constexpr int MAX_HIRE_YEARS = 4;
constexpr int MAX_CONTRACT_YEARS = 5;

float dpi() { return ImGui::GetStyle().FontScaleDpi; }

std::string keyAttributes(const StaffMember& member)
{
  const auto keys = StaffModel::keyAttributes(member.role);
  return std::format("{} {}  ·  {} {}", LOC(StaffModel::attributeKey(keys[0])),
                     member.attribute(keys[0]),
                     LOC(StaffModel::attributeKey(keys[1])),
                     member.attribute(keys[1]));
}

const char* resultKey(GameController::StaffActionResult result)
{
  using Result = GameController::StaffActionResult;
  switch (result)
  {
    case Result::RoleFull:
      return "STAFF_RESULT_ROLE_FULL";
    case Result::CannotAfford:
      return "STAFF_RESULT_CANNOT_AFFORD";
    case Result::InvalidTerms:
      return "STAFF_RESULT_INVALID_TERMS";
    case Result::NotAvailable:
    case Result::UnknownStaff:
      return "STAFF_RESULT_NOT_AVAILABLE";
    case Result::NoClub:
    case Result::Ok:
      break;
  }
  return "STAFF_RESULT_NO_CLUB";
}
}  // namespace

StaffScene::StaffScene(GUIView* parent) : ManagementScene(parent) {}

void StaffScene::update(float /*deltaTime*/) {}

void StaffScene::refresh()
{
  GameController& controller = guiView->getController();
  staff_rows.clear();
  responsibilities.clear();
  const auto managed = controller.getManagedTeam();
  if (!managed)
  {
    market_rows.clear();
    return;
  }
  const TeamID team_id = managed->get().getId();
  effects = controller.getStaffEffects(team_id);
  const auto staff = controller.getStaff(team_id);
  for (const StaffMember* member : staff)
  {
    StaffRow row;
    row.id = member->id;
    row.role = member->role;
    row.name = member->name();
    row.attributes = keyAttributes(*member);
    row.age = member->age;
    row.rating = StaffModel::rating(*member);
    row.wage = Format::money(member->wage);
    row.demand = Format::money(controller.getStaffWageDemand(member->id));
    row.contract_years = member->contract_years;
    row.contract =
        member->contract_years == 1
            ? std::string(LOC("STAFF_CONTRACT_ONE_SEASON"))
            : formatLocalized("STAFF_CONTRACT_YEARS",
                              {std::to_string(member->contract_years)});
    row.extend_help = formatLocalized("STAFF_EXTEND_HELP", {row.demand});
    staff_rows.push_back(std::move(row));
  }

  // Who covers each area (the best specialist, else the assistant).
  const auto holder = [&](StaffRole role) -> std::string
  {
    for (const StaffMember* member : staff)
    {
      if (member->role == role) return member->name();
    }
    for (const StaffMember* member : staff)
    {
      if (member->role == StaffRole::AssistantManager)
        return std::format("{} ({})", member->name(), LOC("STAFF_COVERING"));
    }
    return LOC("STAFF_VACANT");
  };
  responsibilities = {
      {"STAFF_AREA_TACTICS", effects.coaching_tactical,
       holder(StaffRole::AssistantManager)},
      {"STAFF_AREA_FITNESS", effects.coaching_fitness,
       holder(StaffRole::FitnessCoach)},
      {"STAFF_AREA_ATTACKING", effects.coaching_attacking,
       holder(StaffRole::AttackingCoach)},
      {"STAFF_AREA_DEFENDING", effects.coaching_defending,
       holder(StaffRole::DefendingCoach)},
      {"STAFF_AREA_GOALKEEPING", effects.coaching_goalkeeping,
       holder(StaffRole::GoalkeepingCoach)},
      {"STAFF_AREA_YOUTH", effects.coaching_youth,
       holder(StaffRole::YouthCoach)},
      {"STAFF_AREA_MEDICAL", (1.12f - effects.layoff_multiplier) / 0.27f,
       holder(StaffRole::Physio)},
      {"STAFF_AREA_SCIENCE", (1.08f - effects.injury_prevention) / 0.20f,
       holder(StaffRole::SportsScientist)},
      {"STAFF_AREA_SCOUTING", effects.scouting_accuracy,
       holder(StaffRole::Scout)},
  };
  rebuildMarket();
}

void StaffScene::rebuildMarket()
{
  GameController& controller = guiView->getController();
  market_rows.clear();
  const auto managed = controller.getManagedTeam();
  if (!managed) return;
  // Occupied posts per role, counted once.
  std::array<std::size_t, STAFF_ROLE_COUNT> employed{};
  for (const StaffMember* member : controller.getStaff(managed->get().getId()))
    ++employed[static_cast<std::size_t>(member->role)];
  for (const StaffMember* member : controller.getStaffMarket())
  {
    if (role_filter >= 0 && static_cast<int>(member->role) != role_filter)
      continue;
    const int rating = StaffModel::rating(*member);
    if (rating < min_rating) continue;
    StaffRow row;
    row.id = member->id;
    row.role = member->role;
    row.name = member->name();
    row.attributes = keyAttributes(*member);
    row.age = member->age;
    row.rating = rating;
    row.demand = Format::money(controller.getStaffWageDemand(member->id));
    const auto role = static_cast<std::size_t>(member->role);
    row.role_full = employed[role] >= StaffModel::ROLE_LIMITS[role];
    market_rows.push_back(std::move(row));
  }
}

void StaffScene::showResult(int result, const char* success_key)
{
  const auto value = static_cast<GameController::StaffActionResult>(result);
  if (value == GameController::StaffActionResult::Ok)
  {
    showToast(LOC(success_key));
    refresh();
  }
  else
  {
    showToast(LOC(resultKey(value)), true);
  }
}

void StaffScene::renderContent()
{
  const auto managed = guiView->getController().getManagedTeam();
  if (!managed) return;
  const Theme::Palette& palette = Theme::palette();
  UI::pageHeader(LOC("STAFF_TITLE"), LOC("STAFF_SUBTITLE"));

  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float tile = (ImGui::GetContentRegionAvail().x - 3.0f * gap) / 4.0f;
  const std::string payroll = Format::money(effects.weekly_payroll);
  const std::string payroll_note = formatLocalized(
      "STAFF_TILE_PAYROLL_NOTE", {std::to_string(staff_rows.size())});
  UI::statTile("payroll", LOC("STAFF_TILE_PAYROLL"), payroll.c_str(),
               payroll_note.c_str(), palette.text, tile);
  ImGui::SameLine();
  const std::string coaching =
      std::format("{:.0f}", effects.training_quality * 100.0f);
  UI::statTile("coaching", LOC("STAFF_TILE_COACHING"), coaching.c_str(),
               LOC("STAFF_TILE_COACHING_NOTE"),
               Theme::ratingColor(effects.training_quality * 100.0f), tile);
  ImGui::SameLine();
  const std::string layoffs =
      std::format("{:+.0f}%", (effects.layoff_multiplier - 1.0f) * 100.0f);
  UI::statTile(
      "medical", LOC("STAFF_TILE_MEDICAL"), layoffs.c_str(),
      LOC("STAFF_TILE_MEDICAL_NOTE"),
      effects.layoff_multiplier <= 1.0f ? palette.positive : palette.negative,
      tile);
  ImGui::SameLine();
  const std::string scouting =
      std::format("{:.0f}%", effects.scouting_accuracy * 100.0f);
  UI::statTile("scouting", LOC("STAFF_TILE_SCOUTING"), scouting.c_str(),
               LOC("STAFF_TILE_SCOUTING_NOTE"),
               Theme::ratingColor(effects.scouting_accuracy * 100.0f), tile);

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * dpi()));
  const float available = ImGui::GetContentRegionAvail().x;
  const bool twoColumns = available >= TWO_COLUMN_MIN_WIDTH * dpi();
  const float height = TOP_CARD_HEIGHT * dpi();
  const float left =
      twoColumns ? std::floor((available - gap) * 0.62f) : available;
  renderStaff(left, height);
  if (twoColumns) ImGui::SameLine();
  renderImpact(twoColumns ? available - gap - left : available, height);
  renderMarket(std::max(ImGui::GetContentRegionAvail().y, 260.0f * dpi()));
  renderReleaseConfirm();

  // Actions run after the tables so the rows are never rebuilt mid-draw.
  GameController& controller = guiView->getController();
  switch (pending.kind)
  {
    case PendingAction::Kind::HIRE:
      showResult(
          static_cast<int>(controller.hireStaff(pending.id, pending.years)),
          "STAFF_HIRED_TOAST");
      break;
    case PendingAction::Kind::EXTEND:
      showResult(static_cast<int>(
                     controller.extendStaffContract(pending.id, pending.years)),
                 "STAFF_EXTENDED_TOAST");
      break;
    case PendingAction::Kind::RELEASE:
      showResult(static_cast<int>(controller.releaseStaff(pending.id)),
                 "STAFF_RELEASED_TOAST");
      break;
    case PendingAction::Kind::NONE:
      break;
  }
  pending = {};
}

void StaffScene::renderStaff(float width, float height)
{
  GameController& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("staff_current", LOC("STAFF_CURRENT"), ImVec2(width, height));
  if (staff_rows.empty())
  {
    UI::emptyState(LOC("STAFF_EMPTY_TITLE"), LOC("STAFF_EMPTY_BODY"));
    UI::endCard();
    return;
  }
  if (UI::beginDataTable("staff_table", 7,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                             ImGuiTableFlags_ScrollY |
                             ImGuiTableFlags_SizingFixedFit,
                         760.0f, ImVec2(0.0f, 0.0f), 1))
  {
    ImGui::TableSetupColumn(LOC("STAFF_COL_NAME"),
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(LOC("STAFF_COL_ROLE"));
    ImGui::TableSetupColumn(LOC("STAFF_COL_AGE"));
    ImGui::TableSetupColumn(LOC("STAFF_COL_RATING"));
    ImGui::TableSetupColumn(LOC("STAFF_COL_WAGE"));
    ImGui::TableSetupColumn(LOC("STAFF_COL_CONTRACT"));
    ImGui::TableSetupColumn(LOC("STAFF_COL_ACTIONS"));
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableHeadersRow();
    for (const StaffRow& row : staff_rows)
    {
      ImGui::PushID(static_cast<int>(row.id));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.name.c_str());
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\n%s", row.attributes.c_str(),
                          LOC(StaffModel::roleDescriptionKey(row.role)));
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%s",
                         LOC(StaffModel::roleKey(row.role)));
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.age);
      ImGui::TableNextColumn();
      UI::ratingChip(row.rating);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.wage.c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(
          row.contract_years <= 1 ? palette.warning : palette.text, "%s",
          row.contract.c_str());
      ImGui::TableNextColumn();
      const int extended = row.contract_years + 1;
      ImGui::BeginDisabled(extended > MAX_CONTRACT_YEARS);
      if (ImGui::SmallButton(LOC("STAFF_EXTEND")))
        pending = {PendingAction::Kind::EXTEND, row.id,
                   static_cast<uint8_t>(extended)};
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", row.extend_help.c_str());
      ImGui::SameLine();
      if (ImGui::SmallButton(LOC("STAFF_RELEASE")))
      {
        release_candidate = row.id;
        const auto data = controller.getGameData();
        const StaffMember* member =
            data ? data->getStaff().find(row.id) : nullptr;
        release_text = formatLocalized(
            "STAFF_RELEASE_BODY",
            {row.name,
             Format::money(member ? StaffModel::severance(*member) : 0)});
        release_requested = true;
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void StaffScene::renderImpact(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("staff_impact", LOC("STAFF_IMPACT"), ImVec2(width, height),
                true);
  const float keyWidth = KEY_WIDTH * dpi();
  for (const Responsibility& item : responsibilities)
  {
    const float quality = std::clamp(item.quality, 0.0f, 1.0f);
    const std::string value = std::format("{:.0f}", quality * 100.0f);
    UI::meter(LOC(item.area_key), quality, keyWidth,
              Theme::ratingColor(quality * 100.0f), value.c_str());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + keyWidth);
    ImGui::TextColored(palette.faint, "%s", item.holder.c_str());
  }
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * dpi()));
  UI::sectionLabel(LOC("STAFF_EFFECTS"));
  const std::string risk =
      std::format("x{:.2f}", static_cast<double>(effects.injury_prevention));
  UI::keyValue(LOC("STAFF_EFFECT_INJURY_RISK"), risk.c_str(), keyWidth);
  const std::string youth = std::format(
      "{:+.1f}", static_cast<double>(effects.youth_potential_bonus));
  UI::keyValue(LOC("STAFF_EFFECT_YOUTH"), youth.c_str(), keyWidth);
  const std::string familiarity =
      std::format("x{:.2f}", static_cast<double>(effects.familiarity_rate));
  UI::keyValue(LOC("STAFF_EFFECT_FAMILIARITY"), familiarity.c_str(), keyWidth);
  UI::endCard();
}

void StaffScene::renderMarket(float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("staff_market", LOC("STAFF_MARKET"), ImVec2(0.0f, height));

  bool filters_changed = false;
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(palette.muted, "%s", LOC("STAFF_FILTER_ROLE"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(200.0f * dpi());
  const char* role_label =
      role_filter < 0
          ? LOC("STAFF_FILTER_ALL")
          : LOC(StaffModel::roleKey(static_cast<StaffRole>(role_filter)));
  if (ImGui::BeginCombo("##role_filter", role_label))
  {
    if (ImGui::Selectable(LOC("STAFF_FILTER_ALL"), role_filter < 0))
    {
      filters_changed = role_filter >= 0;
      role_filter = -1;
    }
    for (int role = 0; role < static_cast<int>(STAFF_ROLE_COUNT); ++role)
    {
      if (ImGui::Selectable(
              LOC(StaffModel::roleKey(static_cast<StaffRole>(role))),
              role == role_filter))
      {
        filters_changed = role != role_filter;
        role_filter = role;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine(0.0f, Theme::Space::L * dpi());
  ImGui::TextColored(palette.muted, "%s", LOC("STAFF_FILTER_RATING"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(160.0f * dpi());
  ImGui::SliderInt("##min_rating", &min_rating, 0, 90);
  filters_changed |= ImGui::IsItemDeactivatedAfterEdit();
  ImGui::SameLine(0.0f, Theme::Space::L * dpi());
  ImGui::TextColored(palette.muted, "%s", LOC("STAFF_HIRE_YEARS"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(110.0f * dpi());
  ImGui::SliderInt("##hire_years", &hire_years, 1, MAX_HIRE_YEARS);
  if (filters_changed) rebuildMarket();

  if (market_rows.empty())
  {
    UI::emptyState(LOC("STAFF_MARKET_EMPTY_TITLE"),
                   LOC("STAFF_MARKET_EMPTY_BODY"));
    UI::endCard();
    return;
  }
  if (UI::beginDataTable("market_table", 7,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                             ImGuiTableFlags_ScrollY |
                             ImGuiTableFlags_SizingFixedFit,
                         820.0f, ImVec2(0.0f, 0.0f), 1))
  {
    ImGui::TableSetupColumn(LOC("STAFF_COL_NAME"),
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(LOC("STAFF_COL_ROLE"));
    ImGui::TableSetupColumn(LOC("STAFF_COL_AGE"));
    ImGui::TableSetupColumn(LOC("STAFF_COL_RATING"));
    ImGui::TableSetupColumn(LOC("STAFF_COL_ATTRIBUTES"));
    ImGui::TableSetupColumn(LOC("STAFF_COL_DEMAND"));
    ImGui::TableSetupColumn("");
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(market_rows.size()));
    while (clipper.Step())
    {
      for (int index = clipper.DisplayStart; index < clipper.DisplayEnd;
           ++index)
      {
        const StaffRow& row = market_rows[static_cast<std::size_t>(index)];
        ImGui::PushID(static_cast<int>(row.id));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(row.name.c_str());
        ImGui::TableNextColumn();
        ImGui::TextColored(palette.muted, "%s",
                           LOC(StaffModel::roleKey(row.role)));
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s",
                            LOC(StaffModel::roleDescriptionKey(row.role)));
        ImGui::TableNextColumn();
        ImGui::Text("%d", row.age);
        ImGui::TableNextColumn();
        UI::ratingChip(row.rating);
        ImGui::TableNextColumn();
        ImGui::TextColored(palette.muted, "%s", row.attributes.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(row.demand.c_str());
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(row.role_full);
        if (ImGui::SmallButton(LOC("STAFF_HIRE")))
          pending = {PendingAction::Kind::HIRE, row.id,
                     static_cast<uint8_t>(hire_years)};
        ImGui::EndDisabled();
        if (row.role_full &&
            ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
          ImGui::SetTooltip("%s", LOC("STAFF_RESULT_ROLE_FULL"));
        ImGui::PopID();
      }
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void StaffScene::renderReleaseConfirm()
{
  if (release_requested)
  {
    release_requested = false;
    ImGui::OpenPopup(RELEASE_POPUP_ID);
  }
  const UI::DialogResult result = UI::confirmDialog(
      RELEASE_POPUP_ID, LOC("STAFF_RELEASE_TITLE"), release_text.c_str(),
      LOC("STAFF_RELEASE"), LOC("SETTINGS_CANCEL"));
  if (result == UI::DialogResult::CONFIRM && release_candidate != 0)
  {
    pending = {PendingAction::Kind::RELEASE, release_candidate, 0};
    release_candidate = 0;
  }
  else if (result == UI::DialogResult::CANCEL)
  {
    release_candidate = 0;
  }
}
