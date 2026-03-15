# -----------------------------
# Path Configuration
# -----------------------------
configure_file(
  "${PROJECT_SOURCE_DIR}/src/global/paths.h.in"
  "${PROJECT_BINARY_DIR}/src/global/paths.h"
)

# -----------------------------
# Sources & Library
# -----------------------------
set(CORE_SOURCES
    # Controller
    controller/game_controller.h
    controller/game_controller.cpp

    # Database
    database/database.h
    database/database.cpp
    database/datagenerator.h
    database/datagenerator.cpp
    database/SQLLoader.h
    database/SQLLoader.cpp
    database/gamedata.h
    database/gamedata.cpp

    # Global
    global/global.h
    global/languages.h
    global/logger.h
    global/logger.cpp
    global/queries.h
    global/roles.h
    global/stats_config.h
    global/types.h

    # GUI
    gui/button_manager.h
    gui/button_manager.cpp
    gui/dropdown.h
    gui/dropdown.cpp
    gui/gui_scene.h
    gui/gui_scene.cpp
    gui/gui_view.h
    gui/gui_view.cpp
    gui/scenes/main_game_scene.h
    gui/scenes/main_game_scene.cpp
    gui/scenes/main_menu_scene.h
    gui/scenes/main_menu_scene.cpp
    gui/scenes/roster_scene.h
    gui/scenes/roster_scene.cpp
    gui/scenes/settings_scene.h
    gui/scenes/settings_scene.cpp
    gui/scenes/strategy_scene.h
    gui/scenes/strategy_scene.cpp
    gui/scenes/team_selection_scene.h
    gui/scenes/team_selection_scene.cpp

    # Model
    model/calendar.h
    model/calendar.cpp
    model/finances.h
    model/finances.cpp
    model/game.h
    model/game.cpp
    model/gamedate.h
    model/gamedate.cpp
    model/league.h
    model/league.cpp
    model/lineup.h
    model/lineup.cpp
    model/match.h
    model/match.cpp
    model/player.h
    model/player.cpp
    model/settings_manager.h
    model/settings_manager.cpp
    model/strategy.h
    model/strategy.cpp
    model/team.h
    model/team.cpp
)

add_library(core_lib ${CORE_SOURCES})

# -----------------------------
# Include Directories
# -----------------------------
target_include_directories(core_lib PUBLIC
  ${PROJECT_BINARY_DIR}/src
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/src/model
  ${PROJECT_SOURCE_DIR}/src/controller
  ${PROJECT_SOURCE_DIR}/src/database
  ${PROJECT_SOURCE_DIR}/src/gui
  ${PROJECT_SOURCE_DIR}/src/gui/scenes
)

target_link_libraries(core_lib PUBLIC
  fmt::fmt
  spdlog::spdlog
  nlohmann_json::nlohmann_json
  SQLite::SQLite3
  SDL3::SDL3
  SDL3_ttf::SDL3_ttf
)

# Basic warnings for all builds
target_compile_options(core_lib PRIVATE -Wall -Wextra -pedantic)

# -----------------------------
# Executable
# -----------------------------
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE core_lib)

# -----------------------------
# Build type flags (Applying to targets)
# -----------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(core_lib PRIVATE DEBUG)
    target_compile_options(core_lib PUBLIC -Og -g3 -fsanitize=address,undefined ${DEBUG_WARNINGS})
    target_link_options(core_lib PUBLIC -fsanitize=address,undefined)
    
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
    target_compile_options(${PROJECT_NAME} PRIVATE -Og -g3 -fsanitize=address,undefined ${DEBUG_WARNINGS})
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)

elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(core_lib PRIVATE NDEBUG)
    target_compile_options(core_lib PRIVATE -O2 -march=native -flto=auto)
    target_link_options(core_lib PRIVATE -flto=auto)

    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
    target_compile_options(${PROJECT_NAME} PRIVATE -O2 -march=native -flto=auto)
    target_link_options(${PROJECT_NAME} PRIVATE -flto=auto)

elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(core_lib PRIVATE -O2 -g -march=native)
    target_compile_options(${PROJECT_NAME} PRIVATE -O2 -g -march=native)
endif()
