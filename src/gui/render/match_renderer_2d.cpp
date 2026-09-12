// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/render/match_renderer_2d.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "global/language_manager.h"
#include "gui/render/match_kit_colors.h"
#include "model/player.h"
#include "model/role_utils.h"

namespace
{
float worldToScreenX(float worldX, const MatchViewport& viewport)
{
  return viewport.x + worldX * viewport.width;
}

float worldToScreenY(float worldY, const MatchViewport& viewport)
{
  return viewport.y + worldY * viewport.height;
}

ImVec2 worldToScreen(Vector2F world, const MatchViewport& viewport)
{
  return {worldToScreenX(world.x, viewport), worldToScreenY(world.y, viewport)};
}

/// Deterministic row/column hash used to place crowd dots without RNG.
/// Renderers must never call the simulation RNG, so this is a fixed pure mix.
float crowdHash(std::int32_t x, std::int32_t y)
{
  const std::uint32_t seed =
      static_cast<std::uint32_t>(MatchSceneTuning::Stadium::CROWD_SEED);
  std::uint32_t value = static_cast<std::uint32_t>(x) * 73856093U ^
                        static_cast<std::uint32_t>(y) * 19349663U ^ seed;
  value ^= value >> 13;
  value *= 0x5bd1e995U;
  value ^= value >> 15;
  return static_cast<float>(value & 0xffffU) / 65535.0f;
}

/** Player names are drawn at this share of the UI font size. */
constexpr float NAME_FONT_SCALE = 0.82f;

void drawStadium(ImDrawList& drawList, const MatchViewport& viewport)
{
  const float apron = MatchSceneTuning::Stadium::APRON_WIDTH;
  const ImVec2 surroundMin{viewport.x - apron, viewport.y - apron};
  const ImVec2 surroundMax{viewport.x + viewport.width + apron,
                           viewport.y + viewport.height + apron};
  // Running track / surrounds between the stands and the touchline.
  drawList.AddRectFilled(surroundMin, surroundMax,
                         MatchSceneTuning::Stadium::SURROUND_COLOR);

  const float standHeight = MatchSceneTuning::Stadium::STAND_HEIGHT;
  const float roofHeight = MatchSceneTuning::Stadium::ROOF_HEIGHT;

  const auto drawStandBand =
      [&](float yStart, float height, float innerX, float innerWidth)
  {
    drawList.AddRectFilled({innerX, yStart},
                           {innerX + innerWidth, yStart + height},
                           MatchSceneTuning::Stadium::STAND_SHADOW_COLOR);
    drawList.AddRectFilled({innerX, yStart},
                           {innerX + innerWidth, yStart + height},
                           MatchSceneTuning::Stadium::STAND_COLOR);
    drawList.AddRectFilled({innerX, yStart},
                           {innerX + innerWidth, yStart + roofHeight},
                           MatchSceneTuning::Stadium::ROOF_COLOR);
    const float step = MatchSceneTuning::Stadium::CROWD_DOT_STEP;
    const float radius = MatchSceneTuning::Stadium::CROWD_DOT_RADIUS;
    for (float x = innerX + step; x < innerX + innerWidth; x += step)
    {
      for (float y = yStart + roofHeight + step; y < yStart + height - step;
           y += step)
      {
        const int cellX = static_cast<int>(x / step);
        const int cellY = static_cast<int>(y / step);
        const float hash = crowdHash(cellX, cellY);
        const float jitterX = (hash - 0.5f) * step * 0.6f;
        const float jitterY =
            ((crowdHash(cellY + 7, cellX + 3)) - 0.5f) * step * 0.6f;
        const int colorIndex =
            static_cast<int>(hash *
                             MatchSceneTuning::Stadium::CROWD_COLOR_COUNT) %
            MatchSceneTuning::Stadium::CROWD_COLOR_COUNT;
        const ImU32 color =
            colorIndex == 0   ? MatchSceneTuning::Stadium::CROWD_A_COLOR
            : colorIndex == 1 ? MatchSceneTuning::Stadium::CROWD_B_COLOR
                              : MatchSceneTuning::Stadium::CROWD_C_COLOR;
        drawList.AddCircleFilled({x + jitterX, y + jitterY}, radius, color);
      }
    }
  };

  // Top and bottom stands span the whole apron width; left/right only where
  // they do not overlap the ends (goals and ad boards are drawn by the pitch).
  drawStandBand(surroundMin.y, standHeight, surroundMin.x,
                surroundMax.x - surroundMin.x);
  drawStandBand(surroundMax.y - standHeight, standHeight, surroundMin.x,
                surroundMax.x - surroundMin.x);
  const float cornerInset = MatchSceneTuning::Pitch::GOAL_DEPTH;
  drawStandBand(surroundMin.y, surroundMax.y - surroundMin.y, surroundMin.x,
                apron - cornerInset);
  drawStandBand(surroundMin.y, surroundMax.y - surroundMin.y,
                surroundMax.x - apron + cornerInset, apron - cornerInset);
}

#ifdef DEBUG
ImU32 intentDebugColor(PlayerIntent intent)
{
  if (intent == PlayerIntent::RUN_IN_BEHIND ||
      intent == PlayerIntent::ATTACK_BOX || intent == PlayerIntent::OVERLAP)
    return MatchSceneTuning::Marker::DEBUG_RUN_COLOR;
  if (intent == PlayerIntent::PRESS_BALL ||
      intent == PlayerIntent::CLAIM_LOOSE_BALL ||
      intent == PlayerIntent::COVER_PRESS ||
      intent == PlayerIntent::BLOCK_PASSING_LANE)
  {
    return MatchSceneTuning::Marker::DEBUG_PRESS_COLOR;
  }
  if (intent == PlayerIntent::OFFER_SUPPORT ||
      intent == PlayerIntent::RECEIVE_PASS ||
      intent == PlayerIntent::CARRY_BALL)
  {
    return MatchSceneTuning::Marker::DEBUG_SUPPORT_COLOR;
  }
  return MatchSceneTuning::Marker::DEBUG_TARGET_COLOR;
}
#endif

void drawGoalFrame(ImDrawList& drawList, bool leftGoal, float lineX, float top,
                   float bottom, float depthPx);

/**
 * Real pitch geometry in metres (Law 1). Everything on the 2D pitch is drawn
 * from these at the viewport's pixels-per-metre scale, so the markings,
 * players and ball keep their true proportions at any window size.
 */
namespace PitchMetres
{
constexpr float LENGTH = MatchTuning::Pitch::LENGTH_METRES;
constexpr float WIDTH = MatchTuning::Pitch::WIDTH_METRES;
constexpr float LINE = 0.12f;
constexpr float CENTRE_CIRCLE = 9.15f;
constexpr float PENALTY_AREA_DEPTH = 16.5f;
constexpr float PENALTY_AREA_WIDTH = 40.32f;
constexpr float GOAL_AREA_DEPTH = 5.5f;
constexpr float GOAL_AREA_WIDTH = 18.32f;
constexpr float PENALTY_SPOT = 11.0f;
constexpr float SPOT_RADIUS = 0.2f;
constexpr float CORNER_ARC = 1.0f;
constexpr float GOAL_WIDTH = 7.32f;
constexpr float GOAL_DEPTH = 2.0f;
/** Player marker ~1.8 m across (a body plus shoulders seen from above). */
constexpr float PLAYER_RADIUS = 0.9f;
constexpr float PLAYER_OUTLINE = 0.18f;
constexpr float DIRECTION_LENGTH = 1.7f;
/** The ball is drawn larger than life (0.7 m) so it stays readable. */
constexpr float BALL_RADIUS = 0.35f;
constexpr float MIN_LINE_PIXELS = 1.0f;
constexpr float MIN_PLAYER_PIXELS = 3.0f;
constexpr float MIN_BALL_PIXELS = 2.0f;
}  // namespace PitchMetres

float pixelsPerMetre(const MatchViewport& viewport)
{
  return viewport.width / PitchMetres::LENGTH;
}

void drawPitch(ImDrawList& drawList, const MatchViewport& viewport)
{
  namespace M = PitchMetres;
  constexpr ImU32 line_color = MatchSceneTuning::Pitch::LINE_COLOR;
  const float ppm = pixelsPerMetre(viewport);
  const float line = std::max(M::MIN_LINE_PIXELS, M::LINE * ppm);
  const ImVec2 p_min{viewport.x, viewport.y};
  const ImVec2 p_max{viewport.x + viewport.width, viewport.y + viewport.height};

  // Alternating mowing bands give the pitch depth without texture assets.
  const float stripeWidth =
      viewport.width /
      static_cast<float>(MatchSceneTuning::Pitch::MOWING_STRIPE_COUNT);
  for (int stripe = 0; stripe < MatchSceneTuning::Pitch::MOWING_STRIPE_COUNT;
       ++stripe)
  {
    const ImU32 color =
        stripe % MatchSceneTuning::Pitch::MOWING_COLOR_PERIOD == 0
            ? MatchSceneTuning::Pitch::GRASS_COLOR
            : MatchSceneTuning::Pitch::ALTERNATE_GRASS_COLOR;
    drawList.AddRectFilled(
        ImVec2(p_min.x + static_cast<float>(stripe) * stripeWidth, p_min.y),
        ImVec2(p_min.x + static_cast<float>(stripe + 1) * stripeWidth, p_max.y),
        color);
  }
  drawList.AddRect(p_min, p_max, line_color, 0.0f, ImDrawFlags_None, line);

  const float center_x = p_min.x + viewport.width * 0.5f;
  const float center_y = p_min.y + viewport.height * 0.5f;
  drawList.AddLine(ImVec2(center_x, p_min.y), ImVec2(center_x, p_max.y),
                   line_color, line);
  drawList.AddCircle(ImVec2(center_x, center_y), M::CENTRE_CIRCLE * ppm,
                     line_color, MatchSceneTuning::Pitch::CENTRE_CIRCLE_SEGMENTS,
                     line);
  drawList.AddCircleFilled(ImVec2(center_x, center_y),
                           std::max(line, M::SPOT_RADIUS * ppm), line_color);

  const auto drawArea = [&](float depth, float width)
  {
    const float top = center_y - width * 0.5f * ppm;
    const float bottom = center_y + width * 0.5f * ppm;
    drawList.AddRect(ImVec2(p_min.x, top), ImVec2(p_min.x + depth * ppm, bottom),
                     line_color, 0.0f, ImDrawFlags_None, line);
    drawList.AddRect(ImVec2(p_max.x - depth * ppm, top), ImVec2(p_max.x, bottom),
                     line_color, 0.0f, ImDrawFlags_None, line);
  };
  drawArea(M::PENALTY_AREA_DEPTH, M::PENALTY_AREA_WIDTH);
  drawArea(M::GOAL_AREA_DEPTH, M::GOAL_AREA_WIDTH);
  const float spotRadius = std::max(line, M::SPOT_RADIUS * ppm);
  drawList.AddCircleFilled(ImVec2(p_min.x + M::PENALTY_SPOT * ppm, center_y),
                           spotRadius, line_color);
  drawList.AddCircleFilled(ImVec2(p_max.x - M::PENALTY_SPOT * ppm, center_y),
                           spotRadius, line_color);

  // Goals extend beyond the goal line. A back panel, posts, crossbar and net
  // mesh make the ball visibly enter the goal mouth instead of vanishing.
  const float goalTop = center_y - M::GOAL_WIDTH * 0.5f * ppm;
  const float goalBottom = center_y + M::GOAL_WIDTH * 0.5f * ppm;
  const float goalDepth = M::GOAL_DEPTH * ppm;
  drawGoalFrame(drawList, true, p_min.x, goalTop, goalBottom, goalDepth);
  drawGoalFrame(drawList, false, p_max.x, goalTop, goalBottom, goalDepth);

  const float corner_r = M::CORNER_ARC * ppm;
  const float PI = std::numbers::pi_v<float>;
  const int segments = MatchSceneTuning::Pitch::CORNER_ARC_SEGMENTS;
  drawList.PathArcTo(p_min, corner_r, 0.0f, PI * 0.5f, segments);
  drawList.PathStroke(line_color, ImDrawFlags_None, line);
  drawList.PathArcTo(ImVec2(p_max.x, p_min.y), corner_r, PI * 0.5f, PI,
                     segments);
  drawList.PathStroke(line_color, ImDrawFlags_None, line);
  drawList.PathArcTo(ImVec2(p_min.x, p_max.y), corner_r, -PI * 0.5f, 0.0f,
                     segments);
  drawList.PathStroke(line_color, ImDrawFlags_None, line);
  drawList.PathArcTo(p_max, corner_r, PI, PI * 1.5f, segments);
  drawList.PathStroke(line_color, ImDrawFlags_None, line);
}

/// Draws a single goal: back panel, white posts and crossbar, and a net mesh
/// spanning the goal mouth. `lineX` is the goal line, the net lies between it
/// and `lineX -/+ depthPx`.
void drawGoalFrame(ImDrawList& drawList, bool leftGoal, float lineX, float top,
                   float bottom, float depthPx)
{
  const float backX = leftGoal ? lineX - depthPx : lineX + depthPx;
  const float postThickness = MatchSceneTuning::GoalFrame::POST_THICKNESS;
  const float netThickness = MatchSceneTuning::GoalFrame::NET_LINE_THICKNESS;

  // Goal mouth back panel from net to goal line.
  drawList.AddRectFilled({std::min(backX, lineX), top},
                         {std::max(backX, lineX), bottom},
                         MatchSceneTuning::GoalFrame::NET_COLOR);

  // Side posts.
  drawList.AddLine({lineX, top}, {backX, top},
                   MatchSceneTuning::GoalFrame::POST_COLOR, postThickness);
  drawList.AddLine({lineX, bottom}, {backX, bottom},
                   MatchSceneTuning::GoalFrame::POST_COLOR, postThickness);
  // Crossbar along the goal line.
  drawList.AddLine({lineX, top}, {lineX, bottom},
                   MatchSceneTuning::GoalFrame::POST_COLOR, postThickness);

  // Net mesh: vertical strands across the depth, horizontal strands down the
  // mouth, so a ball inside the goal is visible against the grid.
  const float span = bottom - top;
  const float origin = std::min(backX, lineX);
  const int verticalLines = MatchSceneTuning::GoalFrame::NET_VERTICAL_LINES;
  for (int i = 1; i < verticalLines; ++i)
  {
    const float fraction =
        static_cast<float>(i) / static_cast<float>(verticalLines);
    const float x = origin + std::abs(depthPx) * fraction;
    drawList.AddLine({x, top}, {x, bottom},
                     MatchSceneTuning::GoalFrame::NET_COLOR, netThickness);
  }
  const int horizontalLines = MatchSceneTuning::GoalFrame::NET_HORIZONTAL_LINES;
  for (int i = 1; i < horizontalLines; ++i)
  {
    const float fraction =
        static_cast<float>(i) / static_cast<float>(horizontalLines);
    const float y = top + span * fraction;
    drawList.AddLine({std::min(backX, lineX), y}, {std::max(backX, lineX), y},
                     MatchSceneTuning::GoalFrame::NET_COLOR, netThickness);
  }
}

void drawPlayer(ImDrawList& drawList, const MatchRenderPlayer& player,
                float alpha, const MatchViewport& viewport, const MatchKits& kits,
                bool showName)
{
  namespace M = PitchMetres;
  const float ppm = pixelsPerMetre(viewport);
  const Vector2F interpolated = lerpRenderPosition(
      player.previousPosition, player.currentPosition, alpha);
  const ImVec2 pos = worldToScreen(interpolated, viewport);
  const float radius = std::max(M::MIN_PLAYER_PIXELS, M::PLAYER_RADIUS * ppm);
  const float outline = std::max(1.0f, M::PLAYER_OUTLINE * ppm);

  const KitColors& kit =
      player.isGoalkeeper
          ? (player.isHomeTeam ? kits.homeGoalkeeper : kits.awayGoalkeeper)
          : (player.isHomeTeam ? kits.home : kits.away);
  drawList.AddCircleFilled(ImVec2(pos.x + outline, pos.y + outline),
                           radius + outline,
                           MatchSceneTuning::Marker::PLAYER_SHADOW_COLOR);
  drawList.AddCircleFilled(pos, radius + outline, kit.trim);
  drawList.AddCircleFilled(pos, radius, kit.shirt);
  if (player.possessesBall)
  {
    drawList.AddCircle(pos, radius + outline * 3.0f,
                       MatchSceneTuning::Marker::POSSESSION_RING_COLOR, 0,
                       std::max(1.0f, outline));
  }

  // Direction pointer.
  const float facingAngle =
      player.previousFacingAngle +
      (player.currentFacingAngle - player.previousFacingAngle) * alpha;
  const float dirLen = std::max(radius * 1.6f, M::DIRECTION_LENGTH * ppm);
  const ImVec2 dirEnd(pos.x + std::cos(facingAngle) * dirLen,
                      pos.y + std::sin(facingAngle) * dirLen);
  drawList.AddLine(pos, dirEnd, MatchSceneTuning::Marker::DIRECTION_COLOR,
                   std::max(1.0f, outline));

  // Names sit centred below the marker at a small size; by default only the
  // ball carrier and a hovered player are labelled so formations stay
  // readable.
  const ImVec2 mouse = ImGui::GetMousePos();
  const float hoverRadius = radius + MatchSceneTuning::Marker::HOVER_RADIUS * 0.5f;
  const bool hovered = (mouse.x - pos.x) * (mouse.x - pos.x) +
                           (mouse.y - pos.y) * (mouse.y - pos.y) <
                       hoverRadius * hoverRadius;
  if (player.player && (showName || player.possessesBall || hovered))
  {
    const float fontSize = ImGui::GetFontSize() * NAME_FONT_SCALE;
    const std::string& name = player.player->getName();
    const ImVec2 size =
        ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, name.c_str());
    const ImVec2 textPos{pos.x - size.x * 0.5f, pos.y + radius + outline * 2.0f};
    drawList.AddText(ImGui::GetFont(), fontSize,
                     {textPos.x + 1.0f, textPos.y + 1.0f},
                     MatchSceneTuning::Marker::PLAYER_SHADOW_COLOR, name.c_str());
    drawList.AddText(ImGui::GetFont(), fontSize, textPos,
                     MatchSceneTuning::Marker::LABEL_COLOR, name.c_str());
  }
  if (player.player && hovered)
  {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(player.player->getName().c_str());
    ImGui::TextUnformatted(
        fmt::sprintf(LOC("MATCH_PLAYER_TOOLTIP"),
                     RoleUtils::shortName(player.player->getRole()),
                     playerIntentLabel(player.intent),
                     static_cast<double>(
                         player.stamina *
                         MatchSceneTuning::Scoreboard::PERCENT_SCALE),
                     static_cast<double>(player.speedMetresPerSecond))
            .c_str());
    ImGui::EndTooltip();
  }
}
}  // namespace

