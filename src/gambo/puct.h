/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PUCT — Predictor Upper Confidence Bound Search (Experimental)

#ifndef GAMBO_PUCT_H_INCLUDED
#define GAMBO_PUCT_H_INCLUDED

#include <cstdint>

#include "../position.h"
#include "../types.h"

namespace Gambo::PUCT {

using namespace Stockfish;

struct PUCTConfig {
    double c_puct = 1.414;
    double dirichlet_alpha = 0.3;
    double dirichlet_epsilon = 0.25;
    int max_depth = 20;
    bool enabled = false;
};

struct PUCTResult {
    Move best_move;
    Value value;
    int visits;
    double confidence;
};

class PUCTSearch {
public:
    PUCTSearch() = default;

    void init(const PUCTConfig& cfg) { config = cfg; enabled = cfg.enabled; }

    PUCTResult search(const Position& pos, int simulations);

    bool is_enabled() const { return enabled; }
    void set_enabled(bool e) { enabled = e; }

private:
    PUCTConfig config;
    bool enabled = false;

    struct Node {
        Move move;
        Value value = 0;
        int visits = 0;
        double prior = 0;
        double q_value = 0;
        std::vector<Node*> children;
        Node* parent = nullptr;
    };

    double uct_value(Node* node, int parent_visits) const;
    void backup(Node* node, Value value);
    double dirichlet_noise(int move_count) const;
};

}  // namespace Gambo::PUCT

#endif  // GAMBO_PUCT_H_INCLUDED