/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// CCA — Counterfactual Candidate Analysis implementation

#include "cca.h"

#include <algorithm>

#include "../evaluate.h"
#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::CCA {

using namespace Stockfish;

std::vector<CandidateAnalysis> CounterfactualAnalyzer::analyze_candidates(const Position& pos, Search::Stack* ss,
                                                                         const std::vector<Move>& candidates, int depth) {
    std::vector<CandidateAnalysis> results;
    stats = {};

    for (Move m : candidates) {
        if ((int)results.size() >= config.max_candidates) break;

        CandidateAnalysis analysis = analyze_single_candidate(pos, ss, m, depth);
        results.push_back(analysis);
        stats.candidates_analyzed++;

        if (analysis.is_fragile) stats.fragile_candidates++;
        if (analysis.refutation_depth > 0) stats.refutations_found++;
    }

    return results;
}

CandidateAnalysis CounterfactualAnalyzer::analyze_single_candidate(const Position& pos, Search::Stack* ss,
                                                                  Move candidate, int depth) {
    CandidateAnalysis result;
    result.move = candidate;

    // Make the candidate move
    Position next_pos = pos;
    next_pos.do_move(candidate, ss);

    // Search for opponent's best reply
    int refutation_depth = find_refutation(next_pos, ss + 1, MOVE_NONE, depth - 1);
    result.refutation_depth = refutation_depth;

    // Count defensive resources
    int defenses = count_defensive_resources(next_pos, ss + 1);
    result.defensive_resources = defenses;

    // Estimate worst case score
    // In practice, this would be from the search
    result.worst_case_score = -VALUE_INFINITE;  // placeholder

    // Determine fragility
    int score_diff = 0;  // would be candidate score - worst case
    result.is_fragile = (score_diff > config.fragility_threshold);
    result.stability_score = std::max(0, 100 - score_diff / 10);

    return result;
}

int CounterfactualAnalyzer::find_refutation(const Position& pos, Search::Stack* ss, Move candidate, int depth) {
    // Search for opponent's refuting move
    // Placeholder - would do a bounded search
    if (depth <= 0) return 0;

    MoveList<GenType::LEGAL> moves(pos);
    for (Move m : moves) {
        // Check if this move significantly reduces the score
        // In practice, would do a quick search
    }

    return 0;  // No refutation found within budget
}

int CounterfactualAnalyzer::count_defensive_resources(const Position& pos, Search::Stack* ss) {
    int count = 0;

    // Count pieces that can defend
    for (Square s : pos.pieces(pos.side_to_move())) {
        Bitboard defenders = pos.attackers_to(s) & pos.pieces(pos.side_to_move());
        if (defenders) count++;
    }

    // Count king safety resources
    Square king_sq = pos.square<KING>(pos.side_to_move());
    count += popcount(pos.attackers_to(king_sq) & pos.pieces(pos.side_to_move()));

    return count;
}

}  // namespace Gambo::CCA