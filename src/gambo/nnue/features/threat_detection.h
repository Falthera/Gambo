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

// Threat detection feature system for GAMBO NNUE evaluation
// Replaces FullThreats with GAMBO-native implementation

#ifndef GAMBO_NNUE_FEATURES_THREAT_DETECTION_H_INCLUDED
#define GAMBO_NNUE_FEATURES_THREAT_DETECTION_H_INCLUDED

#include <cstdint>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Gambo {
class Position;
}

namespace Gambo::NNUE::Features {

struct FusedUpdateData {
    Bitboard dp2removedOriginBoard = 0;
    Bitboard dp2removedTargetBoard = 0;
    Square dp2removed = SQ_NONE;
};

class ThreatDetection {
public:
    // Feature identification
    static constexpr const char* Name = "ThreatDetection";
    static constexpr std::uint32_t HashValue = 0x3F234CB8u;

    // Feature dimensions: ~60,720 (attacker piece * target piece * from * to * king_sq)
    static constexpr IndexType Dimensions = 60720;

    // Maximum simultaneously active features
    static constexpr IndexType MaxActiveDimensions = 128;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType = DirtyThreats;

    // Create feature index for threat relationship
    // perspective: side to move's perspective
    // attacker: attacking piece type and color
    // from: source square of attack
    // to: target square
    // target: target piece type and color
    // king_sq: position of the king from perspective's view
    static IndexType make_index(Color perspective, Piece attacker, Square from, Square to,
                                Piece target, Square king_sq);

    // Populate active feature indices for current position
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);

    // Populate changed feature indices after a move
    static void append_changed_indices(Color perspective, Square ksq,
                                       const DiffType& diff, IndexList& removed, IndexList& added,
                                       FusedUpdateData* fd = nullptr, bool first = false);

    // Check if a full accumulator refresh is required
    static bool requires_refresh(const DiffType& diff, Color perspective);

private:
    // Orient square according to perspective
    static Square orient(Color perspective, Square s) {
        return (perspective == WHITE) ? s : Square(63 - s);
    }

    // King bucket mapping
    static constexpr std::int8_t OrientTBL[SQUARE_NB] = {
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
    };

    // Piece type mapping for threats
    static constexpr int map[PIECE_TYPE_NB-2][PIECE_TYPE_NB-2] = {
        { 0,  1, -1,  2, -1, -1},
        { 0,  1,  2,  3,  4, -1},
        { 0,  1,  2,  3, -1, -1},
        { 0,  1,  2,  3, -1, -1},
        { 0,  1,  2,  3, -1, -1},
        {-1, -1, -1, -1, -1, -1}
    };
};

}  // namespace Gambo::NNUE::Features

#endif  // GAMBO_NNUE_FEATURES_THREAT_DETECTION_H_INCLUDED