// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/widgets/widgets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "global/language_manager.h"
#include "gui/widgets/theme.h"

namespace
{
constexpr float CARD_TITLE_GAP = 6.0f;
constexpr float BAR_HEIGHT = 7.0f;
constexpr float VALUE_COLUMN_WIDTH = 34.0f;
constexpr float FORM_MARKER_SIZE = 18.0f;
constexpr float DIALOG_TEXT_WIDTH = 440.0f;
constexpr float DIALOG_BUTTON_WIDTH = 150.0f;

float scaled(float value) { return value * ImGui::GetStyle().FontScaleDpi; }
}  // namespace

namespace UI
{

void pageHeader(const char* title, const char* subtitle)
{
  {
    Theme::ScopedText heading(Theme::Text::HEADING);
    ImGui::TextUnformatted(title);
  }
  if (subtitle != nullptr && *subtitle != '\0')
  {
    Theme::ScopedText small(Theme::Text::SMALL);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::palette().muted);
    ImGui::TextUnformatted(subtitle);
    ImGui::PopStyleColor();
  }
  ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::XS)));
}

void sectionLabel(const char* text)
{
  Theme::ScopedText caption(Theme::Text::CAPTION);
  ImGui::PushStyleColor(ImGuiCol_Text, Theme::palette().muted);
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

namespace
{
void openCard(const char* id, const char* title, ImVec2 size,
              ImGuiChildFlags extraFlags, ImGuiWindowFlags windowFlags)
{
  ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::palette().surface);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(scaled(Theme::Space::M), scaled(Theme::Space::M)));
  ImGui::BeginChild(id, size,
                    ImGuiChildFlags_Borders |
                        ImGuiChildFlags_AlwaysUseWindowPadding | extraFlags,
                    windowFlags);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  if (title != nullptr && *title != '\0')
  {
    sectionLabel(title);
    ImGui::Dummy(
        ImVec2(0.0f, scaled(CARD_TITLE_GAP) - ImGui::GetStyle().ItemSpacing.y));
  }
}
}  // namespace

void beginCard(const char* id, const char* title, ImVec2 size, bool scroll)
{
  ImGuiWindowFlags flags = ImGuiWindowFlags_None;
  if (!scroll)
    flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  openCard(id, title, size, ImGuiChildFlags_None, flags);
}

void beginAutoHeightCard(const char* id, const char* title)
{
  openCard(id, title, ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY,
           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

void endCard() { ImGui::EndChild(); }

float statTileHeight()
{
  const ImGuiStyle& style = ImGui::GetStyle();
  const float dpi = style.FontScaleDpi;
  return (Theme::textSize(Theme::Text::CAPTION) +
          Theme::textSize(Theme::Text::DISPLAY) +
          Theme::textSize(Theme::Text::SMALL)) *
             dpi +
         2.0f * style.ItemSpacing.y * 0.5f + 2.0f * scaled(Theme::Space::M) +
         scaled(Theme::Space::XS);
}

void statTile(const char* id, const char* caption, const char* value,
              const char* footnote, const ImVec4& valueColor, float width)
{
  beginCard(id, nullptr, ImVec2(width, statTileHeight()));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(ImGui::GetStyle().ItemSpacing.x,
                             ImGui::GetStyle().ItemSpacing.y * 0.5f));
  sectionLabel(caption);
  {
    // Long values (club names, objectives) step down a size instead of
    // overflowing the tile; the line height stays that of DISPLAY.
    const float available = ImGui::GetContentRegionAvail().x;
    const float lineStart = ImGui::GetCursorPosY();
    Theme::Text level = Theme::Text::DISPLAY;
    for (const Theme::Text candidate :
         {Theme::Text::DISPLAY, Theme::Text::HEADING, Theme::Text::TITLE,
          Theme::Text::BODY})
    {
      level = candidate;
      Theme::ScopedText probe(candidate);
      if (ImGui::CalcTextSize(value).x <= available) break;
    }
    const float displayHeight =
        Theme::textSize(Theme::Text::DISPLAY) * ImGui::GetStyle().FontScaleDpi;
    Theme::ScopedText fitted(level);
    ImGui::SetCursorPosY(lineStart +
                         (displayHeight - ImGui::GetTextLineHeight()) * 0.5f);
    const ImVec2 clipMin = ImGui::GetCursorScreenPos();
    ImGui::PushClipRect(
        clipMin, ImVec2(clipMin.x + available, clipMin.y + displayHeight),
        true);
    ImGui::TextColored(valueColor, "%s", value);
    ImGui::PopClipRect();
    ImGui::SetCursorPosY(lineStart + displayHeight);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
  }
  if (footnote != nullptr && *footnote != '\0')
  {
    Theme::ScopedText small(Theme::Text::SMALL);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::palette().muted);
    ImGui::TextUnformatted(footnote);
    ImGui::PopStyleColor();
  }
  ImGui::PopStyleVar();
  endCard();
}

