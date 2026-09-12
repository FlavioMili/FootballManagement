// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// U21 squad and national-team job of the GameController.

#include <algorithm>
#include <cmath>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "model/calendar.h"
#include "model/world_rng.h"
#include "model/world_tuning.h"

namespace
{
/** Oldest first-team player offered for an over-age U21 place. */
constexpr int OVERAGE_CANDIDATE_AGE = 24;
/** Youngest U18 player offered a move up to the U21s. */
constexpr int U18_CANDIDATE_AGE = 17;
constexpr size_t CALL_UP_FIXTURES = 5;
constexpr int ANNOUNCE_DAYS = 7;
}  // namespace

// ========== U21 squad ==========

std::vector<GameController::YouthPlayerView>
GameController::getReserveCandidates() const
{
  std::vector<YouthPlayerView> views;
  const auto team = managedClub();
  if (!team || !game) return views;
  const TeamID team_id = team->get().getId();
  const YouthAcademy& academy = game->getWorld().getYouth();
  const auto addView = [&](const Player& player, YouthStatus status,
                           YouthContract contract)
  {
    YouthPlayerView view;
    view.id = player.getId();
    view.name = player.getName();
    view.role = player.getRole();
    view.age = player.getAge();
    view.nationality = player.getNationality();
    view.height = player.getHeight();
    view.status = status;
    view.contract = contract;
    view.contract_years = player.getContractYears();
    view.wage = player.getWage();
    view.estimate = academy.estimate(team_id, view.id);
    view.personality_key = YouthModel::personalityKey(player.getTraits());
    view.homegrown = academy.isHomegrown(view.id);
    view.loan_listed = game->getTransfers().isLoanListed(view.id);
    views.push_back(std::move(view));
  };
  for (const auto& player_ref : gamedata->getPlayersForTeam(team_id))
  {
    const Player& player = player_ref.get();
    if (player.getAge() > OVERAGE_CANDIDATE_AGE ||
        academy.isAcademyPlayer(player.getId()) ||
        game->getTransfers().findLoan(player.getId()))
      continue;
    addView(player, YouthStatus::Graduated, YouthContract::Professional);
  }
  for (const YouthRecord* youth : academy.members(team_id, YouthStatus::Squad))
  {
    const Player& player = gamedata->getPlayers().at(youth->player_id);
    if (player.getAge() >= U18_CANDIDATE_AGE)
      addView(player, YouthStatus::Squad, youth->contract);
  }
  std::ranges::sort(views,
                    [](const YouthPlayerView& a, const YouthPlayerView& b)
                    {
                      if (a.age != b.age) return a.age < b.age;
                      return a.id < b.id;
                    });
  return views;
}

GameController::ReserveOverview GameController::getReserveOverview() const
{
  ReserveOverview overview;
  const auto team = managedClub();
  if (!team || !game) return overview;
  const TeamID team_id = team->get().getId();
  const YouthAcademy& academy = game->getWorld().getYouth();
  overview.quota = academy.reserveQuota(team_id);
  overview.country =
      leagueProfile(team->get().getLeagueId()).domestic_nationality;
  const std::vector<YouthTableRow> rows = academy.reserveTable(team_id);
  overview.league_size = static_cast<int>(rows.size());
  for (std::size_t index = 0; index < rows.size(); ++index)
  {
    if (rows[index].team_id != team_id) continue;
    overview.league_position = static_cast<int>(index) + 1;
    overview.table = rows[index];
  }
  return overview;
}

std::vector<YouthTableRow> GameController::getReserveTable() const
{
  const auto team = managedClub();
  if (!team || !game) return {};
  return game->getWorld().getYouth().reserveTable(team->get().getId());
}

const std::vector<YouthResult>& GameController::getReserveResults() const
{
  static const std::vector<YouthResult> EMPTY;
  return game ? game->getWorld().getYouth().reserveResults() : EMPTY;
}

