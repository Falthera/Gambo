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

// Definition of input features KingSafety of NNUE evaluation function
// King-relative features for both friendly and enemy kings

#ifndef NNUE_FEATURES_KING_SAFETY_H_INCLUDED
#define NNUE_FEATURES_KING_SAFETY_H_INCLUDED

#include <cstdint>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE::Features {

class KingSafety {
    // Unique number for each piece type on each square relative to king
    enum {
        KS_NONE     = 0,
        KS_W_PAWN   = 0,
        KS_B_PAWN   = 1 * SQUARE_NB,
        KS_W_KNIGHT = 2 * SQUARE_NB,
        KS_B_KNIGHT = 3 * SQUARE_NB,
        KS_W_BISHOP = 4 * SQUARE_NB,
        KS_B_BISHOP = 5 * SQUARE_NB,
        KS_W_ROOK   = 6 * SQUARE_NB,
        KS_B_ROOK   = 7 * SQUARE_NB,
        KS_W_QUEEN  = 8 * SQUARE_NB,
        KS_B_QUEEN  = 9 * SQUARE_NB,
        KS_KING     = 10 * SQUARE_NB,
        KS_NB       = 11 * SQUARE_NB
    };

    static constexpr IndexType PieceSquareIndex[COLOR_NB][PIECE_NB] = {
      // Convention: W - us, B - them (relative to king perspective)
      {KS_NONE, KS_W_PAWN, KS_W_KNIGHT, KS_W_BISHOP, KS_W_ROOK, KS_W_QUEEN, KS_KING, KS_NONE,
       KS_NONE, KS_B_PAWN, KS_B_KNIGHT, KS_B_BISHOP, KS_B_ROOK, KS_B_QUEEN, KS_KING, KS_NONE},
      {KS_NONE, KS_B_PAWN, KS_B_KNIGHT, KS_B_BISHOP, KS_B_ROOK, KS_B_QUEEN, KS_KING, KS_NONE,
       KS_NONE, KS_W_PAWN, KS_W_KNIGHT, KS_W_BISHOP, KS_W_ROOK, KS_W_QUEEN, KS_KING, KS_NONE}};

public:
    // Feature name
    static constexpr const char* Name = "KingSafety(Friend,Enemy)";

    // Hash value embedded in the evaluation file
    static constexpr std::uint32_t HashValue = 0x9f234cb8u;

    // Number of feature dimensions: 2 kings * (64 squares * 11 piece types / 2)
    static constexpr IndexType Dimensions =
      static_cast<IndexType>(SQUARE_NB) * static_cast<IndexType>(KS_NB);

    // King bucket mapping - same as HalfKAv2_hm but for both kings
#define KS_B(v) (v * KS_NB)
    static constexpr IndexType KingBuckets[SQUARE_NB] = {
        KS_B(28), KS_B(29), KS_B(30), KS_B(31), KS_B(31), KS_B(30), KS_B(29), KS_B(28),
        KS_B(24), KS_B(25), KS_B(26), KS_B(27), KS_B(27), KS_B(26), KS_B(25), KS_B(24),
        KS_B(20), KS_B(21), KS_B(22), KS_B(23), KS_B(23), KS_B(22), KS_B(21), KS_B(20),
        KS_B(16), KS_B(17), KS_B(18), KS_B(19), KS_B(19), KS_B(18), KS_B(17), KS_B(16),
        KS_B(12), KS_B(13), KS_B(14), KS_B(15), KS_B(15), KS_B(14), KS_B(13), KS_B(12),
        KS_B( 8), KS_B( 9), KS_B(10), KS_B(11), KS_B(11), KS_B(10), KS_B( 9), KS_B( 8),
        KS_B( 4), KS_B( 5), KS_B( 6), KS_B( 7), KS_B( 7), KS_B( 6), KS_B( 5), KS_B( 4),
        KS_B( 0), KS_B( 1), KS_B( 2), KS_B( 3), KS_B( 3), KS_B( 2), KS_B( 1), KS_B( 0),
    };
#undef KS_B

    // Orient a square according to perspective (rotates by 180 for black)
    static constexpr IndexType OrientTBL[SQUARE_NB] = {
        SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
        SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
        SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
        SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
        SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
        SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
        SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
        SQ_H1, SQ_H1, SQ_H1, SQ_H1, SQ_A1, SQ_A1, SQ_A1, SQ_A1,
    };

    // Maximum number of simultaneously active features.
    // 2 kings * up to 32 pieces = 64
    static constexpr IndexType MaxActiveDimensions = 64;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType = DirtyPiece;

    // Index of a feature for a given king position and another piece on some square
    static IndexType make_index(Color perspective, Square s, Piece pc, Square ksq);

    // Get a list of indices for active features (both kings)
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);

    // Get a list of indices for recently changed features
    static void append_changed_indices(
      Color perspective, Square ksq, const DiffType& diff, IndexList& removed, IndexList& added);

    // Returns whether the change stored in this DirtyPiece means
    // that a full accumulator refresh is required.
    static bool requires_refresh(const DiffType& diff, Color perspective);
};

}  // namespace Stockfish::Eval::NNUE::Features

#endif  // #ifndef NNUE_FEATURES_KING_SAFETY_H_INCLUDED