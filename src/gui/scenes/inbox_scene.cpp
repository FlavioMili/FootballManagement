// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/inbox_scene.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>

#include "controller/game_controller.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"

namespace
{
constexpr float FILTER_WIDTH = 220.0f;
constexpr float THREAD_LIST_RATIO = 0.42f;
constexpr int DIGEST_WINDOW_DAYS = 6;

// Days since 1970-01-01 (proleptic Gregorian), for grouping by week.
int dayNumber(const GameDateValue& date)
{
  const int year = date.year - (date.month <= 2 ? 1 : 0);
  const int era = (year >= 0 ? year : year - 399) / 400;
  const int yearOfEra = year - era * 400;
  const int month = date.month;
  const int dayOfYear =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + date.day - 1;
  const int dayOfEra =
      yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + dayOfEra - 719468;
}

ImVec4 categoryColor(InboxCategory category)
{
  const Theme::Palette& palette = Theme::palette();
  switch (category)
  {
    case InboxCategory::Match:
      return palette.info;
    case InboxCategory::Injury:
      return palette.negative;
    case InboxCategory::Transfer:
      return palette.accent;
    case InboxCategory::Contract:
    case InboxCategory::Finance:
      return palette.warning;
    case InboxCategory::Youth:
      return palette.positive;
    case InboxCategory::Board:
      return palette.text;
    case InboxCategory::General:
    case InboxCategory::COUNT:
      break;
  }
  return palette.muted;
}
}  // namespace

InboxScene::InboxScene(GUIView* parent) : ManagementScene(parent) {}

void InboxScene::update(float /*deltaTime*/) {}

void InboxScene::refresh() { rebuildThreads(); }

void InboxScene::rebuildThreads()
{
  const auto& messages = guiView->getController().getInbox();
  threads.clear();
  unread_by_category.fill(0);
  total_by_category.fill(0);
  for (const InboxMessage& message : messages)
  {
    const auto category = static_cast<size_t>(message.category);
    if (category >= CATEGORY_COUNT) continue;
    ++total_by_category[category];
    if (!message.read) ++unread_by_category[category];
  }

  // Newest first; fold same-kind messages from the same week into a digest.
  int groupStartDay = 0;
  for (size_t reverse = messages.size(); reverse > 0; --reverse)
  {
    const size_t index = reverse - 1;
    const InboxMessage& message = messages[index];
    if (category_filter >= 0 &&
        static_cast<int>(message.category) != category_filter)
      continue;
    if (unread_only && message.read) continue;
    const int day = dayNumber(message.date);
    if (!threads.empty())
    {
      Thread& last = threads.back();
      const InboxMessage& head = messages[last.messages.front()];
      if (head.category == message.category &&
          head.title_key == message.title_key &&
          groupStartDay - day <= DIGEST_WINDOW_DAYS)
      {
        last.messages.push_back(index);
        if (!message.read) ++last.unread;
        continue;
      }
    }
    Thread& thread = threads.emplace_back();
    thread.messages.push_back(index);
    thread.category = message.category;
    thread.title = message.formatTitle();
    thread.date_text = Format::dayMonth(message.date);
    thread.unread = message.read ? 0 : 1;
    groupStartDay = day;
  }
  for (Thread& thread : threads)
  {
    if (thread.messages.size() < 2)
    {
      thread.detail = LOC(inboxCategoryKey(thread.category));
      continue;
    }
    thread.detail = fmt::sprintf(LOC("INBOX_DIGEST_DETAIL"), thread.title);
    thread.title = fmt::sprintf(LOC("INBOX_DIGEST_TITLE"),
                                LOC(inboxCategoryKey(thread.category)),
                                thread.messages.size());
    for (const size_t messageIndex : thread.messages)
      thread.message_titles.push_back(messages[messageIndex].formatTitle());
  }
  if (selected_thread >= static_cast<int>(threads.size())) selected_thread = -1;
}

