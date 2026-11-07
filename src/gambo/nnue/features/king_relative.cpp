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

// King-relative feature system implementation
// Replaces HalfKAv2_hm with GAMBO-native implementation

#include "king_relative.h"

#include "../../../position.h"
#include "../nnue_common.h"

namespace Gambo::NNUE::Features {

IndexType KingRelative::make_index(Color perspective, Square king_sq,
                                   Square piece_sq, Piece piece) {
    const Color piece_color = color_of(piece);
    const PieceType pt = type_of(piece);
    
    // Piece type index: 0-9 (excluding king)
    int pt_idx;
    switch (pt) {
        case PAWN:   pt_idx = (piece_color == WHITE) ? 0 : 5; break;
        case KNIGHT: pt_idx = (piece_color == WHITE) ? 1 : 6; break;
        case BISHOP: pt_idx = (piece_color == WHITE) ? 2 : 7; break;
        case ROOK:   pt_idx = (piece_color == WHITE) ? 3 : 8; break;
        case QUEEN:  pt_idx = (piece_color == WHITE) ? 4 : 9; break;
        default:     pt_idx = 0; break;
    }
    
    const Square piece_sq_rel = orient(perspective, piece_sq);
    const IndexType king_idx = king_sq;  // 0-63
    
    return (king_idx * 10 + pt_idx) * 64 + piece_sq_rel;
}

void KingRelative::append_active_indices(Color perspective, const Position& pos, IndexList& active) {
    const Square ksq = pos.square<KING>(perspective);
    
    // Iterate all pieces except kings
    for (PieceType pt = PAWN; pt <= QUEEN; ++pt) {
        for (Color c : {WHITE, BLACK}) {
            Piece piece = make_piece(c, pt);
            Bitboard bb = pos.pieces(c, pt);
            while (bb) {
                Square sq = pop_lsb(bb);
                IndexType idx = make_index(perspective, pos.square<KING>(perspective), sq, piece);
                if (active.size() < MaxActiveDimensions) {
                    active.push_back(idx);
                }
            }
        }
    }
}

void KingRelative::append_changed_indices(Color perspective, Square ksq,
                                           const Position& pos, const DiffType& diff, IndexList& removed, IndexList& added) {
    // For piece moves: remove old position, add new position
    if (diff.type() == DirtyPiece::Moved) {
        const Piece pc = diff.moved_piece();
        if (type_of(pc) != KING) {
            Square from = diff.from_sq();
            Square to = diff.to_sq();
            
            IndexType old_idx = make_index(perspective, ksq, from, pc);
            IndexType new_idx = make_index(perspective, ksq, to, pc);
            
            if (removed.size() < MaxActiveDimensions) removed.push_back(old_idx);
            if (added.size() < MaxActiveDimensions) added.push_back(new_idx);
        }
    }
    // For captures: remove captured piece
    else if (diff.type() == DirtyPiece::Captured) {
        const Piece pc = diff.captured_piece();
        Square sq = diff.to_sq();
        
        IndexType idx = make_index(perspective, ksq, sq, pc);
        if (removed.size() < MaxActiveDimensions) removed.push_back(idx);
    }
    // For promotions: remove pawn, add promoted piece
    else if (diff.type() == DirtyPiece::Promoted) {
        const Piece pc = diff.moved_piece();
        Square to = diff.to_sq();
        
        IndexType old_idx = make_index(perspective, ksq, to, make_piece(perspective, PAWN));
        IndexType new_idx = make_index(perspective, ksq, to, pc);
        
        if (removed.size() < MaxActiveDimensions) removed.push_back(old_idx);
        if (added.size() < MaxActiveDimensions) added.push_back(new_idx);
    }
}

bool KingRelative::requires_refresh(const DiffType& diff, Color perspective) {
    // King moves require full refresh
    if (diff.type() == DirtyPiece::Moved) {
        const Piece pc = diff.moved_piece();
        if (type_of(pc) == KING) return true;
    }
    return false;
}

// King bucket mapping (same as HalfKAv2_hm)
#define B(v) (v * 32)
const IndexType KingRelative::KingBuckets[SQUARE_NB] = {
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
// Convention: W - us, B - them (viewed from other side, W and B are reversed)
const IndexType KingRelative::PieceSquareIndex[COLOR_NB][PIECE_NB] = {
    {0, 0, 1, 2, 3, 4, 5, 0, 0, 6, 7, 8, 9, 10, 11, 0},
    {0, 6, 7, 8, 9, 10, 11, 0, 0, 0, 1, 2, 3, 4, 5, 0}
};

// Orient square according to perspective
Square KingRelative::orient(Color perspective, Square s) {
    return (perspective == WHITE) ? s : Square(63 - s);
}

}  // namespace Gambo::NNUE::Features