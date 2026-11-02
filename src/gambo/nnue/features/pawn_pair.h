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

// Pawn-pair feature system for GAMBO NNUE evaluation
// GAMBO-native implementation of SFNNv16-equivalent pawn-pair features

#ifndef GAMBO_NNUE_FEATURES_PAWN_PAIR_H_INCLUDED
#define GAMBO_NNUE_FEATURES_PAWN_PAIR_H_INCLUDED

#include <cstdint>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Gambo {
class Position;
}

namespace Gambo::NNUE::Features {

// Pawn-pair relationship types
enum class PawnPairType : std::uint8_t {
    CONNECTED   = 0,  // Adjacent files, same/adjacent rank
    ISOLATED    = 1,  // No friendly pawn on adjacent files
    DOUBLED     = 2,  // Two pawns on same file
    BACKWARD    = 3,  // Behind adjacent friendly pawns, cannot advance safely
    PASSED      = 4,  // No enemy pawns blocking promotion path
    CANDIDATE   = 5,  // Potential passed pawn (blocked by 1 enemy pawn)
    PROTECTED   = 6,  // Defended by friendly pawn
    PHALANX     = 7,  // Same rank, adjacent files
    PAIR_NB     = 8
};

class PawnPair {
public:
    // Feature identification
    static constexpr const char* Name = "PawnPair";
    static constexpr std::uint32_t HashValue = 0x1F234CB8u;

    // Feature dimensions: ~8192 (configurable)
    // 64 king squares * 32 pawn squares * 4 relationship types / 2 (symmetry)
    static constexpr IndexType Dimensions = 8192;

    // Maximum simultaneously active features (typical pawn configurations)
    static constexpr IndexType MaxActiveDimensions = 128;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType = DirtyPiece;

    // Create feature index for a pawn pair
    // perspective: side to move's perspective
    // king_sq: position of the king from perspective's view
    // pawn1, pawn2: the two pawns forming the pair
    // type: the relationship type between the two pawns
    static IndexType make_index(Color perspective, Square king_sq,
                                Square pawn1, Square pawn2, PawnPairType type);

    // Populate active feature indices for current position
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);

    // Populate changed feature indices after a move
    static void append_changed_indices(Color perspective, Square ksq,
                                       const DiffType& diff, IndexList& removed, IndexList& added);

    // Check if a full accumulator refresh is required
    static bool requires_refresh(const DiffType& diff, Color perspective);

private:
    // Detect pawn-pair relationship type between two pawns
    static PawnPairType detect_pair_type(Color perspective, const Position& pos,
                                         Square pawn1, Square pawn2);

    // Check if two squares are on adjacent files
    static bool adjacent_files(Square a, Square b);

    // Check if square b is behind square a from perspective
    static bool is_behind(Color perspective, Square a, Square b);

    // Check if pawn on square is passed
    static bool is_passed(Color perspective, const Position& pos, Square sq);

    // Check if pawn on square is candidate passed
    static bool is_candidate(Color perspective, const Position& pos, Square sq);

    // Check if pawn on square is protected by friendly pawn
    static bool is_protected(Color perspective, const Position& pos, Square sq);

    // Check if two pawns form a phalanx (same rank, adjacent files)
    static bool is_phalanx(Color perspective, Square a, Square b);

    // Check if two pawns are connected (adjacent files, same/adjacent rank)
    static bool is_connected(Color perspective, Square a, Square b);

    // Check if two pawns are doubled (same file)
    static bool is_doubled(Square a, Square b);

    // Check if pawn is isolated (no friendly pawn on adjacent files)
    static bool is_isolated(Color perspective, const Position& pos, Square sq);

    // Check if pawn is backward (behind adjacent friendly pawns, cannot advance)
    static bool is_backward_pawn(Color perspective, const Position& pos, Square sq);
};

}  // namespace Gambo::NNUE::Features

#endif  // GAMBO_NNUE_FEATURES_PAWN_PAIR_H_INCLUDED