// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/**
 * @brief Rebindable keyboard shortcuts.
 *
 * Every shortcut is an action with a stable id ("nav.home", "match.pause"),
 * a label key, a category and up to two key chords (primary and alternate).
 * Screens ask whether an action was pressed instead of testing keys, so the
 * player can rebind anything from Settings > Controls. Only bindings that
 * differ from the defaults are written to the settings file, by name (never
 * by ImGuiKey value), so an action registered later (or renamed) never
 * breaks the file.
 *
 * Two actions conflict when they share a chord and can be active at the
 * same time: actions of one context, or any action and a GLOBAL one. Space
 * can therefore be Continue in the management screens and Pause in a match.
 * PLAY actions are listened to instead of the MATCH ones while the player
 * controls the team, so they only clash with each other (and GLOBAL).
 */
namespace Input
{

/** @brief Where an action is listened to (decides what conflicts). */
enum class Context : std::uint8_t
{
  GLOBAL,     /*!< Everywhere. */
  MANAGEMENT, /*!< The career screens (sidebar, top bar). */
  MATCH,      /*!< The live match, watching as manager. */
  PLAY,       /*!< The live match, controlling the team on the pitch
                   (replaces the MATCH keys while active). */
};

/** @brief Group an action is listed under in Settings > Controls. */
enum class Category : std::uint8_t
{
  NAVIGATION, /*!< Screens, history, command palette. */
  CAREER,     /*!< Continue, save, help. */
  MATCH,      /*!< Pause, speed, panels, mute. */
  CAMERA,     /*!< Match view and camera presets. */
  SHOUTS,     /*!< Touchline shouts. */
  PLAY,       /*!< On-pitch controls of the play mode. */
  COUNT
};

/** @brief Language key of a category's heading. */
const char* categoryKey(Category category);

/** @brief Handle of a registered action (index, stable for the run). */
using ActionId = std::size_t;

/** @brief Number of chords an action can have (primary, alternate). */
inline constexpr std::size_t BINDING_SLOTS = 2;

/** @brief Description of an action, given once at registration. */
struct ActionDef
{
  std::string id;          /*!< Stable id, e.g. "match.pause". */
  std::string label_key;   /*!< Language key of the label. */
  Category category = Category::NAVIGATION;
  Context context = Context::MANAGEMENT;
  ImGuiKeyChord primary = ImGuiKey_None;   /*!< Default chord. */
  ImGuiKeyChord alternate = ImGuiKey_None; /*!< Default second chord. */
  bool rebindable = true; /*!< Fixed actions are listed but locked. */
};

/** @brief A registered action with its current chords. */
struct Action
{
  ActionDef def;
  std::array<ImGuiKeyChord, BINDING_SLOTS> chords{};
};

/** @brief Outcome of a rebind request. */
struct RebindResult
{
  bool applied = false;
  /** Action already using the chord (set when not applied). */
  std::optional<ActionId> conflict;
  std::size_t conflict_slot = 0;
};

/** @brief True when two contexts can be live at once. */
bool contextsOverlap(Context first, Context second);

/** @brief Stable name of a chord for the settings file ("Ctrl+K"). */
std::string chordToString(ImGuiKeyChord chord);

/** @brief Parses chordToString() output; nullopt when invalid. */
std::optional<ImGuiKeyChord> chordFromString(std::string_view text);

/** @brief Name shown to the player ("Ctrl+K", "Alt+Left", "-"). */
std::string chordLabel(ImGuiKeyChord chord);

/** @brief True for keys that can be bound (letters, digits, F-keys...). */
bool isBindableKey(ImGuiKey key);

/**
 * @brief The chord pressed this frame, for "press a key" capture: the first
 * bindable key pressed with the modifiers held. Modifier keys alone and the
 * mouse never count.
 */
std::optional<ImGuiKeyChord> capturePressedChord();

/**
 * @brief The chord of an SDL key-down event. Letters and digits match both
 * the character and the physical key, so the number row keeps working on
 * layouts where it types symbols (Shift+1 on AZERTY).
 */
std::array<ImGuiKeyChord, 2> chordsOfEvent(const SDL_KeyboardEvent& event);

/**
 * @brief Registry of every action. One instance per run (registry()).
 *
 * Registration is idempotent: registering an id again returns the existing
 * handle, so screens may register their own actions when constructed.
 * Bindings stored in the settings are applied at registration and on
 * reloadFromSettings().
 */
class ActionRegistry
{
 public:
  ActionRegistry();

  /** @brief Registers an action (or returns the existing one). */
  ActionId registerAction(const ActionDef& def);

  /** @brief Handle of an action by id. */
  [[nodiscard]] std::optional<ActionId> find(std::string_view id) const;

  [[nodiscard]] const Action& action(ActionId id) const;
  [[nodiscard]] std::size_t size() const { return actions.size(); }

  /** @brief Current chord in a slot (ImGuiKey_None when unbound). */
  [[nodiscard]] ImGuiKeyChord chord(ActionId id, std::size_t slot = 0) const;

  /** @brief Label of the primary chord, or of the alternate one. */
  [[nodiscard]] std::string label(ActionId id) const;