namespace
{
void barRow(const char* label, float fraction, float labelWidth,
            const ImVec4& color, const char* valueText, float width)
{
  const Theme::Palette& palette = Theme::palette();
  const float lineHeight = ImGui::GetTextLineHeight();
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float available =
      width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
  const float valueWidth =
      std::max(scaled(VALUE_COLUMN_WIDTH), ImGui::CalcTextSize(valueText).x);
  const float barWidth =
      std::max(scaled(24.0f),
               available - labelWidth - valueWidth - scaled(Theme::Space::S));

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->PushClipRect(
      start, ImVec2(start.x + labelWidth - scaled(4.0f), start.y + lineHeight),
      true);
  drawList->AddText(start, Theme::toU32(palette.muted), label);
  drawList->PopClipRect();

  const float barHeight = scaled(BAR_HEIGHT);
  const ImVec2 barMin(start.x + labelWidth,
                      start.y + (lineHeight - barHeight) * 0.5f);
  const ImVec2 barMax(barMin.x + barWidth, barMin.y + barHeight);
  const float clamped = std::clamp(fraction, 0.0f, 1.0f);
  drawList->AddRectFilled(barMin, barMax, Theme::toU32(palette.raised),
                          barHeight * 0.5f);
  if (clamped > 0.0f)
    drawList->AddRectFilled(barMin,
                            ImVec2(barMin.x + barWidth * clamped, barMax.y),
                            Theme::toU32(color), barHeight * 0.5f);
  const float textWidth = ImGui::CalcTextSize(valueText).x;
  drawList->AddText(ImVec2(start.x + available - textWidth, start.y),
                    Theme::toU32(color), valueText);
  ImGui::Dummy(ImVec2(available, lineHeight));
}
}  // namespace

void attributeBar(const char* label, float value, float labelWidth,
                  float maxValue, float width)
{
  std::array<char, 16> text{};
  std::snprintf(text.data(), text.size(), "%.0f", static_cast<double>(value));
  barRow(label, value / maxValue, labelWidth,
         Theme::ratingColor(value / maxValue * 100.0f), text.data(), width);
}

void meter(const char* label, float fraction, float labelWidth,
           const ImVec4& color, const char* valueText)
{
  barRow(label, fraction, labelWidth, color, valueText, 0.0f);
}

void ratingChip(double rating)
{
  std::array<char, 16> text{};
  std::snprintf(text.data(), text.size(), "%.0f", rating);
  const ImVec2 textSize = ImGui::CalcTextSize(text.data());
  const float padX = scaled(6.0f);
  const ImVec2 size(std::max(textSize.x + 2.0f * padX, scaled(32.0f)),
                    textSize.y + scaled(2.0f));
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const ImVec4 color = Theme::ratingColor(rating);
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(start, ImVec2(start.x + size.x, start.y + size.y),
                          Theme::toU32(color, 0.18f), scaled(4.0f));
  drawList->AddText(ImVec2(start.x + (size.x - textSize.x) * 0.5f,
                           start.y + (size.y - textSize.y) * 0.5f),
                    Theme::toU32(color), text.data());
  ImGui::Dummy(size);
}

