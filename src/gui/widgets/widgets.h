// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <cstdint>
#include <span>
#include <string_view>

/**
 * @brief Reusable, theme-aware widgets for management screens.
 *
 * All widgets draw with the current Theme palette, never allocate per call
 * beyond ImGui's own buffers, and keep ImGui's ID/layout semantics so they can
 * be mixed freely with regular ImGui calls.
 */
namespace UI
{

/** @brief Result of a modal confirmation dialog. */
enum class DialogResult : uint8_t
{
  NONE,
  CONFIRM,
  CANCEL
};

/** @brief Match outcome for form strips. */
enum class Outcome : uint8_t
{
  WIN,
  DRAW,
  LOSS
};

/** @brief One bar in a horizontal bar chart. */
struct BarDatum
{
  std::string_view label;
  float value = 0.0f;
  ImVec4 color;
  std::string_view valueText;
};

/** @brief Large page title with an optional muted subtitle. */
void pageHeader(const char* title, const char* subtitle = nullptr);

/** @brief Small uppercase section label. */
void sectionLabel(const char* text);

/**
 * @brief Starts a bordered card. Always pair with endCard().
 * @param id Stable ImGui id.
 * @param title Optional caption drawn at the top of the card.
 * @param size Card size (0 = fill available on that axis).
 * @param scroll Allow the card body to scroll.
 */
void beginCard(const char* id, const char* title, ImVec2 size,
               bool scroll = false);

/** @brief Starts a full-width card that grows to fit its content. */
void beginAutoHeightCard(const char* id, const char* title);

/** @brief Ends a card started with beginCard() or beginAutoHeightCard(). */
void endCard();

/** @brief Figure tile: caption, big value and an optional footnote. */
void statTile(const char* id, const char* caption, const char* value,
              const char* footnote, const ImVec4& valueColor, float width);

/** @brief Height used by statTile(), for layout calculations. */
float statTileHeight();

/**
 * @brief Labelled horizontal bar coloured with the rating scale.
 * @param width Total row width; 0 uses the available width.
 */
void attributeBar(const char* label, float value, float labelWidth,
                  float maxValue = 100.0f, float width = 0.0f);

/** @brief Labelled bar with an explicit colour and value text (0-1). */
void meter(const char* label, float fraction, float labelWidth,
           const ImVec4& color, const char* valueText);

/** @brief Rounded rating chip, coloured by the rating scale. */
void ratingChip(double rating);

/** @brief Small coloured badge with text. */
void badge(const char* text, const ImVec4& color);

/** @brief Row of W/D/L markers (latest last). */
void formStrip(std::span<const Outcome> outcomes);

/** @brief Muted placeholder shown when a list has nothing to display. */
void emptyState(const char* title, const char* body);

/**
 * @brief Modal confirmation. Open with ImGui::OpenPopup(id) beforehand.
 * @return CONFIRM or CANCEL on the frame a button is pressed, else NONE.
 */
DialogResult confirmDialog(const char* id, const char* title, const char* body,
                           const char* confirmLabel, const char* cancelLabel);

/** @brief Line chart of a series in a fixed box. */
void sparkline(const char* id, std::span<const float> values, ImVec2 size,
               const ImVec4& color);

/** @brief Bars above/below a zero line (positive green, negative red). */
void deltaBars(const char* id, std::span<const float> values, ImVec2 size);

/** @brief Horizontal bars with labels and values, scaled to the maximum. */
void barChart(const char* id, std::span<const BarDatum> bars, float width,
              float labelWidth);

/**
 * @brief Begins a data table that scrolls horizontally only when the region
 * is narrower than minWidth, freezing the header row (and freezeColumns
 * leading columns while scrolling). Pair with ImGui::EndTable() when true.
 */
bool beginDataTable(const char* id, int columns, ImGuiTableFlags flags,
                    float minWidth, ImVec2 size, int freezeColumns = 1);

/** @brief Accent-filled call-to-action button. */
bool primaryButton(const char* label, ImVec2 size = ImVec2(0, 0));

/** @brief Selectable text that looks like a link; returns true on click. */
bool link(const char* label, const char* id);

/** @brief Muted key on the left, value on the right of the current row. */
void keyValue(const char* key, const char* value, float keyWidth);

/** @brief SameLine() only when an item of the given width still fits. */
void sameLineIfFits(float nextWidth);

/** @brief Width of a regular button with this label. */
float buttonWidth(const char* label);

/** @brief Text right-aligned in the current table cell or content region. */
void textRight(const char* text);

/** @brief Coloured text right-aligned in the current cell. */
void textRightColored(const ImVec4& color, const char* text);

/** @brief Strictly-weak three-way comparison helper for table sorting. */
template <typename T>
int compare(const T& left, const T& right)
{
  return left < right ? -1 : (right < left ? 1 : 0);
}

}  // namespace UI
