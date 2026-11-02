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

// Threat detection feature system implementation

#include "threat_detection.h"

#include "../../../position.h"
#include "../nnue_common.h"

namespace Gambo::NNUE::Features {

IndexType ThreatDetection::make_index(Color perspective, Piece attacker, Square from, Square to,
                                      Piece target, Square king_sq) {
    // Index encoding for threat features:
    // Dimensions = 60720
    // attacker: 10 piece types (no king) * 2 colors = 20
    // target: 10 piece types * 2 colors = 20
    // from: 64 squares
    // to: 64 squares
    // king_sq: 64 squares
    // But with symmetry and constraints, reduced to 60720
    
    // Simplified encoding:
    // attacker_type: 0-9 (piece type excluding king)
    // target_type: 0-9
    // from_sq: 0-63
    // to_sq: 0-63
    // king_sq: 0-63
    
    // Use king bucket for perspective
    const Color us = perspective;
    const Color them = ~perspective;
    
    const PieceType att_pt = type_of(attacker);
    const Color att_color = color_of(attacker);
    const PieceType tgt_pt = type_of(target);
    const Color tgt_color = color_of(target);
    
    // Map piece types to 0-9 (excl king)
    auto pt_to_idx = [](PieceType pt, Color c) -> int {
        int base = (c == WHITE) ? 0 : 5;
        switch (pt) {
            case PAWN:   return base + 0;
            case KNIGHT: return base + 1;
            case BISHOP: return base + 1;
            case ROOK:   return base + 2;
            case QUEEN:  return base + 3;
            default: return 0;
        }
    };
    
    int att_idx = pt_to_idx(att_pt, att_color);
    int tgt_idx = pt_to_idx(tgt_pt, tgt_color);
    
    // King bucket for perspective
    const Square king_sq_rel = orient(perspective, king_sq);
    
    // Index = (att_idx * 10 + tgt_idx) * 64 * 64 + from * 64 + to + king_bucket * 64 * 64
    // This is a simplified version - actual SFNNv16 uses more complex mapping
    
    // For now, use a simplified hash
    return ((att_idx * 10 + tgt_idx) * 64 + from) * 64 + to;
}

// Orient square according to perspective
Square ThreatDetection::orient(Color perspective, Square s) {
    return (perspective == WHITE) ? s : Square(63 - s);
}

void ThreatDetection::append_active_indices(Color perspective, const Position& pos, IndexList& active) {
    const Color us = perspective;
    const Color them = ~perspective;
    const Square ksq = pos.square<KING>(us);
    
    // Generate threat features for our pieces attacking their pieces
    // and their pieces attacking our pieces
    
    // Our pieces attacking their pieces
    for (PieceType pt = PAWN; pt <= QUEEN; ++pt) {
        Bitboard attackers = pos.pieces(us, pt);
        while (attackers) {
            Square from = pop_lsb(attackers);
            Piece attacker = make_piece(us, pt);
            
            // Generate attacks from this piece
            Bitboard attacks = pos.attacks_from(attacker, from);
            while (attacks) {
                Square to = pop_lsb(attacks);
                Piece target = pos.piece_on(to);
                if (target != NO_PIECE && color_of(target) == them) {
                    IndexType idx = make_index(perspective, make_piece(us, pt), from, to, target, pos.square<KING>(us));
                    if (active.size() < MaxActiveDimensions) {
                        active.push_back(idx);
                    }
                }
            }
        }
    }
    
    // Their pieces attacking our pieces
    for (PieceType pt = PAWN; pt <= QUEEN; ++pt) {
        Bitboard attackers = pos.pieces(them, pt);
        while (attackers) {
            Square from = pop_lsb(attackers);
            Piece attacker = make_piece(them, pt);
            
            Bitboard attacks = pos.attacks_from(attacker, from);
            while (attacks) {
                Square to = pop_lsb(attacks);
                Piece target = pos.piece_on(to);
                if (target != NO_PIECE && color_of(target) == us) {
                    IndexType idx = make_index(perspective, attacker, from, to, target, pos.square<KING>(us));
                    if (active.size() < MaxActiveDimensions) {
                        active.push_back(idx);
                    }
                }
            }
        }
    }
}

void ThreatDetection::append_changed_indices(Color perspective, Square ksq,
                                             const DiffType& diff, IndexList& removed, IndexList& added,
                                             FusedUpdateData* fd, bool first) {
    // Threat features change when pieces move or are captured
    // This is complex - simplified implementation signals refresh for piece moves
    if (diff.type() == DirtyThreats::Moved || diff.type() == DirtyThreats::Captured) {
        // Would need to track all affected threat relationships
        // For now, signal refresh needed
    }
}

bool ThreatDetection::requires_refresh(const DiffType& diff, Color perspective) {
    // Threat features change on any piece move or capture
    return diff.type() == DirtyThreats::Moved || diff.type() == DirtyThreats::Captured;
}

// Orient square according to perspective
Square ThreatDetection::orient(Color perspective, Square s) {
    return (perspective == WHITE) ? s : Square(63 - s);
}

}  // namespace Gambo::NNUE::Features