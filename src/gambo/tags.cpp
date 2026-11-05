/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// T-AGS implementation (Experimental)

#include "tags.h"

#include "../position.h"
#include "../types.h"

namespace Gambo::TAGS {

using namespace Stockfish;

TAGSResult TranspositionAwareGraphSearch::search(const Position& pos, int max_nodes) {
    TAGSResult result;
    result.best_move = MOVE_NONE;
    result.value = VALUE_NONE;
    result.nodes_explored = 0;

    if (!enabled) return result;

    // Placeholder
    return result;
}

}  // namespace Gambo::TAGS