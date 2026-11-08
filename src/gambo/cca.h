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

// CCA - Counterfactual Candidate Analysis
// Root candidate robustness analysis with defensive response enumeration

#ifndef GAMBO_CCA_H_INCLUDED
#define GAMBO_CCA_H_INCLUDED

#include <array>
#include <cstdint>
#include <vector>
#include <string>

#include "../types.h"
#include "../position.h"
#include "../search.h"

namespace Stockfish {
class Position;
class Stack;

namespace CCA {

// Candidate analysis result
struct CandidateAnalysis {
    Move move = Move::none();
    Value score = 0;
    Value bestResponseScore = 0;
    Move bestResponse = Move::none();
    int robustness = 0;           // 0-100, higher = more robust
    int refutationDepth = 0;      // Depth at which refutation found
    std::vector<Move> refutationLine;
    bool isRobust = false;
    bool hasTrap = false;         // Move creates trap
    int riskLevel = 0;            // 0-100, higher = riskier
    
    // Counterfactual: "What if opponent plays X?"
    struct Counterfactual {
        Move opponentMove = Move::none();
        Value resultingScore = 0;
        bool isRefutation = false;
        std::string description;
    };
    std::vector<Counterfactual> counterfactuals;
};

// Root candidate robustness analyzer
struct CandidateAnalyzer {
    struct Config {
        int maxCandidates = 5;
        int analysisDepth = 12;
        int responseDepth = 8;
        int maxCounterfactuals = 10;
        bool enabled = true;
    };
    
    static Config config;
    
    // Analyze root candidates
    static std::vector<CandidateAnalysis> analyze_candidates(
        const Position& pos, Stack* ss, Depth depth, Value alpha, Value beta,
        const RootMoves& rootMoves);
    
    // Deep analysis of a single candidate
    static CandidateAnalysis analyze_candidate(
        const Position& pos, Stack* ss, Move move, Depth depth, Value alpha, Value beta);
    
    // Find best defensive response for opponent
    static Move find_best_response(const Position& pos, Stack* ss, Move ourMove, 
                                   Depth depth, Value alpha, Value beta);
    
    // Generate counterfactual scenarios
    static std::vector<CandidateAnalysis::Counterfactual> generate_counterfactuals(
        const Position& pos, Stack* ss, Move ourMove, Depth depth);
    
    // Evaluate candidate robustness
    static int evaluate_robustness(const CandidateAnalysis& analysis);
    
    // Check if move has tactical trap
    static bool has_trap(const Position& pos, Stack* ss, Move move, Depth depth);
    
    // Get analysis statistics
    struct Stats {
        std::uint64_t totalAnalyzed = 0;
        std::uint64_t robustCandidates = 0;
        std::uint64_t trapsFound = 0;
        std::uint64_t refutationsFound = 0;
        int avgAnalysisDepth = 0;
    };
    
    static const Stats& get_stats();
    
    static void init_cca();
};

}  // namespace CCA
}  // namespace Stockfish

#endif  // GAMBO_CCA_H_INCLUDED