// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "controller/game_controller.h"
#include "global/logger.h"
#include "model/game.h"
#include "model/team.h"
#include "model/player.h"
#include "gui/gui_view.h"
#include "gui/scenes/main_menu_scene.h"
#include "gui/scenes/settings_scene.h"
#include "gui/scenes/main_game_scene.h"
#include "gui/scenes/roster_scene.h"
#include "gui/scenes/strategy_scene.h"
#include "gui/scenes/team_selection_scene.h"
#include <SDL3/SDL.h>

class GameFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Logger to prevent segfaults when Game or Database try to log
        Logger::init();
        
        game = std::make_unique<Game>();
        controller = std::make_unique<GameController>(std::move(game));
    }

    std::unique_ptr<Game> game;
    std::unique_ptr<GameController> controller;
};

TEST_F(GameFlowTest, FullLifecycle) {
    // 1. Start a New Game
    // We assume there's a valid team ID, e.g., team ID 1.
    // Normally we'd fetch an actual team from the game's team list.
    auto teams = controller->getTeams();
    ASSERT_FALSE(teams.empty()) << "No teams loaded in the database!";
    
    TeamID firstTeamId = teams.front().get().getId();
    EXPECT_NO_THROW(controller->selectManagedTeam(firstTeamId));

    // 2. Data Access Check
    auto userTeamOpt = controller->getManagedTeam();
    ASSERT_TRUE(userTeamOpt.has_value()) << "User team should be assigned after selectManagedTeam";
    
    auto roster = controller->getPlayersForTeam(userTeamOpt->get().getId());
    EXPECT_GT(roster.size(), 0) << "User team should have players in the roster";

    // 3. Time Advancement
    // Simulate advancing a few days
    EXPECT_NO_THROW({
        controller->advanceDay();
        controller->advanceDay();
    });

    // 4. Persistence Check
    EXPECT_NO_THROW({
        controller->saveGame();
    }) << "saveGame() should not throw or core dump";

    // We can verify that the controller survives saving without invalidating state
    EXPECT_EQ(controller->getManagedTeam()->get().getId(), firstTeamId);
}

TEST_F(GameFlowTest, GUIFlowLifecycle) {
    // Enable headless SDL for testing
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    
    GUIView view(*controller);
    EXPECT_TRUE(view.initialize());

    auto step_frame = [&view]() {
        view.handleEvents();
        view.update(0.16f);
        view.render();
    };

    // 1. Initial frame (Main Menu)
    EXPECT_NO_THROW(step_frame());

    // 2. Change to Settings
    view.changeScene(std::make_unique<SettingsScene>(&view));
    EXPECT_NO_THROW(step_frame());

    // 3. Change back to Main Menu
    view.changeScene(std::make_unique<MainMenuScene>(&view));
    EXPECT_NO_THROW(step_frame());

    // 4. Change to Main Game Scene
    view.changeScene(std::make_unique<MainGameScene>(&view));
    EXPECT_NO_THROW(step_frame());

    // Pop the implicit TeamSelectionScene overlay
    view.popScene();
    EXPECT_NO_THROW(step_frame());

    // 5. Roster Scene Overlay
    view.overlayScene(std::make_unique<RosterScene>(&view));
    EXPECT_NO_THROW(step_frame());

    // Pop Roster Scene
    view.popScene();
    EXPECT_NO_THROW(step_frame());

    // 6. Strategy Scene Overlay
    view.overlayScene(std::make_unique<StrategyScene>(&view));
    EXPECT_NO_THROW(step_frame());

    // Pop Strategy Scene
    view.popScene();
    EXPECT_NO_THROW(step_frame());
}
