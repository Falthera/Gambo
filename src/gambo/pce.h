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

// PCE — Position Criticality Engine
// Estimates how sensitive a position is to search mistakes

#ifndef GAMBO_PCE_H_INCLUDED
#define GAMBO_PCE_H_INCLUDED

#include <cstdint>

// Include required Stockfish headers
#include "../position.h"
#include "../types.h"
#include "../search.h"

namespace Gambo::PCE {

// Criticality levels
enum class CriticalityLevel : int {
    LOW       = 0,  // Stable position, standard search sufficient
    MEDIUM    = 1,  // Some tactical elements, increased attention
    HIGH      = 2,  // Sharp position, needs verification
    EXTREME   = 3   // Highly volatile, maximum resources
};

// Position Criticality Engine - estimates position sensitivity
class PositionCriticalityEngine {
public:
    // Compute criticality score [0..255] from position features
    static int evaluate(const Stockfish::Position& pos, const Stockfish::Search::Stack* ss, int depth);

    // Get criticality level from score
    static CriticalityLevel get_level(int score) {
        if (score < 64) return CriticalityLevel::LOW;
        if (score < 128) return CriticalityLevel::MEDIUM;
        if (score < 192) return CriticalityLevel::HIGH;
        return CriticalityLevel::EXTREME;
    }

private:
    // Signal extractors
    static int king_exposure(const Stockfish::Position& pos);
    static int forcing_move_density(const Stockfish::Position& pos);
    static int evaluation_volatility(const Stockfish::Position& pos, const Stockfish::Search::Stack* ss);
    static int pv_instability(const Stockfish::Search::Stack* ss);
    static int candidate_disagreement(const Stockfish::Position& pos);
    static int tactical_threat_density(const Stockfish::Position& pos);
    static int material_instability(const Stockfish::Position& pos);
    static int search_tree_instability(const Stockfish::Search::Stack* ss);
};

}  // namespace Gambo::PCE

#endif  // GAMBO_PCE_H_INCLUDED