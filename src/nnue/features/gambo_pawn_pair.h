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

// Gambo PawnPair feature set for NNUE evaluation

#ifndef GAMBO_NNUE_FEATURES_PAWN_PAIR_H_INCLUDED
#define GAMBO_NNUE_FEATURES_PAWN_PAIR_H_INCLUDED

#include <cstdint>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"
#include "../../gambo/nnue/features/pawn_pair.h"

namespace Stockfish {
class Position;
}

namespace Gambo::NNUE::Features {

using namespace Stockfish;

class PawnPairWrapper {
public:
    using IndexType = std::uint32_t;
    using DiffType = DirtyPiece;
    static constexpr IndexType Dimensions = PawnPair::Dimensions;
    static constexpr IndexType MaxActiveDimensions = PawnPair::MaxActiveDimensions;
    using IndexList = typename PawnPair::IndexList;

    static IndexType make_index(Color perspective, Square king_sq,
                                Square pawn1, Square pawn2, PawnPairType type) {
        return PawnPair::make_index(perspective, king_sq, pawn1, pawn2, type);
    }

    static void append_active_indices(Color perspective, const Position& pos, IndexList& active) {
        PawnPair::append_active_indices(perspective, pos, active);
    }

    static void append_changed_indices(Color perspective, Square ksq,
                                       const Position& pos, const DiffType& diff, IndexList& removed, IndexList& added) {
        PawnPair::append_changed_indices(perspective, ksq, pos, diff, removed, added);
    }

    static bool requires_refresh(const DiffType& diff, Color perspective) {
        return PawnPair::requires_refresh(diff, perspective);
    }
};

}  // namespace Gambo::NNUE::Features

#endif  // GAMBO_NNUE_FEATURES_PAWN_PAIR_H_INCLUDED