/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PTC — Persistent Threat Context implementation

#include "ptc.h"

#include <algorithm>

#include "../bitboard.h"
#include "../position.h"
#include "../types.h"

namespace Gambo::PTC {

using namespace Stockfish;

void ThreatContextManager::init(const Position& pos) {
    context.clear();
    for (auto& h : history) h.clear();
    full_recalc(pos);
    history[0] = context;
}

void ThreatContextManager::full_recalc(const Position& pos) {
    context.clear();
    context.generation++;
    context.ply = 0;
    calc_threats(pos);
    calc_obligations(pos);
    history[0] = context;
}

void ThreatContextManager::update_make_move(const Position& pos, Move m) {
    context.generation++;
    context.ply++;
    // Save to history
    if (context.ply < 256) history[context.ply] = context;

    // Simplified: full recalc
    calc_threats(pos);
    calc_obligations(pos);
}

void ThreatContextManager::update_unmake_move(const Position& pos, Move m) {
    context.ply--;
    if (context.ply >= 0 && context.ply < 256) {
        context = history[context.ply];
    } else {
        full_recalc(pos);
    }
}

void ThreatContextManager::calc_threats(const Position& pos) {
    for (Color c : {WHITE, BLACK}) {
        Color them = ~c;
        context.threats[c].clear();
        context.threatened_pieces[c] = 0;

        // Check attacks on enemy pieces
        for (Square s : pos.pieces(them)) {
            Bitboard attackers = pos.attackers_to(s) & pos.pieces(c);
            if (attackers) {
                ThreatInfo t;
                t.target_square = s;
                t.target_piece = type_of(pos.piece_on(s));
                t.attackers = attackers;
                t.defenders = pos.attackers_to(s) & pos.pieces(them);
                t.threat_level = popcount(attackers);
                t.move_count_to_execute = 1;
                t.is_forcing = pos.gives_check(Move()) || t.target_piece == KING;
                context.threats[c].push_back(t);
                context.threatened_pieces[them] |= square_bb(s);
            }
        }

        // Check defended pieces
        for (Square s : pos.pieces(c)) {
            Bitboard defenders = pos.attackers_to(s) & pos.pieces(c);
            if (defenders) {
                context.defended_pieces[c] |= square_bb(s);
            }
        }
    }
}

void ThreatContextManager::calc_obligations(const Position& pos) {
    for (Color c : {WHITE, BLACK}) {
        Color them = ~c;
        context.obligations[c].clear();

        // King in check - must respond
        Square king_sq = pos.square<KING>(c);
        if (pos.checkers()) {
            ObligationInfo o;
            o.piece_square = king_sq;
            o.obligation_type = 3;  // move king
            o.urgency = 5;
            // Find legal king moves
            Bitboard king_moves = attacks_bb<KING>(king_sq) & ~pos.pieces(c);
            for (Square to : king_moves) {
                o.forced_move = make_move(king_sq, to);
                break;  // just first for now
            }
            context.obligations[c].push_back(o);
        }

        // Pinned pieces - must stay on pin line
        for (Square s : pos.pieces(c)) {
            Bitboard pinners = pos.pinners_to(s);
            if (pinners) {
                ObligationInfo o;
                o.piece_square = s;
                o.obligation_type = 1;  // defend/hold
                o.urgency = 4;
                context.obligations[c].push_back(o);
            }
        }

        // Threatened pieces that must move or be defended
        for (const auto& t : context.threats[them]) {
            if (t.threat_level >= 2 && t.target_piece != KING) {
                ObligationInfo o;
                o.piece_square = t.target_square;
                o.obligation_type = 1;  // defend
                o.urgency = t.threat_level;
                context.obligations[c].push_back(o);
            }
        }
    }
}

bool ThreatContextManager::has_threat_on(Square s, Color c) const {
    return context.threatened_pieces[c] & square_bb(s);
}

ThreatInfo ThreatContextManager::get_threat_on(Square s, Color c) const {
    for (const auto& t : context.threats[c]) {
        if (t.target_square == s) return t;
    }
    return {};
}

bool ThreatContextManager::has_obligation_for(Square s, Color c) const {
    for (const auto& o : context.obligations[c]) {
        if (o.piece_square == s) return true;
    }
    return false;
}

ObligationInfo ThreatContextManager::get_obligation_for(Square s, Color c) const {
    for (const auto& o : context.obligations[c]) {
        if (o.piece_square == s) return o;
    }
    return {};
}

Bitboard ThreatContextManager::all_threats(Color c) const {
    Bitboard result = 0;
    for (const auto& t : context.threats[c]) {
        result |= square_bb(t.target_square);
    }
    return result;
}

Bitboard ThreatContextManager::all_obligations(Color c) const {
    Bitboard result = 0;
    for (const auto& o : context.obligations[c]) {
        result |= square_bb(o.piece_square);
    }
    return result;
}

}  // namespace Gambo::PTC