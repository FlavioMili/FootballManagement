// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "global/types.h"

class DatabaseConnection;
class GameData;
class Player;

/** @brief Why a mentoring change was refused. */
enum class MentoringError : std::uint8_t
{
  None = 0,
  UnknownPlayer,
  NotSameClub,   /*!< Mentor and mentee must play for the managed club. */
  MentorTooYoung,
  MenteeTooOld,
  SamePlayer,
  AlreadyMentor, /*!< The mentor already leads a group. */
  AlreadyMentee, /*!< The youngster is already in a group. */
  MentorIsMentee,
  GroupFull,
  UnknownGroup
};

/** Language key of @p error (e.g. "MENTORING_ERROR_GROUP_FULL"). */
const char* mentoringErrorKey(MentoringError error);

/** @brief One youngster in a mentoring group. */
struct MenteeState
{
  PlayerID player_id = 0;
  /** Trait points moved so far (signed) and the part not yet applied. */
  float professionalism_shift = 0.0f;
  float temperament_shift = 0.0f;
  float professionalism_pending = 0.0f;
  float temperament_pending = 0.0f;
};

/** @brief A senior player guiding up to three youngsters. */
struct MentoringGroup
{
  std::uint32_t id = 0;
  TeamID team_id = 0;
  PlayerID mentor_id = 0;
  std::vector<MenteeState> mentees;
};

/**
 * @namespace Mentoring
 * @brief Bounded, slow personality transfer from mentor to youngster.
 *
 * Personality changes little in young adulthood: meta-analyses of
 * longitudinal studies find mean-level shifts of a fraction of a standard
 * deviation per decade, larger in the late teens and early twenties, with
 * social roles (a first job, a mentor) among the drivers (Roberts, Walton &
 * Viechtbauer 2006). Each week a mentee's professionalism and temperament
 * move a small share of the gap towards the mentor's, scaled by the
 * youngster's receptiveness (age) and the mentor's standing (age and
 * experience), at most 0.2 points a week and 10 points in total. A
 * professional mentor also adds up to 4% to the youngster's development;
 * an unhappy mentor spreads his mood instead.
 */
namespace Mentoring
{
inline constexpr int MENTOR_MIN_AGE = 25;
inline constexpr int MENTEE_MAX_AGE = 23;
inline constexpr std::size_t MAX_MENTEES = 3;
/** Share of the trait gap closed per week at full receptiveness. [P] */
inline constexpr float WEEKLY_RATE = 0.006f;
inline constexpr float MAX_WEEKLY_STEP = 0.2f;
inline constexpr float MAX_TOTAL_SHIFT = 10.0f;
inline constexpr float MAX_DEVELOPMENT_BONUS = 0.04f;
/** Below this morale the mentor's gloom rubs off (0.5 morale a week). */
inline constexpr float UNHAPPY_MENTOR_MORALE = 35.0f;
inline constexpr float GLOOM_PER_WEEK = 0.5f;

/** 1.0 up to 18, falling to 0.4 at 23. */
float receptiveness(int age);

/** 0.6 at 25, rising to 1.0 from 33. */
float influence(int mentor_age);

/**
 * Weekly trait move towards @p mentor_value, bounded so that the total
 * shift never leaves [-MAX_TOTAL_SHIFT, MAX_TOTAL_SHIFT].
 */
float weeklyShift(float mentor_value, float mentee_value, float receptiveness,
                  float influence, float already_shifted);

/** Development multiplier from the mentor's professionalism (1.0-1.04). */
float developmentMultiplier(std::uint8_t mentor_professionalism);
}  // namespace Mentoring

/**
 * @class MentoringSystem
 * @brief Mentoring groups of the managed club.
 *
 * Groups lapse on their own when a member leaves the club or a youngster
 * outgrows the scheme.
 */
class MentoringSystem
{
 public:
  /** Creates a group led by @p mentor_id; returns its id through @p id. */
  MentoringError createGroup(const GameData& gamedata, TeamID team_id,
                             PlayerID mentor_id, std::uint32_t* id = nullptr);
  MentoringError addMentee(const GameData& gamedata, std::uint32_t group_id,
                           PlayerID mentee_id);
  MentoringError removeMentee(std::uint32_t group_id, PlayerID mentee_id);
  MentoringError dissolve(std::uint32_t group_id);

  /** Weekly nudges, morale and pruning of lapsed memberships. */
  void onWeek(GameData& gamedata);

  /** Development multiplier of a player (1.0 when not mentored). */
  float developmentMultiplier(const GameData& gamedata,
                              PlayerID player_id) const;

  const std::vector<MentoringGroup>& groups() const { return all; }
  std::vector<MentoringGroup> groupsFor(TeamID team_id) const;
  /** Group containing a player as mentor or mentee (nullptr if none). */
  const MentoringGroup* groupOf(PlayerID player_id) const;

  void clear();
  void load(const std::shared_ptr<DatabaseConnection>& db_conn);
  void save(const std::shared_ptr<DatabaseConnection>& db_conn) const;

 private:
  MentoringGroup* find(std::uint32_t group_id);
  void prune(const GameData& gamedata);

  std::vector<MentoringGroup> all;
  std::uint32_t next_id = 1;
};
