// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/standings_scene.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <format>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/competition.h"
#include "model/draw_ceremony.h"
#include "model/game.h"

namespace
{
constexpr float BADGE_HEIGHT = 16.0f;
/** Kit-coloured mini shield in front of a club name (same line). */
void teamBadge(const GameController& controller, TeamID team)
{
  const ClubIdentity* identity = controller.getClubIdentity(team);
  if (identity == nullptr) return;
  UI::clubBadge(nullptr, identity->primary_colour, identity->secondary_colour,
                BADGE_HEIGHT);
  ImGui::SameLine(0.0f, Theme::Space::S * Theme::scale());
}

// League table columns (unscaled widths); priority 0 never hides.
const std::array<UI::Column, 11>& standingsColumns()
{
  static const std::array<UI::Column, 11> columns = {{
      {"MAIN_GAME_POS", 36.0f, 0},
      {"MAIN_GAME_TEAM", 0.0f, 0},
      {"TABLE_COL_PLAYED", 34.0f, 1},
      {"TABLE_COL_WON", 34.0f, 3},
      {"TABLE_COL_DRAWN", 34.0f, 3},
      {"TABLE_COL_LOST", 34.0f, 3},
      {"TABLE_COL_GF", 38.0f, 4},
      {"TABLE_COL_GA", 38.0f, 4},
      {"TABLE_COL_GD", 46.0f, 2},
      {"MAIN_GAME_PTS", 44.0f, 0},
      {"TABLE_COL_FORM", 118.0f, 2},
  }};
  return columns;
}

constexpr const char* TABLE_KEY = "standings";

/** Short badge code and tooltip of a decided status. */
struct ClinchLabel
{
  const char* code_key;
  const char* tooltip_key;
};

ClinchLabel clinchLabel(Standings::Clinch status)
{
  using Standings::Clinch;
  switch (status)
  {
    case Clinch::CHAMPION:
      return {"STANDINGS_CLINCH_CHAMPION_SHORT", "STANDINGS_CLINCH_CHAMPION"};
    case Clinch::PROMOTED:
      return {"STANDINGS_CLINCH_PROMOTED_SHORT", "STANDINGS_CLINCH_PROMOTED"};
    case Clinch::PLAY_OFF:
      return {"STANDINGS_CLINCH_PLAY_OFF_SHORT", "STANDINGS_CLINCH_PLAY_OFF"};
    case Clinch::CONTINENTAL_TOP:
      return {"STANDINGS_CLINCH_CONTINENTAL_SHORT",
              "STANDINGS_CLINCH_CONTINENTAL_TOP"};
    case Clinch::CONTINENTAL:
      return {"STANDINGS_CLINCH_CONTINENTAL_SHORT",
              "STANDINGS_CLINCH_CONTINENTAL"};
    case Clinch::SAFE:
      return {"STANDINGS_CLINCH_SAFE_SHORT", "STANDINGS_CLINCH_SAFE"};
    case Clinch::RELEGATED:
      return {"STANDINGS_CLINCH_RELEGATED_SHORT", "STANDINGS_CLINCH_RELEGATED"};
    case Clinch::OPEN:
      break;
  }
  return {nullptr, nullptr};
}

ImVec4 clinchColor(Standings::Clinch status)
{
  const Theme::Palette& palette = Theme::palette();
  switch (status)
  {
    case Standings::Clinch::CHAMPION:
    case Standings::Clinch::PROMOTED:
      return palette.positive;
    case Standings::Clinch::PLAY_OFF:
    case Standings::Clinch::CONTINENTAL_TOP:
    case Standings::Clinch::CONTINENTAL:
      return palette.info;
    case Standings::Clinch::RELEGATED:
      return palette.negative;
    case Standings::Clinch::SAFE:
    case Standings::Clinch::OPEN:
      break;
  }
  return palette.muted;
}

/** "win the title", "stay up", ... (completes the need sentences). */
const char* raceGoalKey(Standings::Race race)
{
  switch (race)
  {
    case Standings::Race::TITLE:
      return "STANDINGS_GOAL_TITLE";
    case Standings::Race::PROMOTION:
      return "STANDINGS_GOAL_PROMOTION";
    case Standings::Race::PLAY_OFF:
      return "STANDINGS_GOAL_PLAY_OFF";
    case Standings::Race::CONTINENTAL_TOP:
      return "STANDINGS_GOAL_CONTINENTAL_TOP";
    case Standings::Race::CONTINENTAL:
      return "STANDINGS_GOAL_CONTINENTAL";
    case Standings::Race::SURVIVAL:
      break;
  }
  return "STANDINGS_GOAL_SURVIVAL";
}

/** Whole sentence for a race the club can no longer decide alone. */
const char* raceHelpKey(Standings::Race race)
{
  switch (race)
  {
    case Standings::Race::TITLE:
      return "STANDINGS_NEED_HELP_TITLE";
    case Standings::Race::PROMOTION:
      return "STANDINGS_NEED_HELP_PROMOTION";
    case Standings::Race::PLAY_OFF:
      return "STANDINGS_NEED_HELP_PLAY_OFF";
    case Standings::Race::CONTINENTAL_TOP:
      return "STANDINGS_NEED_HELP_CONTINENTAL_TOP";
    case Standings::Race::CONTINENTAL:
      return "STANDINGS_NEED_HELP_CONTINENTAL";
    case Standings::Race::SURVIVAL:
      break;
  }
  return "STANDINGS_NEED_HELP_SURVIVAL";
}

std::string needSentence(const Standings::RaceNeed& need,
                         const std::string& opponent)
{
  if (need.kind == Standings::RaceNeed::Kind::NEEDS_HELP)
    return LOC(raceHelpKey(need.race));
  const char* goal = LOC(raceGoalKey(need.race));
  const int points = need.points;
  const int games = need.games_left;
  if (games == 1)
  {
    const char* key = points <= 1
                          ? (need.next_at_home ? "STANDINGS_NEED_DRAW_HOME"
                                               : "STANDINGS_NEED_DRAW_AWAY")
                          : (need.next_at_home ? "STANDINGS_NEED_WIN_HOME"
                                               : "STANDINGS_NEED_WIN_AWAY");
    return fmt::sprintf(LOC(key), opponent.c_str(), goal);
  }
  if (points >= 3 * games)
    return fmt::sprintf(LOC("STANDINGS_NEED_WIN_ALL"), games, goal);
  if (points % 3 == 0)
    return fmt::sprintf(LOC("STANDINGS_NEED_WINS"), points / 3, games, goal);
  if (points == 1)
    return fmt::sprintf(LOC("STANDINGS_NEED_POINT"), games, goal);
  return fmt::sprintf(LOC("STANDINGS_NEED_POINTS"), points, games, goal);
}

constexpr size_t MAX_SCORERS = 15;
constexpr float SCORERS_WIDTH = 300.0f;
constexpr float SCORERS_MIN_WIDTH = 980.0f;
constexpr float TIE_WIDTH = 230.0f;

}  // namespace

