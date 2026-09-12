// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/management_scene.h"

#include <imgui.h>

#include <algorithm>
#include <format>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/scenes/club_scene.h"
#include "gui/scenes/fixtures_scene.h"
#include "gui/scenes/inbox_scene.h"
#include "gui/scenes/lineup_scene.h"
#include "gui/scenes/main_game_scene.h"
#include "gui/scenes/main_menu_scene.h"
#include "gui/scenes/match_report_scene.h"
#include "gui/scenes/player_profile_scene.h"
#include "gui/scenes/roster_scene.h"
#include "gui/scenes/scouting_scene.h"
#include "gui/scenes/settings_scene.h"
#include "gui/scenes/staff_scene.h"
#include "gui/scenes/standings_scene.h"
#include "gui/scenes/strategy_scene.h"
#include "gui/scenes/training_scene.h"
#include "gui/scenes/transfer_market_scene.h"
#include "gui/view_models/player_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/icons.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/role_utils.h"

namespace
{
constexpr float SIDEBAR_WIDTH = 228.0f;
constexpr float SIDEBAR_COLLAPSED_WIDTH = 60.0f;
constexpr float SIDEBAR_COLLAPSE_BELOW = 1150.0f;
constexpr float ICON_SIZE = 16.0f;
constexpr float TOP_BAR_HEIGHT = 60.0f;
constexpr float NAV_ITEM_HEIGHT = 34.0f;
/** Items shrink down to this height before the navigation has to scroll. */
constexpr float NAV_ITEM_MIN_HEIGHT = 24.0f;
constexpr float CLUB_BADGE_SIZE = 38.0f;
constexpr float CONTINUE_MIN_WIDTH = 200.0f;
constexpr float PALETTE_WIDTH = 620.0f;
constexpr float PALETTE_TOP_OFFSET = 70.0f;
constexpr size_t PALETTE_MAX_RESULTS = 14;
constexpr float TOAST_SECONDS = 3.5f;
constexpr const char* PALETTE_POPUP_ID = "##command_palette";
constexpr const char* MAIN_MENU_CONFIRM_ID = "##confirm_main_menu";

struct NavEntry
{
  NavSection section;
  const char* label_key;
  const char* shortcut;
  ImGuiKey key;
  UI::Icon icon;
};

struct NavGroup
{
  const char* title_key;
  std::array<const NavEntry*, 4> entries;
};

// clang-format off
constexpr NavEntry NAV_HOME{NavSection::HOME, "NAV_HOME", "F1", ImGuiKey_F1, UI::Icon::HOME};
constexpr NavEntry NAV_INBOX{NavSection::INBOX, "NAV_INBOX", "F2", ImGuiKey_F2, UI::Icon::INBOX};
constexpr NavEntry NAV_SQUAD{NavSection::SQUAD, "NAV_SQUAD", "F3", ImGuiKey_F3, UI::Icon::SQUAD};
constexpr NavEntry NAV_LINEUP{NavSection::LINEUP, "NAV_LINEUP", "F4", ImGuiKey_F4, UI::Icon::LINEUP};
constexpr NavEntry NAV_TACTICS{NavSection::TACTICS, "NAV_TACTICS", "F5", ImGuiKey_F5, UI::Icon::TACTICS};
constexpr NavEntry NAV_FIXTURES{NavSection::FIXTURES, "NAV_FIXTURES", "F6", ImGuiKey_F6, UI::Icon::FIXTURES};
constexpr NavEntry NAV_STANDINGS{NavSection::STANDINGS, "NAV_STANDINGS", "F7", ImGuiKey_F7, UI::Icon::STANDINGS};
constexpr NavEntry NAV_TRANSFERS{NavSection::TRANSFERS, "NAV_TRANSFERS", "F8", ImGuiKey_F8, UI::Icon::TRANSFERS};
constexpr NavEntry NAV_FINANCES{NavSection::FINANCES, "NAV_FINANCES", "F9", ImGuiKey_F9, UI::Icon::FINANCES};
constexpr NavEntry NAV_CLUB{NavSection::CLUB, "NAV_CLUB", "F10", ImGuiKey_F10, UI::Icon::CLUB};
constexpr NavEntry NAV_SCOUTING{NavSection::SCOUTING, "NAV_SCOUTING", "F11", ImGuiKey_F11, UI::Icon::SEARCH};
constexpr NavEntry NAV_TRAINING{NavSection::TRAINING, "NAV_TRAINING", nullptr, ImGuiKey_None, UI::Icon::FIXTURES};
constexpr NavEntry NAV_STAFF{NavSection::STAFF, "NAV_STAFF", nullptr, ImGuiKey_None, UI::Icon::SQUAD};
// clang-format on

constexpr std::array<const NavEntry*, 13> ALL_NAV = {
    &NAV_HOME,     &NAV_INBOX,     &NAV_SQUAD,     &NAV_LINEUP,   &NAV_TACTICS,
    &NAV_FIXTURES, &NAV_STANDINGS, &NAV_TRANSFERS, &NAV_FINANCES, &NAV_CLUB,
    &NAV_SCOUTING, &NAV_TRAINING,  &NAV_STAFF};

constexpr std::array<NavGroup, 4> NAV_GROUPS = {{
    {"NAV_GROUP_CLUB", {&NAV_HOME, &NAV_INBOX, &NAV_FINANCES, &NAV_CLUB}},
    {"NAV_GROUP_TEAM", {&NAV_SQUAD, &NAV_LINEUP, &NAV_TACTICS, &NAV_TRAINING}},
    {"NAV_GROUP_COMPETITION",
     {&NAV_FIXTURES, &NAV_STANDINGS, nullptr, nullptr}},
    {"NAV_GROUP_RECRUITMENT",
     {&NAV_TRANSFERS, &NAV_SCOUTING, &NAV_STAFF, nullptr}},
}};

MainGameScene* careerHub(GUIView* view)
{
  return dynamic_cast<MainGameScene*>(view->getBaseScene());
}

bool navItem(const char* label, const char* shortcut, bool selected,
             UI::Icon icon, bool collapsed, float height, size_t badge = 0)
{
  const Theme::Palette& palette = Theme::palette();
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const ImVec2 size(ImGui::GetContentRegionAvail().x, height);
  ImGui::PushID(label);
  const bool pressed = ImGui::InvisibleButton("##nav", size);
  ImGui::PopID();
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const ImVec2 end(start.x + size.x, start.y + size.y);
  if (selected)
  {
    drawList->AddRectFilled(start, end, Theme::toU32(palette.accent, 0.16f),
                            4.0f * Theme::scale());
    drawList->AddRectFilled(
        start, ImVec2(start.x + 3.0f * Theme::scale(), end.y),
        Theme::toU32(palette.accent), 2.0f * Theme::scale());
  }
  else if (hovered)
  {
    drawList->AddRectFilled(start, end, Theme::toU32(palette.raised),
                            4.0f * Theme::scale());
  }
  const ImU32 color =
      Theme::toU32(selected || hovered ? palette.text : palette.muted);
  const float iconSize = ICON_SIZE * Theme::scale();
  const float iconX =
      collapsed ? start.x + size.x * 0.5f
                : start.x + Theme::Space::M * Theme::scale() + iconSize * 0.5f;
  UI::drawIcon(drawList, icon, ImVec2(iconX, start.y + size.y * 0.5f), iconSize,
               selected ? Theme::toU32(palette.accent) : color);
  if (badge > 0)
  {
    // Unread count pill (top-right of the icon when collapsed).
    ImGui::PushFont(nullptr, Theme::textSize(Theme::Text::CAPTION));
    const std::string count = badge > 99 ? "99+" : std::to_string(badge);
    const ImVec2 textSize = ImGui::CalcTextSize(count.c_str());
    const float pillWidth =
        std::max(textSize.x + 8.0f * Theme::scale(), 18.0f * Theme::scale());
    const float pillHeight = textSize.y + 2.0f * Theme::scale();
    const ImVec2 pillMin =
        collapsed ? ImVec2(iconX + 2.0f * Theme::scale(),
                           start.y + 3.0f * Theme::scale())
                  : ImVec2(end.x - pillWidth - Theme::Space::S * Theme::scale(),
                           start.y + (size.y - pillHeight) * 0.5f);
    drawList->AddRectFilled(
        pillMin, ImVec2(pillMin.x + pillWidth, pillMin.y + pillHeight),
        Theme::toU32(palette.accent), pillHeight * 0.5f);
    drawList->AddText(ImVec2(pillMin.x + (pillWidth - textSize.x) * 0.5f,
                             pillMin.y + 1.0f * Theme::scale()),
                      Theme::toU32(palette.on_accent), count.c_str());
    ImGui::PopFont();
    shortcut = nullptr;
  }
  if (collapsed)
  {
    if (hovered)
      ImGui::SetTooltip("%s%s%s", label, shortcut ? "   " : "",
                        shortcut ? shortcut : "");
    return pressed;
  }
  const float textY = start.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f;
  const float textX =
      iconX + iconSize * 0.5f + Theme::Space::S * Theme::scale();
  float labelRight = end.x - Theme::Space::S * Theme::scale();
  if (shortcut != nullptr)
  {
    ImGui::PushFont(nullptr, Theme::textSize(Theme::Text::CAPTION));
    const ImVec2 hintSize = ImGui::CalcTextSize(shortcut);
    labelRight -= hintSize.x + Theme::Space::S * Theme::scale();
    drawList->AddText(
        ImVec2(end.x - hintSize.x - Theme::Space::S * Theme::scale(),
               start.y + (size.y - hintSize.y) * 0.5f),
        Theme::toU32(palette.faint), shortcut);
    ImGui::PopFont();
  }
  if (UI::drawTextFitted(drawList, ImVec2(textX, textY), color, label,
                         labelRight - textX) &&
      hovered)
    ImGui::SetTooltip("%s", label);
  return pressed;
}

std::string clubInitials(const std::string& name)
{
  std::string initials;
  bool atWordStart = true;
  for (const char character : name)
  {
    if (character == ' ')
    {
      atWordStart = true;
      continue;
    }
    if (atWordStart && initials.size() < 2) initials.push_back(character);
    atWordStart = false;
  }
  return initials.empty() ? std::string("?") : initials;
}

int paletteHistoryCallback(ImGuiInputTextCallbackData* data)
{
  auto* selection = static_cast<int*>(data->UserData);
  if (data->EventKey == ImGuiKey_UpArrow)
    --*selection;
  else if (data->EventKey == ImGuiKey_DownArrow)
    ++*selection;
  return 0;
}
}  // namespace

