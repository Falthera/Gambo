/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// DAG-Search — Directed Acyclic Graph Search (Experimental)

#ifndef GAMBO_DAGSEARCH_H_INCLUDED
#define GAMBO_DAGSEARCH_H_INCLUDED

#include <cstdint>
#include <vector>

#include "../position.h"
#include "../types.h"

namespace Gambo::DAGSearch {

using namespace Stockfish;

struct DAGConfig {
    int max_nodes = 50000;
    bool merge_equivalent = true;
    bool enabled = false;
};

struct DAGResult {
    Move best_move;
    Value value;
    int nodes;
};

class DAGSearch {
public:
    DAGSearch() = default;

    void init(const DAGConfig& cfg) { config = cfg; enabled = cfg.enabled; }

    DAGResult search(const Position& pos, int max_nodes);

    bool is_enabled() const { return enabled; }
    void set_enabled(bool e) { enabled = e; }

private:
    DAGConfig config;
    bool enabled = false;

    struct DAGNode {
        uint64_t key;
        Value value;
        int depth;
        std::vector<uint64_t> parents;
        std::vector<uint64_t> children;
        int ref_count = 0;
    };

    std::vector<DAGNode> nodes;
};

}  // namespace Gambo::DAGSearch

#endif  // GAMBO_DAGSEARCH_H_INCLUDED