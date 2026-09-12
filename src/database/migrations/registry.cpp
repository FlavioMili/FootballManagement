// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <string>

#include "database/migrations/migrations.h"

namespace
{
std::string utcNow()
{
  return std::format(
      "{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(
                       std::chrono::system_clock::now()));
}

// 0001: save_meta gets the provenance of the save. Saves from before
// versioning have no creation date; a seed is only recorded when the world
// state stored one (never invented).
void versionMetadata(const Migrations::MigrationContext& context)
{
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(context.db,
                     "UPDATE save_meta SET game_version = ?, engine_version = "
                     "?, rules_edition = ?, rng_version = ?, sim_version = ?, "
                     "created_at_utc = ? WHERE id = 1;",
                     -1, &stmt, nullptr);
  const std::string created = context.fresh ? utcNow() : std::string();
  sqlite3_bind_text(stmt, 1, SaveFormat::GAME_VERSION.data(),
                    static_cast<int>(SaveFormat::GAME_VERSION.size()),
                    SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, SaveFormat::ENGINE_VERSION.data(),
                    static_cast<int>(SaveFormat::ENGINE_VERSION.size()),
                    SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, SaveFormat::RULES_EDITION.data(),
                    static_cast<int>(SaveFormat::RULES_EDITION.size()),
                    SQLITE_STATIC);
  sqlite3_bind_int(stmt, 4, SaveFormat::RNG_VERSION);
  sqlite3_bind_int(stmt, 5, SaveFormat::SIM_VERSION);
  sqlite3_bind_text(stmt, 6, created.c_str(), -1, SQLITE_TRANSIENT);
  const int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE)
    throw DatabaseException(std::string("save_meta: ") +
                            sqlite3_errmsg(context.db));

  if (context.fresh) return;
  const bool seeded =
      Migrations::tableExists(context.db, "WorldState") &&
      sqlite3_exec(context.db,
                   "UPDATE save_meta SET world_seed = (SELECT seed FROM "
                   "WorldState WHERE id = 1) WHERE id = 1 AND EXISTS (SELECT "
                   "1 FROM WorldState WHERE id = 1);",
                   nullptr, nullptr, nullptr) == SQLITE_OK &&
      sqlite3_changes(context.db) > 0;
  if (!seeded)
    sqlite3_exec(context.db, "UPDATE save_meta SET seed_unknown = 1;", nullptr,
                 nullptr, nullptr);
}

// Columns that earlier builds added with "ALTER TABLE ... fails harmlessly".
constexpr std::array LEAGUE_TIEBREAK = {
    Migrations::ColumnSpec{"Leagues", "tiebreak", "INTEGER NOT NULL DEFAULT 0"},
};

constexpr std::array FIXTURE_COMPETITIONS = {
    Migrations::ColumnSpec{"Fixtures", "competition_id",
                           "INTEGER NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Fixtures", "stage", "INTEGER NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Fixtures", "extra_time",
                           "INTEGER NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Fixtures", "home_penalties", "INTEGER"},
    Migrations::ColumnSpec{"Fixtures", "away_penalties", "INTEGER"},
};

constexpr std::array TRANSFER_BIDS = {
    Migrations::ColumnSpec{"TransferList", "highest_bidder_id", "INTEGER"},
    Migrations::ColumnSpec{"TransferList", "highest_bid",
                           "INTEGER NOT NULL DEFAULT 0"},
};

constexpr std::array WORLD_SIMULATION = {
    Migrations::ColumnSpec{"Players", "potential", "REAL NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Players", "traits", "TEXT NOT NULL DEFAULT ''"},
    Migrations::ColumnSpec{"Players", "dynamics", "TEXT NOT NULL DEFAULT ''"},
    Migrations::ColumnSpec{"Teams", "reputation", "INTEGER NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Teams", "stadium_capacity",
                           "INTEGER NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Teams", "ticket_price",
                           "INTEGER NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Teams", "training_facilities",
                           "INTEGER NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Teams", "youth_facilities",
                           "INTEGER NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Teams", "transfer_budget",
                           "INTEGER NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Teams", "wage_budget",
                           "INTEGER NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"Teams", "recent_form", "TEXT NOT NULL DEFAULT ''"},
};

// Scout reports record the ability range and whether they were opened;
// reports of older saves count as already read.
constexpr std::array SCOUT_REPORT_DETAILS = {
    Migrations::ColumnSpec{"ScoutReports", "overall_low",
                           "REAL NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"ScoutReports", "overall_high",
                           "REAL NOT NULL DEFAULT 0"},
    Migrations::ColumnSpec{"ScoutReports", "seen", "INTEGER NOT NULL DEFAULT 1"},
};

// Append new migrations at the end with the next number; never renumber,
// edit or remove a released one (see README.md).
constexpr std::array<Migrations::Migration, 6> REGISTRY = {{
    {1, "0001_version_metadata", {}, &versionMetadata},
    {2, "0002_league_tiebreak", LEAGUE_TIEBREAK, nullptr},
    {3, "0003_fixture_competitions", FIXTURE_COMPETITIONS, nullptr},
    {4, "0004_transfer_bids", TRANSFER_BIDS, nullptr},
    {5, "0005_world_simulation", WORLD_SIMULATION, nullptr},
    {6, "0006_scout_report_details", SCOUT_REPORT_DETAILS, nullptr},
}};

static_assert(std::ranges::is_sorted(REGISTRY, {}, &Migrations::Migration::number),
              "migrations must be ordered by number");
}  // namespace

std::span<const Migrations::Migration> Migrations::registry()
{
  return REGISTRY;
}

int Migrations::currentSchemaVersion() { return REGISTRY.back().number; }