namespace Navigation
{

void open(GUIView* view, NavSection section)
{
  switch (section)
  {
    case NavSection::HOME:
    case NavSection::FINANCES:
      if (MainGameScene* hub = careerHub(view))
        hub->showPage(section == NavSection::HOME
                          ? MainGameScene::Page::OVERVIEW
                          : MainGameScene::Page::FINANCES);
      view->navigateTo(nullptr);
      return;
    case NavSection::INBOX:
      view->navigateTo(std::make_unique<InboxScene>(view));
      return;
    case NavSection::CLUB:
      view->navigateTo(std::make_unique<ClubScene>(view));
      return;
    case NavSection::SQUAD:
      view->navigateTo(std::make_unique<RosterScene>(view));
      return;
    case NavSection::LINEUP:
      view->navigateTo(std::make_unique<LineupScene>(view));
      return;
    case NavSection::TACTICS:
      view->navigateTo(std::make_unique<StrategyScene>(view));
      return;
    case NavSection::FIXTURES:
      view->navigateTo(std::make_unique<FixturesScene>(view));
      return;
    case NavSection::STANDINGS:
      view->navigateTo(std::make_unique<StandingsScene>(view));
      return;
    case NavSection::TRANSFERS:
      view->navigateTo(std::make_unique<TransferMarketScene>(view));
      return;
    case NavSection::SCOUTING:
      view->navigateTo(std::make_unique<ScoutingScene>(view));
      return;
    case NavSection::TRAINING:
      view->navigateTo(std::make_unique<TrainingScene>(view));
      return;
    case NavSection::STAFF:
      view->navigateTo(std::make_unique<StaffScene>(view));
      return;
    case NavSection::NONE:
      return;
  }
}

void openPlayer(GUIView* view, PlayerID playerId)
{
  view->overlayScene(std::make_unique<PlayerProfileScene>(view, playerId));
}

void openMatchReport(GUIView* view, GameDateValue date, TeamID homeId,
                     TeamID awayId)
{
  view->overlayScene(
      std::make_unique<MatchReportScene>(view, date, homeId, awayId));
}

void openClub(GUIView* view, TeamID teamId)
{
  view->navigateTo(std::make_unique<RosterScene>(view, teamId));
}

void back(GUIView* view)
{
  if (view->getOverlayDepth() > 0) view->popScene();
}

}  // namespace Navigation

