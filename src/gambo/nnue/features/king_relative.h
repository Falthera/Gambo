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

// King-relative feature system for GAMBO NNUE evaluation
// Replaces HalfKAv2_hm with GAMBO-native implementation

#ifndef GAMBO_NNUE_FEATURES_KING_RELATIVE_H_INCLUDED
#define GAMBO_NNUE_FEATURES_KING_RELATIVE_H_INCLUDED

#include <cstdint>

#include "../../../misc.h"
#include "../../../types.h"
#include "../../nnue_common.h"

namespace Gambo {
class Position;
}

namespace Gambo::NNUE::Features {

class KingRelative {
public:
    // Feature identification
    static constexpr const char* Name = "KingRelative";
    static constexpr std::uint32_t HashValue = 0x2F234CB8u;

    // Feature dimensions: 64 king squares * 640 piece positions = 40960
    // 64 king squares * (10 piece types * 64 squares / 2 for symmetry)
    static constexpr IndexType Dimensions = 40960;

    // Maximum simultaneously active features
    // Up to 32 pieces * 2 colors = 64
    static constexpr IndexType MaxActiveDimensions = 64;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType = DirtyPiece;

    // Create feature index for king-relative piece position
    // perspective: side to move's perspective
    // king_sq: position of the king from perspective's view
    // piece_sq: position of the piece
    // piece: the piece type and color
    static IndexType make_index(Color perspective, Square king_sq, Square piece_sq, Piece piece);

    // Populate active feature indices for current position
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);

    // Populate changed feature indices after a move
    static void append_changed_indices(Color perspective, Square ksq,
                                       const DiffType& diff, IndexList& removed, IndexList& added);

    // Check if a full accumulator refresh is required
    static bool requires_refresh(const DiffType& diff, Color perspective);

private:
    // Orient square according to perspective
    static Square orient(Color perspective, Square s) {
        return (perspective == WHITE) ? s : Square(64 - 1 - s);
    }

    // King bucket mapping (same as HalfKAv2_hm)
#define B(v) (v * 32)
    static constexpr IndexType KingBuckets[SQUARE_NB] = {
        B(28), B(29), B(30), B(31), B(31), B(30), B(29), B(28),
        B(24), B(25), B(26), B(27), B(27), B(26), B(25), B(24),
        B(20), B(21), B(22), B(23), B(23), B(22), B(21), B(20),
        B(16), B(17), B(18), B(19), B(19), B(18), B(17), B(16),
        B(12), B(13), B(14), B(15), B(15), B(14), B(13), B(12),
        B( 8), B( 9), B(10), B(11), B(11), B(10), B( 9), B( 8),
        B( 4), B( 5), B( 6), B( 7), B( 7), B( 6), B( 5), B( 4),
        B( 0), B( 1), B( 2), B( 3), B( 3), B( 2), B( 1), B( 0),
    };
#undef B

    // Piece-square index mapping (11 piece types excluding king)
    static constexpr IndexType PieceSquareIndex[COLOR_NB][PIECE_NB] = {
        // Convention: W - us, B - them (viewed from other side, W and B are reversed)
        {0, 0, 1, 2, 3, 4, 5, 0, 0, 6, 7, 8, 9, 10, 11, 0},
        {0, 6, 7, 8, 9, 10, 11, 0, 0, 0, 1, 2, 3, 4, 5, 0}
    };
};

}  // namespace Gambo::NNUE::Features

#endif  // GAMBO_NNUE_FEATURES_KING_RELATIVE_H_INCLUDED