// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/transfer_market.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <utility>

#include "database/gamedata.h"
#include "database/repositories/transfer_repository.h"
#include "global/global.h"
#include "model/competition_manager.h"
#include "model/finances.h"
#include "model/player.h"
#include "model/team.h"
#include "model/transfer_tuning.h"
#include "model/world_rng.h"
#include "model/world_simulation.h"

namespace
{
using namespace TransferNegotiation;

constexpr std::array<const char*, 6> KIND_KEYS = {
    "TRANSFER_KIND_PERMANENT",    "TRANSFER_KIND_FREE",
    "TRANSFER_KIND_LOAN",         "TRANSFER_KIND_LOAN_RETURN",
    "TRANSFER_KIND_PRE_CONTRACT", "TRANSFER_KIND_RELEASE"};

// Recipient id that matches no club: suppresses the world's generic
// signing/sale news when this class posts a more specific message.
constexpr TeamID NO_RECIPIENT = std::numeric_limits<TeamID>::max();
constexpr int DAYS_PER_YEAR = 365;
constexpr int FIRST_TEAM_SIZE = 16;
constexpr std::size_t AI_FREE_AGENT_CHECKS = 5;
constexpr std::size_t PRE_CONTRACT_SAMPLE = 24;
constexpr float PRE_CONTRACT_LEVEL_MARGIN = 3.0f;
constexpr int PRE_CONTRACT_MAX_AGE = 31;
constexpr double PRE_CONTRACT_SUCCESS = 0.5;
constexpr std::uint8_t BIGGER_BORROWER_EXTRA_SHARE = 25;
constexpr std::uint64_t SELLER_RESOLVE_KEY = 0x5E11E4;

int level(SquadRole role) { return static_cast<int>(role); }

std::int64_t weeklyPayroll(const GameData& gamedata, const Team& team)
{
  return team.getFinances().getCurrentWageSpending(gamedata, team);
}

/** Players of a club by overall, best first. */
std::vector<std::pair<double, PlayerID>> rankedSquad(const GameData& gamedata,
                                                     const Team& team)
{
  const StatsConfig& config = gamedata.getStatsConfig();
  std::vector<std::pair<double, PlayerID>> ranked;
  ranked.reserve(team.getPlayerIDs().size());
  for (const PlayerID player_id : team.getPlayerIDs())
  {
    if (const auto player = gamedata.getPlayer(player_id))
      ranked.emplace_back(player->get().getOverall(config), player_id);
  }
  std::ranges::sort(ranked, std::greater<>{});
  return ranked;
}

/** Mean overall of the best FIRST_TEAM_SIZE players. */
float squadLevel(const std::vector<std::pair<double, PlayerID>>& ranked)
{
  if (ranked.empty()) return 0.0f;
  const std::size_t count =
      std::min<std::size_t>(ranked.size(), FIRST_TEAM_SIZE);
  double total = 0.0;
  for (std::size_t i = 0; i < count; ++i) total += ranked[i].first;
  return static_cast<float>(total / static_cast<double>(count));
}

/** Transfer money a club can commit today. */
std::int64_t availableBudget(const Team& team, std::int64_t committed)
{
  const Finances& finances = team.getFinances();
  return std::max<std::int64_t>(
      0, std::min(finances.getTransferBudget(), finances.getBalance()) -
             committed);
}
}  // namespace

const char* transferKindKey(TransferKind kind)
{
  const auto index = static_cast<std::size_t>(kind);
  return index < KIND_KEYS.size() ? KIND_KEYS[index] : "";
}

