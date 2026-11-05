/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// IPFC — Incremental Position Feature Cache implementation

#include "ipfc.h"

#include <algorithm>
#include <cstdint>

#include "../bitboard.h"
#include "../position.h"
#include "../types.h"
#include "iamm.h"

namespace Gambo::IPFC {

using namespace Stockfish;

// Helper functions for adjacent files
constexpr Bitboard adjacent_file_bb(Square s) {
    return (file_of(s) > FILE_A ? square_bb(Square(int(s) - 1)) : Bitboard(0))
         | (file_of(s) < FILE_H ? square_bb(Square(int(s) + 1)) : Bitboard(0));
}

constexpr Bitboard shift_up(Bitboard b) { return shift<NORTH>(b); }
constexpr Bitboard shift_down(Bitboard b) { return shift<SOUTH>(b); }

void PositionFeatureCacheManager::init(const Position& pos) {
    cache.clear();
    cache.generation = 0;
    cache.valid = false;

    attack_manager.init(pos);

    full_recalc(pos);
    cache.valid = true;
}

void PositionFeatureCacheManager::full_recalc(const Position& pos) {
    cache.generation++;
    attack_manager.full_recalc(pos);
    cache.attack_map = attack_manager.get_map();

    calc_pawn_structure(pos);
    calc_king_safety(pos);
    calc_mobility(pos);
    calc_threats(pos);

    cache.valid = true;
}

void PositionFeatureCacheManager::update_make_move(const Position& pos, Move m) {
    cache.generation++;
    attack_manager.update_make_move(pos, m);
    cache.attack_map = attack_manager.get_map();

    update_pawn_structure_make(pos, m);
    update_king_safety_make(pos, m);
    update_mobility_make(pos, m);
    update_threats_make(pos, m);
}

void PositionFeatureCacheManager::update_unmake_move(const Position& pos, Move m) {
    cache.generation++;
    attack_manager.update_unmake_move(pos, m);
    cache.attack_map = attack_manager.get_map();

    // For now, do full recalc on unmake (could be optimized)
    full_recalc(pos);
}

void PositionFeatureCacheManager::calc_pawn_structure(const Position& pos) {
    auto& ps = cache.pawn_structure;

    for (Color c : {WHITE, BLACK}) {
        Bitboard pawns = pos.pieces(c, PAWN);
        Bitboard enemy_pawns = pos.pieces(~c, PAWN);

        ps.pawn_count[c] = popcount(pawns);

        // Passed pawns
        Bitboard passed = 0;
        for (Bitboard b = pawns; b; ) {
            Square s = pop_lsb(b);
            if (c == WHITE) {
                Bitboard front = shift_up(square_bb(s));
                if (!(front & enemy_pawns)) passed |= square_bb(s);
            } else {
                Bitboard front = shift_down(square_bb(s));
                if (!(front & enemy_pawns)) passed |= square_bb(s);
            }
        }
        ps.passed_pawns[c] = passed;

        // Isolated pawns
        Bitboard isolated = 0;
        for (Bitboard b = pawns; b; ) {
            Square s = pop_lsb(b);
            Bitboard adjacent_files = adjacent_file_bb(s);
            if (!(adjacent_files & pawns)) isolated |= square_bb(s);
        }
        ps.isolated_pawns[c] = isolated;

        // Doubled pawns
        Bitboard doubled = 0;
        for (File f = FILE_A; f <= FILE_H; ++f) {
            Bitboard file_pawns = pawns & file_bb(f);
            if (popcount(file_pawns) > 1) doubled |= file_pawns;
        }
        ps.doubled_pawns[c] = doubled;

        // Backward pawns
        Bitboard backward = 0;
        for (Bitboard b = pawns; b; ) {
            Square s = pop_lsb(b);
            if (c == WHITE) {
                Bitboard rear = shift_down(square_bb(s));
                if ((rear & pawns) && !(rear & pos.pieces(c, PAWN) & adjacent_file_bb(s))) {
                    backward |= square_bb(s);
                }
            } else {
                Bitboard rear = shift_up(square_bb(s));
                if ((rear & pawns) && !(rear & pos.pieces(c, PAWN) & adjacent_file_bb(s))) {
                    backward |= square_bb(s);
                }
            }
        }
        ps.backward_pawns[c] = backward;

        // Pawn islands
        uint8_t islands = 0;
        Bitboard seen = 0;
        for (Bitboard b = pawns; b; ) {
            Square s = pop_lsb(b);
            if (seen & square_bb(s)) continue;
            islands++;
            Bitboard island = square_bb(s);
            Bitboard todo = square_bb(s);
            while (todo) {
                Square sq = pop_lsb(todo);
                Bitboard adj = adjacent_file_bb(sq) & pawns;
                todo |= adj & ~seen;
                seen |= square_bb(sq);
                island |= adj;
            }
        }
        ps.pawn_islands[c] = Bitboard(islands);

        // Candidate passed pawns
        Bitboard candidates = passed & ~pos.pieces(c, PAWN);
        ps.candidate_pawns[c] = candidates;

        // Pawn attacks
        ps.pawn_attacks[c] = (c == WHITE) ? pawn_attacks_bb<WHITE>(pawns) : pawn_attacks_bb<BLACK>(pawns);

        // Pawn structure score (simplified)
        int score = 0;
        score -= popcount(isolated) * 15;
        score -= popcount(doubled) * 10;
        score -= popcount(backward) * 12;
        score += popcount(passed) * 20;
        score -= (islands - 1) * 8;
        ps.pawn_score[c] = std::max(0, score);
    }
}

void PositionFeatureCacheManager::calc_king_safety(const Position& pos) {
    auto& ks = cache.king_safety;

    for (Color c : {WHITE, BLACK}) {
        Color them = ~c;
        Square king_sq = pos.square<KING>(c);

        ks.king_zone[c] = attacks_bb<KING>(king_sq) | square_bb(king_sq);
        ks.king_attackers[c] = pos.attackers_to(king_sq);
        ks.king_defenders[c] = pos.attackers_to(king_sq) & pos.pieces(c);

        ks.in_check[c] = pos.checkers() != 0;
        ks.checkers[c] = pos.checkers();

        // Pinned pieces
        Bitboard pinned = 0;
        for (Bitboard b = pos.pieces(c); b; ) {
            Square s = pop_lsb(b);
            if (attack_manager.is_pinned(s, c)) {
                pinned |= square_bb(s);
            }
        }
        ks.pinned_pieces[c] = pinned;

        // Pin rays
        for (Bitboard b = pos.pieces(c); b; ) {
            Square s = pop_lsb(b);
            ks.pin_rays[s] = attack_manager.pin_ray(s);
        }

        // King safety score
        int attackers = popcount(ks.king_attackers[c]);
        int defenders = popcount(ks.king_defenders[c]);
        ks.king_safety_score[c] = std::max(0, 100 - attackers * 8 + defenders * 5);
    }
}

void PositionFeatureCacheManager::calc_mobility(const Position& pos) {
    auto& mob = cache.mobility;

    for (Color c : {WHITE, BLACK}) {
        mob.knight_mobility[c] = 0;
        mob.bishop_mobility[c] = 0;
        mob.rook_mobility[c] = 0;
        mob.queen_mobility[c] = 0;
        mob.knight_targets[c] = 0;
        mob.bishop_targets[c] = 0;
        mob.rook_targets[c] = 0;
        mob.queen_targets[c] = 0;

        Bitboard occupied = pos.pieces();
        Bitboard own = pos.pieces(c);

        for (Bitboard b = pos.pieces(c, KNIGHT); b; ) {
            Square s = pop_lsb(b);
            Bitboard targets = attacks_bb<KNIGHT>(s) & ~own;
            mob.knight_targets[c] |= targets;
            mob.knight_mobility[c] += popcount(targets);
        }

        for (Bitboard b = pos.pieces(c, BISHOP); b; ) {
            Square s = pop_lsb(b);
            Bitboard targets = attacks_bb<BISHOP>(s, occupied) & ~own;
            mob.bishop_targets[c] |= targets;
            mob.bishop_mobility[c] += popcount(targets);
        }

        for (Bitboard b = pos.pieces(c, ROOK); b; ) {
            Square s = pop_lsb(b);
            Bitboard targets = attacks_bb<ROOK>(s, occupied) & ~own;
            mob.rook_targets[c] |= targets;
            mob.rook_mobility[c] += popcount(targets);
        }

        for (Bitboard b = pos.pieces(c, QUEEN); b; ) {
            Square s = pop_lsb(b);
            Bitboard targets = attacks_bb<QUEEN>(s, occupied) & ~own;
            mob.queen_targets[c] |= targets;
            mob.queen_mobility[c] += popcount(targets);
        }
    }
}

void PositionFeatureCacheManager::calc_threats(const Position& pos) {
    auto& th = cache.threats;

    for (Color c : {WHITE, BLACK}) {
        Color them = ~c;
        Bitboard own_pieces = pos.pieces(c);
        Bitboard enemy_pieces = pos.pieces(them);

        th.tactical_threats[c] = 0;
        th.hanging_pieces[c] = 0;
        th.defended_pieces[c] = 0;
        th.xray_attacks[c] = 0;

        // Hanging pieces (not defended, attacked by enemy)
        for (Bitboard b = own_pieces; b; ) {
            Square s = pop_lsb(b);
            Bitboard attackers = pos.attackers_to(s) & enemy_pieces;
            Bitboard defenders = pos.attackers_to(s) & own_pieces;
            if (attackers && !defenders) {
                th.hanging_pieces[c] |= square_bb(s);
            }
            if (defenders) {
                th.defended_pieces[c] |= square_bb(s);
            }
        }

        // Tactical threats (attacks on enemy pieces)
        for (Bitboard b = enemy_pieces; b; ) {
            Square s = pop_lsb(b);
            Bitboard attackers = pos.attackers_to(s) & own_pieces;
            if (attackers) {
                th.tactical_threats[c] |= square_bb(s);
            }
        }

        // X-ray attacks
        for (Bitboard b = pos.pieces(c, BISHOP, ROOK, QUEEN); b; ) {
            Square s = pop_lsb(b);
            PieceType pt = type_of(pos.piece_on(s));
            Bitboard attacks = attacks_bb(pt, s, pos.pieces());
            th.xray_attacks[c] |= attacks;
        }

        // Threat score
        int threat_score = 0;
        threat_score += popcount(th.tactical_threats[c]) * 10;
        threat_score += popcount(th.hanging_pieces[c]) * 25;
        threat_score += popcount(th.defended_pieces[c]) * 2;
        th.threat_score[c] = threat_score;
    }
}

void PositionFeatureCacheManager::update_pawn_structure_make(const Position& pos, Move m) {
    (void)m;
    // Simplified: recalc affected area
    // Full recalc for correctness
    calc_pawn_structure(pos);
}

void PositionFeatureCacheManager::update_pawn_structure_unmake(const Position& pos, Move m) {
    (void)m;
    calc_pawn_structure(pos);
}

void PositionFeatureCacheManager::update_king_safety_make(const Position& pos, Move m) {
    (void)m;
    calc_king_safety(pos);
}

void PositionFeatureCacheManager::update_mobility_make(const Position& pos, Move m) {
    (void)m;
    calc_mobility(pos);
}

void PositionFeatureCacheManager::update_threats_make(const Position& pos, Move m) {
    (void)m;
    calc_threats(pos);
}

}  // namespace Gambo::IPFC