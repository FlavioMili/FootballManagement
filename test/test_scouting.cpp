// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "global/logger.h"
#include "global/runtime_paths.h"
#include "model/scouting.h"

namespace
{
constexpr std::uint64_t WORLD_SEED = 20250702;
constexpr LeagueID OWN_LEAGUE = 1;      // Italian League
constexpr LeagueID LOWER_LEAGUE = 6;    // Italian Lower League (same country)
constexpr LeagueID FOREIGN_LEAGUE = 3;  // English League

int uniqueSlot(int offset)
{
  return 300'000 + static_cast<int>(getpid() % 100'000) * 10 + offset;
}

struct SlotCleanup
{
  int slot;
  ~SlotCleanup() { RuntimePaths::removeSave(slot); }
};

TeamID firstTeamOf(const GameController& controller, LeagueID league_id)
{
  auto ids = controller.getLeagueById(league_id)->get().getTeamIDs();
  std::ranges::sort(ids);
  return ids.front();
}

/** Career at the most reputable club of the own league (several scouts). */
std::unique_ptr<GameController> makeCareer(int slot)
{
  Logger::init();
  auto controller = std::make_unique<GameController>();
  controller->newGame(slot, WORLD_SEED);
  TeamID best = firstTeamOf(*controller, OWN_LEAGUE);
  for (const TeamID id :
       controller->getLeagueById(OWN_LEAGUE)->get().getTeamIDs())
  {
    if (controller->getTeamById(id)->get().getReputation() >
        controller->getTeamById(best)->get().getReputation())
      best = id;
  }
  controller->selectManagedTeam(best);
  return controller;
}

std::vector<PlayerID> playersOf(const GameController& controller,
                                TeamID team_id)
{
  std::vector<PlayerID> ids;
  for (const Player& player : controller.getPlayersForTeam(team_id))
    ids.push_back(player.getId());
  std::ranges::sort(ids);
  return ids;
}

std::vector<PlayerID> leaguePlayers(const GameController& controller,
                                    LeagueID league_id)
{
  std::vector<PlayerID> ids;
  for (const TeamID team_id :
       controller.getLeagueById(league_id)->get().getTeamIDs())
  {
    const auto team = playersOf(controller, team_id);
    ids.insert(ids.end(), team.begin(), team.end());
  }
  std::ranges::sort(ids);
  return ids;
}

const Player& playerOf(const GameController& controller, PlayerID id)
{
  return controller.getGameData()->getPlayer(id)->get();
}

double trueOverall(const GameController& controller, PlayerID id)
{
  return playerOf(controller, id).getOverall(controller.getStatsConfig());
}

ScoutingSystem& scoutingOf(GameController& controller)
{
  return const_cast<Game*>(controller.getGame())->getWorld().getScouting();
}

bool inboxHas(const GameController& controller, const char* title_key,
              PlayerID player_id)
{
  return std::ranges::any_of(controller.getInbox(),
                             [&](const InboxMessage& message)
                             {
                               return message.title_key == title_key &&
                                      message.player_id == player_id;
                             });
}
}  // namespace

