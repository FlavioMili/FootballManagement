#include <benchmark/benchmark.h>

#include <filesystem>
#include <memory>

#include "database/database.h"
#include "database/gamedata.h"
#include "global/paths.h"

// Set up the database environment for benchmarks
class DatabaseFixture : public benchmark::Fixture {
 public:
  void SetUp(const ::benchmark::State& state) {
    (void)state;
    db = std::make_shared<Database>();
    db->initialize();
  }

  void TearDown(const ::benchmark::State& state) {
    (void)state;
    db.reset();
  }

  std::shared_ptr<Database> db;
};

BENCHMARK_DEFINE_F(DatabaseFixture, BM_LoadFromDB)(benchmark::State& state) {
  for (auto _ : state) { GameData::instance().loadFromDB(db); }
}

BENCHMARK_DEFINE_F(DatabaseFixture, BM_SaveToDB)(benchmark::State& state) {
  // Pre-load data so we have something to save
  GameData::instance().loadFromDB(db);

  for (auto _ : state) { GameData::instance().saveToDB(); }
}

// Register the functions as a benchmark
BENCHMARK_REGISTER_F(DatabaseFixture, BM_LoadFromDB)->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(DatabaseFixture, BM_SaveToDB)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