void InboxScene::openMessage(size_t messageIndex)
{
  selected_message = messageIndex;
  const auto& messages = guiView->getController().getInbox();
  if (messageIndex < messages.size())
  {
    selected_title = messages[messageIndex].formatTitle();
    selected_body = messages[messageIndex].formatBody();
  }
  if (messageIndex < messages.size() && !messages[messageIndex].read)
  {
    guiView->getController().markInboxMessageRead(messages[messageIndex].id);
    rebuildThreads();
  }
}

void InboxScene::renderContent()
{
  GameController& controller = guiView->getController();
  const std::string subtitle =
      fmt::sprintf(LOC("INBOX_SUBTITLE"), controller.getUnreadInboxCount(),
                   controller.getInbox().size());
  UI::pageHeader(LOC("INBOX_TITLE"), subtitle.c_str());
  ImGui::BeginDisabled(controller.getUnreadInboxCount() == 0);
  if (UI::secondaryButton(LOC("INBOX_MARK_ALL_READ")))
  {
    controller.markAllInboxMessagesRead();
    rebuildThreads();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Checkbox(LOC("INBOX_UNREAD_ONLY"), &unread_only)) rebuildThreads();

  const float height = ImGui::GetContentRegionAvail().y;
  const float available = ImGui::GetContentRegionAvail().x;
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float filterWidth = FILTER_WIDTH * Theme::scale();
  const float listWidth =
      std::floor((available - filterWidth - 2.0f * gap) * THREAD_LIST_RATIO);
  renderFilters(filterWidth, height);
  ImGui::SameLine();
  renderThreads(listWidth, height);
  ImGui::SameLine();
  renderReader(height);
}

void InboxScene::renderFilters(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("inbox_filters", LOC("INBOX_CATEGORIES"), ImVec2(width, height),
                true);
  const auto filterRow =
      [&](int category, const char* label, size_t unread, size_t total)
  {
    ImGui::PushID(category);
    if (ImGui::Selectable("##filter", category_filter == category, 0,
                          ImVec2(0.0f, ImGui::GetFrameHeight())))
    {
      category_filter = category;
      selected_thread = -1;
      expanded_thread = -1;
      rebuildThreads();
    }
    ImGui::SameLine(Theme::Space::S * Theme::scale());
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    const std::string count = unread > 0
                                  ? fmt::sprintf("%zu / %zu", unread, total)
                                  : std::to_string(total);
    UI::textRightColored(unread > 0 ? palette.accent : palette.faint,
                         count.c_str());
    ImGui::PopID();
  };
  size_t unreadTotal = 0;
  size_t total = 0;
  for (size_t index = 0; index < CATEGORY_COUNT; ++index)
  {
    unreadTotal += unread_by_category[index];
    total += total_by_category[index];
  }
  filterRow(-1, LOC("INBOX_ALL"), unreadTotal, total);
  ImGui::Separator();
  for (size_t index = 0; index < CATEGORY_COUNT; ++index)
  {
    if (total_by_category[index] == 0) continue;
    filterRow(static_cast<int>(index),
              LOC(inboxCategoryKey(static_cast<InboxCategory>(index))),
              unread_by_category[index], total_by_category[index]);
  }
  UI::endCard();
}

