// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/render/match_renderer_2d.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>

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

const char* playerIntentLabel(PlayerIntent intent)
{
  switch (intent)
  {
    case PlayerIntent::HOLD_SHAPE:
      return "Hold shape";
    case PlayerIntent::CARRY_BALL:
      return "Carry ball";
    case PlayerIntent::OFFER_SUPPORT:
      return "Offer support";
    case PlayerIntent::RECEIVE_PASS:
      return "Receive pass";
    case PlayerIntent::RUN_IN_BEHIND:
      return "Run in behind";
    case PlayerIntent::ATTACK_BOX:
      return "Attack box";
    case PlayerIntent::OVERLAP:
      return "Overlap";
    case PlayerIntent::PRESS_BALL:
      return "Press ball";
    case PlayerIntent::COVER_PRESS:
      return "Cover press";
    case PlayerIntent::BLOCK_PASSING_LANE:
      return "Block passing lane";
    case PlayerIntent::MARK_OPPONENT:
      return "Mark opponent";
    case PlayerIntent::CLAIM_LOOSE_BALL:
      return "Claim loose ball";
    case PlayerIntent::RECOVER_SHAPE:
      return "Recover shape";
    case PlayerIntent::GOALKEEP:
      return "Goalkeep";
  }
  return "Unknown";
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

void drawPitch(ImDrawList& drawList, const MatchViewport& viewport)
{
  constexpr ImU32 line_color = MatchSceneTuning::Pitch::LINE_COLOR;
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
  // Border
  drawList.AddRect(p_min, p_max, line_color,
                   MatchSceneTuning::Pitch::RECTANGLE_ROUNDING,
                   ImDrawFlags_None, MatchSceneTuning::Pitch::LINE_THICKNESS);

  const float center_y =
      p_min.y + viewport.height * MatchSceneTuning::Pitch::CENTRE_RATIO;
  const float center_x =
      p_min.x + viewport.width * MatchSceneTuning::Pitch::CENTRE_RATIO;
  // Center line (vertical)
  drawList.AddLine(ImVec2(center_x, p_min.y), ImVec2(center_x, p_max.y),
                   line_color, MatchSceneTuning::Pitch::LINE_THICKNESS);

  // Center circle
  drawList.AddCircle(
      ImVec2(center_x, center_y),
      viewport.height * MatchSceneTuning::Pitch::CENTRE_CIRCLE_RADIUS_RATIO,
      line_color, MatchSceneTuning::Pitch::CENTRE_CIRCLE_SEGMENTS,
      MatchSceneTuning::Pitch::LINE_THICKNESS);
  drawList.AddCircleFilled(ImVec2(center_x, center_y),
                           MatchSceneTuning::Pitch::CENTRE_SPOT_RADIUS,
                           line_color);

  // Penalty boxes
  const float pen_box_w =
      viewport.width * MatchSceneTuning::Pitch::PENALTY_BOX_WIDTH_RATIO;
  const float pen_box_h =
      viewport.height * MatchSceneTuning::Pitch::PENALTY_BOX_HEIGHT_RATIO;
  const float pen_box_y =
      center_y - pen_box_h * MatchSceneTuning::Pitch::CENTRE_RATIO;

  drawList.AddRect(ImVec2(p_min.x, pen_box_y),
                   ImVec2(p_min.x + pen_box_w, pen_box_y + pen_box_h),
                   line_color, MatchSceneTuning::Pitch::RECTANGLE_ROUNDING,
                   ImDrawFlags_None, MatchSceneTuning::Pitch::LINE_THICKNESS);
  drawList.AddRect(ImVec2(p_max.x - pen_box_w, pen_box_y),
                   ImVec2(p_max.x, pen_box_y + pen_box_h), line_color,
                   MatchSceneTuning::Pitch::RECTANGLE_ROUNDING,
                   ImDrawFlags_None, MatchSceneTuning::Pitch::LINE_THICKNESS);

  // Goal boxes
  const float goal_box_w =
      viewport.width * MatchSceneTuning::Pitch::GOAL_BOX_WIDTH_RATIO;
  const float goal_box_h =
      viewport.height * MatchSceneTuning::Pitch::GOAL_BOX_HEIGHT_RATIO;
  const float goal_box_y =
      center_y - goal_box_h * MatchSceneTuning::Pitch::CENTRE_RATIO;

  drawList.AddRect(ImVec2(p_min.x, goal_box_y),
                   ImVec2(p_min.x + goal_box_w, goal_box_y + goal_box_h),
                   line_color, MatchSceneTuning::Pitch::RECTANGLE_ROUNDING,
                   ImDrawFlags_None, MatchSceneTuning::Pitch::LINE_THICKNESS);
  drawList.AddRect(ImVec2(p_max.x - goal_box_w, goal_box_y),
                   ImVec2(p_max.x, goal_box_y + goal_box_h), line_color,
                   MatchSceneTuning::Pitch::RECTANGLE_ROUNDING,
                   ImDrawFlags_None, MatchSceneTuning::Pitch::LINE_THICKNESS);

  // Goals extend beyond the touchline. A back panel, posts, crossbar, and net
  // mesh make the ball visibly enter the goal mouth instead of vanishing at
  // the line.
  const float goalHeight =
      viewport.height * MatchSceneTuning::Pitch::GOAL_WIDTH_RATIO;
  const float goalTop =
      center_y - goalHeight * MatchSceneTuning::Pitch::CENTRE_RATIO;
  const float goalDepth = MatchSceneTuning::Pitch::GOAL_DEPTH;
  drawList.AddRectFilled(ImVec2(p_min.x - goalDepth, goalTop),
                         ImVec2(p_min.x, goalTop + goalHeight),
                         MatchSceneTuning::Stadium::APRON_COLOR);
  drawList.AddRectFilled(ImVec2(p_max.x, goalTop),
                         ImVec2(p_max.x + goalDepth, goalTop + goalHeight),
                         MatchSceneTuning::Stadium::APRON_COLOR);
  drawGoalFrame(drawList, true, p_min.x, goalTop, goalTop + goalHeight,
                goalDepth);
  drawGoalFrame(drawList, false, p_max.x, goalTop, goalTop + goalHeight,
                goalDepth);
  drawList.AddCircleFilled(
      ImVec2(p_min.x +
                 viewport.width * MatchSceneTuning::Pitch::PENALTY_SPOT_X_RATIO,
             center_y),
      MatchSceneTuning::Pitch::PENALTY_SPOT_RADIUS, line_color);
  drawList.AddCircleFilled(
      ImVec2(p_max.x -
                 viewport.width * MatchSceneTuning::Pitch::PENALTY_SPOT_X_RATIO,
             center_y),
      MatchSceneTuning::Pitch::PENALTY_SPOT_RADIUS, line_color);

  // Corner arcs
  const float corner_r =
      viewport.height * MatchSceneTuning::Pitch::CORNER_RADIUS_RATIO;
  const float PI = std::numbers::pi_v<float>;
  // Top-left
  drawList.PathArcTo(ImVec2(p_min.x, p_min.y), corner_r, 0.0f,
                     PI * MatchSceneTuning::Pitch::CENTRE_RATIO,
                     MatchSceneTuning::Pitch::CORNER_ARC_SEGMENTS);
  drawList.PathStroke(line_color, ImDrawFlags_None,
                      MatchSceneTuning::Pitch::LINE_THICKNESS);
  // Top-right
  drawList.PathArcTo(ImVec2(p_max.x, p_min.y), corner_r,
                     PI * MatchSceneTuning::Pitch::CENTRE_RATIO, PI,
                     MatchSceneTuning::Pitch::CORNER_ARC_SEGMENTS);
  drawList.PathStroke(line_color, ImDrawFlags_None,
                      MatchSceneTuning::Pitch::LINE_THICKNESS);
  // Bottom-left
  drawList.PathArcTo(ImVec2(p_min.x, p_max.y), corner_r,
                     -PI * MatchSceneTuning::Pitch::CENTRE_RATIO, 0.0f,
                     MatchSceneTuning::Pitch::CORNER_ARC_SEGMENTS);
  drawList.PathStroke(line_color, ImDrawFlags_None,
                      MatchSceneTuning::Pitch::LINE_THICKNESS);
  // Bottom-right
  drawList.PathArcTo(
      ImVec2(p_max.x, p_max.y), corner_r, PI,
      PI * MatchSceneTuning::Pitch::BOTTOM_RIGHT_ARC_END_MULTIPLIER,
      MatchSceneTuning::Pitch::CORNER_ARC_SEGMENTS);
  drawList.PathStroke(line_color, ImDrawFlags_None,
                      MatchSceneTuning::Pitch::LINE_THICKNESS);
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
                float alpha, const MatchViewport& viewport)
{
  const Vector2F interpolated = lerpRenderPosition(
      player.previousPosition, player.currentPosition, alpha);
  const ImVec2 pos = worldToScreen(interpolated, viewport);

  const ImU32 color = player.isHomeTeam ? MatchSceneTuning::Marker::HOME_COLOR
                                        : MatchSceneTuning::Marker::AWAY_COLOR;
  drawList.AddCircleFilled(
      ImVec2(pos.x + MatchSceneTuning::Marker::PLAYER_SHADOW_OFFSET,
             pos.y + MatchSceneTuning::Marker::PLAYER_SHADOW_OFFSET),
      MatchSceneTuning::Marker::PLAYER_SHADOW_RADIUS,
      MatchSceneTuning::Marker::PLAYER_SHADOW_COLOR);
  drawList.AddCircleFilled(pos, MatchSceneTuning::Marker::PLAYER_OUTLINE_RADIUS,
                           MatchSceneTuning::Marker::PLAYER_OUTLINE_COLOR);
  drawList.AddCircleFilled(pos, MatchSceneTuning::Marker::PLAYER_RADIUS, color);
  if (player.possessesBall)
  {
    drawList.AddCircle(pos, MatchSceneTuning::Marker::POSSESSION_RING_RADIUS,
                       MatchSceneTuning::Marker::POSSESSION_RING_COLOR, 0,
                       MatchSceneTuning::Marker::POSSESSION_RING_THICKNESS);
  }

  // Direction pointer
  const float facingAngle =
      player.previousFacingAngle +
      (player.currentFacingAngle - player.previousFacingAngle) * alpha;
  constexpr float dirLen = MatchSceneTuning::Marker::DIRECTION_LENGTH;
  const ImVec2 dirEnd(pos.x + std::cos(facingAngle) * dirLen,
                      pos.y + std::sin(facingAngle) * dirLen);
  drawList.AddLine(pos, dirEnd, MatchSceneTuning::Marker::DIRECTION_COLOR,
                   MatchSceneTuning::Marker::DIRECTION_THICKNESS);

  // Full labels are useful on demand, but drawing 22 of them continuously
  // makes compact formations unreadable.
  const ImVec2 mouse = ImGui::GetMousePos();
  const float mouseDistanceSquared = (mouse.x - pos.x) * (mouse.x - pos.x) +
                                     (mouse.y - pos.y) * (mouse.y - pos.y);
  if (player.player &&
      (player.possessesBall ||
       mouseDistanceSquared < MatchSceneTuning::Marker::HOVER_RADIUS *
                                  MatchSceneTuning::Marker::HOVER_RADIUS))
  {
    drawList.AddText(ImVec2(pos.x + MatchSceneTuning::Marker::LABEL_X_OFFSET,
                            pos.y + MatchSceneTuning::Marker::LABEL_Y_OFFSET),
                     MatchSceneTuning::Marker::LABEL_COLOR,
                     player.player->getName().c_str());
  }
  if (player.player &&
      mouseDistanceSquared < MatchSceneTuning::Marker::HOVER_RADIUS *
                                 MatchSceneTuning::Marker::HOVER_RADIUS)
  {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(player.player->getName().c_str());
    ImGui::Text(
        "%s | %s | stamina %.0f%%",
        RoleUtils::toString(player.player->getRole()).c_str(),
        playerIntentLabel(player.intent),
        static_cast<double>(player.stamina *
                            MatchSceneTuning::Scoreboard::PERCENT_SCALE));
    ImGui::EndTooltip();
  }
}
}  // namespace

