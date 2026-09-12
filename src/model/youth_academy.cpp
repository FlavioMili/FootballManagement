// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/youth_academy.h"

#include <sqlite3.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <functional>
#include <map>
#include <ranges>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

#include "database/database_connection.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "model/club_economy.h"
#include "model/finances.h"
#include "model/inbox.h"
#include "model/role_utils.h"
#include "model/team.h"
#include "model/world_generation.h"
#include "model/world_rng.h"
#include "model/world_tuning.h"

namespace
{
// Stream keys. Intake candidates use (year, team << 8 | index); the class
// draw uses index 255. Names/attributes, matches and estimates add a salt to
// key_a so that they never collide with the candidate draws.
constexpr std::uint64_t CLASS_INDEX = 255;
constexpr std::uint64_t MATERIALIZE_SALT = 1ULL << 32;
constexpr std::uint64_t MATCH_SALT = 1ULL << 48;
constexpr std::uint64_t ESTIMATE_SALT = 2ULL << 48;
constexpr std::uint64_t SELECTION_SALT = 3ULL << 48;
constexpr std::uint64_t RECRUITMENT_SALT = 4ULL << 48;
constexpr std::uint64_t STYLE_SALT = 0x57A1E;

/** Mean intake potential relative to the club's first-team level. [P] */
constexpr float INTAKE_POTENTIAL_OFFSET = -20.0f;
/** Most monthly progress points kept per player (three seasons). */
constexpr std::size_t PROGRESS_POINTS = 36;
/** U18 matchdays run from early August to late May, one per week. */
constexpr std::uint8_t SEASON_START_MONTH = 8;
constexpr std::uint8_t SEASON_START_DAY = 8;
constexpr std::uint8_t SEASON_END_MONTH = 5;
constexpr std::uint8_t SEASON_END_DAY = 25;
constexpr std::int32_t MATCH_WEEKDAY = 5;
/** Days after a board decision before the next request is heard. */
constexpr std::int32_t REQUEST_COOLDOWN_DAYS = 90;
/** Board confidence needed to fund the academy. [P] */
constexpr float MIN_BOARD_CONFIDENCE = 35.0f;
/** An academy may be this much better than the club's reputation. [P] */
constexpr int STATURE_HEADROOM = 25;
/** Weeks of payroll kept in reserve before funding a project. */
constexpr std::int64_t PAYROLL_RESERVE_WEEKS = 12;
/** Promotions never take a computer-managed senior squad beyond the size
 * the season-end trim keeps. */
constexpr std::size_t FIRST_TEAM_LIMIT = WorldTuning::Youth::AI_SQUAD_TARGET;
/** Ability below the club's first-team level still good enough to move up
 * at 19 (a squad player), and early for a 17 or 18-year-old. [P] */
constexpr float READY_MARGIN = 10.0f;
constexpr float EARLY_READY_MARGIN = 5.0f;
/** Expected potential (below first-team level) worth a new contract. [P] */
constexpr float KEEP_MARGIN = 18.0f;
/** Expected potential (below first-team level) worth a first professional
 * contract: only the best prospects of an academy sign one before 19, the
 * rest stay on scholarships. [RR 4, FW 5] */
constexpr float PRO_PROSPECT_MARGIN = 8.0f;
/** Ability of the stand-ins who fill a short U18 team sheet. */
constexpr float STAND_IN_OVERALL = 26.0f;

constexpr std::array<PlayerRole, 12> ROLES = {
    PlayerRole::GK,  PlayerRole::CB, PlayerRole::LB,  PlayerRole::RB,
    PlayerRole::CDM, PlayerRole::CM, PlayerRole::CAM, PlayerRole::LM,
    PlayerRole::RM,  PlayerRole::LW, PlayerRole::RW,  PlayerRole::ST};
constexpr std::array<float, 12> ROLE_WEIGHTS = {
    1.0f, 3.0f, 1.0f, 1.0f, 1.0f, 2.0f, 1.0f, 0.5f, 0.5f, 1.0f, 1.0f, 2.0f};

/** Foreign youngsters most often come from these talent pools. [P] */
constexpr std::array<Language, 12> FOREIGN_YOUTH = {
    Language::FR, Language::BR, Language::PT, Language::ES,
    Language::NL, Language::BE, Language::HR, Language::DE,
    Language::EN, Language::IT, Language::DK, Language::PL};

std::uint64_t candidateKey(TeamID team_id, std::uint64_t index)
{
  return (static_cast<std::uint64_t>(team_id) << 8) | index;
}

float toUnit(std::uint8_t value) { return static_cast<float>(value) / 100.0f; }

std::uint8_t clampTrait(float value)
{
  return static_cast<std::uint8_t>(
      std::lround(std::clamp(value, 1.0f, 100.0f)));
}

std::int32_t intakeOrdinal(std::uint16_t year)
{
  return dayOrdinal(GameDateValue(year, YouthModel::INTAKE_MONTH,
                                  YouthModel::INTAKE_DAY));
}

std::uint16_t seasonYear(const GameDateValue& date)
{
  return date.month >= 7 ? date.year
                         : static_cast<std::uint16_t>(date.year - 1);
}

bool inYouthSeason(const GameDateValue& date)
{
  if (date.month == SEASON_START_MONTH) return date.day >= SEASON_START_DAY;
  if (date.month == SEASON_END_MONTH) return date.day <= SEASON_END_DAY;
  return date.month > SEASON_START_MONTH || date.month < SEASON_END_MONTH;
}

/** Share of a team's goals scored by a player in this role. [P] */
float scoringWeight(PlayerRole role)
{
  switch (role)
  {
    case PlayerRole::ST:
      return 5.0f;
    case PlayerRole::LW:
    case PlayerRole::RW:
      return 3.5f;
    case PlayerRole::CAM:
      return 3.0f;
    case PlayerRole::LM:
    case PlayerRole::RM:
      return 2.5f;
    case PlayerRole::CM:
      return 1.5f;
    case PlayerRole::CDM:
      return 0.8f;
    case PlayerRole::LB:
    case PlayerRole::RB:
      return 0.6f;
    case PlayerRole::CB:
      return 0.5f;
    default:
      return 0.0f;
  }
}

float heightMean(PlayerRole role)
{
  switch (role)
  {
    case PlayerRole::GK:
      return 187.0f;
    case PlayerRole::CB:
      return 185.0f;
    case PlayerRole::ST:
      return 181.0f;
    case PlayerRole::CDM:
      return 180.0f;
    case PlayerRole::LW:
    case PlayerRole::RW:
    case PlayerRole::LM:
    case PlayerRole::RM:
      return 175.0f;
    default:
      return 178.0f;
  }
}

bool leftSided(PlayerRole role)
{
  return role == PlayerRole::LB || role == PlayerRole::LM ||
         role == PlayerRole::LW;
}

/**
 * Attributes of a youngster with the role's strengths above his other
 * attributes; the whole set is shifted so the role overall hits @p target.
 */
std::map<std::string, float> youthStats(WorldRng& rng, PlayerRole role,
                                        float target,
                                        const StatsConfig& config)
{
  std::map<std::string, float> weights;
  double max_weight = 0.0;
  if (const auto focus =
          config.role_focus.find(RoleUtils::getBroadCategory(role));
      focus != config.role_focus.end())
  {
    const RoleFocus& role_focus = focus->second;
    for (std::size_t i = 0;
         i < std::min(role_focus.stats.size(), role_focus.weights.size()); ++i)
    {
      weights[role_focus.stats[i]] = static_cast<float>(role_focus.weights[i]);
      max_weight = std::max(max_weight, role_focus.weights[i]);
    }
  }
  std::map<std::string, float> stats;
  for (const std::string& name : config.possible_stats)
  {
    float offset = -5.0f;
    if (const auto weight = weights.find(name);
        weight != weights.end() && max_weight > 0.0)
      offset = -2.0f + 10.0f * weight->second / static_cast<float>(max_weight);
    stats[name] = target + offset + rng.normal(0.0f, 4.5f);
  }
  for (int pass = 0; pass < 3; ++pass)
  {
    const auto delta = static_cast<float>(
        static_cast<double>(target) -
        WorldGeneration::overallFor(role, stats, config));
    for (auto& [name, value] : stats)
      value = std::clamp(value + delta, static_cast<float>(MIN_STAT_VAL),
                         static_cast<float>(MAX_STAT_VAL) - 1.0f);
  }
  return stats;
}

std::uint32_t roundWage(double wage, double minimum)
{
  return static_cast<std::uint32_t>(
      std::lround(std::max(minimum, wage) / 50.0) * 50);
}

void post(Inbox& inbox, const GameDateValue& date, InboxCategory category,
          const char* title_key, const char* body_key,
          std::vector<std::string> args,
          std::optional<PlayerID> player_id = std::nullopt)
{
  InboxMessage message;
  message.date = date;
  message.category = category;
  message.title_key = title_key;
  message.body_key = body_key;
  message.args = std::move(args);
  message.player_id = player_id;
  inbox.add(std::move(message));
}

std::string joinNames(const std::vector<std::string>& names)
{
  std::string text;
  for (const std::string& name : names)
  {
    if (!text.empty()) text += ", ";
    text += name;
  }
  return text;
}

// ---------------------------------------------------------------- SQLite

template <typename T>
T columnAs(sqlite3_stmt* stmt, int column)
{
  return static_cast<T>(sqlite3_column_int64(stmt, column));
}

template <typename Read>
void forEachRow(const DatabaseConnection& db, const char* sql, Read read)
{
  sqlite3_stmt* stmt = db.prepareStatement(sql);
  while (sqlite3_step(stmt) == SQLITE_ROW) read(stmt);
  sqlite3_finalize(stmt);
}

template <typename Range, typename Bind>
void insertAll(const DatabaseConnection& db, const char* sql,
               const Range& items, Bind bind)
{
  sqlite3_stmt* stmt = db.prepareStatement(sql);
  for (const auto& item : items)
  {
    bind(stmt, item);
    db.executeStep(stmt);
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
  }
  sqlite3_finalize(stmt);
}

std::string encodeProgress(const std::vector<YouthProgressPoint>& points)
{
  std::string text;
  for (const YouthProgressPoint& point : points)
  {
    text += std::to_string(point.day);
    text += ':';
    text += std::to_string(std::lround(point.overall * 10.0f));
    text += ';';
  }
  return text;
}

std::vector<YouthProgressPoint> decodeProgress(std::string_view text)
{
  std::vector<YouthProgressPoint> points;
  while (!text.empty())
  {
    const std::size_t end = text.find(';');
    const std::string_view item = text.substr(0, end);
    const std::size_t colon = item.find(':');
    std::int32_t day = 0;
    int tenths = 0;
    if (colon != std::string_view::npos &&
        std::from_chars(item.data(), item.data() + colon, day).ec ==
            std::errc{} &&
        std::from_chars(item.data() + colon + 1, item.data() + item.size(),
                        tenths)
                .ec == std::errc{})
      points.push_back({day, static_cast<float>(tenths) / 10.0f});
    if (end == std::string_view::npos) break;
    text.remove_prefix(end + 1);
  }
  if (points.size() > PROGRESS_POINTS)
    points.erase(points.begin(),
                 points.end() - static_cast<std::ptrdiff_t>(PROGRESS_POINTS));
  return points;
}

/** Default reach of a club's youth recruitment network. [P] */
std::uint8_t defaultRecruitment(std::uint64_t seed, const Team& team)
{
  const double noise =
      WorldRng::hashUniform(seed, RngDomain::YouthIntake, RECRUITMENT_SALT,
                            team.getId()) -
      0.5;
  return static_cast<std::uint8_t>(std::clamp(
      std::lround(0.75 * team.getReputation() + 12.0 + 16.0 * noise), 10L,
      95L));
}
}  // namespace

