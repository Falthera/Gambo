/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// IAMM — Incremental Attack Map Maintenance implementation

#include "iamm.h"

#include <algorithm>
#include <cstdint>

#include "../bitboard.h"
#include "../position.h"
#include "../types.h"

namespace Gambo::IAMM {

using namespace Stockfish;

void AttackMapManager::init(const Position& pos) {
    attack_map.clear();
    for (auto& h : history) h.clear();

    // Full initial calculation
    for (Bitboard b = pos.pieces(); b; ) {
        Square s = pop_lsb(&b);
        Piece pc = pos.piece_on(s);
        Color c = color_of(pc);
        add_piece_attacks(pos, s, pc, true);
    }

    recalc_pins(pos);
    recalc_king_zones(pos);

    // Save initial state
    history[0] = attack_map;
}

void AttackMapManager::full_recalc(const Position& pos) {
    attack_map.clear();

    for (Bitboard b = pos.pieces(); b; ) {
        Square s = pop_lsb(&b);
        Piece pc = pos.piece_on(s);
        Color c = color_of(pc);
        add_piece_attacks(pos, s, pc, true);
    }

    recalc_pins(pos);
    recalc_king_zones(pos);
}

void AttackMapManager::update_make_move(const Position& pos, Move m) {
    // Save current state for undo
    // (In practice, we'd save to history[ply], but for simplicity we track changes)

    Square from = m.from_sq();
    Square to = m.to_sq();
    Piece moved_piece = pos.piece_on(from);
    Color c = color_of(moved_piece);
    PieceType pt = type_of(moved_piece);
    Piece captured = pos.piece_on(to);

    // Remove attacks from source square
    if (captured != NO_PIECE) {
        // Captured piece is removed
        add_piece_attacks(pos, to, captured, false);
    }

    // Remove moved piece from from-square
    add_piece_attacks(pos, from, moved_piece, false);

    // Add moved piece to to-square
    // Note: we need the piece after move (could be promoted)
    Piece new_piece = moved_piece;
    if (m.promotion_type()) {
        new_piece = make_piece(c, m.promotion_type());
    }
    add_piece_attacks(pos, to, new_piece, true);

    // Handle special moves
    if (pt == KING && std::abs(int(file_of(to)) - int(file_of(from))) == 2) {
        // Castling - move rook
        Square rook_from = (to > from) ? SQ_H1 : SQ_A1;
        Square rook_to = (to > from) ? SQ_F1 : SQ_D1;
        if (c == BLACK) {
            rook_from = (to > from) ? SQ_H8 : SQ_A8;
            rook_to = (to > from) ? SQ_F8 : SQ_D8;
        }
        Piece rook = make_piece(c, ROOK);
        add_piece_attacks(pos, rook_from, rook, false);
        add_piece_attacks(pos, rook_to, rook, true);
    }

    if (pt == PAWN && to == pos.ep_square()) {
        // En passant capture
        Square captured_pawn_sq = (c == WHITE) ? Square(to - 8) : Square(to + 8);
        Piece pawn = make_piece(~c, PAWN);
        add_piece_attacks(pos, captured_pawn_sq, pawn, false);
    }

    // Update pins and king zones
    recalc_pins(pos);
    recalc_king_zones(pos);
}

void AttackMapManager::update_unmake_move(const Position& pos, Move m) {
    // For full correctness, we'd restore from history
    // For now, do full recalc as fallback
    full_recalc(pos);
}

void AttackMapManager::add_piece_attacks(const Position& pos, Square sq, Piece pc, bool adding) {
    Color c = color_of(pc);
    PieceType pt = type_of(pc);

    if (pt == PAWN) {
        update_pawn_attacks(pos, sq, c, adding);
    } else if (pt == KNIGHT) {
        Bitboard attacks = attacks_bb<KNIGHT>(sq);
        if (adding) attack_map.attackers[sq] |= attacks;
        else attack_map.attackers[sq] &= ~attacks;
    } else if (pt == BISHOP || pt == ROOK || pt == QUEEN) {
        update_slider_attacks(pos, sq, sq, pt, adding);
    } else if (pt == KING) {
        update_king_attacks(pos, sq, c, adding);
    }
}

void AttackMapManager::update_pawn_attacks(const Position& pos, Square sq, Color c, bool adding) {
    Bitboard attacks = attacks_bb<PAWN>(sq, c);
    if (adding) {
        if (c == WHITE) attack_map.attacked_by_w[sq] |= attacks;
        else attack_map.attacked_by_b[sq] |= attacks;
    } else {
        if (c == WHITE) attack_map.attacked_by_w[sq] &= ~attacks;
        else attack_map.attacked_by_b[sq] &= ~attacks;
    }
}

void AttackMapManager::update_slider_attacks(const Position& pos, Square from, Square to, PieceType pt, bool adding) {
    Bitboard occupied = pos.pieces();
    Bitboard attacks = attacks_bb(pt, to, occupied);

    if (adding) {
        attack_map.attackers[to] |= attacks;
    } else {
        attack_map.attackers[to] &= ~attacks;
    }
}

void AttackMapManager::update_king_attacks(const Position& pos, Square sq, Color c, bool adding) {
    Bitboard attacks = attacks_bb<KING>(sq);
    if (adding) {
        if (c == WHITE) attack_map.white_king_zone |= attacks;
        else attack_map.black_king_zone |= attacks;
    } else {
        if (c == WHITE) attack_map.white_king_zone &= ~attacks;
        else attack_map.black_king_zone &= ~attacks;
    }
}

void AttackMapManager::recalc_pins(const Position& pos) {
    // Clear previous pin info
    for (auto& ray : attack_map.pin_rays) ray = 0;
    for (auto& pinner : attack_map.pinners) pinner = 0;

    for (Color c : {WHITE, BLACK}) {
        Square king_sq = pos.square<KING>(c);
        Color them = ~c;

        // Check slider pins from enemy pieces
        Bitboard enemy_sliders = pos.pieces(them, ROOK, BISHOP, QUEEN);
        Bitboard occupied = pos.pieces();

        for (Bitboard b = enemy_sliders; b; ) {
            Square s = pop_lsb(&b);
            Piece pc = pos.piece_on(s);
            PieceType pt = type_of(pc);

            Bitboard attacks = attacks_bb(pt, s, occupied);
            if (attacks & square_bb(king_sq)) {
                // Potential pin - check if there's exactly one piece between
                Bitboard between = BetweenBB[s][king_sq] & occupied;
                if (popcount(between) == 1) {
                    Square pinned_sq = lsb(between);
                    if (pos.piece_on(pinned_sq) != NO_PIECE && color_of(pos.piece_on(pinned_sq)) == c) {
                        attack_map.pin_rays[pinned_sq] = LineBB[s][king_sq];
                        attack_map.pinners[pinned_sq] |= square_bb(s);
                    }
                }
            }
        }
    }
}

void AttackMapManager::recalc_king_zones(const Position& pos) {
    for (Color c : {WHITE, BLACK}) {
        Square king_sq = pos.square<KING>(c);
        Bitboard zone = attacks_bb<KING>(king_sq) | square_bb(king_sq);
        Bitboard enemy_attackers = pos.attackers_to(king_sq);

        if (c == WHITE) {
            attack_map.white_king_zone = zone;
            attack_map.white_king_attackers = enemy_attackers;
        } else {
            attack_map.black_king_zone = zone;
            attack_map.black_king_attackers = enemy_attackers;
        }
    }
}

Bitboard AttackMapManager::attackers_to(Square s, Color c) const {
    Bitboard result = attack_map.attackers[s];
    // Filter by color
    if (c == WHITE) result &= pos.pieces(WHITE);
    else result &= pos.pieces(BLACK);
    return result;
}

Bitboard AttackMapManager::attacked_by(Color c) const {
    Bitboard result = 0;
    if (c == WHITE) {
        for (const auto& bb : attack_map.attacked_by_w) result |= bb;
    } else {
        for (const auto& bb : attack_map.attacked_by_b) result |= bb;
    }
    return result;
}

bool AttackMapManager::is_pinned(Square s, Color c) const {
    return attack_map.pinners[s] != 0;
}

Bitboard AttackMapManager::pin_ray(Square s) const {
    return attack_map.pin_rays[s];
}

Bitboard AttackMapManager::king_zone(Color c) const {
    return (c == WHITE) ? attack_map.white_king_zone : attack_map.black_king_zone;
}

Bitboard AttackMapManager::king_attackers(Color c) const {
    return (c == WHITE) ? attack_map.white_king_attackers : attack_map.black_king_attackers;
}

}  // namespace Gambo::IAMM