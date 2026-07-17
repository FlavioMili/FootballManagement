// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/transfer_market_scene.h"

#include <fmt/format.h>
#include <imgui.h>

#include <algorithm>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "global/language_manager.h"
#include "model/game.h"
#include "model/role_utils.h"

TransferMarketScene::TransferMarketScene(GUIView* parent) : GUIScene(parent) {}

void TransferMarketScene::onEnter() { refreshData(); }

void TransferMarketScene::update(float /*deltaTime*/) {}

SceneID TransferMarketScene::getID() const { return SceneID::TRANSFER_MARKET; }

void TransferMarketScene::refreshData()
{
  auto& controller = guiView->getController();
  TeamID my_team = controller.getGame()->getManagedTeamId();

  cached_listings = controller.getListingsExcludingTeam(my_team);
  cached_bids = controller.getIncomingBids();
  cached_free_agents =
      controller.getGameData()->getPlayersForTeam(FREE_AGENTS_TEAM_ID);
}

void TransferMarketScene::render()
{
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoSavedSettings;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);

  if (ImGui::Begin(LOC("TRANSFER_TITLE"), nullptr, window_flags))
  {
    auto& controller = guiView->getController();
    TeamID my_team = controller.getGame()->getManagedTeamId();

    uint32_t budget = controller.transferBudgetForTeam(my_team);
    ImGui::Text(LOC("TRANSFER_BUDGET"), fmt::format("{:L}", budget).c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 300);
    if (controller.isTransferWindowOpen())
    {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", LOC("TRANSFER_WINDOW_OPEN"));
    }
    else
    {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s",
                         LOC("TRANSFER_WINDOW_CLOSED"));
    }

    if (ImGui::Button(LOC("ROSTER_BACK"), ImVec2(120, 30)))
    {
      guiView->popScene();
    }

    ImGui::Spacing();

    if (ImGui::BeginTabBar("TransferMarketTabs"))
    {
      if (ImGui::BeginTabItem(LOC("TRANSFER_TAB_BUY")))
      {
        active_tab = Tab::BUY;
        renderBuyTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(LOC("TRANSFER_TAB_LISTINGS")))
      {
        active_tab = Tab::LISTINGS;
        renderListingsTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(LOC("TRANSFER_TAB_BIDS")))
      {
        active_tab = Tab::BIDS;
        renderBidsTab();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }

    renderConfirmDialog();
    renderCounterDialog();
  }
  ImGui::End();
}

