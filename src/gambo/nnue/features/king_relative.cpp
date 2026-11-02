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
    // King bucket (0-31)
    const IndexType king_bucket = KingBuckets[king_sq];
    
    // Piece-square index (0-639 for 10 piece types * 64 squares)
    // PieceSquareIndex[perspective][piece] gives base offset
    const IndexType psq_idx = PieceSquareIndex[perspective][piece];
    const IndexType piece_sq_rel = orient(perspective, piece_sq);
    
    // Index = king_bucket * 1280 + psq_idx * 64 + piece_sq_rel
    // Wait: 40960 / 32 king_buckets = 1280 per bucket
    // 10 piece types * 64 squares = 640, but with symmetry = 320? No.
    // Let's use: king_bucket * 1280 + piece_sq_index
    // 1280 = 10 piece types * 64 squares / 2 (symmetry?) 
    // Actually: 10 piece types * 64 squares = 640, but with king symmetry
    // For HalfKAv2_hm: 64 king squares * 10 piece types * 64 squares / 2 = 20480
    // But we have 40960, so 2x that. Maybe we don't divide by 2?
    
    // Let's use: Dimensions = 40960 = 64 king squares * 640
    // 640 = 10 piece types * 64 squares
    // Per king bucket: 640
    // Index = king_bucket * 640 + piece_index * 64 + square
    // king_bucket is 0-63 (not 0-31!)
    
    // Actually our KingBuckets has 64 entries mapped to 0-31 buckets
    // But for KingRelative we want both kings, so 64 buckets
    
    // Let me use a simpler mapping:
    // 40960 = 64 king_squares * 10 piece_types * 64 squares
    // Index = (king_sq * 10 + piece_type) * 64 + piece_sq
    
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

// Orient square according to perspective
Square KingRelative::orient(Color perspective, Square s) {
    return (perspective == WHITE) ? s : Square(63 - s);
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
                                          const DiffType& diff, IndexList& removed, IndexList& added) {
    // For piece moves: remove old position, add new position
    if (diff.type() == DirtyPiece::Moved) {
        const Piece pc = diff.moved_piece();
        if (type_of(pc) != KING) {
            Square from = diff.from();
            Square to = diff.to();
            
            IndexType old_idx = make_index(perspective, ksq, from, pc);
            IndexType new_idx = make_index(perspective, ksq, to, pc);
            
            if (removed.size() < MaxActiveDimensions) removed.push_back(old_idx);
            if (added.size() < MaxActiveDimensions) added.push_back(new_idx);
        }
    }
    // For captures: remove captured piece
    else if (diff.type() == DirtyPiece::Captured) {
        const Piece pc = diff.captured_piece();
        Square sq = diff.to();
        
        IndexType idx = make_index(perspective, ksq, sq, pc);
        if (removed.size() < MaxActiveDimensions) removed.push_back(idx);
    }
    // For promotions: remove pawn, add promoted piece
    else if (diff.type() == DirtyPiece::Promoted) {
        const Piece pc = diff.moved_piece();
        Square to = diff.to();
        
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

// Orient square according to perspective
Square KingRelative::orient(Color perspective, Square s) {
    return (perspective == WHITE) ? s : Square(63 - s);
}

}  // namespace Gambo::NNUE::Features