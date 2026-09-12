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
#include <cstdint>
#include <optional>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/role_utils.h"

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

void InboxScene::refresh()
{
  // New messages shift indices once the inbox is full: follow the id.
  if (selected_message != SIZE_MAX)
  {
    const auto& messages = guiView->getController().getInbox();
    const auto found = std::ranges::find(messages, selected_message_id,
                                         &InboxMessage::id);
    selected_message =
        found != messages.end()
            ? static_cast<size_t>(std::distance(messages.begin(), found))
            : SIZE_MAX;
  }
  rebuildDecisions();
  rebuildThreads();
}

void InboxScene::rebuildDecisions()
{
  GameController& controller = guiView->getController();
  const auto& messages = controller.getInbox();
  const GameDateValue today = controller.getCurrentDate();
  decisions.clear();
  hidden.assign(messages.size(), false);
  archived_count = 0;
  std::vector<std::pair<InboxAction, PlayerID>> seen;
  // Newest first: one entry per decision (repeated bids fold into one).
  for (size_t reverse = messages.size(); reverse > 0; --reverse)
  {
    const size_t index = reverse - 1;
    const InboxMessage& message = messages[index];
    const InboxAction action = Inbox::actionFor(message.title_key);
    if (Inbox::isDecision(action) && controller.isInboxDecisionPending(message))
    {
      hidden[index] = true;
      const PlayerID player = message.player_id.value_or(0);
      if (std::ranges::contains(seen, std::pair{action, player})) continue;
      seen.emplace_back(action, player);
      Decision& decision = decisions.emplace_back();
      decision.message = index;
      decision.action = action;
      decision.player = player;
      decision.title = message.formatTitle();
      decision.body = message.formatBody();
      decision.date_text = Format::dayMonth(message.date);
      if (action == InboxAction::RespondOffer)
      {
        for (const IncomingOffer& offer : controller.getIncomingOffers())
        {
          if (offer.player_id != player) continue;
          const auto buyer = controller.getTeamById(offer.buyer);
          const char* buyer_name =
              buyer ? buyer->get().getName().c_str() : "";
          const bool awaiting = offer.status == OfferStatus::AwaitingBuyer;
          const int days =
              std::max(0, dayNumber(offer.expires) - dayNumber(today));
          decision.options.push_back(
              {offer.id,
               awaiting
                   ? fmt::sprintf(LOC("INBOX_DECISION_AWAITING_OPTION"),
                                  buyer_name,
                                  Format::dayMonth(offer.respond_on).c_str())
                   : fmt::sprintf(
                         Format::plural(offer.loan
                                            ? "INBOX_DECISION_LOAN_OPTION"
                                            : "INBOX_DECISION_OFFER_OPTION",
                                        days),
                         buyer_name,
                         Format::money(offer.loan ? offer.loan_terms.loan_fee
                                                  : offer.terms.fee)
                             .c_str(),
                         days),
               !offer.loan, awaiting});
        }
      }
      else if (action == InboxAction::YouthTrialists)
      {
        for (const auto& trialist :
             controller.getYouthPlayers(YouthStatus::Candidate))
          decision.options.push_back(
              {trialist.id,
               fmt::sprintf(
                   LOC("INBOX_DECISION_TRIALIST_OPTION"), trialist.name.c_str(),
                   RoleUtils::shortName(trialist.role), trialist.age,
                   static_cast<int>(trialist.estimate.potential_low),
                   static_cast<int>(trialist.estimate.potential_high))});
      }
      continue;
    }
    if (Inbox::isArchived(message, today))
    {
      ++archived_count;
      if (!show_archived) hidden[index] = true;
    }
  }
  if (tab < 0) tab = decisions.empty() ? 1 : 0;
}

