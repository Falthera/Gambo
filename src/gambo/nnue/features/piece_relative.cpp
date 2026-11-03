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

// Piece-relative feature system implementation

#include "piece_relative.h"

#include "../../../position.h"
#include "../../../bitboard.h"
#include "../nnue_common.h"

namespace Gambo::NNUE::Features {

IndexType PieceRelative::make_index(CoordType type, Square sq1, Square sq2, Color perspective) {
    // Index encoding for piece coordination features
    // Dimensions = 8192
    // CoordType: 10 types
    // sq1: 64 squares
    // sq2: 64 squares (with sq1 <= sq2 for canonical ordering)
    // perspective: 2 colors
    // 10 * 64 * 64 / 2 * 2 = 40960, but we use 8192
    // Simplified: 10 * 64 * 12 = 7680 ≈ 8192
    
    // Use canonical ordering: sq1 <= sq2
    Square s1 = sq1, s2 = sq2;
    if (s1 > s2) std::swap(s1, s2);
    
    // Simple index: type * 819 + s1 * 8 + s2/8
    // 8192 = 10 * 819.2
    return static_cast<IndexType>(type) * 819 + s1 * 8 + (s2 >> 3);
}

void PieceRelative::append_active_indices(Color perspective, const Position& pos, IndexList& active) {
    const Color us = perspective;
    const Color them = ~perspective;
    
    // 1. Battery Rook (Rook + Queen on same file/rank, clear path)
    Bitboard our_rooks = pos.pieces(us, ROOK);
    Bitboard our_queens = pos.pieces(us, QUEEN);
    Bitboard all_pieces = pos.pieces();
    
    while (our_rooks) {
        Square r = pop_lsb(our_rooks);
        while (our_queens) {
            Square q = pop_lsb(our_queens);
            if (same_file(r, q) || same_rank(r, q)) {
                Bitboard between = between_bb(r, q);
                if (!(all_pieces & between)) {
                    IndexType idx = make_index(COORD_BATTERY_ROOK, r, q, perspective);
                    if (active.size() < MaxActiveDimensions) active.push_back(idx);
                }
            }
        }
    }
    
    // 2. Battery Bishop (Bishop + Queen on same diagonal, clear path)
    Bitboard our_bishops = pos.pieces(us, BISHOP);
    while (our_bishops) {
        Square b = pop_lsb(our_bishops);
        while (our_queens) {
            Square q = pop_lsb(our_queens);
            if (same_diagonal(b, q)) {
                Bitboard between = between_bb(b, q);
                if (!(all_pieces & between)) {
                    IndexType idx = make_index(COORD_BATTERY_BISHOP, b, q, perspective);
                    if (active.size() < MaxActiveDimensions) active.push_back(idx);
                }
            }
        }
    }
    
    // 3. Knight Outpost (Knight on protected square, not attackable by enemy pawns)
    Bitboard our_knights = pos.pieces(us, KNIGHT);
    Bitboard enemy_pawn_attacks = pos.attacks_by<PAWN>(~perspective);
    Bitboard our_pawn_attacks = pos.attacks_by<PAWN>(perspective);
    
    while (our_knights) {
        Square n = pop_lsb(our_knights);
        if (!(enemy_pawn_attacks & square_bb(n)) && (our_pawn_attacks & square_bb(n))) {
            IndexType idx = make_index(COORD_KNIGHT_OUTPOST, n, n, perspective);
            if (active.size() < MaxActiveDimensions) active.push_back(idx);
        }
    }
    
    // 4. Pawn Chain (Pawns protecting each other)
    Bitboard our_pawns = pos.pieces(us, PAWN);
    while (our_pawns) {
        Square p = pop_lsb(our_pawns);
        Bitboard pawn_attacks = attacks_bb(make_piece(perspective, PAWN), p, pos.pieces());
        Bitboard protected_pawns = our_pawns & pawn_attacks;
        while (protected_pawns) {
            Square protected_sq = pop_lsb(protected_pawns);
            IndexType idx = make_index(COORD_PAWN_CHAIN, p, protected_sq, perspective);
            if (active.size() < MaxActiveDimensions) active.push_back(idx);
        }
    }
    
    // 5. Rook on Open File
    Bitboard all_pawns = pos.pieces(PAWN);
    while (our_rooks) {
        Square r = pop_lsb(our_rooks);
        File f = file_of(r);
        if (!(all_pawns & file_bb(f))) {
            IndexType idx = make_index(COORD_ROOK_OPEN_FILE, r, r, perspective);
            if (active.size() < MaxActiveDimensions) active.push_back(idx);
        }
    }
    
    // 6. Bishop Pair
    if (popcount(pos.pieces(us, BISHOP)) >= 2) {
        IndexType idx = make_index(COORD_BISHOP_PAIR, SQ_A1, SQ_A1, perspective);
        if (active.size() < MaxActiveDimensions) active.push_back(idx);
    }
    
    // 7. Knight Pair
    if (popcount(pos.pieces(us, KNIGHT)) >= 2) {
        IndexType idx = make_index(COORD_KNIGHT_PAIR, SQ_A1, SQ_A1, perspective);
        if (active.size() < MaxActiveDimensions) active.push_back(idx);
    }
    
    // 8. Queen + Knight Coordination (near enemy king)
    Bitboard enemy_king_zone = attacks_bb<KING>(pos.square<KING>(~perspective)) | square_bb(pos.square<KING>(~perspective));
    while (our_queens) {
        Square q = pop_lsb(our_queens);
        while (our_knights) {
            Square n = pop_lsb(our_knights);
            Bitboard q_attacks = attacks_bb(make_piece(perspective, QUEEN), q, pos.pieces());
            Bitboard n_attacks = attacks_bb(make_piece(perspective, KNIGHT), n, pos.pieces());
            if (popcount(q_attacks & n_attacks & enemy_king_zone) >= 2) {
                IndexType idx = make_index(COORD_QUEEN_KNIGHT, q, n, perspective);
                if (active.size() < MaxActiveDimensions) active.push_back(idx);
            }
        }
    }
    
    // 9. Connected Rooks
    Bitboard our_rooks2 = pos.pieces(us, ROOK);
    while (our_rooks2) {
        Square r1 = pop_lsb(our_rooks2);
        Bitboard our_rooks3 = our_rooks2;
        while (our_rooks3) {
            Square r2 = pop_lsb(our_rooks3);
            if ((same_file(r1, r2) || same_rank(r1, r2))) {
                Bitboard between = between_bb(r1, r2);
                if (!(all_pieces & between)) {
                    IndexType idx = make_index(COORD_ROOK_ROOK, r1, r2, perspective);
                    if (active.size() < MaxActiveDimensions) active.push_back(idx);
                }
            }
        }
    }
}

void PieceRelative::append_changed_indices(Color perspective, Square ksq,
                                           const DiffType& diff, IndexList& removed, IndexList& added) {
    // INFU: Incremental piece coordination feature updates
    // Track outposts, pawn chains, rook batteries, rook open files
    
    if (diff.type() == DirtyPiece::Moved) {
        const Piece pc = diff.moved_piece();
        Square from = diff.from_sq();
        Square to = diff.to_sq();
        
        // 1. Outposts: if knight moves, remove old, add new
        if (type_of(pc) == KNIGHT) {
            IndexType old_idx = make_index(COORD_OUTPOST, from, from, perspective);
            IndexType new_idx = make_index(COORD_OUTPOST, to, to, perspective);
            if (removed.size() < MaxActiveDimensions) removed.push_back(old_idx);
            if (added.size() < MaxActiveDimensions) added.push_back(new_idx);
        }
        
        // 2. Pawn chains: if pawn moves, update chains
        if (type_of(pc) == PAWN) {
            // Remove old chains involving this pawn
            Bitboard our_pawns = pos.pieces(perspective, PAWN) | square_bb(from);
            while (our_pawns) {
                Square p = pop_lsb(our_pawns);
                if (p != from) {
                    Bitboard pawn_attacks = attacks_bb(make_piece(perspective, PAWN), p, pos.pieces());
                    if (pawn_attacks & square_bb(from) || pawn_attacks & square_bb(to)) {
                        IndexType old_idx = make_index(COORD_PAWN_CHAIN, p, from, perspective);
                        IndexType new_idx = make_index(COORD_PAWN_CHAIN, p, to, perspective);
                        if (removed.size() < MaxActiveDimensions) removed.push_back(old_idx);
                        if (added.size() < MaxActiveDimensions) added.push_back(new_idx);
                    }
                }
            }
            
            // Add new chains
            our_pawns = pos.pieces(perspective, PAWN);
            while (our_pawns) {
                Square p = pop_lsb(our_pawns);
                Bitboard pawn_attacks = attacks_bb(make_piece(perspective, PAWN), p, pos.pieces());
                Bitboard protected_pawns = our_pawns & pawn_attacks;
                while (protected_pawns) {
                    Square protected_sq = pop_lsb(protected_pawns);
                    IndexType new_idx = make_index(COORD_PAWN_CHAIN, p, protected_sq, perspective);
                    if (added.size() < MaxActiveDimensions) added.push_back(new_idx);
                }
            }
        }
        
        // 3. Rook batteries: if rook moves, update
        if (type_of(pc) == ROOK) {
            Bitboard our_rooks = pos.pieces(perspective, ROOK);
            while (our_rooks) {
                Square r = pop_lsb(our_rooks);
                if (r != from) {
                    IndexType old_idx = make_index(COORD_ROOK_ROOK, from, r, perspective);
                    IndexType new_idx = make_index(COORD_ROOK_ROOK, to, r, perspective);
                    if (removed.size() < MaxActiveDimensions) removed.push_back(old_idx);
                    if (added.size() < MaxActiveDimensions) added.push_back(new_idx);
                }
            }
        }
        
        // 4. Rook open files
        if (type_of(pc) == ROOK) {
            Bitboard all_pawns = pos.pieces(PAWN);
            File f = file_of(from);
            if (!(all_pawns & file_bb(f))) {
                IndexType old_idx = make_index(COORD_ROOK_OPEN_FILE, from, from, perspective);
                if (removed.size() < MaxActiveDimensions) removed.push_back(old_idx);
            }
            f = file_of(to);
            if (!(all_pawns & file_bb(f))) {
                IndexType new_idx = make_index(COORD_ROOK_OPEN_FILE, to, to, perspective);
                if (added.size() < MaxActiveDimensions) added.push_back(new_idx);
            }
        }
    }
    else if (diff.type() == DirtyPiece::Captured) {
        const Piece pc = diff.captured_piece();
        Square sq = diff.to_sq();
        
        // Remove coordination features involving captured piece
        if (type_of(pc) == KNIGHT) {
            IndexType idx = make_index(COORD_OUTPOST, sq, sq, perspective);
            if (removed.size() < MaxActiveDimensions) removed.push_back(idx);
        }
        if (type_of(pc) == ROOK) {
            Bitboard our_rooks = pos.pieces(perspective, ROOK);
            while (our_rooks) {
                Square r = pop_lsb(our_rooks);
                if (r != sq) {
                    IndexType idx = make_index(COORD_ROOK_ROOK, sq, r, perspective);
                    if (removed.size() < MaxActiveDimensions) removed.push_back(idx);
                }
            }
            Bitboard all_pawns = pos.pieces(PAWN);
            File f = file_of(sq);
            if (!(all_pawns & file_bb(f))) {
                IndexType idx = make_index(COORD_ROOK_OPEN_FILE, sq, sq, perspective);
                if (removed.size() < MaxActiveDimensions) removed.push_back(idx);
            }
        }
        if (type_of(pc) == PAWN) {
            // Remove pawn chains involving captured pawn
            Bitboard our_pawns = pos.pieces(perspective, PAWN) | square_bb(sq);
            while (our_pawns) {
                Square p = pop_lsb(our_pawns);
                if (p != sq) {
                    Bitboard pawn_attacks = attacks_bb(make_piece(perspective, PAWN), p, pos.pieces() | square_bb(sq));
                    if (pawn_attacks & square_bb(sq)) {
                        IndexType idx = make_index(COORD_PAWN_CHAIN, p, sq, perspective);
                        if (removed.size() < MaxActiveDimensions) removed.push_back(idx);
                    }
                }
            }
        }
    }
    else if (diff.type() == DirtyPiece::Promoted) {
        Square to = diff.to_sq();
        const Piece pc = diff.moved_piece();
        
        // Remove pawn features
        IndexType old_idx = make_index(COORD_PAWN_CHAIN, to, to, perspective);
        if (removed.size() < MaxActiveDimensions) removed.push_back(old_idx);
        
        // Add promoted piece features (handled by Moved case on next call)
    }
}

bool PieceRelative::requires_refresh(const DiffType& diff, Color perspective) {
    // Coordination features change on piece moves
    return diff.type() == DirtyPiece::Moved || diff.type() == DirtyPiece::Captured ||
           diff.type() == DirtyPiece::Promoted;
}

}  // namespace Gambo::NNUE::Features