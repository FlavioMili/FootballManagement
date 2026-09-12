// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// Talks over the AI clubs' offers for the managed club's players: what the
// negotiation shows, the club's answers (accept, reject, counter, name a
// price, not for sale), the buyers' answers on later days and bids on
// listed players turned into offers the manager can negotiate.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "model/buyer_negotiation.h"
#include "model/inbox.h"
#include "model/interactions.h"
#include "model/transfer_market.h"
#include "model/transfer_tuning.h"
#include "model/world_rng.h"
#include "model/world_simulation.h"

namespace
{
using BuyerNegotiation::Move;
using TransferNegotiation::OfferTerms;

constexpr std::uint64_t REPLY_DELAY_KEY = 0x0FFE01;
constexpr std::uint64_t REPLY_PLAN_KEY = 0x0FFE02;
constexpr std::uint64_t PERSONAL_TERMS_KEY = 0x0FFE03;
constexpr std::uint64_t LISTING_BID_KEY = 0x0FFE04;

void notify(WorldSimulation& world, const GameDateValue& date,
            const char* title, const char* body,
            std::vector<std::string> args, PlayerID player_id, TeamID club)
{
  InboxMessage message;
  message.date = date;
  message.category = InboxCategory::Transfer;
  message.title_key = title;
  message.body_key = body;
  message.args = std::move(args);
  message.player_id = player_id;
  message.team_id = club;
  world.getInbox().add(std::move(message));
}

const char* replyBody(Move move)
{
  switch (move)
  {
    case Move::FinalOffer:
      return "INBOX_OFFER_FINAL_BODY";
    case Move::Restated:
      return "INBOX_OFFER_RESTATED_BODY";
    default:
      break;
  }
  return "INBOX_OFFER_REPLY_BODY";
}
}  // namespace

// ========== The talks ==========

BuyerNegotiation::PlayerStance GameController::playerStance(
    PlayerID pid, TeamID buyer_id) const
{
  const auto player = gamedata->getPlayer(pid);
  const auto buyer = gamedata->getTeam(buyer_id);
  if (!game || !player || !buyer) return BuyerNegotiation::PlayerStance::Open;
  const Player& who = player->get();
  const auto club = gamedata->getTeam(who.getTeamId());
  BuyerNegotiation::StanceFacts facts;
  const PlayerRelation* relation =
      game->getWorld().getInteractions().relation(pid);
  facts.transfer_request =
      relation != nullptr && relation->request == TalkRequest::Transfer;
  facts.would_join = game->getTransfers().wouldJoin(
      pid, buyer_id, TransferNegotiation::ContractKind::Transfer);
  facts.reputation_gap =
      static_cast<int>(buyer->get().getReputation()) -
      (club ? static_cast<int>(club->get().getReputation()) : 0);
  facts.ambition = who.getTraits().ambition;
  facts.loyalty = who.getTraits().loyalty;
  facts.morale = who.getDynamics().morale;
  return BuyerNegotiation::stanceFor(facts);
}

std::optional<GameController::IncomingOfferView>
GameController::getIncomingOfferView(std::uint32_t offer_id) const
{
  if (!game) return std::nullopt;
  const TransferMarket& market = game->getTransfers();
  const IncomingOffer* offer = market.findIncomingOffer(offer_id);
  if (offer == nullptr || offer->loan) return std::nullopt;
  const auto player = gamedata->getPlayer(offer->player_id);
  const auto buyer = gamedata->getTeam(offer->buyer);
  if (!player || !buyer) return std::nullopt;
  IncomingOfferView view;
  view.offer_id = offer->id;
  view.player_id = offer->player_id;
  view.buyer = offer->buyer;
  view.player_name = player->get().getName();
  view.buyer_name = buyer->get().getName();
  view.age = player->get().getAge();
  view.role = getSquadRole(offer->player_id);
  view.market_value = getPlayerMarketValue(offer->player_id);
  view.asking_price = listingPrice(offer->player_id);
  view.contract_years = player->get().getContractYears();
  view.stance = playerStance(offer->player_id, offer->buyer);
  view.rivals = market.rivalBids(*offer);
  view.status = offer->status;
  view.expires = offer->expires;
  view.respond_on = offer->respond_on;
  view.terms = offer->terms;
  view.asked = offer->asked;
  view.history = offer->history;
  view.final_offer = !offer->history.empty() &&
                     offer->history.back().move == Move::FinalOffer;
  const TransferNegotiation::WindowInfo window =
      TransferNegotiation::windowInfo(game->getCurrentDate());
  view.window_open = window.open;
  view.days_to_deadline = window.open ? window.days_to_deadline : -1;
  return view;
}