void InboxScene::rebuildThreads()
{
  const auto& messages = guiView->getController().getInbox();
  threads.clear();
  unread_by_category.fill(0);
  total_by_category.fill(0);
  const auto isHidden = [this](size_t index)
  { return index < hidden.size() && hidden[index]; };
  for (size_t index = 0; index < messages.size(); ++index)
  {
    const InboxMessage& message = messages[index];
    const auto category = static_cast<size_t>(message.category);
    if (category >= CATEGORY_COUNT || isHidden(index)) continue;
    ++total_by_category[category];
    if (!message.read) ++unread_by_category[category];
  }

  // Newest first; fold same-kind messages from the same week into a digest.
  int groupStartDay = 0;
  for (size_t reverse = messages.size(); reverse > 0; --reverse)
  {
    const size_t index = reverse - 1;
    const InboxMessage& message = messages[index];
    if (isHidden(index)) continue;
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
    selected_message_id = messages[messageIndex].id;
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
  const std::string decisionsLabel =
      fmt::sprintf(LOC("INBOX_TAB_DECISIONS"), decisions.size());
  const std::array<const char*, 2> tabs = {decisionsLabel.c_str(),
                                           LOC("INBOX_TAB_INFO")};
  UI::segmented("##inbox_tab", tab, tabs);
  if (tab == 0)
  {
    renderDecisions();
    return;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(controller.getUnreadInboxCount() == 0);
  if (UI::secondaryButton(LOC("INBOX_MARK_ALL_READ")))
  {
    controller.markAllInboxMessagesRead();
    rebuildThreads();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Checkbox(LOC("INBOX_UNREAD_ONLY"), &unread_only)) rebuildThreads();
  ImGui::SameLine();
  const std::string archivedLabel =
      fmt::sprintf(LOC("INBOX_SHOW_ARCHIVED"), archived_count);
  if (ImGui::Checkbox(archivedLabel.c_str(), &show_archived))
  {
    selected_message = SIZE_MAX;
    refresh();
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    ImGui::SetTooltip(
        "%s",
        fmt::sprintf(LOC("INBOX_ARCHIVE_HELP"), Inbox::ARCHIVE_DAYS).c_str());

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
  // Opening an unread message rebuilds `threads`: it happens after the loop.
  std::optional<size_t> toOpen;
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
      toOpen = thread.messages.front();
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
      const size_t children =
          std::min(thread.messages.size(), thread.message_titles.size());
      for (size_t child = 0; child < children; ++child)
      {
        const size_t messageIndex = thread.messages[child];
        ImGui::PushID(static_cast<int>(messageIndex));
        if (ImGui::Selectable(thread.message_titles[child].c_str(),
                              selected_message == messageIndex))
          toOpen = messageIndex;
        ImGui::PopID();
      }
      ImGui::Unindent(Theme::Space::L * Theme::scale());
    }
    ImGui::PopID();
  }
  if (toOpen) openMessage(*toOpen);
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
  GameController& controller = guiView->getController();
  if (Inbox::actionFor(message.title_key) == InboxAction::Shortlist &&
      message.player_id && !controller.isShortlisted(*message.player_id) &&
      controller.getManagedTeam() &&
      controller.getScoutedRow(*message.player_id).has_value())
  {
    const auto player = controller.getGameData()->getPlayer(*message.player_id);
    if (player &&
        player->get().getTeamId() != controller.getManagedTeam()->get().getId())
    {
      if (message.player_id || message.team_id) ImGui::SameLine();
      if (UI::primaryButton(LOC("INBOX_SHORTLIST")) &&
          controller.addToShortlist(*message.player_id))
        showToast(LOC("INBOX_SHORTLISTED"));
    }
  }
  talk_dialog.inboxAction(controller, message);
  talk_dialog.render(controller);
  UI::endCard();
}

void InboxScene::renderDecisions()
{
  GameController& controller = guiView->getController();
  if (decisions.empty())
  {
    UI::emptyState(LOC("INBOX_DECISIONS_EMPTY_TITLE"),
                   LOC("INBOX_DECISIONS_EMPTY_BODY"));
    talk_dialog.render(controller);
    // Still drawn when the talks just ended the last decision.
    offer_dialog.render(controller);
    return;
  }
  const size_t count = decisions.size();
  for (size_t index = 0; index < count && index < decisions.size(); ++index)
  {
    ImGui::PushID(static_cast<int>(index));
    renderDecision(decisions[index]);
    ImGui::PopID();
  }
  if (talk_dialog.render(controller)) refresh();
  if (offer_dialog.render(controller)) refresh();
}

void InboxScene::renderDecision(const Decision& decision)
{
  GameController& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  const auto& messages = controller.getInbox();
  if (decision.message >= messages.size()) return;
  const InboxMessage& message = messages[decision.message];
  UI::beginAutoHeightCard("decision", nullptr, 0.0f);
  UI::badge(LOC(inboxCategoryKey(message.category)),
            categoryColor(message.category));
  ImGui::SameLine();
  ImGui::TextColored(palette.muted, "%s", decision.date_text.c_str());
  {
    Theme::ScopedText title(Theme::Text::TITLE);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(decision.title.c_str());
    ImGui::PopTextWrapPos();
  }
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(palette.muted, "%s", decision.body.c_str());
  ImGui::PopTextWrapPos();

  // Actions post new messages (the inbox vector may reallocate or drop its
  // oldest entry): only the id is used afterwards.
  const uint32_t messageId = message.id;
  bool changed = false;
  const auto act = [&](bool ok, const char* done)
  {
    controller.markInboxMessageRead(messageId);
    showToast(LOC(ok ? done : "INBOX_DECISION_FAILED"), !ok);
    changed = true;
  };
  const auto optionRow =
      [&](const Decision::Option& option, const char* yes, const char* no)
  {
    ImGui::PushID(static_cast<int>(option.id));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(option.text.c_str());
    const float buttons = UI::buttonWidth(yes, UI::ButtonSize::COMPACT) +
                          UI::buttonWidth(no, UI::ButtonSize::COMPACT) +
                          ImGui::GetStyle().ItemSpacing.x;
    if (UI::sameLineIfFits(buttons))
      ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                    ImGui::GetContentRegionMax().x - buttons));
    const bool accepted =
        UI::primaryButton(yes, ImVec2(0.0f, 0.0f), UI::ButtonSize::COMPACT);
    ImGui::SameLine();
    const bool rejected =
        UI::secondaryButton(no, ImVec2(0.0f, 0.0f), UI::ButtonSize::COMPACT);
    ImGui::PopID();
    return accepted ? 1 : rejected ? -1 : 0;
  };

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
  switch (decision.action)
  {
    case InboxAction::RespondOffer:
      for (const Decision::Option& option : decision.options)
      {
        if (option.negotiable)
        {
          // Transfer bids are answered in the talks: accept, reject,
          // counter, name a price or not for sale.
          ImGui::PushID(static_cast<int>(option.id));
          ImGui::AlignTextToFramePadding();
          ImGui::TextUnformatted(option.text.c_str());
          const char* label = LOC(option.awaiting ? "INBOX_DECISION_VIEW_TALKS"
                                                  : "INBOX_DECISION_NEGOTIATE");
          const float button = UI::buttonWidth(label, UI::ButtonSize::COMPACT);
          if (UI::sameLineIfFits(button))
            ImGui::SetCursorPosX(std::max(
                ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - button));
          if (UI::primaryButton(label, ImVec2(0.0f, 0.0f),
                                UI::ButtonSize::COMPACT))
          {
            controller.markInboxMessageRead(messageId);
            offer_dialog.open(controller, option.id);
          }
          ImGui::PopID();
          continue;
        }
        const int answer = optionRow(option, LOC("INBOX_DECISION_ACCEPT"),
                                     LOC("INBOX_DECISION_REJECT"));
        if (answer > 0)
          act(controller.acceptIncomingOffer(option.id),
              "INBOX_DECISION_ACCEPTED");
        else if (answer < 0)
          act(controller.rejectIncomingOffer(option.id),
              "INBOX_DECISION_REJECTED");
        if (changed) break;
      }
      break;
    case InboxAction::YouthTrialists:
      for (const Decision::Option& option : decision.options)
      {
        const int answer = optionRow(option, LOC("INBOX_DECISION_SIGN"),
                                     LOC("INBOX_DECISION_RELEASE"));
        if (answer > 0)
          act(controller.signYouthCandidate(option.id) == YouthActionResult::Ok,
              "INBOX_DECISION_SIGNED");
        else if (answer < 0)
          act(controller.releaseYouthCandidate(option.id) ==
                  YouthActionResult::Ok,
              "INBOX_DECISION_RELEASED");
        if (changed) break;
      }
      break;
    case InboxAction::ReplyToPlayer:
      if (UI::primaryButton(LOC("TALK_INBOX_REPLY")))
      {
        controller.markInboxMessageRead(messageId);
        talk_dialog.open(controller, decision.player);
      }
      break;
    case InboxAction::Shortlist:
    case InboxAction::None:
      break;
  }
  if (!changed && decision.player != 0)
  {
    if (UI::link(LOC("INBOX_OPEN_PLAYER"), "open_player"))
      Navigation::openPlayer(guiView, decision.player);
  }
  UI::endCard();
  if (changed) refresh();
}
