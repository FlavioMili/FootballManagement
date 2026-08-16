add_executable(unit_tests
  test_player.cpp
  test_game_flow.cpp
  test_gamedata.cpp
  test_database.cpp
  test_match_engine.cpp
  test_match_scenarios.cpp
  test_transfer_market.cpp
)

target_link_libraries(unit_tests PRIVATE
  GTest::gtest_main
  core_lib
)

set(FM_TEST_MATCH_SEED 424242)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(unit_tests PRIVATE DEBUG)
endif()

set_property(TARGET unit_tests PROPERTY TEST_LAUNCHER
  "${CMAKE_COMMAND};-E;env;ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0;SDL_VIDEODRIVER=dummy;XDG_DATA_HOME=/tmp/football-management-tests;FM_MATCH_SEED=${FM_TEST_MATCH_SEED}"
)

include(GoogleTest)
gtest_discover_tests(
  unit_tests
  DISCOVERY_MODE PRE_TEST
  PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0;SDL_VIDEODRIVER=dummy;XDG_DATA_HOME=/tmp/football-management-tests;FM_MATCH_SEED=${FM_TEST_MATCH_SEED}"
)
