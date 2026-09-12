// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <span>
#include <string>

#include "gui/widgets/widgets.h"
#include "model/transfer_negotiation.h"

/**
 * @namespace TransferTermsEditor
 * @brief Pieces shared by the transfer dialogs: the structured offer editor
 * (fee, upfront share, instalments, add-ons, sell-on), captions, panels and
 * right-aligned action rows.
 */
namespace TransferTermsEditor
{
// Every value the buyer AI can propose is a segment (aiOfferTerms pays 50%
// upfront, a spread goes down to 30% and 20%, a sell-on cut ends on 5% or
// 25%): the highlighted segment is always the value that will be sent.
inline constexpr std::array<std::uint8_t, 7> UPFRONT_OPTIONS = {
    100, 80, 60, 50, 40, 30, 20};
inline constexpr std::array<const char*, 7> UPFRONT_LABELS = {
    "100%", "80%", "60%", "50%", "40%", "30%", "20%"};
inline constexpr std::array<const char*, 5> YEAR_LABELS = {"1", "2", "3", "4",
                                                           "5"};
inline constexpr std::array<std::uint16_t, 4> APPEARANCE_TARGETS = {10, 20, 30,
                                                                   50};
inline constexpr std::array<std::uint16_t, 4> GOAL_TARGETS = {5, 10, 15, 20};
inline constexpr std::array<std::uint8_t, 7> SELL_ON_OPTIONS = {
    0, 5, 10, 15, 20, 25, 30};
inline constexpr std::array<const char*, 7> SELL_ON_LABELS = {
    "0%", "5%", "10%", "15%", "20%", "25%", "30%"};
inline constexpr std::uint8_t DEFAULT_INSTALMENT_YEARS = 2;
inline constexpr std::int64_t FEE_STEP = 10'000;

/** Index of @p value among @p options (the nearest one if absent). */
template <typename T, typename V>
int optionIndex(std::span<const T> options, V value)
{
  std::size_t best = 0;
  for (std::size_t index = 1; index < options.size(); ++index)
  {
    if (std::abs(static_cast<int>(options[index]) - static_cast<int>(value)) <
        std::abs(static_cast<int>(options[best]) - static_cast<int>(value)))
      best = index;
  }
  return static_cast<int>(best);
}

template <typename T, std::size_t N, typename V>
int optionIndex(const std::array<T, N>& options, V value)
{
  return optionIndex(std::span<const T>(options), value);
}

/** Small muted caption above a dialog field. */
void formLabel(const char* text);

/** Moves the cursor so the buttons labelled @p labels end at the right
 * edge (null labels are skipped). */
void alignActions(std::initializer_list<const char*> labels);

/**
 * Card-looking panel for dialogs, built on a one-cell table instead of a
 * child window so an auto-sized dialog fits its content on the first frame.
 * Pair with endPanel() when it returns true.
 */
bool beginPanel(const char* id, const char* title);
void endPanel();

/**
 * Snaps a structure onto the editor's segments (upfront share, instalment
 * years, add-on targets, sell-on), so what is highlighted is what is sent.
 * Every value the buyer AI proposes is already a segment; this only moves
 * values of older saves. Returns true when something changed.
 */
bool snapToOptions(TransferNegotiation::OfferTerms& terms);

/**
 * Editor of a structured club-to-club offer: the fee (with @p fee_chips),
 * the upfront share and instalment years, appearance and goal add-ons with
 * their targets and the sell-on share. Returns true when a value changed.
 */
bool edit(TransferNegotiation::OfferTerms& terms,
          std::span<const UI::MoneyChip> fee_chips);

/** Payment structure in words: "Cash" or "60% upfront, 3 yrs". */
std::string structureText(const TransferNegotiation::OfferTerms& terms);

}  // namespace TransferTermsEditor