ManagementScene::ManagementScene(GUIView* guiViewPtr) : GUIScene(guiViewPtr) {}

void ManagementScene::onEnter() { refresh(); }

void ManagementScene::onResume() { refresh(); }

void ManagementScene::showToast(std::string message, bool isError)
{
  toast_message = std::move(message);
  toast_is_error = isError;
  toast_seconds = TOAST_SECONDS;
}

void ManagementScene::syncClubAccent()
{
  const auto managed = guiView->getController().getManagedTeam();
  Theme::setClub(managed ? std::optional<TeamID>(managed->get().getId())
                         : std::nullopt);
}

void ManagementScene::render()
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  MainGameScene* hub = careerHub(guiView);
  const bool advancing = hub != nullptr && hub->isAdvancing();
  // While advancing, the frozen frame drawn by GUIView shows through.
  ImGui::Begin("##management_shell", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoScrollWithMouse |
                   (advancing && guiView->getBackdrop() != nullptr
                        ? ImGuiWindowFlags_NoBackground
                        : ImGuiWindowFlags_None));
  ImGui::PopStyleVar(3);

  if (advancing)
  {
    hub->renderContinueOverlay();
    ImGui::End();
    return;
  }

  // Game state is only read once no simulation runs in the background.
  syncClubAccent();
  toast_seconds = std::max(0.0f, toast_seconds - ImGui::GetIO().DeltaTime);
  handleShortcuts();

  const bool collapsed = ImGui::GetMainViewport()->WorkSize.x <
                         SIDEBAR_COLLAPSE_BELOW * Theme::scale();
  renderSidebar(collapsed);
  ImGui::SameLine(0.0f, 0.0f);
  ImGui::BeginGroup();
  renderTopBar(TOP_BAR_HEIGHT * Theme::scale());
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(Theme::Space::XL * Theme::scale(),
                             Theme::Space::L * Theme::scale()));
  ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f),
                    ImGuiChildFlags_AlwaysUseWindowPadding);
  ImGui::PopStyleVar();
  renderContent();
  ImGui::EndChild();
  ImGui::EndGroup();

  renderPalette();
  renderMainMenuConfirm();
  if (hub != nullptr)
    if (const float fade = hub->continueFadeOut(); fade > 0.0f)
    {
      ImGui::GetForegroundDrawList()->AddRectFilled(
          viewport->WorkPos,
          ImVec2(viewport->WorkPos.x + viewport->WorkSize.x,
                 viewport->WorkPos.y + viewport->WorkSize.y),
          IM_COL32(0, 0, 0, static_cast<int>(120.0f * fade)));
    }
  ImGui::End();
}

