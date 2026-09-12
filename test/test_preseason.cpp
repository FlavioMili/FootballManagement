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
#include <map>
#include <memory>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/logger.h"
#include "global/runtime_paths.h"
#include "model/inbox.h"
#include "model/preseason.h"

namespace
{
constexpr std::uint64_t WORLD_SEED = 20250702;

int uniqueSlot(int offset)
{
  return 5'000'000 + static_cast<int>(getpid() % 100'000) * 10 + offset;
}

struct SlotCleanup
{
  int slot;
  ~SlotCleanup() { RuntimePaths::removeSave(slot); }
};

std::unique_ptr<GameController> makeCareer(int slot, size_t club_index = 0)
{
  Logger::init();
  auto controller = std::make_unique<GameController>();
  controller->newGame(slot, WORLD_SEED);
  controller->selectManagedTeam(
      controller->getTeams()[club_index].get().getId());
  return controller;
}

/** Every club plays at most once on @p date. */
bool oncePerClub(const GameController& controller, const GameDateValue& date)
{
  std::map<TeamID, int> seen;
  for (const Match& match :
       controller.getGame()->getCalendar().getMatchesForDate(date))
  {
    if (++seen[match.getHomeTeamId()] > 1) return false;
    if (++seen[match.getAwayTeamId()] > 1) return false;
  }
  return true;
}

std::vector<OpponentOption> anyOpponents(const GameController& controller,
                                         const GameDateValue& date,
                                         bool abroad)
{
  std::vector<OpponentOption> all;
  for (const OpponentLevel level :
       {OpponentLevel::Similar, OpponentLevel::Weaker, OpponentLevel::Stronger})
  {
    const auto options = controller.getFriendlyOpponents(date, level, abroad);
    all.insert(all.end(), options.begin(), options.end());
  }
  return all;
}
}  // namespace

TEST(Preseason, PricingRules)
{
  const GameDateValue first(2025, 7, 12);
  const CampQuote domestic =
      Preseason::campQuote(TrainingCamp::Domestic, 50'000'000.0, first);
  const CampQuote abroad =
      Preseason::campQuote(TrainingCamp::Abroad, 50'000'000.0, first);
  EXPECT_EQ(domestic.start, GameDateValue(2025, 7, 13));
  EXPECT_EQ(domestic.end, GameDateValue(2025, 7, 18));
  EXPECT_GT(abroad.cost, domestic.cost);
  EXPECT_GT(abroad.sharpness, domestic.sharpness);
  EXPECT_FALSE(Preseason::campQuote(TrainingCamp::None, 1.0, first).available);
  EXPECT_GT(Preseason::tourFee(50'000'000.0, 90),
            Preseason::tourFee(50'000'000.0, 40));
  EXPECT_TRUE(Preseason::levelMatches(OpponentLevel::Weaker, 60, 50));
  EXPECT_FALSE(Preseason::levelMatches(OpponentLevel::Weaker, 60, 55));
  EXPECT_TRUE(Preseason::levelMatches(OpponentLevel::Similar, 60, 55));
  EXPECT_TRUE(Preseason::levelMatches(OpponentLevel::Stronger, 60, 70));
}

TEST(Preseason, ChoosingAnOpponentSwapsFixturesSafely)
{
  const SlotCleanup slot{uniqueSlot(0)};
  auto controller = makeCareer(slot.slot);
  const TeamID managed = controller->getManagedTeam()->get().getId();
  const auto slots = controller->getPreseasonFriendlies();
  ASSERT_EQ(slots.size(), 4u);
  ASSERT_TRUE(slots.front().editable);
  const GameDateValue date = slots.front().date;
  const size_t matches_before =
      controller->getGame()->getCalendar().getMatchesForDate(date).size();
  const auto options = anyOpponents(*controller, date, false);
  const auto choice = std::ranges::find_if(
      options, [&](const OpponentOption& option)
      { return option.team_id != slots.front().opponent_id; });
  ASSERT_NE(choice, options.end());

  ASSERT_TRUE(
      controller->setPreseasonFriendly(date, choice->team_id, true, false));
  const auto after = controller->getPreseasonFriendlies();
  EXPECT_EQ(after.front().opponent_id, choice->team_id);
  EXPECT_TRUE(after.front().home);
  EXPECT_EQ(controller->getGame()->getCalendar().getMatchesForDate(date).size(),
            matches_before);
  EXPECT_TRUE(oncePerClub(*controller, date));
  // The venue can be switched with the same opponent.
  ASSERT_TRUE(
      controller->setPreseasonFriendly(date, choice->team_id, false, false));
  EXPECT_FALSE(controller->getPreseasonFriendlies().front().home);
  // Past dates and the managed club itself are refused.
  EXPECT_FALSE(controller->setPreseasonFriendly(
      controller->getCurrentDate(), choice->team_id, true, false));
  EXPECT_FALSE(controller->setPreseasonFriendly(date, managed, true, false));

  // The assistant's plan fills the editable dates without repeats.
  const auto plan = controller->getPreseasonSuggestion();
  ASSERT_EQ(plan.size(), 4u);
  std::vector<TeamID> opponents;
  for (const FriendlySuggestion& friendly : plan)
    opponents.push_back(friendly.opponent_id);
  std::ranges::sort(opponents);
  EXPECT_EQ(std::ranges::unique(opponents).begin(), opponents.end());
  const auto own = controller->getManagedTeam()->get().getReputation();
  const auto weaker = controller->getTeamById(plan[0].opponent_id);
  if (!controller->getFriendlyOpponents(plan[0].date, OpponentLevel::Weaker,
                                        false)
           .empty())
    EXPECT_LE(weaker->get().getReputation() + Preseason::LEVEL_GAP, own);
  EXPECT_EQ(controller->applyPreseasonSuggestion(), 4u);
  for (size_t index = 0; index < plan.size(); ++index)
  {
    EXPECT_EQ(controller->getPreseasonFriendlies()[index].opponent_id,
              plan[index].opponent_id);
    EXPECT_TRUE(oncePerClub(*controller, plan[index].date));
  }
}

TEST(Preseason, TourFeeCampAndRoundTrip)
{
  const SlotCleanup slot{uniqueSlot(1)};
  auto controller = makeCareer(slot.slot);
  const TeamID managed = controller->getManagedTeam()->get().getId();
  const auto slots = controller->getPreseasonFriendlies();
  ASSERT_GE(slots.size(), 2u);
  const GameDateValue tour_date = slots[1].date;
  const auto abroad = anyOpponents(*controller, tour_date, true);
  ASSERT_FALSE(abroad.empty());
  const TeamID host = abroad.front().team_id;
  ASSERT_TRUE(controller->setPreseasonFriendly(tour_date, host, false, true));
  EXPECT_TRUE(controller->getPreseasonFriendlies()[1].tour);
  // A tour needs an away match.
  const TeamID visitor = abroad[abroad.size() > 1 ? 1 : 0].team_id;
  ASSERT_TRUE(controller->setPreseasonFriendly(slots[2].date, visitor, true,
                                               true));
  EXPECT_FALSE(controller->getPreseasonFriendlies()[2].tour);

  // Camp: booking pays, cancelling refunds, booking again pays again.
  const int64_t start_balance =
      controller->getManagedTeam()->get().getFinances().getBalance();
  const CampQuote domestic = controller->getCampQuote(TrainingCamp::Domestic);
  ASSERT_TRUE(domestic.available);
  ASSERT_TRUE(controller->bookTrainingCamp(TrainingCamp::Domestic));
  EXPECT_EQ(controller->getManagedTeam()->get().getFinances().getBalance(),
            start_balance - domestic.cost);
  ASSERT_TRUE(controller->bookTrainingCamp(TrainingCamp::None));
  EXPECT_EQ(controller->getManagedTeam()->get().getFinances().getBalance(),
            start_balance);
  ASSERT_TRUE(controller->bookTrainingCamp(TrainingCamp::Domestic));
  EXPECT_EQ(controller->getPreseasonState().camp_end, domestic.end);

  ASSERT_TRUE(controller->saveGame());
  {
    GameController reloaded;
    ASSERT_TRUE(reloaded.loadGame(slot.slot));
    EXPECT_EQ(reloaded.getPreseasonState().camp, TrainingCamp::Domestic);
    EXPECT_EQ(reloaded.getPreseasonState().camp_end, domestic.end);
    EXPECT_EQ(reloaded.getPreseasonState().tour_dates.size(), 1u);
    EXPECT_EQ(reloaded.getPreseasonFriendlies()[1].opponent_id, host);
  }

  const int64_t expected_fee = controller->getTourFee(host);
  ASSERT_GT(expected_fee, 0);
  while (!(tour_date < controller->getCurrentDate())) controller->advanceDay();
  controller->advanceDay();
  EXPECT_TRUE(controller->getPreseasonState().camp_applied);
  EXPECT_TRUE(std::ranges::any_of(controller->getInbox(),
                                  [](const InboxMessage& message)
                                  { return message.title_key ==
                                           "INBOX_CAMP_DONE_TITLE"; }));
  const auto& ledger = controller->getFinanceLedger(managed);
  EXPECT_TRUE(std::ranges::any_of(
      ledger, [&](const FinanceTransaction& transaction)
      {
        return transaction.date == tour_date &&
               transaction.category == FinanceCategory::Matchday &&
               transaction.amount == expected_fee;
      }));
  EXPECT_EQ(controller->getPreseasonState().tour_matches, 1u);
}

TEST(Preseason, CampSharpensTheFitSquad)
{
  const SlotCleanup slot{uniqueSlot(2)};
  auto controller = makeCareer(slot.slot);
  ASSERT_TRUE(controller->bookTrainingCamp(TrainingCamp::Abroad));
  const CampQuote quote = controller->getCampQuote(TrainingCamp::Abroad);
  const TeamID managed = controller->getManagedTeam()->get().getId();
  auto gamedata = controller->getGameData();
  const PlayerID player_id =
      controller->getPlayersForTeam(managed).front().get().getId();
  Player& player = gamedata->getPlayers().at(player_id);
  player.mutableDynamics().sharpness = 50.0f;
  player.mutableDynamics().injury_days = 0;
  Game* game = const_cast<Game*>(controller->getGame());
  const float familiarity = gamedata->getTraining().plan(managed).familiarity;
  game->getWorld().getPreseason().onDay(*gamedata, quote.end, managed,
                                        game->getWorld().getInbox());
  EXPECT_FLOAT_EQ(player.getDynamics().sharpness, 50.0f + quote.sharpness);
  EXPECT_FLOAT_EQ(gamedata->getTraining().plan(managed).familiarity,
                  std::min(100.0f, familiarity + quote.familiarity));
  // Once only.
  game->getWorld().getPreseason().onDay(*gamedata, quote.end, managed,
                                        game->getWorld().getInbox());
  EXPECT_FLOAT_EQ(player.getDynamics().sharpness, 50.0f + quote.sharpness);
}
