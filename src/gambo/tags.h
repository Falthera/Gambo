/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// T-AGS — Transposition-Aware Graph Search (Experimental)

#ifndef GAMBO_TAGS_H_INCLUDED
#define GAMBO_TAGS_H_INCLUDED

#include <cstdint>
#include <unordered_map>

#include "../position.h"
#include "../types.h"

namespace Gambo::TAGS {

using namespace Stockfish;

struct TAGSConfig {
    int max_nodes = 100000;
    bool use_transposition = true;
    bool enabled = false;
};

struct TAGSResult {
    Move best_move;
    Value value;
    int nodes_explored;
};

class TranspositionAwareGraphSearch {
public:
    TranspositionAwareGraphSearch() = default;

    void init(const TAGSConfig& cfg) { config = cfg; enabled = cfg.enabled; }

    TAGSResult search(const Position& pos, int max_nodes);

    bool is_enabled() const { return enabled; }
    void set_enabled(bool e) { enabled = e; }

private:
    TAGSConfig config;
    bool enabled = false;

    struct GraphNode {
        uint64_t key;
        Value value;
        int depth;
        std::vector<uint64_t> successors;
        std::vector<uint64_t> predecessors;
    };

    std::unordered_map<uint64_t, GraphNode> graph;
};

}  // namespace Gambo::TAGS

#endif  // GAMBO_TAGS_H_INCLUDED