// ---------------------------------------------------------------------------
// YouthModel
// ---------------------------------------------------------------------------

namespace YouthModel
{
IntakeClass planIntake(const IntakeInputs& inputs, std::uint64_t world_seed,
                       std::uint16_t year, TeamID team_id)
{
  using Youth = WorldTuning::Youth;
  IntakeClass intake;
  WorldRng group = WorldRng::stream(world_seed, RngDomain::YouthIntake, year,
                                    candidateKey(team_id, CLASS_INDEX));
  // Most groups are ordinary; a golden generation turns up about once in
  // twenty years, a little more often with a wide recruitment network. [P]
  intake.class_offset = group.normal(0.0f, 2.2f);
  if (group.chance(0.035 + 0.03 * static_cast<double>(inputs.recruitment)))
    intake.class_offset += group.uniform(4.5f, 8.0f);
  if (intake.class_offset >= 4.5f)
    intake.quality = IntakeQuality::Golden;
  else if (intake.class_offset >= 1.5f)
    intake.quality = IntakeQuality::Promising;
  else if (intake.class_offset < -2.0f)
    intake.quality = IntakeQuality::Weak;

  // Top academies take in 8-15 youngsters a year, small ones a handful.
  // [RR 4.3]
  const int size = std::clamp(
      static_cast<int>(std::lround(
          3.0f + 5.0f * inputs.recruitment + 4.0f * inputs.facilities +
          1.5f * inputs.head_ability + group.normal(0.0f, 1.2f))),
      3, 15);

  const float level = WorldGeneration::teamLevel(inputs.reputation);
  // Even the best academies produce one or two first-team players a year
  // [RR 4]: the group sits well below the club's level, with a tail.
  const float mean =
      level + INTAKE_POTENTIAL_OFFSET +
      4.0f * (2.0f * inputs.facilities - 1.0f) +
      2.0f * (2.0f * inputs.recruitment - 1.0f) + inputs.staff_bonus +
      regionTalent(inputs.country) + intake.class_offset;
  // A professional head of youth development attracts and shapes
  // professional youngsters.
  const float professionalism_mean =
      55.0f + 16.0f * (inputs.head_professionalism - 0.5f) *
                  (inputs.head_ability > 0.0f ? 1.0f : 0.0f);
  const double wonderkid_chance = static_cast<double>(
      0.004f * (0.5f + inputs.facilities + inputs.recruitment) *
      (1.0f + std::max(0.0f, regionTalent(inputs.country)) / 3.0f));

  intake.candidates.reserve(static_cast<std::size_t>(size));
  for (int index = 0; index < size; ++index)
  {
    WorldRng rng =
        WorldRng::stream(world_seed, RngDomain::YouthIntake, year,
                         candidateKey(team_id, static_cast<std::uint64_t>(index)));
    CandidateProfile candidate;
    candidate.role = ROLES[rng.weightedIndex(ROLE_WEIGHTS)];
    candidate.age = static_cast<std::uint8_t>(15 + rng.weightedIndex(
                                                       std::array<float, 3>{
                                                           0.3f, 0.4f, 0.3f}));
    candidate.potential =
        std::clamp(rng.normal(mean, Youth::POTENTIAL_STDDEV), 30.0f, 95.0f);
    if (rng.chance(wonderkid_chance))
    {
      candidate.wonderkid = true;
      candidate.potential = std::max(
          candidate.potential,
          std::min(95.0f, std::max(level + 10.0f, 78.0f) +
                              std::abs(rng.normal(0.0f, 4.0f))));
    }
    candidate.current = std::max(
        15.0f, candidate.potential * rng.uniform(0.50f, 0.62f) -
                   1.5f * static_cast<float>(17 - candidate.age));
    candidate.traits.professionalism =
        clampTrait(rng.normal(professionalism_mean, 16.0f));
    candidate.traits.ambition = clampTrait(rng.normal(50.0f, 18.0f));
    candidate.traits.temperament = clampTrait(rng.normal(55.0f, 15.0f));
    candidate.traits.loyalty = clampTrait(rng.normal(50.0f, 18.0f));
    candidate.traits.injury_proneness = static_cast<std::uint8_t>(
        std::lround(std::clamp(rng.lognormal(40.0f, 0.4f), 5.0f, 100.0f)));
    // Academies recruit mostly from their own region. [P]
    candidate.local = rng.chance(0.9);
    intake.candidates.push_back(candidate);
  }
  return intake;
}

IntakePreview summarize(const IntakeClass& intake)
{
  IntakePreview preview;
  preview.quality = intake.quality;
  preview.size = intake.candidates.size();
  std::vector<const CandidateProfile*> ranked;
  for (const CandidateProfile& candidate : intake.candidates)
    ranked.push_back(&candidate);
  std::ranges::sort(ranked, [](const CandidateProfile* a,
                               const CandidateProfile* b)
                    { return a->potential > b->potential; });
  if (ranked.empty()) return preview;
  preview.personality_key = personalityKey(ranked.front()->traits);
  preview.standout[0] = ranked.front()->role;
  for (const CandidateProfile* candidate : ranked)
  {
    if (candidate->role != preview.standout[0])
    {
      preview.standout[1] = candidate->role;
      break;
    }
  }
  return preview;
}

float regionTalent(Language country)
{
  // Designer estimates of how deep each country's youth pool is.
  switch (country)
  {
    case Language::BR:
      return 3.0f;
    case Language::FR:
      return 2.5f;
    case Language::ES:
    case Language::PT:
    case Language::NL:
      return 2.0f;
    case Language::EN:
    case Language::DE:
    case Language::BE:
    case Language::HR:
      return 1.5f;
    case Language::IT:
      return 1.0f;
    case Language::US:
    case Language::CN:
    case Language::JP:
      return -1.0f;
    default:
      return 0.0f;
  }
}

int firstProfessionalAge(Language country)
{
  // England signs first professional contracts at 17, most countries at 16.
  return country == Language::EN ? 17 : 16;
}

float academyGrade(const IntakeInputs& inputs)
{
  return std::clamp(0.35f * inputs.facilities + 0.35f * inputs.recruitment +
                        0.15f * std::clamp(inputs.junior_coaching, 0.0f, 1.0f) +
                        0.15f * inputs.head_ability,
                    0.0f, 1.0f);
}

std::size_t u18SquadLimit(float grade)
{
  // Category-one academies carry two full scholar years, small ones a
  // squad of about eighteen. [FW 5]
  return static_cast<std::size_t>(
      16 + std::lround(10.0f * std::clamp(grade, 0.0f, 1.0f)));
}

int computerSignings(float grade, std::size_t candidates)
{
  // The best academies keep most of a large intake, small ones only the
  // few they can coach. [RR 4.3]
  const float share = 0.45f + 0.5f * std::clamp(grade, 0.0f, 1.0f);
  return std::clamp(static_cast<int>(std::lround(
                        share * static_cast<float>(candidates))),
                    std::min<int>(SIGN_MIN, static_cast<int>(candidates)),
                    SIGN_MAX);
}

std::uint32_t scholarshipWage(LeagueID league_id)
{
  const double revenue_millions =
      static_cast<double>(leagueProfile(league_id).average_revenue_eur) / 1e6;
  return static_cast<std::uint32_t>(
      std::lround(std::clamp(60.0 + revenue_millions, 80.0, 450.0) / 10.0) *
      10);
}

std::uint8_t youthContractYears(int age)
{
  return static_cast<std::uint8_t>(std::clamp(19 - age, 2, 3));
}

bool isHomegrown(int joined_age, int age)
{
  return std::min(age, 21) - std::max(joined_age, 15) >= HOMEGROWN_SEASONS;
}

float staffProfessionalism(std::uint64_t world_seed, StaffID staff_id)
{
  return static_cast<float>(WorldRng::hashUniform(
      world_seed, RngDomain::Staff, staff_id, STYLE_SALT));
}

const char* staffStyleKey(float professionalism)
{
  if (professionalism >= 0.7f) return "YOUTH_STYLE_DEMANDING";
  if (professionalism >= 0.35f) return "YOUTH_STYLE_BALANCED";
  return "YOUTH_STYLE_RELAXED";
}

const char* personalityKey(const PlayerTraits& traits)
{
  if (traits.professionalism >= 80) return "YOUTH_PERS_MODEL_PRO";
  if (traits.professionalism >= 68) return "YOUTH_PERS_PROFESSIONAL";
  if (traits.ambition >= 75 && traits.professionalism >= 50)
    return "YOUTH_PERS_DRIVEN";
  if (traits.temperament <= 30) return "YOUTH_PERS_VOLATILE";
  if (traits.professionalism <= 32) return "YOUTH_PERS_CASUAL";
  if (traits.ambition <= 25) return "YOUTH_PERS_CONTENT";
  if (traits.loyalty >= 80) return "YOUTH_PERS_LOYAL";
  return "YOUTH_PERS_BALANCED";
}

const char* qualityKey(IntakeQuality quality)
{
  switch (quality)
  {
    case IntakeQuality::Weak:
      return "YOUTH_QUALITY_WEAK";
    case IntakeQuality::Promising:
      return "YOUTH_QUALITY_PROMISING";
    case IntakeQuality::Golden:
      return "YOUTH_QUALITY_GOLDEN";
    case IntakeQuality::Average:
      break;
  }
  return "YOUTH_QUALITY_AVERAGE";
}

const char* contractKey(YouthContract contract)
{
  switch (contract)
  {
    case YouthContract::Scholarship:
      return "YOUTH_CONTRACT_SCHOLARSHIP";
    case YouthContract::Professional:
      return "YOUTH_CONTRACT_PROFESSIONAL";
    case YouthContract::None:
      break;
  }
  return "YOUTH_CONTRACT_NONE";
}

const char* requestResultKey(UpgradeRequestResult result)
{
  switch (result)
  {
    case UpgradeRequestResult::Approved:
      return "YOUTH_REQUEST_APPROVED";
    case UpgradeRequestResult::InProgress:
      return "YOUTH_REQUEST_IN_PROGRESS";
    case UpgradeRequestResult::TooSoon:
      return "YOUTH_REQUEST_TOO_SOON";
    case UpgradeRequestResult::AtMaximum:
      return "YOUTH_REQUEST_AT_MAXIMUM";
    case UpgradeRequestResult::BeyondStature:
      return "YOUTH_REQUEST_BEYOND_STATURE";
    case UpgradeRequestResult::CannotAfford:
      return "YOUTH_REQUEST_CANNOT_AFFORD";
    case UpgradeRequestResult::NoConfidence:
      return "YOUTH_REQUEST_NO_CONFIDENCE";
    case UpgradeRequestResult::NoClub:
      break;
  }
  return "YOUTH_REQUEST_NO_CLUB";
}

const char* actionResultKey(YouthActionResult result)
{
  switch (result)
  {
    case YouthActionResult::Ok:
      return "YOUTH_ACTION_OK";
    case YouthActionResult::UnknownPlayer:
      return "YOUTH_ACTION_UNKNOWN";
    case YouthActionResult::NotAllowed:
      return "YOUTH_ACTION_NOT_ALLOWED";
    case YouthActionResult::TooYoung:
      return "YOUTH_ACTION_TOO_YOUNG";
    case YouthActionResult::TooOld:
      return "YOUTH_ACTION_TOO_OLD";
    case YouthActionResult::NoClub:
      break;
  }
  return "YOUTH_ACTION_NO_CLUB";
}

std::pair<int, int> simulateMatch(float home_strength, float away_strength,
                                  WorldRng& rng)
{
  // Poisson goals around 2.9 per match; ten overall points roughly double
  // the stronger side's scoring rate. Youth football is high scoring. [P]
  const float diff = (home_strength - away_strength) / 10.0f;
  const auto poisson = [&rng](float lambda)
  {
    const double limit = std::exp(-static_cast<double>(lambda));
    double product = rng.uniform01();
    int goals = 0;
    while (product > limit && goals < 12)
    {
      ++goals;
      product *= rng.uniform01();
    }
    return goals;
  };
  const float home_rate = std::clamp(1.55f * std::exp(0.35f * diff), 0.2f, 5.0f);
  const float away_rate =
      std::clamp(1.30f * std::exp(-0.35f * diff), 0.2f, 5.0f);
  return {poisson(home_rate), poisson(away_rate)};
}

std::int64_t upgradeCost(AcademyUpgrade kind, int target, LeagueID league_id)
{
  // Scaled by league revenue: an academy upgrade costs a few months of a
  // mid-table club's revenue at the top, much less lower down. [P]
  const double scale = std::clamp(
      static_cast<double>(leagueProfile(league_id).average_revenue_eur) /
          146e6,
      0.15, 2.6);
  const double base = kind == AcademyUpgrade::Facilities
                          ? 400'000.0 + 70'000.0 * target
                          : 100'000.0 + 20'000.0 * target;
  return std::llround(scale * base / 10'000.0) * 10'000;
}

int upgradeDays(AcademyUpgrade kind, int target)
{
  return kind == AcademyUpgrade::Facilities ? 150 + 2 * target : 45;
}

std::uint8_t upgradeTarget(AcademyUpgrade kind, int current)
{
  const int step = kind == AcademyUpgrade::Facilities ? 10 : 15;
  return static_cast<std::uint8_t>(std::min(100, current + step));
}
}  // namespace YouthModel

// ---------------------------------------------------------------------------
// YouthAcademy
// ---------------------------------------------------------------------------

YouthAcademy::YouthAcademy(std::shared_ptr<GameData> gd)
    : gamedata(std::move(gd))
{
}

void YouthAcademy::ensureReady()
{
  if (!ready) bootstrap();
}

void YouthAcademy::bootstrap()
{
  records.clear();
  clubs.clear();
  managed_results.clear();
  // Clubs in id order: a contract changes the wage scale later contracts of
  // the same club read.
  std::vector<TeamID> team_ids;
  for (const auto& [team_id, team] : gamedata->getTeams())
    if (team_id != FREE_AGENTS_TEAM_ID) team_ids.push_back(team_id);
  std::ranges::sort(team_ids);
  auto& players = gamedata->getPlayers();
  for (const TeamID team_id : team_ids)
  {
    clubState(team_id);
    // Every club player (a save may already list some in the academy).
    const Team& team = gamedata->getTeam(team_id)->get();
    std::vector<PlayerID> teenagers;
    for (const auto* ids : {&team.getPlayerIDs(), &team.getAcademyIDs()})
    {
      for (const PlayerID player_id : *ids)
      {
        const auto player = players.find(player_id);
        if (player != players.end() && player->second.getAge() <= 17)
          teenagers.push_back(player_id);
      }
    }
    std::ranges::sort(teenagers);
    for (const PlayerID player_id : teenagers)
    {
      YouthRecord youth;
      youth.player_id = player_id;
      youth.team_id = team_id;
      youth.status = YouthStatus::Squad;
      // Most teenagers are scholars; the best prospects already hold a
      // first professional contract.
      Player& player = players.at(player_id);
      applyContract(player, youth,
                    deservesProfessional(team_id, player)
                        ? YouthContract::Professional
                        : YouthContract::Scholarship);
      records.emplace(player_id, std::move(youth));
    }
  }
  ready = true;
  syncFlags();
}

void YouthAcademy::syncFlags()
{
  for (auto& [player_id, player] : gamedata->getPlayers())
  {
    const auto found = records.find(player_id);
    const bool academy = found != records.end() &&
                         found->second.status != YouthStatus::Graduated &&
                         found->second.team_id == player.getTeamId();
    // Also moves him between the club's senior and academy lists.
    if (player.isAcademyPlayer() != academy)
      gamedata->setAcademyMember(player_id, academy);
  }
}

AcademyClub& YouthAcademy::clubState(TeamID team_id)
{
  auto [found, inserted] = clubs.try_emplace(team_id);
  if (inserted)
  {
    found->second.team_id = team_id;
    found->second.table.team_id = team_id;
    if (const auto team = gamedata->getTeam(team_id))
      found->second.recruitment =
          defaultRecruitment(gamedata->getWorldSeed(), team->get());
  }
  return found->second;
}

void YouthAcademy::prune()
{
  auto& players = gamedata->getPlayers();
  std::vector<PlayerID> moved;
  std::erase_if(records,
                [&players, &moved](const auto& entry)
                {
                  const YouthRecord& youth = entry.second;
                  const auto player = players.find(youth.player_id);
                  if (player == players.end()) return true;
                  if (player->second.getTeamId() != youth.team_id)
                  {
                    // Sold, released or loaned out: no longer an academy
                    // player anywhere.
                    moved.push_back(youth.player_id);
                    return true;
                  }
                  return youth.status == YouthStatus::Graduated &&
                         player->second.getAge() > 21;
                });
  for (const PlayerID player_id : moved)
    gamedata->setAcademyMember(player_id, false);
}

void YouthAcademy::onDayAdvanced(const GameDateValue& date,
                                 TeamID managed_team_id, Inbox& inbox)
{
  ensureReady();
  const std::int32_t ordinal = dayOrdinal(date);
  if (ordinal % 7 == 0) prune();
  if (date.day == 1) snapshotProgress(date, managed_team_id);
  completeProjects(date, managed_team_id, inbox);
  if (date.month == YouthModel::PREVIEW_MONTH &&
      date.day == YouthModel::PREVIEW_DAY)
    postPreview(date, managed_team_id, inbox);
  const std::int32_t intake = intakeOrdinal(date.year);
  if (ordinal == intake) runIntake(date, managed_team_id, inbox);
  if (ordinal == intake + YouthModel::DECISION_DAYS - 7)
    remindDecisions(date, managed_team_id, inbox);
  if (ordinal == intake + YouthModel::DECISION_DAYS)
    closeDecisions(date, managed_team_id, inbox);
  if (inYouthSeason(date) && ordinal % 7 == MATCH_WEEKDAY)
    playMatchday(date, managed_team_id);
}

void YouthAcademy::onSeasonEnd(const GameDateValue& /*date*/,
                               TeamID managed_team_id)
{
  ensureReady();
  // Computer-managed clubs keep the academy players their staff rate; the
  // others drop out of professional football when their contracts run out
  // (most scholars never play professionally, CT-W12).
  std::vector<std::pair<PlayerID, TeamID>> leaving;
  // In id order: a renewal changes the wage scale later renewals of the
  // same club read, and the hash map's order differs after a reload.
  std::vector<PlayerID> ids;
  ids.reserve(records.size());
  for (const auto& [player_id, youth] : records) ids.push_back(player_id);
  std::ranges::sort(ids);
  for (const PlayerID player_id : ids)
  {
    YouthRecord& youth = records.at(player_id);
    if (youth.team_id == managed_team_id ||
        youth.status != YouthStatus::Squad)
      continue;
    const auto team = gamedata->getTeam(youth.team_id);
    auto& players = gamedata->getPlayers();
    const auto found = players.find(player_id);
    if (!team || found == players.end() ||
        found->second.getContractYears() != 1)
      continue;
    const YouthEstimate judged = estimate(youth.team_id, player_id);
    const float expected = 0.5f * (judged.potential_low + judged.potential_high);
    // The best prospects sign a first professional contract, the others
    // worth keeping another scholarship year or two.
    if (deservesProfessional(youth.team_id, found->second))
      applyContract(found->second, youth, YouthContract::Professional);
    else if (expected >=
             WorldGeneration::teamLevel(team->get().getReputation()) -
                 KEEP_MARGIN)
      applyContract(found->second, youth, YouthContract::Scholarship);
    else
      leaving.emplace_back(player_id, youth.team_id);
  }
  std::ranges::sort(leaving);
  for (const auto& [player_id, team_id] : leaving)
    leaveFootball(team_id, player_id);
}

void YouthAcademy::onSeasonStart(const GameDateValue& date,
                                 TeamID managed_team_id, Inbox& inbox)
{
  ensureReady();
  prune();
  for (auto& [team_id, academy] : clubs)
  {
    academy.table = YouthTableRow{};
    academy.table.team_id = team_id;
  }
  managed_results.clear();

  std::vector<std::string> graduates;
  std::vector<std::string> eligible;
  for (auto& [player_id, youth] : records)
  {
    youth.appearances = 0;
    youth.minutes = 0;
    youth.goals = 0;
    youth.rating_total = 0.0f;
    if (youth.status != YouthStatus::Squad) continue;
    Player& player = gamedata->getPlayers().at(player_id);
    if (player.getAge() > YouthModel::U18_MAX_AGE &&
        youth.team_id == managed_team_id)
    {
      // Over-age players move up to the first-team squad.
      youth.status = YouthStatus::Graduated;
      gamedata->setAcademyMember(player_id, false);
      graduates.push_back(player.getName());
      continue;
    }
    if (youth.team_id != managed_team_id ||
        youth.contract != YouthContract::Scholarship)
      continue;
    const auto team = gamedata->getTeam(youth.team_id);
    if (team && player.getAge() >= YouthModel::firstProfessionalAge(
                                       leagueProfile(team->get().getLeagueId())
                                           .domestic_nationality))
      eligible.push_back(player.getName());
  }
  promoteComputerAcademies(managed_team_id);
  std::ranges::sort(graduates);
  std::ranges::sort(eligible);
  if (!graduates.empty())
    post(inbox, date, InboxCategory::Youth, "INBOX_YOUTH_GRADUATES_TITLE",
         "INBOX_YOUTH_GRADUATES_BODY",
         {std::to_string(graduates.size()), joinNames(graduates)});
  if (!eligible.empty())
    post(inbox, date, InboxCategory::Youth, "INBOX_YOUTH_PRO_AGE_TITLE",
         "INBOX_YOUTH_PRO_AGE_BODY",
         {std::to_string(eligible.size()), joinNames(eligible)});
}

float YouthAcademy::developmentMultiplier(const Player& player) const
{
  const auto found = records.find(player.getId());
  if (found == records.end() || found->second.status != YouthStatus::Squad ||
      found->second.team_id != player.getTeamId())
    return 1.0f;
  const YouthRecord& youth = found->second;
  const auto academy = clubs.find(youth.team_id);
  const std::uint16_t matches =
      academy == clubs.end() ? 0 : academy->second.table.played;
  const float youth_share =
      matches > 0 ? static_cast<float>(youth.minutes) /
                        (90.0f * static_cast<float>(matches))
                  : 0.0f;
  // Same minutes factor as the senior model, fed by U18 minutes.
  const float senior =
      0.6f + 0.4f * std::min(1.0f, player.getDynamics().playing_share / 0.6f);
  const float junior = 0.6f + 0.4f * std::min(1.0f, youth_share / 0.6f);
  const float coaching =
      0.85f + 0.35f * gamedata->getStaff().effects(youth.team_id).coaching_youth;
  float facilities = 1.0f;
  if (const auto team = gamedata->getTeam(youth.team_id))
    facilities = 0.92f + 0.16f * toUnit(team->get().getProfile().youth_facilities);
  return std::clamp(std::max(senior, junior) / senior * coaching * facilities,
                    0.8f, 1.6f);
}

// ---------------------------------------------------------------- Queries

const YouthRecord* YouthAcademy::record(PlayerID player_id) const
{
  const auto found = records.find(player_id);
  return found == records.end() ? nullptr : &found->second;
}

bool YouthAcademy::isAcademyPlayer(PlayerID player_id) const
{
  const auto found = records.find(player_id);
  if (found == records.end() ||
      found->second.status == YouthStatus::Graduated)
    return false;
  const auto player = gamedata->getPlayer(player_id);
  return player && player->get().getTeamId() == found->second.team_id;
}

std::vector<const YouthRecord*> YouthAcademy::members(TeamID team_id,
                                                      YouthStatus status) const
{
  std::vector<const YouthRecord*> found;
  for (const auto& [player_id, youth] : records)
  {
    if (youth.team_id != team_id || youth.status != status) continue;
    const auto player = gamedata->getPlayer(player_id);
    if (player && player->get().getTeamId() == team_id)
      found.push_back(&youth);
  }
  std::ranges::sort(found, {}, &YouthRecord::player_id);
  return found;
}

const AcademyClub* YouthAcademy::club(TeamID team_id) const
{
  const auto found = clubs.find(team_id);
  return found == clubs.end() ? nullptr : &found->second;
}

IntakeInputs YouthAcademy::intakeInputs(TeamID team_id) const
{
  IntakeInputs inputs;
  const auto team = gamedata->getTeam(team_id);
  if (!team) return inputs;
  inputs.facilities = toUnit(team->get().getProfile().youth_facilities);
  const auto academy = clubs.find(team_id);
  inputs.recruitment = toUnit(
      academy != clubs.end()
          ? academy->second.recruitment
          : defaultRecruitment(gamedata->getWorldSeed(), team->get()));
  const StaffEffects& effects = gamedata->getStaff().effects(team_id);
  inputs.junior_coaching = effects.coaching_youth;
  inputs.staff_bonus = effects.youth_potential_bonus;
  const HeadOfYouth head = headOfYouth(team_id);
  if (head.id != 0)
  {
    inputs.head_ability = toUnit(head.ability);
    inputs.head_professionalism = head.professionalism;
  }
  inputs.reputation = team->get().getReputation();
  inputs.country = leagueProfile(team->get().getLeagueId()).domestic_nationality;
  return inputs;
}

AcademyRatings YouthAcademy::ratings(TeamID team_id) const
{
  AcademyRatings result;
  const auto team = gamedata->getTeam(team_id);
  if (!team) return result;
  const IntakeInputs inputs = intakeInputs(team_id);
  result.facilities = team->get().getProfile().youth_facilities;
  result.recruitment =
      static_cast<std::uint8_t>(std::lround(inputs.recruitment * 100.0f));
  result.junior_coaching = static_cast<std::uint8_t>(
      std::lround(std::clamp(inputs.junior_coaching, 0.0f, 1.0f) * 100.0f));
  for (const StaffMember* member : gamedata->getStaff().clubStaff(team_id))
  {
    if (member->role == StaffRole::HeadOfYouth)
    {
      result.head = StaffModel::rating(*member);
      break;
    }
  }
  return result;
}

HeadOfYouth YouthAcademy::headOfYouth(TeamID team_id) const
{
  HeadOfYouth head;
  for (const StaffMember* member : gamedata->getStaff().clubStaff(team_id))
  {
    if (member->role != StaffRole::HeadOfYouth) continue;
    head.id = member->id;
    head.name = member->name();
    head.ability = member->attribute(StaffAttribute::YouthDevelopment);
    head.judging = member->attribute(StaffAttribute::JudgingPotential);
    head.professionalism =
        YouthModel::staffProfessionalism(gamedata->getWorldSeed(), member->id);
    head.style_key = YouthModel::staffStyleKey(head.professionalism);
    break;
  }
  return head;
}

IntakePreview YouthAcademy::preview(TeamID team_id, std::uint16_t year) const
{
  return YouthModel::summarize(YouthModel::planIntake(
      intakeInputs(team_id), gamedata->getWorldSeed(), year, team_id));
}

float YouthAcademy::judgingSd(TeamID team_id) const
{
  // The head of youth development judges the youngsters; without one the
  // best youth coach does it less reliably.
  float accuracy = 0.15f;
  for (const StaffMember* member : gamedata->getStaff().clubStaff(team_id))
  {
    const float judging =
        toUnit(member->attribute(StaffAttribute::JudgingPotential));
    if (member->role == StaffRole::HeadOfYouth)
      accuracy = std::max(accuracy, judging);
    else if (member->role == StaffRole::YouthCoach)
      accuracy = std::max(accuracy, 0.8f * judging);
  }
  return StaffModel::potentialErrorSd(accuracy);
}

YouthEstimate YouthAcademy::estimate(TeamID viewer, PlayerID player_id) const
{
  YouthEstimate result;
  const auto player = gamedata->getPlayer(player_id);
  if (!player) return result;
  const auto current =
      static_cast<float>(player->get().getOverall(gamedata->getStatsConfig()));
  const float potential = std::max(current, player->get().getPotential());
  const float sd = judgingSd(viewer);
  // A fixed judgement per player and club: the bias does not change from
  // one day to the next, the ranges follow the player's development.
  WorldRng rng = WorldRng::stream(gamedata->getWorldSeed(),
                                  RngDomain::YouthIntake,
                                  ESTIMATE_SALT | player_id, viewer);
  const float current_centre = current + rng.normal(0.0f, 0.25f * sd);
  const float potential_centre = potential + rng.normal(0.0f, 0.5f * sd);
  const float current_half = 0.35f * sd + 1.0f;
  const float potential_half = 0.8f * sd + 1.0f;
  const auto clampRating = [](float value)
  { return std::clamp(std::round(value), 1.0f, 99.0f); };
  result.current_low = clampRating(current_centre - current_half);
  result.current_high = clampRating(current_centre + current_half);
  result.potential_low =
      std::max(result.current_low, clampRating(potential_centre - potential_half));
  result.potential_high =
      std::max(result.current_high, clampRating(potential_centre + potential_half));
  return result;
}

std::vector<YouthTableRow> YouthAcademy::table(LeagueID league_id) const
{
  std::vector<YouthTableRow> rows;
  const auto league = gamedata->getLeague(league_id);
  if (!league) return rows;
  for (const TeamID team_id : league->get().getTeamIDs())
  {
    const auto academy = clubs.find(team_id);
    YouthTableRow row;
    if (academy != clubs.end()) row = academy->second.table;
    row.team_id = team_id;
    rows.push_back(row);
  }
  std::ranges::sort(rows,
                    [](const YouthTableRow& a, const YouthTableRow& b)
                    {
                      if (a.points() != b.points())
                        return a.points() > b.points();
                      const int diff_a = a.goals_for - a.goals_against;
                      const int diff_b = b.goals_for - b.goals_against;
                      if (diff_a != diff_b) return diff_a > diff_b;
                      if (a.goals_for != b.goals_for)
                        return a.goals_for > b.goals_for;
                      return a.team_id < b.team_id;
                    });
  return rows;
}

bool YouthAcademy::isHomegrown(PlayerID player_id) const
{
  const auto found = records.find(player_id);
  if (found == records.end() || found->second.status == YouthStatus::Candidate)
    return false;
  const auto player = gamedata->getPlayer(player_id);
  return player &&
         YouthModel::isHomegrown(found->second.joined_age,
                                 player->get().getAge());
}

UpgradeQuote YouthAcademy::quote(TeamID team_id, AcademyUpgrade kind,
                                 const GameDateValue& date,
                                 float board_confidence, bool embargoed) const
{
  UpgradeQuote result;
  result.kind = kind;
  const auto team = gamedata->getTeam(team_id);
  const auto academy = clubs.find(team_id);
  if (!team || academy == clubs.end() || kind == AcademyUpgrade::None)
  {
    result.verdict = UpgradeRequestResult::NoClub;
    return result;
  }
  const AcademyClub& state = academy->second;
  result.current = kind == AcademyUpgrade::Facilities
                       ? team->get().getProfile().youth_facilities
                       : state.recruitment;
  result.target = YouthModel::upgradeTarget(kind, result.current);
  result.cost =
      YouthModel::upgradeCost(kind, result.target, team->get().getLeagueId());
  result.days = YouthModel::upgradeDays(kind, result.target);
  if (state.project == kind)
  {
    result.running_start_day = state.project_start_day;
    result.running_done_day = state.project_done_day;
    result.target = state.project_target;
  }
  const Finances& finances = team->get().getFinances();
  const std::int64_t reserve =
      PAYROLL_RESERVE_WEEKS *
      finances.getCurrentWageSpending(*gamedata, team->get());
  const std::int32_t ordinal = dayOrdinal(date);
  if (state.project != AcademyUpgrade::None)
    result.verdict = UpgradeRequestResult::InProgress;
  else if (result.current >= 100)
    result.verdict = UpgradeRequestResult::AtMaximum;
  else if (state.last_request_day > 0 &&
           ordinal - state.last_request_day < REQUEST_COOLDOWN_DAYS)
    result.verdict = UpgradeRequestResult::TooSoon;
  else if (result.target > team->get().getReputation() + STATURE_HEADROOM)
    result.verdict = UpgradeRequestResult::BeyondStature;
  else if (embargoed || finances.getBalance() - reserve < result.cost)
    result.verdict = UpgradeRequestResult::CannotAfford;
  else if (board_confidence < MIN_BOARD_CONFIDENCE)
    result.verdict = UpgradeRequestResult::NoConfidence;
  return result;
}

// ---------------------------------------------------------------- Actions

YouthActionResult YouthAcademy::signCandidate(TeamID team_id,
                                              PlayerID player_id)
{
  const auto found = records.find(player_id);
  auto& players = gamedata->getPlayers();
  const auto player = players.find(player_id);
  if (found == records.end() || player == players.end())
    return YouthActionResult::UnknownPlayer;
  YouthRecord& youth = found->second;
  if (youth.team_id != team_id || youth.status != YouthStatus::Candidate ||
      player->second.getTeamId() != team_id)
    return YouthActionResult::NotAllowed;
  const auto team = gamedata->getTeam(team_id);
  if (!team) return YouthActionResult::NoClub;
  const int pro_age = YouthModel::firstProfessionalAge(
      leagueProfile(team->get().getLeagueId()).domestic_nationality);
  youth.status = YouthStatus::Squad;
  youth.joined_age = static_cast<std::uint8_t>(player->second.getAge());
  applyContract(player->second, youth,
                player->second.getAge() >= pro_age
                    ? YouthContract::Professional
                    : YouthContract::Scholarship);
  return YouthActionResult::Ok;
}

YouthActionResult YouthAcademy::releaseCandidate(TeamID team_id,
                                                 PlayerID player_id)
{
  const auto found = records.find(player_id);
  if (found == records.end() || !gamedata->getPlayer(player_id))
    return YouthActionResult::UnknownPlayer;
  if (found->second.team_id != team_id ||
      found->second.status != YouthStatus::Candidate)
    return YouthActionResult::NotAllowed;
  leaveFootball(team_id, player_id);
  return YouthActionResult::Ok;
}

void YouthAcademy::leaveFootball(TeamID team_id, PlayerID player_id)
{
  if (const auto team = gamedata->getTeam(team_id))
  {
    Team& club = team->get();
    club.removePlayerID(player_id);
    // Line-ups hold raw pointers: rebuild one that picked the player.
    const Lineup& lineup = club.getLineup();
    const bool selected =
        (lineup.getGoalkeeper() &&
         lineup.getGoalkeeper()->getId() == player_id) ||
        std::ranges::any_of(lineup.getOutfieldPlayers(),
                            [player_id](const auto& positioned)
                            {
                              return positioned.player &&
                                     positioned.player->getId() == player_id;
                            });
    if (selected) club.generateStartingXI(*gamedata, gamedata->getStatsConfig());
  }
  records.erase(player_id);
  gamedata->removePlayer(player_id);
}

std::size_t YouthAcademy::firstTeamSize(TeamID team_id) const
{
  const auto team = gamedata->getTeam(team_id);
  return team ? team->get().getPlayerIDs().size() : 0;
}

void YouthAcademy::promoteComputerAcademies(TeamID managed_team_id)
{
  // After ageing: 19-year-olds who are good enough for the senior squad
  // move up, the rest drop out; clearly ready 17 and 18-year-olds move up
  // early. Promotions never push the senior squad beyond its limit.
  const StatsConfig& config = gamedata->getStatsConfig();
  std::unordered_map<TeamID, std::vector<std::pair<float, PlayerID>>> squads;
  for (const auto& [player_id, youth] : records)
  {
    if (youth.team_id == managed_team_id || youth.status != YouthStatus::Squad)
      continue;
    const auto player = gamedata->getPlayer(player_id);
    if (!player || player->get().getTeamId() != youth.team_id) continue;
    squads[youth.team_id].emplace_back(
        static_cast<float>(player->get().getOverall(config)), player_id);
  }
  std::vector<TeamID> team_ids;
  for (const auto& [team_id, squad] : squads) team_ids.push_back(team_id);
  std::ranges::sort(team_ids);
  for (const TeamID team_id : team_ids)
  {
    auto& squad = squads[team_id];
    std::ranges::sort(squad, std::greater<>{});
    const auto team = gamedata->getTeam(team_id);
    if (!team) continue;
    const float level = WorldGeneration::teamLevel(team->get().getReputation());
    std::size_t seniors = firstTeamSize(team_id);
    std::vector<PlayerID> leaving;
    for (const auto& [overall, player_id] : squad)
    {
      Player& player = gamedata->getPlayers().at(player_id);
      const bool over_age = player.getAge() > YouthModel::U18_MAX_AGE;
      const float margin = over_age ? READY_MARGIN : EARLY_READY_MARGIN;
      YouthRecord& youth = records.at(player_id);
      if (overall >= level - margin && seniors < FIRST_TEAM_LIMIT)
      {
        // First-team players hold professional contracts.
        if (youth.contract != YouthContract::Professional)
          applyContract(player, youth, YouthContract::Professional);
        records.erase(player_id);
        gamedata->setAcademyMember(player.getId(), false);
        ++seniors;
      }
      else if (over_age)
      {
        leaving.push_back(player_id);
      }
      else if (youth.contract == YouthContract::Scholarship &&
               deservesProfessional(team_id, player))
      {
        // A year older: the best scholars turn professional.
        applyContract(player, youth, YouthContract::Professional);
      }
    }
    for (const PlayerID player_id : leaving) leaveFootball(team_id, player_id);
  }
}

YouthActionResult YouthAcademy::offerProfessional(TeamID team_id,
                                                  PlayerID player_id)
{
  const auto found = records.find(player_id);
  auto& players = gamedata->getPlayers();
  const auto player = players.find(player_id);
  if (found == records.end() || player == players.end())
    return YouthActionResult::UnknownPlayer;
  YouthRecord& youth = found->second;
  if (youth.team_id != team_id || youth.status != YouthStatus::Squad ||
      youth.contract != YouthContract::Scholarship ||
      player->second.getTeamId() != team_id)
    return YouthActionResult::NotAllowed;
  const auto team = gamedata->getTeam(team_id);
  if (!team) return YouthActionResult::NoClub;
  if (player->second.getAge() <
      YouthModel::firstProfessionalAge(
          leagueProfile(team->get().getLeagueId()).domestic_nationality))
    return YouthActionResult::TooYoung;
  applyContract(player->second, youth, YouthContract::Professional);
  return YouthActionResult::Ok;
}

YouthActionResult YouthAcademy::promote(TeamID team_id, PlayerID player_id)
{
  const auto found = records.find(player_id);
  auto& players = gamedata->getPlayers();
  const auto player = players.find(player_id);
  if (found == records.end() || player == players.end())
    return YouthActionResult::UnknownPlayer;
  YouthRecord& youth = found->second;
  if (youth.team_id != team_id || youth.status != YouthStatus::Squad ||
      player->second.getTeamId() != team_id)
    return YouthActionResult::NotAllowed;
  // First-team players hold professional contracts.
  if (youth.contract != YouthContract::Professional)
  {
    const YouthActionResult signed_pro = offerProfessional(team_id, player_id);
    if (signed_pro != YouthActionResult::Ok) return signed_pro;
  }
  youth.status = YouthStatus::Graduated;
  gamedata->setAcademyMember(player_id, false);
  return YouthActionResult::Ok;
}

YouthActionResult YouthAcademy::demote(TeamID team_id, PlayerID player_id)
{
  const auto player = gamedata->getPlayer(player_id);
  if (!player) return YouthActionResult::UnknownPlayer;
  if (player->get().getTeamId() != team_id)
    return YouthActionResult::NotAllowed;
  if (player->get().getAge() > YouthModel::U18_MAX_AGE)
    return YouthActionResult::TooOld;
  auto [found, inserted] = records.try_emplace(player_id);
  YouthRecord& youth = found->second;
  if (!inserted && youth.status != YouthStatus::Graduated)
    return YouthActionResult::NotAllowed;
  if (inserted)
  {
    youth.player_id = player_id;
    youth.team_id = team_id;
    youth.joined_age = static_cast<std::uint8_t>(player->get().getAge());
    youth.contract = YouthContract::Professional;
  }
  youth.status = YouthStatus::Squad;
  gamedata->setAcademyMember(player_id, true);
  return YouthActionResult::Ok;
}

UpgradeRequestResult YouthAcademy::requestUpgrade(const GameDateValue& date,
                                                  TeamID team_id,
                                                  AcademyUpgrade kind,
                                                  float board_confidence,
                                                  bool embargoed, Inbox& inbox)
{
  ensureReady();
  const UpgradeQuote offer =
      quote(team_id, kind, date, board_confidence, embargoed);
  switch (offer.verdict)
  {
    case UpgradeRequestResult::NoClub:
    case UpgradeRequestResult::InProgress:
    case UpgradeRequestResult::AtMaximum:
    case UpgradeRequestResult::TooSoon:
      return offer.verdict;
    default:
      break;
  }
  AcademyClub& state = clubState(team_id);
  const std::int32_t ordinal = dayOrdinal(date);
  state.last_request_day = ordinal;
  const char* kind_key = kind == AcademyUpgrade::Facilities
                             ? "@YOUTH_UPGRADE_FACILITIES"
                             : "@YOUTH_UPGRADE_RECRUITMENT";
  if (offer.verdict != UpgradeRequestResult::Approved)
  {
    post(inbox, date, InboxCategory::Board, "INBOX_ACADEMY_REFUSED_TITLE",
         "INBOX_ACADEMY_REFUSED_BODY",
         {kind_key,
          std::string("@") + YouthModel::requestResultKey(offer.verdict)});
    return offer.verdict;
  }
  Team& team = gamedata->getTeam(team_id)->get();
  team.getFinances().record(date, FinanceCategory::Facilities, -offer.cost);
  state.project = kind;
  state.project_start_day = ordinal;
  state.project_done_day = ordinal + offer.days;
  state.project_target = offer.target;
  post(inbox, date, InboxCategory::Board, "INBOX_ACADEMY_APPROVED_TITLE",
       "INBOX_ACADEMY_APPROVED_BODY",
       {kind_key, formatMoney(offer.cost), std::to_string(offer.days),
        std::to_string(offer.target)});
  return UpgradeRequestResult::Approved;
}

// ---------------------------------------------------------------- Cycle

std::string YouthAcademy::headName(TeamID team_id) const
{
  const HeadOfYouth head = headOfYouth(team_id);
  return head.id != 0 ? head.name : std::string("@YOUTH_ACADEMY_STAFF");
}

void YouthAcademy::postPreview(const GameDateValue& date,
                               TeamID managed_team_id, Inbox& inbox) const
{
  if (managed_team_id == FREE_AGENTS_TEAM_ID) return;
  const IntakePreview outlook = preview(managed_team_id, date.year);
  std::string positions = RoleUtils::shortNameArg(outlook.standout[0]);
  if (outlook.standout[1] != PlayerRole::UNKNOWN)
    positions += ", " + RoleUtils::shortNameArg(outlook.standout[1]);
  post(inbox, date, InboxCategory::Youth, "INBOX_YOUTH_PREVIEW_TITLE",
       "INBOX_YOUTH_PREVIEW_BODY",
       {headName(managed_team_id),
        std::string("@") + YouthModel::qualityKey(outlook.quality), positions,
        std::string("@") + outlook.personality_key,
        std::to_string(outlook.size)});
}

double YouthAcademy::wageScale(TeamID team_id) const
{
  const auto team = gamedata->getTeam(team_id);
  if (!team) return 0.0;
  const StatsConfig& config = gamedata->getStatsConfig();
  double index_total = 0.0;
  std::int64_t payroll = 0;
  for (const PlayerID player_id : team->get().getPlayerIDs())
  {
    const auto player = gamedata->getPlayer(player_id);
    if (!player || player->get().getWage() == 0) continue;
    index_total += ClubEconomy::wageIndex(player->get().getOverall(config),
                                          player->get().getAge());
    payroll += player->get().getWage();
  }
  return index_total > 0.0 ? static_cast<double>(payroll) / index_total : 0.0;
}

std::uint32_t YouthAcademy::contractWage(TeamID team_id, const Player& player,
                                         YouthContract contract) const
{
  const auto team = gamedata->getTeam(team_id);
  if (!team || contract == YouthContract::None) return 0;
  if (contract == YouthContract::Scholarship)
    return YouthModel::scholarshipWage(team->get().getLeagueId());
  const double index = ClubEconomy::wageIndex(
      player.getOverall(gamedata->getStatsConfig()), player.getAge());
  // A first professional contract always pays more than a scholarship.
  const double scholarship =
      YouthModel::scholarshipWage(team->get().getLeagueId());
  return roundWage(wageScale(team_id) * index,
                   std::max(300.0, 2.0 * scholarship));
}

void YouthAcademy::applyContract(Player& player, YouthRecord& youth,
                                 YouthContract contract) const
{
  youth.contract = contract;
  player.setWage(contractWage(youth.team_id, player, contract));
  player.setContractYears(contract == YouthContract::Scholarship
                              ? YouthModel::youthContractYears(player.getAge())
                              : YouthModel::MINOR_CONTRACT_YEARS);
}

bool YouthAcademy::deservesProfessional(TeamID team_id,
                                        const Player& player) const
{
  const auto team = gamedata->getTeam(team_id);
  if (!team) return false;
  const int age = std::max(
      YouthModel::PRO_POLICY_AGE,
      YouthModel::firstProfessionalAge(
          leagueProfile(team->get().getLeagueId()).domestic_nationality));
  if (player.getAge() < age) return false;
  const YouthEstimate judged = estimate(team_id, player.getId());
  const float expected = 0.5f * (judged.potential_low + judged.potential_high);
  return expected >= WorldGeneration::teamLevel(team->get().getReputation()) -
                         PRO_PROSPECT_MARGIN;
}

void YouthAcademy::runIntake(const GameDateValue& date, TeamID managed_team_id,
                             Inbox& inbox)
{
  const StatsConfig& config = gamedata->getStatsConfig();
  const std::uint64_t seed = gamedata->getWorldSeed();
  std::vector<TeamID> team_ids;
  for (const auto& [team_id, team] : gamedata->getTeams())
  {
    if (team_id != FREE_AGENTS_TEAM_ID) team_ids.push_back(team_id);
  }
  // Sorted so player ids are allocated in the same order on every run.
  std::ranges::sort(team_ids);
  // Youngsters get names nobody in the world carries yet.
  NameRegistry names;
  for (const auto& [player_id, player] : gamedata->getPlayers())
    names.claim(player.getName());
  std::unordered_map<TeamID, int> u18_sizes;
  for (const auto& [player_id, youth] : records)
  {
    if (youth.status == YouthStatus::Squad) ++u18_sizes[youth.team_id];
  }

  for (const TeamID team_id : team_ids)
  {
    Team& team = gamedata->getTeam(team_id)->get();
    clubState(team_id);
    const IntakeInputs inputs = intakeInputs(team_id);
    const IntakeClass intake =
        YouthModel::planIntake(inputs, seed, date.year, team_id);
    const bool managed = team_id == managed_team_id;

    // The club's staff rank the group by their (imperfect) judgement.
    const float sd = judgingSd(team_id);
    std::vector<std::pair<float, std::size_t>> ranked;
    for (std::size_t index = 0; index < intake.candidates.size(); ++index)
    {
      const float noise = WorldRng::stream(seed, RngDomain::YouthIntake,
                                           SELECTION_SALT | date.year,
                                           candidateKey(team_id, index))
                              .normal(0.0f, 0.5f * sd);
      ranked.emplace_back(intake.candidates[index].potential + noise, index);
    }
    std::ranges::sort(ranked, std::greater<>{});
    std::size_t take = ranked.size();
    if (!managed)
    {
      // Bigger and better academies sign more of their (bigger) intake.
      const float grade = YouthModel::academyGrade(inputs);
      const int room = static_cast<int>(YouthModel::u18SquadLimit(grade)) -
                       u18_sizes[team_id];
      take = static_cast<std::size_t>(std::clamp(
          std::min(YouthModel::computerSignings(grade, ranked.size()), room),
          0, YouthModel::SIGN_MAX));
    }
    if (take == 0) continue;

    WorldRng rng = WorldRng::stream(seed, RngDomain::YouthIntake,
                                    MATERIALIZE_SALT | date.year, team_id);
    SquadSurnames surnames;
    for (const auto* ids : {&team.getPlayerIDs(), &team.getAcademyIDs()})
    {
      for (const PlayerID player_id : *ids)
      {
        if (const auto player = gamedata->getPlayer(player_id))
          surnames.add(player->get().getLastName());
      }
    }
    PlayerID best = 0;
    bool special = false;
    for (std::size_t rank = 0; rank < take; ++rank)
    {
      const CandidateProfile& profile = intake.candidates[ranked[rank].second];
      Language nationality = inputs.country;
      if (!profile.local)
      {
        std::array<float, FOREIGN_YOUTH.size()> weights{};
        for (std::size_t i = 0; i < FOREIGN_YOUTH.size(); ++i)
          weights[i] = FOREIGN_YOUTH[i] == inputs.country ? 0.0f : 1.0f;
        nationality = FOREIGN_YOUTH[rng.weightedIndex(weights)];
      }
      const auto [first, last] =
          WorldGeneration::drawName(rng, nationality, names, surnames);
      const float age_shrink = 2.0f * static_cast<float>(17 - profile.age);
      const auto height = static_cast<std::uint8_t>(std::lround(
          std::clamp(rng.normal(heightMean(profile.role) - age_shrink, 5.5f),
                     155.0f, 205.0f)));
      const Foot foot =
          rng.chance(leftSided(profile.role) ? 0.65 : 0.2) ? Foot::Left
                                                           : Foot::Right;
      Player youngster(gamedata->allocatePlayerId(), team_id, first, last,
                       profile.role, nationality, 0, 0, profile.age,
                       YouthModel::youthContractYears(profile.age), height,
                       foot,
                       youthStats(rng, profile.role, profile.current, config));
      youngster.setTraits(profile.traits);
      youngster.setPotential(profile.potential);
      youngster.setAcademyPlayer(true);
      const PlayerID youngster_id = youngster.getId();
      gamedata->addPlayer(youngster_id, youngster);
      team.addAcademyID(youngster_id);

      YouthRecord youth;
      youth.player_id = youngster_id;
      youth.team_id = team_id;
      youth.joined_age = profile.age;
      if (managed)
      {
        // Trialists: nothing is paid until the manager offers a contract.
        youth.status = YouthStatus::Candidate;
        youth.contract = YouthContract::None;
      }
      else
      {
        // Scholarships for the intake; a first professional contract only
        // for an outstanding prospect old enough to sign one.
        youth.status = YouthStatus::Squad;
        Player& signed_player = gamedata->getPlayers().at(youngster_id);
        applyContract(signed_player, youth,
                      deservesProfessional(team_id, signed_player)
                          ? YouthContract::Professional
                          : YouthContract::Scholarship);
      }
      records.insert_or_assign(youngster_id, std::move(youth));
      if (rank == 0) best = youngster_id;
      special = special || profile.wonderkid;
    }
    if (managed && best != 0)
    {
      const Player& top = gamedata->getPlayers().at(best);
      post(
          inbox, date, InboxCategory::Youth, "INBOX_YOUTH_INTAKE_TITLE",
          special ? "INBOX_YOUTH_INTAKE_SPECIAL_BODY"
                  : "INBOX_YOUTH_INTAKE_BODY",
          {headName(team_id), std::to_string(take), top.getName(),
           RoleUtils::shortNameArg(top.getRole()), std::to_string(top.getAge()),
           std::to_string(YouthModel::DECISION_DAYS)},
          best);
    }
  }
}

void YouthAcademy::remindDecisions(const GameDateValue& date,
                                   TeamID managed_team_id, Inbox& inbox) const
{
  if (managed_team_id == FREE_AGENTS_TEAM_ID) return;
  const std::size_t pending =
      members(managed_team_id, YouthStatus::Candidate).size();
  if (pending == 0) return;
  post(inbox, date, InboxCategory::Youth, "INBOX_YOUTH_REMINDER_TITLE",
       "INBOX_YOUTH_REMINDER_BODY", {std::to_string(pending), "7"});
}

void YouthAcademy::closeDecisions(const GameDateValue& date,
                                  TeamID managed_team_id, Inbox& inbox)
{
  std::vector<std::pair<PlayerID, TeamID>> leaving;
  for (const auto& [player_id, youth] : records)
  {
    if (youth.status == YouthStatus::Candidate)
      leaving.emplace_back(player_id, youth.team_id);
  }
  std::ranges::sort(leaving);
  std::vector<std::string> names;
  for (const auto& [player_id, team_id] : leaving)
  {
    if (team_id == managed_team_id)
    {
      if (const auto player = gamedata->getPlayer(player_id))
        names.push_back(player->get().getName());
    }
    releaseCandidate(team_id, player_id);
  }
  if (names.empty()) return;
  std::ranges::sort(names);
  post(inbox, date, InboxCategory::Youth, "INBOX_YOUTH_DEADLINE_TITLE",
       "INBOX_YOUTH_DEADLINE_BODY",
       {std::to_string(names.size()), joinNames(names)});
}

void YouthAcademy::playMatchday(const GameDateValue& date,
                                TeamID managed_team_id)
{
  const StatsConfig& config = gamedata->getStatsConfig();
  const std::int32_t ordinal = dayOrdinal(date);
  const std::uint16_t season = seasonYear(date);
  const std::int32_t round =
      (ordinal - dayOrdinal(GameDateValue(season, SEASON_START_MONTH,
                                          SEASON_START_DAY))) /
      7;

  // Team sheets: every club's available U18 players, development priority
  // first (ability plus a share of the room left to grow).
  struct Sheet
  {
    std::vector<std::pair<float, Player*>> players;
    float strength = STAND_IN_OVERALL;
  };
  std::unordered_map<TeamID, Sheet> sheets;
  auto& players = gamedata->getPlayers();
  for (const auto& [player_id, youth] : records)
  {
    if (youth.status != YouthStatus::Squad) continue;
    const auto found = players.find(player_id);
    if (found == players.end() || found->second.getTeamId() != youth.team_id ||
        !found->second.isAvailable())
      continue;
    Player& player = found->second;
    const auto overall = static_cast<float>(player.getOverall(config));
    const float priority =
        overall + 0.25f * std::max(0.0f, player.getPotential() - overall);
    sheets[youth.team_id].players.emplace_back(priority, &player);
  }
  for (auto& [team_id, sheet] : sheets)
  {
    std::ranges::sort(sheet.players,
                      [](const auto& a, const auto& b)
                      {
                        return a.first != b.first
                                   ? a.first > b.first
                                   : a.second->getId() < b.second->getId();
                      });
    float total = 0.0f;
    for (std::size_t i = 0; i < 11; ++i)
      total += i < sheet.players.size()
                   ? static_cast<float>(
                         sheet.players[i].second->getOverall(config))
                   : STAND_IN_OVERALL;
    sheet.strength = total / 11.0f;
  }

  const auto play = [&](TeamID home, TeamID away)
  {
    WorldRng rng = WorldRng::stream(gamedata->getWorldSeed(),
                                    RngDomain::YouthIntake,
                                    MATCH_SALT | static_cast<std::uint32_t>(ordinal),
                                    home);
    Sheet& home_sheet = sheets[home];
    Sheet& away_sheet = sheets[away];
    const auto [home_goals, away_goals] = YouthModel::simulateMatch(
        home_sheet.strength, away_sheet.strength, rng);
    const auto record_side = [&](TeamID team_id, Sheet& sheet, int scored,
                                 int conceded)
    {
      YouthTableRow& row = clubState(team_id).table;
      ++row.played;
      row.goals_for = static_cast<std::uint16_t>(row.goals_for + scored);
      row.goals_against =
          static_cast<std::uint16_t>(row.goals_against + conceded);
      if (scored > conceded)
        ++row.won;
      else if (scored == conceded)
        ++row.drawn;
      else
        ++row.lost;
      // Eleven starters and three substitutes; goals follow the roles.
      const std::size_t used = std::min<std::size_t>(14, sheet.players.size());
      std::array<float, 14> weights{};
      std::array<int, 14> goals{};
      for (std::size_t i = 0; i < used; ++i)
        weights[i] = scoringWeight(sheet.players[i].second->getRole()) *
                     (i < 11 ? 1.0f : 0.3f);
      const bool anyone =
          std::ranges::any_of(weights, [](float w) { return w > 0.0f; });
      for (int goal = 0; goal < scored && anyone; ++goal)
        ++goals[rng.weightedIndex(std::span<const float>(weights.data(), used))];
      const float margin = static_cast<float>(std::clamp(scored - conceded, -3, 3));
      for (std::size_t i = 0; i < used; ++i)
      {
        Player& player = *sheet.players[i].second;
        const auto found = records.find(player.getId());
        if (found == records.end()) continue;
        YouthRecord& youth = found->second;
        const float rating = std::clamp(
            6.4f + 0.25f * margin +
                0.03f * (static_cast<float>(player.getOverall(config)) -
                         sheet.strength) +
                0.6f * static_cast<float>(goals[i]) + rng.normal(0.0f, 0.45f),
            4.0f, 9.8f);
        ++youth.appearances;
        youth.minutes = static_cast<std::uint16_t>(youth.minutes +
                                                   (i < 11 ? 90 : 25));
        youth.goals = static_cast<std::uint16_t>(youth.goals + goals[i]);
        youth.rating_total += rating;
      }
      if (team_id == managed_team_id)
      {
        managed_results.push_back(
            {date, team_id == home ? away : home, team_id == home,
             static_cast<std::uint8_t>(scored),
             static_cast<std::uint8_t>(conceded)});
      }
    };
    record_side(home, home_sheet, home_goals, away_goals);
    record_side(away, away_sheet, away_goals, home_goals);
  };

  // Round robin by league (circle method), home and away alternating.
  for (const auto& [league_id, league] : gamedata->getLeagues())
  {
    std::vector<TeamID> teams = league.getTeamIDs();
    std::ranges::sort(teams);
    if (teams.size() < 2) continue;
    if (teams.size() % 2 == 1) teams.push_back(FREE_AGENTS_TEAM_ID);
    const auto slots = static_cast<std::int32_t>(teams.size());
    const std::int32_t rounds = slots - 1;
    const std::int32_t turn = round % rounds;
    const bool second_half = (round / rounds) % 2 == 1;
    const auto slot = [&](std::int32_t position)
    {
      return position == 0
                 ? teams[0]
                 : teams[static_cast<std::size_t>(
                       1 + (position - 1 + turn) % rounds)];
    };
    for (std::int32_t i = 0; i < slots / 2; ++i)
    {
      const TeamID first = std::min(slot(i), slot(slots - 1 - i));
      const TeamID second = std::max(slot(i), slot(slots - 1 - i));
      if (first == FREE_AGENTS_TEAM_ID) continue;
      // Each pair meets once per half: a coin decides who hosts the first
      // meeting, the return match swaps.
      const bool first_hosts =
          (WorldRng::hashUniform(gamedata->getWorldSeed(),
                                 RngDomain::YouthIntake, MATCH_SALT | season,
                                 (static_cast<std::uint64_t>(first) << 32) |
                                     second) < 0.5) != second_half;
      play(first_hosts ? first : second, first_hosts ? second : first);
    }
  }
}

void YouthAcademy::completeProjects(const GameDateValue& date,
                                    TeamID managed_team_id, Inbox& inbox)
{
  const std::int32_t ordinal = dayOrdinal(date);
  for (auto& [team_id, academy] : clubs)
  {
    if (academy.project == AcademyUpgrade::None ||
        academy.project_done_day > ordinal)
      continue;
    const auto team = gamedata->getTeam(team_id);
    if (team && academy.project == AcademyUpgrade::Facilities)
    {
      ClubProfile profile = team->get().getProfile();
      profile.youth_facilities =
          std::max(profile.youth_facilities, academy.project_target);
      team->get().setProfile(profile);
    }
    else if (academy.project == AcademyUpgrade::Recruitment)
    {
      academy.recruitment =
          std::max(academy.recruitment, academy.project_target);
    }
    if (team_id == managed_team_id)
    {
      post(inbox, date, InboxCategory::Board, "INBOX_ACADEMY_DONE_TITLE",
           "INBOX_ACADEMY_DONE_BODY",
           {academy.project == AcademyUpgrade::Facilities
                ? "@YOUTH_UPGRADE_FACILITIES"
                : "@YOUTH_UPGRADE_RECRUITMENT",
            std::to_string(academy.project_target)});
    }
    academy.project = AcademyUpgrade::None;
    academy.project_start_day = 0;
    academy.project_done_day = 0;
    academy.project_target = 0;
  }
}

void YouthAcademy::snapshotProgress(const GameDateValue& date,
                                    TeamID managed_team_id)
{
  if (managed_team_id == FREE_AGENTS_TEAM_ID) return;
  const StatsConfig& config = gamedata->getStatsConfig();
  const std::int32_t ordinal = dayOrdinal(date);
  for (auto& [player_id, youth] : records)
  {
    if (youth.team_id != managed_team_id ||
        youth.status == YouthStatus::Candidate)
      continue;
    const auto player = gamedata->getPlayer(player_id);
    if (!player) continue;
    youth.progress.push_back(
        {ordinal, static_cast<float>(player->get().getOverall(config))});
    if (youth.progress.size() > PROGRESS_POINTS)
      youth.progress.erase(youth.progress.begin());
  }
}

// ---------------------------------------------------------------- Persistence

void YouthAcademy::load(const std::shared_ptr<DatabaseConnection>& db_conn)
{
  records.clear();
  clubs.clear();
  managed_results.clear();
  const DatabaseConnection& db = *db_conn;
  forEachRow(db,
             "SELECT team_id, recruitment, project, project_start_day, "
             "project_done_day, project_target, last_request_day, played, "
             "won, drawn, lost, goals_for, goals_against FROM YouthAcademies;",
             [&](sqlite3_stmt* stmt)
             {
               AcademyClub academy;
               academy.team_id = columnAs<TeamID>(stmt, 0);
               academy.recruitment = static_cast<std::uint8_t>(
                   std::clamp(columnAs<int>(stmt, 1), 1, 100));
               const auto project = columnAs<int>(stmt, 2);
               academy.project =
                   project >= 0 &&
                           project <= static_cast<int>(AcademyUpgrade::Recruitment)
                       ? static_cast<AcademyUpgrade>(project)
                       : AcademyUpgrade::None;
               academy.project_start_day = columnAs<std::int32_t>(stmt, 3);
               academy.project_done_day = columnAs<std::int32_t>(stmt, 4);
               academy.project_target = columnAs<std::uint8_t>(stmt, 5);
               academy.last_request_day = columnAs<std::int32_t>(stmt, 6);
               academy.table.team_id = academy.team_id;
               academy.table.played = columnAs<std::uint16_t>(stmt, 7);
               academy.table.won = columnAs<std::uint16_t>(stmt, 8);
               academy.table.drawn = columnAs<std::uint16_t>(stmt, 9);
               academy.table.lost = columnAs<std::uint16_t>(stmt, 10);
               academy.table.goals_for = columnAs<std::uint16_t>(stmt, 11);
               academy.table.goals_against = columnAs<std::uint16_t>(stmt, 12);
               clubs.emplace(academy.team_id, academy);
             });
  forEachRow(
      db,
      "SELECT player_id, team_id, status, contract, joined_age, appearances, "
      "minutes, goals, rating_total, progress FROM YouthPlayers;",
      [&](sqlite3_stmt* stmt)
      {
        YouthRecord youth;
        youth.player_id = columnAs<PlayerID>(stmt, 0);
        youth.team_id = columnAs<TeamID>(stmt, 1);
        const auto status = columnAs<int>(stmt, 2);
        const auto contract = columnAs<int>(stmt, 3);
        if (status < 0 || status > static_cast<int>(YouthStatus::Graduated) ||
            contract < 0 ||
            contract > static_cast<int>(YouthContract::Professional))
          return;
        youth.status = static_cast<YouthStatus>(status);
        youth.contract = static_cast<YouthContract>(contract);
        youth.joined_age = columnAs<std::uint8_t>(stmt, 4);
        youth.appearances = columnAs<std::uint16_t>(stmt, 5);
        youth.minutes = columnAs<std::uint16_t>(stmt, 6);
        youth.goals = columnAs<std::uint16_t>(stmt, 7);
        youth.rating_total = static_cast<float>(sqlite3_column_double(stmt, 8));
        if (const unsigned char* text = sqlite3_column_text(stmt, 9))
          youth.progress = decodeProgress(reinterpret_cast<const char*>(text));
        records.emplace(youth.player_id, std::move(youth));
      });
  forEachRow(db,
             "SELECT date, opponent_id, home, goals_for, goals_against FROM "
             "YouthResults ORDER BY seq;",
             [&](sqlite3_stmt* stmt)
             {
               YouthResult result;
               result.date = dateFromInt(columnAs<std::int32_t>(stmt, 0));
               result.opponent_id = columnAs<TeamID>(stmt, 1);
               result.home = columnAs<int>(stmt, 2) != 0;
               result.goals_for = columnAs<std::uint8_t>(stmt, 3);
               result.goals_against = columnAs<std::uint8_t>(stmt, 4);
               managed_results.push_back(result);
             });
  // Saves from before the academy (or written before it was set up) get
  // their U18 squads from the players' ages.
  ready = !clubs.empty();
  if (ready)
    syncFlags();
  else
    bootstrap();
}

void YouthAcademy::save(const std::shared_ptr<DatabaseConnection>& db_conn) const
{
  const DatabaseConnection& db = *db_conn;
  for (const char* table : {"YouthAcademies", "YouthPlayers", "YouthResults"})
  {
    sqlite3_exec(db.getRaw(), (std::string("DELETE FROM ") + table + ";").c_str(),
                 nullptr, nullptr, nullptr);
  }
  if (!ready) return;
  insertAll(db,
            "INSERT INTO YouthAcademies (team_id, recruitment, project, "
            "project_start_day, project_done_day, project_target, "
            "last_request_day, played, won, drawn, lost, goals_for, "
            "goals_against) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            clubs,
            [](sqlite3_stmt* row, const auto& entry)
            {
              const AcademyClub& academy = entry.second;
              sqlite3_bind_int(row, 1, static_cast<int>(academy.team_id));
              sqlite3_bind_int(row, 2, academy.recruitment);
              sqlite3_bind_int(row, 3, static_cast<int>(academy.project));
              sqlite3_bind_int(row, 4, academy.project_start_day);
              sqlite3_bind_int(row, 5, academy.project_done_day);
              sqlite3_bind_int(row, 6, academy.project_target);
              sqlite3_bind_int(row, 7, academy.last_request_day);
              sqlite3_bind_int(row, 8, academy.table.played);
              sqlite3_bind_int(row, 9, academy.table.won);
              sqlite3_bind_int(row, 10, academy.table.drawn);
              sqlite3_bind_int(row, 11, academy.table.lost);
              sqlite3_bind_int(row, 12, academy.table.goals_for);
              sqlite3_bind_int(row, 13, academy.table.goals_against);
            });
  insertAll(db,
            "INSERT INTO YouthPlayers (player_id, team_id, status, contract, "
            "joined_age, appearances, minutes, goals, rating_total, progress) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            records,
            [](sqlite3_stmt* row, const auto& entry)
            {
              const YouthRecord& youth = entry.second;
              sqlite3_bind_int(row, 1, static_cast<int>(youth.player_id));
              sqlite3_bind_int(row, 2, static_cast<int>(youth.team_id));
              sqlite3_bind_int(row, 3, static_cast<int>(youth.status));
              sqlite3_bind_int(row, 4, static_cast<int>(youth.contract));
              sqlite3_bind_int(row, 5, youth.joined_age);
              sqlite3_bind_int(row, 6, youth.appearances);
              sqlite3_bind_int(row, 7, youth.minutes);
              sqlite3_bind_int(row, 8, youth.goals);
              sqlite3_bind_double(row, 9, youth.rating_total);
              const std::string progress = encodeProgress(youth.progress);
              sqlite3_bind_text(row, 10, progress.c_str(), -1, SQLITE_TRANSIENT);
            });
  int seq = 0;
  insertAll(db,
            "INSERT INTO YouthResults (seq, date, opponent_id, home, "
            "goals_for, goals_against) VALUES (?, ?, ?, ?, ?, ?);",
            managed_results,
            [&seq](sqlite3_stmt* row, const YouthResult& result)
            {
              sqlite3_bind_int(row, 1, seq++);
              sqlite3_bind_int(row, 2, dateToInt(result.date));
              sqlite3_bind_int(row, 3, static_cast<int>(result.opponent_id));
              sqlite3_bind_int(row, 4, result.home ? 1 : 0);
              sqlite3_bind_int(row, 5, result.goals_for);
              sqlite3_bind_int(row, 6, result.goals_against);
            });
}