void badge(const char* text, const ImVec4& color)
{
  Theme::ScopedText caption(Theme::Text::CAPTION);
  const ImVec2 textSize = ImGui::CalcTextSize(text);
  const ImVec2 padding(scaled(6.0f), scaled(2.0f));
  const ImVec2 size(textSize.x + 2.0f * padding.x,
                    textSize.y + 2.0f * padding.y);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(start, ImVec2(start.x + size.x, start.y + size.y),
                          Theme::toU32(color, 0.20f), scaled(3.0f));
  drawList->AddText(ImVec2(start.x + padding.x, start.y + padding.y),
                    Theme::toU32(color), text);
  ImGui::Dummy(size);
}

void formStrip(std::span<const Outcome> outcomes)
{
  const Theme::Palette& palette = Theme::palette();
  Theme::ScopedText caption(Theme::Text::CAPTION);
  const float size = scaled(FORM_MARKER_SIZE);
  const float gap = scaled(3.0f);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
  const float top = start.y + std::max(0.0f, (lineHeight - size) * 0.5f);
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  float x = start.x;
  for (const Outcome outcome : outcomes)
  {
    const ImVec4& color = outcome == Outcome::WIN    ? palette.positive
                          : outcome == Outcome::DRAW ? palette.faint
                                                     : palette.negative;
    const char* letter = outcome == Outcome::WIN    ? LOC("FORM_WIN_SHORT")
                         : outcome == Outcome::DRAW ? LOC("FORM_DRAW_SHORT")
                                                    : LOC("FORM_LOSS_SHORT");
    drawList->AddRectFilled(ImVec2(x, top), ImVec2(x + size, top + size),
                            Theme::toU32(color), scaled(3.0f));
    const ImVec2 letterSize = ImGui::CalcTextSize(letter);
    drawList->AddText(ImVec2(x + (size - letterSize.x) * 0.5f,
                             top + (size - letterSize.y) * 0.5f),
                      IM_COL32(12, 14, 18, 255), letter);
    x += size + gap;
  }
  const auto count = static_cast<float>(outcomes.size());
  ImGui::Dummy(ImVec2(count > 0.0f ? count * (size + gap) - gap : 0.0f, size));
}

void emptyState(const char* title, const char* body)
{
  const float width = ImGui::GetContentRegionAvail().x;
  ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::L)));
  {
    Theme::ScopedText heading(Theme::Text::TITLE);
    const float titleWidth = ImGui::CalcTextSize(title).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         std::max(0.0f, (width - titleWidth) * 0.5f));
    ImGui::TextUnformatted(title);
  }
  if (body != nullptr && *body != '\0')
  {
    Theme::ScopedText small(Theme::Text::SMALL);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::palette().muted);
    const float wrap = std::min(width, scaled(DIALOG_TEXT_WIDTH));
    const float bodyWidth =
        std::min(wrap, ImGui::CalcTextSize(body, nullptr, false, wrap).x);
    const float indent = std::max(0.0f, (width - bodyWidth) * 0.5f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + bodyWidth + 1.0f);
    ImGui::TextUnformatted(body);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }
  ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::L)));
}

