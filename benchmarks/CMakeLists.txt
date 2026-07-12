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
