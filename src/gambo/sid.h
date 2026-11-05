/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// SID — Search Instability Detection
// Monitors PV changes, evaluation swings, and tactical volatility

#ifndef GAMBO_SID_H_INCLUDED
#define GAMBO_SID_H_INCLUDED

#include <cstdint>

// Include required Stockfish headers
#include "../position.h"
#include "../types.h"
#include "../search.h"

namespace Gambo::SID {

// Instability levels
enum class InstabilityLevel : int {
    STABLE    = 0,  // No significant instability detected
    MINOR     = 1,  // Small evaluation swings, PV stable
    MODERATE  = 2,  // Noticeable PV changes or eval swings
    HIGH      = 3,  // Frequent PV changes, large eval swings
    CRITICAL  = 4   // Extreme instability, search unreliable
};

// Search Instability Detection - independent monitoring system
class SearchInstabilityDetector {
public:
    SearchInstabilityDetector() = default;

    // Update with current search state, returns instability level
    InstabilityLevel update(const Stockfish::Position& pos, const Stockfish::Search::Stack* ss,
                           int depth, Stockfish::Value currentScore, Stockfish::Value prevScoreIn,
                           const Stockfish::Search::PVMoves* pv, const Stockfish::Search::PVMoves* prevPVIn);

    // Get current instability metrics
    int get_pv_changes() const { return pvChanges; }
    int get_eval_swings() const { return evalSwings; }
    int get_tactical_volatility() const { return tacticalVolatility; }
    int get_total_instability() const { return totalInstability; }

    // Reset for new search
    void reset() {
        pvChanges = 0;
        evalSwings = 0;
        tacticalVolatility = 0;
        totalInstability = 0;
        prevPV.clear();
        prevScore = Stockfish::VALUE_NONE;
    }

    // Check if verification search is needed
    bool needs_verification() const {
        return totalInstability > 50;
    }

private:
    int pvChanges = 0;
    int evalSwings = 0;
    int tacticalVolatility = 0;
    int totalInstability = 0;
    Stockfish::Search::PVMoves prevPV;
    Stockfish::Value prevScore = Stockfish::VALUE_NONE;
};

}  // namespace Gambo::SID

#endif  // GAMBO_SID_H_INCLUDED