const char* playerIntentLabel(PlayerIntent intent)
{
  switch (intent)
  {
    case PlayerIntent::HOLD_SHAPE:
      return LOC("MATCH_INTENT_HOLD_SHAPE");
    case PlayerIntent::CARRY_BALL:
      return LOC("MATCH_INTENT_CARRY_BALL");
    case PlayerIntent::OFFER_SUPPORT:
      return LOC("MATCH_INTENT_OFFER_SUPPORT");
    case PlayerIntent::RECEIVE_PASS:
      return LOC("MATCH_INTENT_RECEIVE_PASS");
    case PlayerIntent::RUN_IN_BEHIND:
      return LOC("MATCH_INTENT_RUN_IN_BEHIND");
    case PlayerIntent::ATTACK_BOX:
      return LOC("MATCH_INTENT_ATTACK_BOX");
    case PlayerIntent::OVERLAP:
      return LOC("MATCH_INTENT_OVERLAP");
    case PlayerIntent::PRESS_BALL:
      return LOC("MATCH_INTENT_PRESS_BALL");
    case PlayerIntent::COVER_PRESS:
      return LOC("MATCH_INTENT_COVER_PRESS");
    case PlayerIntent::BLOCK_PASSING_LANE:
      return LOC("MATCH_INTENT_BLOCK_PASSING_LANE");
    case PlayerIntent::MARK_OPPONENT:
      return LOC("MATCH_INTENT_MARK_OPPONENT");
    case PlayerIntent::CLAIM_LOOSE_BALL:
      return LOC("MATCH_INTENT_CLAIM_LOOSE_BALL");
    case PlayerIntent::RECOVER_SHAPE:
      return LOC("MATCH_INTENT_RECOVER_SHAPE");
    case PlayerIntent::GOALKEEP:
      return LOC("MATCH_INTENT_GOALKEEP");
  }
  return "";
}

