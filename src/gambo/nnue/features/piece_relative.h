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

// Piece-relative feature system for GAMBO NNUE evaluation
// Replaces PieceCoordination with GAMBO-native implementation

#ifndef GAMBO_NNUE_FEATURES_PIECE_RELATIVE_H_INCLUDED
#define GAMBO_NNUE_FEATURES_PIECE_RELATIVE_H_INCLUDED

#include <cstdint>

#include "../../../misc.h"
#include "../../../types.h"
#include "../nnue_common.h"

namespace Gambo {
class Position;
}

namespace Gambo::NNUE::Features {

class PieceRelative {
public:
    enum CoordType : std::uint8_t {
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

    // Feature identification
    static constexpr const char* Name = "PieceRelative";
    static constexpr std::uint32_t HashValue = 0x4F234CB8u;

    // Feature dimensions: ~8,192
    static constexpr IndexType Dimensions = 8192;

    // Maximum simultaneously active features
    static constexpr IndexType MaxActiveDimensions = 128;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType = DirtyPiece;

    // Create feature index for piece coordination
    static IndexType make_index(CoordType type, Square sq1, Square sq2, Color perspective);

    // Populate active feature indices for current position
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);

    // Populate changed feature indices after a move
    static void append_changed_indices(Color perspective, Square ksq,
                                       const DiffType& diff, IndexList& removed, IndexList& added);

    // Check if a full accumulator refresh is required
    static bool requires_refresh(const DiffType& diff, Color perspective);

private:
    // Check if two squares are on same file
    static bool same_file(Square a, Square b) { return file_of(a) == file_of(b); }
    
    // Check if two squares are on same rank
    static bool same_rank(Square a, Square b) { return rank_of(a) == rank_of(b); }
    
    // Check if two squares are on same diagonal
    static bool same_diagonal(Square a, Square b) {
        return (file_of(a) + rank_of(a) == file_of(b) + rank_of(b)) ||
               (file_of(a) - rank_of(a) == file_of(b) - rank_of(b));
    }
    
    // Check if two squares are on same anti-diagonal
    static bool same_anti_diagonal(Square a, Square b) {
        return (file_of(a) + rank_of(a) == file_of(b) + rank_of(b)) ||
               (file_of(a) - rank_of(a) == file_of(b) - rank_of(b));
    }
};

}  // namespace Gambo::NNUE::Features

#endif  // GAMBO_NNUE_FEATURES_PIECE_RELATIVE_H_INCLUDED