/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// EDSE — Event-Driven Search Extensions implementation

#include "edse.h"

#include <algorithm>

#include "../bitboard.h"
#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::EDSE {

using namespace Stockfish;

int EventDrivenExtensions::check_events(const Position& pos, const Search::Stack* ss, Move move, int depth, int ply) {
    if (depth <= 0) return 0;

    int total_extension = 0;

    // Check each event type
    int ext = check_critical_check(pos, move);
    if (ext > 0) {
        stats.critical_checks++;
        total_extension += ext;
    }

    ext = check_singular_tactical(pos, move);
    if (ext > 0) {
        stats.singular_tactical++;
        total_extension += ext;
    }

    ext = check_major_threat(pos, ss, move);
    if (ext > 0) {
        stats.major_threats++;
        total_extension += ext;
    }

    ext = check_forced_response(pos, move);
    if (ext > 0) {
        stats.forced_responses++;
        total_extension += ext;
    }

    ext = check_tactical_discovery(pos, move);
    if (ext > 0) {
        stats.tactical_discoveries++;
        total_extension += ext;
    }

    ext = check_promotion_threat(pos, move);
    if (ext > 0) {
        stats.promotion_threats++;
        total_extension += ext;
    }

    ext = check_king_safety_crisis(pos, move);
    if (ext > 0) {
        stats.king_safety_crisis++;
        total_extension += ext;
    }

    // Cap total extensions
    total_extension = std::min(total_extension, config.max_extensions_per_node);
    stats.total_extensions += total_extension;

    return total_extension;
}

void EventDrivenExtensions::on_move_made(const Position& pos, const Search::Stack* ss, Move move, int ply) {
    // Record event for history
    if (event_count < 256) {
        EventInfo e;
        e.ply = ply;
        e.trigger_move = move;
        event_history[event_count++] = e;
    }
}

int EventDrivenExtensions::check_critical_check(const Position& pos, Move move) const {
    if (pos.gives_check(move)) {
        // Check if it's a critical check (mate threat or winning material)
        Square ksq = pos.square<KING>(pos.side_to_move());
        Bitboard checkers = pos.checkers();

        // If check gives immediate mate threat or wins queen
        if (pos.see(move) >= VALUE_QUEEN / 2) {
            return config.critical_check_extension;
        }
    }
    return 0;
}

int EventDrivenExtensions::check_singular_tactical(const Position& pos, Move move) const {
    // Check if this is the only good capture/tactical move
    MoveList<GenType::LEGAL> moves(pos);
    int good_moves = 0;

    for (Move m : moves) {
        if (pos.see(m) >= 0) good_moves++;
    }

    if (good_moves == 1 && pos.see(move) > 0) {
        return config.singular_tactical_extension;
    }
    return 0;
}

int EventDrivenExtensions::check_major_threat(const Position& pos, const Search::Stack* ss, Move move) const {
    // Check if move creates a major new threat
    Square to = to_sq(move);
    Piece moved = pos.piece_on(from_sq(move));

    // Threat to enemy king
    Square their_king = pos.square<KING>(~pos.side_to_move());
    if (attacks_bb(type_of(moved), to, pos.pieces()) & square_bb(their_king)) {
        return config.major_threat_extension;
    }

    // Threat to high-value piece
    Bitboard enemy_queens = pos.pieces(~pos.side_to_move(), QUEEN);
    if (enemy_queens) {
        Square qsq = lsb(enemy_queens);
        if (attacks_bb(type_of(moved), to, pos.pieces()) & square_bb(qsq)) {
            return config.major_threat_extension;
        }
    }

    return 0;
}

int EventDrivenExtensions::check_forced_response(const Position& pos, Move move) const {
    // Check if opponent has only one legal reply
    Position next_pos = pos;
    Search::Stack dummy_ss;
    next_pos.do_move(move, &dummy_ss);

    MoveList<GenType::LEGAL> opp_moves(next_pos);
    if (opp_moves.size() == 1) {
        return config.forced_response_extension;
    }
    return 0;
}

int EventDrivenExtensions::check_tactical_discovery(const Position& pos, Move move) const {
    // Check for discovered attack
    Square from = from_sq(move);
    Square to = to_sq(move);
    Piece moved = pos.piece_on(from);

    // If a sliding piece moved away, check for discovered attacks
    if (type_of(moved) == BISHOP || type_of(moved) == ROOK || type_of(moved) == QUEEN) {
        // Simplified - would check actual discovered attacks
        return config.tactical_discovery_extension;
    }
    return 0;
}

int EventDrivenExtensions::check_promotion_threat(const Position& pos, Move move) const {
    if (type_of(move) == PROMOTION) {
        return config.promotion_threat_extension;
    }

    // Check if pawn move creates promotion threat
    Piece moved = pos.piece_on(from_sq(move));
    if (type_of(moved) == PAWN) {
        Square to = to_sq(move);
        if ((pos.side_to_move() == WHITE && relative_rank(WHITE, to) == RANK_7) ||
            (pos.side_to_move() == BLACK && relative_rank(BLACK, to) == RANK_2)) {
            return config.promotion_threat_extension;
        }
    }
    return 0;
}

int EventDrivenExtensions::check_king_safety_crisis(const Position& pos, Move move) const {
    // Check if move significantly exposes king
    Square our_king = pos.square<KING>(pos.side_to_move());
    Bitboard king_zone = attacks_bb<KING>(our_king) | square_bb(our_king);

    // Count enemy attackers to king zone
    Bitboard enemy_attackers = pos.attackers_to(our_king) & pos.pieces(~pos.side_to_move());
    int attackers = popcount(enemy_attackers);

    if (attackers >= 3) {
        return config.king_safety_extension;
    }
    return 0;
}

}  // namespace Gambo::EDSE