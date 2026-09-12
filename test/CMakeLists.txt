set(FM_TEST_MATCH_SEED 424242)
# Fixed world seed: generated leagues, players and world events are then
# identical on every test run.
set(FM_TEST_WORLD_SEED 20250702)
set(FM_TEST_ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;FM_TEST_RUNTIME_ROOT=/tmp/football-management-tests;FM_MATCH_SEED=${FM_TEST_MATCH_SEED};FM_WORLD_SEED=${FM_TEST_WORLD_SEED};FM_SIM_THREADS=2")
set(FM_TEST_LAUNCHER
  "${CMAKE_COMMAND};-E;env;${FM_TEST_ENVIRONMENT}")

add_executable(unit_tests
  test_localization.cpp
  test_next_action.cpp
  test_opposition_report.cpp
  test_match_analysis.cpp
  test_delegation.cpp
  test_data_hub.cpp
  test_player.cpp
  test_game_flow.cpp
  test_gamedata.cpp
  test_database.cpp
  test_match_engine.cpp
  test_match_renderer_3d.cpp
  test_match_scenarios.cpp
  test_transfer_market.cpp
  test_transfer_negotiation.cpp
  test_buyer_negotiation.cpp
  test_competitions.cpp
  test_staggered_rounds.cpp
  test_view_models.cpp
  test_finances.cpp
  test_world_sim.cpp
  test_match_scheduler.cpp
  test_scouting.cpp
  test_training.cpp
  test_staff.cpp
  test_world_data.cpp
  test_interactions.cpp
  test_save_migrations.cpp
  test_continental.cpp
  test_national_teams.cpp
  test_youth_academy.cpp
  test_manager_career.cpp
  test_squad_status.cpp
  test_squad_planner.cpp
  test_set_pieces.cpp
  test_medical_agenda.cpp
  test_awards.cpp
  test_records.cpp
  test_facility_projects.cpp
  test_preseason.cpp
  test_mentoring.cpp
  test_holiday.cpp
  test_nav_history.cpp
)
target_link_libraries(unit_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET unit_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")

# This target deliberately excludes all GUI sources. It is the fast headless
# correctness and leak-check lane for model, controller, and persistence code.
add_executable(core_unit_tests
  test_localization.cpp
  test_next_action.cpp
  test_opposition_report.cpp
  test_match_analysis.cpp
  test_delegation.cpp
  test_data_hub.cpp
  test_player.cpp
  test_gamedata.cpp
  test_database.cpp
  test_match_scenarios.cpp
  test_match_calibration.cpp
  test_match_rules.cpp
  test_transfer_market.cpp
  test_transfer_negotiation.cpp
  test_buyer_negotiation.cpp
  test_competitions.cpp
  test_staggered_rounds.cpp
  test_finances.cpp
  test_world_sim.cpp
  test_match_scheduler.cpp
  test_scouting.cpp
  test_training.cpp
  test_staff.cpp
  test_world_data.cpp
  test_interactions.cpp
  test_save_migrations.cpp
  test_continental.cpp
  test_national_teams.cpp
  test_youth_academy.cpp
  test_manager_career.cpp
  test_squad_status.cpp
  test_squad_planner.cpp
  test_set_pieces.cpp
  test_medical_agenda.cpp
  test_awards.cpp
  test_records.cpp
  test_facility_projects.cpp
  test_preseason.cpp
  test_mentoring.cpp
  test_holiday.cpp
)
target_link_libraries(core_unit_tests PRIVATE GTest::gtest_main fm_core)
set_property(TARGET core_unit_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(unit_tests PRIVATE DEBUG)
  target_compile_definitions(core_unit_tests PRIVATE DEBUG)
endif()

include(GoogleTest)
gtest_discover_tests(
  unit_tests
  DISCOVERY_MODE PRE_TEST
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit"
)
gtest_discover_tests(
  core_unit_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "core::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;core;leak-check"
)

# Match audio: mixer, procedural sounds and event mapping (headless, rendered
# by hand; the device test uses the dummy audio driver).
add_executable(audio_tests test_audio.cpp)
target_link_libraries(audio_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET audio_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  audio_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "audio::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;audio"
)

# Long GUI playtest journey (a season of a career through the real screens).
# Own executable so it carries its own label: exclude it with `-LE playtest`.
add_executable(playtest_tests test_playtest_journey.cpp)
target_link_libraries(playtest_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET playtest_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(playtest_tests PRIVATE DEBUG)
endif()
gtest_discover_tests(
  playtest_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "playtest::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "playtest;slow"
    TIMEOUT 400
)

# Seeded GUI monkey (random user through the real GUIView) and the widget
# effect sweep. Replay: FM_MONKEY_SEED=<n> FM_MONKEY_STEPS=<n>; exclude with
# `-LE monkey`.
add_executable(monkey_tests test_gui_monkey.cpp)
target_link_libraries(monkey_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET monkey_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  monkey_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "monkey::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "monkey;slow"
    TIMEOUT 900
)

# Scout pages of the Scouting screen through the real GUIView (screenshots
# in the runtime captures folder; FM_KEEP_TEST_ARTIFACTS=1 keeps them).
add_executable(scouting_ui_tests test_scouting_ui.cpp)
target_link_libraries(scouting_ui_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET scouting_ui_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  scouting_ui_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "scouting_ui::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;gui"
    TIMEOUT 600
)

# The Youth screen through the real GUIView (screenshots in the runtime
# captures folder; FM_KEEP_TEST_ARTIFACTS=1 keeps them).
add_executable(youth_ui_tests test_youth_ui.cpp)
target_link_libraries(youth_ui_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET youth_ui_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  youth_ui_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "youth_ui::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;gui"
    TIMEOUT 600
)

# Medical centre, calendar, squad planner, comparison and set-piece dialog
# through the real GUIView (screenshots in the runtime captures folder;
# FM_KEEP_TEST_ARTIFACTS=1 keeps them).
add_executable(squad_ui_tests test_squad_screens.cpp)
target_link_libraries(squad_ui_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET squad_ui_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  squad_ui_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "squad_ui::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;gui"
    TIMEOUT 600
)

# Manager career screens through the real GUIView (screenshots in the
# runtime captures folder; FM_KEEP_TEST_ARTIFACTS=1 keeps them).
add_executable(manager_ui_tests test_manager_ui.cpp)
target_link_libraries(manager_ui_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET manager_ui_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  manager_ui_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "manager_ui::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;gui"
    TIMEOUT 600
)

# Back / Forward history driven by touchpad swipes, mouse side buttons and
# Alt+arrows through the real GUIView.
add_executable(navigation_ui_tests test_navigation_ui.cpp)
target_link_libraries(navigation_ui_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET navigation_ui_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  navigation_ui_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "navigation_ui::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;gui"
    TIMEOUT 600
)

# Talks over an AI club's bid for a managed player, from the inbox and the
# transfer market, through the real GUIView at several window sizes and UI
# scales (screenshots in the runtime captures folder).
add_executable(transfer_ui_tests test_transfer_ui.cpp)
target_link_libraries(transfer_ui_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET transfer_ui_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  transfer_ui_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "transfer_ui::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;gui"
    TIMEOUT 600
)

# Guidance screens (checklist, next steps, delegation, opposition report,
# data hub, inbox decisions, half-time analysis) through the real GUIView at
# several window sizes and UI scales; screenshots in the captures folder.
add_executable(guidance_ui_tests test_guidance_ui.cpp)
target_link_libraries(guidance_ui_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET guidance_ui_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  guidance_ui_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "guidance_ui::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;gui"
    TIMEOUT 600
)

# Matchday controls of the live match (substitutions, tactics, shouts)
# through the real GUIView at several sizes; screenshots in the captures
# folder.
add_executable(match_controls_ui_tests test_match_controls_ui.cpp)
target_link_libraries(match_controls_ui_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET match_controls_ui_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  match_controls_ui_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "match_ui::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;gui"
    TIMEOUT 600
)

# Planning, holiday, awards and records screens through the real GUIView
# (screenshots in the runtime captures folder).
add_executable(career_ui_tests test_career_ui.cpp)
target_link_libraries(career_ui_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET career_ui_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  career_ui_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "career_ui::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;gui"
    TIMEOUT 900
)

# Adversarial user scenarios (double Continue, saves during Continue, empty
# squads, old saves, corrupt settings, HiDPI). Exclude with `-LE adversarial`.
add_executable(adversarial_tests test_adversarial.cpp)
target_link_libraries(adversarial_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET adversarial_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  adversarial_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "adversarial::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "adversarial;slow"
    TIMEOUT 600
)

# Balance lab (fm_lab): report math, verdicts, JSON schema and a tiny
# thread-count-invariant smoke run. Headless, links fm_core only.
add_executable(lab_tests test_lab_report.cpp)
target_link_libraries(lab_tests PRIVATE GTest::gtest_main fm_lab_lib)
set_property(TARGET lab_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")
gtest_discover_tests(
  lab_tests
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "lab::"
  PROPERTIES
    ENVIRONMENT "${FM_TEST_ENVIRONMENT}"
    LABELS "unit;lab"
)

# A deliberately invalid executable verifies that the sanitizer preset really
# instruments project code. CTest expects its non-zero sanitizer exit.
if(FM_ENABLE_SANITIZERS)
  add_executable(sanitizer_probe EXCLUDE_FROM_ALL sanitizer_probe.cpp)
  target_compile_options(sanitizer_probe PRIVATE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(sanitizer_probe PRIVATE -fsanitize=address,undefined)
  add_test(NAME sanitizer-instrumentation
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target sanitizer_probe)
  set_tests_properties(sanitizer-instrumentation PROPERTIES
    LABELS "unit;sanitizer"
    FIXTURES_SETUP sanitizer_probe_built)
  add_test(NAME sanitizer-detects-invalid-access COMMAND sanitizer_probe)
  set_tests_properties(sanitizer-detects-invalid-access PROPERTIES
    LABELS "unit;sanitizer"
    FIXTURES_REQUIRED sanitizer_probe_built
    WILL_FAIL TRUE)
endif()