TEST(ScoutingTest, BaselineKnowledgeFollowsTheRelationToTheClub)
{
  const SlotCleanup slot{uniqueSlot(0)};
  auto controller = makeCareer(slot.slot);
  const TeamID own = controller->getManagedTeam()->get().getId();
  TeamID rival = 0;
  for (const TeamID id :
       controller->getLeagueById(OWN_LEAGUE)->get().getTeamIDs())
  {
    if (id != own) rival = id;
  }

  EXPECT_FLOAT_EQ(
      controller->getScoutingKnowledge(playersOf(*controller, own)[0]),
      ScoutingTuning::OWN_KNOWLEDGE);
  EXPECT_FLOAT_EQ(
      controller->getScoutingKnowledge(playersOf(*controller, rival)[0]),
      ScoutingTuning::SAME_LEAGUE_KNOWLEDGE);
  EXPECT_FLOAT_EQ(controller->getScoutingKnowledge(
                      leaguePlayers(*controller, LOWER_LEAGUE)[0]),
                  ScoutingTuning::SAME_COUNTRY_KNOWLEDGE);
  EXPECT_FLOAT_EQ(controller->getScoutingKnowledge(
                      leaguePlayers(*controller, FOREIGN_LEAGUE)[0]),
                  ScoutingTuning::FOREIGN_KNOWLEDGE);
  EXPECT_GE(controller->getScouts().size(), 2U);
  for (const ScoutProfile& scout : controller->getScouts())
  {
    EXPECT_FALSE(scout.name.empty());
    EXPECT_GE(scout.judging_ability, 1);
    EXPECT_LE(scout.judging_ability, 100);
    EXPECT_GE(scout.adaptability, 30);
    EXPECT_LE(scout.adaptability, 90);
  }
}

TEST(ScoutingTest, FacingAnOpponentTeachesAboutHisPlayers)
{
  const SlotCleanup slot{uniqueSlot(7)};
  auto controller = makeCareer(slot.slot);
  const TeamID own = controller->getManagedTeam()->get().getId();
  controller->advanceToNextManagedFixture();
  const GameDateValue today = controller->getCurrentDate();
  for (const Match& match :
       controller->getGame()->getCalendar().getMatchesForDate(today))
  {
    if (match.getHomeTeamId() != own && match.getAwayTeamId() != own) continue;
    const TeamID opponent = match.getHomeTeamId() == own
                                ? match.getAwayTeamId()
                                : match.getHomeTeamId();
    const PlayerID rival = playersOf(*controller, opponent).front();
    const float before = controller->getScoutingKnowledge(rival);
    ASSERT_TRUE(controller->setMatchResult(today, match.getHomeTeamId(),
                                           match.getAwayTeamId(), 1, 1));
    EXPECT_GT(controller->getScoutingKnowledge(rival), before + 1.0f);
    return;
  }
  FAIL() << "No managed fixture found";
}