void ManagementScene::renderSidebar(bool collapsed)
{
  const float width =
      (collapsed ? SIDEBAR_COLLAPSED_WIDTH : SIDEBAR_WIDTH) * Theme::scale();
  const Theme::Palette& palette = Theme::palette();
  GameController& controller = guiView->getController();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, palette.sidebar);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(Theme::Space::S * Theme::scale(),
                             Theme::Space::L * Theme::scale()));
  ImGui::PushStyleVar(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(Theme::Space::S * Theme::scale(), 2.0f * Theme::scale()));
  ImGui::BeginChild("##sidebar", ImVec2(width, 0.0f),
                    ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar);

  // Every destination should be visible without scrolling: on short windows
  // (720p, large UI scales) the items shrink towards a compact height. The
  // navigation still scrolls (mouse wheel) as a last resort, so the footer
  // actions stay pinned and reachable at any window height and UI scale.
  const float spacing = ImGui::GetStyle().ItemSpacing.y;
  size_t itemCount = 3;  // Footer actions.
  for (const NavGroup& group : NAV_GROUPS)
    itemCount += static_cast<size_t>(std::ranges::count_if(
        group.entries, [](const NavEntry* entry) { return entry != nullptr; }));
  const float clubBlock =
      controller.getManagedTeam()
          ? (CLUB_BADGE_SIZE + Theme::Space::M) * Theme::scale() + spacing
          : 0.0f;
  const float groupOverhead = (Theme::Space::S + 2.0f) * Theme::scale() +
                              ImGui::GetTextLineHeight() + 3.0f * spacing;
  const float fixedHeight =
      clubBlock + static_cast<float>(NAV_GROUPS.size()) * groupOverhead +
      Theme::Space::S * Theme::scale() + 2.0f * spacing;
  const float itemHeight = std::clamp(
      (ImGui::GetContentRegionAvail().y - fixedHeight) /
              static_cast<float>(itemCount) -
          spacing,
      NAV_ITEM_MIN_HEIGHT * Theme::scale(), NAV_ITEM_HEIGHT * Theme::scale());
  const float footerHeight =
      3.0f * (itemHeight + spacing) + Theme::Space::S * Theme::scale();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::BeginChild(
      "##sidebar_nav",
      ImVec2(0.0f, std::max(itemHeight,
                            ImGui::GetContentRegionAvail().y - footerHeight)),
      ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
  ImGui::PopStyleVar();

  // Club identity block.
  if (const auto managed = controller.getManagedTeam())
  {
    const Team& club = managed->get();
    const float badgeSize = CLUB_BADGE_SIZE * Theme::scale();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const ImVec2 badgeMin(
        collapsed
            ? start.x + (ImGui::GetContentRegionAvail().x - badgeSize) * 0.5f
            : start.x + Theme::Space::XS * Theme::scale(),
        start.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        badgeMin, ImVec2(badgeMin.x + badgeSize, badgeMin.y + badgeSize),
        Theme::toU32(palette.accent), 8.0f * Theme::scale());
    const std::string initials = clubInitials(club.getName());
    {
      Theme::ScopedText heading(Theme::Text::TITLE);
      const ImVec2 initialsSize = ImGui::CalcTextSize(initials.c_str());
      drawList->AddText(
          ImVec2(badgeMin.x + (badgeSize - initialsSize.x) * 0.5f,
                 badgeMin.y + (badgeSize - initialsSize.y) * 0.5f),
          Theme::toU32(palette.on_accent), initials.c_str());
    }
    const float textX =
        badgeMin.x + badgeSize + Theme::Space::S * Theme::scale();
    const float textWidth = collapsed
                                ? 0.0f
                                : start.x + ImGui::GetContentRegionAvail().x -
                                      textX - 2.0f * Theme::scale();
    bool cut = false;
    if (textWidth > 0.0f)
    {
      cut |= UI::drawTextFitted(
          drawList, ImVec2(textX, start.y + 1.0f * Theme::scale()),
          Theme::toU32(palette.text), club.getName(), textWidth);
      if (const auto league = controller.getLeagueById(club.getLeagueId()))
      {
        ImGui::PushFont(nullptr, Theme::textSize(Theme::Text::SMALL));
        cut |= UI::drawTextFitted(
            drawList,
            ImVec2(textX, start.y + badgeSize - ImGui::GetTextLineHeight()),
            Theme::toU32(palette.muted), league->get().getName(), textWidth);
        ImGui::PopFont();
      }
    }
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, badgeSize));
    if ((cut || collapsed) && ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", club.getName().c_str());
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::M * Theme::scale() -
                                  ImGui::GetStyle().ItemSpacing.y));
  }

  const NavSection current = navSection();
  for (const NavGroup& group : NAV_GROUPS)
  {
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
    if (collapsed)
    {
      ImGui::Separator();
    }
    else
    {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                           Theme::Space::M * Theme::scale());
      UI::sectionLabel(LOC(group.title_key));
    }
    ImGui::Dummy(ImVec2(0.0f, 2.0f * Theme::scale()));
    for (const NavEntry* entry : group.entries)
    {
      if (entry == nullptr) continue;
      const size_t badge = entry->section == NavSection::INBOX
                               ? controller.getUnreadInboxCount()
                               : 0U;
      if (navItem(LOC(entry->label_key), entry->shortcut,
                  current == entry->section, entry->icon, collapsed, itemHeight,
                  badge))
        Navigation::open(guiView, entry->section);
    }
  }
  sidebar_nav_overflow = ImGui::GetScrollMaxY() > 0.0f;

  ImGui::EndChild();

  // Footer actions pinned to the bottom of the sidebar.
  ImGui::Separator();
  if (navItem(LOC("MAIN_GAME_SAVE_GAME"), "Ctrl+S", false, UI::Icon::SAVE,
              collapsed, itemHeight))
  {
    controller.saveGame();
    showToast(LOC("DASHBOARD_SAVED"));
  }
  if (navItem(LOC("MENU_SETTINGS"), nullptr, false, UI::Icon::SETTINGS,
              collapsed, itemHeight))
    guiView->navigateTo(std::make_unique<SettingsScene>(guiView, true));
  if (navItem(LOC("MAIN_GAME_MAIN_MENU"), nullptr, false, UI::Icon::EXIT,
              collapsed, itemHeight))
    main_menu_confirm_requested = true;

  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
}