void InboxScene::renderThreads(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("inbox_threads", nullptr, ImVec2(width, height), true);
  if (threads.empty())
  {
    UI::emptyState(LOC("INBOX_EMPTY_TITLE"), LOC("INBOX_EMPTY_BODY"));
    UI::endCard();
    return;
  }
  const float rowHeight =
      ImGui::GetTextLineHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
  for (size_t index = 0; index < threads.size(); ++index)
  {
    const Thread& thread = threads[index];
    const bool digest = thread.messages.size() > 1;
    ImGui::PushID(static_cast<int>(index));
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const bool selected = selected_thread == static_cast<int>(index);
    if (ImGui::Selectable("##thread", selected, 0, ImVec2(0.0f, rowHeight)))
    {
      selected_thread = static_cast<int>(index);
      if (digest)
        expanded_thread = expanded_thread == static_cast<int>(index)
                              ? -1
                              : static_cast<int>(index);
      openMessage(thread.messages.front());
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float left = start.x + Theme::Space::M * Theme::scale();
    drawList->AddRectFilled(ImVec2(start.x, start.y + 3.0f * Theme::scale()),
                            ImVec2(start.x + 3.0f * Theme::scale(),
                                   start.y + rowHeight - 3.0f * Theme::scale()),
                            Theme::toU32(categoryColor(thread.category)));
    const ImU32 titleColor =
        Theme::toU32(thread.unread > 0 ? palette.text : palette.muted);
    const float right = start.x + ImGui::GetContentRegionAvail().x;
    const float dateWidth = ImGui::CalcTextSize(thread.date_text.c_str()).x;
    bool cut = UI::drawTextFitted(
        drawList, ImVec2(left, start.y + 2.0f * Theme::scale()), titleColor,
        thread.title,
        right - dateWidth - Theme::Space::S * Theme::scale() - left);
    drawList->AddText(
        ImVec2(right - dateWidth, start.y + 2.0f * Theme::scale()),
        Theme::toU32(palette.faint), thread.date_text.c_str());
    cut |=
        UI::drawTextFitted(drawList,
                           ImVec2(left, start.y + ImGui::GetTextLineHeight() +
                                            3.0f * Theme::scale()),
                           Theme::toU32(palette.faint), thread.detail,
                           right - left - Theme::Space::M * Theme::scale());
    if (cut && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
      ImGui::SetTooltip("%s\n%s", thread.title.c_str(), thread.detail.c_str());
    if (thread.unread > 0)
      drawList->AddCircleFilled(
          ImVec2(right - 4.0f * Theme::scale(),
                 start.y + ImGui::GetTextLineHeight() * 1.6f),
          3.5f * Theme::scale(), Theme::toU32(palette.accent));

    if (digest && expanded_thread == static_cast<int>(index))
    {
      ImGui::Indent(Theme::Space::L * Theme::scale());
      for (size_t child = 0; child < thread.messages.size(); ++child)
      {
        const size_t messageIndex = thread.messages[child];
        ImGui::PushID(static_cast<int>(messageIndex));
        if (ImGui::Selectable(thread.message_titles[child].c_str(),
                              selected_message == messageIndex))
          openMessage(messageIndex);
        ImGui::PopID();
      }
      ImGui::Unindent(Theme::Space::L * Theme::scale());
    }
    ImGui::PopID();
  }
  UI::endCard();
}

void InboxScene::renderReader(float height)
{
  const Theme::Palette& palette = Theme::palette();
  const auto& messages = guiView->getController().getInbox();
  UI::beginCard("inbox_reader", nullptr, ImVec2(0.0f, height), true);
  if (selected_message >= messages.size())
  {
    UI::emptyState(LOC("INBOX_SELECT_TITLE"), LOC("INBOX_SELECT_BODY"));
    UI::endCard();
    return;
  }
  const InboxMessage& message = messages[selected_message];
  UI::badge(LOC(inboxCategoryKey(message.category)),
            categoryColor(message.category));
  ImGui::SameLine();
  ImGui::TextColored(palette.muted, "%s", Format::date(message.date).c_str());
  {
    Theme::ScopedText title(Theme::Text::TITLE);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(selected_title.c_str());
    ImGui::PopTextWrapPos();
  }
  ImGui::Separator();
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(selected_body.c_str());
  ImGui::PopTextWrapPos();
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::M * Theme::scale()));
  if (message.player_id && UI::secondaryButton(LOC("INBOX_OPEN_PLAYER")))
    Navigation::openPlayer(guiView, *message.player_id);
  if (message.player_id && message.team_id) ImGui::SameLine();
  if (message.team_id && UI::secondaryButton(LOC("INBOX_OPEN_CLUB")))
    Navigation::openClub(guiView, *message.team_id);
  talk_dialog.inboxAction(guiView->getController(), message);
  talk_dialog.render(guiView->getController());
  UI::endCard();
}
