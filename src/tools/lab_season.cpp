// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "tools/lab_season.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <map>
#include <ostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "model/game.h"
#include "model/injury.h"
#include "model/transfer_market.h"
#include "tools/lab_runner.h"

namespace Lab
{
namespace
{
constexpr int LAB_SAVE_SLOT = 1;
constexpr int PROGRESS_EVERY_DAYS = 30;
constexpr double POTENTIAL_HIGH = 80.0;
constexpr double POTENTIAL_GOOD = 70.0;

bool inRange(const GameDateValue& date, const GameDateValue& from,
             const GameDateValue& to)
{
  return !(date < from) && !(to < date);
}

bool isSeasonEnd(const GameDateValue& date)
{
  return date.month == 6 && date.day == 30;
}

std::int64_t revenueOf(const FinanceSummary& summary)
{
  const auto amount = [&](FinanceCategory category)
  { return std::abs(summary.by_category[static_cast<std::size_t>(category)]); };
  return amount(FinanceCategory::Matchday) +
         amount(FinanceCategory::Broadcasting) +
         amount(FinanceCategory::Sponsorship) +
         amount(FinanceCategory::PrizeMoney);
}

double populationSd(const std::vector<double>& values)
{
  if (values.size() < 2) return 0.0;
  double mean = 0.0;
  for (const double value : values) mean += value;
  mean /= static_cast<double>(values.size());
  double squares = 0.0;
  for (const double value : values) squares += (value - mean) * (value - mean);
  return std::sqrt(squares / static_cast<double>(values.size()));
}

double percentile(std::vector<double> values, double q)
{
  if (values.empty()) return std::nan("");
  std::ranges::sort(values);
  const auto index = static_cast<std::size_t>(
      std::lround(q * static_cast<double>(values.size() - 1)));
  return values[index];
}

std::vector<LeagueID> selectLeagues(const GameController& controller,
                                    const std::string& filter)
{
  std::vector<LeagueID> leagues;
  if (filter == "all" || filter == "top")
  {
    for (const auto& league : controller.getLeagues())
    {
      const LeagueID id = league.get().getId();
      if (filter == "all" || controller.getLeagueTier(id) == 1)
        leagues.push_back(id);
    }
  }
  else
  {
    std::size_t begin = 0;
    while (begin <= filter.size())
    {
      const std::size_t end = std::min(filter.find(',', begin), filter.size());
      const std::string token = filter.substr(begin, end - begin);
      int id = 0;
      try
      {
        id = std::stoi(token);
      }
      catch (const std::exception&)
      {
        throw std::invalid_argument(
            std::format("invalid league id '{}' in --leagues", token));
      }
      if (id <= 0 || id > 255 ||
          !controller.getLeagueById(static_cast<LeagueID>(id)))
        throw std::invalid_argument(std::format("unknown league id {}", id));
      leagues.push_back(static_cast<LeagueID>(id));
      begin = end + 1;
    }
  }
  std::ranges::sort(leagues);
  if (leagues.empty()) throw std::invalid_argument("no league selected");
  return leagues;
}

struct InjuryOnset
{
  InjuryType type = InjuryType::None;
  double days = 0.0;
};

/** Everything collected over all seasons. */
struct WorldTotals
{
  std::vector<MatchSample> matches;
  std::size_t world_matches = 0;
  double wall_seconds = 0.0;
  std::vector<double> season_seconds;
  // League-seasons.
  std::vector<double> champion_ppg;
  std::vector<double> champion_points;
  std::vector<double> relegated_ppg;
  std::vector<double> relegated_points;
  std::vector<double> scorer_goals;
  std::vector<double> scorer_per_game;
  std::vector<double> points_sd;
  std::vector<double> goal_diff_sd;
  TextTable leagues{"League seasons",
                    {"Season", "League", "Tier", "Clubs", "Games",
                     "Champion pts", "Best relegated pts", "Top scorer goals",
                     "Points SD"},
                    {}};
  // Club-seasons.
  std::vector<double> revenue;
  std::vector<double> net;
  std::vector<double> top_wages;
  std::vector<double> top_revenue;
  std::vector<double> lower_wages;
  std::vector<double> lower_revenue;
  std::size_t clubs_negative = 0;
  std::size_t clubs_counted = 0;
  std::vector<double> squad_sizes;
  std::vector<double> top_mean_ages;
  std::vector<double> ages;
  std::vector<double> potentials;
  std::vector<double> overalls;
  std::vector<double> new_players;
  std::vector<double> removed_players;
  std::vector<double> incoming_moves;
  std::vector<double> incoming_fee_moves;
  std::vector<double> incoming_loans;
  std::vector<double> club_injuries;
  std::vector<double> club_players_at_start;
  std::array<std::int64_t, FINANCE_CATEGORY_COUNT> top_mix{};
  // Wage-position regression inputs (pooled league-seasons).
  std::vector<double> wage_x;
  std::vector<double> position_y;
  // Transfers and injuries.
  std::map<TransferKind, std::size_t> transfer_kinds;
  std::size_t fee_moves = 0;
  std::size_t counted_moves = 0;
  std::vector<InjuryOnset> injuries;
};

class SeasonRunner
{
 public:
  explicit SeasonRunner(const SeasonOptions& run_options)
      : options(run_options)
  {
  }

