/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// NFAR — NNUE Fast Accumulator Refresh implementation

#include "nfar.h"

#include <algorithm>
#include <cstring>

#include "../position.h"
#include "../types.h"

namespace Gambo::NFAR {

using namespace Stockfish;
using namespace Stockfish::Eval::NNUE;

FastAccumulatorRefresh::FastAccumulatorRefresh(size_t cache_size_)
    : cache_size(cache_size_), refresh_cache(cache_size) {
    clear_cache();
}

template<typename FeatureSet>
void FastAccumulatorRefresh::refresh_accumulator(const Position& pos,
                                                const FeatureTransformer& ft,
                                                Color perspective,
                                                AccumulatorState<FeatureSet>& accumulator,
                                                AccumulatorCaches& caches) {
    // Try cache first
    if (find_cached(pos, FeatureSet::Index, perspective, accumulator.accumulation[perspective].data())) {
        stats.hits++;
        return;
    }

    stats.misses++;
    stats.full_refreshes++;

    // Full refresh using the standard NNUE accumulator
    accumulator.refresh(pos, ft, caches, perspective);
    store_cached(pos, FeatureSet::Index, perspective, accumulator.accumulation[perspective].data());
}

template<typename FeatureSet>
void FastAccumulatorRefresh::update_incremental(const Position& pos,
                                               const FeatureTransformer& ft,
                                               Color perspective,
                                               Square changed_sq,
                                               AccumulatorState<FeatureSet>& accumulator,
                                               AccumulatorCaches& caches,
                                               bool forward) {
    stats.incremental_refreshes++;
    accumulator.update_incremental(pos, ft, changed_sq, caches, perspective, forward);
}

bool FastAccumulatorRefresh::find_cached(const Position& pos, uint8_t feature_set_id,
                                        uint8_t perspective, std::int16_t* out_accumulator) {
    uint64_t key = pos.key();

    for (auto& entry : refresh_cache) {
        if (entry.valid && entry.position_key == key &&
            entry.feature_set_id == feature_set_id &&
            entry.perspective == perspective) {
            // Cache hit - copy accumulator data
            copy_accumulator(entry.accumulation[perspective].data(), out_accumulator, L1);
            entry.age = current_age++;
            return true;
        }
    }
    return false;
}

void FastAccumulatorRefresh::store_cached(const Position& pos, uint8_t feature_set_id,
                                         uint8_t perspective, const std::int16_t* accumulator) {
    // Find LRU entry to replace
    size_t lru_idx = 0;
    int min_age = INT_MAX;

    for (size_t i = 0; i < refresh_cache.size(); ++i) {
        if (!refresh_cache[i].valid || refresh_cache[i].age < min_age) {
            min_age = refresh_cache[i].valid ? refresh_cache[i].age : -1;
            lru_idx = i;
            if (!refresh_cache[i].valid) break;
        }
    }

    if (refresh_cache[lru_idx].valid) {
        stats.cache_evictions++;
    }

    auto& entry = refresh_cache[lru_idx];
    entry.position_key = pos.key();
    entry.feature_set_id = feature_set_id;
    entry.perspective = perspective;
    entry.valid = true;
    entry.age = current_age++;

    copy_accumulator(accumulator, entry.accumulation[perspective].data(), L1);
}

void FastAccumulatorRefresh::copy_accumulator(const std::int16_t* src, std::int16_t* dst, size_t size) {
    std::memcpy(dst, src, size * sizeof(std::int16_t));
}

bool FastAccumulatorRefresh::use_incremental(const Position& pos, Square changed_sq, int depth) const {
    (void)pos;
    // Use incremental for shallow depths or simple moves
    if (depth < 8) return true;
    if (changed_sq == SQ_NONE) return false;
    return true;  // Default to incremental
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

void FastAccumulatorRefresh::reset_stats() {
    stats.hits = 0;
    stats.misses = 0;
    stats.full_refreshes = 0;
    stats.incremental_refreshes = 0;
    stats.cache_evictions = 0;
}

// Explicit template instantiations for the four Stockfish feature sets
template void FastAccumulatorRefresh::refresh_accumulator<Features::HalfKAv2_hm>(
    const Position&, const FeatureTransformer&, Color,
    AccumulatorState<Features::HalfKAv2_hm>&, AccumulatorCaches&);

template void FastAccumulatorRefresh::refresh_accumulator<Features::KingSafety>(
    const Position&, const FeatureTransformer&, Color,
    AccumulatorState<Features::KingSafety>&, AccumulatorCaches&);

template void FastAccumulatorRefresh::refresh_accumulator<Features::PieceCoordination>(
    const Position&, const FeatureTransformer&, Color,
    AccumulatorState<Features::PieceCoordination>&, AccumulatorCaches&);

template void FastAccumulatorRefresh::refresh_accumulator<Features::FullThreats>(
    const Position&, const FeatureTransformer&, Color,
    AccumulatorState<Features::FullThreats>&, AccumulatorCaches&);

template void FastAccumulatorRefresh::update_incremental<Features::HalfKAv2_hm>(
    const Position&, const FeatureTransformer&, Color, Square,
    AccumulatorState<Features::HalfKAv2_hm>&, AccumulatorCaches&, bool);

template void FastAccumulatorRefresh::update_incremental<Features::KingSafety>(
    const Position&, const FeatureTransformer&, Color, Square,
    AccumulatorState<Features::KingSafety>&, AccumulatorCaches&, bool);

template void FastAccumulatorRefresh::update_incremental<Features::PieceCoordination>(
    const Position&, const FeatureTransformer&, Color, Square,
    AccumulatorState<Features::PieceCoordination>&, AccumulatorCaches&, bool);

template void FastAccumulatorRefresh::update_incremental<Features::FullThreats>(
    const Position&, const FeatureTransformer&, Color, Square,
    AccumulatorState<Features::FullThreats>&, AccumulatorCaches&, bool);

}  // namespace Gambo::NFAR