// ========== The club's answers ==========

GameController::OfferOutcome GameController::settleIncomingOffer(
    std::uint32_t offer_id)
{
  if (!game || !isTransferWindowOpen()) return OfferOutcome::Failed;
  TransferMarket& market = game->getTransfers();
  const IncomingOffer* found = market.findIncomingOffer(offer_id);
  // While the buyer considers a counter its bid is not on the table.
  if (found == nullptr || found->status != OfferStatus::AwaitingClub)
    return OfferOutcome::Failed;
  const IncomingOffer offer = *found;
  const TeamID managed = game->getManagedTeamId();
  const GameDateValue today = game->getCurrentDate();
  const auto player = gamedata->getPlayer(offer.player_id);
  const auto buyer = gamedata->getTeam(offer.buyer);
  if (!player || !buyer || player->get().getTeamId() != managed ||
      !market.canBeTraded(offer.player_id))
  {
    market.removeIncomingOffer(offer_id);
    return OfferOutcome::Failed;
  }
  bool completed = false;
  if (offer.loan)
  {
    completed = market.startLoan(offer.player_id, offer.buyer, offer.loan_terms,
                                 today, managed);
  }
  else
  {
    // With the fee agreed the buyer talks personal terms with the player;
    // the rare refusal depends on how keen he is on the move.
    const double roll = WorldRng::hashUniform(
        gamedata->getWorldSeed(), RngDomain::Transfers, offer.id,
        mixHash(offer.player_id, PERSONAL_TERMS_KEY));
    if (roll < BuyerNegotiation::termsRefusalChance(
                   playerStance(offer.player_id, offer.buyer)))
    {
      market.removeIncomingOffer(offer_id);
      market.closeTalks(offer.player_id, offer.buyer, today);
      notify(game->getWorld(), today, "INBOX_OFFER_TERMS_REFUSED_TITLE",
             "INBOX_OFFER_TERMS_REFUSED_BODY",
             {player->get().getName(), buyer->get().getName()},
             offer.player_id, offer.buyer);
      return OfferOutcome::TermsRefused;
    }
    const auto context =
        market.playerContext(offer.player_id, offer.buyer,
                             TransferNegotiation::ContractKind::Transfer);
    TransferMarket::Deal deal;
    deal.player_id = offer.player_id;
    deal.buyer_id = offer.buyer;
    deal.terms = offer.terms;
    deal.contract = TransferNegotiation::demandedOffer(
        TransferNegotiation::contractDemand(context));
    // The bidder may have spent its budget since making the offer.
    completed =
        canPayDeal(deal) && market.completeTransfer(deal, today, managed);
  }
  market.removeIncomingOffer(offer_id);
  if (completed) purgeStaleListings();
  return completed ? OfferOutcome::Sold : OfferOutcome::Failed;
}

bool GameController::acceptIncomingOffer(std::uint32_t offer_id)
{
  return settleIncomingOffer(offer_id) == OfferOutcome::Sold;
}