DialogResult confirmDialog(const char* id, const char* title, const char* body,
                           const char* confirmLabel, const char* cancelLabel)
{
  DialogResult result = DialogResult::NONE;
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (!ImGui::BeginPopupModal(id, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoSavedSettings))
    return result;
  {
    Theme::ScopedText heading(Theme::Text::TITLE);
    ImGui::TextUnformatted(title);
  }
  ImGui::PushTextWrapPos(scaled(DIALOG_TEXT_WIDTH));
  ImGui::PushStyleColor(ImGuiCol_Text, Theme::palette().muted);
  ImGui::TextUnformatted(body);
  ImGui::PopStyleColor();
  ImGui::PopTextWrapPos();
  ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::S)));
  const ImVec2 buttonSize(scaled(DIALOG_BUTTON_WIDTH), 0.0f);
  if (ImGui::Button(cancelLabel, buttonSize) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape))
  {
    result = DialogResult::CANCEL;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (primaryButton(confirmLabel, buttonSize))
  {
    result = DialogResult::CONFIRM;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
  return result;
}

void sparkline(const char* id, std::span<const float> values, ImVec2 size,
               const ImVec4& color)
{
  ImGui::PushID(id);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  ImGui::Dummy(size);
  ImGui::PopID();
  if (values.size() < 2) return;
  const auto [minimum, maximum] = std::ranges::minmax_element(values);
  const float low = *minimum;
  const float range = std::max(*maximum - low, 1e-6f);
  const float step = size.x / static_cast<float>(values.size() - 1);
  const float inset = scaled(2.0f);
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 previous;
  for (size_t index = 0; index < values.size(); ++index)
  {
    const ImVec2 point(
        start.x + step * static_cast<float>(index),
        start.y + inset +
            (size.y - 2.0f * inset) * (1.0f - (values[index] - low) / range));
    if (index > 0)
      drawList->AddLine(previous, point, Theme::toU32(color), scaled(1.6f));
    previous = point;
  }
  drawList->AddCircleFilled(previous, scaled(2.5f), Theme::toU32(color));
}

void deltaBars(const char* id, std::span<const float> values, ImVec2 size)
{
  const Theme::Palette& palette = Theme::palette();
  ImGui::PushID(id);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  ImGui::Dummy(size);
  ImGui::PopID();
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const float middle = start.y + size.y * 0.5f;
  drawList->AddLine(ImVec2(start.x, middle), ImVec2(start.x + size.x, middle),
                    Theme::toU32(palette.border));
  if (values.empty()) return;
  float largest = 1e-6f;
  for (const float value : values) largest = std::max(largest, std::abs(value));
  const float slot = size.x / static_cast<float>(values.size());
  const float barWidth = std::max(2.0f, slot * 0.6f);
  for (size_t index = 0; index < values.size(); ++index)
  {
    const float height = values[index] / largest * (size.y * 0.5f - 1.0f);
    const float x = start.x + slot * (static_cast<float>(index) + 0.5f);
    drawList->AddRectFilled(
        ImVec2(x - barWidth * 0.5f, std::min(middle, middle - height)),
        ImVec2(x + barWidth * 0.5f, std::max(middle, middle - height)),
        Theme::toU32(values[index] >= 0.0f ? palette.positive
                                           : palette.negative),
        scaled(1.5f));
  }
}

void barChart(const char* id, std::span<const BarDatum> bars, float width,
              float labelWidth)
{
  ImGui::PushID(id);
  const Theme::Palette& palette = Theme::palette();
  float maximum = 0.0f;
  for (const BarDatum& bar : bars) maximum = std::max(maximum, bar.value);
  const float lineHeight = ImGui::GetTextLineHeight();
  const float valueWidth = scaled(70.0f);
  const float trackWidth =
      std::max(scaled(20.0f), width - labelWidth - valueWidth);
  const float barHeight = lineHeight * 0.62f;
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  for (const BarDatum& bar : bars)
  {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    drawList->PushClipRect(
        start,
        ImVec2(start.x + labelWidth - scaled(6.0f), start.y + lineHeight),
        true);
    drawList->AddText(start, Theme::toU32(palette.muted), bar.label.data(),
                      bar.label.data() + bar.label.size());
    drawList->PopClipRect();
    const ImVec2 barMin(start.x + labelWidth,
                        start.y + (lineHeight - barHeight) * 0.5f);
    drawList->AddRectFilled(barMin,
                            ImVec2(barMin.x + trackWidth, barMin.y + barHeight),
                            Theme::toU32(palette.raised), scaled(2.0f));
    const float fraction = maximum > 0.0f ? bar.value / maximum : 0.0f;
    if (fraction > 0.0f)
      drawList->AddRectFilled(
          barMin,
          ImVec2(barMin.x + trackWidth * fraction, barMin.y + barHeight),
          Theme::toU32(bar.color), scaled(2.0f));
    drawList->AddText(
        ImVec2(barMin.x + trackWidth + scaled(Theme::Space::S), start.y),
        Theme::toU32(palette.text), bar.valueText.data(),
        bar.valueText.data() + bar.valueText.size());
    ImGui::Dummy(ImVec2(width, lineHeight));
  }
  ImGui::PopID();
}

bool beginDataTable(const char* id, int columns, ImGuiTableFlags flags,
                    float minWidth, ImVec2 size, int freezeColumns)
{
  const float scaledMinimum = scaled(minWidth);
  const bool scrollX = ImGui::GetContentRegionAvail().x < scaledMinimum;
  if (scrollX) flags |= ImGuiTableFlags_ScrollX;
  if (!ImGui::BeginTable(id, columns, flags, size,
                         scrollX ? scaledMinimum : 0.0f))
    return false;
  if ((flags & ImGuiTableFlags_ScrollY) != 0)
    ImGui::TableSetupScrollFreeze(scrollX ? freezeColumns : 0, 1);
  return true;
}

bool primaryButton(const char* label, ImVec2 size)
{
  const ImVec4& accent = Theme::palette().accent;
  ImGui::PushStyleColor(ImGuiCol_Button, accent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(std::min(1.0f, accent.x * 1.15f + 0.03f),
                               std::min(1.0f, accent.y * 1.15f + 0.03f),
                               std::min(1.0f, accent.z * 1.15f + 0.03f), 1.0f));
  ImGui::PushStyleColor(
      ImGuiCol_ButtonActive,
      ImVec4(accent.x * 0.82f, accent.y * 0.82f, accent.z * 0.82f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_Text, Theme::palette().on_accent);
  const bool pressed = ImGui::Button(label, size);
  ImGui::PopStyleColor(4);
  return pressed;
}

bool link(const char* label, const char* id)
{
  ImGui::PushID(id);
  const ImVec2 size = ImGui::CalcTextSize(label, nullptr, true);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton("##link", size);
  const bool hovered = ImGui::IsItemHovered();
  ImGui::PopID();
  const ImVec4 color =
      ImGui::GetStyleColorVec4(hovered ? ImGuiCol_Text : ImGuiCol_TextLink);
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const char* hidden = std::strstr(label, "##");
  drawList->AddText(start, Theme::toU32(color), label, hidden);
  if (hovered)
  {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    drawList->AddLine(ImVec2(start.x, start.y + size.y - 1.0f),
                      ImVec2(start.x + size.x, start.y + size.y - 1.0f),
                      Theme::toU32(color), 1.0f);
  }
  return clicked;
}

void keyValue(const char* key, const char* value, float keyWidth)
{
  const float startX = ImGui::GetCursorPosX();
  ImGui::PushStyleColor(ImGuiCol_Text, Theme::palette().muted);
  ImGui::TextUnformatted(key);
  ImGui::PopStyleColor();
  // Long (translated) labels push the value right instead of overlapping.
  ImGui::SameLine(std::max(startX + keyWidth,
                           ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x +
                               ImGui::GetStyle().ItemSpacing.x * 2.0f));
  ImGui::TextUnformatted(value);
}

void sameLineIfFits(float nextWidth)
{
  const float lineEnd =
      ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
  if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + nextWidth <=
      lineEnd)
    ImGui::SameLine();
}

float buttonWidth(const char* label)
{
  return ImGui::CalcTextSize(label, nullptr, true).x +
         2.0f * ImGui::GetStyle().FramePadding.x;
}

void textRight(const char* text)
{
  const float width = ImGui::CalcTextSize(text).x;
  const float available = ImGui::GetContentRegionAvail().x;
  if (available > width)
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available - width);
  ImGui::TextUnformatted(text);
}

void textRightColored(const ImVec4& color, const char* text)
{
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  textRight(text);
  ImGui::PopStyleColor();
}

}  // namespace UI
