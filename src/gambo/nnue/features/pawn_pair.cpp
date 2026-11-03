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

// Pawn-pair feature system implementation

#include "pawn_pair.h"

#include "../../../position.h"
#include "../nnue_common.h"

namespace Gambo::NNUE::Features {

// PawnPair feature index computation
// Index formula: (king_bucket * 1024) + (pawn1_relative * 32) + (pawn2_relative * 4) + pair_type
// Where king_bucket maps king position to 0-31 buckets (same as HalfKAv2_hm)
// pawn_relative is the pawn square relative to king (0-31 after folding)
// pair_type is 0-7 (8 types)

IndexType PawnPair::make_index(Color perspective, Square king_sq,
                               Square pawn1, Square pawn2, PawnPairType type) {
    // King bucket (0-31)
    const IndexType king_bucket = KingBuckets[king_sq];
    
    // Relative pawn squares (0-31 after king-relative folding)
    const Square pawn1_rel = orient(perspective, pawn1);
    const Square pawn2_rel = orient(perspective, pawn2);
    
    const IndexType p1 = static_cast<IndexType>(pawn1_rel) & 0x1Fu;  // 0-31
    const IndexType p2 = static_cast<IndexType>(pawn2_rel) & 0x1Fu;  // 0-31
    
    // Order pawns for canonical representation (p1 <= p2)
    const IndexType p_low = std::min(p1, p2);
    const IndexType p_high = std::max(p1, p2);
    
    const IndexType pair_idx = static_cast<IndexType>(type);
    
    // Index = king_bucket * 1024 + p_low * 32 + p_high * 4 + pair_type
    // 32 * 32 * 4 = 4096 per king bucket, 32 buckets = 131072
    // But we use 8192 dimensions, so: 8192 / 32 = 256 per bucket
    // 256 = 8 * 8 * 4 (p_low * 8 + p_high * 4 + type)
    // Wait: we need to fit 8192 total
    // 32 king buckets * 256 = 8192
    // 256 = 8 pawn1 * 8 pawn2 * 4 types? No...
    // Let's use: 8*8*4 = 256 per king bucket
    // p_low in 0-7, p_high in 0-7, type in 0-3? No, we have 8 types.
    
    // Actually let's use a simpler mapping:
    // Dimensions = 8192 = 32 king_buckets * 256
    // 256 = 32 pawn1 * 8 types (pawn2 is implicit from relationship?)
    // Or: 16 pawn1 * 16 pawn2 = 256 (but we only have 32 squares after folding)
    
    // Simpler: use the same indexing as HalfKAv2_hm but for pawn pairs
    // 32 king buckets * (32*32/2) * 8 / 2 = 32 * 4096 = too big
    
    // Let's use a practical mapping:
    // 8192 = 32 king_buckets * 256
    // 256 = 32 pawn1 positions * 8 pair_types (pawn2 implicit from type?)
    
    // Better: use a hash-like approach that fits 8192
    const IndexType pawn1_idx = p1 & 31;
    const IndexType pair_type_idx = static_cast<IndexType>(type) & 7;
    
    // Simple hash: (king_bucket * 256) + (pawn1_idx * 8) + pair_type
    return king_bucket + (pawn1_idx * 8) + pair_type_idx;
}

// King bucket mapping (same as HalfKAv2_hm)
#define B(v) (v * 32)
const IndexType PawnPair::KingBuckets[SQUARE_NB] = {
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

// Orient square according to perspective
Square PawnPair::orient(Color perspective, Square s) {
    return (perspective == WHITE) ? s : Square(63 - s);
}

// Detect pawn-pair relationship type
PawnPairType PawnPair::detect_pair_type(Color perspective, const Position& pos,
                                        Square pawn1, Square pawn2) {
    if (is_doubled(pawn1, pawn2)) return PawnPairType::DOUBLED;
    if (is_connected(perspective, pawn1, pawn2)) return PawnPairType::CONNECTED;
    if (is_phalanx(perspective, pawn1, pawn2)) return PawnPairType::PHALANX;
    if (is_isolated(perspective, pos, pawn1) && is_isolated(perspective, pos, pawn2))
        return PawnPairType::ISOLATED;
    if (is_backward_pawn(perspective, pos, pawn1) && is_backward_pawn(perspective, pos, pawn2))
        return PawnPairType::BACKWARD;
    if (is_passed(perspective, pos, pawn1) && is_passed(perspective, pos, pawn2))
        return PawnPairType::PASSED;
    if (is_candidate(perspective, pos, pawn1) && is_candidate(perspective, pos, pawn2))
        return PawnPairType::CANDIDATE;
    if (is_protected(perspective, pos, pawn1) && is_protected(perspective, pos, pawn2))
        return PawnPairType::PROTECTED;
    
    // Default: use first matching type in priority order
    if (is_doubled(pawn1, pawn2)) return PawnPairType::DOUBLED;
    if (is_connected(perspective, pawn1, pawn2)) return PawnPairType::CONNECTED;
    if (is_phalanx(perspective, pawn1, pawn2)) return PawnPairType::PHALANX;
    if (is_isolated(perspective, pos, pawn1)) return PawnPairType::ISOLATED;
    if (is_backward_pawn(perspective, pos, pawn1)) return PawnPairType::BACKWARD;
    if (is_passed(perspective, pos, pawn1)) return PawnPairType::PASSED;
    if (is_candidate(perspective, pos, pawn1)) return PawnPairType::CANDIDATE;
    if (is_protected(perspective, pos, pawn1)) return PawnPairType::PROTECTED;
    if (is_phalanx(perspective, pawn1, pawn2)) return PawnPairType::PHALANX;
    
    return PawnPairType::CONNECTED;  // Default
}

// Helper functions
bool PawnPair::adjacent_files(Square a, Square b) {
    return std::abs(int(file_of(a)) - int(file_of(b))) == 1;
}

bool PawnPair::is_behind(Color perspective, Square a, Square b) {
    int rank_a = rank_of(a);
    int rank_b = rank_of(b);
    return (perspective == WHITE) ? (rank_b < rank_a) : (rank_b > rank_a);
}

bool PawnPair::is_passed(Color perspective, const Position& pos, Square sq) {
    const Color us = perspective;
    const Color them = ~perspective;
    const File f = file_of(sq);
    const Rank r = rank_of(sq);
    
    // Check squares ahead on same and adjacent files
    for (int df = -1; df <= 1; ++df) {
        File adj_f = File(int(f) + df);
        if (adj_f < FILE_A || adj_f > FILE_H) continue;
        
        for (int dr = 1; dr < 8; ++dr) {
            Rank adj_r = Rank((perspective == WHITE) ? (r + dr) : (r - dr));
            if (adj_r < RANK_1 || adj_r > RANK_8) break;
            
            Square ahead = make_square(adj_f, adj_r);
            if (pos.piece_on(ahead) == make_piece(them, PAWN)) {
                return false;  // Enemy pawn blocks
            }
        }
    }
    return true;
}

bool PawnPair::is_candidate(Color perspective, const Position& pos, Square sq) {
    const Color them = ~perspective;
    const File f = file_of(sq);
    const Rank r = rank_of(sq);
    
    // Candidate passed: blocked by exactly 1 enemy pawn
    int blockers = 0;
    for (int df = -1; df <= 1; ++df) {
        File adj_f = File(int(f) + df);
        if (adj_f < FILE_A || adj_f > FILE_H) continue;
        
        for (int dr = 1; dr < 8; ++dr) {
            Rank adj_r = Rank((perspective == WHITE) ? (r + dr) : (r - dr));
            if (adj_r < RANK_1 || adj_r > RANK_8) break;
            
            Square ahead = make_square(File(int(f) + df), adj_r);
            if (pos.piece_on(ahead) == make_piece(them, PAWN)) {
                ++blockers;
            }
        }
    }
    return blockers == 1;
}

bool PawnPair::is_protected(Color perspective, const Position& pos, Square sq) {
    const Color us = perspective;
    const File f = file_of(sq);
    const Rank r = rank_of(sq);
    
    // Protected by friendly pawn from behind
    if (perspective == WHITE) {
        if (r > RANK_1) {
            if (f > FILE_A && pos.piece_on(make_square(File(int(f)-1), Rank(int(r)-1))) == make_piece(us, PAWN))
                return true;
            if (f < FILE_H && pos.piece_on(make_square(File(int(f)+1), Rank(int(r)-1))) == make_piece(us, PAWN))
                return true;
        }
    } else {
        if (r < RANK_8) {
            if (f > FILE_A && pos.piece_on(make_square(File(int(f)-1), Rank(int(r)+1))) == make_piece(us, PAWN))
                return true;
            if (f < FILE_H && pos.piece_on(make_square(File(int(f)+1), Rank(int(r)+1))) == make_piece(us, PAWN))
                return true;
        }
    }
    return false;
}

bool PawnPair::is_phalanx(Color perspective, Square a, Square b) {
    return rank_of(a) == rank_of(b) && adjacent_files(a, b);
}

bool PawnPair::is_connected(Color perspective, Square a, Square b) {
    return adjacent_files(a, b) && std::abs(int(rank_of(a)) - int(rank_of(b))) <= 1;
}

bool PawnPair::is_doubled(Square a, Square b) {
    return file_of(a) == file_of(b);
}

bool PawnPair::is_isolated(Color perspective, const Position& pos, Square sq) {
    const Color us = perspective;
    const File f = file_of(sq);
    
    // Check adjacent files for friendly pawns
    if (f > FILE_A) {
        File adj_f = File(int(f) - 1);
        for (Rank r = RANK_1; r <= RANK_8; ++r) {
            if (pos.piece_on(make_square(adj_f, r)) == make_piece(us, PAWN))
                return false;
        }
    }
    if (f < FILE_H) {
        File adj_f = File(int(f) + 1);
        for (Rank r = RANK_1; r <= RANK_8; ++r) {
            if (pos.piece_on(make_square(adj_f, r)) == make_piece(us, PAWN))
                return false;
        }
    }
    return true;
}

bool PawnPair::is_backward_pawn(Color perspective, const Position& pos, Square sq) {
    // Pawn that cannot be protected by adjacent pawns and is behind them
    if (is_protected(perspective, pos, sq)) return false;
    
    const Color us = perspective;
    const File f = file_of(sq);
    const Rank r = rank_of(sq);
    
    // Check if behind adjacent friendly pawns
    if (perspective == WHITE) {
        if (r < RANK_4) return false;  // Not far enough advanced
        
        bool has_advanced_adjacent = false;
        if (f > FILE_A) {
            for (int ri = int(r) + 1; ri <= int(RANK_8); ++ri) {
                Rank r2 = Rank(ri);
                if (pos.piece_on(make_square(File(int(f)-1), r2)) == make_piece(WHITE, PAWN)) {
                    has_advanced_adjacent = true;
                    break;
                }
            }
        }
        if (f < FILE_H) {
            for (int ri = int(r) + 1; ri <= int(RANK_8); ++ri) {
                Rank r2 = Rank(ri);
                if (pos.piece_on(make_square(File(int(f)+1), r2)) == make_piece(WHITE, PAWN)) {
                    has_advanced_adjacent = true;
                    break;
                }
            }
        }
        return has_advanced_adjacent;
    } else {
        if (r > RANK_5) return false;
        
        bool has_advanced_adjacent = false;
        if (f > FILE_A) {
            for (int ri = int(r) - 1; ri >= int(RANK_1); --ri) {
                Rank r2 = Rank(ri);
                if (pos.piece_on(make_square(File(int(f)-1), r2)) == make_piece(BLACK, PAWN)) {
                    has_advanced_adjacent = true;
                    break;
                }
            }
        }
        if (f < FILE_H) {
            for (int ri = int(r) - 1; ri >= int(RANK_1); --ri) {
                Rank r2 = Rank(ri);
                if (pos.piece_on(make_square(File(int(f)+1), r2)) == make_piece(BLACK, PAWN)) {
                    has_advanced_adjacent = true;
                    break;
                }
            }
        }
        return has_advanced_adjacent;
    }
}

// Populate active feature indices for current position
void PawnPair::append_active_indices(Color perspective, const Position& pos, IndexList& active) {
    const Color us = perspective;
    const Square ksq = pos.square<KING>(us);
    
    // Collect all our pawns
    Bitboard our_pawns = pos.pieces(us, PAWN);
    std::array<Square, 16> pawn_squares;
    int pawn_count = 0;
    
    while (our_pawns) {
        pawn_squares[pawn_count++] = pop_lsb(our_pawns);
    }
    
    // Generate all unique pawn pairs
    for (int i = 0; i < pawn_count; ++i) {
        for (int j = i + 1; j < pawn_count; ++j) {
            Square p1 = pawn_squares[i];
            Square p2 = pawn_squares[j];
            
            PawnPairType type = detect_pair_type(perspective, pos, p1, p2);
            IndexType idx = make_index(perspective, pos.square<KING>(us), p1, p2, type);
            
            if (active.size() < MaxActiveDimensions) {
                active.push_back(idx);
            }
        }
    }
}

// Populate changed feature indices after a move
void PawnPair::append_changed_indices(Color perspective, Square ksq,
                                      const DiffType& diff, IndexList& removed, IndexList& added) {
    // INFU: Incremental NNUE Feature Update for pawn pairs
    // Track pawn moves, captures, promotions, en passant
    
    if (diff.type() == DirtyPiece::Moved) {
        const Piece pc = diff.moved_piece();
        if (type_of(pc) == PAWN) {
            Square from = diff.from_sq();
            Square to = diff.to_sq();
            
            // Remove old pawn-pair features involving the moved pawn at 'from'
            Bitboard other_pawns = pos.pieces(perspective, PAWN) & ~square_bb(to);
            while (other_pawns) {
                Square other = pop_lsb(other_pawns);
                PawnPairType type = detect_pair_type(perspective, pos, from, other);
                IndexType idx = make_index(perspective, ksq, from, other, type);
                if (removed.size() < MaxActiveDimensions) removed.push_back(idx);
            }
            
            // Add new pawn-pair features involving the moved pawn at 'to'
            other_pawns = pos.pieces(perspective, PAWN) & ~square_bb(to);
            while (other_pawns) {
                Square other = pop_lsb(other_pawns);
                PawnPairType type = detect_pair_type(perspective, pos, to, other);
                IndexType idx = make_index(perspective, ksq, to, other, type);
                if (added.size() < MaxActiveDimensions) added.push_back(idx);
            }
            return;
        }
    }
    
    if (diff.type() == DirtyPiece::Captured) {
        const Piece pc = diff.captured_piece();
        if (type_of(pc) == PAWN) {
            Square sq = diff.to_sq();
            // Remove all pawn-pair features involving the captured pawn
            Bitboard other_pawns = pos.pieces(perspective, PAWN) | square_bb(sq);
            while (other_pawns) {
                Square other = pop_lsb(other_pawns);
                if (other != sq) {
                    PawnPairType type = detect_pair_type(perspective, pos, sq, other);
                    IndexType idx = make_index(perspective, ksq, sq, other, type);
                    if (removed.size() < MaxActiveDimensions) removed.push_back(idx);
                }
            }
            return;
        }
    }
    
    if (diff.type() == DirtyPiece::Promoted) {
        Square to = diff.to_sq();
        // Remove pawn features, add promoted piece features (handled by PieceRelative)
        Bitboard other_pawns = pos.pieces(perspective, PAWN);
        while (other_pawns) {
            Square other = pop_lsb(other_pawns);
            PawnPairType type = detect_pair_type(perspective, pos, to, other);
            IndexType idx = make_index(perspective, ksq, to, other, type);
            if (removed.size() < MaxActiveDimensions) removed.push_back(idx);
        }
        return;
    }
    
    if (diff.type() == DirtyPiece::EnPassant) {
        Square ep_sq = diff.to_sq();
        Square captured_sq = (perspective == WHITE) ? Square(ep_sq - 8) : Square(ep_sq + 8);
        
        // Remove captured pawn pairs
        Bitboard other_pawns = pos.pieces(perspective, PAWN) | square_bb(captured_sq);
        while (other_pawns) {
            Square other = pop_lsb(other_pawns);
            if (other != captured_sq) {
                PawnPairType type = detect_pair_type(perspective, pos, captured_sq, other);
                IndexType idx = make_index(perspective, ksq, captured_sq, other, type);
                if (removed.size() < MaxActiveDimensions) removed.push_back(idx);
            }
        }
        
        // Add new pawn pairs with moved pawn
        other_pawns = pos.pieces(perspective, PAWN);
        while (other_pawns) {
            Square other = pop_lsb(other_pawns);
            PawnPairType type = detect_pair_type(perspective, pos, ep_sq, other);
            IndexType idx = make_index(perspective, ksq, ep_sq, other, type);
            if (added.size() < MaxActiveDimensions) added.push_back(idx);
        }
        return;
    }
}

bool PawnPair::requires_refresh(const DiffType& diff, Color perspective) {
    // Full refresh needed for pawn moves, promotions, captures
    if (diff.type() == DirtyPiece::Moved) {
        const Piece pc = diff.moved_piece();
        if (type_of(pc) == PAWN) return true;
    }
    if (diff.type() == DirtyPiece::Promoted) return true;
    if (diff.type() == DirtyPiece::Captured) {
        const Piece pc = diff.captured_piece();
        if (type_of(pc) == PAWN) return true;
    }
    if (diff.type() == DirtyPiece::EnPassant) return true;
    return false;
}

}  // namespace Gambo::NNUE::Features