void TransferMarketScene::renderBuyTab()
{
  auto& controller = guiView->getController();

  ImGui::Text("%s", LOC("TRANSFER_FILTERS"));

  // Filters (Simplified for space)
  const char* roles[] = {LOC("TRANSFER_ALL"),
                         "GK",
                         "CB",
                         "LB",
                         "RB",
                         "CDM",
                         "CM",
                         "CAM",
                         "LM",
                         "RM",
                         "LW",
                         "RW",
                         "ST"};
  ImGui::Combo(LOC("TRANSFER_FILTER_ROLE"), &filter_role_index, roles,
               IM_ARRAYSIZE(roles));
  ImGui::SliderInt(LOC("TRANSFER_FILTER_AGE"), &filter_max_age, 15, 45);
  ImGui::SliderFloat(LOC("TRANSFER_FILTER_PRICE"), &filter_max_price, 0.0f,
                     200000000.0f, "%.0f");

  ImGui::Separator();

  if (ImGui::BeginTable("BuyTable", 8,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY,
                        ImVec2(0, 300)))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_NAME"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_TEAM"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_ROLE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_AGE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_OVR"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_VALUE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_PRICE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_ACTION"));
    ImGui::TableHeadersRow();

    for (const auto* listing : cached_listings)
    {
      auto player_opt = controller.getGameData()->getPlayer(listing->player_id);
      if (!player_opt.has_value()) continue;

      const Player& p = player_opt->get();

      if (filter_role_index > 0 &&
          p.getRole() != static_cast<PlayerRole>(filter_role_index))
        continue;
      if (p.getAge() > filter_max_age) continue;
      if (static_cast<float>(listing->asking_price) > filter_max_price)
        continue;

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", p.getName().c_str());
      ImGui::TableNextColumn();
      auto seller_opt =
          controller.getGameData()->getTeam(listing->seller_team_id);
      ImGui::Text("%s", seller_opt.has_value()
                            ? seller_opt->get().getName().c_str()
                            : "Unknown");

      ImGui::TableNextColumn();
      ImGui::Text("%s", RoleUtils::toString(p.getRole()).c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", static_cast<int>(p.getAge()));
      ImGui::TableNextColumn();
      ImGui::Text("%d", static_cast<int>(p.getOverall(
                            controller.getGameData()->getStatsConfig())));
      ImGui::TableNextColumn();
      ImGui::Text("€%d", controller.getPlayerMarketValue(p.getId()));
      ImGui::TableNextColumn();
      ImGui::Text("€%u", listing->asking_price);

      ImGui::TableNextColumn();
      std::string btn_id =
          fmt::format("{}##{}", LOC("TRANSFER_BUY"), p.getId());
      if (ImGui::Button(btn_id.c_str()))
      {
        confirm_state = {true, p.getId(), listing->seller_team_id,
                         listing->asking_price, false};
      }
    }
    ImGui::EndTable();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("%s", LOC("TRANSFER_FREE_AGENTS"));

  if (ImGui::BeginTable("FreeAgentsTable", 8,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY,
                        ImVec2(0, 300)))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_NAME"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_TEAM"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_ROLE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_AGE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_OVR"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_VALUE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_PRICE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_ACTION"));
    ImGui::TableHeadersRow();

    for (const auto& ref : cached_free_agents)
    {
      const Player& p = ref.get();

      if (filter_role_index > 0 &&
          p.getRole() != static_cast<PlayerRole>(filter_role_index))
        continue;
      if (p.getAge() > filter_max_age) continue;

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", p.getName().c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s",
                         LOC("TRANSFER_FREE_AGENT_LABEL"));

      ImGui::TableNextColumn();
      ImGui::Text("%s", RoleUtils::toString(p.getRole()).c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", static_cast<int>(p.getAge()));
      ImGui::TableNextColumn();
      ImGui::Text("%d", static_cast<int>(p.getOverall(
                            controller.getGameData()->getStatsConfig())));
      ImGui::TableNextColumn();
      ImGui::Text("€%d", controller.getPlayerMarketValue(p.getId()));
      ImGui::TableNextColumn();
      ImGui::Text("%s", LOC("TRANSFER_FREE"));

      ImGui::TableNextColumn();
      std::string btn_id =
          fmt::format("{}##{}", LOC("TRANSFER_SIGN"), p.getId());
      if (ImGui::Button(btn_id.c_str()))
      {
        confirm_state = {true, p.getId(), FREE_AGENTS_TEAM_ID, 0, true};
      }
    }
    ImGui::EndTable();
  }
}

