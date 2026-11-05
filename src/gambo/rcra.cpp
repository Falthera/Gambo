/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// RCRA — Root Candidate Robustness Analysis implementation

#include "rcra.h"

#include <algorithm>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::RCRA {

using namespace Stockfish;

std::vector<RootCandidateInfo> RootCandidateAnalyzer::analyze_candidates(const Position& pos, Search::Stack* ss,
                                                                        const std::vector<Search::RootMove>& root_moves,
                                                                        int depth) {
    std::vector<RootCandidateInfo> results;
    stats = {};

    for (const auto& rm : root_moves) {
        if ((int)results.size() >= config.max_candidates) break;

        RootCandidateInfo info = analyze_single_candidate(pos, ss, rm, depth);
        results.push_back(info);
        stats.candidates_analyzed++;

        if (info.is_reliable) stats.reliable_candidates++;
        else stats.unreliable_candidates++;
    }

    prev_candidates = results;
    return results;
}

void RootCandidateAnalyzer::on_iteration_complete(const Position& pos, const std::vector<Search::RootMove>& root_moves, int depth) {
    // Track PV changes
    if (!prev_candidates.empty()) {
        for (size_t i = 0; i < std::min(root_moves.size(), prev_candidates.size()); ++i) {
            if (root_moves[i].move != prev_candidates[i].move) {
                stats.pv_changes++;
            }
        }
    }
}

RootCandidateInfo RootCandidateAnalyzer::analyze_single_candidate(const Position& pos, Search::Stack* ss,
                                                                 const Search::RootMove& rm, int depth) {
    RootCandidateInfo info;
    info.move = rm.move;
    info.score = rm.score;

    // Stability from score history
    info.stability = compute_stability(rm);

    // PV consistency
    info.pv_consistency = compute_pv_consistency(rm);

    // Tactical vulnerability
    info.tactical_vulnerability = compute_tactical_vulnerability(pos, ss, rm.move);

    // Defensive quality
    info.defensive_quality = compute_defensive_quality(pos, ss, rm.move);

    // Overall robustness
    info.robustness_score = (info.stability + info.pv_consistency * 2 + info.defensive_quality
                            - info.tactical_vulnerability) / 4;
    info.robustness_score = std::clamp(info.robustness_score, 0, 100);

    info.is_reliable = (info.robustness_score >= config.min_stability_for_reliable &&
                       info.pv_consistency >= config.min_pv_consistency &&
                       info.tactical_vulnerability <= config.max_tactical_vulnerability);

    return info;
}

int RootCandidateAnalyzer::compute_stability(const Search::RootMove& rm) const {
    // Score stability - how much score varies across iterations
    // In practice would track score_history
    // Placeholder: based on score variance
    return 50;  // neutral
}

int RootCandidateAnalyzer::compute_pv_consistency(const Search::RootMove& rm) const {
    // How often the PV move stays the same
    // Placeholder
    return rm.pv ? (int)rm.pv->size() * 10 : 0;
}

int RootCandidateAnalyzer::compute_tactical_vulnerability(const Position& pos, Search::Stack* ss, Move m) const {
    // How tactically vulnerable is this move
    // Check if move creates weaknesses
    Position next_pos = pos;
    next_pos.do_move(m, ss);

    int vulnerability = 0;

    // Check if our king becomes exposed
    Square our_king = next_pos.square<KING>(next_pos.side_to_move());
    Bitboard enemy_attackers = next_pos.attackers_to(our_king) & next_pos.pieces(~next_pos.side_to_move());
    vulnerability += popcount(enemy_attackers) * 10;

    // Check if we lose material
    // Placeholder

    return std::min(vulnerability, 100);
}

int RootCandidateAnalyzer::compute_defensive_quality(const Position& pos, Search::Stack* ss, Move m) const {
    // Quality of defensive resources after this move
    Position next_pos = pos;
    next_pos.do_move(m, ss);

    int defense = 0;

    // Count pieces defending each other
    for (Square s : next_pos.pieces(next_pos.side_to_move())) {
        Bitboard defenders = next_pos.attackers_to(s) & next_pos.pieces(next_pos.side_to_move());
        if (defenders) defense += 5;
    }

    // King safety
    Square king_sq = next_pos.square<KING>(next_pos.side_to_move());
    Bitboard king_defenders = next_pos.attackers_to(king_sq) & next_pos.pieces(next_pos.side_to_move());
    defense += popcount(king_defenders) * 10;

    return std::min(defense, 100);
}

}  // namespace Gambo::RCRA