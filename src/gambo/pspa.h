/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PSPA — Pluggable Search Policy Architecture (Experimental)

#ifndef GAMBO_PSPA_H_INCLUDED
#define GAMBO_PSPA_H_INCLUDED

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::PSPA {

using namespace Stockfish;

enum class SearchPolicy {
    ALPHABETA = 0,
    MCTS = 1,
    PUCT = 2,
    HYBRID = 3,
};

struct PSPAConfig {
    SearchPolicy default_policy = SearchPolicy::ALPHABETA;
    bool enabled = false;
};

struct PSPAResult {
    Move best_move;
    Value value;
    SearchPolicy policy_used;
    int nodes;
};

class PluggableSearchPolicy {
public:
    PluggableSearchPolicy() = default;

    void init(const PSPAConfig& cfg) { config = cfg; enabled = cfg.enabled; }

    PSPAResult search(const Position& pos, Search::Stack* ss, int depth);

    bool is_enabled() const { return enabled; }
    void set_enabled(bool e) { enabled = e; }
    void set_policy(SearchPolicy p) { current_policy = p; }

private:
    PSPAConfig config;
    bool enabled = false;
    SearchPolicy current_policy = SearchPolicy::ALPHABETA;

    PSPAResult search_alphabeta(const Position& pos, Search::Stack* ss, int depth);
    PSPAResult search_mcts(const Position& pos, Search::Stack* ss, int depth);
    PSPAResult search_puct(const Position& pos, Search::Stack* ss, int depth);
    PSPAResult search_hybrid(const Position& pos, Search::Stack* ss, int depth);
};

}  // namespace Gambo::PSPA

#endif  // GAMBO_PSPA_H_INCLUDED