YouthActionResult GameController::moveToReserves(PlayerID player_id)
{
  const auto team = managedClub();
  if (!team || !game) return YouthActionResult::NoClub;
  return game->getWorld().getYouth().moveToReserves(team->get().getId(),
                                                    player_id);
}

YouthActionResult GameController::promoteReservePlayer(PlayerID player_id)
{
  const auto team = managedClub();
  if (!team || !game) return YouthActionResult::NoClub;
  return game->getWorld().getYouth().promoteReserve(team->get().getId(),
                                                    player_id);
}

YouthActionResult GameController::moveReserveToU18(PlayerID player_id)
{
  const auto team = managedClub();
  if (!team || !game) return YouthActionResult::NoClub;
  return game->getWorld().getYouth().reserveToU18(team->get().getId(),
                                                  player_id);
}

// ========== National-team job ==========

const NationalJob* GameController::getNationalJob() const
{
  if (!game || !game->getNationalJob().hasJob()) return nullptr;
  return &game->getNationalJob().getJob();
}

bool GameController::hasNationalJob() const
{
  return game && game->getNationalJob().hasJob();
}

const std::vector<NationalStint>& GameController::getNationalJobHistory() const
{
  static const std::vector<NationalStint> NONE;
  return game ? game->getNationalJob().getHistory() : NONE;
}

std::vector<GameController::NationalVacancyView>
GameController::getNationalVacancies() const
{
  std::vector<NationalVacancyView> views;
  if (!game || !hasCareer()) return views;
  const NationalManagement& market = game->getNationalJob();
  const NationalTeams& teams = game->getNationalTeams();
  const ManagerProfile& profile = game->getCareer().getProfile();
  const std::vector<Language> ranking = teams.ranking();
  for (const NationalVacancy& vacancy : market.getVacancies())
  {
    const auto it = std::ranges::find(ranking, vacancy.nation);
    if (it == ranking.end()) continue;
    NationalVacancyView view;
    view.nation = vacancy.nation;
    view.rank = static_cast<int>(it - ranking.begin()) + 1;
    view.stature = market.stature(teams, vacancy.nation);
    view.chance = market.applicationChance(teams, profile, vacancy.nation);
    view.required_licence = NationalJobModel::requiredLicence(view.stature);
    view.weekly_wage = NationalJobModel::weeklyWage(view.stature);
    view.opened = vacancy.opened;
    view.compatriot = profile.nationality == vacancy.nation;
    if (const NationalApplication* application =
            market.findApplication(vacancy.nation))
      view.stage = application->stage;
    views.push_back(view);
  }
  std::ranges::sort(views,
                    [](const NationalVacancyView& a, const NationalVacancyView& b)
                    {
                      return a.chance != b.chance ? a.chance > b.chance
                                                  : a.rank < b.rank;
                    });
  return views;
}

NationalApplyResult GameController::applyForNationalJob(Language nation)
{
  if (!game || !hasCareer()) return NationalApplyResult::NoProfile;
  return game->getNationalJob().apply(nation, game->getCurrentDate(),
                                      game->getCareer().getProfile(),
                                      hasSelectedTeam());
}

const std::vector<NationalJobOffer>& GameController::getNationalJobOffers() const
{
  static const std::vector<NationalJobOffer> NONE;
  return game ? game->getNationalJob().getOffers() : NONE;
}

NationalApplyResult GameController::acceptNationalJobOffer(
    std::uint32_t offer_id)
{
  if (!game || !hasCareer()) return NationalApplyResult::NoProfile;
  return game->getNationalJob().accept(
      offer_id, game->getCurrentDate(), game->getNationalTeams(),
      game->getCareer(), hasSelectedTeam(), game->getWorld().getInbox());
}

bool GameController::declineNationalJobOffer(std::uint32_t offer_id)
{
  return game && game->getNationalJob().decline(offer_id);
}

