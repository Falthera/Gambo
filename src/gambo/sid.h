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

#include "../position.h"
#include "../search.h"
#include "../types.h"

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
    InstabilityLevel update(const Position& pos, const Search::Stack* ss, 
                           int depth, Value currentScore, Value prevScore,
                           const Search::PVMoves* pv, const Search::PVMoves* prevPV);

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
        prevScore = VALUE_NONE;
    }

    // Check if verification search is needed
    bool needs_verification() const {
        return totalInstability > 50;
    }

    // SID: Time management integration
    // Returns time multiplier based on instability (1.0 = normal, >1.0 = more time)
    double get_time_multiplier() const {
        switch (get_instability_level()) {
            case InstabilityLevel::STABLE:    return 0.8;  // Use less time when stable
            case InstabilityLevel::MINOR:     return 1.0;
            case InstabilityLevel::MODERATE:  return 1.5;
            case InstabilityLevel::HIGH:      return 2.5;
            case InstabilityLevel::CRITICAL:  return 4.0;  // Allocate much more time
        }
        return 1.0;
    }

    InstabilityLevel get_instability_level() const {
        if (totalInstability < 20) return InstabilityLevel::STABLE;
        if (totalInstability < 40) return InstabilityLevel::MINOR;
        if (totalInstability < 80) return InstabilityLevel::MODERATE;
        if (totalInstability < 140) return InstabilityLevel::HIGH;
        return InstabilityLevel::CRITICAL;
    }

    // Get recommended depth extension based on instability
    Depth get_depth_extension() const {
        switch (get_instability_level()) {
            case InstabilityLevel::STABLE:    return 0;
            case InstabilityLevel::MINOR:     return 1;
            case InstabilityLevel::MODERATE:  return 2;
            case InstabilityLevel::HIGH:      return 3;
            case InstabilityLevel::CRITICAL:  return 5;
        }
        return 0;
    }

    // Check if we should trigger emergency verification
    bool needs_emergency_verification() const {
        return totalInstability > 180;
    }

private:
    int pvChanges = 0;
    int evalSwings = 0;
    int tacticalVolatility = 0;
    int totalInstability = 0;
    Search::PVMoves prevPV;
    Value prevScore = VALUE_NONE;
};

}  // namespace Gambo::SID

#endif  // GAMBO_SID_H_INCLUDED