StandingsScene::StandingsScene(GUIView* parent) : ManagementScene(parent) {}

void StandingsScene::update(float /*deltaTime*/) {}

void StandingsScene::refresh()
{
  GameController& controller = guiView->getController();
  if (!league_chosen)
  {
    if (const auto managed = controller.getManagedTeam())
      league_id = managed->get().getLeagueId();
    else if (!controller.getLeagues().empty())
      league_id = controller.getLeagues().front().get().getId();
    if (const auto data = controller.getGameData())
      cup_id = Competitions::rootLeague(*data, league_id);
    league_chosen = true;
  }
  cup_names.clear();
  for (const LeagueID id : controller.getCupIds())
    cup_names.emplace_back(id, controller.getCupName(id));

  if (showing_cup)
  {
    cup = controller.getCupStatus(cup_id);
    return;
  }
  past_seasons = controller.getArchivedSeasons(league_id);
  if (past_season && !std::ranges::contains(past_seasons, *past_season))
    past_season.reset();
  zones = CompetitionView::zonesFor(controller, league_id);
  scorers.clear();
  clinch.clear();
  need_lines.clear();
  const auto data = controller.getGameData();
  if (past_season)
  {
    refreshPastSeason(*past_season);
    return;
  }
  table = CompetitionView::buildStandings(controller, league_id);
  refreshRace();
  for (const PlayerSeasonStats& stats :
       controller.getTopScorers(MatchType::LEAGUE, league_id, MAX_SCORERS))
  {
    // Players can retire mid-history; skip ids that no longer resolve.
    const auto player = data ? data->getPlayer(stats.player_id) : std::nullopt;
    if (!player) continue;
    const auto club = controller.getTeamById(stats.team_id);
    scorers.push_back({stats.player_id, player->get().getName(),
                       club ? club->get().getName() : std::string(),
                       stats.goals, stats.assists});
  }
}