  LabReport run()
  {
    const auto started = std::chrono::steady_clock::now();
    controller.newGame(LAB_SAVE_SLOT, options.seed);
    controller.setSimulationThreads(options.threads);
    leagues = selectLeagues(controller, options.leagues);
    for (int season = 0; season < options.seasons; ++season) runSeason();
    totals.wall_seconds = std::chrono::duration<double>(
                              std::chrono::steady_clock::now() - started)
                              .count();
    return buildReport();
  }

 private:
  const SeasonOptions& options;
  GameController controller;
  std::vector<LeagueID> leagues;
  WorldTotals totals;
  std::unordered_map<PlayerID, std::int32_t> last_injury_day;
  std::unordered_map<TeamID, int> injuries_by_club;

  bool selected(LeagueID league) const
  {
    return std::ranges::binary_search(leagues, league);
  }

  std::vector<TeamID> clubIds() const
  {
    std::vector<TeamID> ids;
    for (const auto& team : controller.getTeams())
      ids.push_back(team.get().getId());
    std::ranges::sort(ids);
    return ids;
  }

  /** Records injuries that started since the previous call. */
  void pollInjuries(bool count)
  {
    for (const auto& [id, player] : controller.getGameData()->getPlayers())
    {
      const PlayerDynamics& dynamics = player.getDynamics();
      const auto [entry, inserted] =
          last_injury_day.try_emplace(id, dynamics.last_injury_day);
      if (inserted || entry->second == dynamics.last_injury_day) continue;
      entry->second = dynamics.last_injury_day;
      if (!count || dynamics.injury == InjuryType::None ||
          player.getTeamId() == FREE_AGENTS_TEAM_ID)
        continue;
      totals.injuries.push_back(
          {dynamics.last_injury, static_cast<double>(dynamics.injury_days)});
      ++injuries_by_club[player.getTeamId()];
    }
  }