void TransferMarketScene::renderListingsTab()
{
  auto& controller = guiView->getController();
  TeamID my_team = controller.getGame()->getManagedTeamId();

  ImGui::Text("%s", LOC("TRANSFER_LIST_PLAYER"));
  const auto& my_players = controller.getGameData()->getPlayersForTeam(my_team);

  std::vector<const Player*> listable_players;
  for (const auto& ref : my_players)
  {
    if (!controller.isPlayerListed(ref.get().getId()))
    {
      listable_players.push_back(&ref.get());
    }
  }

  if (!listable_players.empty())
  {
    if (list_player_index >= static_cast<int>(listable_players.size()))
      list_player_index = 0;

    std::string combo_preview = listable_players[list_player_index]->getName();
    if (ImGui::BeginCombo(LOC("TRANSFER_SELECT_PLAYER"), combo_preview.c_str()))
    {
      for (size_t i = 0; i < listable_players.size(); i++)
      {
        bool is_selected = (static_cast<size_t>(list_player_index) == i);
        if (ImGui::Selectable(listable_players[i]->getName().c_str(),
                              is_selected))
        {
          list_player_index = static_cast<int>(i);
          list_price_input = static_cast<float>(
              controller.getPlayerMarketValue(listable_players[i]->getId()));
        }
        if (is_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    ImGui::InputFloat(LOC("TRANSFER_ASKING_PRICE"), &list_price_input,
                      100000.0f, 1000000.0f, "%.0f");
    if (ImGui::Button(LOC("TRANSFER_LIST_PLAYER")))
    {
      controller.listPlayerForTransfer(
          listable_players[list_player_index]->getId(),
          static_cast<uint32_t>(list_price_input));
      refreshData();
    }
  }

  ImGui::Separator();
  ImGui::Text("%s", LOC("TRANSFER_YOUR_LISTINGS"));

  if (ImGui::BeginTable("MyListingsTable", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY,
                        ImVec2(0, 300)))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_NAME"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_ROLE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_OVR"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_PRICE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_ACTION"));
    ImGui::TableHeadersRow();

    for (const auto& ref : my_players)
    {
      if (controller.isPlayerListed(ref.get().getId()))
      {
        const auto& all_listings = controller.getAllListings();
        auto it = all_listings.find(ref.get().getId());
        if (it == all_listings.end()) continue;
        const TransferListing& listing = it->second;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s", ref.get().getName().c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%s", RoleUtils::toString(ref.get().getRole()).c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d", static_cast<int>(ref.get().getOverall(
                              controller.getGameData()->getStatsConfig())));
        ImGui::TableNextColumn();
        ImGui::Text("€%u", listing.asking_price);

        ImGui::TableNextColumn();
        std::string btn_id =
            fmt::format("{}##{}", LOC("TRANSFER_UNLIST"), ref.get().getId());
        if (ImGui::Button(btn_id.c_str()))
        {
          controller.removePlayerFromTransfer(ref.get().getId());
          refreshData();
        }
      }
    }
    ImGui::EndTable();
  }
}

void TransferMarketScene::renderBidsTab()
{
  auto& controller = guiView->getController();

  ImGui::Text("%s", LOC("TRANSFER_INCOMING_BIDS"));

  if (cached_bids.empty())
  {
    ImGui::Text("%s", LOC("TRANSFER_NO_BIDS"));
    return;
  }

  if (ImGui::BeginTable("BidsTable", 6,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY,
                        ImVec2(0, 300)))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_NAME"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_BIDDER"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_AMOUNT"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_VS_VALUE"));
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_ACTION"));
    ImGui::TableHeadersRow();

    for (const auto& [pid, listing] : cached_bids)
    {
      auto player_opt = controller.getGameData()->getPlayer(pid);
      if (!player_opt.has_value()) continue;

      auto bidder_opt =
          controller.getGameData()->getTeam(listing.highest_bidder_id.value());
      std::string bidder_name =
          bidder_opt.has_value() ? bidder_opt->get().getName() : "Unknown";

      uint32_t value = controller.getPlayerMarketValue(pid);
      float vs_val = static_cast<float>(listing.highest_bid) /
                     static_cast<float>(value) * 100.0f;

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", player_opt->get().getName().c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%s", bidder_name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("€%u", listing.highest_bid);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f%%", static_cast<double>(vs_val));

      ImGui::TableNextColumn();
      std::string accept_id =
          fmt::format("{}##{}", LOC("TRANSFER_ACCEPT"), pid);
      if (ImGui::Button(accept_id.c_str()))
      {
        controller.acceptBid(pid);
        refreshData();
      }
      ImGui::SameLine();
      std::string reject_id =
          fmt::format("{}##{}", LOC("TRANSFER_REJECT"), pid);
      if (ImGui::Button(reject_id.c_str()))
      {
        controller.rejectBid(pid);
        refreshData();
      }
      ImGui::SameLine();
      std::string counter_id =
          fmt::format("{}##{}", LOC("TRANSFER_COUNTER"), pid);
      if (ImGui::Button(counter_id.c_str()))
      {
        counter_state = {true, pid};
        counter_price_input = static_cast<float>(listing.highest_bid);
      }
    }
    ImGui::EndTable();
  }
}

