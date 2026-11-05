/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PG-MCTS — Policy-Guided Monte Carlo Tree Search (Experimental)
// Stub implementation - disabled by default

#ifndef GAMBO_PGMCTS_H_INCLUDED
#define GAMBO_PGMCTS_H_INCLUDED

#include <cstdint>

#include "../position.h"
#include "../types.h"

namespace Gambo::PG_MCTS {

using namespace Stockfish;

struct MCTSConfig {
    int max_iterations = 1000;
    double c_puct = 1.414;
    int max_depth = 20;
    bool enabled = false;  // Disabled by default
};

struct MCTSResult {
    Move best_move;
    Value value;
    int iterations;
    double confidence;
};

class PolicyGuidedMCTS {
public:
    PolicyGuidedMCTS() = default;

    void init(const MCTSConfig& cfg) { config = cfg; enabled = cfg.enabled; }

    MCTSResult search(const Position& pos, int iterations);

    bool is_enabled() const { return enabled; }
    void set_enabled(bool e) { enabled = e; }

private:
    MCTSConfig config;
    bool enabled = false;

    struct Node {
        Move move;
        Value value = 0;
        int visits = 0;
        double prior = 0;
        std::vector<Node*> children;
        Node* parent = nullptr;
    };
};

}  // namespace Gambo::PG_MCTS

#endif  // GAMBO_PGMCTS_H_INCLUDED