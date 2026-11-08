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

// CCA - Counterfactual Candidate Analysis implementation

#include "cca.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "timeman.h"

namespace Stockfish {
namespace CCA {

CCA::Config CCA::config;
struct CCAStats {
    std::uint64_t totalAnalyzed = 0;
    std::uint64_t robustCandidates = 0;
    std::uint64_t trapsFound = 0;
    std::uint64_t refutationsFound = 0;
    int avgAnalysisDepth = 0;
} ccaStats;

void CCA::init_cca() {
    config.enabled = true;
    config.maxCandidates = 5;
    config.analysisDepth = 12;
    config.responseDepth = 8;
    config.maxCounterfactuals = 10;
    ccaStats = CCAStats{};
}

std::vector<CandidateAnalysis> CandidateAnalyzer::analyze_candidates(
    const Position& pos, Stack* ss, Depth depth, Value alpha, Value beta,
    const RootMoves& rootMoves) {
    
    if (!config.enabled) return {};
    
    std::vector<CandidateAnalysis> results;
    int candidatesToAnalyze = std::min(config.maxCandidates, (int)rootMoves.size());
    
    for (int i = 0; i < candidatesToAnalyze; ++i) {
        const Move move = rootMoves[i].pv[0];
        if (!move.is_ok()) continue;
        
        CandidateAnalysis analysis = analyze_candidate(pos, ss, move, depth, alpha, beta);
        analysis.move = move;
        analysis.score = rootMoves[i].score;
        
        results.push_back(analysis);
        ccaStats.totalAnalyzed++;
        
        if (analysis.isRobust) ccaStats.robustCandidates++;
        if (analysis.hasTrap) ccaStats.trapsFound++;
        if (analysis.refutationDepth > 0) ccaStats.refutationsFound++;
    }
    
    ccaStats.avgAnalysisDepth = (ccaStats.avgAnalysisDepth * (ccaStats.totalAnalyzed - candidatesToAnalyze) + config.analysisDepth) / ccaStats.totalAnalyzed;
    
    return results;
}

CandidateAnalysis CandidateAnalyzer::analyze_candidate(
    const Position& pos, Stack* ss, Move move, Depth depth, Value alpha, Value beta) {
    
    CandidateAnalysis analysis;
    analysis.move = move;
    
    // Make the move
    StateInfo st;
    bool givesCheck = pos.gives_check(move);
    Position posAfter = pos;
    posAfter.do_move(move, st, givesCheck, {}, {}, nullptr, nullptr);
    
    // Search opponent's best response
    Move bestResponse = find_best_response(posAfter, ss, move, config.responseDepth, -beta, -alpha);
    analysis.bestResponse = bestResponse;
    
    if (bestResponse.is_ok()) {
        // Evaluate the position after opponent's best response
        StateInfo oppSt;
        bool oppGivesCheck = posAfter.gives_check(bestResponse);
        Position posAfterResponse = posAfter;
        posAfterResponse.do_move(bestResponse, oppSt, oppGivesCheck, {}, {}, nullptr, nullptr);
        
        // Evaluate from our perspective (negate since it's opponent's turn)
        Value responseScore = -posAfterResponse.psqt_eval(); // Simplified evaluation
        analysis.bestResponseScore = responseScore;
        
        // Check if this is a refutation (score significantly worse than original)
        Value scoreDelta = responseScore; // Would be compared to original score
        if (scoreDelta < -200) { // Significant score drop
            analysis.refutationDepth = config.responseDepth;
            analysis.isRobust = false;
            ccaStats.refutationsFound++;
        } else {
            analysis.isRobust = true;
            ccaStats.robustCandidates++;
        }
    } else {
        // No good response found - move is robust
        analysis.isRobust = true;
        ccaStats.robustCandidates++;
    }
    
    // Check for traps
    analysis.hasTrap = has_trap(pos, ss, move, depth);
    if (analysis.hasTrap) ccaStats.trapsFound++;
    
    // Evaluate robustness
    analysis.robustness = evaluate_robustness(analysis);
    analysis.riskLevel = 100 - analysis.robustness;
    
    // Generate counterfactuals
    analysis.counterfactuals = generate_counterfactuals(pos, ss, move, depth);
    
    return analysis;
}

Move CandidateAnalyzer::find_best_response(const Position& pos, Stack* ss, Move ourMove, 
                                           Depth depth, Value alpha, Value beta) {
    Move bestResponse = Move::none();
    Value bestScore = -VALUE_INFINITE;
    
    MoveList<GenType::LEGAL> moves(pos);
    for (Move m : moves) {
        StateInfo st;
        bool givesCheck = pos.gives_check(m);
        Position testPos = pos;
        testPos.do_move(m, st, givesCheck, {}, {}, nullptr, nullptr);
        
        // Simple evaluation
        Value score = -testPos.psqt_eval(); // From our perspective
        
        if (score > bestScore) {
            bestScore = score;
            bestResponse = m;
            
            // Beta cutoff
            if (score >= beta) break;
        }
    }
    
    return bestResponse;
}

std::vector<CandidateAnalysis::Counterfactual> CandidateAnalyzer::generate_counterfactuals(
    const Position& pos, Stack* ss, Move ourMove, Depth depth) {
    
    std::vector<Counterfactual> counterfactuals;
    
    Position posAfter = pos;
    StateInfo st;
    bool givesCheck = pos.gives_check(ourMove);
    posAfter.do_move(ourMove, st, givesCheck, {}, {}, nullptr, nullptr);
    
    MoveList<GenType::LEGAL> moves(posAfter);
    int count = 0;
    
    for (Move m : moves) {
        if (count >= config.maxCounterfactuals) break;
        
        Counterfactual cf;
        cf.opponentMove = m;
        
        StateInfo st;
        bool givesCheck = posAfter.gives_check(m);
        Position testPos = posAfter;
        testPos.do_move(m, st, givesCheck, {}, {}, nullptr, nullptr);
        
        cf.resultingScore = -testPos.psqt_eval();
        cf.isRefutation = cf.resultingScore < -200; // Simplified
        
        // Generate description
        if (cf.isRefutation) {
            cf.description = "Refutation: " + m.to_string();
        } else if (posAfter.gives_check(m)) {
            cf.description = "Checking move: " + m.to_string();
        } else if (posAfter.capture_stage(m)) {
            cf.description = "Capture: " + m.to_string();
        } else {
            cf.description = "Alternative: " + m.to_string();
        }
        
        counterfactuals.push_back(cf);
        count++;
    }
    
    return counterfactuals;
}

int CandidateAnalyzer::evaluate_robustness(const CandidateAnalysis& analysis) {
    int robustness = 100;
    
    // Reduce for refutation
    if (analysis.refutationDepth > 0) {
        robustness -= 50;
        robustness -= std::min(30, analysis.refutationDepth * 3);
    }
    
    // Reduce for risk
    if (analysis.riskLevel > 50) {
        robustness -= (analysis.riskLevel - 50) / 2;
    }
    
    // Increase for trap
    if (analysis.hasTrap) {
        robustness += 20;
    }
    
    // Bonus for no good opponent response
    if (analysis.bestResponse == Move::none()) {
        robustness += 30;
    }
    
    return std::clamp(robustness, 0, 100);
}

bool CandidateAnalyzer::has_trap(const Position& pos, Stack* ss, Move move, Depth depth) {
    // A trap is a move that looks bad but leads to a strong position
    // Simplified: check if move is a sacrifice that leads to mate or large advantage
    
    StateInfo st;
    bool givesCheck = pos.gives_check(move);
    Position testPos = pos;
    testPos.do_move(move, st, givesCheck, {}, {}, nullptr, nullptr);
    
    // Check for forcing sequences
    MoveList<GenType::LEGAL> moves(testPos);
    int forcingMoves = 0;
    
    for (Move m : moves) {
        if (testPos.gives_check(m) || testPos.capture_stage(m)) {
            forcingMoves++;
        }
    }
    
    // Trap if we have forcing moves and opponent has few good replies
    return forcingMoves >= 2;
}

const CCA::Stats& CandidateAnalyzer::get_stats() {
    return ccaStats;
}

}  // namespace CCA
}  // namespace Stockfish