void ManagementScene::renderTopBar(float height)
{
  const Theme::Palette& palette = Theme::palette();
  GameController& controller = guiView->getController();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(Theme::Space::XL * Theme::scale(), 0.0f));
  ImGui::BeginChild("##topbar", ImVec2(0.0f, height),
                    ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar);
  ImGui::PopStyleVar();
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  const ImGuiStyle& style = ImGui::GetStyle();
  const float frameHeight = ImGui::GetFrameHeight();
  const float centeredY = (height - frameHeight) * 0.5f;
  // Text sharing a line with framed widgets is baseline-aligned by ImGui.
  const float textY = centeredY;
  const float gap = Theme::Space::L * Theme::scale();

  // Right cluster (date, balance, continue) is measured first so the left
  // side can shrink on narrow windows instead of overlapping it.
  MainGameScene* hub = careerHub(guiView);
  const std::string dateText = Format::date(controller.getCurrentDate());
  std::string balanceText;
  bool negativeBalance = false;
  if (const auto managed = controller.getManagedTeam())
  {
    const int64_t balance = managed->get().getFinances().getBalance();
    negativeBalance = balance < 0;
    balanceText = Format::money(balance);
  }
  const std::string continueText =
      hub != nullptr ? hub->continueLabel() : std::string();
  const float continueWidth =
      continueText.empty()
          ? 0.0f
          : std::max(CONTINUE_MIN_WIDTH * Theme::scale(),
                     ImGui::CalcTextSize(continueText.c_str()).x +
                         4.0f * style.FramePadding.x);
  const float dateWidth = ImGui::CalcTextSize(dateText.c_str()).x;
  const float balanceWidth = ImGui::CalcTextSize(balanceText.c_str()).x;
  const float rightEdge = ImGui::GetWindowContentRegionMax().x;
  const float leftEdge = ImGui::GetCursorPosX();

  const std::string backLabel =
      std::string("‹  ") + LOC("NAV_BACK") + "###shell_back";
  const bool canGoBack = guiView->getOverlayDepth() > 0;
  const float backWidth =
      canGoBack ? ImGui::CalcTextSize(backLabel.c_str(), nullptr, true).x +
                      2.0f * style.FramePadding.x + style.ItemSpacing.x
                : 0.0f;
  const float fullSearchWidth = 300.0f * Theme::scale();
  const float compactSearchWidth = frameHeight + 2.0f * Theme::scale();
  const float balanceBlock = balanceWidth + gap;
  const float continueBlock = continueWidth > 0.0f ? continueWidth + gap : 0.0f;
  const float available = rightEdge - leftEdge - backWidth - continueBlock -
                          balanceBlock - style.ItemSpacing.x;
  const bool showDate = available - dateWidth - gap >= compactSearchWidth;
  const float searchRoom = available - (showDate ? dateWidth + gap : 0.0f);
  const bool fullSearch = searchRoom >= fullSearchWidth;
  const float searchWidth = fullSearch ? fullSearchWidth : compactSearchWidth;

  ImGui::SetCursorPosY(centeredY);
  if (canGoBack)
  {
    if (ImGui::Button(backLabel.c_str())) Navigation::back(guiView);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip("%s", LOC("NAV_BACK_HINT"));
    ImGui::SameLine();
  }

  // Search launcher styled as an input field with a drawn magnifier.
  {
    const std::string searchLabel =
        fullSearch
            ? std::format("      {}###shell_search", LOC("PALETTE_LAUNCHER"))
            : std::string("###shell_search");
    ImGui::PushStyleColor(ImGuiCol_Button, palette.surface);
    ImGui::PushStyleColor(ImGuiCol_Text, palette.faint);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    const ImVec2 searchPos = ImGui::GetCursorScreenPos();
    if (ImGui::Button(searchLabel.c_str(), ImVec2(searchWidth, 0.0f)))
      openPalette();
    if (!fullSearch && ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", LOC("PALETTE_LAUNCHER"));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    const float iconSize = 14.0f * Theme::scale();
    UI::drawIcon(
        drawList, UI::Icon::SEARCH,
        ImVec2(fullSearch ? searchPos.x + style.FramePadding.x + iconSize * 0.5f
                          : searchPos.x + searchWidth * 0.5f,
               searchPos.y + frameHeight * 0.5f),
        iconSize, Theme::toU32(palette.faint));
  }

  if (toast_seconds > 0.0f && !toast_message.empty())
  {
    const float alpha =
        Theme::reducedMotion() ? 1.0f : std::min(1.0f, toast_seconds / 0.4f);
    ImGui::SameLine(0.0f, gap);
    ImGui::SetCursorPosY(textY);
    const float toastRoom = rightEdge - (showDate ? dateWidth + gap : 0.0f) -
                            balanceBlock - continueBlock - gap -
                            ImGui::GetCursorPosX();
    const ImVec4& tone = toast_is_error ? palette.negative : palette.positive;
    const ImVec2 clipMin = ImGui::GetCursorScreenPos();
    ImGui::PushClipRect(
        clipMin,
        ImVec2(clipMin.x + std::max(0.0f, toastRoom), clipMin.y + frameHeight),
        true);
    ImGui::TextColored(ImVec4(tone.x, tone.y, tone.z, alpha), "%s",
                       toast_message.c_str());
    ImGui::PopClipRect();
  }

  const float clusterWidth =
      (showDate ? dateWidth + gap : 0.0f) + balanceWidth + continueBlock;
  ImGui::SameLine();
  ImGui::SetCursorPos(ImVec2(
      std::max(ImGui::GetCursorPosX(), rightEdge - clusterWidth), textY));
  if (showDate)
  {
    ImGui::TextColored(palette.muted, "%s", dateText.c_str());
    ImGui::SameLine(0.0f, gap);
    ImGui::SetCursorPosY(textY);
  }
  ImGui::TextColored(negativeBalance ? palette.negative : palette.text, "%s",
                     balanceText.c_str());
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOC("FINANCE_CASH"));
  if (hub != nullptr && continueWidth > 0.0f)
  {
    ImGui::SameLine(0.0f, gap);
    ImGui::SetCursorPosY(centeredY);
    const std::string label = continueText + "###shell_continue";
    if (UI::primaryButton(label.c_str(), ImVec2(continueWidth, 0.0f)))
      hub->requestContinue();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", LOC("SHELL_CONTINUE_TOOLTIP"));
  }
  ImGui::EndChild();
  const ImVec2 barMin = ImGui::GetItemRectMin();
  const ImVec2 barMax = ImGui::GetItemRectMax();
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(barMin.x, barMax.y - 1.0f), barMax, Theme::toU32(palette.border));
}