// Hidden-value safety: the API the screens use returns estimates for other
// clubs' players and exact values only for the managed squad.
TEST(ScoutingTest, UiApiReturnsEstimatesNotTrueValues)
{
  const SlotCleanup slot{uniqueSlot(1)};
  auto controller = makeCareer(slot.slot);
  const auto foreign = leaguePlayers(*controller, FOREIGN_LEAGUE);

  int differing = 0;
  int attribute_differing = 0;
  for (const PlayerID id : foreign)
  {
    const auto view = controller->getScoutedView(id);
    ASSERT_TRUE(view.has_value());
    EXPECT_FALSE(view->own);
    EXPECT_EQ(view->knowledge, 5);
    if (std::abs(view->overall - trueOverall(*controller, id)) > 0.5)
      ++differing;
    const auto& stats = playerOf(*controller, id).getStats();
    ASSERT_EQ(view->attributes.size(), stats.size());
    for (const ScoutedAttribute& attribute : view->attributes)
    {
      if (std::abs(attribute.estimate - stats.at(attribute.name)) > 0.5f)
        ++attribute_differing;
      EXPECT_LE(attribute.low, attribute.estimate);
      EXPECT_GE(attribute.high, attribute.estimate);
    }
    EXPECT_LE(view->potential_low, view->potential_high);
    EXPECT_GE(view->potential_low, view->overall - 1e-3f);
    // Displayed values never flicker between calls.
    const auto again = controller->getScoutedView(id);
    EXPECT_FLOAT_EQ(again->overall, view->overall);
    EXPECT_FLOAT_EQ(again->potential_low, view->potential_low);
    EXPECT_FLOAT_EQ(again->potential_high, view->potential_high);
  }
  EXPECT_GT(differing, static_cast<int>(foreign.size() * 3 / 4));
  EXPECT_GT(attribute_differing,
            static_cast<int>(
                foreign.size() *
                playerOf(*controller, foreign[0]).getStats().size() * 3 / 4));

  // The search sorts by the same estimates the profile shows.
  ScoutSearchFilter filter;
  filter.league_id = FOREIGN_LEAGUE;
  filter.limit = 0;
  const auto rows = controller->searchScoutedPlayers(filter);
  ASSERT_EQ(rows.size(), foreign.size());
  bool true_order_differs = false;
  for (std::size_t index = 0; index < rows.size(); ++index)
  {
    EXPECT_FLOAT_EQ(rows[index].overall,
                    controller->getScoutedView(rows[index].player_id)->overall);
    if (index == 0) continue;
    EXPECT_GE(rows[index - 1].overall, rows[index].overall);
    if (trueOverall(*controller, rows[index - 1].player_id) <
        trueOverall(*controller, rows[index].player_id))
      true_order_differs = true;
  }
  EXPECT_TRUE(true_order_differs);

  // Own players are known exactly.
  const TeamID own = controller->getManagedTeam()->get().getId();
  for (const PlayerID id : playersOf(*controller, own))
  {
    const auto view = controller->getScoutedView(id);
    EXPECT_TRUE(view->own);
    EXPECT_NEAR(view->overall, trueOverall(*controller, id), 1e-3);
    for (const ScoutedAttribute& attribute : view->attributes)
      EXPECT_FLOAT_EQ(attribute.estimate,
                      playerOf(*controller, id).getStats().at(attribute.name));
  }
  // The profile's potential range comes from the same model.
  const auto potential = controller->getPotentialEstimate(foreign[0]);
  EXPECT_FLOAT_EQ(potential.low,
                  controller->getScoutedView(foreign[0])->potential_low);
}

TEST(ScoutingTest, EstimatesConvergeWithKnowledgeAndDecayBack)
{
  const SlotCleanup slot{uniqueSlot(2)};
  auto controller = makeCareer(slot.slot);
  ScoutingSystem& scouting = scoutingOf(*controller);
  const auto foreign = leaguePlayers(*controller, FOREIGN_LEAGUE);

  const auto attributeError = [&]
  {
    double total = 0.0;
    for (const PlayerID id : foreign)
    {
      const auto& stats = playerOf(*controller, id).getStats();
      for (const ScoutedAttribute& attribute : scouting.view(id)->attributes)
        total += std::abs(attribute.estimate - stats.at(attribute.name));
    }
    return total;
  };
  const auto overallError = [&]
  {
    double total = 0.0;
    for (const PlayerID id : foreign)
      total +=
          std::abs(scouting.view(id)->overall - trueOverall(*controller, id));
    return total;
  };
  const auto potentialWidth = [&]
  {
    double total = 0.0;
    for (const PlayerID id : foreign)
    {
      const auto view = *scouting.view(id);
      total += view.potential_high - view.potential_low;
    }
    return total;
  };

  const double overall_before = overallError();
  const double width_before = potentialWidth();
  double previous = attributeError();
  // With more observation the estimates move steadily towards the truth.
  for (int step = 0; step < 20; ++step)
  {
    for (const PlayerID id : foreign) scouting.observe(id, 10.0f, nullptr);
    const double current = attributeError();
    EXPECT_LE(current, previous + 1e-6);
    previous = current;
  }
  for (const PlayerID id : foreign) EXPECT_GE(scouting.view(id)->knowledge, 80);
  EXPECT_LT(overallError(), overall_before * 0.5);
  EXPECT_LT(potentialWidth(), width_before);

  // Knowledge fades slowly back to the baseline without scouting.
  const PlayerID id = foreign.front();
  const float known = scouting.knowledgeOf(id);
  Inbox inbox;
  GameDateValue date = controller->getCurrentDate();
  for (int day = 0; day < 100; ++day)
  {
    date.nextDay();
    scouting.onDayAdvanced(date, inbox);
  }
  EXPECT_NEAR(scouting.knowledgeOf(id),
              known - 100.0f * ScoutingTuning::DAILY_DECAY, 0.01f);
  for (int day = 0; day < 2000; ++day)
  {
    date.nextDay();
    scouting.onDayAdvanced(date, inbox);
  }
  EXPECT_FLOAT_EQ(scouting.knowledgeOf(id), ScoutingTuning::FOREIGN_KNOWLEDGE);
}

