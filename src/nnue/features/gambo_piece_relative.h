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

// Gambo PieceRelative feature set for NNUE evaluation

#ifndef GAMBO_NNUE_FEATURES_PIECE_RELATIVE_H_INCLUDED
#define GAMBO_NNUE_FEATURES_PIECE_RELATIVE_H_INCLUDED

#include <cstdint>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"
#include "../../../gambo/nnue/features/piece_relative.h"

namespace Stockfish {
class Position;
}

namespace Gambo::NNUE::Features {

using namespace Stockfish;

class PieceRelativeWrapper {
public:
    using IndexType = std::uint32_t;
    using DiffType = DirtyPiece;
    static constexpr IndexType Dimensions = PieceRelative::Dimensions;
    static constexpr IndexType MaxActiveDimensions = PieceRelative::MaxActiveDimensions;
    using IndexList = typename PieceRelative::IndexList;

    enum CoordType : std::uint8_t {
        COORD_NONE = 0,
        COORD_BATTERY_ROOK = 1,
        COORD_BATTERY_BISHOP = 2,
        COORD_KNIGHT_OUTPOST = 3,
        COORD_PAWN_CHAIN = 4,
        COORD_ROOK_OPEN_FILE = 5,
        COORD_BISHOP_PAIR = 6,
        COORD_KNIGHT_PAIR = 7,
        COORD_QUEEN_KNIGHT = 8,
        COORD_ROOK_ROOK = 9,
        COORD_NB = 10
    };

    static IndexType make_index(CoordType type, Square sq1, Square sq2, Color perspective) {
        return PieceRelative::make_index(type, sq1, sq2, perspective);
    }

    static void append_active_indices(Color perspective, const Position& pos, IndexList& active) {
        PieceRelative::append_active_indices(perspective, pos, active);
    }

    static void append_changed_indices(Color perspective, Square ksq,
                                       const Position& pos, const DiffType& diff, IndexList& removed, IndexList& added) {
        PieceRelative::append_changed_indices(perspective, ksq, pos, diff, removed, added);
    }

    static bool requires_refresh(const DiffType& diff, Color perspective) {
        return PieceRelative::requires_refresh(diff, perspective);
    }
};

}  // namespace Gambo::NNUE::Features

#endif  // GAMBO_NNUE_FEATURES_PIECE_RELATIVE_H_INCLUDED