bool GameController::rejectIncomingOffer(std::uint32_t offer_id)
{
  if (!game) return false;
  TransferMarket& market = game->getTransfers();
  const IncomingOffer* found = market.findIncomingOffer(offer_id);
  if (found == nullptr) return false;
  const IncomingOffer offer = *found;
  const auto player = gamedata->getPlayer(offer.player_id);
  const auto buyer = gamedata->getTeam(offer.buyer);
  const BuyerNegotiation::PlayerStance stance =
      playerStance(offer.player_id, offer.buyer);
  market.removeIncomingOffer(offer_id);
  if (offer.loan || !player || !buyer ||
      player->get().getTeamId() != game->getManagedTeamId())
    return true;
  market.closeTalks(offer.player_id, offer.buyer, game->getCurrentDate());
  // A player who wanted the move takes a big bid turned down badly.
  const BuyerNegotiation::RejectionEffect effect =
      BuyerNegotiation::rejectionEffect(
          stance,
          BuyerNegotiation::isBigBid(offer.terms, player->get().getAge(),
                                     getPlayerMarketValue(offer.player_id)),
          player->get().getTraits().ambition);
  WorldSimulation& world = game->getWorld();
  world.getInteractions().onBidRejected(
      game->getCurrentDate(), offer.player_id, buyer->get().getName(),
      effect.morale_delta, effect.trust_delta, effect.transfer_request,
      world.getInbox());
  return true;
}

GameController::OfferOutcome GameController::counterIncomingOffer(
    std::uint32_t offer_id, const OfferTerms& terms)
{
  return sendCounter(offer_id, terms, false);
}

GameController::OfferOutcome GameController::nameAskingPrice(
    std::uint32_t offer_id, uint32_t price)
{
  if (!game || price == 0) return OfferOutcome::Failed;
  const IncomingOffer* found =
      game->getTransfers().findIncomingOffer(offer_id);
  if (found == nullptr || found->loan ||
      found->status != OfferStatus::AwaitingClub)
    return OfferOutcome::Failed;
  // The price goes on the transfer list for every club to see; this
  // bidder gets it as a take-it-or-leave-it figure.
  listPlayerForTransfer(found->player_id, price);
  OfferTerms flat;
  flat.fee = price;
  return sendCounter(offer_id, flat, true);
}

bool GameController::declareNotForSale(std::uint32_t offer_id)
{
  if (!game) return false;
  TransferMarket& market = game->getTransfers();
  const IncomingOffer* found = market.findIncomingOffer(offer_id);
  if (found == nullptr || found->loan) return false;
  const PlayerID pid = found->player_id;
  // The bidder hears it first (and the player reacts to that bid); the
  // other clubs are simply told.
  rejectIncomingOffer(offer_id);
  std::vector<std::uint32_t> others;
  for (const IncomingOffer& offer : market.incomingOffers())
    if (offer.player_id == pid && !offer.loan) others.push_back(offer.id);
  for (const std::uint32_t other : others) market.removeIncomingOffer(other);
  if (isPlayerListed(pid)) removePlayerFromTransfer(pid);
  const GameDateValue today = game->getCurrentDate();
  const TransferNegotiation::WindowInfo window =
      TransferNegotiation::windowInfo(today);
  market.setNotForSale(
      pid, today + static_cast<std::size_t>(
                       window.open ? std::max(window.days_to_deadline, 0) : 0));
  return true;
}

GameController::OfferOutcome GameController::sendCounter(
    std::uint32_t offer_id, const OfferTerms& terms, bool firm)
{
  if (!game || !isTransferWindowOpen() ||
      !TransferNegotiation::isValid(terms) || terms.fee == 0)
    return OfferOutcome::Failed;
  TransferMarket& market = game->getTransfers();
  const IncomingOffer* found = market.findIncomingOffer(offer_id);
  if (found == nullptr || found->loan ||
      found->status != OfferStatus::AwaitingClub)
    return OfferOutcome::Failed;
  IncomingOffer offer = *found;
  const GameDateValue today = game->getCurrentDate();
  const TransferNegotiation::WindowInfo window =
      TransferNegotiation::windowInfo(today);
  const int delay = BuyerNegotiation::replyDelay(
      WorldRng::hashUniform(gamedata->getWorldSeed(), RngDomain::Transfers,
                            offer.id, mixHash(offer.round, REPLY_DELAY_KEY)),
      window.days_to_deadline);
  offer.asked = terms;
  offer.firm = firm;
  offer.status = OfferStatus::AwaitingBuyer;
  offer.respond_on = today + static_cast<std::size_t>(delay);
  offer.history.push_back(
      {today, firm ? Move::AskingPrice : Move::Counter, terms});
  market.updateIncomingOffer(offer);
  // Near the deadline there is no time to sleep on it.
  if (delay > 0) return OfferOutcome::AwaitingReply;
  return answerCounter(offer_id);
}

