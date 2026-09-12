// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "settings_scene.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <filesystem>

#include "controller/game_controller.h"
#include "global/language_manager.h"
#include "global/paths.h"
#include "global/runtime_paths.h"
#include "gui/gui_constants.h"
#include "gui/scenes/main_menu_scene.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "settings_manager.h"

SceneID SettingsScene::getID() const { return SceneID::SETTINGS; }

SettingsScene::SettingsScene(GUIView* guiView_ptr) : GUIScene(guiView_ptr) {}

SettingsScene::SettingsScene(GUIView* guiView_ptr, bool inCareer)
    : GUIScene(guiView_ptr), in_career(inCareer)
{
}

namespace
{
/** A language's own name ("Italiano"); other languages keep the file name. */
std::string nativeLanguageName(Language language, const std::string& fileName)
{
  switch (language)
  {
    case Language::EN:
      return "English";
    case Language::IT:
      return "Italiano";
    default:
      return fileName;
  }
}
}  // namespace

void SettingsScene::onEnter()
{
  languageOptions.clear();
  availableLanguageEnums.clear();
  resolutionOptions.clear();
  fpsOptionsStrings.clear();
  for (const auto& [lang, str] : languageToString)
  {
    std::string filePath = AssetPaths::language(str);
    if (std::filesystem::exists(filePath))
    {
      languageOptions.push_back(nativeLanguageName(lang, str));
      availableLanguageEnums.push_back(lang);
    }
  }
  if (languageOptions.empty())
  {
    languageOptions.push_back("English");
    availableLanguageEnums.push_back(Language::EN);
  }
  for (const auto& res : GUIConstants::RESOLUTIONS)
  {
    resolutionOptions.push_back(std::to_string(res.width) + "x" +
                                std::to_string(res.height));
  }
  for (const auto& fps : GUIConstants::FPS_OPTIONS)
  {
    fpsOptionsStrings.push_back(std::to_string(fps));
  }

  SettingsManager* settingsManager = SettingsManager::instance();
  const auto& settings = settingsManager->getSettings();

  auto itLang = std::find(availableLanguageEnums.begin(),
                          availableLanguageEnums.end(), settings.language);
  selectedLanguage = (itLang != availableLanguageEnums.end())
                         ? static_cast<int>(std::distance(
                               availableLanguageEnums.begin(), itLang))
                         : 0;

  auto itFps = std::find(GUIConstants::FPS_OPTIONS.begin(),
                         GUIConstants::FPS_OPTIONS.end(), settings.fps_limit);
  selectedFPS = (itFps != GUIConstants::FPS_OPTIONS.end())
                    ? static_cast<int>(std::distance(
                          GUIConstants::FPS_OPTIONS.begin(), itFps))
                    : 0;

  auto itRes = std::find_if(GUIConstants::RESOLUTIONS.begin(),
                            GUIConstants::RESOLUTIONS.end(),
                            [&](const auto& r)
                            {
                              return r.width == settings.resolution_width &&
                                     r.height == settings.resolution_height;
                            });
  selectedResolution = (itRes != GUIConstants::RESOLUTIONS.end())
                           ? static_cast<int>(std::distance(
                                 GUIConstants::RESOLUTIONS.begin(), itRes))
                           : 0;

  fullscreen = settings.fullscreen;
  original_settings = settings;
  pending_ui_scale =
      settings.ui_scale > 0.0f ? settings.ui_scale : Theme::scale();
}

void SettingsScene::update(float deltaTime)
{
  if (showWipeDataOverlay && wipeDataTimer > 0.0f)
  {
    wipeDataTimer -= deltaTime;
  }
}