  void runSeason()
  {
    const auto started = std::chrono::steady_clock::now();
    const GameDateValue seasonStart = controller.getCurrentDate();
    const int seasonNumber = controller.getCurrentSeason();
    const auto gamedata = controller.getGameData();

    std::unordered_map<PlayerID, TeamID> startPlayers;
    std::unordered_map<TeamID, int> startSquads;
    for (const auto& [id, player] : gamedata->getPlayers())
    {
      startPlayers.emplace(id, player.getTeamId());
      if (player.getTeamId() != FREE_AGENTS_TEAM_ID)
        ++startSquads[player.getTeamId()];
    }
    injuries_by_club.clear();
    pollInjuries(false);

    int days = 0;
    while (!isSeasonEnd(controller.getCurrentDate()))
    {
      controller.advanceDay();
      pollInjuries(true);
      if (++days > options.max_days_per_season)
        throw std::runtime_error(std::format(
            "season {} did not reach 30 June within {} days", seasonNumber,
            options.max_days_per_season));
      if (options.progress && days % PROGRESS_EVERY_DAYS == 0)
        *options.progress << std::format(
            "[fm_lab] season {}/{} day {} ({})\n", seasonNumber,
            options.seasons, days, controller.getCurrentDate().toString());
    }

    std::map<LeagueID, std::vector<StandingRow>> tables;
    captureSeasonEnd(tables);
    captureClubs();

    // 1 July: season transition (prize money, history, retirements, youth).
    controller.advanceDay();
    const GameDateValue rollover = controller.getCurrentDate();
    if (controller.getCurrentSeason() == seasonNumber)
      throw std::runtime_error("season transition did not happen on 1 July");
    captureHistory(seasonNumber, tables);
    captureFinances(seasonStart, rollover);
    captureTransfers(seasonStart, rollover);
    capturePopulation(startPlayers, startSquads);
    totals.season_seconds.push_back(std::chrono::duration<double>(
                                        std::chrono::steady_clock::now() -
                                        started)
                                        .count());
  }

  void captureSeasonEnd(std::map<LeagueID, std::vector<StandingRow>>& tables)
  {
    for (const auto& [date, matches] :
         controller.getGame()->getCalendar().getFullCalendar())
    {
      for (const Match& match : matches)
      {
        if (!match.isPlayed()) continue;
        ++totals.world_matches;
        if (match.getMatchType() != MatchType::LEAGUE ||
            !selected(match.getCompetitionId()))
          continue;
        if (const auto report = controller.getMatchReport(
                date, match.getHomeTeamId(), match.getAwayTeamId()))
          totals.matches.push_back(sampleFromReport(*report));
      }
    }
    for (const LeagueID league : leagues)
    {
      std::vector<StandingRow> rows = controller.getStandings(league);
      if (rows.empty() || rows.front().played == 0) continue;
      const double games = rows.front().played;
      totals.champion_points.push_back(rows.front().points);
      totals.champion_ppg.push_back(rows.front().points / games);
      std::vector<double> ppg;
      std::vector<double> goalDiff;
      for (const StandingRow& row : rows)
      {
        const double played = std::max<double>(row.played, 1.0);
        ppg.push_back(row.points / played);
        goalDiff.push_back(row.goal_difference / played);
      }
      totals.points_sd.push_back(populationSd(ppg));
      totals.goal_diff_sd.push_back(populationSd(goalDiff));
      const auto scorers =
          controller.getTopScorers(MatchType::LEAGUE, league, 1);
      if (!scorers.empty())
      {
        totals.scorer_goals.push_back(scorers.front().goals);
        totals.scorer_per_game.push_back(scorers.front().goals / games);
      }
      // Wage-to-position inputs: ln(wage / league mean), -ln(p/(N+1-p)).
      std::vector<double> wages;
      for (const StandingRow& row : rows)
        wages.push_back(static_cast<double>(
            std::max<std::int64_t>(controller.getWeeklyWageBill(row.team_id),
                                   1)));
      double meanWage = 0.0;
      for (const double wage : wages) meanWage += wage;
      meanWage /= static_cast<double>(wages.size());
      const auto clubs = static_cast<double>(rows.size());
      for (std::size_t i = 0; i < rows.size(); ++i)
      {
        const double position = rows[i].position;
        totals.wage_x.push_back(std::log(wages[i] / meanWage));
        totals.position_y.push_back(
            -std::log(position / (clubs + 1.0 - position)));
      }
      tables.emplace(league, std::move(rows));
    }
  }