MatchViewport computeMatchViewport(float topLeftX, float topLeftY,
                                   float availableWidth, float availableHeight)
{
  const float referenceWidth = MatchSceneTuning::Pitch::WIDTH;
  const float referenceHeight = MatchSceneTuning::Pitch::HEIGHT;
  const float scale = std::clamp(std::min(availableWidth / referenceWidth,
                                          availableHeight / referenceHeight),
                                 MatchSceneTuning::Pitch::MIN_SCALE,
                                 MatchSceneTuning::Pitch::MAX_SCALE);
  MatchViewport viewport;
  viewport.x = topLeftX;
  viewport.y = topLeftY;
  viewport.width = referenceWidth * scale;
  viewport.height = referenceHeight * scale;
  return viewport;
}

void drawGoalCelebration(ImDrawList& drawList, const MatchViewport& viewport,
                         bool scoredByHome, int homeScore, int awayScore,
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
  const std::string banner = "GOAL!";
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
  (void)scoredByHome;
}

void MatchRenderer2D::render(const MatchRenderSnapshot& snapshot,
                             const MatchRenderOptions& options,
                             const MatchViewport& viewport)
{
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  if (!draw_list) return;

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
#else
  (void)options;
#endif

  for (const MatchRenderPlayer& player : snapshot.players)
  {
    drawPlayer(*draw_list, player, alpha, viewport);
  }

  const Vector2F interpolatedBall = lerpRenderPosition(
      snapshot.ball.previousPosition, snapshot.ball.currentPosition, alpha);
  const ImVec2 ballPos = worldToScreen(interpolatedBall, viewport);
  const float ballZ =
      snapshot.ball.previousZ +
      (snapshot.ball.currentZ - snapshot.ball.previousZ) * alpha;
  // Ground shadow keeps the ball readable when it is lifted over the grass.
  draw_list->AddCircleFilled(ballPos,
                             MatchSceneTuning::Marker::BALL_RADIUS *
                                 MatchSceneTuning::Marker::BALL_SHADOW_SCALE,
                             MatchSceneTuning::Marker::BALL_SHADOW_COLOR);
  const float elevation = ballZ * MatchSceneTuning::Marker::BALL_HEIGHT_SCALE *
                          MatchSceneTuning::Marker::BALL_RADIUS;
  draw_list->AddCircleFilled(ImVec2(ballPos.x, ballPos.y - elevation),
                             MatchSceneTuning::Marker::BALL_RADIUS,
                             MatchSceneTuning::Marker::BALL_COLOR);

  if (snapshot.state == MatchState::GOAL)
  {
    drawGoalCelebration(*draw_list, viewport, snapshot.goalScoredByHome,
                        snapshot.homeScore, snapshot.awayScore,
                        snapshot.goalCelebrationRemaining);
  }
}
