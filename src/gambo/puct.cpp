/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PUCT implementation (Experimental)

#include "puct.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "../position.h"
#include "../types.h"

namespace Gambo::PUCT {

using namespace Stockfish;

PUCTResult PUCTSearch::search(const Position& pos, int simulations) {
    PUCTResult result;
    result.best_move = MOVE_NONE;
    result.value = VALUE_NONE;
    result.visits = 0;
    result.confidence = 0.0;

    if (!enabled) return result;

    // Placeholder
    return result;
}

double PUCTSearch::uct_value(Node* node, int parent_visits) const {
    if (node->visits == 0) return std::numeric_limits<double>::infinity();
    return node->q_value + config.c_puct * node->prior * std::sqrt(parent_visits) / (1 + node->visits);
}

void PUCTSearch::backup(Node* node, Value value) {
    while (node) {
        node->visits++;
        node->q_value += (value - node->q_value) / node->visits;
        node = node->parent;
    }
}

double PUCTSearch::dirichlet_noise(int move_count) const {
    static std::mt19937 gen(42);
    std::gamma_distribution<> dist(config.dirichlet_alpha, 1.0);
    return dist(gen);
}

}  // namespace Gambo::PUCT