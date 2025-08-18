#include "lineup.h"
#include <sstream>
#include <algorithm>

// ---------------- Constructor -----------------
Lineup::Lineup()
{ gridClear(); }

// -------------------- Goalkeeper --------------------
void Lineup::setGoalkeeper(Player* gk) {
  goalkeeper = gk;  // nullptr allowed
}

Player* Lineup::getGoalkeeper() const {
  return goalkeeper;
}

// -------------- Grid (outfield players) ---------------
void Lineup::placePlayer(int row, int col, Player* player) {
  int index = toIndex(row, col);
  placePlayer(index, player);
}

void Lineup::removePlayer(int row, int col) {
  int index = toIndex(row, col);
  removePlayer(index);
}

Player* Lineup::getPlayerAt(int row, int col) const {
  int index = toIndex(row, col);
  return getPlayerAt(index);
}

void Lineup::placePlayer(int index, Player* player) {
  if (index < 0 || index >= LINEUP_GRID_SIZE) return;
  grid[index] = player;
}

void Lineup::removePlayer(int index) {
  if (index < 0 || index >= LINEUP_GRID_SIZE) return;
  grid[index] = nullptr;
}

Player* Lineup::getPlayerAt(int index) const {
  if (index < 0 || index >= LINEUP_GRID_SIZE) return nullptr;
  return grid[index];
}

// -------------- Reserves ---------------
void Lineup::setReserves(const std::vector<Player*>& subs) {
  reserves = subs;  // copies the pointers; nullptrs allowed
}

const std::vector<Player*>& Lineup::getReserves() const {
  return reserves;
}

// --------------- Strategy ------------------
void Lineup::setStrategy(const Strategy& strat) {
  strategy = strat;
}

const Strategy& Lineup::getStrategy() const {
  return strategy;
}

// ---------- Debug / Visualisation --------------
std::string Lineup::toString() const {
  std::ostringstream oss;
  oss << "Goalkeeper: " << (goalkeeper ? goalkeeper->getName() : "None") << "\n";
  oss << "Grid:\n";
  for (int r = 0; r < LINEUP_GRID_ROWS; ++r) {
    for (int c = 0; c < LINEUP_GRID_COLS; ++c) {
      Player* p = getPlayerAt(r, c);
      oss << "[" << (p ? p->getName() : "Empty") << "]";
    }
    oss << "\n";
  }
  oss << "Reserves: ";
  for (auto* sub : reserves) {
    oss << (sub ? sub->getName() : "Empty") << " ";
  }
  oss << "\n";
  return oss.str();
}

void Lineup::generateStartingXI(const std::vector<Player>& allPlayers, const StatsConfig& stats_config) {
  // Clear previous lineup
  gridClear();
  reserves.clear();

  std::vector<const Player*> outfieldPlayers;
  const Player* bestGK = nullptr;

  // Separate GK from outfield
  for (const auto& p : allPlayers) {
    if (p.getRole() == "Goalkeeper") {
      if (!bestGK || p.getOverall(stats_config) > bestGK->getOverall(stats_config))
        bestGK = &p;
    } else {
      outfieldPlayers.push_back(&p);
    }
  }

  // Set goalkeeper
  goalkeeper = const_cast<Player*>(bestGK); // safe because we store pointers
  if (!goalkeeper) return; // no GK found, abort

  // Sort outfield players by overall descending
  std::sort(outfieldPlayers.begin(), outfieldPlayers.end(),
            [&](const Player* a, const Player* b) {
            return a->getOverall(stats_config) > b->getOverall(stats_config);
            });

  // Place top 10 outfield players in grid (row 4 = defense, row 0 = attack)
  int rows = LINEUP_GRID_ROWS;
  int cols = LINEUP_GRID_COLS;
  int placed = 0;

  for (int r = rows - 1; r >= 0 && placed < 10; --r) {
    for (int c = 0; c < cols && placed < 10; ++c) {
      grid[toIndex(r, c)] = const_cast<Player*>(outfieldPlayers[placed]);
      ++placed;
    }
  }

  // Remaining players go to reserves
  for (size_t i = placed; i < outfieldPlayers.size(); ++i)
    reserves.push_back(const_cast<Player*>(outfieldPlayers[i]));
}