// ========== The buyer's answers ==========

void GameController::processOfferReplies()
{
  absorbListingBids();
  if (!game || !hasSelectedTeam()) return;
  for (const std::uint32_t offer_id :
       game->getTransfers().dueOfferReplies(game->getCurrentDate()))
    answerCounter(offer_id);
}

GameController::OfferOutcome GameController::answerCounter(
    std::uint32_t offer_id)
{
  TransferMarket& market = game->getTransfers();
  const IncomingOffer* found = market.findIncomingOffer(offer_id);
  if (found == nullptr || found->status != OfferStatus::AwaitingBuyer)
    return OfferOutcome::Failed;
  IncomingOffer offer = *found;
  const GameDateValue today = game->getCurrentDate();
  const auto player = gamedata->getPlayer(offer.player_id);
  const auto buyer = gamedata->getTeam(offer.buyer);
  if (!player || !buyer ||
      player->get().getTeamId() != game->getManagedTeamId())
  {
    market.removeIncomingOffer(offer_id);
    return OfferOutcome::Failed;
  }
  const TransferNegotiation::WindowInfo window =
      TransferNegotiation::windowInfo(today);
  BuyerNegotiation::BuyerContext context;
  context.ceiling = offer.max_fee;
  context.cash = transferBudgetForTeam(offer.buyer);
  context.age = player->get().getAge();
  context.patience = offer.patience;
  context.answered = offer.round;
  context.insults = offer.insults;
  context.rivals = market.rivalBids(offer);
  context.days_to_deadline = window.open ? window.days_to_deadline : -1;
  const BuyerNegotiation::BuyerReply reply = BuyerNegotiation::respond(
      context, offer.terms, offer.asked, offer.firm,
      WorldRng::hashUniform(gamedata->getWorldSeed(), RngDomain::Transfers,
                            offer.id, mixHash(offer.round, REPLY_PLAN_KEY)));

  const std::string player_name = player->get().getName();
  const std::string buyer_name = buyer->get().getName();
  offer.status = OfferStatus::AwaitingClub;
  offer.firm = false;
  if (reply.insulted) ++offer.insults;
  switch (reply.decision)
  {
    case BuyerNegotiation::Decision::Accept:
    {
      offer.terms = reply.terms;
      offer.history.push_back({today, Move::Accepted, reply.terms});
      market.updateIncomingOffer(offer);
      const OfferOutcome outcome = settleIncomingOffer(offer_id);
      if (outcome == OfferOutcome::Failed)
        notify(game->getWorld(), today, "INBOX_OFFER_COLLAPSED_TITLE",
               "INBOX_OFFER_COLLAPSED_BODY", {player_name, buyer_name},
               offer.player_id, offer.buyer);
      return outcome;
    }
    case BuyerNegotiation::Decision::Counter:
    {
      ++offer.round;
      offer.terms = reply.terms;
      // After its final offer the next counter ends the talks.
      if (reply.move == Move::FinalOffer) offer.patience = offer.round;
      offer.history.push_back({today, reply.move, reply.terms});
      offer.expires = TransferMarket::answerDeadline(
          today, TransferTuning::Buyer::ANSWER_DAYS);
      market.updateIncomingOffer(offer);
      // The body names what shaped the answer (e.g. more instalments).
      const BuyerNegotiation::Why why =
          reply.reasons.size() > 1 ? reply.reasons[1]
          : reply.reasons.empty()  ? BuyerNegotiation::Why::Improved
                                   : reply.reasons.front();
      notify(game->getWorld(), today, "INBOX_OFFER_REPLY_TITLE",
             replyBody(reply.move),
             {player_name, buyer_name, formatMoney(reply.terms.fee),
              std::string("@") + BuyerNegotiation::whyKey(why)},
             offer.player_id, offer.buyer);
      return OfferOutcome::Countered;
    }
    case BuyerNegotiation::Decision::WalkAway:
      break;
  }
  market.removeIncomingOffer(offer_id);
  market.closeTalks(offer.player_id, offer.buyer, today);
  notify(game->getWorld(), today, "INBOX_OFFER_WITHDRAWN_TITLE",
         reply.insulted ? "INBOX_OFFER_WITHDRAWN_INSULT_BODY"
                        : "INBOX_OFFER_WITHDRAWN_BODY",
         {player_name, buyer_name}, offer.player_id, offer.buyer);
  return OfferOutcome::WalkedAway;
}