  void captureClubs()
  {
    const StatsConfig& config = controller.getStatsConfig();
    for (const auto& team : controller.getTeams())
    {
      const Team& club = team.get();
      if (!selected(club.getLeagueId())) continue;
      const auto& squad = controller.getPlayersForTeam(club.getId());
      totals.squad_sizes.push_back(static_cast<double>(squad.size()));
      double ageSum = 0.0;
      for (const auto& player : squad)
      {
        const double age = player.get().getAge();
        ageSum += age;
        totals.ages.push_back(age);
        totals.potentials.push_back(
            static_cast<double>(player.get().getPotential()));
        totals.overalls.push_back(player.get().getOverall(config));
      }
      if (!squad.empty() && controller.getLeagueTier(club.getLeagueId()) == 1)
        totals.top_mean_ages.push_back(ageSum /
                                       static_cast<double>(squad.size()));
    }
  }

  void captureHistory(int seasonNumber,
                      const std::map<LeagueID, std::vector<StandingRow>>& tables)
  {
    for (const SeasonHistoryEntry& entry : controller.getSeasonHistory())
    {
      if (entry.season != seasonNumber ||
          entry.competition_type != MatchType::LEAGUE)
        continue;
      const auto table = tables.find(entry.competition_id);
      if (table == tables.end()) continue;
      const std::vector<StandingRow>& rows = table->second;
      std::optional<StandingRow> bestRelegated;
      for (const StandingRow& row : rows)
        if (std::ranges::contains(entry.relegated, row.team_id) &&
            (!bestRelegated || row.position < bestRelegated->position))
          bestRelegated = row;
      if (bestRelegated && bestRelegated->played > 0)
      {
        totals.relegated_points.push_back(bestRelegated->points);
        totals.relegated_ppg.push_back(
            static_cast<double>(bestRelegated->points) / bestRelegated->played);
      }
      totals.leagues.rows.push_back(
          {std::to_string(seasonNumber), entry.competition_name,
           std::to_string(controller.getLeagueTier(entry.competition_id)),
           std::to_string(rows.size()), std::to_string(rows.front().played),
           std::to_string(rows.front().points),
           bestRelegated ? std::to_string(bestRelegated->points) : "-",
           std::to_string(entry.top_scorer_goals),
           std::format("{:.1f}", populationSd([&]
                                              {
                                                std::vector<double> points;
                                                for (const StandingRow& row :
                                                     rows)
                                                  points.push_back(row.points);
                                                return points;
                                              }()))});
    }
  }

  void captureFinances(const GameDateValue& from, const GameDateValue& to)
  {
    for (const auto& team : controller.getTeams())
    {
      const Team& club = team.get();
      if (!selected(club.getLeagueId())) continue;
      const FinanceSummary summary =
          controller.getFinanceSummary(club.getId(), from, to);
      const auto category = [&](FinanceCategory c)
      { return summary.by_category[static_cast<std::size_t>(c)]; };
      const auto revenue = static_cast<double>(revenueOf(summary));
      const auto wages =
          static_cast<double>(std::abs(category(FinanceCategory::Wages)));
      totals.revenue.push_back(revenue);
      totals.net.push_back(static_cast<double>(
          summary.net() - category(FinanceCategory::OpeningBalance)));
      if (controller.getLeagueTier(club.getLeagueId()) == 1)
      {
        totals.top_wages.push_back(wages);
        totals.top_revenue.push_back(revenue);
        for (std::size_t c = 0; c < FINANCE_CATEGORY_COUNT; ++c)
          totals.top_mix[c] += std::abs(summary.by_category[c]);
      }
      else
      {
        totals.lower_wages.push_back(wages);
        totals.lower_revenue.push_back(revenue);
      }
      ++totals.clubs_counted;
      if (club.getFinances().getBalance() < 0) ++totals.clubs_negative;
    }
  }

