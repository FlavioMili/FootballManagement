// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <future>
#include <string>
#include <vector>

#include "gui/gui_scene.h"
#include "gui/gui_view.h"

/**
 * @brief Scene for the main menu.
 */
class MainMenuScene : public GUIScene
{
 public:
  /**
   * @brief Constructs a new MainMenuScene.
   * @param guiView_ptr Pointer to the GUIView.
   */
  explicit MainMenuScene(GUIView* guiView_ptr);

  /**
   * @brief Destroys the MainMenuScene.
   */
  ~MainMenuScene() override = default;

  /**
   * @brief Populated when the scene is entered. Caches the save slot metadata.
   */
  void onEnter() override;

  /**
   * @brief Updates scene logic.
   * @param deltaTime Time elapsed since last update.
   */
  void update(float deltaTime) override;

  /**
   * @brief Renders the scene.
   */
  void render() override;

  /**
   * @brief Gets the ID of this scene.
   * @return The SceneID (MAIN_MENU).
   */
  [[nodiscard]] SceneID getID() const override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;
  bool is_new_game = false;
  int loading_slot = 0;
  bool is_loading_rendered = false;
  bool loading_operation_started = false;
  std::future<bool> loading_operation;
  std::vector<GameController::SaveSlotMetadata> cached_metadata;
  int latest_career_slot = 0; /**< Most recently saved career, 0 = none. */
  int overwrite_slot = 0;     /**< Occupied slot awaiting confirmation. */

  // Save management: backups, restore, delete and load errors.
  int backups_slot = 0; /**< Slot whose backups dialog is open. */
  bool backups_requested = false;
  std::vector<SaveBackup> backups; /**< Read when the dialog opens. */
  std::vector<std::string> backup_labels;
  std::filesystem::path restore_candidate; /**< Awaiting confirmation. */
  std::filesystem::path pending_restore;   /**< Restored by the loader. */
  int delete_slot = 0; /**< Slot awaiting delete confirmation. */
  std::string delete_text;
  int load_error_slot = 0; /**< Slot whose load just failed. */
  bool load_error_requested = false;
  std::string load_error_text;

  void loadCachedMetadata();
  void renderBackdrop();
  void renderSlotPicker();
  void renderSlotActions(int slot,
                         const GameController::SaveSlotMetadata& metadata);
  void renderBackups();
  void renderLoadError();
  void openBackups(int slot);
  void startSlot(int slot, bool newGame);
};
