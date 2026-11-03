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

#include "piece_coordination.h"

#include "../../bitboard.h"
#include "../../position.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish::Eval::NNUE::Features {

// Index of a feature for a given coordination type and squares
IndexType PieceCoordination::make_index(CoordType type, Square sq1, Square sq2, Color perspective) {
    const IndexType flip = 56 * perspective;
    const Square osq1 = sq1 ^ flip;
    const Square osq2 = sq2 ^ flip;
    
    // Ensure consistent ordering for symmetry
    const IndexType idx1 = osq1 < osq2 ? osq1 * SQUARE_NB + osq2 : osq2 * SQUARE_NB + osq1;
    
    return type * 2048 + idx1;
}

// Check if two squares are on same file/rank
inline bool same_file_or_rank(Square s1, Square s2) {
    return file_of(s1) == file_of(s2) || rank_of(s1) == rank_of(s2);
}

// Check if two squares are on same diagonal
inline bool same_diagonal(Square s1, Square s2) {
    return (file_of(s1) + rank_of(s1)) == (file_of(s2) + rank_of(s2))
        || (file_of(s1) - rank_of(s1)) == (file_of(s2) - rank_of(s2));
}

// Check if square is an outpost for a knight (not attackable by pawns, protected by own pawn)
inline bool is_knight_outpost(const Position& pos, Square sq, Color us) {
    const Color them = ~us;
    const Bitboard pawnAttacks = pos.attacks_by<PAWN>(them);
    if (pawnAttacks & square_bb(sq))
        return false;
    
    const Bitboard ourPawnAttacks = pos.attacks_by<PAWN>(us);
    return ourPawnAttacks & square_bb(sq);
}

// Check if file is open/semi-open for a rook
inline bool is_file_open_for_rook(const Position& pos, Square sq, Color us) {
    const File f = file_of(sq);
    const Bitboard ourPawns = pos.pieces(us, PAWN);
    const Bitboard theirPawns = pos.pieces(~us, PAWN);
    
    const Bitboard fileBB = FileBB[f];
    return !(ourPawns & fileBB) && !(theirPawns & fileBB); // Open file
}

inline bool is_file_semi_open_for_rook(const Position& pos, Square sq, Color us) {
    const File f = file_of(sq);
    const Bitboard ourPawns = pos.pieces(us, PAWN);
    return !(ourPawns & FileBB[f]); // Semi-open file (no our pawns)
}