  void captureTransfers(const GameDateValue& from, const GameDateValue& to)
  {
    std::unordered_map<TeamID, int> moves;
    std::unordered_map<TeamID, int> feeMoves;
    std::unordered_map<TeamID, int> loans;
    for (const TransferRecord& record :
         controller.getGame()->getTransfers().history())
    {
      if (!inRange(record.date, from, to)) continue;
      ++totals.transfer_kinds[record.kind];
      if (record.kind == TransferKind::Release ||
          record.kind == TransferKind::LoanReturn)
        continue;
      ++totals.counted_moves;
      ++moves[record.to_team];
      const bool fee = record.kind == TransferKind::Permanent && record.fee > 0;
      if (fee)
      {
        ++totals.fee_moves;
        ++feeMoves[record.to_team];
      }
      if (record.kind == TransferKind::Loan) ++loans[record.to_team];
    }
    for (const TeamID club : clubIds())
    {
      totals.incoming_moves.push_back(moves[club]);
      totals.incoming_fee_moves.push_back(feeMoves[club]);
      totals.incoming_loans.push_back(loans[club]);
    }
  }

  void capturePopulation(
      const std::unordered_map<PlayerID, TeamID>& startPlayers,
      const std::unordered_map<TeamID, int>& startSquads)
  {
    std::unordered_map<TeamID, int> added;
    std::unordered_map<TeamID, int> removed;
    std::unordered_set<PlayerID> present;
    for (const auto& [id, player] : controller.getGameData()->getPlayers())
    {
      present.insert(id);
      if (!startPlayers.contains(id)) ++added[player.getTeamId()];
    }
    for (const auto& [id, team] : startPlayers)
      if (!present.contains(id)) ++removed[team];
    for (const TeamID club : clubIds())
    {
      totals.new_players.push_back(added[club]);
      totals.removed_players.push_back(removed[club]);
      const auto squad = startSquads.find(club);
      if (squad != startSquads.end() && squad->second > 0)
      {
        totals.club_injuries.push_back(injuries_by_club[club]);
        totals.club_players_at_start.push_back(squad->second);
      }
    }
  }