TEST(ScoutingTest, PlayerAssignmentCostsMoneyAndFilesAGradedReport)
{
  const SlotCleanup slot{uniqueSlot(3)};
  auto controller = makeCareer(slot.slot);
  const Team& team = controller->getManagedTeam()->get();
  const PlayerID target = leaguePlayers(*controller, FOREIGN_LEAGUE)[3];
  const ScoutProfile scout = controller->getScouts().front();

  const int64_t cost =
      controller->getScoutAssignmentCost(ScoutTargetKind::Player, target, 10);
  EXPECT_EQ(cost, 10 * ScoutingTuning::DAILY_COST_FOREIGN);
  const int64_t balance = team.getFinances().getBalance();
  ASSERT_EQ(controller->startScoutAssignment(scout.id, ScoutTargetKind::Player,
                                             target, 10),
            ScoutAssignError::None);
  EXPECT_EQ(team.getFinances().getBalance(), balance - cost);
  EXPECT_EQ(team.getFinances().getLedger().back().category,
            FinanceCategory::Staff);
  EXPECT_EQ(team.getFinances().getLedger().back().amount, -cost);

  // Validation.
  EXPECT_EQ(controller->startScoutAssignment(scout.id, ScoutTargetKind::League,
                                             FOREIGN_LEAGUE, 10),
            ScoutAssignError::ScoutBusy);
  const std::uint32_t other = controller->getScouts()[1].id;
  EXPECT_EQ(controller->startScoutAssignment(other, ScoutTargetKind::League,
                                             FOREIGN_LEAGUE, 1),
            ScoutAssignError::InvalidDuration);
  EXPECT_EQ(controller->startScoutAssignment(
                other, ScoutTargetKind::Player,
                playersOf(*controller, team.getId())[0], 10),
            ScoutAssignError::InvalidTarget);
  EXPECT_EQ(controller->startScoutAssignment(other, ScoutTargetKind::Country,
                                             LOWER_LEAGUE, 10),
            ScoutAssignError::InvalidTarget);
  EXPECT_EQ(controller->startScoutAssignment(
                0xFFFFFFF0U, ScoutTargetKind::FreeAgents, 0, 10),
            ScoutAssignError::UnknownScout);

  const float knowledge_before = controller->getScoutingKnowledge(target);
  for (int day = 0; day < 10; ++day) controller->advanceDay();
  EXPECT_GT(controller->getScoutingKnowledge(target), knowledge_before + 25.0f);
  ASSERT_FALSE(controller->getScoutReports().empty());
  const ScoutReport& report = controller->getScoutReports().back();
  EXPECT_EQ(report.player_id, target);
  EXPECT_EQ(report.scout_name, scout.name);
  EXPECT_EQ(report.date, controller->getCurrentDate());
  EXPECT_GT(report.confidence, 0);
  EXPECT_LE(report.potential_low, report.potential_high);
  const int reasons = static_cast<int>(report.fits_need) +
                      static_cast<int>(report.affordable) +
                      static_cast<int>(report.available);
  EXPECT_EQ(report.grade, reasons == 3   ? ScoutGrade::A
                          : reasons == 2 ? ScoutGrade::B
                                         : ScoutGrade::C);
  EXPECT_TRUE(inboxHas(*controller, "SCOUT_MSG_REPORT_TITLE", target));
  EXPECT_EQ(controller->getScoutedView(target)->latest_report_id, report.id);
  EXPECT_EQ(scoutingOf(*controller).activeAssignment(scout.id), nullptr);
}

