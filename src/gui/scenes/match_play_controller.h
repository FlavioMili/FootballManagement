// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>

#include "model/match_engine.h"

/** How freely control moves between team-mates (Settings::play_auto_switch). */
enum class PlayAutoSwitch : std::uint8_t
{
  /** Only the ball carrier is taken automatically; the rest is manual. */
  OFF,
  /** Also the receiver of a pass, and the best placed defender once the
   * stick has been idle for a moment. */
  ASSISTED,
  /** Control always follows the best placed team-mate. */
  AUTO
};

/**
 * Screen pixels per pitch metre at the active footballer, along the pitch
 * length (+x) and width (+y), so a stick pushed "up the screen" runs up the
 * screen in any view or camera.
 */
struct PlayScreenBasis
{
  float lengthX = 1.0f;
  float lengthY = 0.0f;
  float widthX = 0.0f;
  float widthY = 1.0f;
};

/**
 * The human's side of play mode: keyboard and gamepad state, the action
 * buttons with their power bar, and who the active footballer is.
 *
 * Devices are read on every frame, but the engine only receives one sample
 * per simulation step (taken on the frame in which the engine is about to
 * step), as a tick-stamped MatchPlayerInput record; switching goes through
 * MatchEngine::setControlledPlayer. Power comes from how many simulation
 * steps a button was held, so a recorded match replays exactly and the frame
 * rate never changes what happens on the pitch.
 */
class MatchPlayController
{
 public:
  /** Assistance and device options (from the settings). */
  struct Options
  {
    PlayAutoSwitch autoSwitch = PlayAutoSwitch::AUTO;
    /** 0 none, 1 normal, 2 strong (MatchPlayerInput::passAssist). */
    std::uint8_t passAssist = 1;
    /** Stick dead zone in [0, 1). */
    float deadZone = 0.2f;
  };

  /** Seconds of simulated time to fill the power bar. */
  static constexpr float FULL_POWER_SECONDS = 1.0f;

  MatchPlayController() = default;
  ~MatchPlayController();
  MatchPlayController(const MatchPlayController&) = delete;
  MatchPlayController& operator=(const MatchPlayController&) = delete;

  /** Takes control of `homeTeam`'s best placed footballer. */
  void begin(MatchEngine& engine, bool homeTeam, const Options& options);
  /** Hands every player back to the AI. */
  void end(MatchEngine& engine);
  [[nodiscard]] bool isActive() const { return active; }
  [[nodiscard]] bool controlsHome() const { return home; }
  void setOptions(const Options& changed) { settings = changed; }
  [[nodiscard]] const Options& options() const { return settings; }

  /**
   * Keyboard and gamepad events (including hot-plug); true when the event
   * was one of the play controls (the caller should not use it for anything
   * else).
   */
  bool handleEvent(const SDL_Event& event);
  /** Forgets presses and a charging button (after a pause or a dialog). */
  void clearPresses();
  /** True once per press of the pause control (Esc, P, Start). */
  bool takePauseRequest();

  /**
   * Once per frame, before the engine advances by `simulatedSeconds`:
   * switches the active footballer when the rules say so and, if the engine
   * is about to step, submits the device state as that step's input.
   */
  void update(MatchEngine& engine, float simulatedSeconds,
              const PlayScreenBasis& basis);

  /** The footballer a manual switch would pick now (0 when none). */
  [[nodiscard]] PlayerID nextSwitch() const { return next_switch; }
  /** Filled share of the power bar while a button is held, else 0. */
  [[nodiscard]] float chargeShare(const MatchEngine& engine) const;
  [[nodiscard]] bool isCharging() const
  {
    return charging != MatchInputAction::NONE;
  }
  /** True while a gamepad is connected and in use. */
  [[nodiscard]] bool hasGamepad() const { return gamepad != nullptr; }
  /** Name of the connected gamepad (empty when none). */
  [[nodiscard]] const char* gamepadName() const;
  /** The last input sent to the engine (tests, presentation). */
  [[nodiscard]] const MatchPlayerInput& lastInput() const { return submitted; }

 private:
  /** A play control, from either device. */
  enum Control : std::uint8_t
  {
    PASS,
    SHOOT,
    THROUGH,
    LOB,
    SWITCH,
    SPRINT,
    JOCKEY,
    PAUSE,
    CONTROL_COUNT
  };

  /** Control a key or gamepad button stands for (CONTROL_COUNT: none). */
  static Control controlForKey(SDL_Scancode scancode);
  static Control controlForButton(int button);
  void press(Control control);
  void release(Control control);
  [[nodiscard]] bool held(Control control) const;
  void openGamepad(SDL_JoystickID which);
  void openFirstGamepad();
  void closeGamepad();
  /** Stick direction on screen (x right, y down), magnitude 0..1. */
  void readStick(float& x, float& y) const;
  /** Hands control to `player` now, keeping the device state. */
  void switchTo(MatchEngine& engine, PlayerID player);
  void applySwitchingRules(MatchEngine& engine, PlayerID current, bool idle);

  bool active = false;
  bool home = true;
  Options settings;

  std::array<bool, SDL_SCANCODE_COUNT> keys{};
  /** Held state of each control from buttons (keys or pad) and presses
   * and releases since the last update. */
  std::array<std::uint8_t, CONTROL_COUNT> keyHolds{};
  std::array<bool, CONTROL_COUNT> padHolds{};
  std::array<bool, CONTROL_COUNT> pressed{};
  std::array<bool, CONTROL_COUNT> released{};
  bool pause_requested = false;

  SDL_Gamepad* gamepad = nullptr;
  bool gamepad_subsystem = false;

  /** Button being held for power, and the step it was pressed at. */
  MatchInputAction charging = MatchInputAction::NONE;
  std::uint64_t charge_step = 0;
  /** Action (and its power) waiting for the next step's input. */
  MatchInputAction pending = MatchInputAction::NONE;
  float pending_power = 0.0f;

  MatchPlayerInput submitted;
  bool submitted_any = false;
  PlayerID next_switch = 0;
  /** Simulated seconds of the last switch, and of the last manual one with
   * the player it left (so repeated presses cycle). */
  double last_switch_seconds = -1e9;
  double last_manual_seconds = -1e9;
  PlayerID manual_skip = 0;
  /** Simulated seconds since which the stick has been idle. */
  double idle_since = 0.0;
  bool was_idle = true;
};
