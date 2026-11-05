/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// TVS — Tactical Verification Search implementation

#include "tvs.h"

#include <algorithm>
#include <chrono>
#include <limits>

#include "../evaluate.h"
#include "../movepick.h"
#include "../position.h"
#include "../search.h"
#include "../thread.h"
#include "../timeman.h"
#include "../types.h"

namespace Gambo::TVS {

using namespace Stockfish;

VerificationResult TacticalVerification::verify_mate_claim(const Position& pos, Search::Stack* ss,
                                                          int depth, int mate_in) {
    stats.total_verifications++;

    VerificationContext ctx;
    ctx.pos = const_cast<Position*>(&pos);
    ctx.ss = ss;
    ctx.claimed_score = mate_in > 0 ? VALUE_MATE - mate_in : -VALUE_MATE - mate_in;
    ctx.depth = std::max(depth, config.mate_verification_depth);
    ctx.ply = 0;
    ctx.budget_nodes = config.max_nodes;
    ctx.budget_time_ms = config.max_time_ms;
    ctx.start_time = time_manager.startTime;

    VerificationResult result = mate_search(ctx, mate_in);

    if (result == VerificationResult::VERIFIED) stats.verified++;
    else if (result == VerificationResult::REFUTED) stats.refuted++;
    else if (result == VerificationResult::UNKNOWN) stats.unknown++;
    else if (result == VerificationResult::TIMEOUT) stats.timeouts++;

    return result;
}

VerificationResult TacticalVerification::verify_win_claim(const Position& pos, Search::Stack* ss,
                                                         int depth, Value score) {
    stats.total_verifications++;

    VerificationContext ctx;
    ctx.pos = const_cast<Position*>(&pos);
    ctx.ss = ss;
    ctx.claimed_score = score;
    ctx.depth = std::max(depth, config.win_verification_depth);
    ctx.ply = 0;
    ctx.budget_nodes = config.max_nodes;
    ctx.budget_time_ms = config.max_time_ms;
    ctx.start_time = time_manager.startTime;

    VerificationResult result = search_verification(ctx, false);

    if (result == VerificationResult::VERIFIED) stats.verified++;
    else if (result == VerificationResult::REFUTED) stats.refuted++;
    else if (result == VerificationResult::UNKNOWN) stats.unknown++;
    else if (result == VerificationResult::TIMEOUT) stats.timeouts++;

    return result;
}

VerificationResult TacticalVerification::verify_draw_claim(const Position& pos, Search::Stack* ss,
                                                          int depth) {
    stats.total_verifications++;

    VerificationContext ctx;
    ctx.pos = const_cast<Position*>(&pos);
    ctx.ss = ss;
    ctx.claimed_score = VALUE_DRAW;
    ctx.depth = std::max(depth, config.draw_verification_depth);
    ctx.ply = 0;
    ctx.budget_nodes = config.max_nodes / 2;  // Less budget for draw
    ctx.budget_time_ms = config.max_time_ms / 2;
    ctx.start_time = time_manager.startTime;

    VerificationResult result = search_verification(ctx, false);

    if (result == VerificationResult::VERIFIED) stats.verified++;
    else if (result == VerificationResult::REFUTED) stats.refuted++;
    else if (result == VerificationResult::UNKNOWN) stats.unknown++;
    else if (result == VerificationResult::TIMEOUT) stats.timeouts++;

    return result;
}

VerificationResult TacticalVerification::verify_tactical_claim(const Position& pos, Search::Stack* ss,
                                                              int depth, Value score, Move claim_move) {
    stats.total_verifications++;

    VerificationContext ctx;
    ctx.pos = const_cast<Position*>(&pos);
    ctx.ss = ss;
    ctx.claimed_score = score;
    ctx.depth = depth;
    ctx.ply = 0;
    ctx.budget_nodes = config.max_nodes;
    ctx.budget_time_ms = config.max_time_ms;
    ctx.start_time = time_manager.startTime;

    // Try the claimed move first
    if (claim_move != Move::none()) {
        StateInfo st;
        ctx.pos->do_move(claim_move, st);
        Value reply_score = -verify_evaluate(*ctx.pos, ctx.ss + 1);
        ctx.pos->undo_move(claim_move);

        if (reply_score <= ctx.claimed_score) {
            stats.refuted++;
            return VerificationResult::REFUTED;
        }
    }

    VerificationResult result = search_verification(ctx, false);

    if (result == VerificationResult::VERIFIED) stats.verified++;
    else if (result == VerificationResult::REFUTED) stats.refuted++;
    else if (result == VerificationResult::UNKNOWN) stats.unknown++;
    else if (result == VerificationResult::TIMEOUT) stats.timeouts++;

    return result;
}

VerificationResult TacticalVerification::search_verification(VerificationContext& ctx, bool is_mate_search) {
    (void)is_mate_search;

    // Simplified verification search - in practice would call search with bounds
    // For now, do a bounded search with the claimed score as alpha

    Value alpha = ctx.claimed_score - 50;  // Small margin
    Value beta = ctx.claimed_score + 50;

    // Search with aspiration window around claimed score
    // In a real implementation, we'd call the search function here
    // For now, return UNKNOWN as placeholder
    stats.nodes_searched += 1000;  // placeholder

    if (budget_exhausted(ctx)) return VerificationResult::TIMEOUT;

    return VerificationResult::UNKNOWN;
}

VerificationResult TacticalVerification::mate_search(VerificationContext& ctx, int mate_in) {
    (void)mate_in;
    // Proof-number-like mate verification
    // Search for forced mate within mate_in plies

    if (budget_exhausted(ctx)) return VerificationResult::TIMEOUT;

    // Generate legal moves
    MoveList<GenType::LEGAL> moves(*ctx.pos);

    if (moves.size() == 0) {
        // No legal moves - check if mate
        if (ctx.pos->checkers()) {
            return VerificationResult::VERIFIED;
        }
        return VerificationResult::REFUTED;  // Stalemate
    }

    // Try each move
    for (Move m : moves) {
        if (budget_exhausted(ctx)) return VerificationResult::TIMEOUT;

        StateInfo st;
        ctx.pos->do_move(m, st);
        ctx.ply++;

        // Opponent's turn - check if they have any defense
        bool has_defense = false;
        MoveList<GenType::LEGAL> opp_moves(*ctx.pos);
        for (Move opp_m : opp_moves) {
            if (budget_exhausted(ctx)) {
                ctx.pos->undo_move(m);
                return VerificationResult::TIMEOUT;
            }

            StateInfo st2;
            ctx.pos->do_move(opp_m, st2);
            // Check if opponent avoids mate
            if (!ctx.pos->checkers()) {
                // Opponent not in check - could be defense
                has_defense = true;
            }
            ctx.pos->undo_move(opp_m);

            if (has_defense) break;
        }

        ctx.pos->undo_move(m);
        ctx.ply--;

        if (!has_defense) {
            // All opponent replies lead to mate or are illegal
            return VerificationResult::VERIFIED;
        }
    }

    return VerificationResult::REFUTED;
}

VerificationResult TacticalVerification::defense_search(VerificationContext& ctx) {
    (void)ctx;
    // Search for defensive resources
    // Placeholder - would do a more sophisticated search
    return VerificationResult::UNKNOWN;
}

bool TacticalVerification::budget_exhausted(const VerificationContext& ctx) const {
    // Check time budget
    TimePoint elapsed = time_manager.elapsed_time();
    if (elapsed >= ctx.budget_time_ms) return true;

    // Node budget would be tracked by search
    return false;
}

Value TacticalVerification::verify_evaluate(const Position& pos, Search::Stack* ss) {
    // Quick evaluation for verification
    // Use classical eval for speed
    NNUE::AccumulatorStack accum_stack;
    NNUE::AccumulatorCaches accum_caches(*threads.main_manager()->network());
    return Eval::evaluate(*threads.main_manager()->network(), pos, accum_stack, accum_caches, 0);
}

}  // namespace Gambo::TVS