  LabReport buildReport()
  {
    std::map<std::string, Estimate> metrics = matchMetrics(totals.matches);
    metrics["champion_ppg"] = meanEstimate(totals.champion_ppg);
    metrics["champion_points"] = meanEstimate(totals.champion_points);
    metrics["best_relegated_ppg"] = meanEstimate(totals.relegated_ppg);
    metrics["best_relegated_points"] = meanEstimate(totals.relegated_points);
    metrics["top_scorer_goals"] = meanEstimate(totals.scorer_goals);
    metrics["top_scorer_goals_per_game"] = meanEstimate(totals.scorer_per_game);
    metrics["points_sd_ppg"] = meanEstimate(totals.points_sd);
    metrics["goal_diff_sd_per_game"] = meanEstimate(totals.goal_diff_sd);
    metrics["median_club_revenue"] = medianEstimate(totals.revenue);
    metrics["median_club_net"] = medianEstimate(totals.net);
    metrics["wage_revenue_top"] =
        ratioEstimate(totals.top_wages, totals.top_revenue);
    metrics["wage_revenue_lower"] =
        ratioEstimate(totals.lower_wages, totals.lower_revenue);
    metrics["negative_cash_share"] =
        proportionEstimate(totals.clubs_negative, totals.clubs_counted);
    metrics["fee_transfer_share"] =
        proportionEstimate(totals.fee_moves, totals.counted_moves);
    metrics["transfers_per_club"] = meanEstimate(totals.incoming_moves);
    metrics["fee_moves_per_club"] = meanEstimate(totals.incoming_fee_moves);
    metrics["loans_per_club"] = meanEstimate(totals.incoming_loans);
    metrics["squad_size"] = meanEstimate(totals.squad_sizes);
    metrics["player_age_mean"] = meanEstimate(totals.ages);
    metrics["mean_age_top_squads"] = meanEstimate(totals.top_mean_ages);
    metrics["new_players_per_club"] = meanEstimate(totals.new_players);
    metrics["removed_players_per_club"] = meanEstimate(totals.removed_players);
    metrics["injuries_per_player_season"] =
        ratioEstimate(totals.club_injuries, totals.club_players_at_start);

    std::size_t hamstrings = 0;
    std::map<InjuryType, std::vector<double>> layoffs;
    for (const InjuryOnset& onset : totals.injuries)
    {
      if (onset.type == InjuryType::HamstringStrain ||
          onset.type == InjuryType::HamstringTightness)
        ++hamstrings;
      layoffs[onset.type].push_back(onset.days);
    }
    metrics["hamstring_share"] =
        proportionEstimate(hamstrings, totals.injuries.size());
    const auto median = [&](InjuryType type)
    {
      const auto found = layoffs.find(type);
      return found == layoffs.end() ? Estimate{}
                                    : medianEstimate(found->second);
    };
    metrics["hamstring_layoff_median"] = median(InjuryType::HamstringStrain);
    metrics["ankle_layoff_median"] = median(InjuryType::AnkleSprain);
    metrics["acl_layoff_median"] = median(InjuryType::KneeAcl);

    const std::vector<double>& x = totals.wage_x;
    const std::vector<double>& y = totals.position_y;
    metrics["wage_position_r2"] = bootstrapEstimate(
        x.size(),
        [&](std::span<const std::size_t> indices)
        {
          const auto n = static_cast<double>(indices.size());
          double sx = 0.0, sy = 0.0, sxx = 0.0, syy = 0.0, sxy = 0.0;
          for (const std::size_t i : indices)
          {
            sx += x[i];
            sy += y[i];
            sxx += x[i] * x[i];
            syy += y[i] * y[i];
            sxy += x[i] * y[i];
          }
          const double cov = sxy - sx * sy / n;
          const double vx = sxx - sx * sx / n;
          const double vy = syy - sy * sy / n;
          if (vx <= 0.0 || vy <= 0.0) return std::nan("");
          return cov * cov / (vx * vy);
        },
        0x3A6E5ULL);

    if (totals.world_matches > 0)
    {
      Estimate speed;
      speed.value = speed.low = speed.high =
          1000.0 * totals.wall_seconds /
          static_cast<double>(totals.world_matches);
      speed.n = totals.world_matches;
      metrics["ms_per_world_match"] = speed;
    }

    LabReport report;
    report.mode = "season";
    report.seed = options.seed;
    report.samples = static_cast<std::size_t>(options.seasons);
    report.threads = options.threads;
    report.wall_seconds = totals.wall_seconds;
    report.ms_per_match =
        totals.world_matches == 0
            ? 0.0
            : 1000.0 * totals.wall_seconds /
                  static_cast<double>(totals.world_matches);
    report.seconds_per_season =
        totals.wall_seconds / std::max(options.seasons, 1);
    report.parameters["leagues"] = options.leagues;
    report.parameters["league_ids"] = [&]
    {
      std::string ids;
      for (const LeagueID league : leagues)
        ids += (ids.empty() ? "" : ",") + std::to_string(league);
      return ids;
    }();
    report.parameters["world_matches_played"] =
        std::to_string(totals.world_matches);
    report.parameters["league_matches_measured"] =
        std::to_string(totals.matches.size());
    static constexpr std::array SCOPES = {Scope::Match, Scope::World};
    report.rows = evaluateTargets(metrics, SCOPES);
    report.tables.push_back(totals.leagues);
    for (TextTable& table : matchTables(totals.matches))
      report.tables.push_back(std::move(table));
    report.tables.push_back(revenueMixTable());
    report.tables.push_back(populationTable());
    report.tables.push_back(transferTable());
    report.tables.push_back(injuryTable(layoffs));
    report.notes.push_back(
        "Season mode measures league matches from structured match reports; "
        "engine-only metrics (penalties, substitutions, distance, game "
        "states...) come from `fm_lab matches`.");
    report.notes.push_back(
        "AI-only world (no managed club); every club is run by the AI.");
    return report;
  }

