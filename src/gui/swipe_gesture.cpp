// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/swipe_gesture.h"

#include <algorithm>
#include <cmath>

bool SwipeGesture::expired(std::uint64_t now_ns) const
{
  return phase != Phase::IDLE && now_ns > last_ns &&
         now_ns - last_ns > tuning.idle_ns;
}

SwipeGesture::Step SwipeGesture::feed(float dx, float dy,
                                      std::uint64_t timestamp_ns)
{
  if (expired(timestamp_ns)) reset();
  if (phase == Phase::IDLE) phase = Phase::UNDECIDED;
  last_ns = std::max(last_ns, timestamp_ns);
  if (phase == Phase::IGNORED || phase == Phase::DONE) return Step::NONE;

  travel_x += dx;
  travel_y += dy;
  const float sideways = std::fabs(travel_x);
  const float vertical = std::fabs(travel_y);
  if (phase == Phase::UNDECIDED)
  {
    if (std::max(sideways, vertical) < tuning.slop) return Step::NONE;
    phase = sideways >= vertical * tuning.dominance ? Phase::SIDEWAYS
                                                    : Phase::IGNORED;
    if (phase == Phase::IGNORED) return Step::NONE;
  }
  // A swipe that turns into a vertical scroll is a scroll after all.
  if (vertical * tuning.dominance > sideways)
  {
    phase = Phase::IGNORED;
    return Step::NONE;
  }
  if (sideways < tuning.threshold) return Step::NONE;
  phase = Phase::DONE;
  return travel_x > 0.0f ? Step::BACK : Step::FORWARD;
}

void SwipeGesture::suppress(std::uint64_t timestamp_ns)
{
  if (expired(timestamp_ns)) reset();
  phase = Phase::IGNORED;
  last_ns = std::max(last_ns, timestamp_ns);
}

void SwipeGesture::reset()
{
  phase = Phase::IDLE;
  travel_x = 0.0f;
  travel_y = 0.0f;
  last_ns = 0;
}

float SwipeGesture::progress(std::uint64_t now_ns) const
{
  if (expired(now_ns)) return 0.0f;
  switch (phase)
  {
    case Phase::SIDEWAYS:
      return std::clamp(travel_x / tuning.threshold, -1.0f, 1.0f);
    case Phase::DONE:
      return travel_x > 0.0f ? 1.0f : -1.0f;
    case Phase::IDLE:
    case Phase::UNDECIDED:
    case Phase::IGNORED:
      return 0.0f;
  }
  return 0.0f;
}
