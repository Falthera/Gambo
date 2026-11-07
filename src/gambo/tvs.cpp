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

// TVS - Tactical Verification Search implementation

#include "tvs.h"
#include "search.h"
#include "timeman.h"
#include "thread.h"

namespace Stockfish {
namespace TVS {

TVSConfig tvsConfig;
TVSStats tvsStats;

// Determine if a position warrants tactical verification
bool should_verify(const Position& pos, Depth depth, int pcScore, Move bestMove) {
    if (!tvsConfig.enabled) return false;
    
    // Verify in tactically complex positions
    const int tacticalComplexity = attack_pressure(pos, pos.side_to_move());
    
    // High tactical complexity
    if (tacticalComplexity > 12) return true;
    
    // Low depth but high score swing (potential blunder)
    if (depth < 10 && pcScore > 100) return true;
    
    // Sacrificial moves
    if (pos.piece_on(bestMove.from_sq()) != NO_PIECE && pos.capture(bestMove)) {
        int materialSacrificed = PieceValue[pos.moved_piece(bestMove)] - PieceValue[pos.piece_on(bestMove.to_sq())];
        if (materialSacrificed > PieceValue[PAWN]) return true;
    }
    
    // King in danger
    Color us = pos.side_to_move();
    if (king_danger(pos, us) > 8) return true;
    
    return false;
}

// Bounded verification search
VerificationStatus verify_move(TVSContext& ctx, Move move, Depth verificationDepth) {
    if (!tvsConfig.enabled) return VerificationStatus::INCONCLUSIVE;
    
    tvsStats.totalVerifications++;
    
    const Position& pos = *ctx.pos;
    Stack* ss = ctx.ss;
    
    // Save original bounds
    Value origAlpha = ctx.alpha;
    Value origBeta = ctx.beta;
    
    // Set verification bounds: try to prove move is good (fails low on opponent's reply)
    // Use null-window search around current evaluation
    Value staticEval = ctx.pos->psqt_eval();
    Value verifyBeta = staticEval + 50;  // Small window
    Value verifyAlpha = verifyBeta - 32;
    
    // Do move
    StateInfo st;
    bool givesCheck = ctx.pos->gives_check(move);
    ctx.pos->do_move(move, st, givesCheck, {}, {}, nullptr, nullptr);
    
    // Search opponent's replies with reduced depth
    Depth vDepth = std::min(verificationDepth, ctx.depth / 2);
    vDepth = std::max(vDepth, Depth(1));
    
    // Search with time/node budget
    auto startTime = Time::now();
    auto startNodes = Threads.nodes_searched();
    
    // Search opponent's best reply
    Value opponentScore = -VALUE_INFINITE;
    MovePicker mp(*ctx.pos, Move::none(), vDepth, nullptr, nullptr, nullptr, nullptr, nullptr, 0);
    
    while ((Move oppMove = mp.next_move()) != Move::none()) {
        if (ctx.aborted) break;
        
        // Check time budget
        int elapsedMs = int((Time::now() - startTime).count() / 1000000);
        if (elapsedMs >= tvsConfig.timeBudgetMs) {
            ctx.aborted = true;
            tvsStats.timeAborts++;
            break;
        }
        
        // Check node budget
        int usedNodes = int(Threads.nodes_searched() - startNodes);
        if (usedNodes >= tvsConfig.nodeBudget) {
            ctx.aborted = true;
            tvsStats.nodeAborts++;
            break;
        }
        
        StateInfo oppSt;
        bool oppGivesCheck = ctx.pos->gives_check(oppMove);
        ctx.pos->do_move(oppMove, oppSt, oppGivesCheck, {}, {}, nullptr, nullptr);
        
        Value score = -Search::Worker::search<NonPV>(*ctx.pos, ctx.ss + 1, -verifyBeta, -verifyAlpha, vDepth - 1, false);
        
        ctx.pos->undo_move(oppMove);
        
        if (score > opponentScore) opponentScore = score;
        if (score >= verifyBeta) break; // Opponent found refutation
    }
    
    // Undo our move
    ctx.pos->undo_move(move);
    
    // Update stats
    int elapsedMs = int((Time::now() - startTime).count() / 1000000);
    int usedNodes = int(Threads.nodes_searched() - Threads.nodes_searched()); // placeholder
    
    tvsStats.avgDepth = (tvsStats.avgDepth * tvsStats.totalVerifications + verificationDepth) / (tvsStats.totalVerifications + 1);
    tvsStats.avgTimeMs = (tvsStats.avgTimeMs * tvsStats.totalVerifications + elapsedMs) / (tvsStats.totalVerifications + 1);
    
    // Determine result
    if (ctx.aborted) {
        return VerificationStatus::INCONCLUSIVE;
    }
    
    // If opponent couldn't refute (score < verifyBeta), move is VERIFIED
    if (opponentScore < verifyBeta) {
        tvsStats.verified++;
        return VerificationStatus::VERIFIED;
    }
    
    // If opponent found move that exceeds verifyBeta, move is REFUTED
    tvsStats.refuted++;
    return VerificationStatus::REFUTED;
}

// Verify top N candidate moves
void verify_candidates(const Position& pos, Stack* ss, Depth depth, Value alpha, Value beta,
                       const Move* candidates, int numCandidates,
                       VerificationStatus* results) {
    if (!tvsConfig.enabled || numCandidates <= 0) return;
    
    int candidatesToVerify = std::min(numCandidates, tvsConfig.maxCandidates);
    
    TVSContext ctx;
    ctx.pos = &pos;
    ctx.ss = ss;
    ctx.depth = depth;
    ctx.alpha = alpha;
    ctx.beta = beta;
    ctx.timeSpentMs = 0;
    ctx.nodesUsed = 0;
    ctx.aborted = false;
    
    // Use time management to determine verification depth
    Depth verificationDepth = std::max(Depth(4), depth / 3);
    
    for (int i = 0; i < candidatesToVerify; ++i) {
        Move m = candidates[i];
        if (!m.is_ok()) {
            results[i] = VerificationStatus::UNKNOWN;
            continue;
        }
        
        // Only verify non-trivial moves
        if (pos.capture(m) || pos.gives_check(m) || pos.piece_on(m.from_sq()) != NO_PIECE) {
            results[i] = verify_move(ctx, m, verificationDepth);
        } else {
            results[i] = VerificationStatus::UNKNOWN;
        }
        
        // If we got a REFUTED result, we can update the move ordering
        if (results[i] == VerificationStatus::REFUTED) {
            // This move is likely bad, but we still return the result
        }
    }
}

// Get verification status as string
const char* status_to_string(VerificationStatus status) {
    switch (status) {
        case VerificationStatus::VERIFIED: return "VERIFIED";
        case VerificationStatus::REFUTED: return "REFUTED";
        case VerificationStatus::INCONCLUSIVE: return "INCONCLUSIVE";
        default: return "UNKNOWN";
    }
}

// Initialize TVS subsystem
void init_tvs() {
    tvsConfig.enabled = true;
    tvsConfig.maxDepth = 12;
    tvsConfig.timeBudgetMs = 50;
    tvsConfig.nodeBudget = 10000;
    tvsConfig.maxCandidates = 3;
    
    tvsStats = {};
}

}  // namespace TVS
}  // namespace Stockfish