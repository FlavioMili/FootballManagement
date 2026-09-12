// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "player_repository.h"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "database/SQLLoader.h"
#include "model/role_utils.h"

namespace
{
class StatsSaxParser final : public nlohmann::json_sax<nlohmann::json>
{
 public:
  bool null() override { return false; }
  bool boolean(bool /*value*/) override { return false; }

  bool number_integer(number_integer_t value) override
  {
    return addValue(static_cast<float>(value));
  }

  bool number_unsigned(number_unsigned_t value) override
  {
    return addValue(static_cast<float>(value));
  }

  bool number_float(number_float_t value, const string_t& /*token*/) override
  {
    return addValue(static_cast<float>(value));
  }

  bool string(string_t& /*value*/) override { return false; }
  bool binary(binary_t& /*value*/) override { return false; }

  bool start_object(std::size_t /*elements*/) override
  {
    ++depth;
    return depth == ROOT_OBJECT_DEPTH;
  }

  bool key(string_t& value) override
  {
    currentKey = std::move(value);
    return depth == ROOT_OBJECT_DEPTH;
  }

  bool end_object() override
  {
    if (depth != ROOT_OBJECT_DEPTH) return false;
    --depth;
    return true;
  }

  bool start_array(std::size_t /*elements*/) override { return false; }
  bool end_array() override { return false; }

  bool parse_error(std::size_t /*position*/, const std::string& /*lastToken*/,
                   const nlohmann::detail::exception& /*exception*/) override
  {
    return false;
  }

  std::map<std::string, float> takeStats() { return std::move(stats); }

 private:
  bool addValue(float value)
  {
    if (depth != ROOT_OBJECT_DEPTH || currentKey.empty()) return false;
    stats.emplace(std::move(currentKey), value);
    currentKey.clear();
    return true;
  }

