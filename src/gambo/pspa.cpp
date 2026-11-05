/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PSPA implementation (Experimental)

#include "pspa.h"

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::PSPA {

using namespace Stockfish;

PSPAResult PluggableSearchPolicy::search(const Position& pos, Search::Stack* ss, int depth) {
    PSPAResult result;
    result.best_move = MOVE_NONE;
    result.value = VALUE_NONE;
    result.policy_used = current_policy;
    result.nodes = 0;

    if (!enabled) return result;

    switch (current_policy) {
        case SearchPolicy::ALPHABETA:
            return search_alphabeta(pos, ss, depth);
        case SearchPolicy::MCTS:
            return search_mcts(pos, ss, depth);
        case SearchPolicy::PUCT:
            return search_puct(pos, ss, depth);
        case SearchPolicy::HYBRID:
            return search_hybrid(pos, ss, depth);
    }

    return result;
}

PSPAResult PluggableSearchPolicy::search_alphabeta(const Position& pos, Search::Stack* ss, int depth) {
    PSPAResult result;
    result.policy_used = SearchPolicy::ALPHABETA;
    // Placeholder
    return result;
}

PSPAResult PluggableSearchPolicy::search_mcts(const Position& pos, Search::Stack* ss, int depth) {
    PSPAResult result;
    result.policy_used = SearchPolicy::MCTS;
    // Placeholder
    return result;
}

PSPAResult PluggableSearchPolicy::search_puct(const Position& pos, Search::Stack* ss, int depth) {
    PSPAResult result;
    result.policy_used = SearchPolicy::PUCT;
    // Placeholder
    return result;
}

PSPAResult PluggableSearchPolicy::search_hybrid(const Position& pos, Search::Stack* ss, int depth) {
    PSPAResult result;
    result.policy_used = SearchPolicy::HYBRID;
    // Placeholder
    return result;
}

}  // namespace Gambo::PSPA