// ========== Bids on listed players ==========

bool GameController::routeListingBid(PlayerID pid, TeamID bidder_id,
                                     uint32_t bid, bool announce)
{
  if (!game || bid == 0) return false;
  TransferMarket& market = game->getTransfers();
  const GameDateValue today = game->getCurrentDate();
  const auto player = gamedata->getPlayer(pid);
  if (!player || market.isNotForSale(pid, today) ||
      market.talksClosed(pid, bidder_id, today) ||
      std::ranges::any_of(market.incomingOffers(),
                          [&](const IncomingOffer& offer)
                          {
                            return offer.player_id == pid &&
                                   offer.buyer == bidder_id;
                          }))
    return false;
  IncomingOffer offer;
  offer.player_id = pid;
  offer.buyer = bidder_id;
  offer.terms = TransferNegotiation::aiOfferTerms(bid);
  // The most the club would pay for him, never below its own bid.
  const double opening =
      BuyerNegotiation::buyerCost(offer.terms, player->get().getAge());
  offer.max_fee = static_cast<std::uint32_t>(std::max(
      opening, static_cast<double>(calculateMaxPrice(
                   pid, bidder_id, evaluateSquadNeeds(bidder_id)))));
  const TransferNegotiation::WindowInfo window =
      TransferNegotiation::windowInfo(today);
  offer.patience = BuyerNegotiation::drawPatience(
      WorldRng::hashUniform(gamedata->getWorldSeed(), RngDomain::Transfers,
                            pid, mixHash(bidder_id, LISTING_BID_KEY)),
      window.days_to_deadline);
  offer.created = today;
  offer.expires = TransferMarket::answerDeadline(
      today, TransferTuning::Offer::INCOMING_OFFER_DAYS);
  market.addIncomingOffer(std::move(offer));
  if (announce)
    game->getWorld().onTransferBid(today, pid, bidder_id, bid,
                                   game->getManagedTeamId());
  return true;
}

void GameController::absorbListingBids()
{
  if (!game || !hasSelectedTeam() || !isTransferWindowOpen()) return;
  const TeamID managed = game->getManagedTeamId();
  std::vector<PlayerID> absorbed;
  for (const auto& [pid, listing] : transfer_listings)
  {
    if (listing.seller_team_id == managed && listing.highest_bidder_id &&
        listing.highest_bid > 0)
      absorbed.push_back(pid);
  }
  std::ranges::sort(absorbed);
  for (const PlayerID pid : absorbed)
  {
    TransferListing& listing = transfer_listings.at(pid);
    const TeamID bidder = *listing.highest_bidder_id;
    const uint32_t bid = listing.highest_bid;
    listing.highest_bid = 0;
    listing.highest_bidder_id.reset();
    gamedata->saveTransferListing(listing);
    routeListingBid(pid, bidder, bid, false);
  }
}
