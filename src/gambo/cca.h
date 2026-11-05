/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// CCA — Counterfactual Candidate Analysis
// Examines robustness of root candidates against defensive responses

#ifndef GAMBO_CCA_H_INCLUDED
#define GAMBO_CCA_H_INCLUDED

#include <array>
#include <cstdint>
#include <vector>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::CCA {

using namespace Stockfish;

struct CandidateAnalysis {
    Move move;
    Value score;
    Value worst_case_score;
    int refutation_depth;
    int defensive_resources;
    bool is_fragile;
    int stability_score;  // 0-100

    CandidateAnalysis() : score(VALUE_NONE), worst_case_score(VALUE_NONE),
                         refutation_depth(0), defensive_resources(0),
                         is_fragile(false), stability_score(0) {}
};

struct CCAConfig {
    int max_candidates = 4;
    int search_depth = 12;
    int max_nodes_per_candidate = 10000;
    int fragility_threshold = 100;  // centipawns
};

struct CCAStats {
    int candidates_analyzed = 0;
    int fragile_candidates = 0;
    int refutations_found = 0;
};

class CounterfactualAnalyzer {
public:
    CounterfactualAnalyzer() = default;

    // Analyze root candidates
    std::vector<CandidateAnalysis> analyze_candidates(const Position& pos, Search::Stack* ss,
                                                     const std::vector<Move>& candidates, int depth);

    // Configuration
    void set_config(const CCAConfig& cfg) { config = cfg; }
    const CCAConfig& get_config() const { return config; }

    // Statistics
    const CCAStats& get_stats() const { return stats; }
    void reset_stats() { stats = {}; }

private:
    CCAConfig config;
    CCAStats stats;

    CandidateAnalysis analyze_single_candidate(const Position& pos, Search::Stack* ss,
                                              Move candidate, int depth);
    int find_refutation(const Position& pos, Search::Stack* ss, Move candidate, int depth);
    int count_defensive_resources(const Position& pos, Search::Stack* ss);
};

}  // namespace Gambo::CCA

#endif  // GAMBO_CCA_H_INCLUDED