TransferMarket::TransferMarket(std::shared_ptr<GameData> data,
                               WorldSimulation& world_simulation,
                               const CompetitionManager& competition_manager)
    : gamedata(std::move(data)),
      world(world_simulation),
      competitions(competition_manager)
{
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

Player* TransferMarket::mutablePlayer(PlayerID player_id)
{
  auto& players = gamedata->getPlayers();
  const auto found = players.find(player_id);
  return found == players.end() ? nullptr : &found->second;
}

std::string TransferMarket::teamName(TeamID team_id) const
{
  const auto team = gamedata->getTeam(team_id);
  return team ? team->get().getName() : std::string(FREE_AGENTS_TEAM_NAME);
}

void TransferMarket::movePlayer(PlayerID player_id, TeamID from_team,
                                TeamID to_team)
{
  Player* player = mutablePlayer(player_id);
  if (!player) return;
  if (auto from = gamedata->getTeam(from_team))
    from->get().removePlayerID(player_id);
  if (auto to = gamedata->getTeam(to_team)) to->get().addPlayerID(player_id);
  gamedata->transferPlayer(player_id, to_team);
  if (player->getTransferStatus() == TransferStatus::Listed)
  {
    player->setTransferStatus(TransferStatus::NotListed);
    gamedata->deleteTransferListing(player_id);
  }
  // Line-ups hold raw pointers: rebuild both clubs.
  const StatsConfig& config = gamedata->getStatsConfig();
  if (auto from = gamedata->getTeam(from_team))
    from->get().generateStartingXI(*gamedata, config);
  if (auto to = gamedata->getTeam(to_team))
    to->get().generateStartingXI(*gamedata, config);
  PlayerDynamics& dynamics = player->mutableDynamics();
  dynamics.transfer_interest_weeks = 0;
  dynamics.playing_share = 0.0f;
}

void TransferMarket::pay(TeamID payer, TeamID payee, std::int64_t amount,
                         const GameDateValue& date)
{
  if (amount <= 0) return;
  if (auto from = gamedata->getTeam(payer);
      from && payer != FREE_AGENTS_TEAM_ID)
    from->get().getFinances().record(date, FinanceCategory::TransferFeeOut,
                                     -amount);
  if (auto to = gamedata->getTeam(payee); to && payee != FREE_AGENTS_TEAM_ID)
    to->get().getFinances().record(date, FinanceCategory::TransferFeeIn,
                                   amount);
}

std::int64_t TransferMarket::paySellOns(PlayerID player_id, TeamID seller,
                                        std::uint32_t fee,
                                        const GameDateValue& date)
{
  std::int64_t total = 0;
  std::erase_if(pending_obligations,
                [&](const TransferObligation& obligation)
                {
                  if (obligation.kind != ObligationKind::SellOn ||
                      obligation.player_id != player_id ||
                      obligation.payer != seller)
                    return false;
                  const std::int64_t share =
                      static_cast<std::int64_t>(fee) * obligation.amount / 100;
                  if (share > 0)
                  {
                    if (auto payee = gamedata->getTeam(obligation.payee))
                      payee->get().getFinances().record(
                          date, FinanceCategory::TransferFeeIn, share);
                    total += share;
                  }
                  return true;
                });
  return total;
}

void TransferMarket::addRecord(const TransferRecord& record)
{
  records.push_back(record);
}

void TransferMarket::post(const GameDateValue& date, InboxCategory category,
                          std::string title_key, std::string body_key,
                          std::vector<std::string> args,
                          std::optional<PlayerID> player_id,
                          std::optional<TeamID> team_id)
{
  InboxMessage message;
  message.date = date;
  message.category = category;
  message.title_key = std::move(title_key);
  message.body_key = std::move(body_key);
  message.args = std::move(args);
  message.player_id = player_id;
  message.team_id = team_id;
  world.getInbox().add(std::move(message));
}

std::uint16_t TransferMarket::careerCount(PlayerID player_id, TeamID team_id,
                                          ObligationKind kind) const
{
  int total = 0;
  for (const PlayerSeasonStats& stats : competitions.getPlayerCareer(player_id))
  {
    if (stats.team_id != team_id) continue;
    total +=
        kind == ObligationKind::GoalBonus ? stats.goals : stats.appearances;
  }
  return static_cast<std::uint16_t>(std::min(
      total, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
}

PlayerMarketFlags& TransferMarket::mutableFlags(PlayerID player_id)
{
  return player_flags[player_id];
}

void TransferMarket::pruneFlags(PlayerID player_id)
{
  const auto found = player_flags.find(player_id);
  if (found != player_flags.end() && found->second.empty())
    player_flags.erase(found);
}

void TransferMarket::clearOnMove(PlayerID player_id)
{
  const auto found = player_flags.find(player_id);
  if (found != player_flags.end())
  {
    found->second.release_clause = 0;
    found->second.promised_role.reset();
    found->second.loan_listed = false;
    pruneFlags(player_id);
  }
  negotiations.erase(player_id);
  std::erase_if(incoming, [player_id](const IncomingOffer& offer)
                { return offer.player_id == player_id; });
}

// ---------------------------------------------------------------------------
// Decision contexts
// ---------------------------------------------------------------------------

SquadRole TransferMarket::projectedRole(PlayerID player_id,
                                        TeamID club_id) const
{
  const auto player = gamedata->getPlayer(player_id);
  if (!player) return SquadRole::Fringe;
  if (player->get().getTeamId() == club_id) return world.squadRole(player_id);
  const StatsConfig& config = gamedata->getStatsConfig();
  const double overall = player->get().getOverall(config);
  std::size_t rank = 0;
  for (const auto& other : gamedata->getPlayersForTeam(club_id))
  {
    if (other.get().getOverall(config) > overall) ++rank;
  }
  return roleForRank(rank);
}

SaleContext TransferMarket::saleContext(PlayerID player_id, TeamID buyer_id,
                                        const GameDateValue& date,
                                        std::uint32_t listing_price) const
{
  SaleContext context;
  const auto player = gamedata->getPlayer(player_id);
  if (!player) return context;
  const Player& p = player->get();
  p.updateMarketValue(gamedata->getStatsConfig());
  context.market_value = p.getMarketValue();
  context.age = p.getAge();
  context.role = world.squadRole(player_id);
  context.contract_years = p.getContractYears();
  const auto seller = gamedata->getTeam(p.getTeamId());
  const auto buyer = gamedata->getTeam(buyer_id);
  if (seller) context.seller_reputation = seller->get().getReputation();
  if (buyer) context.buyer_reputation = buyer->get().getReputation();
  context.same_league =
      seller && buyer &&
      seller->get().getLeagueId() == buyer->get().getLeagueId();
  context.listed = listing_price > 0;
  context.listing_price = listing_price;
  if (const PlayerMarketFlags* extras = flags(player_id))
    context.release_clause = extras->release_clause;
  const WindowInfo window = windowInfo(date);
  context.winter_window = window.winter;
  context.days_to_deadline = window.open ? window.days_to_deadline : -1;
  // One draw per player and window: asking again cannot change the answer.
  const std::uint64_t window_key =
      static_cast<std::uint64_t>(date.year) * 2U + (window.winter ? 1U : 0U);
  context.resolve = static_cast<float>(
      WorldRng::hashUniform(gamedata->getWorldSeed(), RngDomain::Transfers,
                            player_id, mixHash(window_key, SELLER_RESOLVE_KEY)));
  return context;
}

PlayerContext TransferMarket::playerContext(PlayerID player_id, TeamID club_id,
                                            ContractKind kind) const
{
  PlayerContext context;
  context.kind = kind;
  const auto player = gamedata->getPlayer(player_id);
  if (!player) return context;
  const Player& p = player->get();
  context.age = p.getAge();
  const LoanDeal* loan = findLoan(player_id);
  context.current_wage = loan ? loan->full_wage : p.getWage();
  p.updateMarketValue(gamedata->getStatsConfig());
  context.market_value = p.getMarketValue();
  context.ambition = p.getTraits().ambition;
  context.loyalty = p.getTraits().loyalty;
  context.unsettled = p.getDynamics().transfer_interest_weeks > 0;
  const TeamID current = loan ? loan->parent : p.getTeamId();
  if (const auto team = gamedata->getTeam(current);
      team && current != FREE_AGENTS_TEAM_ID)
  {
    context.current_club_reputation = team->get().getReputation();
    context.current_role = world.squadRole(player_id);
  }
  else
  {
    context.current_role = SquadRole::Backup;
  }
  if (const auto club = gamedata->getTeam(club_id))
    context.new_club_reputation = club->get().getReputation();
  context.projected_role = projectedRole(player_id, club_id);
  return context;
}

bool TransferMarket::wouldJoin(PlayerID player_id, TeamID club_id,
                               ContractKind kind) const
{
  const PlayerContext context = playerContext(player_id, club_id, kind);
  const ContractDemand demand = contractDemand(context);
  return evaluateContract(context, demandedOffer(demand), 0).accepted;
}

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------

std::uint32_t TransferMarket::agentFee(const Deal& deal)
{
  using Offer = TransferTuning::Offer;
  if (deal.kind == TransferKind::Permanent && deal.terms.fee > 0)
    return static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(deal.terms.fee) * Offer::AGENT_FEE_PERCENT /
        100U);
  return deal.contract.weekly_wage * Offer::FREE_AGENT_FEE_WEEKS;
}

bool TransferMarket::completeTransfer(const Deal& deal,
                                      const GameDateValue& date,
                                      TeamID managed_team_id)
{
  Player* player = mutablePlayer(deal.player_id);
  const auto buyer = gamedata->getTeam(deal.buyer_id);
  if (!player || !buyer || deal.buyer_id == FREE_AGENTS_TEAM_ID) return false;
  const TeamID seller_id = player->getTeamId();
  if (seller_id == deal.buyer_id || !isValid(deal.terms) ||
      deal.contract.years == 0 ||
      deal.contract.years > maxContractYears(player->getAge()))
    return false;
  if (deal.kind != TransferKind::PreContract && !canBeTraded(deal.player_id))
    return false;
  Deal effective = deal;
  if (seller_id == FREE_AGENTS_TEAM_ID)
  {
    effective.terms = OfferTerms{};
    if (effective.kind == TransferKind::Permanent)
      effective.kind = TransferKind::Free;
  }
  const OfferTerms& terms = effective.terms;
  Finances& buyer_finances = buyer->get().getFinances();

  if (terms.fee > 0)
  {
    const std::uint32_t upfront = upfrontAmount(terms);
    const std::int64_t sell_on =
        paySellOns(deal.player_id, seller_id, terms.fee, date);
    buyer_finances.record(date, FinanceCategory::TransferFeeOut,
                          -static_cast<std::int64_t>(upfront));
    if (auto seller = gamedata->getTeam(seller_id))
    {
      // A sell-on share above the upfront part is a net outflow; the
      // allowance never drops below zero.
      Finances& finances = seller->get().getFinances();
      finances.record(date, FinanceCategory::TransferFeeIn,
                      static_cast<std::int64_t>(upfront) - sell_on);
      finances.setTransferBudget(finances.getTransferBudget());
    }
    const auto instalments = instalmentAmounts(terms);
    for (std::size_t year = 0; year < instalments.size(); ++year)
    {
      TransferObligation obligation;
      obligation.id = next_id++;
      obligation.kind = ObligationKind::Instalment;
      obligation.player_id = deal.player_id;
      obligation.payer = deal.buyer_id;
      obligation.payee = seller_id;
      obligation.amount = instalments[year];
      obligation.due = date + static_cast<std::size_t>(
                                  DAYS_PER_YEAR * static_cast<int>(year + 1));
      pending_obligations.push_back(obligation);
    }
    const auto add_bonus =
        [&](ObligationKind kind, std::uint32_t amount, std::uint16_t target)
    {
      if (amount == 0 || target == 0) return;
      TransferObligation obligation;
      obligation.id = next_id++;
      obligation.kind = kind;
      obligation.player_id = deal.player_id;
      obligation.payer = deal.buyer_id;
      obligation.payee = seller_id;
      obligation.amount = amount;
      obligation.due = date;
      obligation.target = target;
      obligation.baseline = careerCount(deal.player_id, deal.buyer_id, kind);
      pending_obligations.push_back(obligation);
    };
    add_bonus(ObligationKind::AppearanceBonus, terms.appearance_bonus,
              terms.appearance_target);
    add_bonus(ObligationKind::GoalBonus, terms.goal_bonus, terms.goal_target);
    if (terms.sell_on_percent > 0)
    {
      TransferObligation obligation;
      obligation.id = next_id++;
      obligation.kind = ObligationKind::SellOn;
      obligation.player_id = deal.player_id;
      obligation.payer = deal.buyer_id;
      obligation.payee = seller_id;
      obligation.amount = terms.sell_on_percent;
      obligation.due = date;
      pending_obligations.push_back(obligation);
    }
    payables_dirty = true;
  }
  else
  {
    // Clauses of the old club lapse on a free move.
    std::erase_if(pending_obligations,
                  [&](const TransferObligation& obligation)
                  {
                    return obligation.player_id == deal.player_id &&
                           obligation.kind != ObligationKind::Instalment &&
                           obligation.payer == seller_id;
                  });
  }

  if (const std::uint32_t agent = agentFee(effective); agent > 0)
    buyer_finances.record(date, FinanceCategory::TransferFeeOut,
                          -static_cast<std::int64_t>(agent));
  if (deal.contract.signing_bonus > 0)
    buyer_finances.record(
        date, FinanceCategory::Wages,
        -static_cast<std::int64_t>(deal.contract.signing_bonus));

  clearOnMove(deal.player_id);
  movePlayer(deal.player_id, seller_id, deal.buyer_id);
  player->setWage(deal.contract.weekly_wage);
  player->setContractYears(deal.contract.years);
  if (deal.contract.release_clause > 0 || deal.contract.promised_role)
  {
    PlayerMarketFlags& extras = mutableFlags(deal.player_id);
    extras.release_clause = deal.contract.release_clause;
    extras.promised_role = deal.contract.promised_role;
    extras.promise_date = date;
  }
  addRecord({deal.player_id, date, seller_id, deal.buyer_id, terms.fee,
             effective.kind});

  if (effective.kind == TransferKind::Permanent)
  {
    world.onTransferCompleted(date, deal.player_id, seller_id, deal.buyer_id,
                              terms.fee, managed_team_id);
  }
  else
  {
    world.onTransferCompleted(date, deal.player_id, seller_id, deal.buyer_id, 0,
                              NO_RECIPIENT);
    if (deal.buyer_id == managed_team_id)
      post(date, InboxCategory::Transfer, "INBOX_FREE_SIGNING_TITLE",
           "INBOX_FREE_SIGNING_BODY",
           {player->getName(), teamName(seller_id),
            std::to_string(deal.contract.years)},
           deal.player_id, seller_id);
    else if (seller_id == managed_team_id)
      post(date, InboxCategory::Transfer, "INBOX_FREE_EXIT_TITLE",
           "INBOX_FREE_EXIT_BODY", {player->getName(), teamName(deal.buyer_id)},
           deal.player_id, deal.buyer_id);
  }
  return true;
}

bool TransferMarket::startLoan(PlayerID player_id, TeamID borrower_id,
                               const LoanTerms& terms,
                               const GameDateValue& date,
                               TeamID managed_team_id)
{
  Player* player = mutablePlayer(player_id);
  if (!player || !gamedata->getTeam(borrower_id)) return false;
  const TeamID parent = player->getTeamId();
  if (parent == FREE_AGENTS_TEAM_ID || borrower_id == FREE_AGENTS_TEAM_ID ||
      parent == borrower_id || !canBeTraded(player_id) ||
      terms.wage_share > 100 ||
      !loanWithinLimits(player_id, parent, borrower_id))
    return false;

  LoanDeal loan;
  loan.player_id = player_id;
  loan.parent = parent;
  loan.borrower = borrower_id;
  loan.start = date;
  loan.end = loanEndDate(date, terms.duration);
  loan.wage_share = terms.wage_share;
  loan.full_wage = player->getWage();
  loan.option_fee = terms.option_fee;
  loan.obligation = terms.obligation && terms.option_fee > 0;
  loan.recall_clause = terms.recall_clause;
  if (!(date < loan.end)) return false;

  pay(borrower_id, parent, terms.loan_fee, date);
  clearOnMove(player_id);
  movePlayer(player_id, parent, borrower_id);
  player->setWage(static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(loan.full_wage) * loan.wage_share / 100U));
  active_loans[player_id] = loan;
  addRecord({player_id, date, parent, borrower_id, terms.loan_fee,
             TransferKind::Loan});

  const std::string end_text = loan.end.toString();
  if (parent == managed_team_id)
    post(date, InboxCategory::Transfer, "INBOX_LOAN_OUT_TITLE",
         "INBOX_LOAN_OUT_BODY",
         {player->getName(), teamName(borrower_id), end_text,
          std::to_string(loan.wage_share)},
         player_id, borrower_id);
  else if (borrower_id == managed_team_id)
    post(date, InboxCategory::Transfer, "INBOX_LOAN_IN_TITLE",
         "INBOX_LOAN_IN_BODY",
         {player->getName(), teamName(parent), end_text,
          std::to_string(loan.wage_share)},
         player_id, parent);
  return true;
}

bool TransferMarket::endLoan(PlayerID player_id, const GameDateValue& date,
                             TeamID managed_team_id, bool recalled)
{
  const auto found = active_loans.find(player_id);
  if (found == active_loans.end()) return false;
  const LoanDeal loan = found->second;
  active_loans.erase(found);
  Player* player = mutablePlayer(player_id);
  if (!player) return true;
  player->setWage(loan.full_wage);
  movePlayer(player_id, player->getTeamId(), loan.parent);
  addRecord({player_id, date, loan.borrower, loan.parent, 0,
             TransferKind::LoanReturn});
  if (loan.parent == managed_team_id || loan.borrower == managed_team_id)
    post(date, InboxCategory::Transfer,
         recalled ? "INBOX_LOAN_RECALL_TITLE" : "INBOX_LOAN_RETURN_TITLE",
         recalled ? "INBOX_LOAN_RECALL_BODY" : "INBOX_LOAN_RETURN_BODY",
         {player->getName(), teamName(loan.borrower), teamName(loan.parent)},
         player_id,
         loan.parent == managed_team_id ? loan.borrower : loan.parent);
  return true;
}

bool TransferMarket::exerciseLoanOption(PlayerID player_id,
                                        const GameDateValue& date,
                                        TeamID managed_team_id)
{
  const auto found = active_loans.find(player_id);
  if (found == active_loans.end() || found->second.option_fee == 0)
    return false;
  const LoanDeal loan = found->second;
  Player* player = mutablePlayer(player_id);
  if (!player) return false;
  active_loans.erase(found);
  player->setWage(loan.full_wage);
  movePlayer(player_id, player->getTeamId(), loan.parent);

  Deal deal;
  deal.player_id = player_id;
  deal.buyer_id = loan.borrower;
  deal.kind = TransferKind::Permanent;
  deal.terms = aiOfferTerms(loan.option_fee);
  deal.contract.weekly_wage = loan.full_wage;
  deal.contract.years = std::clamp<std::uint8_t>(
      std::max<std::uint8_t>(player->getContractYears(), 2), 1,
      maxContractYears(player->getAge()));
  if (completeTransfer(deal, date, managed_team_id)) return true;
  // The purchase failed: the player stays with the parent club.
  addRecord({player_id, date, loan.borrower, loan.parent, 0,
             TransferKind::LoanReturn});
  return false;
}

bool TransferMarket::releasePlayer(PlayerID player_id,
                                   const GameDateValue& date,
                                   TeamID managed_team_id)
{
  Player* player = mutablePlayer(player_id);
  if (!player) return false;
  const TeamID club_id = player->getTeamId();
  const auto club = gamedata->getTeam(club_id);
  if (!club || club_id == FREE_AGENTS_TEAM_ID || !canBeTraded(player_id))
    return false;
  const std::int64_t severance =
      severancePay(player->getWage(), player->getContractYears(), date);
  if (severance > 0)
    club->get().getFinances().record(date, FinanceCategory::Wages, -severance);
  std::erase_if(pending_obligations,
                [&](const TransferObligation& obligation)
                {
                  return obligation.player_id == player_id &&
                         obligation.kind != ObligationKind::Instalment &&
                         obligation.payer == club_id;
                });
  clearOnMove(player_id);
  movePlayer(player_id, club_id, FREE_AGENTS_TEAM_ID);
  player->setContractYears(0);
  addRecord({player_id, date, club_id, FREE_AGENTS_TEAM_ID,
             static_cast<std::uint32_t>(std::min<std::int64_t>(
                 severance, std::numeric_limits<std::uint32_t>::max())),
             TransferKind::Release});
  if (club_id == managed_team_id)
    post(date, InboxCategory::Contract, "INBOX_RELEASE_TITLE",
         "INBOX_RELEASE_BODY", {player->getName(), formatMoney(severance)},
         player_id, club_id);
  return true;
}

bool TransferMarket::agreePreContract(PlayerID player_id, TeamID club_id,
                                      const ContractOffer& terms,
                                      const GameDateValue& date,
                                      TeamID managed_team_id)
{
  const Player* player = mutablePlayer(player_id);
  if (!player || !gamedata->getTeam(club_id)) return false;
  const LoanDeal* loan = findLoan(player_id);
  const TeamID owner = loan ? loan->parent : player->getTeamId();
  if (owner == FREE_AGENTS_TEAM_ID || owner == club_id ||
      club_id == FREE_AGENTS_TEAM_ID || pre_contracts.contains(player_id) ||
      !canSignPreContract(player->getContractYears(), date) ||
      terms.years == 0 || terms.years > maxContractYears(player->getAge()))
    return false;
  pre_contracts[player_id] = {player_id, owner, club_id, date, terms};
  negotiations.erase(player_id);
  std::erase_if(incoming, [player_id](const IncomingOffer& offer)
                { return offer.player_id == player_id; });
  if (club_id == managed_team_id)
    post(date, InboxCategory::Transfer, "INBOX_PRECONTRACT_SIGNED_TITLE",
         "INBOX_PRECONTRACT_SIGNED_BODY", {player->getName(), teamName(owner)},
         player_id, owner);
  else if (owner == managed_team_id)
    post(date, InboxCategory::Transfer, "INBOX_PRECONTRACT_LOST_TITLE",
         "INBOX_PRECONTRACT_LOST_BODY", {player->getName(), teamName(club_id)},
         player_id, club_id);
  return true;
}

void TransferMarket::recordLegacyTransfer(PlayerID player_id, TeamID from_team,
                                          TeamID to_team, std::uint32_t fee,
                                          const GameDateValue& date)
{
  if (fee > 0 && from_team != FREE_AGENTS_TEAM_ID)
  {
    const std::int64_t sell_on = paySellOns(player_id, from_team, fee, date);
    if (auto seller = gamedata->getTeam(from_team); seller && sell_on > 0)
    {
      Finances& finances = seller->get().getFinances();
      finances.record(date, FinanceCategory::TransferFeeIn, -sell_on);
      finances.setTransferBudget(finances.getTransferBudget());
    }
  }
  clearOnMove(player_id);
  addRecord({player_id, date, from_team, to_team, fee,
             from_team == FREE_AGENTS_TEAM_ID ? TransferKind::Free
                                              : TransferKind::Permanent});
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

bool TransferMarket::canBeTraded(PlayerID player_id) const
{
  return !active_loans.contains(player_id) &&
         !pre_contracts.contains(player_id);
}

bool TransferMarket::loanWithinLimits(PlayerID player_id, TeamID parent,
                                      TeamID borrower) const
{
  using L = TransferTuning::Loan;
  int out = 0;
  int in = 0;
  int between = 0;
  const auto counts = [&](PlayerID id)
  {
    const auto player = gamedata->getPlayer(id);
    return player && player->get().getAge() > L::EXEMPT_MAX_AGE;
  };
  for (const auto& [id, loan] : active_loans)
  {
    if (loan.parent == parent && loan.borrower == borrower) ++between;
    if (!counts(id)) continue;
    if (loan.parent == parent) ++out;
    if (loan.borrower == borrower) ++in;
  }
  if (between >= L::MAX_BETWEEN_CLUBS) return false;
  if (!counts(player_id)) return true;
  return out < L::MAX_LOANS_OUT && in < L::MAX_LOANS_IN;
}

std::int64_t TransferMarket::committedPayables(TeamID team_id,
                                               const GameDateValue& date) const
{
  const std::int32_t day = dayOrdinal(date);
  if (payables_dirty || payables_day != day)
  {
    payables_cache.clear();
    const GameDateValue season_end = seasonEndDate(date);
    for (const TransferObligation& obligation : pending_obligations)
    {
      if (obligation.kind == ObligationKind::Instalment &&
          !(season_end < obligation.due))
        payables_cache[obligation.payer] += obligation.amount;
    }
    payables_day = day;
    payables_dirty = false;
  }
  const auto found = payables_cache.find(team_id);
  return found == payables_cache.end() ? 0 : found->second;
}

// ---------------------------------------------------------------------------
// Daily processing
// ---------------------------------------------------------------------------

void TransferMarket::onDayAdvanced(const GameDateValue& date,
                                   TeamID managed_team_id)
{
  const bool week_end = dayOrdinal(date) % 7 == 0;
  if (week_end) payLoanWageShares(date);
  processLoanEnds(date, managed_team_id);
  if (date.month == 7 && date.day == 1)
    executePreContracts(date, managed_team_id);
  payDueInstalments(date, managed_team_id);
  if (week_end) checkAddOns(date, managed_team_id);
  if (date.day == 1) checkPromises(date, managed_team_id);
  expireOffers(date);
  postNewsDigest(date, managed_team_id);
}

void TransferMarket::payLoanWageShares(const GameDateValue& date)
{
  // The borrower's payroll carries its share through the loanee's wage;
  // the parent pays the rest on the same weekly cadence.
  for (const auto& [player_id, loan] : active_loans)
  {
    const auto parent = gamedata->getTeam(loan.parent);
    const std::int64_t share = static_cast<std::int64_t>(loan.full_wage) *
                               (100 - loan.wage_share) / 100;
    if (parent && share > 0)
      parent->get().getFinances().record(date, FinanceCategory::Wages, -share);
  }
}

void TransferMarket::processLoanEnds(const GameDateValue& date,
                                     TeamID managed_team_id)
{
  std::vector<PlayerID> ending;
  for (const auto& [player_id, loan] : active_loans)
  {
    if (!(date < loan.end)) ending.push_back(player_id);
  }
  std::ranges::sort(ending);
  for (const PlayerID player_id : ending)
  {
    const LoanDeal loan = active_loans.at(player_id);
    bool bought = false;
    if (loan.option_fee > 0)
    {
      bool buy = loan.obligation;
      if (!buy && loan.borrower != managed_team_id)
      {
        // AI borrowers keep loanees who earned a place and are affordable.
        const auto borrower = gamedata->getTeam(loan.borrower);
        buy = borrower &&
              level(world.squadRole(player_id)) <= level(SquadRole::Rotation) &&
              availableBudget(borrower->get(),
                              committedPayables(loan.borrower, date)) >=
                  static_cast<std::int64_t>(loan.option_fee);
      }
      if (buy) bought = exerciseLoanOption(player_id, date, managed_team_id);
    }
    if (!bought && active_loans.contains(player_id))
      endLoan(player_id, date, managed_team_id, false);
  }
}

void TransferMarket::executePreContracts(const GameDateValue& date,
                                         TeamID managed_team_id)
{
  std::vector<PreContractDeal> due;
  due.reserve(pre_contracts.size());
  for (const auto& [player_id, deal] : pre_contracts) due.push_back(deal);
  pre_contracts.clear();
  std::ranges::sort(due, {}, &PreContractDeal::player_id);
  for (const PreContractDeal& agreement : due)
  {
    Deal deal;
    deal.player_id = agreement.player_id;
    deal.buyer_id = agreement.to_team;
    deal.kind = TransferKind::PreContract;
    deal.contract = agreement.terms;
    const Player* player = mutablePlayer(agreement.player_id);
    // Retired players simply drop out of their agreement.
    if (player && player->getTeamId() != agreement.to_team)
      completeTransfer(deal, date, managed_team_id);
  }
}

void TransferMarket::payDueInstalments(const GameDateValue& date,
                                       TeamID managed_team_id)
{
  std::vector<TransferObligation> due;
  std::erase_if(pending_obligations,
                [&](const TransferObligation& obligation)
                {
                  if (obligation.kind != ObligationKind::Instalment ||
                      date < obligation.due)
                    return false;
                  due.push_back(obligation);
                  return true;
                });
  if (due.empty()) return;
  payables_dirty = true;
  for (const TransferObligation& obligation : due)
  {
    pay(obligation.payer, obligation.payee, obligation.amount, date);
    if (obligation.payer != managed_team_id &&
        obligation.payee != managed_team_id)
      continue;
    const auto player = gamedata->getPlayer(obligation.player_id);
    const bool paying = obligation.payer == managed_team_id;
    post(date, InboxCategory::Finance,
         paying ? "INBOX_INSTALMENT_PAID_TITLE"
                : "INBOX_INSTALMENT_RECEIVED_TITLE",
         paying ? "INBOX_INSTALMENT_PAID_BODY"
                : "INBOX_INSTALMENT_RECEIVED_BODY",
         {formatMoney(obligation.amount),
          teamName(paying ? obligation.payee : obligation.payer),
          player ? player->get().getName() : std::string()},
         std::nullopt, paying ? obligation.payee : obligation.payer);
  }
}

void TransferMarket::checkAddOns(const GameDateValue& date,
                                 TeamID managed_team_id)
{
  std::vector<TransferObligation> triggered;
  std::erase_if(pending_obligations,
                [&](const TransferObligation& obligation)
                {
                  if (obligation.kind != ObligationKind::AppearanceBonus &&
                      obligation.kind != ObligationKind::GoalBonus)
                    return false;
                  const auto player = gamedata->getPlayer(obligation.player_id);
                  // The condition lapses when the player leaves the buying
                  // club.
                  if (!player || player->get().getTeamId() != obligation.payer)
                    return true;
                  const std::uint16_t count = careerCount(
                      obligation.player_id, obligation.payer, obligation.kind);
                  if (count < obligation.baseline + obligation.target)
                    return false;
                  triggered.push_back(obligation);
                  return true;
                });
  for (const TransferObligation& obligation : triggered)
  {
    pay(obligation.payer, obligation.payee, obligation.amount, date);
    if (obligation.payer != managed_team_id &&
        obligation.payee != managed_team_id)
      continue;
    const auto player = gamedata->getPlayer(obligation.player_id);
    post(date, InboxCategory::Finance, "INBOX_ADDON_TITLE", "INBOX_ADDON_BODY",
         {player ? player->get().getName() : std::string(),
          formatMoney(obligation.amount), teamName(obligation.payer),
          teamName(obligation.payee)},
         obligation.player_id, std::nullopt);
  }
}

void TransferMarket::checkPromises(const GameDateValue& date,
                                   TeamID managed_team_id)
{
  using N = TransferTuning::Negotiation;
  const std::int32_t today = dayOrdinal(date);
  for (auto& [player_id, extras] : player_flags)
  {
    if (!extras.promised_role ||
        today - dayOrdinal(extras.promise_date) < N::PROMISE_GRACE_DAYS)
      continue;
    Player* player = mutablePlayer(player_id);
    if (!player || player->getTeamId() != managed_team_id) continue;
    const SquadRole actual = world.squadRole(player_id);
    if (level(actual) - level(*extras.promised_role) < N::BROKEN_PROMISE_LEVELS)
      continue;
    PlayerDynamics& dynamics = player->mutableDynamics();
    dynamics.morale =
        std::max(0.0f, dynamics.morale - N::BROKEN_PROMISE_MORALE);
    post(date, InboxCategory::Contract, "INBOX_PROMISE_BROKEN_TITLE",
         "INBOX_PROMISE_BROKEN_BODY",
         {player->getName(),
          "@" + std::string(squadRoleKey(*extras.promised_role))},
         player_id, managed_team_id);
    extras.promised_role.reset();
  }
  std::erase_if(player_flags,
                [](const auto& entry) { return entry.second.empty(); });
}

void TransferMarket::expireOffers(const GameDateValue& date)
{
  std::erase_if(incoming, [&](const IncomingOffer& offer)
                { return offer.expires < date; });
  std::erase_if(negotiations,
                [&](const auto& entry) { return entry.second.expires < date; });
}

void TransferMarket::postNewsDigest(const GameDateValue& date,
                                    TeamID managed_team_id)
{
  if (managed_team_id == FREE_AGENTS_TEAM_ID) return;
  const std::int32_t today = dayOrdinal(date);
  const bool open = date.isTransferWindowOpen();
  const bool window_closed = !open && (date - 1).isTransferWindowOpen();
  std::int32_t first = 0;
  std::int32_t last = 0;
  constexpr std::int32_t PERIOD = TransferTuning::Market::NEWS_DIGEST_DAYS;
  if (open && today % PERIOD == 0)
  {
    first = today - PERIOD + 1;
    last = today;
  }
  else if (window_closed)
  {
    const std::int32_t offset = today % PERIOD == 0 ? PERIOD : today % PERIOD;
    first = today - offset + 1;
    last = today - 1;
  }
  else
  {
    return;
  }

  int fee_moves = 0;
  int free_moves = 0;
  int loan_moves = 0;
  std::vector<const TransferRecord*> deals;
  for (auto it = records.rbegin(); it != records.rend(); ++it)
  {
    const std::int32_t day = dayOrdinal(it->date);
    if (day < first) break;
    if (day > last) continue;
    switch (it->kind)
    {
      case TransferKind::Permanent:
        ++fee_moves;
        deals.push_back(&*it);
        break;
      case TransferKind::Free:
      case TransferKind::PreContract:
        ++free_moves;
        break;
      case TransferKind::Loan:
        ++loan_moves;
        break;
      default:
        break;
    }
  }
  const int total = fee_moves + free_moves + loan_moves;
  if (total == 0) return;
  std::ranges::sort(deals, [](const TransferRecord* a, const TransferRecord* b)
                    { return a->fee > b->fee; });
  std::string lines;
  std::size_t listed = 0;
  for (const TransferRecord* deal : deals)
  {
    if (listed == TransferTuning::Market::NEWS_DIGEST_LIMIT) break;
    const auto player = gamedata->getPlayer(deal->player_id);
    if (!player) continue;
    if (!lines.empty()) lines += '\n';
    lines += player->get().getName() + ": " + teamName(deal->from_team) +
             " -> " + teamName(deal->to_team) + " (" + formatMoney(deal->fee) +
             ")";
    ++listed;
  }
  post(date, InboxCategory::Transfer, "INBOX_TRANSFER_NEWS_TITLE",
       "INBOX_TRANSFER_NEWS_BODY",
       {std::to_string(total), std::to_string(fee_moves),
        std::to_string(free_moves), std::to_string(loan_moves),
        lines.empty() ? std::string("@INBOX_TRANSFER_NEWS_NONE") : lines},
       std::nullopt, std::nullopt);
}

// ---------------------------------------------------------------------------
// AI
// ---------------------------------------------------------------------------

std::optional<TransferMarket::AiNeed> TransferMarket::assessNeed(
    const std::vector<std::pair<double, PlayerID>>& ranked) const
{
  using M = TransferTuning::Market;
  struct Group
  {
    PlayerRole group;
    std::size_t starters; /*!< Slots in a typical XI. */
    std::size_t depth;    /*!< Squad players wanted. */
  };
  static constexpr std::array<Group, 7> GROUPS = {{{PlayerRole::GK, 1, 3},
                                                   {PlayerRole::CB, 2, 4},
                                                   {PlayerRole::LB, 1, 2},
                                                   {PlayerRole::RB, 1, 2},
                                                   {PlayerRole::CM, 3, 5},
                                                   {PlayerRole::LW, 2, 3},
                                                   {PlayerRole::ST, 1, 3}}};
  if (ranked.size() >= M::AI_MAX_SQUAD + 2) return std::nullopt;
  const float team_level = squadLevel(ranked);
  std::optional<AiNeed> need;
  float worst_deficit = static_cast<float>(M::UPGRADE_DEFICIT);
  for (const Group& group : GROUPS)
  {
    std::vector<double> overalls;
    for (const auto& [overall, player_id] : ranked)
    {
      // Loanees count; players leaving on a pre-contract do not.
      const auto player = gamedata->getPlayer(player_id);
      if (player && positionGroup(player->get().getRole()) == group.group &&
          !pre_contracts.contains(player_id))
        overalls.push_back(overall);
    }
    if (overalls.size() < group.depth)
      return AiNeed{group.group, team_level - M::SHORTAGE_LEVEL_MARGIN};
    // overalls are sorted best first because ranked is.
    const auto weakest_starter =
        static_cast<float>(overalls[group.starters - 1]);
    const float deficit = team_level - weakest_starter;
    if (deficit > worst_deficit)
    {
      worst_deficit = deficit;
      need = AiNeed{group.group, weakest_starter + M::UPGRADE_MARGIN};
    }
  }
  return need;
}

void TransferMarket::listLoanProspects(TeamID club_id)
{
  const auto club = gamedata->getTeam(club_id);
  if (!club || club_id == FREE_AGENTS_TEAM_ID) return;
  const auto ranked = rankedSquad(*gamedata, club->get());
  if (ranked.size() > TransferTuning::Market::AI_TARGET_SQUAD)
    listLoanProspects(ranked);
}

void TransferMarket::listLoanProspects(
    const std::vector<std::pair<double, PlayerID>>& ranked)
{
  using M = TransferTuning::Market;
  int listed = 0;
  for (const auto& [overall, player_id] : ranked)
  {
    if (isLoanListed(player_id)) ++listed;
  }
  for (std::size_t rank = M::LOAN_OUT_MIN_RANK;
       rank < ranked.size() && listed < M::MAX_LOAN_LISTED_PER_CLUB; ++rank)
  {
    const PlayerID player_id = ranked[rank].second;
    const auto player = gamedata->getPlayer(player_id);
    if (!player ||
        player->get().getAge() > TransferTuning::Loan::PROSPECT_MAX_AGE ||
        !canBeTraded(player_id) || isLoanListed(player_id) ||
        player->get().getTransferStatus() == TransferStatus::Listed)
      continue;
    setLoanListed(player_id, true);
    ++listed;
  }
}

bool TransferMarket::aiShedSurplus(
    TeamID club_id, const std::vector<std::pair<double, PlayerID>>& ranked,
    const GameDateValue& date, TeamID managed_team_id, WorldRng& rng)
{
  using M = TransferTuning::Market;
  if (ranked.size() > M::AI_TARGET_SQUAD + 2)
  {
    // A bloated squad lets any surplus senior go; prospects are loaned.
    const int min_age = ranked.size() > M::AI_MAX_SQUAD
                            ? M::SURPLUS_RELEASE_MIN_AGE
                            : M::RELEASE_MIN_AGE;
    for (std::size_t rank = ranked.size(); rank-- > M::RELEASE_MIN_RANK;)
    {
      const PlayerID player_id = ranked[rank].second;
      const auto player = gamedata->getPlayer(player_id);
      if (player && player->get().getAge() >= min_age &&
          player->get().getContractYears() > 0 && canBeTraded(player_id) &&
          releasePlayer(player_id, date, managed_team_id))
        return true;
    }
  }
  if (ranked.size() <= M::AI_TARGET_SQUAD) return false;
  listLoanProspects(ranked);
  // Parent clubs place their prospects where they will play. [P]
  const auto& clubs = gamedata->getTeamsVector();
  for (const auto& [overall, player_id] : ranked)
  {
    if (!isLoanListed(player_id) || clubs.empty()) continue;
    const Player& player = gamedata->getPlayer(player_id)->get();
    const SquadRole role = world.squadRole(player_id);
    for (int attempt = 0; attempt < M::LOAN_PLACEMENT_TRIES; ++attempt)
    {
      const Team& borrower = clubs[static_cast<std::size_t>(rng.uniformInt(
                                       0, static_cast<int>(clubs.size()) - 1))]
                                 .get();
      const TeamID borrower_id = borrower.getId();
      if (borrower_id == club_id || borrower_id == managed_team_id ||
          borrower.getPlayerIDs().size() >= M::AI_MAX_SQUAD)
        continue;
      const auto parent = gamedata->getTeam(club_id);
      std::vector<Reason> reasons;
      if (!playerAcceptsLoan(
              role, projectedRole(player_id, borrower_id),
              player.getTraits().ambition,
              parent->get().getReputation() - borrower.getReputation(),
              reasons) ||
          !loanWithinLimits(player_id, club_id, borrower_id))
        continue;
      LoanTerms terms;
      terms.wage_share = TransferTuning::Loan::LISTED_WAGE_SHARE;
      terms.duration =
          rng.chance(0.25) ? LoanDuration::SixMonths : LoanDuration::SeasonEnd;
      const std::int64_t room = borrower.getFinances().getWageBudget() -
                                weeklyPayroll(*gamedata, borrower);
      if (static_cast<std::int64_t>(player.getWage()) * terms.wage_share / 100 >
          room)
        continue;
      if (startLoan(player_id, borrower_id, terms, date, managed_team_id))
        return true;
    }
    break;
  }
  return false;
}

bool TransferMarket::aiSignFreeAgent(TeamID club_id, const AiNeed& need,
                                     std::int64_t wage_room,
                                     const GameDateValue& date,
                                     TeamID managed_team_id)
{
  const StatsConfig& config = gamedata->getStatsConfig();
  std::vector<std::pair<double, PlayerID>> free_agents;
  for (const auto& reference : gamedata->getPlayersForTeam(FREE_AGENTS_TEAM_ID))
  {
    const Player& player = reference.get();
    const double overall = player.getOverall(config);
    if (positionGroup(player.getRole()) == need.group &&
        overall >= static_cast<double>(need.min_overall))
      free_agents.emplace_back(overall, player.getId());
  }
  std::ranges::sort(free_agents, std::greater<>{});
  const std::size_t checks = std::min(free_agents.size(), AI_FREE_AGENT_CHECKS);
  for (std::size_t i = 0; i < checks; ++i)
  {
    const PlayerID player_id = free_agents[i].second;
    const PlayerContext context =
        playerContext(player_id, club_id, ContractKind::FreeAgent);
    const ContractOffer offer = demandedOffer(contractDemand(context));
    if (static_cast<std::int64_t>(offer.weekly_wage) > wage_room ||
        !evaluateContract(context, offer, 0).accepted)
      continue;
    Deal deal;
    deal.player_id = player_id;
    deal.buyer_id = club_id;
    deal.kind = TransferKind::Free;
    deal.contract = offer;
    if (completeTransfer(deal, date, managed_team_id)) return true;
  }
  return false;
}

bool TransferMarket::aiTakeLoan(TeamID club_id, const AiNeed& need,
                                std::int64_t wage_room,
                                const GameDateValue& date,
                                TeamID managed_team_id, WorldRng& rng)
{
  using L = TransferTuning::Loan;
  const auto club = gamedata->getTeam(club_id);
  const StatsConfig& config = gamedata->getStatsConfig();
  std::optional<PlayerID> best;
  double best_overall = static_cast<double>(
      need.min_overall - TransferTuning::Market::LOAN_LEVEL_SLACK);
  for (const auto& [player_id, extras] : player_flags)
  {
    if (!extras.loan_listed) continue;
    const auto player = gamedata->getPlayer(player_id);
    if (!player) continue;
    const Player& p = player->get();
    const TeamID parent = p.getTeamId();
    const double overall = p.getOverall(config);
    if (parent == club_id || parent == FREE_AGENTS_TEAM_ID ||
        positionGroup(p.getRole()) != need.group || overall < best_overall ||
        (best && overall == best_overall && player_id > *best) ||
        !canBeTraded(player_id))
      continue;
    const auto parent_team = gamedata->getTeam(parent);
    const int gap = parent_team ? parent_team->get().getReputation() -
                                      club->get().getReputation()
                                : 0;
    std::vector<Reason> reasons;
    if (!playerAcceptsLoan(world.squadRole(player_id),
                           projectedRole(player_id, club_id),
                           p.getTraits().ambition, gap, reasons) ||
        !loanWithinLimits(player_id, parent, club_id))
      continue;
    best = player_id;
    best_overall = overall;
  }
  if (!best) return false;
  const Player& loanee = gamedata->getPlayer(*best)->get();
  const TeamID parent = loanee.getTeamId();
  if (parent == managed_team_id)
  {
    // The managed club decides on its own players.
    const bool pending = std::ranges::any_of(
        incoming, [&](const IncomingOffer& offer)
        { return offer.player_id == *best && offer.buyer == club_id; });
    if (!pending)
    {
      IncomingOffer offer;
      offer.player_id = *best;
      offer.buyer = club_id;
      offer.loan = true;
      offer.loan_terms.wage_share = L::LISTED_WAGE_SHARE;
      offer.loan_terms.duration =
          rng.chance(0.3) ? LoanDuration::SixMonths : LoanDuration::SeasonEnd;
      offer.created = date;
      offer.expires = date + static_cast<std::size_t>(
                                 TransferTuning::Offer::INCOMING_OFFER_DAYS);
      addIncomingOffer(offer);
      post(date, InboxCategory::Transfer, "INBOX_LOAN_OFFER_TITLE",
           "INBOX_LOAN_OFFER_BODY",
           {loanee.getName(), club->get().getName(),
            std::to_string(offer.loan_terms.wage_share)},
           *best, club_id);
    }
    return false;
  }
  LoanTerms terms;
  terms.duration =
      rng.chance(0.25) ? LoanDuration::SixMonths : LoanDuration::SeasonEnd;
  const auto parent_team = gamedata->getTeam(parent);
  const bool bigger_borrower =
      parent_team &&
      club->get().getReputation() > parent_team->get().getReputation();
  terms.wage_share = static_cast<std::uint8_t>(
      std::min(100, L::LISTED_WAGE_SHARE +
                        (bigger_borrower ? BIGGER_BORROWER_EXTRA_SHARE : 0)));
  const std::int64_t borrower_wage =
      static_cast<std::int64_t>(loanee.getWage()) * terms.wage_share / 100;
  if (borrower_wage > wage_room) return false;
  return startLoan(*best, club_id, terms, date, managed_team_id);
}

bool TransferMarket::aiBuy(
    TeamID club_id, const AiNeed& need, std::int64_t wage_room,
    const std::unordered_map<PlayerID, TransferListing>& listings,
    const GameDateValue& date, TeamID managed_team_id, WorldRng& rng)
{
  using M = TransferTuning::Market;
  const auto club = gamedata->getTeam(club_id);
  const std::int64_t budget =
      availableBudget(club->get(), committedPayables(club_id, date));
  if (budget <= 0) return false;
  const StatsConfig& config = gamedata->getStatsConfig();
  const auto eligible = [&](const Player& player)
  {
    const TeamID owner = player.getTeamId();
    return owner != club_id && owner != managed_team_id &&
           owner != FREE_AGENTS_TEAM_ID &&
           positionGroup(player.getRole()) == need.group &&
           canBeTraded(player.getId()) &&
           player.getOverall(config) >= static_cast<double>(need.min_overall);
  };

  // Listed players plus a sample of other clubs' players.
  std::vector<std::pair<double, PlayerID>> candidates;
  for (const auto& [player_id, listing] : listings)
  {
    const auto player = gamedata->getPlayer(player_id);
    if (player && listing.seller_team_id == player->get().getTeamId() &&
        eligible(player->get()))
      candidates.emplace_back(player->get().getOverall(config), player_id);
  }
  const auto& everyone = gamedata->getPlayersVector();
  for (std::size_t sample = 0; sample < M::AI_BUY_SAMPLE && !everyone.empty();
       ++sample)
  {
    const Player& player =
        everyone[static_cast<std::size_t>(
                     rng.uniformInt(0, static_cast<int>(everyone.size()) - 1))]
            .get();
    if (eligible(player) && !listings.contains(player.getId()))
      candidates.emplace_back(player.getOverall(config), player.getId());
  }
  // Cheapest route to the level first: the weakest player that fixes the
  // need is the most affordable.
  std::ranges::sort(candidates);
  candidates.erase(std::ranges::unique(candidates).begin(), candidates.end());
  std::size_t tried = 0;
  for (const auto& [overall, player_id] : candidates)
  {
    if (tried++ == M::AI_BUY_ATTEMPTS) break;
    const auto listing = listings.find(player_id);
    const std::uint32_t listing_price =
        listing == listings.end() ? 0 : listing->second.asking_price;
    const SaleContext sale =
        saleContext(player_id, club_id, date, listing_price);
    const Valuation valuation = valueForSale(sale);
    if (valuation.not_for_sale ||
        static_cast<double>(valuation.asking_fee) >
            static_cast<double>(sale.market_value) * M::AI_MAX_VALUE_MULTIPLE)
      continue;
    Deal deal;
    deal.player_id = player_id;
    deal.buyer_id = club_id;
    deal.kind = TransferKind::Permanent;
    deal.terms = aiOfferTerms(valuation.asking_fee);
    const PlayerContext context =
        playerContext(player_id, club_id, ContractKind::Transfer);
    deal.contract = demandedOffer(contractDemand(context));
    const std::int64_t cash =
        static_cast<std::int64_t>(upfrontAmount(deal.terms)) +
        static_cast<std::int64_t>(agentFee(deal));
    if (cash > budget ||
        static_cast<std::int64_t>(deal.contract.weekly_wage) > wage_room ||
        !evaluateContract(context, deal.contract, 0).accepted ||
        evaluateOffer(sale, deal.terms, 0).decision !=
            ClubResponse::Decision::Accept)
      continue;
    return completeTransfer(deal, date, managed_team_id);
  }
  return false;
}

bool TransferMarket::runAiClub(
    TeamID club_id,
    const std::unordered_map<PlayerID, TransferListing>& listings,
    const GameDateValue& date, TeamID managed_team_id, WorldRng& rng,
    bool free_agents_only)
{
  using M = TransferTuning::Market;
  const auto club = gamedata->getTeam(club_id);
  if (!club || club_id == FREE_AGENTS_TEAM_ID || club_id == managed_team_id)
    return false;
  const auto ranked = rankedSquad(*gamedata, club->get());
  if (!free_agents_only &&
      aiShedSurplus(club_id, ranked, date, managed_team_id, rng))
    return true;
  const auto need = assessNeed(ranked);
  if (!need) return false;
  const Finances& finances = club->get().getFinances();
  const std::int64_t wage_room =
      finances.getWageBudget() - weeklyPayroll(*gamedata, club->get());
  if (aiSignFreeAgent(club_id, *need, wage_room, date, managed_team_id))
    return true;
  if (free_agents_only) return false;
  // Most clubs look for a loan before paying a fee. [P]
  if (rng.chance(M::LOAN_BEFORE_FEE_CHANCE) &&
      aiTakeLoan(club_id, *need, wage_room, date, managed_team_id, rng))
    return true;
  return aiBuy(club_id, *need, wage_room, listings, date, managed_team_id, rng);
}

void TransferMarket::runAiPreContracts(const GameDateValue& date,
                                       TeamID managed_team_id, WorldRng& rng,
                                       int attempts)
{
  if (!canSignPreContract(1, date) || attempts <= 0) return;
  const StatsConfig& config = gamedata->getStatsConfig();
  std::vector<PlayerID> candidates;
  for (const auto& reference : gamedata->getPlayersVector())
  {
    const Player& player = reference.get();
    if (player.getContractYears() == 1 &&
        player.getTeamId() != FREE_AGENTS_TEAM_ID &&
        player.getAge() <= PRE_CONTRACT_MAX_AGE &&
        !pre_contracts.contains(player.getId()))
      candidates.push_back(player.getId());
  }
  std::vector<TeamID> clubs;
  for (const auto& team : gamedata->getTeamsVector())
  {
    const TeamID id = team.get().getId();
    if (id != FREE_AGENTS_TEAM_ID && id != managed_team_id) clubs.push_back(id);
  }
  if (candidates.empty() || clubs.empty()) return;
  std::ranges::sort(candidates);
  std::ranges::sort(clubs);

  for (int attempt = 0; attempt < attempts; ++attempt)
  {
    if (!rng.chance(PRE_CONTRACT_SUCCESS)) continue;
    const TeamID club_id = clubs[static_cast<std::size_t>(
        rng.uniformInt(0, static_cast<int>(clubs.size()) - 1))];
    const auto club = gamedata->getTeam(club_id);
    if (!club) continue;
    const float team_level = squadLevel(rankedSquad(*gamedata, club->get()));
    std::optional<PlayerID> best;
    double best_overall = 0.0;
    for (std::size_t sample = 0; sample < PRE_CONTRACT_SAMPLE; ++sample)
    {
      const PlayerID player_id = candidates[static_cast<std::size_t>(
          rng.uniformInt(0, static_cast<int>(candidates.size()) - 1))];
      const auto player = gamedata->getPlayer(player_id);
      if (!player || player->get().getTeamId() == club_id ||
          pre_contracts.contains(player_id))
        continue;
      const double overall = player->get().getOverall(config);
      if (overall <
              static_cast<double>(team_level - PRE_CONTRACT_LEVEL_MARGIN) ||
          overall <= best_overall ||
          !wouldJoin(player_id, club_id, ContractKind::PreContract))
        continue;
      best = player_id;
      best_overall = overall;
    }
    if (!best) continue;
    const PlayerContext context =
        playerContext(*best, club_id, ContractKind::PreContract);
    agreePreContract(*best, club_id, demandedOffer(contractDemand(context)),
                     date, managed_team_id);
  }
}

void TransferMarket::runAiApproach(TeamID club_id, const GameDateValue& date,
                                   TeamID managed_team_id, WorldRng& rng)
{
  if (managed_team_id == FREE_AGENTS_TEAM_ID || club_id == managed_team_id ||
      !rng.chance(TransferTuning::Market::MANAGED_APPROACH_CHANCE))
    return;
  const auto club = gamedata->getTeam(club_id);
  if (!club) return;
  const auto need = assessNeed(rankedSquad(*gamedata, club->get()));
  if (!need) return;
  const StatsConfig& config = gamedata->getStatsConfig();
  std::optional<PlayerID> best;
  double best_overall = static_cast<double>(need->min_overall);
  for (const auto& reference : gamedata->getPlayersForTeam(managed_team_id))
  {
    const Player& player = reference.get();
    const PlayerID player_id = player.getId();
    const double overall = player.getOverall(config);
    if (positionGroup(player.getRole()) != need->group || overall < best_overall ||
        (best && overall == best_overall && player_id > *best) ||
        player.getTransferStatus() == TransferStatus::Listed ||
        !canBeTraded(player_id) ||
        std::ranges::any_of(
            incoming, [&](const IncomingOffer& offer)
            { return offer.player_id == player_id; }) ||
        !wouldJoin(player_id, club_id, ContractKind::Transfer))
      continue;
    best = player_id;
    best_overall = overall;
  }
  if (!best) return;
  const Valuation valuation =
      valueForSale(saleContext(*best, club_id, date, 0));
  const std::int64_t budget =
      availableBudget(club->get(), committedPayables(club_id, date));
  const double ceiling =
      std::min(static_cast<double>(budget),
               static_cast<double>(valuation.asking_fee) *
                   static_cast<double>(rng.uniform(1.0f, 1.15f)));
  if (ceiling < static_cast<double>(valuation.asking_fee) *
                    static_cast<double>(TransferTuning::Offer::REJECT_SHARE))
    return;
  IncomingOffer offer;
  offer.player_id = *best;
  offer.buyer = club_id;
  offer.max_fee = static_cast<std::uint32_t>(ceiling);
  offer.terms = aiOfferTerms(static_cast<std::uint32_t>(
      std::round(ceiling * static_cast<double>(rng.uniform(0.75f, 0.95f)) /
                 10'000.0) *
      10'000.0));
  offer.created = date;
  offer.expires = date + static_cast<std::size_t>(
                             TransferTuning::Offer::INCOMING_OFFER_DAYS);
  addIncomingOffer(offer);
  world.onTransferBid(date, *best, club_id, offer.terms.fee, managed_team_id);
}

// ---------------------------------------------------------------------------
// Offers and flags
// ---------------------------------------------------------------------------

const IncomingOffer* TransferMarket::findIncomingOffer(
    std::uint32_t offer_id) const
{
  const auto found = std::ranges::find(incoming, offer_id, &IncomingOffer::id);
  return found == incoming.end() ? nullptr : &*found;
}

std::uint32_t TransferMarket::addIncomingOffer(IncomingOffer offer)
{
  offer.id = next_id++;
  incoming.push_back(offer);
  return offer.id;
}

bool TransferMarket::updateIncomingOffer(const IncomingOffer& offer)
{
  const auto found = std::ranges::find(incoming, offer.id, &IncomingOffer::id);
  if (found == incoming.end()) return false;
  *found = offer;
  return true;
}

bool TransferMarket::removeIncomingOffer(std::uint32_t offer_id)
{
  return std::erase_if(incoming, [offer_id](const IncomingOffer& offer)
                       { return offer.id == offer_id; }) > 0;
}

const Negotiation* TransferMarket::findNegotiation(PlayerID player_id) const
{
  const auto found = negotiations.find(player_id);
  return found == negotiations.end() ? nullptr : &found->second;
}

void TransferMarket::setNegotiation(const Negotiation& negotiation)
{
  negotiations[negotiation.player_id] = negotiation;
}

void TransferMarket::removeNegotiation(PlayerID player_id)
{
  negotiations.erase(player_id);
}

const PlayerMarketFlags* TransferMarket::flags(PlayerID player_id) const
{
  const auto found = player_flags.find(player_id);
  return found == player_flags.end() ? nullptr : &found->second;
}

void TransferMarket::setLoanListed(PlayerID player_id, bool listed)
{
  mutableFlags(player_id).loan_listed = listed;
  pruneFlags(player_id);
}

bool TransferMarket::isLoanListed(PlayerID player_id) const
{
  const PlayerMarketFlags* extras = flags(player_id);
  return extras && extras->loan_listed;
}

const LoanDeal* TransferMarket::findLoan(PlayerID player_id) const
{
  const auto found = active_loans.find(player_id);
  return found == active_loans.end() ? nullptr : &found->second;
}

const PreContractDeal* TransferMarket::findPreContract(PlayerID player_id) const
{
  const auto found = pre_contracts.find(player_id);
  return found == pre_contracts.end() ? nullptr : &found->second;
}

std::vector<TransferRecord> TransferMarket::historyFor(PlayerID player_id) const
{
  std::vector<TransferRecord> moves;
  for (const TransferRecord& record : records)
  {
    if (record.player_id == player_id) moves.push_back(record);
  }
  return moves;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

void TransferMarket::load(const std::shared_ptr<DatabaseConnection>& db_conn)
{
  TransferRepository repository(db_conn);
  records = repository.loadHistory();
  persisted_records = records.size();
  pending_obligations = repository.loadObligations();
  active_loans.clear();
  for (const LoanDeal& loan : repository.loadLoans())
    active_loans[loan.player_id] = loan;
  pre_contracts.clear();
  for (const PreContractDeal& deal : repository.loadPreContracts())
    pre_contracts[deal.player_id] = deal;
  player_flags = repository.loadFlags();
  incoming.clear();
  negotiations.clear();
  repository.loadOffers(incoming, negotiations);
  next_id = 1;
  for (const TransferObligation& obligation : pending_obligations)
    next_id = std::max(next_id, obligation.id + 1);
  for (const IncomingOffer& offer : incoming)
    next_id = std::max(next_id, offer.id + 1);
  payables_dirty = true;
}

void TransferMarket::save(
    const std::shared_ptr<DatabaseConnection>& db_conn) const
{
  TransferRepository repository(db_conn);
  repository.appendHistory(
      persisted_records,
      std::span<const TransferRecord>(records).subspan(persisted_records));
  repository.replaceObligations(pending_obligations);
  repository.replaceLoans(active_loans);
  repository.replacePreContracts(pre_contracts);
  repository.replaceFlags(player_flags);
  repository.replaceOffers(incoming, negotiations);
}

void TransferMarket::onSaved() { persisted_records = records.size(); }
