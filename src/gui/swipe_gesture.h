// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>

/**
 * @brief Thresholds of the sideways swipe, in mouse-wheel units.
 *
 * SDL reports touchpad scrolling in logical units that do not depend on the
 * output scale (on Wayland one unit is about ten surface pixels of finger
 * travel), so the same numbers hold on HiDPI screens.
 */
struct SwipeTuning
{
  /** Sideways travel for one step. */
  float threshold = 10.0f;
  /** Travel after which the gesture's direction is decided. */
  float slop = 1.5f;
  /** Sideways travel must exceed vertical travel by this factor. */
  float dominance = 2.0f;
  /** A pause this long between samples ends the gesture. */
  std::uint64_t idle_ns = 200'000'000;
};

/**
 * @brief Recognises a two-finger sideways swipe from wheel samples.
 *
 * Fed with each wheel sample (dx > 0: the fingers moved right), it reports
 * one step per gesture once the sideways travel passes the threshold, and
 * then ignores the rest of that gesture. A gesture that starts vertically is
 * a page scroll and is ignored until it ends; so is one that drifts into a
 * vertical scroll. A pause longer than the idle gap starts a new gesture.
 */
class SwipeGesture
{
 public:
  enum class Step : uint8_t
  {
    NONE,
    BACK,   /*!< Fingers moved right: the previous screen. */
    FORWARD /*!< Fingers moved left: the next screen. */
  };

  SwipeGesture() = default;
  explicit SwipeGesture(const SwipeTuning& settings) : tuning(settings) {}

  /** @brief Adds a wheel sample; returns the step it completes, if any. */
  Step feed(float dx, float dy, std::uint64_t timestamp_ns);

  /**
   * @brief Ignores the rest of the current gesture, e.g. while it runs over
   * a list that scrolls sideways or while a dialog is open.
   */
  void suppress(std::uint64_t timestamp_ns);

  /** @brief Forgets the gesture in progress. */
  void reset();

  /**
   * @brief How far the gesture has gone at @p now_ns: 0 when none, up to 1
   * towards Back and down to -1 towards Forward (±1 once it stepped).
   */
  [[nodiscard]] float progress(std::uint64_t now_ns) const;

  [[nodiscard]] const SwipeTuning& getTuning() const { return tuning; }

 private:
  enum class Phase : uint8_t
  {
    IDLE,
    UNDECIDED,
    SIDEWAYS,
    IGNORED,
    DONE
  };

  [[nodiscard]] bool expired(std::uint64_t now_ns) const;

  SwipeTuning tuning;
  Phase phase = Phase::IDLE;
  float travel_x = 0.0f;
  float travel_y = 0.0f;
  std::uint64_t last_ns = 0;
};