MatchViewport computeMatchViewport(float topLeftX, float topLeftY,
                                   float availableWidth, float availableHeight)
{
  // The pitch keeps its real 105 x 68 proportions at any size: fit the
  // available area and letterbox the rest (the caller centres it).
  constexpr float MIN_PIXELS_PER_METRE = 2.0f;
  const float pixelsPerMetreFit =
      std::max(MIN_PIXELS_PER_METRE,
               std::min(availableWidth / PitchMetres::LENGTH,
                        availableHeight / PitchMetres::WIDTH));
  MatchViewport viewport;
  viewport.x = topLeftX;
  viewport.y = topLeftY;
  viewport.width = PitchMetres::LENGTH * pixelsPerMetreFit;
  viewport.height = PitchMetres::WIDTH * pixelsPerMetreFit;
  return viewport;
}

void drawGoalCelebration(ImDrawList& drawList, const MatchViewport& viewport,
                         int homeScore, int awayScore,
                         float celebrationRemaining)
{
  const float total = MatchTuning::Timing::GOAL_CELEBRATION_SECONDS;
  const float progress =
      total > 0.0f ? std::clamp(1.0f - celebrationRemaining / total, 0.0f, 1.0f)
                   : 0.0f;
  // A fading golden flash behind the banner makes the goal unmistakable.
  const float pulse =
      0.5f +
      0.5f * std::sin(progress *
                      MatchSceneTuning::Celebration::PULSE_PERIOD_SECONDS *
                      MatchSceneTuning::CELEBRATION_RADIANS_PER_PERIOD);
  const float flashAlpha = MatchSceneTuning::Celebration::FLASH_MIN_ALPHA +
                           (MatchSceneTuning::Celebration::FLASH_MAX_ALPHA -
                            MatchSceneTuning::Celebration::FLASH_MIN_ALPHA) *
                               (1.0f - progress) * pulse;
  const ImU32 flashColor =
      IM_COL32(255, 255, 150, static_cast<unsigned int>(flashAlpha) & 0xFF);
  drawList.AddRectFilled(
      {viewport.x, viewport.y},
      {viewport.x + viewport.width, viewport.y + viewport.height}, flashColor,
      0.0f, ImDrawFlags_None);

  const float centerX =
      viewport.x + viewport.width * MatchSceneTuning::Pitch::CENTRE_RATIO;
  const float centerY =
      viewport.y + viewport.height * MatchSceneTuning::Pitch::CENTRE_RATIO;
  const float baseFontSize = ImGui::GetStyle().FontSizeBase;
  const std::string banner = LOC("MATCH_GOAL_BANNER");
  const std::string score =
      std::to_string(homeScore) + " - " + std::to_string(awayScore);

  ImGui::PushFont(nullptr,
                  baseFontSize * MatchSceneTuning::Celebration::BANNER_SCALE);
  const ImVec2 bannerSize = ImGui::CalcTextSize(banner.c_str());
  drawList.AddText({centerX - bannerSize.x * 0.5f, centerY - bannerSize.y},
                   MatchSceneTuning::Celebration::BANNER_TEXT_COLOR,
                   banner.c_str());
  ImGui::PopFont();

  ImGui::PushFont(nullptr,
                  baseFontSize * MatchSceneTuning::Celebration::SCORE_SCALE);
  const ImVec2 scoreSize = ImGui::CalcTextSize(score.c_str());
  drawList.AddText(
      {centerX - scoreSize.x * 0.5f,
       centerY +
           bannerSize.y * MatchSceneTuning::Celebration::SCORE_OFFSET_RATIO},
      MatchSceneTuning::Celebration::SCORE_TEXT_COLOR, score.c_str());
  ImGui::PopFont();
}