void StandingsScene::refreshPastSeason(uint16_t season)
{
  GameController& controller = guiView->getController();
  table.clear();
  if (const std::vector<StandingRow>* rows =
          controller.getArchivedTable(season, league_id))
    for (const StandingRow& archived : *rows)
    {
      CompetitionView::StandingRow row;
      row.team_id = archived.team_id;
      const auto team = controller.getTeamById(archived.team_id);
      row.name = team ? team->get().getName() : std::string("–");
      row.played = archived.played;
      row.won = archived.won;
      row.drawn = archived.drawn;
      row.lost = archived.lost;
      row.goals_for = archived.goals_for;
      row.goals_against = archived.goals_against;
      row.points = archived.points;
      table.push_back(std::move(row));
    }
  // The season record keeps the league's top scorer only.
  const auto data = controller.getGameData();
  for (const SeasonHistoryEntry& entry : controller.getSeasonHistory())
  {
    if (entry.season != season || entry.competition_id != league_id ||
        entry.competition_type != MatchType::LEAGUE || entry.top_scorer_id == 0)
      continue;
    const auto player =
        data ? data->getPlayer(entry.top_scorer_id) : std::nullopt;
    if (!player) continue;
    const auto club = controller.getTeamById(player->get().getTeamId());
    scorers.push_back({entry.top_scorer_id, player->get().getName(),
                       club ? club->get().getName() : std::string(),
                       entry.top_scorer_goals, 0});
  }
}

void StandingsScene::refreshRace()
{
  GameController& controller = guiView->getController();
  clinch.assign(table.size(), Standings::Clinch::OPEN);
  places = {};
  const Game* game = controller.getGame();
  const auto data = controller.getGameData();
  if (game == nullptr || !data) return;
  const auto league = data->getLeague(league_id);
  if (!league) return;
  places = Standings::racePlaces(*data, controller.getContinental(), league_id);
  // Before the first round nothing can be decided (and the table is
  // alphabetical).
  if (std::ranges::none_of(table,
                           [](const auto& row) { return row.played > 0; }))
    return;
  const Standings::RaceInput input =
      Standings::raceInput(league->get(), game->getCalendar(), *data);
  for (const Standings::ClinchReport& report :
       Standings::clinchReports(input, places))
  {
    const auto row = std::ranges::find(table, report.team_id,
                                       &CompetitionView::StandingRow::team_id);
    if (row != table.end())
      clinch[static_cast<size_t>(row - table.begin())] = report.status;
  }

  const auto managed = controller.getManagedTeam();
  if (!managed || managed->get().getLeagueId() != league_id) return;
  const std::vector<Standings::RaceNeed> needs =
      Standings::whatYouNeed(input, places, managed->get().getId());
  if (needs.empty()) return;
  // The most ambitious open race; if it is out of the club's hands, also
  // the best one it can still settle alone; survival whenever it is open.
  std::vector<const Standings::RaceNeed*> shown = {&needs.front()};
  if (needs.front().kind == Standings::RaceNeed::Kind::NEEDS_HELP)
  {
    const auto own = std::ranges::find(
        needs, Standings::RaceNeed::Kind::IN_HANDS, &Standings::RaceNeed::kind);
    if (own != needs.end()) shown.push_back(&*own);
  }
  const auto survival = std::ranges::find(needs, Standings::Race::SURVIVAL,
                                          &Standings::RaceNeed::race);
  if (survival != needs.end() && shown.size() < 2 &&
      !std::ranges::contains(shown, &*survival))
    shown.push_back(&*survival);
  const auto opponent = controller.getTeamById(needs.front().next_opponent);
  const std::string opponent_name =
      opponent ? opponent->get().getName() : std::string();
  for (const Standings::RaceNeed* need : shown)
    need_lines.push_back(needSentence(*need, opponent_name));
}