TEST(ScoutingTest, CoverageAssignmentFollowsTheRecruitmentFocus)
{
  const SlotCleanup slot{uniqueSlot(4)};
  auto controller = makeCareer(slot.slot);
  RecruitmentFocus focus;
  focus.role = PlayerRole::ST;
  focus.min_age = 17;
  focus.max_age = 34;
  const std::uint32_t focus_id = controller->saveRecruitmentFocus(focus);
  ASSERT_NE(focus_id, 0U);
  focus.id = focus_id;
  focus.max_age = 30;
  EXPECT_EQ(controller->saveRecruitmentFocus(focus), focus_id);
  ASSERT_EQ(controller->getRecruitmentFocuses().size(), 1U);
  EXPECT_EQ(controller->getRecruitmentFocuses()[0].max_age, 30);

  const ScoutProfile scout = controller->getScouts().front();
  ASSERT_EQ(controller->startScoutAssignment(scout.id, ScoutTargetKind::League,
                                             FOREIGN_LEAGUE, 30),
            ScoutAssignError::None);
  for (int day = 0; day < 30; ++day) controller->advanceDay();

  const ScoutAssignment& assignment = controller->getScoutAssignments().back();
  EXPECT_TRUE(assignment.finished);
  EXPECT_EQ(assignment.players_observed,
            30 * ScoutingTuning::COVERAGE_PLAYERS_PER_DAY);
  EXPECT_GT(assignment.reports_filed, 0);
  int strikers = 0;
  for (const ScoutReport& report : controller->getScoutReports())
  {
    const Player& player = playerOf(*controller, report.player_id);
    EXPECT_EQ(player.getRole(), PlayerRole::ST);
    EXPECT_LE(player.getAge(), 30);
    ++strikers;
  }
  EXPECT_EQ(strikers, assignment.reports_filed);
  EXPECT_TRUE(std::ranges::any_of(
      controller->getInbox(), [](const InboxMessage& message)
      { return message.title_key == "SCOUT_MSG_DONE_TITLE"; }));

  ScoutSearchFilter filter;
  filter.focus_matches_only = true;
  for (const ScoutedPlayerRow& row : controller->searchScoutedPlayers(filter))
  {
    EXPECT_EQ(row.role, PlayerRole::ST);
    EXPECT_TRUE(row.matches_focus);
  }
  EXPECT_TRUE(controller->removeRecruitmentFocus(focus_id));
  EXPECT_TRUE(controller->getRecruitmentFocuses().empty());
}