namespace
{
constexpr float CONTENT_MAX_WIDTH = 860.0f;
constexpr float LABEL_WIDTH = 210.0f;
constexpr float SWATCH_HEIGHT = 64.0f;
constexpr float MIN_SCALE_PERCENT = 75.0f;
constexpr float MAX_SCALE_PERCENT = 200.0f;

void settingLabel(const char* label, const char* help)
{
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  if (help != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    ImGui::SetTooltip("%s", help);
  ImGui::SameLine(LABEL_WIDTH * Theme::scale());
  ImGui::SetNextItemWidth(-FLT_MIN);
}

template <typename Options>
void optionCombo(const char* id, const Options& options, int& selected)
{
  if (ImGui::BeginCombo(id, options[static_cast<size_t>(selected)].c_str()))
  {
    for (size_t index = 0; index < options.size(); ++index)
    {
      const bool isSelected = static_cast<size_t>(selected) == index;
      if (ImGui::Selectable(options[index].c_str(), isSelected))
        selected = static_cast<int>(index);
      if (isSelected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
}

bool presetCard(Theme::Preset preset, bool selected, float width)
{
  const Theme::Swatch swatch = Theme::presetSwatch(preset);
  const Theme::Palette& palette = Theme::palette();
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const ImVec2 size(width, SWATCH_HEIGHT * Theme::scale());
  ImGui::PushID(static_cast<int>(preset));
  const bool pressed = ImGui::InvisibleButton("##preset", size);
  const bool hovered = ImGui::IsItemHovered();
  ImGui::PopID();
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const ImVec2 end(start.x + size.x, start.y + size.y);
  const float rounding = 6.0f * Theme::scale();
  drawList->AddRectFilled(start, end, Theme::toU32(swatch.background),
                          rounding);
  drawList->AddRectFilled(
      ImVec2(start.x + 8.0f * Theme::scale(), start.y + 8.0f * Theme::scale()),
      ImVec2(start.x + size.x * 0.45f, end.y - 8.0f * Theme::scale()),
      Theme::toU32(swatch.surface), 4.0f * Theme::scale());
  drawList->AddRectFilled(
      ImVec2(start.x + size.x * 0.52f, start.y + 14.0f * Theme::scale()),
      ImVec2(end.x - 10.0f * Theme::scale(), start.y + 20.0f * Theme::scale()),
      Theme::toU32(swatch.text), 2.0f * Theme::scale());
  drawList->AddRectFilled(
      ImVec2(start.x + size.x * 0.52f, start.y + 26.0f * Theme::scale()),
      ImVec2(end.x - 24.0f * Theme::scale(), start.y + 31.0f * Theme::scale()),
      Theme::toU32(swatch.muted), 2.0f * Theme::scale());
  drawList->AddRectFilled(
      ImVec2(start.x + size.x * 0.52f, end.y - 20.0f * Theme::scale()),
      ImVec2(start.x + size.x * 0.78f, end.y - 11.0f * Theme::scale()),
      Theme::toU32(palette.accent), 2.0f * Theme::scale());
  drawList->AddRect(start, end,
                    Theme::toU32(selected  ? palette.accent
                                 : hovered ? palette.muted
                                           : palette.border),
                    rounding, 0, selected ? 2.5f * Theme::scale() : 1.0f);
  const char* name = LOC(Theme::presetKey(preset));
  ImGui::PushFont(nullptr, Theme::textSize(Theme::Text::SMALL));
  const float nameWidth = ImGui::CalcTextSize(name).x;
  ImGui::SetCursorScreenPos(
      ImVec2(start.x + std::max(0.0f, (width - nameWidth) * 0.5f),
             end.y + 3.0f * Theme::scale()));
  ImGui::TextColored(selected ? palette.text : palette.muted, "%s", name);
  ImGui::PopFont();
  return pressed;
}
}  // namespace

void SettingsScene::render()
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(Theme::Space::XL * Theme::scale(),
                             Theme::Space::XL * Theme::scale()));
  ImGui::Begin("##settings", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar(3);
  // Escape backs out like Cancel unless a dialog or a combo takes it first.
  const bool leaveWithEscape =
      !ImGui::IsPopupOpen(
          "", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) &&
      !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

  const float width = std::min(ImGui::GetContentRegionAvail().x,
                               CONTENT_MAX_WIDTH * Theme::scale());
  const float footerHeight =
      ImGui::GetFrameHeightWithSpacing() + Theme::Space::L * Theme::scale();
  ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                (ImGui::GetWindowWidth() - width) * 0.5f));
  ImGui::BeginGroup();
  UI::pageHeader(LOC("SETTINGS_TITLE"), LOC("SETTINGS_SUBTITLE"));
  ImGui::BeginChild(
      "##settings_body",
      ImVec2(width, ImGui::GetContentRegionAvail().y - footerHeight));
  renderGeneral();
  renderAppearance();
  renderAudio();
  renderSaving();
  // Wiping is offered only with no career in memory: a career left for the
  // main menu is still loaded and would be saved back on exit.
  if (!in_career && !guiView->getController().isGameLoaded()) renderData();
  ImGui::EndChild();

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
  const ImVec2 buttonSize(160.0f * Theme::scale(), 0.0f);
  const bool cancelled =
      UI::secondaryButton(LOC("SETTINGS_CANCEL"), buttonSize);
  ImGui::SameLine();
  const bool applied = UI::primaryButton(LOC("SETTINGS_APPLY"), buttonSize);
  ImGui::EndGroup();
  if (cancelled || leaveWithEscape)
    cancel();
  else if (applied)
    applyAndSaveSettings();

  ImGui::End();
}

void SettingsScene::renderGeneral()
{
  UI::beginAutoHeightCard("settings_general", LOC("SETTINGS_SECTION_GENERAL"));
  settingLabel(LOC("SETTINGS_LANGUAGE"), nullptr);
  optionCombo("##language", languageOptions, selectedLanguage);
  settingLabel(LOC("SETTINGS_RESOLUTION"), nullptr);
  optionCombo("##resolution", resolutionOptions, selectedResolution);
  settingLabel(LOC("SETTINGS_REFRESH_RATE"), nullptr);
  optionCombo("##fps", fpsOptionsStrings, selectedFPS);
  settingLabel(LOC("SETTINGS_FULLSCREEN"), nullptr);
  ImGui::Checkbox("##fullscreen", &fullscreen);
  UI::endCard();
}

void SettingsScene::renderAppearance()
{
  Settings& settings = SettingsManager::instance()->get();
  UI::beginAutoHeightCard("settings_appearance",
                          LOC("SETTINGS_SECTION_APPEARANCE"));

  // Theme presets as live swatches.
  ImGui::TextUnformatted(LOC("SETTINGS_THEME"));
  const auto presetCount = static_cast<int>(Theme::Preset::COUNT);
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float cardWidth = (ImGui::GetContentRegionAvail().x -
                           gap * static_cast<float>(presetCount - 1)) /
                          static_cast<float>(presetCount);
  const float rowY = ImGui::GetCursorPosY();
  for (int index = 0; index < presetCount; ++index)
  {
    ImGui::SetCursorPos(
        ImVec2(ImGui::GetStyle().WindowPadding.x +
                   static_cast<float>(index) * (cardWidth + gap),
               rowY));
    if (presetCard(static_cast<Theme::Preset>(index),
                   settings.theme_preset == index, cardWidth))
    {
      settings.theme_preset = index;
      previewAppearance();
    }
  }
  ImGui::SetCursorPosY(rowY + SWATCH_HEIGHT * Theme::scale() +
                       ImGui::GetTextLineHeightWithSpacing() +
                       Theme::Space::S * Theme::scale());

  settingLabel(LOC("SETTINGS_ACCENT"), LOC("SETTINGS_ACCENT_HELP"));
  if (ImGui::RadioButton(LOC("SETTINGS_ACCENT_CLUB"), settings.club_accent))
  {
    settings.club_accent = true;
    previewAppearance();
  }
  ImGui::SameLine();
  if (ImGui::RadioButton(LOC("SETTINGS_ACCENT_CUSTOM"), !settings.club_accent))
  {
    settings.club_accent = false;
    previewAppearance();
  }
  if (!settings.club_accent)
  {
    ImGui::SameLine();
    ImVec4 accent = Theme::unpackRgb(settings.accent_rgb);
    if (ImGui::ColorEdit3(
            "##accent", &accent.x,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
    {
      settings.accent_rgb = Theme::packRgb(accent);
      previewAppearance();
    }
  }

  settingLabel(LOC("SETTINGS_UI_SCALE"), LOC("SETTINGS_UI_SCALE_HELP"));
  bool automatic = settings.ui_scale <= 0.0f;
  if (ImGui::Checkbox(LOC("SETTINGS_UI_SCALE_AUTO"), &automatic))
  {
    settings.ui_scale = automatic ? 0.0f : pending_ui_scale;
    previewAppearance();
  }
  if (!automatic)
  {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    float percent = pending_ui_scale * 100.0f;
    if (ImGui::SliderFloat("##ui_scale", &percent, MIN_SCALE_PERCENT,
                           MAX_SCALE_PERCENT, "%.0f%%"))
      pending_ui_scale = std::round(percent / 5.0f) * 0.05f;
    // Rescaling moves the slider itself, so apply once the drag ends.
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
      settings.ui_scale = pending_ui_scale;
      previewAppearance();
    }
  }

  settingLabel(LOC("SETTINGS_DENSITY"), LOC("SETTINGS_DENSITY_HELP"));
  if (ImGui::RadioButton(LOC("SETTINGS_DENSITY_COMFORTABLE"),
                         !settings.compact_density))
  {
    settings.compact_density = false;
    previewAppearance();
  }
  ImGui::SameLine();
  if (ImGui::RadioButton(LOC("SETTINGS_DENSITY_COMPACT"),
                         settings.compact_density))
  {
    settings.compact_density = true;
    previewAppearance();
  }

  settingLabel(LOC("SETTINGS_REDUCED_MOTION"),
               LOC("SETTINGS_REDUCED_MOTION_HELP"));
  if (ImGui::Checkbox("##reduced_motion", &settings.reduced_motion))
    previewAppearance();

  settingLabel(LOC("SETTINGS_SCREEN_TIPS"), LOC("SETTINGS_SCREEN_TIPS_HELP"));
  // Turning tips back on explains every screen again.
  if (ImGui::Checkbox("##screen_tips", &settings.screen_tips) &&
      settings.screen_tips)
    settings.screen_tips_seen = 0;
  UI::endCard();
}

void SettingsScene::renderAudio()
{
  Settings& settings = SettingsManager::instance()->get();
  UI::beginAutoHeightCard("settings_audio", LOC("SETTINGS_SECTION_AUDIO"));
  // Volumes apply at once (a running match picks them up next frame);
  // Cancel restores the previous values.
  const auto volumeSlider = [](const char* id, float& volume)
  {
    float percent = volume * 100.0f;
    if (ImGui::SliderFloat(id, &percent, 0.0f, 100.0f, "%.0f%%"))
      volume = std::clamp(std::round(percent) / 100.0f, 0.0f, 1.0f);
  };
  settingLabel(LOC("SETTINGS_AUDIO_MASTER"), nullptr);
  volumeSlider("##master_volume", settings.master_volume);
  settingLabel(LOC("SETTINGS_AUDIO_CROWD"), nullptr);
  volumeSlider("##crowd_volume", settings.crowd_volume);
  settingLabel(LOC("SETTINGS_AUDIO_EFFECTS"),
               LOC("SETTINGS_AUDIO_EFFECTS_HELP"));
  volumeSlider("##effects_volume", settings.effects_volume);
  settingLabel(LOC("SETTINGS_AUDIO_MUTE"), LOC("SETTINGS_AUDIO_MUTE_HELP"));
  ImGui::Checkbox("##audio_muted", &settings.audio_muted);
  UI::endCard();
}

void SettingsScene::renderSaving()
{
  Settings& settings = SettingsManager::instance()->get();
  UI::beginAutoHeightCard("settings_saving", LOC("SETTINGS_SECTION_SAVING"));
  static constexpr std::array<const char*, 6> FREQUENCY_KEYS = {
      "AUTOSAVE_OFF",     "AUTOSAVE_DAILY",    "AUTOSAVE_WEEKLY",
      "AUTOSAVE_MONTHLY", "AUTOSAVE_MATCHDAY", "AUTOSAVE_SEASON"};
  settingLabel(LOC("SETTINGS_AUTOSAVE"), LOC("SETTINGS_AUTOSAVE_HELP"));
  const int frequency = std::clamp(settings.autosave_frequency, 0,
                                   static_cast<int>(FREQUENCY_KEYS.size()) - 1);
  ImGui::SetNextItemWidth(260.0f * Theme::scale());
  if (ImGui::BeginCombo("##autosave_frequency",
                        LOC(FREQUENCY_KEYS[static_cast<size_t>(frequency)])))
  {
    for (int index = 0; index < static_cast<int>(FREQUENCY_KEYS.size());
         ++index)
    {
      if (ImGui::Selectable(LOC(FREQUENCY_KEYS[static_cast<size_t>(index)]),
                            index == frequency))
        settings.autosave_frequency = index;
    }
    ImGui::EndCombo();
  }
  settingLabel(LOC("SETTINGS_AUTOSAVE_BACKUPS"),
               LOC("SETTINGS_AUTOSAVE_BACKUPS_HELP"));
  ImGui::SetNextItemWidth(260.0f * Theme::scale());
  ImGui::SliderInt("##autosave_backups", &settings.autosave_backups, 0, 9);
  UI::endCard();
}

void SettingsScene::renderData()
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginAutoHeightCard("settings_data", LOC("SETTINGS_SECTION_DATA"));
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(palette.muted, "%s", LOC("SETTINGS_WIPE_HELP"));
  ImGui::PopTextWrapPos();
  if (UI::dangerButton(LOC("SETTINGS_WIPE_BUTTON")))
  {
    showWipeDataOverlay = true;
    wipeDataTimer = 3.0f;
    ImGui::OpenPopup("###WipeDataPopup");
  }

  const std::string popupTitle =
      std::string(LOC("SETTINGS_WIPE_TITLE")) + "###WipeDataPopup";
  if (ImGui::BeginPopupModal(popupTitle.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
  {
    ImGui::PushTextWrapPos(420.0f * Theme::scale());
    ImGui::TextUnformatted(LOC("SETTINGS_WIPE_WARNING"));
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    const ImVec2 buttonSize(150.0f * Theme::scale(), 0.0f);
    if (UI::secondaryButton(LOC("SETTINGS_CANCEL"), buttonSize) ||
        ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
      showWipeDataOverlay = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine();
    if (wipeDataTimer > 0.0f)
    {
      ImGui::BeginDisabled();
      const std::string waiting =
          fmt::sprintf(LOC("SETTINGS_WIPE_CONFIRM_WAIT"),
                       static_cast<double>(wipeDataTimer));
      UI::dangerButton(waiting.c_str(), buttonSize);
      ImGui::EndDisabled();
    }
    else if (UI::dangerButton(LOC("SETTINGS_WIPE_CONFIRM"), buttonSize))
    {
      RuntimePaths::removeAllSaves();
      showWipeDataOverlay = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  UI::endCard();
}

void SettingsScene::previewAppearance() { guiView->refreshTheme(); }

void SettingsScene::cancel()
{
  SettingsManager::instance()->get() = original_settings;
  guiView->refreshTheme();
  leave();
}

void SettingsScene::leave()
{
  if (in_career)
    guiView->popScene();
  else
    changeScene(std::make_unique<MainMenuScene>(guiView));
}

void SettingsScene::applyAndSaveSettings()
{
  SettingsManager* settingsManager = SettingsManager::instance();
  auto& settings = settingsManager->getSettings();

  if (selectedLanguage >= 0 &&
      selectedLanguage < static_cast<int>(availableLanguageEnums.size()))
  {
    settings.language =
        availableLanguageEnums[static_cast<std::size_t>(selectedLanguage)];
  }

  if (selectedFPS >= 0 &&
      selectedFPS < static_cast<int>(GUIConstants::FPS_OPTIONS.size()))
  {
    settings.fps_limit =
        GUIConstants::FPS_OPTIONS[static_cast<std::size_t>(selectedFPS)];
  }

  if (selectedResolution >= 0 &&
      selectedResolution < static_cast<int>(GUIConstants::RESOLUTIONS.size()))
  {
    settings.resolution_width =
        GUIConstants::RESOLUTIONS[static_cast<std::size_t>(selectedResolution)]
            .width;
    settings.resolution_height =
        GUIConstants::RESOLUTIONS[static_cast<std::size_t>(selectedResolution)]
            .height;
  }
  settings.fullscreen = fullscreen;

  guiView->applyWindowSettings();
  settingsManager->save();
  guiView->refreshTheme();
  guiView->applySavePolicy();

  leave();
}
