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

// RCRA - Root Candidate Robustness Analysis
// Root-level candidate reliability analysis

#ifndef GAMBO_RCRA_H_INCLUDED
#define GAMBO_RCRA_H_INCLUDED

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

namespace RCRA {

// Root candidate robustness result
struct CandidateRobustness {
    Move move = Move::none();
    Value score = 0;
    int robustness = 0;           // 0-100
    int reliability = 0;          // 0-100
    int stability = 0;            // 0-100, score consistency across depths
    
    // Multi-depth analysis
    std::array<Value, 8> scoresAtDepth = {};  // Scores at depths 1-8
    std::array<int, 8> bestMoveAtDepth = {};  // Best move at each depth
    
    // Robustness factors
    bool scoreStable = false;         // Score doesn't fluctuate much
    bool moveStable = false;          // Same move recommended at multiple depths
    bool hasRefutation = false;       // Found refutation
    int refutationDepth = 0;
    Move refutationMove = Move::none();
    
    // Time stability
    bool timeStable = false;          // Score stable over time
    int timeSpentMs = 0;
    
    int overallRobustness() const {
        return (robustness + reliability + stability) / 3;
    }
};

// Root candidate robustness analyzer
struct RootAnalyzer {
    struct Config {
        int maxDepth = 8;
        int timeBudgetMs = 1000;
        bool enabled = true;
    };
    
    static Config config;
    
    // Analyze all root candidates
    static std::vector<CandidateRobustness> analyze_root_candidates(
        const Position& pos, Stack* ss, Depth depth, Value alpha, Value beta,
        const RootMoves& rootMoves);
    
    // Deep single-candidate analysis
    static CandidateRobustness analyze_candidate(
        const Position& pos, Stack* ss, Move move, Depth depth, Value alpha, Value beta);
    
    // Multi-depth score stability check
    static bool check_score_stability(const std::array<Value, 8>& scores);
    
    // Move consistency check
    static bool check_move_consistency(const std::array<int, 8>& moves);
    
    // Refutation search
    static bool search_refutation(const Position& pos, Stack* ss, Move move, 
                                  Depth depth, Value alpha, Value beta);
    
    // Time stability check
    static bool check_time_stability(const Position& pos, Stack* ss, Move move, int timeBudgetMs);
    
    // Get statistics
    struct Stats {
        std::uint64_t totalAnalyzed = 0;
        std::uint64_t robustCandidates = 0;
        std::uint64_t stableCandidates = 0;
        std::uint64_t refutationsFound = 0;
        int avgAnalysisTimeMs = 0;
    };
    
    static const Stats& get_stats();
    
    static void init_rcra();
};

}  // namespace RCRA
}  // namespace Stockfish

#endif  // GAMBO_RCRA_H_INCLUDED