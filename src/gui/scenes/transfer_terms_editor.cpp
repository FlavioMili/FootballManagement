// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/transfer_terms_editor.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <limits>

#include "global/language_manager.h"
#include "gui/widgets/theme.h"
#include "model/transfer_tuning.h"

namespace TransferTermsEditor
{
void formLabel(const char* text)
{
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
  Theme::ScopedText caption(Theme::Text::SMALL);
  ImGui::TextColored(Theme::palette().muted, "%s", text);
}

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

bool beginPanel(const char* id, const char* title)
{
  const float pad = Theme::Space::M * Theme::scale();
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

namespace
{
/** Target segment labels ("20 apps"), rebuilt only when the language's
 * pattern changes, never per frame. */
struct TargetLabels
{
  std::string pattern;
  std::array<std::string, 4> texts;
  std::array<const char*, 4> labels{};
};

std::span<const char* const> targetLabels(TargetLabels& cache, const char* key,
                                          std::span<const std::uint16_t> options)
{
  const char* pattern = LOC(key);
  if (cache.pattern != pattern)
  {
    cache.pattern = pattern;
    for (std::size_t index = 0; index < options.size(); ++index)
    {
      cache.texts[index] = fmt::sprintf(pattern, options[index]);
      cache.labels[index] = cache.texts[index].c_str();
    }
  }
  return std::span<const char* const>(cache.labels).first(options.size());
}

template <typename T, std::size_t N, typename V>
bool snap(const std::array<T, N>& options, V& value)
{
  const T nearest = options[static_cast<std::size_t>(optionIndex(options, value))];
  if (static_cast<int>(nearest) == static_cast<int>(value)) return false;
  value = static_cast<V>(nearest);
  return true;
}
}  // namespace

bool snapToOptions(TransferNegotiation::OfferTerms& terms)
{
  bool changed = snap(UPFRONT_OPTIONS, terms.upfront_percent);
  changed = snap(SELL_ON_OPTIONS, terms.sell_on_percent) || changed;
  if (terms.upfront_percent < 100 && terms.instalment_years == 0)
  {
    terms.instalment_years = DEFAULT_INSTALMENT_YEARS;
    changed = true;
  }
  if (terms.instalment_years > TransferTuning::Offer::MAX_INSTALMENT_YEARS)
  {
    terms.instalment_years = TransferTuning::Offer::MAX_INSTALMENT_YEARS;
    changed = true;
  }
  if (terms.appearance_bonus > 0)
    changed = snap(APPEARANCE_TARGETS, terms.appearance_target) || changed;
  if (terms.goal_bonus > 0)
    changed = snap(GOAL_TARGETS, terms.goal_target) || changed;
  return changed;
}

bool edit(TransferNegotiation::OfferTerms& terms,
          std::span<const UI::MoneyChip> fee_chips)
{
  bool changed = snapToOptions(terms);
  formLabel(LOC("TRANSFER_FIELD_FEE"));
  std::int64_t fee = terms.fee;
  if (UI::moneyInput("##fee", fee,
                     {.minimum = FEE_STEP,
                      .maximum = std::numeric_limits<std::uint32_t>::max(),
                      .chips = fee_chips}))
  {
    terms.fee = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
        fee, FEE_STEP, std::numeric_limits<std::uint32_t>::max()));
    changed = true;
  }

  formLabel(LOC("TRANSFER_FIELD_UPFRONT"));
  int upfrontIndex = optionIndex(UPFRONT_OPTIONS, terms.upfront_percent);
  if (UI::segmented("##upfront", upfrontIndex, UPFRONT_LABELS))
  {
    terms.upfront_percent = UPFRONT_OPTIONS[static_cast<std::size_t>(upfrontIndex)];
    if (terms.upfront_percent < 100 && terms.instalment_years == 0)
      terms.instalment_years = DEFAULT_INSTALMENT_YEARS;
    changed = true;
  }
  if (terms.upfront_percent < 100)
  {
    formLabel(LOC("TRANSFER_FIELD_INSTALMENTS"));
    int yearIndex = std::max(0, terms.instalment_years - 1);
    if (UI::segmented("##years", yearIndex,
                      std::span(YEAR_LABELS).first(
                          TransferTuning::Offer::MAX_INSTALMENT_YEARS)))
    {
      terms.instalment_years = static_cast<std::uint8_t>(yearIndex + 1);
      changed = true;
    }
  }
  else
  {
    terms.instalment_years = 0;
  }

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
  const bool addOns = beginPanel("##offer_add_ons", LOC("TRANSFER_ADD_ONS"));
  static TargetLabels appearance_labels;
  static TargetLabels goal_labels;
  const auto bonusRow = [&](const char* label, const char* id,
                            std::uint32_t& bonus, std::uint16_t& target,
                            std::span<const std::uint16_t> options,
                            const char* target_key, TargetLabels& cache)
  {
    formLabel(label);
    std::int64_t amount = bonus;
    if (UI::moneyInput(id, amount,
                       {.maximum = std::numeric_limits<std::uint32_t>::max()}))
    {
      bonus = static_cast<std::uint32_t>(amount);
      changed = true;
    }
    if (bonus == 0)
    {
      target = 0;
      return;
    }
    int targetIndex = optionIndex(options, target);
    if (target == 0) target = options[static_cast<std::size_t>(targetIndex)];
    ImGui::PushID(id);
    if (UI::segmented("##target", targetIndex,
                      targetLabels(cache, target_key, options)))
    {
      target = options[static_cast<std::size_t>(targetIndex)];
      changed = true;
    }
    ImGui::PopID();
  };
  bonusRow(LOC("TRANSFER_FIELD_APPEARANCE_BONUS"), "##app_bonus",
           terms.appearance_bonus, terms.appearance_target, APPEARANCE_TARGETS,
           "TRANSFER_APPS_SHORT", appearance_labels);
  bonusRow(LOC("TRANSFER_FIELD_GOAL_BONUS"), "##goal_bonus", terms.goal_bonus,
           terms.goal_target, GOAL_TARGETS, "TRANSFER_GOALS_SHORT",
           goal_labels);
  formLabel(LOC("TRANSFER_FIELD_SELL_ON"));
  int sellOnIndex = optionIndex(SELL_ON_OPTIONS, terms.sell_on_percent);
  if (UI::segmented("##sell_on", sellOnIndex, SELL_ON_LABELS))
  {
    terms.sell_on_percent = SELL_ON_OPTIONS[static_cast<std::size_t>(sellOnIndex)];
    changed = true;
  }
  if (addOns) endPanel();
  return changed;
}

std::string structureText(const TransferNegotiation::OfferTerms& terms)
{
  if (terms.instalment_years == 0) return LOC("TRANSFER_TERMS_CASH");
  return fmt::sprintf(LOC("TRANSFER_TERMS_INSTALMENTS"),
                      static_cast<int>(terms.upfront_percent),
                      static_cast<int>(terms.instalment_years));
}

}  // namespace TransferTermsEditor
