// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "player_repository.h"

#include <sqlite3.h>

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

    players.emplace_back(id, team_id, first_name, last_name, playerRole,
                         nationality, wage, status, age, contract_years, height,
                         foot, std::move(stats));
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
  sqlite3_bind_int(stmt, 13, static_cast<int>(player.getId()));

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void PlayerRepository::updatePlayers(
    const std::vector<std::reference_wrapper<const Player>>& players) const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::UPDATE_PLAYER));
  for (const auto& player_ref : players)
  {
    const Player& player = player_ref.get();
    bindPlayerParams(stmt, player, 1);
    sqlite3_bind_int(stmt, 13, static_cast<int>(player.getId()));
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
