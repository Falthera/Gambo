/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// HA-MS — Hybrid Alpha-Beta and MCTS Search (Experimental)

#ifndef GAMBO_HAMS_H_INCLUDED
#define GAMBO_HAMS_H_INCLUDED

#include <cstdint>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::HAMS {

using namespace Stockfish;

struct HAMSConfig {
    int alphabeta_weight = 70;      // percentage
    int mcts_weight = 30;           // percentage
    int switch_depth = 12;          // depth to switch to hybrid
    int mcts_iterations = 1000;
    bool enabled = false;
};

struct HAMSResult {
    Move best_move;
    Value value;
    int alphabeta_nodes;
    int mcts_iterations;
    double confidence;
};

class HybridAlphaBetaMCTS {
public:
    HybridAlphaBetaMCTS() = default;

    void init(const HAMSConfig& cfg) { config = cfg; enabled = cfg.enabled; }

    HAMSResult search(const Position& pos, Search::Stack* ss, int depth);

    bool is_enabled() const { return enabled; }
    void set_enabled(bool e) { enabled = e; }

private:
    HAMSConfig config;
    bool enabled = false;

    HAMSResult run_alphabeta(const Position& pos, Search::Stack* ss, int depth);
    HAMSResult run_mcts(const Position& pos, int iterations);
    HAMSResult merge_results(const HAMSResult& ab, const HAMSResult& mcts);
};

}  // namespace Gambo::HAMS

#endif  // GAMBO_HAMS_H_INCLUDED