  static constexpr std::size_t ROOT_OBJECT_DEPTH = 1;
  std::size_t depth = 0;
  std::string currentKey;
  std::map<std::string, float> stats;
};

/** Parses a comma separated list of numbers; missing values stay at 0. */
template <std::size_t N>
std::array<double, N> parseNumbers(std::string_view text, std::size_t& count)
{
  std::array<double, N> values{};
  count = 0;
  while (!text.empty() && count < N)
  {
    const std::size_t comma = text.find(',');
    const std::string_view token = text.substr(0, comma);
    double value = 0.0;
    if (std::from_chars(token.data(), token.data() + token.size(), value).ec !=
        std::errc{})
      break;
    values[count++] = value;
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  return values;
}

std::string encodeTraits(const PlayerTraits& traits)
{
  return std::format("{},{},{},{},{}", traits.professionalism, traits.ambition,
                     traits.temperament, traits.loyalty,
                     traits.injury_proneness);
}

PlayerTraits decodeTraits(std::string_view text)
{
  std::size_t count = 0;
  const auto values = parseNumbers<5>(text, count);
  PlayerTraits traits;
  if (count < values.size()) return traits;
  const auto trait = [](double value)
  { return static_cast<std::uint8_t>(std::clamp(value, 1.0, 100.0)); };
  traits.professionalism = trait(values[0]);
  traits.ambition = trait(values[1]);
  traits.temperament = trait(values[2]);
  traits.loyalty = trait(values[3]);
  traits.injury_proneness = trait(values[4]);
  return traits;
}

constexpr std::size_t DYNAMICS_FIELDS = 14 + PlayerDynamics::FORM_WINDOW;

std::string encodeDynamics(const PlayerDynamics& dynamics)
{
  std::string text =
      std::format("{:.2f},{:.2f},{:.2f},{:.3f},{},{},{},{},{},{},{},{},{},{}",
                  dynamics.condition, dynamics.sharpness, dynamics.morale,
                  dynamics.playing_share, static_cast<int>(dynamics.injury),
                  dynamics.injury_days, static_cast<int>(dynamics.last_injury),
                  dynamics.last_injury_day, dynamics.last_match_day,
                  dynamics.season_appearances, dynamics.season_minutes,
                  dynamics.week_minutes, dynamics.transfer_interest_weeks,
                  dynamics.rating_count);
  for (const float rating : dynamics.recent_ratings)
    text += std::format(",{:.1f}", rating);
  return text;
}

PlayerDynamics decodeDynamics(std::string_view text)
{
  std::size_t count = 0;
  const auto values = parseNumbers<DYNAMICS_FIELDS>(text, count);
  PlayerDynamics dynamics;
  if (count < DYNAMICS_FIELDS) return dynamics;
  const auto injury = [](double value)
  {
    const auto raw = static_cast<int>(value);
    return raw > 0 && raw < static_cast<int>(InjuryType::COUNT)
               ? static_cast<InjuryType>(raw)
               : InjuryType::None;
  };
  dynamics.condition = static_cast<float>(std::clamp(values[0], 0.0, 100.0));
  dynamics.sharpness = static_cast<float>(std::clamp(values[1], 0.0, 100.0));
  dynamics.morale = static_cast<float>(std::clamp(values[2], 0.0, 100.0));
  dynamics.playing_share = static_cast<float>(std::clamp(values[3], 0.0, 1.0));
  dynamics.injury = injury(values[4]);
  dynamics.injury_days = static_cast<std::uint16_t>(values[5]);
  if (dynamics.injury == InjuryType::None) dynamics.injury_days = 0;
  dynamics.last_injury = injury(values[6]);
  dynamics.last_injury_day = static_cast<std::int32_t>(values[7]);
  dynamics.last_match_day = static_cast<std::int32_t>(values[8]);
  dynamics.season_appearances = static_cast<std::uint16_t>(values[9]);
  dynamics.season_minutes = static_cast<std::uint16_t>(values[10]);
  dynamics.week_minutes = static_cast<std::uint16_t>(values[11]);
  dynamics.transfer_interest_weeks = static_cast<std::uint8_t>(values[12]);
  dynamics.rating_count = static_cast<std::uint8_t>(std::min<double>(
      values[13], static_cast<double>(PlayerDynamics::FORM_WINDOW)));
  for (std::size_t i = 0; i < PlayerDynamics::FORM_WINDOW; ++i)
    dynamics.recent_ratings[i] = static_cast<float>(values[14 + i]);
  return dynamics;
}

const char* columnText(sqlite3_stmt* stmt, int column)
{
  const unsigned char* text = sqlite3_column_text(stmt, column);
  return text ? reinterpret_cast<const char*>(text) : "";
}

constexpr int PLAYER_PARAM_COUNT = 15;

std::map<std::string, float> parsePlayerStats(std::string_view encodedStats)
{
  StatsSaxParser parser;
  if (!nlohmann::json::sax_parse(encodedStats, &parser))
  {
    throw std::runtime_error("Invalid player stats JSON in database");
  }
  return parser.takeStats();
}
}  // namespace

PlayerRepository::PlayerRepository(std::shared_ptr<DatabaseConnection> conn)
    : db_conn(conn)
{
}

std::vector<Player> PlayerRepository::loadAllPlayers() const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::SELECT_ALL_PLAYERS));
  std::vector<Player> players;
  sqlite3_stmt* count_stmt =
      db_conn->prepareStatement("SELECT COUNT(*) FROM Players;");
  if (sqlite3_step(count_stmt) == SQLITE_ROW)
  {
    players.reserve(
        static_cast<std::size_t>(sqlite3_column_int64(count_stmt, 0)));
  }
  sqlite3_finalize(count_stmt);

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    auto id = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
    auto team_id = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));
    auto first_name =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    auto last_name =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    int age = sqlite3_column_int(stmt, 4);
    auto role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    auto nationality_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    auto wage = static_cast<uint32_t>(sqlite3_column_int(stmt, 7));
    int contract_years = sqlite3_column_int(stmt, 8);
    int height = sqlite3_column_int(stmt, 9);
    auto foot_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
    auto stats_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
    int status = sqlite3_column_int(stmt, 12);

    std::string nat_str(nationality_str);
    auto it = stringToLanguage.find(nat_str);
    Language nationality =
        (it != stringToLanguage.end()) ? it->second : Language::EN;
    Foot foot = (std::string(foot_str) == "Left") ? Foot::Left : Foot::Right;

    std::map<std::string, float> stats = parsePlayerStats(stats_str);

    PlayerRole playerRole = RoleUtils::fromString(role);

    Player& player = players.emplace_back(
        id, team_id, first_name, last_name, playerRole, nationality, wage,
        status, age, contract_years, height, foot, std::move(stats));
    const double potential = sqlite3_column_double(stmt, 13);
    if (potential > 0.0) player.setPotential(static_cast<float>(potential));
    player.setTraits(decodeTraits(columnText(stmt, 14)));
    player.mutableDynamics() = decodeDynamics(columnText(stmt, 15));
  }

  sqlite3_finalize(stmt);
  return players;
}

