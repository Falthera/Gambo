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

// Definition of input features PieceCoordination of NNUE evaluation function
// Pairwise piece coordination features (batteries, forks, outposts, etc.)

#ifndef NNUE_FEATURES_PIECE_COORDINATION_H_INCLUDED
#define NNUE_FEATURES_PIECE_COORDINATION_H_INCLUDED

#include <cstdint>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE::Features {

class PieceCoordination {
public:
    // Feature name
    static constexpr const char* Name = "PieceCoordination";

    // Hash value embedded in the evaluation file
    static constexpr std::uint32_t HashValue = 0xAf234cb8u;

    // Number of feature dimensions
    // Piece pairs: 10 piece types (excl kings) * 10 piece types * 64 * 64 / 2 (symmetry)
    // Reduced: only consider piece pairs within attack/defense range
    static constexpr IndexType Dimensions = 8192;

    // Maximum number of simultaneously active features
    static constexpr IndexType MaxActiveDimensions = 128;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType = DirtyPiece;

    // Coordination types
    enum CoordType {
        COORD_NONE = 0,
        COORD_BATTERY_ROOK = 1,      // Rook + Queen on same file/rank
        COORD_BATTERY_BISHOP = 2,    // Bishop + Queen on same diagonal
        COORD_KNIGHT_OUTPOST = 3,    // Knight on protected outpost
        COORD_PAWN_CHAIN = 4,        // Pawns protecting each other
        COORD_ROOK_OPEN_FILE = 5,    // Rook on open/semi-open file
        COORD_BISHOP_PAIR = 6,       // Two bishops
        COORD_KNIGHT_PAIR = 7,       // Two knights
        COORD_QUEEN_KNIGHT = 8,      // Queen + Knight coordination
        COORD_ROOK_ROOK = 9,         // Connected rooks
        COORD_NB = 10
    };

    static IndexType make_index(CoordType type, Square sq1, Square sq2, Color perspective);

    // Get a list of indices for active features
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);

    // Get a list of indices for recently changed features
    static void append_changed_indices(
        Color perspective, Square ksq, const DiffType& diff, IndexList& removed, IndexList& added);

    // Extended version for incremental updates with feature weights
    static void append_changed_indices(
        Color perspective, Square ksq, const DiffType& diff, IndexList& removed, IndexList& added,
        const IndexType* pfBase, IndexType pfStride);

    // Returns whether the change stored in this DirtyPiece means
    // that a full accumulator refresh is required.
    static bool requires_refresh(const DiffType& diff, Color perspective);
};

}  // namespace Stockfish::Eval::NNUE::Features

#endif  // #ifndef NNUE_FEATURES_PIECE_COORDINATION_H_INCLUDED