void StandingsScene::renderNeeds()
{
  const Theme::Palette& palette = Theme::palette();
  {
    Theme::ScopedText caption(Theme::Text::CAPTION);
    ImGui::TextColored(palette.muted, "%s", LOC("STANDINGS_NEED_TITLE"));
  }
  ImGui::PushTextWrapPos(0.0f);
  for (const std::string& line : need_lines)
    ImGui::TextUnformatted(line.c_str());
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
}

std::string StandingsScene::seasonLabel(uint16_t season) const
{
  const uint16_t start = guiView->getController().getSeasonStartYear(season);
  return std::format("{}/{:02}", start, (start + 1) % 100);
}

void StandingsScene::renderContent()
{
  UI::pageHeader(LOC("STANDINGS_TITLE"), LOC("STANDINGS_SUBTITLE"));
  renderCompetitionSelector();
  if (showing_cup)
  {
    renderCup();
    return;
  }
  if (table.empty())
  {
    UI::emptyState(LOC("STANDINGS_EMPTY_TITLE"), LOC("STANDINGS_EMPTY_BODY"));
    return;
  }
  {
    // Column picker at the right end of the selector line.
    const float pickerWidth =
        UI::buttonWidth(LOC("TABLE_COLUMNS"), UI::ButtonSize::COMPACT);
    if (UI::sameLineIfFits(pickerWidth))
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                           ImGui::GetContentRegionAvail().x - pickerWidth);
    UI::columnPicker(TABLE_KEY, standingsColumns(), table_mask);
  }
  const bool started = std::ranges::any_of(
      table, [](const auto& row) { return row.played > 0; });
  if (started) renderHighlights();
  if (!need_lines.empty()) renderNeeds();
  const bool continental = !past_season && places.continental_top > 0;
  const bool legend =
      zones.promotion > 0 || zones.relegation > 0 || continental;
  const float height = ImGui::GetContentRegionAvail().y -
                       (legend ? ImGui::GetFrameHeightWithSpacing() : 0.0f);
  const float available = ImGui::GetContentRegionAvail().x;
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  if (available >= SCORERS_MIN_WIDTH * Theme::scale())
  {
    const float scorersWidth = SCORERS_WIDTH * Theme::scale();
    renderTable(available - scorersWidth - gap, height);
    ImGui::SameLine();
    renderScorers(scorersWidth, height);
  }
  else
  {
    renderTable(0.0f, height);
  }
  if (!legend) return;
  const Theme::Palette& palette = Theme::palette();
  if (zones.promotion > 0)
  {
    UI::badge(LOC("STANDINGS_ZONE_PROMOTION"), palette.positive);
    ImGui::SameLine();
  }
  if (continental)
  {
    UI::badge(LOC("STANDINGS_ZONE_CONTINENTAL"), palette.info);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip("%s", LOC("STANDINGS_ZONE_CONTINENTAL_HINT"));
    ImGui::SameLine();
  }
  if (zones.relegation > 0)
    UI::badge(LOC("STANDINGS_ZONE_RELEGATION"), palette.negative);
}

