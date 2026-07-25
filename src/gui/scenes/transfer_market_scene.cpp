// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/transfer_market_scene.h"

#include <fmt/format.h>
#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <array>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "global/language_manager.h"
#include "model/game.h"
#include "model/role_utils.h"

TransferMarketScene::TransferMarketScene(GUIView* parent) : GUIScene(parent) {}

void TransferMarketScene::onEnter() { refreshData(); }

void TransferMarketScene::update(float /*deltaTime*/)
{
  // No update logic required for this scene yet.
}

SceneID TransferMarketScene::getID() const { return SceneID::TRANSFER_MARKET; }

void TransferMarketScene::refreshData()
{
  const auto& controller = guiView->getController();
  TeamID my_team = controller.getGame()->getManagedTeamId();

  cached_listings = controller.getListingsExcludingTeam(my_team);
  cached_bids = controller.getIncomingBids();
  cached_free_agents =
      controller.getGameData()->getPlayersForTeam(FREE_AGENTS_TEAM_ID);

  cached_all_players.clear();
  for (const auto& [pid, player] : controller.getGameData()->getPlayers())
  {
    if (player.getTeamId() != my_team)
    {
      cached_all_players.push_back(&player);
    }
  }
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
    const auto& controller = guiView->getController();
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
  const auto& controller = guiView->getController();

  ImGui::Text("%s", LOC("TRANSFER_FILTERS"));

  ImGui::InputText("Search Name", filter_search_name,
                   sizeof(filter_search_name));

  static const std::array<const char*, 13> roles = {LOC("TRANSFER_ALL"),
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
  ImGui::Combo(LOC("TRANSFER_FILTER_ROLE"), &filter_role_index, roles.data(),
               static_cast<int>(roles.size()));
  ImGui::SliderInt(LOC("TRANSFER_FILTER_AGE"), &filter_max_age, 15, 45);
  ImGui::SliderFloat(LOC("TRANSFER_FILTER_PRICE"), &filter_max_price, 0.0f,
                     200000000.0f, "%.0f");

  ImGui::Separator();

  std::vector<const Player*> display_players;
  display_players.reserve(cached_all_players.size());

  std::string search_str = filter_search_name;
  std::transform(search_str.begin(), search_str.end(), search_str.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  const auto& all_listings = controller.getAllListings();

  for (const auto* p_ptr : cached_all_players)
  {
    const Player& p = *p_ptr;

    if (!search_str.empty())
    {
      std::string name_lower = p.getName();
      std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (name_lower.find(search_str) == std::string::npos) continue;
    }

    if (filter_role_index > 0 &&
        p.getRole() != static_cast<PlayerRole>(filter_role_index))
      continue;
    if (p.getAge() > filter_max_age) continue;

    uint32_t price = 0;
    if (p.getTeamId() == FREE_AGENTS_TEAM_ID)
    {
      price = 0;
    }
    else if (auto it = all_listings.find(p.getId()); it != all_listings.end())
    {
      price = it->second.asking_price;
    }
    else
    {
      price = controller.getPlayerMarketValue(p.getId());
    }

    if (static_cast<float>(price) > filter_max_price) continue;

    display_players.push_back(p_ptr);
  }

  ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable;

  if (ImGui::BeginTable("BuyTable", 8, flags, ImVec2(0, 450)))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_NAME"),
                            ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_TEAM"), 0, 0.0f, 1);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_ROLE"), 0, 0.0f, 2);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_AGE"), 0, 0.0f, 3);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_OVR"), 0, 0.0f, 4);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_VALUE"), 0, 0.0f, 5);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_PRICE"), 0, 0.0f, 6);
    ImGui::TableSetupColumn(LOC("TRANSFER_COL_ACTION"),
                            ImGuiTableColumnFlags_NoSort, 0.0f, 7);
    ImGui::TableHeadersRow();

    if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs())
    {
      if (sort_specs->SpecsCount > 0)
      {
        const ImGuiTableColumnSortSpecs* spec = &sort_specs->Specs[0];
        bool ascending = (spec->SortDirection == ImGuiSortDirection_Ascending);

        std::ranges::sort(
            display_players,
            [&](const Player* a, const Player* b)
            {
              int cmp = 0;
              switch (spec->ColumnUserID)
              {
                case 0:  // Name
                  cmp = a->getName().compare(b->getName());
                  break;
                case 1:  // Team
                {
                  std::string teamA = (a->getTeamId() == FREE_AGENTS_TEAM_ID)
                                          ? "Free Agent"
                                          : (controller.getGameData()
                                                     ->getTeam(a->getTeamId())
                                                     .has_value()
                                                 ? controller.getGameData()
                                                       ->getTeam(a->getTeamId())
                                                       ->get()
                                                       .getName()
                                                 : "");
                  std::string teamB = (b->getTeamId() == FREE_AGENTS_TEAM_ID)
                                          ? "Free Agent"
                                          : (controller.getGameData()
                                                     ->getTeam(b->getTeamId())
                                                     .has_value()
                                                 ? controller.getGameData()
                                                       ->getTeam(b->getTeamId())
                                                       ->get()
                                                       .getName()
                                                 : "");
                  cmp = teamA.compare(teamB);
                  break;
                }
                case 2:  // Role
                  cmp = static_cast<int>(a->getRole()) -
                        static_cast<int>(b->getRole());
                  break;
                case 3:  // Age
                  cmp = a->getAge() - b->getAge();
                  break;
                case 4:  // OVR
                {
                  int ovrA = static_cast<int>(a->getOverall(
                      controller.getGameData()->getStatsConfig()));
                  int ovrB = static_cast<int>(b->getOverall(
                      controller.getGameData()->getStatsConfig()));
                  cmp = ovrA - ovrB;
                  break;
                }
                case 5:  // Market Value
                {
                  uint32_t valA = controller.getPlayerMarketValue(a->getId());
                  uint32_t valB = controller.getPlayerMarketValue(b->getId());
                  cmp = (valA < valB) ? -1 : (valA > valB ? 1 : 0);
                  break;
                }
                case 6:  // Price
                {
                  auto getPrice = [&](const Player* p) -> uint32_t
                  {
                    if (p->getTeamId() == FREE_AGENTS_TEAM_ID) return 0;
                    if (auto it = all_listings.find(p->getId());
                        it != all_listings.end())
                      return it->second.asking_price;
                    return controller.getPlayerMarketValue(p->getId());
                  };
                  uint32_t priceA = getPrice(a);
                  uint32_t priceB = getPrice(b);
                  cmp = (priceA < priceB) ? -1 : (priceA > priceB ? 1 : 0);
                  break;
                }
              }
              return ascending ? (cmp < 0) : (cmp > 0);
            });
      }
    }

    for (const auto* p_ptr : display_players)
    {
      const Player& p = *p_ptr;

      bool is_listed = false;
      uint32_t price = 0;
      bool is_free_agent = (p.getTeamId() == FREE_AGENTS_TEAM_ID);

      if (is_free_agent)
      {
        price = 0;
      }
      else if (auto it = all_listings.find(p.getId()); it != all_listings.end())
      {
        is_listed = true;
        price = it->second.asking_price;
      }
      else
      {
        price = controller.getPlayerMarketValue(p.getId());
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", p.getName().c_str());

      ImGui::TableNextColumn();
      if (is_free_agent)
      {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s",
                           LOC("TRANSFER_FREE_AGENT_LABEL"));
      }
      else
      {
        auto seller_opt = controller.getGameData()->getTeam(p.getTeamId());
        ImGui::Text("%s", seller_opt.has_value()
                              ? seller_opt->get().getName().c_str()
                              : "Unknown");
      }

      ImGui::TableNextColumn();
      ImGui::Text("%s", RoleUtils::toString(p.getRole()).c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", p.getAge());
      ImGui::TableNextColumn();
      ImGui::Text("%d", static_cast<int>(p.getOverall(
                            controller.getGameData()->getStatsConfig())));
      ImGui::TableNextColumn();
      ImGui::Text("€%d", controller.getPlayerMarketValue(p.getId()));

      ImGui::TableNextColumn();
      if (is_free_agent)
      {
        ImGui::Text("%s", LOC("TRANSFER_FREE"));
      }
      else if (is_listed)
      {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "€%u", price);
      }
      else
      {
        ImGui::Text("€%u", price);
      }

      ImGui::TableNextColumn();
      if (is_free_agent)
      {
        std::string btn_id =
            fmt::format("{}##{}", LOC("TRANSFER_SIGN"), p.getId());
        if (ImGui::Button(btn_id.c_str()))
        {
          confirm_state = {true, p.getId(), FREE_AGENTS_TEAM_ID, 0, true};
        }
      }
      else
      {
        std::string btn_id =
            fmt::format("{}##{}", LOC("TRANSFER_BUY"), p.getId());
        if (ImGui::Button(btn_id.c_str()))
        {
          confirm_state = {true, p.getId(), p.getTeamId(), price, false};
        }
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

    if (std::string combo_preview =
            listable_players[list_player_index]->getName();
        ImGui::BeginCombo(LOC("TRANSFER_SELECT_PLAYER"), combo_preview.c_str()))
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
      if (std::string accept_id =
              fmt::format("{}##{}", LOC("TRANSFER_ACCEPT"), pid);
          ImGui::Button(accept_id.c_str()))
      {
        controller.acceptBid(pid);
        refreshData();
      }
      ImGui::SameLine();
      if (std::string reject_id =
              fmt::format("{}##{}", LOC("TRANSFER_REJECT"), pid);
          ImGui::Button(reject_id.c_str()))
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
    if (auto player_opt =
            controller.getGameData()->getPlayer(confirm_state.player_id);
        player_opt.has_value())
    {
      if (confirm_state.is_free_agent)
      {
        ImGui::Text("%s", fmt::sprintf(LOC("TRANSFER_CONFIRM_SIGN"),
                                       player_opt->get().getName())
                              .c_str());
      }
      else
      {
        ImGui::Text(
            "%s", fmt::sprintf(LOC("TRANSFER_CONFIRM_BUY"),
                               player_opt->get().getName(), confirm_state.price)
                      .c_str());
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
    if (auto it = all_listings.find(counter_state.player_id);
        it != all_listings.end())
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