void TransferMarketScene::renderConfirmDialog()
{
  if (confirm_state.open)
  {
    ImGui::OpenPopup(LOC("TRANSFER_CONFIRM_TITLE"));
    confirm_state.open = false;  // So it doesn't keep opening
  }

  if (ImGui::BeginPopupModal(LOC("TRANSFER_CONFIRM_TITLE"), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
  {
    auto& controller = guiView->getController();
    auto player_opt =
        controller.getGameData()->getPlayer(confirm_state.player_id);

    if (player_opt.has_value())
    {
      if (confirm_state.is_free_agent)
      {
        ImGui::Text("%s: %s", LOC("TRANSFER_CONFIRM_SIGN"),
                    player_opt->get().getName().c_str());
      }
      else
      {
        ImGui::Text("%s: %s for €%u?", LOC("TRANSFER_CONFIRM_BUY"),
                    player_opt->get().getName().c_str(), confirm_state.price);
      }
      ImGui::Separator();

      TeamID my_team = controller.getGame()->getManagedTeamId();
      bool can_afford = controller.canAffordPlayer(
          my_team, confirm_state.player_id, confirm_state.price);

      if (!can_afford)
      {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s",
                           LOC("TRANSFER_NOT_ENOUGH_BUDGET"));
        if (ImGui::Button(LOC("TRANSFER_CANCEL"), ImVec2(120, 0)))
        {
          ImGui::CloseCurrentPopup();
        }
      }
      else
      {
        if (ImGui::Button(LOC("TRANSFER_CONFIRM"), ImVec2(120, 0)))
        {
          if (confirm_state.is_free_agent)
          {
            controller.signFreeAgent(confirm_state.player_id, my_team);
          }
          else
          {
            controller.buyPlayer(confirm_state.player_id, my_team,
                                 confirm_state.price);
          }
          refreshData();
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(LOC("TRANSFER_CANCEL"), ImVec2(120, 0)))
        {
          ImGui::CloseCurrentPopup();
        }
      }
    }
    ImGui::EndPopup();
  }
}

void TransferMarketScene::renderCounterDialog()
{
  if (counter_state.open)
  {
    ImGui::OpenPopup(LOC("TRANSFER_COUNTER_TITLE"));
    counter_state.open = false;
  }

  if (ImGui::BeginPopupModal(LOC("TRANSFER_COUNTER_TITLE"), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
  {
    auto& controller = guiView->getController();
    const auto& all_listings = controller.getAllListings();
    auto it = all_listings.find(counter_state.player_id);

    if (it != all_listings.end())
    {
      ImGui::Text("%s: €%u", LOC("TRANSFER_CURRENT_BID"),
                  it->second.highest_bid);
      ImGui::InputFloat(LOC("TRANSFER_NEW_PRICE"), &counter_price_input,
                        100000.0f, 1000000.0f, "%.0f");

      if (ImGui::Button(LOC("TRANSFER_SUBMIT_COUNTER"), ImVec2(120, 0)))
      {
        controller.counterOffer(counter_state.player_id,
                                static_cast<uint32_t>(counter_price_input));
        refreshData();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button(LOC("TRANSFER_CANCEL"), ImVec2(120, 0)))
      {
        ImGui::CloseCurrentPopup();
      }
    }
    else
    {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
