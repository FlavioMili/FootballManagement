add_executable(unit_tests
  test_player.cpp
)

target_link_libraries(unit_tests PRIVATE
  GTest::gtest_main
  core_lib
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