void ManagementScene::handleShortcuts()
{
  if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_K, ImGuiInputFlags_RouteGlobal))
    openPalette();
  if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
  {
    guiView->getController().saveGame();
    showToast(LOC("DASHBOARD_SAVED"));
  }

  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput || ImGui::IsAnyItemActive() ||
      ImGui::IsPopupOpen(
          "", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    return;

  for (const NavEntry* entry : ALL_NAV)
  {
    if (entry->key != ImGuiKey_None && ImGui::IsKeyPressed(entry->key, false))
    {
      Navigation::open(guiView, entry->section);
      return;
    }
  }
  // Space / Enter continue only while keyboard navigation is not driving a
  // focused widget, so they never double as widget activation.
  if (!io.NavVisible && (ImGui::IsKeyPressed(ImGuiKey_Space, false) ||
                         ImGui::IsKeyPressed(ImGuiKey_Enter, false)))
  {
    if (MainGameScene* hub = careerHub(guiView)) hub->requestContinue();
    return;
  }
  // Escape is claimed through the shortcut router so keyboard navigation
  // does not also treat it as "cancel" and light up a focus frame.
  if (ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteGlobal) ||
      ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_LeftArrow,
                      ImGuiInputFlags_RouteGlobal))
    Navigation::back(guiView);
}