TEST(ScoutingTest, ShortlistRaisesAlertsOnStatusChanges)
{
  const SlotCleanup slot{uniqueSlot(5)};
  auto controller = makeCareer(slot.slot);
  const auto foreign = leaguePlayers(*controller, FOREIGN_LEAGUE);
  PlayerID listed_id = 0;
  PlayerID injured_id = 0;
  auto& players = controller->getGameData()->getPlayers();
  for (const PlayerID id : foreign)
  {
    const Player& player = players.at(id);
    if (player.getTransferStatus() == TransferStatus::Listed ||
        player.getDynamics().injury_days > 0 || player.getContractYears() <= 1)
      continue;
    if (listed_id == 0)
      listed_id = id;
    else if (injured_id == 0)
      injured_id = id;
  }
  ASSERT_NE(injured_id, 0U);
  EXPECT_TRUE(controller->addToShortlist(listed_id));
  EXPECT_TRUE(controller->addToShortlist(injured_id));
  EXPECT_FALSE(controller->addToShortlist(listed_id));
  EXPECT_FALSE(controller->addToShortlist(
      playersOf(*controller, controller->getManagedTeam()->get().getId())[0]));
  EXPECT_TRUE(controller->isShortlisted(listed_id));

  players.at(listed_id).setTransferStatus(TransferStatus::Listed);
  players.at(injured_id).mutableDynamics().injury_days = 30;
  controller->advanceDay();
  EXPECT_TRUE(inboxHas(*controller, "SCOUT_ALERT_LISTED_TITLE", listed_id));
  EXPECT_TRUE(inboxHas(*controller, "SCOUT_ALERT_INJURED_TITLE", injured_id));
  const auto alerts = std::ranges::count_if(
      controller->getInbox(), [](const InboxMessage& message)
      { return message.title_key.starts_with("SCOUT_ALERT_"); });
  controller->advanceDay();
  EXPECT_EQ(std::ranges::count_if(
                controller->getInbox(), [](const InboxMessage& message)
                { return message.title_key.starts_with("SCOUT_ALERT_"); }),
            alerts)
      << "An unchanged status must not alert again";

  const auto comparison = controller->compareShortlistWithSquad();
  ASSERT_EQ(comparison.size(), ScoutingSystem::ROLE_COUNT);
  const auto role = static_cast<std::size_t>(players.at(listed_id).getRole());
  EXPECT_EQ(comparison[role].role, players.at(listed_id).getRole());
  EXPECT_NE(comparison[role].candidate_id, 0U);
  EXPECT_TRUE(controller->removeFromShortlist(listed_id));
  EXPECT_FALSE(controller->isShortlisted(listed_id));
}

TEST(ScoutingTest, StateSurvivesSaveAndLoad)
{
  const SlotCleanup slot{uniqueSlot(6)};
  auto controller = makeCareer(slot.slot);
  const auto foreign = leaguePlayers(*controller, FOREIGN_LEAGUE);
  RecruitmentFocus focus;
  focus.role = PlayerRole::CB;
  focus.max_fee = 5'000'000;
  focus.min_ability = 55;
  controller->saveRecruitmentFocus(focus);
  controller->addToShortlist(foreign[5]);
  const std::uint32_t scout_id = controller->getScouts().front().id;
  ASSERT_EQ(controller->startScoutAssignment(scout_id, ScoutTargetKind::Player,
                                             foreign[7], 5),
            ScoutAssignError::None);
  ASSERT_EQ(controller->startScoutAssignment(controller->getScouts()[1].id,
                                             ScoutTargetKind::League,
                                             FOREIGN_LEAGUE, 20),
            ScoutAssignError::None);
  for (int day = 0; day < 6; ++day) controller->advanceDay();
  ASSERT_FALSE(controller->getScoutReports().empty());

  const float knowledge = controller->getScoutingKnowledge(foreign[7]);
  const auto view = *controller->getScoutedView(foreign[7]);
  const auto reports = controller->getScoutReports();
  const auto assignments = controller->getScoutAssignments();
  controller->saveGame();

  auto reloaded = std::make_unique<GameController>();
  ASSERT_TRUE(reloaded->loadGame(slot.slot));
  EXPECT_FLOAT_EQ(reloaded->getScoutingKnowledge(foreign[7]), knowledge);
  const auto view_after = *reloaded->getScoutedView(foreign[7]);
  EXPECT_FLOAT_EQ(view_after.overall, view.overall);
  EXPECT_FLOAT_EQ(view_after.potential_low, view.potential_low);
  ASSERT_EQ(reloaded->getScoutReports().size(), reports.size());
  for (std::size_t index = 0; index < reports.size(); ++index)
  {
    const ScoutReport& a = reports[index];
    const ScoutReport& b = reloaded->getScoutReports()[index];
    EXPECT_EQ(a.id, b.id);
    EXPECT_EQ(a.date, b.date);
    EXPECT_EQ(a.player_id, b.player_id);
    EXPECT_EQ(a.assignment_id, b.assignment_id);
    EXPECT_EQ(a.scout_name, b.scout_name);
    EXPECT_EQ(a.grade, b.grade);
    EXPECT_EQ(a.fits_need, b.fits_need);
    EXPECT_EQ(a.affordable, b.affordable);
    EXPECT_EQ(a.available, b.available);
    EXPECT_FLOAT_EQ(a.overall, b.overall);
    EXPECT_EQ(a.estimated_fee, b.estimated_fee);
  }
  ASSERT_EQ(reloaded->getScoutAssignments().size(), assignments.size());
  for (std::size_t index = 0; index < assignments.size(); ++index)
  {
    const ScoutAssignment& a = assignments[index];
    const ScoutAssignment& b = reloaded->getScoutAssignments()[index];
    EXPECT_EQ(a.id, b.id);
    EXPECT_EQ(a.kind, b.kind);
    EXPECT_EQ(a.target_id, b.target_id);
    EXPECT_EQ(a.days_done, b.days_done);
    EXPECT_EQ(a.finished, b.finished);
    EXPECT_EQ(a.cost, b.cost);
  }
  ASSERT_EQ(reloaded->getRecruitmentFocuses().size(), 1U);
  EXPECT_EQ(reloaded->getRecruitmentFocuses()[0].role, PlayerRole::CB);
  EXPECT_EQ(reloaded->getRecruitmentFocuses()[0].max_fee, 5'000'000);
  EXPECT_EQ(reloaded->getRecruitmentFocuses()[0].min_ability, 55);
  EXPECT_TRUE(reloaded->isShortlisted(foreign[5]));

  // Knowledge keeps fading identically after the reload.
  controller->advanceDay();
  reloaded->advanceDay();
  EXPECT_FLOAT_EQ(reloaded->getScoutingKnowledge(foreign[7]),
                  controller->getScoutingKnowledge(foreign[7]));
}

