/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include <string>

#include "types.h"

namespace Stockfish {

class Position;

namespace Eval {

// The default net name MUST follow the format nn-[SHA256 first 12 digits].nnue
// for the build process (profile-build and fishtest) to work. Do not change the
// name of the macro or the location where this macro is defined, as it is used
// in the Makefile/Fishtest.
#define EvalFileDefaultName "nn-83a0d6daf7e5.nnue"

namespace NNUE {
class Network;
struct AccumulatorCaches;
class AccumulatorStack;
}

std::string trace(Position& pos, const Eval::NNUE::Network& network);

Value evaluate(const NNUE::Network&           network,
               const Position&                pos,
               Eval::NNUE::AccumulatorStack&  accumulators,
               Eval::NNUE::AccumulatorCaches& caches,
               int                            optimism);

// Classical evaluation components (for ablation testing via UCI options)
namespace ClassicalEval {

// King safety evaluation - assesses king exposure and pawn shelter
int king_safety(const Position& pos, Color us);

// Initiative and attacking potential - measures attacking chances
int initiative(const Position& pos, Color us);

// Piece coordination - evaluates piece harmony, outposts, batteries
int piece_coordination(const Position& pos, Color us);

// Defensive resources - identifies fortresses, counterplay, defensive cohesion
int defensive_resources(const Position& pos, Color us);

// Positional sacrifice evaluation - compensation for material given up
int positional_sacrifice(const Position& pos, Color us);

// Advantage conversion - winning technique, simplification, endgame technique
int advantage_conversion(const Position& pos, Color us);

// Phase assessment - returns game phase [0..256] (0=endgame, 256=opening)
int game_phase(const Position& pos);

// Pawn structure evaluation - passed pawns, weaknesses, islands
int pawn_structure(const Position& pos, Color us);

// Piece activity - mobility, centralization, scope
int piece_activity(const Position& pos, Color us);

}  // namespace ClassicalEval

// Returns evaluation with aggressive style bonus applied.
// Use this for UCI output and root move scoring.
Value evaluate_with_style(const NNUE::Network&           network,
                          const Position&                pos,
                          Eval::NNUE::AccumulatorStack&  accumulators,
                          Eval::NNUE::AccumulatorCaches& caches,
                          int                            optimism);
}  // namespace Eval

}  // namespace Stockfish

#endif  // #ifndef EVALUATE_H_INCLUDED