void ManagementScene::openPalette()
{
  palette_requested = true;
  palette_focus_input = true;
  palette_query.fill('\0');
  palette_selection = 0;
}

void ManagementScene::buildPaletteIndex()
{
  palette_entries.clear();
  GameController& controller = guiView->getController();
  for (const NavEntry* entry : ALL_NAV)
  {
    PaletteEntry item{PaletteEntry::Kind::SECTION,
                      static_cast<uint32_t>(entry->section),
                      LOC(entry->label_key),
                      {},
                      entry->shortcut
                          ? std::format("{}  ·  {}", LOC("PALETTE_KIND_SCREEN"),
                                        entry->shortcut)
                          : std::string(LOC("PALETTE_KIND_SCREEN"))};
    item.label_lower = PlayerView::toLower(item.label);
    palette_entries.push_back(std::move(item));
  }
  for (const auto& teamRef : controller.getTeams())
  {
    const Team& team = teamRef.get();
    if (team.getId() == FREE_AGENTS_TEAM_ID) continue;
    const auto league = controller.getLeagueById(team.getLeagueId());
    PaletteEntry item{
        PaletteEntry::Kind::CLUB,
        team.getId(),
        team.getName(),
        {},
        std::format("{}  ·  {}", LOC("PALETTE_KIND_CLUB"),
                    league ? league->get().getName() : std::string())};
    item.label_lower = PlayerView::toLower(item.label);
    palette_entries.push_back(std::move(item));
  }
  const auto data = controller.getGameData();
  if (!data) return;
  palette_entries.reserve(palette_entries.size() + data->getPlayers().size());
  for (const auto& playerRef : data->getPlayersVector())
  {
    const Player& player = playerRef.get();
    const auto team = controller.getTeamById(player.getTeamId());
    PaletteEntry item{
        PaletteEntry::Kind::PLAYER,
        player.getId(),
        player.getName(),
        {},
        std::format("{}  ·  {}", RoleUtils::toString(player.getRole()),
                    team && player.getTeamId() != FREE_AGENTS_TEAM_ID
                        ? team->get().getName()
                        : std::string(LOC("TRANSFER_FREE_AGENT_LABEL")))};
    item.label_lower = PlayerView::toLower(item.label);
    palette_entries.push_back(std::move(item));
  }
}

void ManagementScene::filterPalette()
{
  palette_filtered_query = palette_query.data();
  palette_matches.clear();
  palette_selection = 0;
  const std::string query = PlayerView::toLower(palette_filtered_query);
  if (query.empty())
  {
    for (size_t index = 0; index < palette_entries.size(); ++index)
      if (palette_entries[index].kind == PaletteEntry::Kind::SECTION)
        palette_matches.push_back(index);
    return;
  }
  // Prefix matches (on any word) rank above plain substring matches.
  std::vector<size_t> substring;
  for (size_t index = 0; index < palette_entries.size(); ++index)
  {
    const std::string& label = palette_entries[index].label_lower;
    const size_t found = label.find(query);
    if (found == std::string::npos) continue;
    const bool wordStart = found == 0 || label[found - 1] == ' ';
    if (wordStart)
    {
      palette_matches.push_back(index);
      if (palette_matches.size() >= PALETTE_MAX_RESULTS) return;
    }
    else if (substring.size() < PALETTE_MAX_RESULTS)
      substring.push_back(index);
  }
  for (const size_t index : substring)
  {
    if (palette_matches.size() >= PALETTE_MAX_RESULTS) break;
    palette_matches.push_back(index);
  }
}

