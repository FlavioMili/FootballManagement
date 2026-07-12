#include <benchmark/benchmark.h>

#include <filesystem>
#include <memory>

#include "database/database.h"
#include "database/gamedata.h"
#include "global/paths.h"

// Set up the database environment for benchmarks
class DatabaseFixture : public benchmark::Fixture {
 public:
  void SetUp(const ::benchmark::State& state) override {
    (void)state;
    // Remove the database file to ensure a clean slate for each benchmark run
    std::filesystem::remove(DATABASE_PATH);
    
    db = std::make_shared<Database>();
    db->initialize();
  }

  void TearDown(const ::benchmark::State& state) override {
    (void)state;
    db.reset();
  }

  std::shared_ptr<Database> db;
};

BENCHMARK_DEFINE_F(DatabaseFixture, BM_LoadFromDB)(benchmark::State& state) {
  state.PauseTiming();
  // Pre-load initial data
  GameData::instance().loadFromDB(db);

  int target_players = state.range(0);
  int current_players = GameData::instance().getPlayers().size();
  
  // Inject extra players directly into the DB
  for (int i = current_players; i < target_players; ++i) {
      Player p(i, 1, "Test", "Player " + std::to_string(i), "ST", 
               Language::EN, 1000, 0, 20, 2, 180, Foot::Right, {});
      db->insertPlayer(p);
  }
  state.ResumeTiming();

  for (auto _ : state) { 
      (void)_;
      GameData::instance().loadFromDB(db); 
  }
}

BENCHMARK_DEFINE_F(DatabaseFixture, BM_SaveToDB)(benchmark::State& state) {
  state.PauseTiming();
  // Pre-load data
  GameData::instance().loadFromDB(db);

  int target_players = state.range(0);
  int current_players = GameData::instance().getPlayers().size();
  
  for (int i = current_players; i < target_players; ++i) {
      Player p(i, 1, "Test", "Player " + std::to_string(i), "ST", 
               Language::EN, 1000, 0, 20, 2, 180, Foot::Right, {});
      db->insertPlayer(p);
      GameData::instance().addPlayer(p.getId(), p);
  }
  state.ResumeTiming();

  for (auto _ : state) { 
      (void)_;
      GameData::instance().saveToDB(); 
  }
}

// Register the functions as a benchmark and test scaling from 512 to 4096 players
BENCHMARK_REGISTER_F(DatabaseFixture, BM_LoadFromDB)->RangeMultiplier(2)->Range(512, 4096)->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(DatabaseFixture, BM_SaveToDB)->RangeMultiplier(2)->Range(512, 4096)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
