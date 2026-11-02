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

#include "king_safety.h"

#include "../../bitboard.h"
#include "../../position.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish::Eval::NNUE::Features {

// Index of a feature for a given king position and another piece on some square
IndexType KingSafety::make_index(Color perspective, Square s, Piece pc, Square ksq) {
    const IndexType flip = 56 * perspective;
    const Color pcColor = color_of(pc);
    const PieceType pt = type_of(pc);

    // Skip if piece is a king (handled by bucket)
    if (pt == KING)
        return IndexType(0);

    // Determine piece-square index based on perspective and piece color
    IndexType psIdx;
    if (pcColor == perspective) {
        // Friendly piece
        switch (pt) {
            case PAWN:   psIdx = KS_W_PAWN; break;
            case KNIGHT: psIdx = KS_W_KNIGHT; break;
            case BISHOP: psIdx = KS_W_BISHOP; break;
            case ROOK:   psIdx = KS_W_ROOK; break;
            case QUEEN:  psIdx = KS_W_QUEEN; break;
            default:     psIdx = KS_NONE; break;
        }
    } else {
        // Enemy piece
        switch (pt) {
            case PAWN:   psIdx = KS_B_PAWN; break;
            case KNIGHT: psIdx = KS_B_KNIGHT; break;
            case BISHOP: psIdx = KS_B_BISHOP; break;
            case ROOK:   psIdx = KS_B_ROOK; break;
            case QUEEN:  psIdx = KS_B_QUEEN; break;
            default:     psIdx = KS_NONE; break;
        }
    }

    return (IndexType(s) ^ OrientTBL[ksq] ^ flip) + psIdx + KingBuckets[int(ksq) ^ flip];
}

// Get a list of indices for active features (both kings)
void KingSafety::append_active_indices(Color perspective, const Position& pos, IndexList& active) {
    // Friendly king perspective
    Square ourKsq = pos.square<KING>(perspective);
    Bitboard bb = pos.pieces();
    while (bb) {
        Square s = pop_lsb(bb);
        Piece pc = pos.piece_on(s);
        if (pc != NO_PIECE && type_of(pc) != KING) {
            active.push_back(make_index(perspective, s, pc, ourKsq));
        }
    }

    // Enemy king perspective - pieces relative to enemy king
    Square theirKsq = pos.square<KING>(~perspective);
    bb = pos.pieces();
    while (bb) {
        Square s = pop_lsb(bb);
        Piece pc = pos.piece_on(s);
        if (pc != NO_PIECE && type_of(pc) != KING) {
            active.push_back(make_index(perspective, s, pc, theirKsq));
        }
    }
}

// Get a list of indices for recently changed features
void KingSafety::append_changed_indices(
  Color perspective, Square ksq, const DiffType& diff, IndexList& removed, IndexList& added) {

    // Handle changes from our king's perspective
    if (diff.pc != NO_PIECE && type_of(diff.pc) != KING) {
        removed.push_back(make_index(perspective, diff.from, diff.pc, ksq));
        if (diff.to != SQ_NONE)
            added.push_back(make_index(perspective, diff.to, diff.pc, ksq));
    }

    if (diff.remove_pc != NO_PIECE && type_of(diff.remove_pc) != KING) {
        removed.push_back(make_index(perspective, diff.remove_sq, diff.remove_pc, ksq));
    }

    if (diff.add_pc != NO_PIECE && type_of(diff.add_pc) != KING) {
        added.push_back(make_index(perspective, diff.add_sq, diff.add_pc, ksq));
    }

    // Also handle from enemy king's perspective
    Square theirKsq = pos.square<KING>(~perspective);
    if (diff.pc != NO_PIECE && type_of(diff.pc) != KING) {
        removed.push_back(make_index(perspective, diff.from, diff.pc, theirKsq));
        if (diff.to != SQ_NONE)
            added.push_back(make_index(perspective, diff.to, diff.pc, theirKsq));
    }

    if (diff.remove_pc != NO_PIECE && type_of(diff.remove_pc) != KING) {
        removed.push_back(make_index(perspective, diff.remove_sq, diff.remove_pc, theirKsq));
    }

    if (diff.add_pc != NO_PIECE && type_of(diff.add_pc) != KING) {
        added.push_back(make_index(perspective, diff.add_sq, diff.add_pc, theirKsq));
    }
}

// Returns whether the change stored in this DirtyPiece means
// that a full accumulator refresh is required.
bool KingSafety::requires_refresh(const DiffType& diff, Color perspective) {
    return diff.pc == make_piece(perspective, KING) || diff.pc == make_piece(~perspective, KING);
}

}  // namespace Stockfish::Eval::NNUE::Features