// Get a list of indices for active features
void PieceCoordination::append_active_indices(Color perspective, const Position& pos, IndexList& active) {
    const Color us = perspective;
    const Color them = ~perspective;
    
    // Our pieces
    Bitboard ourKnights = pos.pieces(us, KNIGHT);
    Bitboard ourBishops = pos.pieces(us, BISHOP);
    Bitboard ourRooks = pos.pieces(us, ROOK);
    Bitboard ourQueens = pos.pieces(us, QUEEN);
    Bitboard ourPawns = pos.pieces(us, PAWN);
    
    // Their pieces
    Bitboard theirKnights = pos.pieces(them, KNIGHT);
    Bitboard theirBishops = pos.pieces(them, BISHOP);
    Bitboard theirRooks = pos.pieces(them, ROOK);
    Bitboard theirQueens = pos.pieces(them, QUEEN);
    Bitboard theirPawns = pos.pieces(them, PAWN);
    
    // COORD_BATTERY_ROOK: Rook + Queen on same file/rank
    for (Square r : ourRooks) {
        for (Square q : ourQueens) {
            if (same_file_or_rank(r, q)) {
                active.push_back(make_index(COORD_BATTERY_ROOK, r, q, perspective));
            }
        }
    }
    
    // COORD_BATTERY_BISHOP: Bishop + Queen on same diagonal
    for (Square b : ourBishops) {
        for (Square q : ourQueens) {
            if (same_diagonal(b, q)) {
                active.push_back(make_index(COORD_BATTERY_BISHOP, b, q, perspective));
            }
        }
    }
    
    // COORD_KNIGHT_OUTPOST: Knight on protected outpost
    for (Square n : ourKnights) {
        if (is_knight_outpost(pos, n, us)) {
            active.push_back(make_index(COORD_KNIGHT_OUTPOST, n, n, perspective));
        }
    }
    
    // COORD_PAWN_CHAIN: Pawns protecting each other
    Bitboard pawnAttacks = pos.attacks_by<PAWN>(us);
    Bitboard protectedPawns = ourPawns & pawnAttacks;
    for (Square p : protectedPawns) {
        // Find the protecting pawn
        Bitboard protectors = pos.attackers_to(p) & ourPawns;
        if (protectors) {
            Square protector = pop_lsb(protectors);
            active.push_back(make_index(COORD_PAWN_CHAIN, protector, p, perspective));
        }
    }
    
    // COORD_ROOK_OPEN_FILE: Rook on open/semi-open file
    for (Square r : ourRooks) {
        if (is_file_open_for_rook(pos, r, us)) {
            active.push_back(make_index(COORD_ROOK_OPEN_FILE, r, r, perspective));
        } else if (is_file_semi_open_for_rook(pos, r, us)) {
            active.push_back(make_index(COORD_ROOK_OPEN_FILE, r, SQ_A1 + file_of(r), perspective));
        }
    }
    
    // COORD_BISHOP_PAIR: Two bishops
    if (popcount(ourBishops) >= 2) {
        Square b1 = pop_lsb(ourBishops);
        Square b2 = pop_lsb(ourBishops);
        active.push_back(make_index(COORD_BISHOP_PAIR, b1, b2, perspective));
    }
    
    // COORD_KNIGHT_PAIR: Two knights
    if (popcount(ourKnights) >= 2) {
        Square n1 = pop_lsb(ourKnights);
        Square n2 = pop_lsb(ourKnights);
        active.push_back(make_index(COORD_KNIGHT_PAIR, n1, n2, perspective));
    }
    
    // COORD_QUEEN_KNIGHT: Queen + Knight coordination (fork potential)
    for (Square q : ourQueens) {
        for (Square n : ourKnights) {
            // Check if queen and knight attack common squares near enemy king
            Bitboard queenAttacks = attacks_bb(make_piece(us, QUEEN), q, pos.pieces());
            Bitboard knightAttacks = attacks_bb(make_piece(us, KNIGHT), n, pos.pieces());
            Square enemyKing = pos.square<KING>(them);
            Bitboard kingZone = attacks_bb<KING>(enemyKing) | square_bb(enemyKing);
            
            if ((queenAttacks & knightAttacks & kingZone) || (queenAttacks & knightAttacks).count() >= 3) {
                active.push_back(make_index(COORD_QUEEN_KNIGHT, q, n, perspective));
            }
        }
    }
    
    // COORD_ROOK_ROOK: Connected rooks (on same rank/file with no pieces between)
    if (popcount(ourRooks) >= 2) {
        Square r1 = pop_lsb(ourRooks);
        Square r2 = pop_lsb(ourRooks);
        if (same_file_or_rank(r1, r2)) {
            // Check if path is clear
            Bitboard between = squares_between_bb(r1, r2);
            if (!(pos.pieces() & between)) {
                active.push_back(make_index(COORD_ROOK_ROOK, r1, r2, perspective));
            }
        }
    }
}

// Get a list of indices for recently changed features
void PieceCoordination::append_changed_indices(
  Color perspective, Square ksq, const DiffType& diff, IndexList& removed, IndexList& added) {
    
    // For simplicity, we mark all piece coordination features as potentially changed
    // when any piece moves. A full refresh is simpler and safer for this feature set.
    // In practice, this feature set is small enough that full refresh is acceptable.
    (void)perspective;
    (void)ksq;
    (void)diff;
    (void)removed;
    (void)added;
}

// Extended version for incremental updates with feature weights
void PieceCoordination::append_changed_indices(
  Color perspective, Square ksq, const DiffType& diff, IndexList& removed, IndexList& added,
  const IndexType* pfBase, IndexType pfStride) {
  (void)pfBase;
  (void)pfStride;
  append_changed_indices(perspective, ksq, diff, removed, added);
}

// Returns whether the change stored in this DirtyPiece means
// that a full accumulator refresh is required.
bool PieceCoordination::requires_refresh(const DiffType& diff, Color perspective) {
    // Any piece move can affect coordination
    return diff.pc != NO_PIECE || diff.remove_pc != NO_PIECE || diff.add_pc != NO_PIECE;
}

}  // namespace Stockfish::Eval::NNUE::Features