  TextTable revenueMixTable() const
  {
    TextTable table{"Top-division revenue mix (CT-W22 reference: TV 46 / gate "
                    "14 / commercial 32 / prize+other 8%)",
                    {"Category", "Share of revenue"},
                    {}};
    const auto amount = [&](FinanceCategory c)
    { return static_cast<double>(totals.top_mix[static_cast<std::size_t>(c)]); };
    const double revenue =
        amount(FinanceCategory::Matchday) +
        amount(FinanceCategory::Broadcasting) +
        amount(FinanceCategory::Sponsorship) +
        amount(FinanceCategory::PrizeMoney);
    const auto row = [&](const char* name, FinanceCategory c)
    {
      table.rows.push_back(
          {name, revenue > 0.0
                     ? std::format("{:.1f}%", 100.0 * amount(c) / revenue)
                     : "-"});
    };
    row("Broadcasting", FinanceCategory::Broadcasting);
    row("Matchday", FinanceCategory::Matchday);
    row("Sponsorship", FinanceCategory::Sponsorship);
    row("Prize money", FinanceCategory::PrizeMoney);
    return table;
  }

  TextTable populationTable() const
  {
    TextTable table{"Player population (selected leagues, season end)",
                    {"Measure", "P10", "P25", "P50", "P75", "P90",
                     ">= 70", ">= 80"},
                    {}};
    const auto row = [&](const char* name, const std::vector<double>& values)
    {
      const auto atLeast = [&](double threshold)
      {
        if (values.empty()) return std::string("-");
        const auto count = std::ranges::count_if(
            values, [&](double value) { return value >= threshold; });
        return std::format("{:.1f}%", 100.0 * static_cast<double>(count) /
                                          static_cast<double>(values.size()));
      };
      table.rows.push_back({name, std::format("{:.1f}", percentile(values, 0.1)),
                            std::format("{:.1f}", percentile(values, 0.25)),
                            std::format("{:.1f}", percentile(values, 0.5)),
                            std::format("{:.1f}", percentile(values, 0.75)),
                            std::format("{:.1f}", percentile(values, 0.9)),
                            atLeast(POTENTIAL_GOOD), atLeast(POTENTIAL_HIGH)});
    };
    row("Potential", totals.potentials);
    row("Overall", totals.overalls);
    row("Age", totals.ages);
    return table;
  }

  TextTable transferTable() const
  {
    TextTable table{"Transfer records by kind (all clubs)",
                    {"Kind", "Moves"},
                    {}};
    static constexpr std::array<std::pair<TransferKind, const char*>, 6> KINDS =
        {{{TransferKind::Permanent, "Permanent"},
          {TransferKind::Free, "Free"},
          {TransferKind::Loan, "Loan"},
          {TransferKind::LoanReturn, "Loan return"},
          {TransferKind::PreContract, "Pre-contract"},
          {TransferKind::Release, "Release"}}};
    for (const auto& [kind, name] : KINDS)
    {
      const auto found = totals.transfer_kinds.find(kind);
      table.rows.push_back(
          {name, std::to_string(found == totals.transfer_kinds.end()
                                    ? 0
                                    : found->second)});
    }
    return table;
  }

  TextTable injuryTable(
      const std::map<InjuryType, std::vector<double>>& layoffs) const
  {
    TextTable table{"Injury onsets by type (club players)",
                    {"Type", "Injuries", "Share", "Median layoff (days)"},
                    {}};
    for (const auto& [type, days] : layoffs)
    {
      table.rows.push_back(
          {InjuryModel::nameKey(type), std::to_string(days.size()),
           std::format("{:.1f}%", 100.0 * static_cast<double>(days.size()) /
                                      static_cast<double>(
                                          totals.injuries.size())),
           std::format("{:.0f}", medianEstimate(days).value)});
    }
    return table;
  }
};
}  // namespace

LabReport runSeasons(const SeasonOptions& options)
{
  if (options.seasons < 1)
    throw std::invalid_argument("--seasons must be >= 1");
  SeasonRunner runner(options);
  return runner.run();
}
}  // namespace Lab
