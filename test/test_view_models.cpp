// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <unordered_set>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/logger.h"
#include "gui/view_models/competition_view.h"
#include "gui/view_models/formation.h"
#include "gui/view_models/player_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "model/game.h"
#include "model/lineup.h"
#include "model/settings_manager.h"

namespace
{
class ViewModelTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    Logger::init();
    controller = std::make_unique<GameController>();
    controller->newGame(97);
    ASSERT_FALSE(controller->getTeams().empty());
    team_id = controller->getTeams().front().get().getId();
    controller->selectManagedTeam(team_id);
  }

  std::unique_ptr<GameController> controller;
  TeamID team_id = 0;
};

const Match* firstLeagueFixture(const GameController& controller,
                                LeagueID leagueId)
{
  const auto league = controller.getLeagueById(leagueId);
  const auto& teams = league->get().getTeamIDs();
  for (const auto& [date, matches] :
       controller.getGame()->getCalendar().getFullCalendar())
  {
    for (const Match& match : matches)
    {
      if (match.getMatchType() == MatchType::LEAGUE &&
          std::ranges::find(teams, match.getHomeTeamId()) != teams.end())
        return &match;
    }
  }
  return nullptr;
}
}  // namespace

TEST(FormatTest, MoneyIsCompactAndSigned)
{
  EXPECT_EQ(Format::money(0), "€0");
  EXPECT_EQ(Format::money(950), "€950");
  EXPECT_EQ(Format::money(13'400), "€13K");
  EXPECT_EQ(Format::money(2'450'000), "€2.45M");
  EXPECT_EQ(Format::money(-31'000'000), "-€31.0M");
  EXPECT_EQ(Format::moneyFull(1'250'000), "€1,250,000");
  EXPECT_EQ(Format::moneyFull(-999), "-€999");
  EXPECT_EQ(Format::signedInt(4), "+4");
  EXPECT_EQ(Format::signedInt(-2), "-2");
}

TEST(FormationTest, RoleFitPrefersNaturalAndAdjacentRoles)
{
  EXPECT_FLOAT_EQ(Formation::roleFit(PlayerRole::CB, PlayerRole::CB), 1.0f);
  EXPECT_FLOAT_EQ(Formation::roleFit(PlayerRole::GK, PlayerRole::CB), 0.0f);
  EXPECT_FLOAT_EQ(Formation::roleFit(PlayerRole::ST, PlayerRole::GK), 0.0f);
  EXPECT_GT(Formation::roleFit(PlayerRole::LW, PlayerRole::LM),
            Formation::roleFit(PlayerRole::RW, PlayerRole::LM));
  EXPECT_GT(Formation::roleFit(PlayerRole::CM, PlayerRole::CDM),
            Formation::roleFit(PlayerRole::ST, PlayerRole::CDM));
}

TEST(FormationTest, PresetsHaveTenSlotsInsideThePitch)
{
  for (const Formation::Preset& preset : Formation::PRESETS)
  {
    for (const Formation::Slot& slot : preset.slots)
    {
      EXPECT_GT(slot.position.x, 0.0f) << preset.name;
      EXPECT_LT(slot.position.x, 1.0f) << preset.name;
      EXPECT_GT(slot.position.y, 0.0f) << preset.name;
      EXPECT_LT(slot.position.y, 1.0f) << preset.name;
      EXPECT_NE(slot.role, PlayerRole::GK) << preset.name;
    }
  }
}

TEST_F(ViewModelTest, AutoPickFillsElevenAndBenchesTheRest)
{
  Team& team = controller->getManagedTeam()->get();
  std::vector<const Player*> squad;
  for (const auto& player : controller->getPlayersForTeam(team_id))
    squad.push_back(&player.get());
  ASSERT_GE(squad.size(), 11u);

  for (size_t index = 0; index < Formation::PRESETS.size(); ++index)
  {
    Lineup& lineup = team.getLineup();
    Formation::autoPick(lineup, Formation::PRESETS[index], squad,
                        controller->getStatsConfig());
    ASSERT_NE(lineup.getGoalkeeper(), nullptr);
    EXPECT_EQ(lineup.getGoalkeeper()->getRole(), PlayerRole::GK);
    EXPECT_EQ(lineup.getOutfieldPlayers().size(), 10u);
    EXPECT_EQ(lineup.getReserves().size(), squad.size() - 11u);
    EXPECT_EQ(Formation::detectPreset(lineup), static_cast<int>(index));

    std::unordered_set<PlayerID> seen;
    seen.insert(lineup.getGoalkeeper()->getId());
    for (const auto& positioned : lineup.getOutfieldPlayers())
      EXPECT_TRUE(seen.insert(positioned.player->getId()).second);
    for (const Player* reserve : lineup.getReserves())
      EXPECT_TRUE(seen.insert(reserve->getId()).second);
    EXPECT_EQ(seen.size(), squad.size());
  }
}

TEST_F(ViewModelTest, ApplyPresetKeepsTheSameEleven)
{
  Lineup& lineup = controller->getManagedTeam()->get().getLineup();
  std::unordered_set<PlayerID> before;
  for (const auto& positioned : lineup.getOutfieldPlayers())
    before.insert(positioned.player->getId());
  const Player* goalkeeper = lineup.getGoalkeeper();
  const size_t reserves = lineup.getReserves().size();

  Formation::applyPreset(lineup, Formation::PRESETS[3],
                         controller->getStatsConfig());
  EXPECT_EQ(lineup.getGoalkeeper(), goalkeeper);
  EXPECT_EQ(lineup.getReserves().size(), reserves);
  std::unordered_set<PlayerID> after;
  for (const auto& positioned : lineup.getOutfieldPlayers())
    after.insert(positioned.player->getId());
  EXPECT_EQ(before, after);
  EXPECT_EQ(Formation::detectPreset(lineup), 3);
}

TEST_F(ViewModelTest, StandingsReflectPlayedResults)
{
  const LeagueID leagueId = controller->getManagedTeam()->get().getLeagueId();
  const auto initial = CompetitionView::buildStandings(*controller, leagueId);
  ASSERT_EQ(initial.size(),
            controller->getLeagueById(leagueId)->get().getTeamIDs().size());
  EXPECT_TRUE(std::ranges::all_of(
      initial, [](const auto& row) { return row.played == 0; }));

  const Match* fixture = firstLeagueFixture(*controller, leagueId);
  ASSERT_NE(fixture, nullptr);
  const TeamID home = fixture->getHomeTeamId();
  const TeamID away = fixture->getAwayTeamId();
  ASSERT_TRUE(controller->setMatchResult(fixture->getDate(), home, away, 3, 1));

  const auto table = CompetitionView::buildStandings(*controller, leagueId);
  ASSERT_EQ(table.front().team_id, home);
  EXPECT_EQ(table.front().points, 3);
  EXPECT_EQ(table.front().goalDifference(), 2);
  EXPECT_EQ(table.front().form_count, 1);
  EXPECT_EQ(table.front().form[0], UI::Outcome::WIN);
  EXPECT_EQ(table.back().team_id, away);
  EXPECT_EQ(table.back().lost, 1);
  EXPECT_EQ(table.back().goals_against, 3);
  EXPECT_TRUE(std::ranges::is_sorted(table,
                                     [](const auto& left, const auto& right)
                                     { return left.points > right.points; }));

  const auto fixtures =
      CompetitionView::buildLeagueFixtures(*controller, leagueId);
  ASSERT_FALSE(fixtures.empty());
  EXPECT_EQ(fixtures.front().round, 1);
  EXPECT_TRUE(std::ranges::is_sorted(fixtures, {},
                                     &CompetitionView::FixtureRow::round));
  const auto clubFixtures =
      CompetitionView::buildClubFixtures(*controller, home);
  ASSERT_FALSE(clubFixtures.empty());
  EXPECT_TRUE(std::ranges::all_of(
      clubFixtures, [home](const auto& row)
      { return row.home_id == home || row.away_id == home; }));
  const auto played =
      std::ranges::find_if(clubFixtures, [&](const auto& row)
                           { return row.played && row.away_id == away; });
  ASSERT_NE(played, clubFixtures.end());
  EXPECT_EQ(CompetitionView::outcomeFor(*played, home), UI::Outcome::WIN);
  EXPECT_EQ(CompetitionView::outcomeFor(*played, away), UI::Outcome::LOSS);
}

TEST_F(ViewModelTest, RoleFitsAreSortedAndCoverEveryGroup)
{
  const Player& player = controller->getPlayersForTeam(team_id).front().get();
  const auto fits = PlayerView::roleFits(player, controller->getStatsConfig());
  ASSERT_EQ(fits.size(), 4u);
  EXPECT_TRUE(std::ranges::is_sorted(fits,
                                     [](const auto& left, const auto& right)
                                     { return left.rating > right.rating; }));
  const PlayerView::PlayerRow row = PlayerView::makeRow(*controller, player);
  EXPECT_EQ(row.id, player.getId());
  EXPECT_EQ(row.name_lower, PlayerView::toLower(player.getName()));
  EXPECT_FALSE(row.value_text.empty());
}

TEST(SettingsPersistenceTest, AppearanceOptionsRoundTrip)
{
  SettingsManager* manager = SettingsManager::instance();
  const Settings original = manager->get();
  Settings& settings = manager->get();
  settings.theme_preset = 3;
  settings.club_accent = false;
  settings.accent_rgb = 0x3366CC;
  settings.ui_scale = 1.25f;
  settings.compact_density = true;
  settings.reduced_motion = true;
  manager->save();

  settings = Settings{};
  manager->load();
  EXPECT_EQ(manager->get().theme_preset, 3);
  EXPECT_FALSE(manager->get().club_accent);
  EXPECT_EQ(manager->get().accent_rgb, 0x3366CCU);
  EXPECT_FLOAT_EQ(manager->get().ui_scale, 1.25f);
  EXPECT_TRUE(manager->get().compact_density);
  EXPECT_TRUE(manager->get().reduced_motion);

  settings = original;
  manager->save();
}

TEST(ThemeTest, PackedAccentRoundTripsAndRatingScaleIsMonotonic)
{
  EXPECT_EQ(Theme::packRgb(Theme::unpackRgb(0x21A663)), 0x21A663U);
  EXPECT_LT(Theme::ratingColor(30.0).y, Theme::ratingColor(80.0).y);
  EXPECT_GT(Theme::ratingColor(30.0).x, Theme::ratingColor(80.0).x);
}
