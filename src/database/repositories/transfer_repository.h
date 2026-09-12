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
#include <span>
#include <unordered_map>
#include <vector>

#include "database/database_connection.h"
#include "model/transfer_market.h"

/**
 * @class TransferRepository
 * @brief Persists the transfer market: history (append-only), payments and
 * clauses, loans, pre-contracts, player market flags, incoming offers and
 * the managed club's open talks.
 *
 * Everything but the history is small and rewritten on every save; all
 * writes run inside the caller's transaction.
 */
class TransferRepository
{
 public:
  explicit TransferRepository(std::shared_ptr<DatabaseConnection> db_conn);

  std::vector<TransferRecord> loadHistory() const;
  /** Inserts @p records numbered from @p first_seq (idempotent). */
  void appendHistory(std::size_t first_seq,
                     std::span<const TransferRecord> records) const;

  std::vector<TransferObligation> loadObligations() const;
  void replaceObligations(
      const std::vector<TransferObligation>& obligations) const;

  std::vector<LoanDeal> loadLoans() const;
  void replaceLoans(const std::unordered_map<PlayerID, LoanDeal>& loans) const;

  std::vector<PreContractDeal> loadPreContracts() const;
  void replacePreContracts(
      const std::unordered_map<PlayerID, PreContractDeal>& deals) const;

  std::unordered_map<PlayerID, PlayerMarketFlags> loadFlags() const;
  void replaceFlags(
      const std::unordered_map<PlayerID, PlayerMarketFlags>& flags) const;

  void loadOffers(std::vector<IncomingOffer>& offers,
                  std::unordered_map<PlayerID, Negotiation>& talks) const;
  void replaceOffers(
      const std::vector<IncomingOffer>& offers,
      const std::unordered_map<PlayerID, Negotiation>& talks) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
