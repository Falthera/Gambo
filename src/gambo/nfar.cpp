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
#include <immintrin.h>

#include "../nnue/nnue_accumulator.h"
#include "../nnue/nnue_common.h"
#include "../position.h"
#include "../types.h"

namespace Gambo::NFAR {

using namespace Stockfish;

FastAccumulatorRefresh::FastAccumulatorRefresh(size_t cache_size_)
    : cache_size(cache_size_), refresh_cache(cache_size) {
    clear_cache();
}

template<typename FeatureSet>
void FastAccumulatorRefresh::refresh_accumulator(const Position& pos,
                                                const FeatureTransformer<FeatureSet>& ft,
                                                Color perspective,
                                                AccumulatorState<FeatureSet>& accumulator,
                                                AccumulatorCaches& caches) {
    // Try cache first
    if (find_cached(pos, FeatureSet::Index, perspective, accumulator.accumulator)) {
        stats.hits++;
        return;
    }

    stats.misses++;

    // Check if we can do incremental
    if (use_incremental(pos, SQ_NONE, 0)) {
        // For now, do full refresh but track it
        stats.full_refreshes++;
        // Call the standard refresh
        accumulator.refresh(pos, ft, caches, perspective);
        store_cached(pos, FeatureSet::Index, perspective, accumulator.accumulator);
    } else {
        stats.full_refreshes++;
        accumulator.refresh(pos, ft, caches, perspective);
        store_cached(pos, FeatureSet::Index, perspective, accumulator.accumulator);
    }
}

template<typename FeatureSet>
void FastAccumulatorRefresh::update_incremental(const Position& pos,
                                               const FeatureTransformer<FeatureSet>& ft,
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
            // Cache hit
            simd_copy_accumulator(entry.accumulator.data(), out_accumulator, entry.accumulator.size());
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

    simd_copy_accumulator(accumulator, entry.accumulator.data(), entry.accumulator.size());
}

void FastAccumulatorRefresh::simd_copy_accumulator(const std::int16_t* src, std::int16_t* dst, size_t size) {
    size_t i = 0;
    // Use AVX2 for 128-bit copies (8 int16 at a time)
    for (; i + 7 < size; i += 8) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), v);
    }
    // Remaining elements
    for (; i < size; ++i) {
        dst[i] = src[i];
    }
}

bool FastAccumulatorRefresh::use_incremental(const Position& pos, Square changed_sq, int depth) const {
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

void FastAccumulatorRefresh::prefetch_likely(const Position& pos, const std::vector<Move>& moves) {
    // Prefetch likely next positions' cache entries
    for (Move m : moves) {
        Position next_pos = pos;
        next_pos.do_move(m);
        // Just touch the cache line to bring it in
        uint64_t key = next_pos.key();
        for (auto& entry : refresh_cache) {
            if (entry.valid && entry.position_key == key) {
                _mm_prefetch(reinterpret_cast<const char*>(&entry), _MM_HINT_T0);
                break;
            }
        }
    }
}

// Explicit template instantiations for common feature sets
template void FastAccumulatorRefresh::refresh_accumulator<Features::HalfKAv2_hm>(
    const Position&, const FeatureTransformer<Features::HalfKAv2_hm>&, Color,
    AccumulatorState<Features::HalfKAv2_hm>&, AccumulatorCaches&);

template void FastAccumulatorRefresh::refresh_accumulator<Features::KingSafety>(
    const Position&, const FeatureTransformer<Features::KingSafety>&, Color,
    AccumulatorState<Features::KingSafety>&, AccumulatorCaches&);

template void FastAccumulatorRefresh::refresh_accumulator<Features::PieceCoordination>(
    const Position&, const FeatureTransformer<Features::PieceCoordination>&, Color,
    AccumulatorState<Features::PieceCoordination>&, AccumulatorCaches&);

template void FastAccumulatorRefresh::refresh_accumulator<Features::FullThreats>(
    const Position&, const FeatureTransformer<Features::FullThreats>&, Color,
    AccumulatorState<Features::FullThreats>&, AccumulatorCaches&);

template void FastAccumulatorRefresh::update_incremental<Features::HalfKAv2_hm>(
    const Position&, const FeatureTransformer<Features::HalfKAv2_hm>&, Color, Square,
    AccumulatorState<Features::HalfKAv2_hm>&, AccumulatorCaches&, bool);

template void FastAccumulatorRefresh::update_incremental<Features::KingSafety>(
    const Position&, const FeatureTransformer<Features::KingSafety>&, Color, Square,
    AccumulatorState<Features::KingSafety>&, AccumulatorCaches&, bool);

template void FastAccumulatorRefresh::update_incremental<Features::PieceCoordination>(
    const Position&, const FeatureTransformer<Features::PieceCoordination>&, Color, Square,
    AccumulatorState<Features::PieceCoordination>&, AccumulatorCaches&, bool);

template void FastAccumulatorRefresh::update_incremental<Features::FullThreats>(
    const Position&, const FeatureTransformer<Features::FullThreats>&, Color, Square,
    AccumulatorState<Features::FullThreats>&, AccumulatorCaches&, bool);

}  // namespace Gambo::NFAR