void StandingsScene::renderCompetitionSelector()
{
  GameController& controller = guiView->getController();
  std::string preview;
  if (showing_cup)
  {
    const auto name = std::ranges::find(
        cup_names, cup_id, &std::pair<LeagueID, std::string>::first);
    preview = name != cup_names.end() ? name->second : std::string();
  }
  else if (const auto current = controller.getLeagueById(league_id))
  {
    preview = current->get().getName();
  }
  ImGui::SetNextItemWidth(300.0f * Theme::scale());
  if (ImGui::BeginCombo("##competition", preview.c_str(),
                        ImGuiComboFlags_HeightLarge))
  {
    ImGui::SeparatorText(LOC("STANDINGS_GROUP_LEAGUES"));
    for (const auto& leagueRef : controller.getLeagues())
    {
      const League& league = leagueRef.get();
      if (ImGui::Selectable(Competitions::leagueName(league).c_str(),
                            !showing_cup && league.getId() == league_id))
      {
        league_id = league.getId();
        showing_cup = false;
        refresh();
      }
    }
    if (!cup_names.empty()) ImGui::SeparatorText(LOC("STANDINGS_GROUP_CUPS"));
    for (const auto& [id, name] : cup_names)
    {
      ImGui::PushID(static_cast<int>(id) + 0x1000);
      if (ImGui::Selectable(name.c_str(), showing_cup && id == cup_id))
      {
        cup_id = id;
        showing_cup = true;
        refresh();
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  if (!showing_cup && !past_seasons.empty())
  {
    // Final tables of earlier seasons, archived when each season ended.
    const std::string current =
        past_season ? seasonLabel(*past_season)
                    : std::string(LOC("STANDINGS_SEASON_CURRENT"));
    const float width = 170.0f * Theme::scale();
    UI::sameLineIfFits(width);
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo("##season", current.c_str()))
    {
      if (ImGui::Selectable(LOC("STANDINGS_SEASON_CURRENT"), !past_season))
      {
        past_season.reset();
        refresh();
      }
      for (const uint16_t season : past_seasons)
      {
        ImGui::PushID(season);
        if (ImGui::Selectable(seasonLabel(season).c_str(),
                              past_season && *past_season == season))
        {
          past_season = season;
          refresh();
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
  }
  if (!showing_cup && past_season)
  {
    const std::string note =
        fmt::sprintf(LOC("STANDINGS_FINAL_TABLE"), seasonLabel(*past_season));
    UI::sameLineIfFits(ImGui::CalcTextSize(note.c_str()).x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(Theme::palette().faint, "%s", note.c_str());
  }
  else if (!showing_cup)
  {
    UI::sameLineIfFits(ImGui::CalcTextSize(LOC("STANDINGS_TIEBREAK_HELP")).x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(Theme::palette().faint, "%s",
                       LOC("STANDINGS_TIEBREAK_HELP"));
  }
}

void StandingsScene::renderHighlights()
{
  const Theme::Palette& palette = Theme::palette();
  const auto bestAttack = std::ranges::max_element(
      table, {}, &CompetitionView::StandingRow::goals_for);
  const auto bestDefence = std::ranges::min_element(
      table,
      [](const auto& left, const auto& right)
      {
        if (left.played == 0 || right.played == 0)
          return left.played > right.played;
        return left.goals_against < right.goals_against;
      });
  const auto mostWins =
      std::ranges::max_element(table, {}, &CompetitionView::StandingRow::won);
  UI::TileRow tiles(4);
  const float width = tiles.width();

  const std::string leaderNote =
      fmt::sprintf(LOC("STANDINGS_POINTS_NOTE"), table.front().points);
  tiles.next();
  UI::statTile("leader", LOC("STANDINGS_LEADER"), table.front().name.c_str(),
               leaderNote.c_str(), palette.text, width);
  tiles.next();
  const std::string attackNote =
      fmt::sprintf(LOC("STANDINGS_GOALS_SCORED"), bestAttack->goals_for);
  UI::statTile("attack", LOC("STANDINGS_BEST_ATTACK"), bestAttack->name.c_str(),
               attackNote.c_str(), palette.text, width);
  tiles.next();
  const std::string defenceNote =
      fmt::sprintf(LOC("STANDINGS_GOALS_CONCEDED"), bestDefence->goals_against);
  UI::statTile("defence", LOC("STANDINGS_BEST_DEFENCE"),
               bestDefence->name.c_str(), defenceNote.c_str(), palette.text,
               width);
  tiles.next();
  const std::string winsNote =
      fmt::sprintf(LOC("STANDINGS_WINS_NOTE"), mostWins->won);
  UI::statTile("wins", LOC("STANDINGS_MOST_WINS"), mostWins->name.c_str(),
               winsNote.c_str(), palette.text, width);
}

void StandingsScene::renderTable(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  const auto managed = guiView->getController().getManagedTeam();
  const TeamID clubId = managed ? managed->get().getId() : 0;
  UI::beginCard("standings_card", nullptr, ImVec2(width, height));
  // Fills the page height (vertical scroll only); secondary columns hide
  // on narrow windows instead of scrolling sideways.
  // Columns hidden in the picker stay hidden at any width.
  std::array<UI::Column, 11> columns = standingsColumns();
  const UI::ColumnMask hidden = UI::hiddenColumns(TABLE_KEY, columns);
  for (UI::Column& column : columns) column.label = LOC(column.label);
  const UI::ColumnMask mask =
      UI::fitColumns(columns, ImGui::GetContentRegionAvail().x, 120.0f, hidden);
  table_mask = mask;
  const size_t continental = past_season ? 0 : places.continental;
  const ImGuiTableFlags flags = ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_BordersInnerH |
                                ImGuiTableFlags_ScrollY;
  if (UI::beginResponsiveTable("standings", columns, mask, flags))
  {
    for (size_t index = 0; index < table.size(); ++index)
    {
      const CompetitionView::StandingRow& row = table[index];
      ImGui::TableNextRow(ImGuiTableRowFlags_None,
                          ImGui::GetTextLineHeight() + 6.0f * Theme::scale());
      // Before the first round the order is alphabetical: no zones yet.
      const bool promoted = row.played > 0 && index < zones.promotion;
      const bool relegated =
          row.played > 0 && index + zones.relegation >= table.size();
      const bool qualifying = row.played > 0 && index < continental;
      if (row.team_id == clubId)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                               Theme::toU32(palette.accent, 0.16f));
      else if (promoted || relegated || qualifying)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                               Theme::toU32(promoted    ? palette.positive
                                            : relegated ? palette.negative
                                                        : palette.info,
                                            0.07f));
      ImGui::TableNextColumn();
      ImGui::TextColored(promoted     ? palette.positive
                         : relegated  ? palette.negative
                         : qualifying ? palette.info
                                      : palette.muted,
                         "%zu", index + 1);
      ImGui::TableNextColumn();
      ImGui::PushID(static_cast<int>(row.team_id));
      teamBadge(guiView->getController(), row.team_id);
      if (ImGui::Selectable(row.name.c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns |
                                ImGuiSelectableFlags_AllowOverlap))
        Navigation::openClub(guiView, row.team_id);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", LOC("STANDINGS_OPEN_CLUB_HINT"));
      // Mathematically decided outcome right after the name.
      if (const Standings::Clinch status =
              index < clinch.size() ? clinch[index] : Standings::Clinch::OPEN;
          status != Standings::Clinch::OPEN)
      {
        const ClinchLabel label = clinchLabel(status);
        ImGui::SameLine(0.0f, Theme::Space::S * Theme::scale());
        UI::badge(LOC(label.code_key), clinchColor(status));
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s", LOC(label.tooltip_key));
      }
      ImGui::PopID();
      if (UI::cell(mask, 2)) ImGui::Text("%d", row.played);
      if (UI::cell(mask, 3)) ImGui::Text("%d", row.won);
      if (UI::cell(mask, 4)) ImGui::Text("%d", row.drawn);
      if (UI::cell(mask, 5)) ImGui::Text("%d", row.lost);
      if (UI::cell(mask, 6)) ImGui::Text("%d", row.goals_for);
      if (UI::cell(mask, 7)) ImGui::Text("%d", row.goals_against);
      if (UI::cell(mask, 8))
      {
        const int difference = row.goalDifference();
        ImGui::TextColored(difference > 0   ? palette.positive
                           : difference < 0 ? palette.negative
                                            : palette.muted,
                           "%s", Format::signedInt(difference).c_str());
      }
      if (UI::cell(mask, 9)) ImGui::Text("%d", row.points);
      if (UI::cell(mask, 10))
        UI::formStrip(std::span(row.form.data(), row.form_count));
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void StandingsScene::renderScorers(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("scorers_card", LOC("STANDINGS_TOP_SCORERS"),
                ImVec2(width, height), true);
  if (scorers.empty())
  {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(palette.faint, "%s", LOC("STANDINGS_NO_SCORERS"));
    ImGui::PopTextWrapPos();
    UI::endCard();
    return;
  }
  if (ImGui::BeginTable("scorers", 3,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
  {
    ImGui::TableSetupColumn("rank");
    ImGui::TableSetupColumn("player", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("goals");
    for (size_t index = 0; index < scorers.size(); ++index)
    {
      const ScorerRow& scorer = scorers[index];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%zu", index + 1);
      ImGui::TableNextColumn();
      ImGui::PushID(static_cast<int>(scorer.id));
      if (UI::link(scorer.name.c_str(), "scorer"))
        Navigation::openPlayer(guiView, scorer.id);
      ImGui::PopID();
      {
        Theme::ScopedText small(Theme::Text::SMALL);
        ImGui::TextColored(palette.faint, "%s", scorer.club.c_str());
      }
      ImGui::TableNextColumn();
      {
        Theme::ScopedText title(Theme::Text::TITLE);
        ImGui::Text("%u", scorer.goals);
      }
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void StandingsScene::renderCup()
{
  const Theme::Palette& palette = Theme::palette();
  GameController& controller = guiView->getController();
  if (!cup || cup->rounds.empty())
  {
    UI::emptyState(LOC("CUP_EMPTY_TITLE"), LOC("CUP_EMPTY_BODY"));
    return;
  }
  const auto managed = controller.getManagedTeam();
  const TeamID clubId = managed ? managed->get().getId() : 0;
  if (cup->winner)
  {
    ImGui::TextColored(
        palette.text, "%s",
        fmt::sprintf(LOC("CUP_WINNER"),
                     controller.getTeamById(*cup->winner)
                         ? controller.getTeamById(*cup->winner)->get().getName()
                         : std::string())
            .c_str());
  }
  else
  {
    const bool stillIn =
        std::ranges::find(cup->remaining, clubId) != cup->remaining.end();
    ImGui::TextColored(
        palette.muted, "%s",
        fmt::sprintf(LOC("CUP_REMAINING"), cup->remaining.size()).c_str());
    ImGui::SameLine(0.0f, Theme::Space::L * Theme::scale());
    UI::badge(LOC(stillIn ? "CUP_CLUB_IN" : "CUP_CLUB_OUT"),
              stillIn ? palette.positive : palette.faint);
  }

  // The latest draw, revealed again tie by tie.
  const char* watch = LOC("DRAW_WATCH");
  UI::sameLineIfFits(UI::buttonWidth(watch), Theme::Space::L * Theme::scale());
  if (UI::secondaryButton(watch))
  {
    const auto data = controller.getGameData();
    std::optional<DrawCeremony> ceremony;
    if (data && controller.getGame() != nullptr)
      ceremony = DrawCeremonies::latestCupRound(
          controller.getGame()->getCalendar(), *data, cup_id,
          controller.getCurrentDate(), clubId);
    if (ceremony)
      draw_dialog.open(*ceremony, controller);
    else
      showToast(LOC("DRAW_UNAVAILABLE"), true);
  }
  draw_dialog.render(guiView);

  // One column per round; each tie shows both clubs and the score.
  UI::beginCard("cup_bracket", nullptr,
                ImVec2(0.0f, ImGui::GetContentRegionAvail().y));
  const int columns = static_cast<int>(cup->rounds.size());
  if (ImGui::BeginTable("cup_rounds", columns,
                        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_SizingFixedFit))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    for (const Competitions::CupRound& round : cup->rounds)
    {
      const std::string label = fmt::sprintf(
          LOC(Competitions::cupRoundLabelKey(round.stage, cup->total_rounds)),
          round.stage);
      ImGui::TableSetupColumn(label.c_str(), ImGuiTableColumnFlags_WidthFixed,
                              TIE_WIDTH * Theme::scale());
    }
    UI::staticHeadersRow();
    size_t maxTies = 0;
    for (const auto& round : cup->rounds)
      maxTies = std::max(maxTies, round.ties.size());
    for (size_t tie = 0; tie < maxTies; ++tie)
    {
      ImGui::TableNextRow();
      for (const Competitions::CupRound& round : cup->rounds)
      {
        ImGui::TableNextColumn();
        if (tie >= round.ties.size()) continue;
        const Match& match = round.ties[tie];
        const auto winner = match.getWinnerId();
        const bool involvesClub =
            match.getHomeTeamId() == clubId || match.getAwayTeamId() == clubId;
        const auto line = [&](TeamID team, uint8_t goals)
        {
          const auto club = controller.getTeamById(team);
          const bool won = winner && *winner == team;
          ImGui::TextColored(won            ? palette.text
                             : winner       ? palette.faint
                             : involvesClub ? palette.accent
                                            : palette.muted,
                             "%s", club ? club->get().getName().c_str() : "");
          if (match.isPlayed())
          {
            ImGui::SameLine();
            UI::textRightColored(won ? palette.positive : palette.muted,
                                 std::to_string(goals).c_str());
          }
        };
        ImGui::PushID(static_cast<int>(tie * 64 + round.stage));
        ImGui::BeginGroup();
        line(match.getHomeTeamId(), match.getHomeScore());
        line(match.getAwayTeamId(), match.getAwayScore());
        {
          Theme::ScopedText small(Theme::Text::CAPTION);
          if (!match.isPlayed())
            ImGui::TextColored(palette.faint, "%s",
                               Format::dayMonth(match.getDate()).c_str());
          else if (match.wentToPenalties())
            ImGui::TextColored(
                palette.faint, "%s",
                fmt::sprintf(LOC("RESULT_PENALTIES"), match.getHomePenalties(),
                             match.getAwayPenalties())
                    .c_str());
          else if (match.wentToExtraTime())
            ImGui::TextColored(palette.faint, "%s",
                               LOC("RESULT_AFTER_EXTRA_TIME"));
          else
            ImGui::TextColored(palette.faint, " ");
        }
        ImGui::EndGroup();
        if (match.isPlayed() && ImGui::IsItemClicked())
          Navigation::openMatchReport(guiView, match.getDate(),
                                      match.getHomeTeamId(),
                                      match.getAwayTeamId());
        ImGui::PopID();
      }
    }
    ImGui::EndTable();
  }
  UI::endCard();
}
