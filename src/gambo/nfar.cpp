/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// NFAR — NNUE Fast Accumulator Refresh (stub implementation)

#include "nfar.h"

#include "../position.h"
#include "../types.h"

namespace Gambo::NFAR {

using namespace Stockfish;

FastAccumulatorRefresh::FastAccumulatorRefresh(size_t cache_size_)
    : cache_size(cache_size_), refresh_cache(cache_size_) {
    clear_cache();
}

void FastAccumulatorRefresh::clear_cache() {
    for (auto& entry : refresh_cache) {
        entry.valid = false;
        entry.age = 0;
    }
    current_age = 0;
}

void FastAccumulatorRefresh::resize_cache(size_t new_size) {
    cache_size = new_size;
    refresh_cache.resize(new_size);
    clear_cache();
}

const RefreshStats& FastAccumulatorRefresh::get_stats() const { return stats; }

void FastAccumulatorRefresh::reset_stats() {
    stats.hits = 0;
    stats.misses = 0;
    stats.full_refreshes = 0;
    stats.incremental_refreshes = 0;
    stats.cache_evictions = 0;
}

}  // namespace Gambo::NFAR