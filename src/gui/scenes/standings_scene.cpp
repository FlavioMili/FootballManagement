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

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"

namespace
{
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
  table = CompetitionView::buildStandings(controller, league_id);
  zones = CompetitionView::zonesFor(controller, league_id);
  scorers.clear();
  const auto data = controller.getGameData();
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
  const bool started = std::ranges::any_of(
      table, [](const auto& row) { return row.played > 0; });
  if (started) renderHighlights();
  const bool legend = zones.promotion > 0 || zones.relegation > 0;
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
      if (ImGui::Selectable(league.getName().c_str(),
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
  if (!showing_cup)
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
  const ImGuiTableFlags flags =
      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
  if (UI::beginDataTable("standings", 11, flags, 620.0f, ImVec2(0.0f, 0.0f), 2))
  {
    ImGui::TableSetupColumn(LOC("MAIN_GAME_POS"));
    ImGui::TableSetupColumn(LOC("MAIN_GAME_TEAM"),
                            ImGuiTableColumnFlags_WidthStretch, 0.0f);
    ImGui::TableSetupColumn(LOC("TABLE_COL_PLAYED"));
    ImGui::TableSetupColumn(LOC("TABLE_COL_WON"));
    ImGui::TableSetupColumn(LOC("TABLE_COL_DRAWN"));
    ImGui::TableSetupColumn(LOC("TABLE_COL_LOST"));
    ImGui::TableSetupColumn(LOC("TABLE_COL_GF"));
    ImGui::TableSetupColumn(LOC("TABLE_COL_GA"));
    ImGui::TableSetupColumn(LOC("TABLE_COL_GD"));
    ImGui::TableSetupColumn(LOC("MAIN_GAME_PTS"));
    ImGui::TableSetupColumn(LOC("TABLE_COL_FORM"));
    UI::staticHeadersRow();
    for (size_t index = 0; index < table.size(); ++index)
    {
      const CompetitionView::StandingRow& row = table[index];
      ImGui::TableNextRow(ImGuiTableRowFlags_None,
                          ImGui::GetTextLineHeight() + 6.0f * Theme::scale());
      // Before the first round the order is alphabetical: no zones yet.
      const bool promoted = row.played > 0 && index < zones.promotion;
      const bool relegated =
          row.played > 0 && index + zones.relegation >= table.size();
      if (row.team_id == clubId)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                               Theme::toU32(palette.accent, 0.16f));
      else if (promoted || relegated)
        ImGui::TableSetBgColor(
            ImGuiTableBgTarget_RowBg1,
            Theme::toU32(promoted ? palette.positive : palette.negative,
                         0.07f));
      ImGui::TableNextColumn();
      ImGui::TextColored(promoted    ? palette.positive
                         : relegated ? palette.negative
                                     : palette.muted,
                         "%zu", index + 1);
      ImGui::TableNextColumn();
      ImGui::PushID(static_cast<int>(row.team_id));
      if (ImGui::Selectable(row.name.c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns))
        Navigation::openClub(guiView, row.team_id);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", LOC("STANDINGS_OPEN_CLUB_HINT"));
      ImGui::PopID();
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.played);
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.won);
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.drawn);
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.lost);
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.goals_for);
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.goals_against);
      ImGui::TableNextColumn();
      const int difference = row.goalDifference();
      ImGui::TextColored(difference > 0   ? palette.positive
                         : difference < 0 ? palette.negative
                                          : palette.muted,
                         "%s", Format::signedInt(difference).c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.points);
      ImGui::TableNextColumn();
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
