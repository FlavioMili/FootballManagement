add_executable(db_benchmarks
  db_benchmark.cpp
)

target_link_libraries(db_benchmarks
  PRIVATE
    core_lib
    benchmark::benchmark_main
)

target_include_directories(db_benchmarks
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

add_executable(match_benchmarks
  match_benchmark.cpp
)

target_link_libraries(match_benchmarks
  PRIVATE
    core_lib
    benchmark::benchmark
)

target_include_directories(match_benchmarks
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)