  /**
   * @brief Pressed this frame through ImGui's shortcut router (any slot).
   * Pass ImGuiInputFlags_Repeat for held keys. A chord is matched exactly:
   * "S" does not fire while Shift is held.
   */
  [[nodiscard]] bool pressed(ActionId id,
                             ImGuiInputFlags flags = ImGuiInputFlags_RouteGlobal) const;
  /** @brief pressed() by id; false for unknown ids. */
  [[nodiscard]] bool pressed(std::string_view id,
                             ImGuiInputFlags flags = ImGuiInputFlags_RouteGlobal) const;

  /** @brief True when an SDL key-down event is one of the action's chords. */
  [[nodiscard]] bool matches(ActionId id, const SDL_KeyboardEvent& event) const;
  [[nodiscard]] bool matches(std::string_view id,
                             const SDL_KeyboardEvent& event) const;

  /**
   * @brief Another action that would clash with @p chord in @p slot of
   * @p id (same chord, overlapping context).
   */
  [[nodiscard]] std::optional<std::pair<ActionId, std::size_t>> conflictFor(
      ActionId id, ImGuiKeyChord chord, std::size_t slot) const;

  /**
   * @brief Binds a chord (ImGuiKey_None clears the slot). A clash is
   * refused unless @p replace is true, which clears the other action's
   * slot. Fixed actions and unbindable keys are refused. The settings are
   * updated (not saved to disk).
   */
  RebindResult rebind(ActionId id, std::size_t slot, ImGuiKeyChord chord,
                      bool replace = false);

  /** @brief Restores one action's default chords. */
  void resetToDefault(ActionId id);
  /** @brief Restores every action's default chords. */
  void resetAll();
  /** @brief True when an action uses its default chords. */
  [[nodiscard]] bool isDefault(ActionId id) const;

  /** @brief Re-applies the bindings stored in the settings. */
  void reloadFromSettings();

  /** @brief Bumped by every binding change (for cached labels). */
  [[nodiscard]] std::uint32_t revision() const { return revision_; }

  /** @brief Calls @p visit for each action of a category, in order. */
  void forEach(Category category,
               const std::function<void(ActionId, const Action&)>& visit) const;

 private:
  struct StringHash
  {
    using is_transparent = void;
    std::size_t operator()(std::string_view text) const
    {
      return std::hash<std::string_view>{}(text);
    }
  };

  void applyStored(ActionId id);
  void store(ActionId id) const;

  std::vector<Action> actions;
  std::uint32_t revision_ = 0;
  std::unordered_map<std::string, ActionId, StringHash, std::equal_to<>> by_id;
};

/** @brief The registry of the running game, with the built-in actions. */
ActionRegistry& registry();

/** @brief Ids of the built-in actions. */
namespace Ids
{
inline constexpr std::string_view NAV_HOME = "nav.home";
inline constexpr std::string_view NAV_INBOX = "nav.inbox";
inline constexpr std::string_view NAV_SQUAD = "nav.squad";
inline constexpr std::string_view NAV_TRAINING = "nav.training";
inline constexpr std::string_view NAV_MATCHES = "nav.matches";
inline constexpr std::string_view NAV_RECRUITMENT = "nav.recruitment";
inline constexpr std::string_view NAV_CLUB = "nav.club";
inline constexpr std::string_view NAV_PALETTE = "nav.palette";
inline constexpr std::string_view NAV_BACK = "nav.back";
inline constexpr std::string_view NAV_FORWARD = "nav.forward";
inline constexpr std::string_view NAV_CLOSE = "nav.close";
inline constexpr std::string_view CAREER_CONTINUE = "career.continue";
inline constexpr std::string_view CAREER_SAVE = "career.save";
inline constexpr std::string_view CAREER_HELP = "career.help";
inline constexpr std::string_view MATCH_PAUSE = "match.pause";
inline constexpr std::string_view MATCH_FASTER = "match.faster";
inline constexpr std::string_view MATCH_SLOWER = "match.slower";
inline constexpr std::string_view MATCH_SUBSTITUTIONS = "match.substitutions";
inline constexpr std::string_view MATCH_TACTICS = "match.tactics";
inline constexpr std::string_view MATCH_MUTE = "match.mute";
inline constexpr std::string_view MATCH_PITCH_FOCUS = "match.pitch_focus";
inline constexpr std::string_view MATCH_BACK = "match.back";
inline constexpr std::string_view MATCH_FULLSCREEN = "match.fullscreen";
inline constexpr std::string_view CAMERA_VIEW_TOGGLE = "camera.view_toggle";
inline constexpr std::string_view CAMERA_BROADCAST = "camera.broadcast";
inline constexpr std::string_view CAMERA_TACTICAL = "camera.tactical";
inline constexpr std::string_view CAMERA_END = "camera.end";
inline constexpr std::string_view CAMERA_PLAYER = "camera.player";
inline constexpr std::string_view CAMERA_FREE = "camera.free";
inline constexpr std::string_view CAMERA_DIRECTOR = "camera.director";
inline constexpr std::string_view CAMERA_FOLLOW_BALL = "camera.follow_ball";
inline constexpr std::string_view CAMERA_RESET = "camera.reset";
inline constexpr std::string_view SCREENSHOT = "general.screenshot";
/** Shouts are "match.shout.0" .. "match.shout.10" (MatchChanges::SHOUTS). */
inline constexpr std::size_t SHOUT_COUNT = 11;
std::string shout(std::size_t index);
}  // namespace Ids

}  // namespace Input
