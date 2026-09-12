// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "model/data_hub.h"
#include "model/delegation.h"
#include "model/onboarding.h"
#include "model/opposition_report.h"

class DatabaseConnection;

/**
 * @brief Per-career state of the guidance features: first-week checklist,
 * delegation policy, opposition instructions and the analytics snapshots of
 * the managed club's matches.
 *
 * Persisted in GuidanceState, DelegatedDuties, OppositionInstructions and
 * ManagedMatchAnalytics (assets/db/schema.sql) inside the game's save
 * transaction.
 */
class CareerGuidance
{
 public:
  /** Snapshots kept (oldest dropped): about two seasons of matches. */
  static constexpr std::size_t MAX_SNAPSHOTS = 130;

  OnboardingState onboarding;
  DelegationPolicy delegation;
  OppositionPlan opposition;

  /** Adds (or replaces the same fixture's) match snapshot. */
  void addSnapshot(ManagedMatchSnapshot snapshot);
  const std::vector<ManagedMatchSnapshot>& getSnapshots() const
  {
    return snapshots;
  }

  /** False for careers saved before the guidance features existed. */
  bool wasStored() const { return stored; }

  void load(const std::shared_ptr<DatabaseConnection>& db_conn);
  /** Writes the state; must run inside the caller's transaction. */
  void save(const std::shared_ptr<DatabaseConnection>& db_conn) const;
  /** Marks the snapshots as persisted after a successful commit. */
  void onSaved();

 private:
  std::vector<ManagedMatchSnapshot> snapshots;
  /** Snapshots from this index on are not in the database yet. */
  std::size_t unsaved_from = 0;
  /** Old snapshots were dropped since the last save. */
  bool pruned = false;
  bool stored = false;
};
