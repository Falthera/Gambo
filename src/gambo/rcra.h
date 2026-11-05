/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// RCRA — Root Candidate Robustness Analysis
// Evaluates candidate reliability at root level

#ifndef GAMBO_RCRA_H_INCLUDED
#define GAMBO_RCRA_H_INCLUDED

#include <array>
#include <cstdint>
#include <vector>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::RCRA {

using namespace Stockfish;

struct RootCandidateInfo {
    Move move;
    Value score;
    Value prev_score;
    int stability;           // Score stability across iterations
    int pv_consistency;      // PV move consistency
    int tactical_vulnerability;
    int defensive_quality;
    int robustness_score;    // 0-100
    bool is_reliable;

    RootCandidateInfo() : score(VALUE_NONE), prev_score(VALUE_NONE),
                         stability(0), pv_consistency(0),
                         tactical_vulnerability(0), defensive_quality(0),
                         robustness_score(0), is_reliable(false) {}
};

struct RCRAConfig {
    int max_candidates = 5;
    int min_stability_for_reliable = 50;
    int min_pv_consistency = 3;
    int max_tactical_vulnerability = 30;
};

struct RCRAStats {
    int candidates_analyzed = 0;
    int reliable_candidates = 0;
    int unreliable_candidates = 0;
    int pv_changes = 0;
};

class RootCandidateAnalyzer {
public:
    RootCandidateAnalyzer() = default;

    // Analyze root candidates after iterative deepening
    std::vector<RootCandidateInfo> analyze_candidates(const Position& pos, Search::Stack* ss,
                                                     const std::vector<Search::RootMove>& root_moves,
                                                     int depth);

    // Called each iteration to track stability
    void on_iteration_complete(const Position& pos, const std::vector<Search::RootMove>& root_moves, int depth);

    // Configuration
    void set_config(const RCRAConfig& cfg) { config = cfg; }
    const RCRAConfig& get_config() const { return config; }

    // Statistics
    const RCRAStats& get_stats() const { return stats; }
    void reset_stats() { stats = {}; }

private:
    RCRAConfig config;
    RCRAStats stats;

    std::vector<RootCandidateInfo> prev_candidates;
    std::vector<Value> score_history[256];  // per move, per iteration

    RootCandidateInfo analyze_single_candidate(const Position& pos, Search::Stack* ss,
                                              const Search::RootMove& rm, int depth);
    int compute_stability(const Search::RootMove& rm) const;
    int compute_pv_consistency(const Search::RootMove& rm) const;
    int compute_tactical_vulnerability(const Position& pos, Search::Stack* ss, Move m) const;
    int compute_defensive_quality(const Position& pos, Search::Stack* ss, Move m) const;
};

}  // namespace Gambo::RCRA

#endif  // GAMBO_RCRA_H_INCLUDED