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

// RCRA - Root Candidate Robustness Analysis implementation

#include "rcra.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "timeman.h"

namespace Stockfish {
namespace RCRA {

RCRA::Config RCRA::config;
struct RCRAStats {
    std::uint64_t totalAnalyzed = 0;
    std::uint64_t robustCandidates = 0;
    std::uint64_t stableCandidates = 0;
    std::uint64_t refutationsFound = 0;
    int avgAnalysisTimeMs = 0;
} rcraStats;

void RCRA::init_rcra() {
    config.enabled = true;
    config.maxDepth = 8;
    config.timeBudgetMs = 1000;
    rcraStats = RCRAStats{};
}

std::vector<CandidateRobustness> RootAnalyzer::analyze_root_candidates(
    const Position& pos, Stack* ss, Depth depth, Value alpha, Value beta,
    const RootMoves& rootMoves) {
    
    if (!config.enabled) return {};
    
    std::vector<CandidateRobustness> results;
    int candidatesToAnalyze = std::min((int)rootMoves.size(), 5);
    
    for (int i = 0; i < candidatesToAnalyze; ++i) {
        const Move move = rootMoves[i].pv[0];
        if (!move.is_ok()) continue;
        
        CandidateRobustness analysis = analyze_candidate(pos, ss, move, depth, alpha, beta);
        analysis.move = move;
        analysis.score = rootMoves[i].score;
        
        results.push_back(analysis);
        rcraStats.totalAnalyzed++;
        
        if (analysis.overallRobustness() >= 70) rcraStats.robustCandidates++;
        if (analysis.scoreStable && analysis.moveStable) rcraStats.stableCandidates++;
        if (analysis.hasRefutation) rcraStats.refutationsFound++;
    }
    
    return results;
}

CandidateRobustness RootAnalyzer::analyze_candidate(
    const Position& pos, Stack* ss, Move move, Depth depth, Value alpha, Value beta) {
    
    CandidateRobustness analysis;
    analysis.move = move;
    
    // Multi-depth analysis (depths 1-8)
    for (int d = 1; d <= config.maxDepth && d <= 8; ++d) {
        Position testPos = pos;
        StateInfo st;
        bool givesCheck = testPos.gives_check(move);
        testPos.do_move(move, st, givesCheck, {}, {}, nullptr, nullptr);
        
        // Search to depth d
        Stack localSs[64];
        Value score = -Search::Worker::search<NonPV>(testPos, localSs, -VALUE_INFINITE, VALUE_INFINITE, Depth(d), false);
        analysis.scoresAtDepth[d-1] = score;
        
        // Track best move at this depth
        MoveList<GenType::LEGAL> moves(testPos);
        if (!moves.empty()) {
            analysis.bestMoveAtDepth[d-1] = moves[0].from_sq() * 64 + moves[0].to_sq(); // Simplified
        }
        
        testPos.undo_move(move);
    }
    
    // Check score stability
    analysis.scoreStable = check_score_stability(analysis.scoresAtDepth);
    if (analysis.scoreStable) analysis.stability += 33;
    
    // Check move consistency
    analysis.moveStable = check_move_consistency(analysis.bestMoveAtDepth);
    if (analysis.moveStable) analysis.stability += 33;
    
    // Refutation search
    analysis.hasRefutation = search_refutation(pos, nullptr, move, depth, alpha, beta);
    if (analysis.hasRefutation) {
        analysis.refutationDepth = 8; // Placeholder
        analysis.refutationMove = Move::none(); // Would be set by search
        analysis.reliability = 0;
    } else {
        analysis.reliability += 50;
    }
    
    // Time stability check
    analysis.timeStable = check_time_stability(pos, nullptr, move, 100);
    if (analysis.timeStable) analysis.stability += 34;
    
    // Overall robustness
    analysis.robustness = (analysis.reliability + analysis.stability) / 2;
    
    return analysis;
}

bool RootAnalyzer::check_score_stability(const std::array<Value, 8>& scores) {
    // Check if scores are consistent across depths
    int validScores = 0;
    Value prevScore = 0;
    bool first = true;
    int maxDelta = 0;
    
    for (int i = 0; i < 8; ++i) {
        if (scores[i] != 0) {
            if (!first) {
                int delta = std::abs(scores[i] - prevScore);
                maxDelta = std::max(maxDelta, delta);
            }
            prevScore = scores[i];
            first = false;
            validScores++;
        }
    }
    
    // Stable if we have at least 4 valid scores and max delta < 50
    return validScores >= 4 && maxDelta < 50;
}

bool RootAnalyzer::check_move_consistency(const std::array<int, 8>& moves) {
    int validMoves = 0;
    int firstMove = -1;
    
    for (int i = 0; i < 8; ++i) {
        if (moves[i] != 0) {
            if (firstMove == -1) {
                firstMove = moves[i];
            } else if (moves[i] == firstMove) {
                validMoves++;
            }
        }
    }
    
    return validMoves >= 3; // Same move at 3+ depths
}

bool RootAnalyzer::search_refutation(const Position& pos, Stack* ss, Move move, 
                                     Depth depth, Value alpha, Value beta) {
    Position testPos = pos;
    StateInfo st;
    bool givesCheck = testPos.gives_check(move);
    testPos.do_move(move, st, givesCheck, {}, {}, nullptr, nullptr);
    
    // Search for opponent response that significantly worsens our position
    MoveList<GenType::LEGAL> moves(testPos);
    for (Move m : moves) {
        StateInfo oppSt;
        bool oppGivesCheck = testPos.gives_check(m);
        Position testPos2 = testPos;
        testPos2.do_move(m, oppSt, oppGivesCheck, {}, {}, nullptr, nullptr);
        
        // Evaluate from our perspective
        Value score = -testPos2.psqt_eval();
        
        if (score <= alpha - 100) { // Significant drop
            return true;
        }
    }
    
    return false;
}

bool RootAnalyzer::check_time_stability(const Position& pos, Stack* ss, Move move, int timeBudgetMs) {
    // Simplified: just check if score is stable over short time
    // In real implementation, would run search multiple times
    return true; // Placeholder
}

std::vector<CandidateRobustness> RootAnalyzer::analyze_root_candidates(
    const Position& pos, Stack* ss, Depth depth, Value alpha, Value beta,
    const RootMoves& rootMoves) {
    
    if (!config.enabled) return {};
    
    std::vector<CandidateRobustness> results;
    int candidatesToAnalyze = std::min((int)rootMoves.size(), 5);
    
    for (int i = 0; i < candidatesToAnalyze; ++i) {
        const Move move = rootMoves[i].pv[0];
        if (!move.is_ok()) continue;
        
        CandidateRobustness analysis = analyze_candidate(pos, ss, move, depth, alpha, beta);
        analysis.move = move;
        analysis.score = rootMoves[i].score;
        
        results.push_back(analysis);
        rcraStats.totalAnalyzed++;
        
        if (analysis.overallRobustness() >= 70) rcraStats.robustCandidates++;
        if (analysis.scoreStable && analysis.moveStable) rcraStats.stableCandidates++;
        if (analysis.hasRefutation) rcraStats.refutationsFound++;
    }
    
    return results;
}

bool RootAnalyzer::check_score_stability(const std::array<Value, 8>& scores) {
    // Simplified version
    return true;
}

bool RootAnalyzer::check_move_consistency(const std::array<int, 8>& moves) {
    // Simplified version
    return true;
}

bool RootAnalyzer::search_refutation(const Position& pos, Stack* ss, Move move, 
                                     Depth depth, Value alpha, Value beta) {
    return false;
}

bool RootAnalyzer::check_time_stability(const Position& pos, Stack* ss, Move move, int timeBudgetMs) {
    return true;
}

const RCRA::Stats& RootAnalyzer::get_stats() {
    return rcraStats;
}

void RCRA::init_rcra() {
    config.enabled = true;
    config.maxDepth = 8;
    config.timeBudgetMs = 1000;
    rcraStats = RCRAStats{};
}

}  // namespace RCRA
}  // namespace Stockfish