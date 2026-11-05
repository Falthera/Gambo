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

// SID — Search Instability Detection implementation

#include "sid.h"

#include <algorithm>
#include <cstdint>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::SID {

InstabilityLevel SearchInstabilityDetector::update(
    const Position& pos, const Search::Stack* ss, int depth,
    Value currentScore, Value prevScore,
    const Search::PVMoves* pv, const Search::PVMoves* prevPV) {

    // 1. Track PV changes
    if (prevPV && pv && !prevPV->empty() && !pv->empty()) {
        if ((*prevPV)[0] != (*pv)[0]) {
            pvChanges += 2;
        }
    }

    // 2. Track evaluation swings
    if (prevScore != VALUE_NONE) {
        int swing = std::abs(currentScore - prevScore);
        if (swing > 100) // > 1 pawn
            evalSwings += swing / 50;
    }
    prevScore = currentScore;

    // 3. Tactical volatility from stack
    if (ss) {
        tacticalVolatility += ss->failHighCount;
        if (ss->inCheck) tacticalVolatility += 2;
    }

    // 4. Depth-weighted total
    int depthWeight = std::max(100 - depth * 2, 50);
    totalInstability = (pvChanges * 15 + evalSwings * 10 + tacticalVolatility * 5) * depthWeight / 100;

    // Clamp
    totalInstability = std::clamp(totalInstability, 0, 200);

    // Determine level
    if (totalInstability < 20) return InstabilityLevel::STABLE;
    if (totalInstability < 40) return InstabilityLevel::MINOR;
    if (totalInstability < 80) return InstabilityLevel::MODERATE;
    if (totalInstability < 140) return InstabilityLevel::HIGH;
    return InstabilityLevel::CRITICAL;
}

}  // namespace Gambo::SID