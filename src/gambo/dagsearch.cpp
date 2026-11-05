/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// DAG-Search implementation (Experimental)

#include "dagsearch.h"

#include "../position.h"
#include "../types.h"

namespace Gambo::DAGSearch {

using namespace Stockfish;

DAGResult DAGSearch::search(const Position& pos, int max_nodes) {
    DAGResult result;
    result.best_move = MOVE_NONE;
    result.value = VALUE_NONE;
    result.nodes = 0;

    if (!enabled) return result;

    // Placeholder
    return result;
}

}  // namespace Gambo::DAGSearch