void ManagementScene::activatePaletteEntry(const PaletteEntry& entry)
{
  switch (entry.kind)
  {
    case PaletteEntry::Kind::SECTION:
      Navigation::open(guiView, static_cast<NavSection>(entry.id));
      break;
    case PaletteEntry::Kind::CLUB:
      Navigation::openClub(guiView, static_cast<TeamID>(entry.id));
      break;
    case PaletteEntry::Kind::PLAYER:
      Navigation::openPlayer(guiView, entry.id);
      break;
  }
}

void ManagementScene::renderPalette()
{
  if (palette_requested)
  {
    palette_requested = false;
    buildPaletteIndex();
    filterPalette();
    ImGui::OpenPopup(PALETTE_POPUP_ID);
  }
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
             viewport->WorkPos.y + PALETTE_TOP_OFFSET * Theme::scale()),
      ImGuiCond_Always, ImVec2(0.5f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(PALETTE_WIDTH * Theme::scale(), 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(Theme::Space::M * Theme::scale(),
                             Theme::Space::M * Theme::scale()));
  const bool open = ImGui::BeginPopup(
      PALETTE_POPUP_ID,
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::PopStyleVar();
  if (!open)
  {
    if (!palette_entries.empty())
    {
      palette_entries.clear();
      palette_entries.shrink_to_fit();
      palette_matches.clear();
    }
    return;
  }

  if (palette_focus_input)
  {
    ImGui::SetKeyboardFocusHere();
    palette_focus_input = false;
  }
  ImGui::SetNextItemWidth(-FLT_MIN);
  const bool submitted =
      ImGui::InputTextWithHint("##palette_query", LOC("PALETTE_HINT"),
                               palette_query.data(), palette_query.size(),
                               ImGuiInputTextFlags_EnterReturnsTrue |
                                   ImGuiInputTextFlags_CallbackHistory,
                               paletteHistoryCallback, &palette_selection);
  if (palette_filtered_query != palette_query.data()) filterPalette();
  const int matchCount = static_cast<int>(palette_matches.size());
  if (matchCount > 0)
    palette_selection = std::clamp(palette_selection, 0, matchCount - 1);

  const PaletteEntry* chosen = nullptr;
  if (submitted && matchCount > 0)
    chosen = &palette_entries[palette_matches[static_cast<size_t>(
        palette_selection)]];
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();

  ImGui::Dummy(ImVec2(0.0f, 2.0f * Theme::scale()));
  if (matchCount == 0)
  {
    ImGui::TextColored(Theme::palette().muted, "%s", LOC("PALETTE_NO_MATCH"));
  }
  for (int row = 0; row < matchCount; ++row)
  {
    const PaletteEntry& entry =
        palette_entries[palette_matches[static_cast<size_t>(row)]];
    ImGui::PushID(row);
    const bool selected = row == palette_selection;
    if (ImGui::Selectable("##palette_row", selected, 0,
                          ImVec2(0.0f, ImGui::GetTextLineHeight())))
      chosen = &entry;
    ImGui::SameLine(Theme::Space::M * Theme::scale());
    ImGui::TextUnformatted(entry.label.c_str());
    ImGui::SameLine();
    {
      Theme::ScopedText small(Theme::Text::SMALL);
      UI::textRightColored(Theme::palette().faint, entry.detail.c_str());
    }
    ImGui::PopID();
  }
  if (chosen != nullptr)
  {
    const PaletteEntry selectedEntry = *chosen;
    ImGui::CloseCurrentPopup();
    activatePaletteEntry(selectedEntry);
  }
  ImGui::EndPopup();
}

void ManagementScene::renderMainMenuConfirm()
{
  if (main_menu_confirm_requested)
  {
    main_menu_confirm_requested = false;
    ImGui::OpenPopup(MAIN_MENU_CONFIRM_ID);
  }
  if (UI::confirmDialog(MAIN_MENU_CONFIRM_ID, LOC("SHELL_MAIN_MENU_TITLE"),
                        LOC("SHELL_MAIN_MENU_BODY"), LOC("MAIN_GAME_MAIN_MENU"),
                        LOC("SETTINGS_CANCEL")) == UI::DialogResult::CONFIRM)
    changeScene(std::make_unique<MainMenuScene>(guiView));
}