void PlayerRepository::bindPlayerParams(sqlite3_stmt* stmt,
                                        const Player& player,
                                        int startIndex) const
{
  nlohmann::json stats_json = player.getStats();
  std::string stats_str = stats_json.dump();

  sqlite3_bind_int(stmt, startIndex++, static_cast<int>(player.getTeamId()));
  sqlite3_bind_text(stmt, startIndex++, player.getFirstName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, startIndex++, player.getLastName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, startIndex++, player.getAge());
  std::string role_str = RoleUtils::toString(player.getRole());
  sqlite3_bind_text(stmt, startIndex++, role_str.c_str(), -1, SQLITE_TRANSIENT);

  auto it = languageToString.find(player.getNationality());
  std::string nationality_str =
      (it != languageToString.end()) ? std::string(it->second) : "English";
  sqlite3_bind_text(stmt, startIndex++, nationality_str.c_str(), -1,
                    SQLITE_TRANSIENT);

  sqlite3_bind_int(stmt, startIndex++, static_cast<int>(player.getWage()));
  sqlite3_bind_int(stmt, startIndex++, player.getContractYears());
  sqlite3_bind_int(stmt, startIndex++, player.getHeight());

  std::string foot_str = (player.getFoot() == Foot::Left) ? "Left" : "Right";
  sqlite3_bind_text(stmt, startIndex++, foot_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, startIndex++, stats_str.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, startIndex++, static_cast<int>(player.getStatus()));
  sqlite3_bind_double(stmt, startIndex++,
                      static_cast<double>(player.getPotential()));
  const std::string traits = encodeTraits(player.getTraits());
  sqlite3_bind_text(stmt, startIndex++, traits.c_str(), -1, SQLITE_TRANSIENT);
  const std::string dynamics = encodeDynamics(player.getDynamics());
  sqlite3_bind_text(stmt, startIndex++, dynamics.c_str(), -1, SQLITE_TRANSIENT);
}

void PlayerRepository::insertPlayer(const Player& player) const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::INSERT_PLAYER));

  bindPlayerParams(stmt, player, 1);

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void PlayerRepository::insertPlayers(
    const std::vector<std::reference_wrapper<const Player>>& players) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      SQLLoader::getQuery(Query::INSERT_PLAYER_WITH_ID));
  for (const auto& player_ref : players)
  {
    const Player& player = player_ref.get();
    sqlite3_bind_int(stmt, 1, static_cast<int>(player.getId()));
    bindPlayerParams(stmt, player, 2);
    db_conn->executeStep(stmt);
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);
  }
  sqlite3_finalize(stmt);
}

void PlayerRepository::insertPlayerWithId(const Player& player) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      SQLLoader::getQuery(Query::INSERT_PLAYER_WITH_ID));

  sqlite3_bind_int(stmt, 1, static_cast<int>(player.getId()));
  bindPlayerParams(stmt, player, 2);

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void PlayerRepository::updatePlayer(const Player& player) const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::UPDATE_PLAYER));

  bindPlayerParams(stmt, player, 1);
  sqlite3_bind_int(stmt, PLAYER_PARAM_COUNT + 1,
                   static_cast<int>(player.getId()));

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void PlayerRepository::updatePlayers(
    const std::vector<std::reference_wrapper<const Player>>& players) const
{
  // Upsert so that players created since the last save (youth intake) are
  // inserted and existing ones updated in the same pass.
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      "INSERT INTO Players (team_id, first_name, last_name, age, role, "
      "nationality, wage, contract_years, height, foot, stats, status, "
      "potential, traits, dynamics, id) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET team_id = excluded.team_id, "
      "first_name = excluded.first_name, last_name = excluded.last_name, "
      "age = excluded.age, role = excluded.role, "
      "nationality = excluded.nationality, wage = excluded.wage, "
      "contract_years = excluded.contract_years, height = excluded.height, "
      "foot = excluded.foot, stats = excluded.stats, "
      "status = excluded.status, potential = excluded.potential, "
      "traits = excluded.traits, dynamics = excluded.dynamics;");
  for (const auto& player_ref : players)
  {
    const Player& player = player_ref.get();
    bindPlayerParams(stmt, player, 1);
    sqlite3_bind_int(stmt, PLAYER_PARAM_COUNT + 1,
                     static_cast<int>(player.getId()));
    db_conn->executeStep(stmt);
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);
  }
  sqlite3_finalize(stmt);
}

void PlayerRepository::deletePlayer(PlayerID player_id) const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::DELETE_PLAYER));
  sqlite3_bind_int(stmt, 1, static_cast<int>(player_id));

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void PlayerRepository::transferPlayer(PlayerID player_id,
                                      uint16_t new_team_id) const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::TRANSFER_PLAYER));

  sqlite3_bind_int(stmt, 1, new_team_id);
  sqlite3_bind_int(stmt, 2, static_cast<int>(player_id));

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}
