/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// NFAR — NNUE Fast Accumulator Refresh (stub)

#ifndef GAMBO_NFAR_H_INCLUDED
#define GAMBO_NFAR_H_INCLUDED

#include <atomic>
#include <cstdint>
#include <vector>

#include "../position.h"
#include "../types.h"

namespace Gambo::NFAR {

using namespace Stockfish;

struct RefreshCacheEntry {
    uint64_t position_key = 0;
    uint8_t feature_set_id = 0;
    uint8_t perspective = 0;
    bool valid = false;
    int age = 0;

    RefreshCacheEntry() = default;
};

struct RefreshStats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> full_refreshes{0};
    std::atomic<uint64_t> incremental_refreshes{0};
    std::atomic<uint64_t> cache_evictions{0};

    RefreshStats() = default;
    RefreshStats(const RefreshStats&) = delete;
    RefreshStats& operator=(const RefreshStats&) = delete;
};

class FastAccumulatorRefresh {
public:
    FastAccumulatorRefresh(size_t cache_size = 1024);
    ~FastAccumulatorRefresh() = default;

    void clear_cache();
    void resize_cache(size_t);
    const RefreshStats& get_stats() const;
    void reset_stats();

private:
    std::vector<RefreshCacheEntry> refresh_cache;
    size_t cache_size = 1024;
    RefreshStats stats;
    std::atomic<uint32_t> current_age{0};
};

}  // namespace Gambo::NFAR

#endif  // GAMBO_NFAR_H_INCLUDED