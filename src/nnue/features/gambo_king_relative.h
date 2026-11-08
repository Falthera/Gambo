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

// Gambo KingRelative feature set for NNUE evaluation

#ifndef GAMBO_NNUE_FEATURES_KING_RELATIVE_H_INCLUDED
#define GAMBO_NNUE_FEATURES_KING_RELATIVE_H_INCLUDED

#include <cstdint>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"
#include "../../../gambo/nnue/features/king_relative.h"

namespace Stockfish {
class Position;
}

namespace Gambo::NNUE::Features {

using namespace Stockfish;

class KingRelativeWrapper {
public:
    using IndexType = std::uint32_t;
    using DiffType = DirtyPiece;
    static constexpr IndexType Dimensions = KingRelative::Dimensions;
    static constexpr IndexType MaxActiveDimensions = KingRelative::MaxActiveDimensions;
    using IndexList = typename KingRelative::IndexList;

    static IndexType make_index(Color perspective, Square king_sq, Square piece_sq, Piece piece) {
        return KingRelative::make_index(perspective, king_sq, piece_sq, piece);
    }

    static void append_active_indices(Color perspective, const Position& pos, IndexList& active) {
        KingRelative::append_active_indices(perspective, pos, active);
    }

    static void append_changed_indices(Color perspective, Square ksq,
                                       const Position& pos, const DiffType& diff, IndexList& removed, IndexList& added) {
        KingRelative::append_changed_indices(perspective, ksq, pos, diff, removed, added);
    }

    static bool requires_refresh(const DiffType& diff, Color perspective) {
        return KingRelative::requires_refresh(diff, perspective);
    }
};

}  // namespace Gambo::NNUE::Features

#endif  // GAMBO_NNUE_FEATURES_KING_RELATIVE_H_INCLUDED