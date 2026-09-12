set(FM_TEST_MATCH_SEED 424242)
# Fixed world seed: generated leagues, players and world events are then
# identical on every test run.
set(FM_TEST_WORLD_SEED 20250702)
set(FM_TEST_ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;FM_TEST_RUNTIME_ROOT=/tmp/football-management-tests;FM_MATCH_SEED=${FM_TEST_MATCH_SEED};FM_WORLD_SEED=${FM_TEST_WORLD_SEED}")
set(FM_TEST_LAUNCHER
  "${CMAKE_COMMAND};-E;env;${FM_TEST_ENVIRONMENT}")

add_executable(unit_tests
  test_player.cpp
  test_game_flow.cpp
  test_gamedata.cpp
  test_database.cpp
  test_match_engine.cpp
  test_match_renderer_3d.cpp
  test_match_scenarios.cpp
  test_transfer_market.cpp
  test_transfer_negotiation.cpp
  test_competitions.cpp
  test_view_models.cpp
  test_finances.cpp
  test_world_sim.cpp
  test_scouting.cpp
  test_training.cpp
  test_staff.cpp
)
target_link_libraries(unit_tests PRIVATE GTest::gtest_main fm_ui)
set_property(TARGET unit_tests PROPERTY TEST_LAUNCHER "${FM_TEST_LAUNCHER}")

# This target deliberately excludes all GUI sources. It is the fast headless
# correctness and leak-check lane for model, controller, and persistence code.
add_executable(core_unit_tests
  test_player.cpp
  test_gamedata.cpp
  test_database.cpp
  test_match_scenarios.cpp
  test_match_calibration.cpp
  test_match_rules.cpp
  test_transfer_market.cpp
  test_transfer_negotiation.cpp
  test_competitions.cpp
  test_finances.cpp
  test_world_sim.cpp
  test_scouting.cpp
  test_training.cpp
  test_staff.cpp
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
