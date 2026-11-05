/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// HA-MS implementation (Experimental)

#include "hams.h"

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::HAMS {

using namespace Stockfish;

HAMSResult HybridAlphaBetaMCTS::search(const Position& pos, Search::Stack* ss, int depth) {
    HAMSResult result;
    result.best_move = MOVE_NONE;
    result.value = VALUE_NONE;
    result.alphabeta_nodes = 0;
    result.mcts_iterations = 0;
    result.confidence = 0.0;

    if (!enabled) return result;

    if (depth < config.switch_depth) {
        // Use pure alpha-beta for shallow depths
        return run_alphabeta(pos, ss, depth);
    }

    // Run both in parallel (or sequentially)
    HAMSResult ab_result = run_alphabeta(pos, ss, depth);
    HAMSResult mcts_result = run_mcts(pos, config.mcts_iterations);

    return merge_results(ab_result, mcts_result);
}

HAMSResult HybridAlphaBetaMCTS::run_alphabeta(const Position& pos, Search::Stack* ss, int depth) {
    HAMSResult result;
    result.best_move = MOVE_NONE;
    result.value = VALUE_NONE;
    result.alphabeta_nodes = 0;
    result.mcts_iterations = 0;
    result.confidence = 0.0;

    // Placeholder - would call actual alpha-beta search
    return result;
}

HAMSResult HybridAlphaBetaMCTS::run_mcts(const Position& pos, int iterations) {
    HAMSResult result;
    result.best_move = MOVE_NONE;
    result.value = VALUE_NONE;
    result.alphabeta_nodes = 0;
    result.mcts_iterations = iterations;
    result.confidence = 0.0;

    // Placeholder - would call MCTS
    return result;
}

HAMSResult HybridAlphaBetaMCTS::merge_results(const HAMSResult& ab, const HAMSResult& mcts) {
    HAMSResult result;

    // Weighted combination
    double ab_w = config.alphabeta_weight / 100.0;
    double mcts_w = config.mcts_weight / 100.0;

    if (ab.best_move != MOVE_NONE && mcts.best_move != MOVE_NONE && ab.best_move == mcts.best_move) {
        // Both agree - high confidence
        result.best_move = ab.best_move;
        result.confidence = 0.9;
    } else if (ab.best_move != MOVE_NONE) {
        // Prefer alpha-beta
        result.best_move = ab.best_move;
        result.confidence = ab_w;
    } else if (mcts.best_move != MOVE_NONE) {
        result.best_move = mcts.best_move;
        result.confidence = mcts_w;
    } else {
        result.best_move = MOVE_NONE;
        result.confidence = 0.0;
    }

    result.value = static_cast<Value>(ab.value * ab_w + mcts.value * mcts_w);
    result.alphabeta_nodes = ab.alphabeta_nodes;
    result.mcts_iterations = mcts.mcts_iterations;

    return result;
}

}  // namespace Gambo::HAMS