void MatchRenderer2D::render(const MatchRenderSnapshot& snapshot,
                             const MatchRenderOptions& options,
                             const MatchViewport& viewport)
{
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  if (!draw_list) return;

  // The same strips as the 3D view, chosen once per fixture.
  TeamID home = 0;
  TeamID away = 0;
  for (const MatchRenderPlayer& player : snapshot.players)
  {
    if (!player.player) continue;
    if (player.isHomeTeam && home == 0) home = player.player->getTeamId();
    if (!player.isHomeTeam && away == 0) away = player.player->getTeamId();
  }
  if (!kitsChosen || home != kitHomeTeam || away != kitAwayTeam)
  {
    kits = chooseMatchKits(home, away);
    kitHomeTeam = home;
    kitAwayTeam = away;
    kitsChosen = true;
  }

  drawStadium(*draw_list, viewport);
  drawPitch(*draw_list, viewport);

  const float alpha = snapshot.interpolationAlpha;

#ifdef DEBUG
  if (options.showAiDebug)
  {
    for (const MatchRenderPlayer& player : snapshot.players)
    {
      const Vector2F interpolated = lerpRenderPosition(
          player.previousPosition, player.currentPosition, alpha);
      const ImVec2 pos = worldToScreen(interpolated, viewport);
      const ImVec2 movementTarget =
          worldToScreen(player.movementTarget, viewport);
      const ImU32 debugColor = intentDebugColor(player.intent);
      draw_list->AddLine(pos, movementTarget, debugColor,
                         MatchSceneTuning::Marker::DEBUG_LINE_THICKNESS);
      draw_list->AddCircle(movementTarget,
                           MatchSceneTuning::Marker::DEBUG_TARGET_RADIUS,
                           debugColor);
    }
  }
#endif

  for (const MatchRenderPlayer& player : snapshot.players)
  {
    drawPlayer(*draw_list, player, alpha, viewport, kits,
               options.showPlayerNames);
  }

  const Vector2F interpolatedBall = lerpRenderPosition(
      snapshot.ball.previousPosition, snapshot.ball.currentPosition, alpha);
  const ImVec2 ballPos = worldToScreen(interpolatedBall, viewport);
  const float ppm = pixelsPerMetre(viewport);
  const float ballRadius =
      std::max(PitchMetres::MIN_BALL_PIXELS, PitchMetres::BALL_RADIUS * ppm);
  const float heightMetres =
      snapshot.ball.previousHeightMetres +
      (snapshot.ball.currentHeightMetres - snapshot.ball.previousHeightMetres) *
          alpha;
  // Ground shadow keeps the ball readable when it is lifted over the grass;
  // the lifted ball is offset upward by half its real height.
  draw_list->AddCircleFilled(
      ballPos, ballRadius * MatchSceneTuning::Marker::BALL_SHADOW_SCALE,
      MatchSceneTuning::Marker::BALL_SHADOW_COLOR);
  const float elevation = heightMetres * 0.5f * ppm;
  draw_list->AddCircleFilled(ImVec2(ballPos.x, ballPos.y - elevation),
                             ballRadius, MatchSceneTuning::Marker::BALL_COLOR);

  if (snapshot.state == MatchState::GOAL)
  {
    drawGoalCelebration(*draw_list, viewport, snapshot.homeScore,
                        snapshot.awayScore, snapshot.goalCelebrationRemaining);
  }
}
