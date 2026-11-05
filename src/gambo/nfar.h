/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// NFAR — NNUE Fast Accumulator Refresh
// Improves accumulator refresh and reconstruction with caching and SIMD

#ifndef GAMBO_NFAR_H_INCLUDED
#define GAMBO_NFAR_H_INCLUDED

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "../nnue/nnue_accumulator.h"
#include "../nnue/nnue_common.h"
#include "../position.h"
#include "../types.h"

namespace Gambo::NFAR {

using namespace Stockfish;

struct RefreshCacheEntry {
    std::array<std::int16_t, MaxAccumulatorSize> accumulator;
    uint64_t position_key;
    uint8_t feature_set_id;
    uint8_t perspective;
    bool valid;
    int age;  // for LRU eviction

    RefreshCacheEntry() : position_key(0), feature_set_id(0), perspective(0), valid(false), age(0) {}
};

struct RefreshStats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> full_refreshes{0};
    std::atomic<uint64_t> incremental_refreshes{0};
    std::atomic<uint64_t> cache_evictions{0};
};

class FastAccumulatorRefresh {
public:
    FastAccumulatorRefresh(size_t cache_size = 1024);
    ~FastAccumulatorRefresh() = default;

    // Main refresh interface
    template<typename FeatureSet>
    void refresh_accumulator(const Position& pos,
                            const FeatureTransformer<FeatureSet>& ft,
                            Color perspective,
                            AccumulatorState<FeatureSet>& accumulator,
                            AccumulatorCaches& caches);

    // Incremental update
    template<typename FeatureSet>
    void update_incremental(const Position& pos,
                           const FeatureTransformer<FeatureSet>& ft,
                           Color perspective,
                           Square changed_sq,
                           AccumulatorState<FeatureSet>& accumulator,
                           AccumulatorCaches& caches,
                           bool forward);

    // Cache management
    void clear_cache();
    void resize_cache(size_t new_size);
    RefreshStats get_stats() const { return stats; }
    void reset_stats();

    // Prefetch for likely positions
    void prefetch_likely(const Position& pos, const std::vector<Move>& moves);

private:
    std::vector<RefreshCacheEntry> refresh_cache;
    size_t cache_size;
    RefreshStats stats;
    std::atomic<uint32_t> current_age{0};

    // Cache lookup
    bool find_cached(const Position& pos, uint8_t feature_set_id, uint8_t perspective,
                    std::int16_t* out_accumulator);

    // Store in cache
    void store_cached(const Position& pos, uint8_t feature_set_id, uint8_t perspective,
                     const std::int16_t* accumulator);

    // SIMD-optimized copy
    void simd_copy_accumulator(const std::int16_t* src, std::int16_t* dst, size_t size);

    // Check if incremental is beneficial
    bool use_incremental(const Position& pos, Square changed_sq, int depth) const;
};

}  // namespace Gambo::NFAR

#endif  // GAMBO_NFAR_H_INCLUDED