TEST(ScoutingTest, UnscoutedEstimatesAreRealisticAndRangesAreHonest)
{
  const SlotCleanup slot{uniqueSlot(8)};
  auto controller = makeCareer(slot.slot);
  const ScoutingSystem& scouting = scoutingOf(*controller);
  const auto foreign = leaguePlayers(*controller, FOREIGN_LEAGUE);
  ASSERT_GT(foreign.size(), 100u);

  std::vector<double> errors;
  size_t inside = 0;
  for (const PlayerID id : foreign)
  {
    const auto row = *scouting.row(id);
    ASSERT_LT(row.knowledge, ScoutingTuning::RANGE_DISPLAY_KNOWLEDGE);
    EXPECT_LE(row.overall_low, row.overall);
    EXPECT_GE(row.overall_high, row.overall);
    EXPECT_NEAR(0.5f * (row.overall_low + row.overall_high), row.overall,
                0.5f)
        << "the estimate is the centre of its range";
    const double truth = trueOverall(*controller, id);
    errors.push_back(std::abs(row.overall - truth));
    if (truth >= row.overall_low - 0.5 && truth <= row.overall_high + 0.5)
      ++inside;
    if (row.age >= 30)
      EXPECT_LE(row.potential_high, std::max(row.overall, row.overall_high) +
                                        1e-3f)
          << "veterans have no headroom beyond their current ability";
    const auto view = *scouting.view(id);
    EXPECT_FLOAT_EQ(view.overall_low, row.overall_low);
    EXPECT_FLOAT_EQ(view.overall_high, row.overall_high);
  }
  std::ranges::sort(errors);
  // Barely known players: a few points off is normal, ten is not.
  const double p90 = errors[errors.size() * 9 / 10];
  EXPECT_LT(p90, 8.0);
  EXPECT_GT(p90, 1.0) << "without scouting the estimate is uncertain";
  // The 80% ranges should contain the truth most of the time.
  const double coverage =
      static_cast<double>(inside) / static_cast<double>(foreign.size());
  EXPECT_GT(coverage, 0.65);
}