bool GameController::resignNationalJob()
{
  if (!hasNationalJob()) return false;
  game->getNationalJob().leave(DepartureReason::Resigned,
                               game->getCurrentDate(), game->getNationalTeams(),
                               game->getCareer(), game->getWorld().getInbox());
  return true;
}

bool GameController::canCombineClubAndNation() const
{
  return hasCareer() && NationalJobModel::canCombineWithClub(
                            game->getCareer().getProfile().reputation);
}

GameController::CallUpView GameController::getCallUpView() const
{
  CallUpView view;
  const NationalJob* job = getNationalJob();
  if (!job) return view;
  const NationalTeams& teams = game->getNationalTeams();
  const GameDateValue today = game->getCurrentDate();
  view.nation = job->nation;
  const NationalTeams::Squad* squad = teams.squadOf(job->nation, today);
  if (squad)
  {
    view.announced = true;
    view.locked = !(today < squad->start);
    view.finals = squad->finals;
    view.start = squad->start;
    view.until = squad->until;
    view.limit = NationalTeams::squadLimit(*squad);
  }
  else
  {
    // The next window's squad is announced a week before it starts.
    const std::uint16_t season = SeasonCalendar::seasonStartYear(today);
    for (const std::uint16_t year : {season, static_cast<std::uint16_t>(season + 1)})
    {
      for (const SeasonCalendar::InternationalWindow& window :
           SeasonCalendar::internationalWindows(year))
      {
        const GameDateValue announce =
            SeasonCalendar::addDays(window.start, -ANNOUNCE_DAYS);
        if (announce < today) continue;
        view.next_announcement = announce;
        view.start = window.start;
        view.until = window.end;
        break;
      }
      if (!(view.next_announcement == GameDateValue())) break;
    }
  }
  const StatsConfig& config = gamedata->getStatsConfig();
  std::vector<std::pair<double, CallUpCandidate>> ranked;
  for (const PlayerID player_id : teams.eligiblePool(job->nation, today))
  {
    const Player& player = gamedata->getPlayers().at(player_id);
    CallUpCandidate candidate;
    candidate.id = player_id;
    candidate.name = player.getName();
    candidate.role = player.getRole();
    candidate.age = player.getAge();
    candidate.club = player.getTeamId();
    candidate.overall = static_cast<int>(player.getOverall(config));
    candidate.form = player.getForm();
    candidate.condition =
        static_cast<int>(std::lround(player.getDynamics().condition));
    if (const International::Record* record = teams.getRecord(player_id))
    {
      candidate.caps = record->caps;
      candidate.goals = record->goals;
    }
    candidate.available = player.isAvailable();
    candidate.selected =
        squad && std::ranges::contains(squad->players, player_id);
    ranked.emplace_back(
        International::selectionScore(player, candidate.caps, config),
        std::move(candidate));
  }
  std::ranges::sort(ranked,
                    [](const auto& a, const auto& b)
                    {
                      if (a.first != b.first) return a.first > b.first;
                      return a.second.id < b.second.id;
                    });
  view.candidates.reserve(ranked.size());
  for (auto& [score, candidate] : ranked)
    view.candidates.push_back(std::move(candidate));
  for (const International::Fixture& fixture : teams.getFixtures())
  {
    if (fixture.played || fixture.date < today ||
        (fixture.home != job->nation && fixture.away != job->nation))
      continue;
    view.fixtures.push_back(fixture);
  }
  std::ranges::sort(view.fixtures, {}, [](const International::Fixture& f)
                    { return dayOrdinal(f.date); });
  if (view.fixtures.size() > CALL_UP_FIXTURES)
    view.fixtures.resize(CALL_UP_FIXTURES);
  return view;
}

International::CallUpResult GameController::setNationalSquad(
    const std::vector<PlayerID>& players)
{
  const NationalJob* job = getNationalJob();
  if (!job) return International::CallUpResult::NoSquad;
  return game->getNationalTeams().setSquad(job->nation, players,
                                           game->getCurrentDate());
}
