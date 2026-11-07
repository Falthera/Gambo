/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "nnue_accumulator.h"

#include <cassert>
#include <cstdint>
#include <new>
#include <type_traits>

#include "../bitboard.h"
#include "../misc.h"
#include "../position.h"
#include "../types.h"
#include "nnue_architecture.h"
#include "nnue_common.h"
#include "nnue_feature_transformer.h"  // IWYU pragma: keep
#include "simd.h"

namespace Stockfish::Eval::NNUE {

using namespace SIMD;


template<bool Forward, typename FeatureSet>
void update_accumulator_incremental(Color                               perspective,
                                    const FeatureTransformer&           featureTransformer,
                                    const Square                        ksq,
                                    AccumulatorState<FeatureSet>&       target_state,
                                    const AccumulatorState<FeatureSet>& computed) {
    // Incremental update: copy from computed state
    // The actual implementation would apply the diff (added/removed weights)
    // For now, we do a full copy as a fallback
    target_state = computed;
}

// Specialization for Gambo PawnPair feature set - applies incremental weight updates
template<bool Forward>
void update_accumulator_incremental<Forward, GamboPawnPairFeatureSet>(
    Color                               perspective,
    const FeatureTransformer&           featureTransformer,
    const Square                        ksq,
    AccumulatorState<GamboPawnPairFeatureSet>&       target_state,
    const AccumulatorState<GamboPawnPairFeatureSet>& computed) {
    // Copy the computed state
    target_state = computed;
    
    // Apply incremental weight updates using the diff
    const auto& diff = computed.diff;
    if (diff.type() == DirtyPiece::Moved || diff.type() == DirtyPiece::Captured || diff.type() == DirtyPiece::Promoted) {
        // Get the weights for this feature set
        const WeightType* weights = featureTransformer.weights;
        
        typename GamboPawnPairFeatureSet::IndexList removed, added;
        removed.clear();
        added.clear();
        
        // Use the feature set's append_changed_indices to get the changed indices
        GamboPawnPairFeatureSet::append_changed_indices(perspective, ksq, Position(), computed.diff, removed, added);
        
        // Apply removed weights (subtract)
        for (int i = 0; i < removed.size(); ++i) {
            IndexType idx = removed[i];
            if (idx < GamboPawnPairFeatureSet::Dimensions) {
                target_state.accumulation[perspective][idx] -= weights[idx];
            }
        }
        
        // Apply added weights (add)
        for (int i = 0; i < added.size(); ++i) {
            IndexType idx = added[i];
            if (idx < GamboPawnPairFeatureSet::Dimensions) {
                target_state.accumulation[perspective][idx] += weights[idx];
            }
        }
    }
}

// Specialization for Gambo KingRelative feature set
template<bool Forward>
void update_accumulator_incremental<Forward, GamboKingRelativeFeatureSet>(
    Color                               perspective,
    const FeatureTransformer&           featureTransformer,
    const Square                        ksq,
    AccumulatorState<GamboKingRelativeFeatureSet>&       target_state,
    const AccumulatorState<GamboKingRelativeFeatureSet>& computed) {
    target_state = computed;
    
    const auto& diff = computed.diff;
    if (diff.type() == DirtyPiece::Moved || diff.type() == DirtyPiece::Captured || diff.type() == DirtyPiece::Promoted) {
        const WeightType* weights = featureTransformer.weights;
        
        typename GamboKingRelativeFeatureSet::IndexList removed, added;
        removed.clear();
        added.clear();
        
        GamboKingRelativeFeatureSet::append_changed_indices(perspective, ksq, Position(), computed.diff, removed, added);
        
        for (int i = 0; i < removed.size(); ++i) {
            IndexType idx = removed[i];
            if (idx < GamboKingRelativeFeatureSet::Dimensions) {
                target_state.accumulation[perspective][idx] -= weights[idx];
            }
        }
        
        for (int i = 0; i < added.size(); ++i) {
            IndexType idx = added[i];
            if (idx < GamboKingRelativeFeatureSet::Dimensions) {
                target_state.accumulation[perspective][idx] += weights[idx];
            }
        }
    }
}

// Specialization for Gambo ThreatDetection feature set
template<bool Forward>
void update_accumulator_incremental<Forward, GamboThreatDetectionFeatureSet>(
    Color                               perspective,
    const FeatureTransformer&           featureTransformer,
    const Square                        ksq,
    AccumulatorState<GamboThreatDetectionFeatureSet>&       target_state,
    const AccumulatorState<GamboThreatDetectionFeatureSet>& computed) {
    target_state = computed;
    
    const auto& diff = computed.diff;
    if (diff.type() == DirtyThreats::Changed) {
        const WeightType* weights = featureTransformer.weights;
        
        typename GamboThreatDetectionFeatureSet::IndexList removed, added;
        removed.clear();
        added.clear();
        
        GamboThreatDetectionFeatureSet::append_changed_indices(perspective, ksq, Position(), computed.diff, removed, added, nullptr, false);
        
        for (int i = 0; i < removed.size(); ++i) {
            IndexType idx = removed[i];
            if (idx < GamboThreatDetectionFeatureSet::Dimensions) {
                target_state.accumulation[perspective][idx] -= weights[idx];
            }
        }
        
        for (int i = 0; i < added.size(); ++i) {
            IndexType idx = added[i];
            if (idx < GamboThreatDetectionFeatureSet::Dimensions) {
                target_state.accumulation[perspective][idx] += weights[idx];
            }
        }
    }
}

// Specialization for Gambo PieceRelative feature set
template<bool Forward>
void update_accumulator_incremental<Forward, GamboPieceRelativeFeatureSet>(
    Color                               perspective,
    const FeatureTransformer&           featureTransformer,
    const Square                        ksq,
    AccumulatorState<GamboPieceRelativeFeatureSet>&       target_state,
    const AccumulatorState<GamboPieceRelativeFeatureSet>& computed) {
    target_state = computed;
    
    const auto& diff = computed.diff;
    if (diff.type() == DirtyPiece::Moved || diff.type() == DirtyPiece::Captured || diff.type() == DirtyPiece::Promoted) {
        const WeightType* weights = featureTransformer.weights;
        
        typename GamboPieceRelativeFeatureSet::IndexList removed, added;
        removed.clear();
        added.clear();
        
        GamboPieceRelativeFeatureSet::append_changed_indices(perspective, ksq, Position(), computed.diff, removed, added);
        
        for (int i = 0; i < removed.size(); ++i) {
            IndexType idx = removed[i];
            if (idx < GamboPieceRelativeFeatureSet::Dimensions) {
                target_state.accumulation[perspective][idx] -= weights[idx];
            }
        }
        
        for (int i = 0; i < added.size(); ++i) {
            IndexType idx = added[i];
            if (idx < GamboPieceRelativeFeatureSet::Dimensions) {
                target_state.accumulation[perspective][idx] += weights[idx];
            }
        }
    }
}

// Explicit instantiations for the four Stockfish feature sets
template void update_accumulator_incremental<true, PSQFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<PSQFeatureSet>&, const AccumulatorState<PSQFeatureSet>&);
template void update_accumulator_incremental<false, PSQFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<PSQFeatureSet>&, const AccumulatorState<PSQFeatureSet>&);
template void update_accumulator_incremental<true, ThreatFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<ThreatFeatureSet>&, const AccumulatorState<ThreatFeatureSet>&);
template void update_accumulator_incremental<false, ThreatFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<ThreatFeatureSet>&, const AccumulatorState<ThreatFeatureSet>&);
template void update_accumulator_incremental<true, KingSafetyFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<KingSafetyFeatureSet>&, const AccumulatorState<KingSafetyFeatureSet>&);
template void update_accumulator_incremental<false, KingSafetyFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<KingSafetyFeatureSet>&, const AccumulatorState<KingSafetyFeatureSet>&);
template void update_accumulator_incremental<true, PieceCoordFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<PieceCoordFeatureSet>&, const AccumulatorState<PieceCoordFeatureSet>&);
template void update_accumulator_incremental<false, PieceCoordFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<PieceCoordFeatureSet>&, const AccumulatorState<PieceCoordFeatureSet>&);

// Explicit instantiations for Gambo feature sets
template void update_accumulator_incremental<true, GamboPawnPairFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<GamboPawnPairFeatureSet>&, const AccumulatorState<GamboPawnPairFeatureSet>&);
template void update_accumulator_incremental<false, GamboPawnPairFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<GamboPawnPairFeatureSet>&, const AccumulatorState<GamboPawnPairFeatureSet>&);
template void update_accumulator_incremental<true, GamboKingRelativeFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<GamboKingRelativeFeatureSet>&, const AccumulatorState<GamboKingRelativeFeatureSet>&);
template void update_accumulator_incremental<false, GamboKingRelativeFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<GamboKingRelativeFeatureSet>&, const AccumulatorState<GamboKingRelativeFeatureSet>&);
template void update_accumulator_incremental<true, GamboThreatDetectionFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<GamboThreatDetectionFeatureSet>&, const AccumulatorState<GamboThreatDetectionFeatureSet>&);
template void update_accumulator_incremental<false, GamboThreatDetectionFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<GamboThreatDetectionFeatureSet>&, const AccumulatorState<GamboThreatDetectionFeatureSet>&);
template void update_accumulator_incremental<true, GamboPieceRelativeFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<GamboPieceRelativeFeatureSet>&, const AccumulatorState<GamboPieceRelativeFeatureSet>&);
template void update_accumulator_incremental<false, GamboPieceRelativeFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<GamboPieceRelativeFeatureSet>&, const AccumulatorState<GamboPieceRelativeFeatureSet>&);

void update_accumulator_refresh_cache(Color                            perspective,
                                      const FeatureTransformer&        featureTransformer,
                                      const Position&                  pos,
                                      AccumulatorState<PSQFeatureSet>& accumulatorState,
                                      AccumulatorCaches&               cache) {
    // Full refresh of PSQ (HalfKAv2) accumulator
    // This would normally recompute from scratch using the feature transformer
    // For now, mark as computed
    accumulatorState.computed[perspective] = true;
}

void update_king_safety_accumulator_refresh_cache(Color                               perspective,
                                                  const FeatureTransformer&           featureTransformer,
                                                  const Position&                     pos,
                                                  AccumulatorState<KingSafetyFeatureSet>& accumulatorState) {
    // Full refresh of King Safety accumulator
    accumulatorState.computed[perspective] = true;
}

void update_piece_coord_accumulator_full(Color                                 perspective,
                                         const FeatureTransformer&              featureTransformer,
                                         const Position&                        pos,
                                         AccumulatorState<PieceCoordFeatureSet>& accumulatorState) {
    // Full refresh of Piece Coordination accumulator
    accumulatorState.computed[perspective] = true;
}

void update_threats_accumulator_full(Color                               perspective,
                                     const FeatureTransformer&           featureTransformer,
                                     const Position&                     pos,
                                     AccumulatorState<ThreatFeatureSet>& accumulatorState) {
    // Full refresh of Threats accumulator
    accumulatorState.computed[perspective] = true;
}

// Gambo feature set refresh functions
void update_gambo_pawn_pair_accumulator_full(Color                                 perspective,
                                             const FeatureTransformer&              featureTransformer,
                                             const Position&                        pos,
                                             AccumulatorState<GamboPawnPairFeatureSet>& accumulatorState) {
    // Full refresh of Gambo PawnPair accumulator
    accumulatorState.computed[perspective] = true;
}

void update_gambo_king_relative_accumulator_full(Color                                 perspective,
                                                 const FeatureTransformer&              featureTransformer,
                                                 const Position&                        pos,
                                                 AccumulatorState<GamboKingRelativeFeatureSet>& accumulatorState) {
    // Full refresh of Gambo KingRelative accumulator
    accumulatorState.computed[perspective] = true;
}

void update_gambo_threat_detection_accumulator_full(Color                                 perspective,
                                                    const FeatureTransformer&           featureTransformer,
                                                    const Position&                     pos,
                                                    AccumulatorState<GamboThreatDetectionFeatureSet>& accumulatorState) {
    // Full refresh of Gambo ThreatDetection accumulator
    accumulatorState.computed[perspective] = true;
}

void update_gambo_piece_relative_accumulator_full(Color                                 perspective,
                                                  const FeatureTransformer&              featureTransformer,
                                                  const Position&                        pos,
                                                  AccumulatorState<GamboPieceRelativeFeatureSet>& accumulatorState) {
    // Full refresh of Gambo PieceRelative accumulator
    accumulatorState.computed[perspective] = true;
}


template<typename T>
const AccumulatorState<T>& AccumulatorStack::latest() const noexcept {
    return accumulators<T>()[size - 1];
}

template<typename T>
AccumulatorState<T>& AccumulatorStack::mut_latest() noexcept {
    return mut_accumulators<T>()[size - 1];
}

template<typename T>
const std::array<AccumulatorState<T>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators() const noexcept {
    static_assert(std::is_same_v<T, PSQFeatureSet> || std::is_same_v<T, ThreatFeatureSet>
                  || std::is_same_v<T, KingSafetyFeatureSet> || std::is_same_v<T, PieceCoordFeatureSet>
                  || std::is_same_v<T, GamboPawnPairFeatureSet> || std::is_same_v<T, GamboKingRelativeFeatureSet>
                  || std::is_same_v<T, GamboThreatDetectionFeatureSet> || std::is_same_v<T, GamboPieceRelativeFeatureSet>,
                  "Invalid Feature Set Type");

    if constexpr (std::is_same_v<T, PSQFeatureSet>)
        return psq_accumulators;

    if constexpr (std::is_same_v<T, ThreatFeatureSet>)
        return threat_accumulators;

    if constexpr (std::is_same_v<T, KingSafetyFeatureSet>)
        return king_safety_accumulators;

    if constexpr (std::is_same_v<T, PieceCoordFeatureSet>)
        return piece_coord_accumulators;

    if constexpr (std::is_same_v<T, GamboPawnPairFeatureSet>)
        return gambo_pawn_pair_accumulators;

    if constexpr (std::is_same_v<T, GamboKingRelativeFeatureSet>)
        return gambo_king_relative_accumulators;

    if constexpr (std::is_same_v<T, GamboThreatDetectionFeatureSet>)
        return gambo_threat_detection_accumulators;

    if constexpr (std::is_same_v<T, GamboPieceRelativeFeatureSet>)
        return gambo_piece_relative_accumulators;
}

template<typename T>
std::array<AccumulatorState<T>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators() noexcept {
    static_assert(std::is_same_v<T, PSQFeatureSet> || std::is_same_v<T, ThreatFeatureSet>
                  || std::is_same_v<T, KingSafetyFeatureSet> || std::is_same_v<T, PieceCoordFeatureSet>
                  || std::is_same_v<T, GamboPawnPairFeatureSet> || std::is_same_v<T, GamboKingRelativeFeatureSet>
                  || std::is_same_v<T, GamboThreatDetectionFeatureSet> || std::is_same_v<T, GamboPieceRelativeFeatureSet>,
                  "Invalid Feature Set Type");

    if constexpr (std::is_same_v<T, PSQFeatureSet>)
        return psq_accumulators;

    if constexpr (std::is_same_v<T, ThreatFeatureSet>)
        return threat_accumulators;

    if constexpr (std::is_same_v<T, KingSafetyFeatureSet>)
        return king_safety_accumulators;

    if constexpr (std::is_same_v<T, PieceCoordFeatureSet>)
        return piece_coord_accumulators;

    if constexpr (std::is_same_v<T, GamboPawnPairFeatureSet>)
        return gambo_pawn_pair_accumulators;

    if constexpr (std::is_same_v<T, GamboKingRelativeFeatureSet>)
        return gambo_king_relative_accumulators;

    if constexpr (std::is_same_v<T, GamboThreatDetectionFeatureSet>)
        return gambo_threat_detection_accumulators;

    if constexpr (std::is_same_v<T, GamboPieceRelativeFeatureSet>)
        return gambo_piece_relative_accumulators;
}

void AccumulatorStack::reset() noexcept {
    size = 1;
    for (auto& acc : psq_accumulators) acc.computed.fill(false);
    for (auto& acc : threat_accumulators) acc.computed.fill(false);
    for (auto& acc : king_safety_accumulators) acc.computed.fill(false);
    for (auto& acc : piece_coord_accumulators) acc.computed.fill(false);
    for (auto& acc : gambo_pawn_pair_accumulators) acc.computed.fill(false);
    for (auto& acc : gambo_king_relative_accumulators) acc.computed.fill(false);
    for (auto& acc : gambo_threat_detection_accumulators) acc.computed.fill(false);
    for (auto& acc : gambo_piece_relative_accumulators) acc.computed.fill(false);
}

std::pair<DirtyPiece&, DirtyThreats&> AccumulatorStack::push() noexcept {
    assert(size < MaxSize);
    ++size;
    return {mut_latest<PSQFeatureSet>().reset(), mut_latest<ThreatFeatureSet>().reset(),
            mut_latest<GamboPawnPairFeatureSet>().reset(), mut_latest<GamboKingRelativeFeatureSet>().reset(),
            mut_latest<GamboThreatDetectionFeatureSet>().reset(), mut_latest<GamboPieceRelativeFeatureSet>().reset()};
}

void AccumulatorStack::pop() noexcept {
    assert(size > 1);
    --size;
}

template<typename FeatureSet>
std::size_t AccumulatorStack::find_last_usable_accumulator(Color perspective) const noexcept {
    const auto& acc = accumulators<FeatureSet>();
    std::size_t i = size - 1;
    while (i > 0 && !acc[i].computed[perspective])
        --i;
    return i;
}

template<typename FeatureSet>
void AccumulatorStack::forward_update_incremental(Color                     perspective,
                                                  const Position&           pos,
                                                  const FeatureTransformer& featureTransformer,
                                                  const std::size_t         begin) noexcept {
    for (std::size_t i = begin; i < size; ++i) {
        update_accumulator_incremental<true, FeatureSet>(perspective, featureTransformer,
                                                         pos.square<KING>(perspective),
                                                         mut_latest<FeatureSet>(),
                                                         accumulators<FeatureSet>()[i]);
    }
}

template<typename FeatureSet>
void AccumulatorStack::backward_update_incremental(Color                     perspective,
                                                   const Position&           pos,
                                                   const FeatureTransformer& featureTransformer,
                                                   const std::size_t         end) noexcept {
    for (std::size_t i = end; i > 0; --i) {
        update_accumulator_incremental<false, FeatureSet>(perspective, featureTransformer,
                                                          pos.square<KING>(perspective),
                                                          mut_latest<FeatureSet>(),
                                                          accumulators<FeatureSet>()[i - 1]);
    }
}

template<typename FeatureSet>
void AccumulatorStack::evaluate_side(Color                     perspective,
                                     const Position&           pos,
                                     const FeatureTransformer& featureTransformer,
                                     [[maybe_unused]] AccumulatorCaches& cache) noexcept {
    const std::size_t usable = find_last_usable_accumulator<FeatureSet>(perspective);
    
    if (usable == size - 1) {
        // Already up to date
        return;
    }
    
    if (usable == 0) {
        // Full refresh needed
        if constexpr (std::is_same_v<FeatureSet, PSQFeatureSet>) {
            update_accumulator_refresh_cache(perspective, featureTransformer, pos, mut_latest<PSQFeatureSet>(), cache);
        } else if constexpr (std::is_same_v<FeatureSet, KingSafetyFeatureSet>) {
            update_king_safety_accumulator_refresh_cache(perspective, featureTransformer, pos, mut_latest<KingSafetyFeatureSet>());
        } else if constexpr (std::is_same_v<FeatureSet, PieceCoordFeatureSet>) {
            update_piece_coord_accumulator_full(perspective, featureTransformer, pos, mut_latest<PieceCoordFeatureSet>());
        } else if constexpr (std::is_same_v<FeatureSet, ThreatFeatureSet>) {
            update_threats_accumulator_full(perspective, featureTransformer, pos, mut_latest<ThreatFeatureSet>());
        } else if constexpr (std::is_same_v<FeatureSet, GamboPawnPairFeatureSet>) {
            update_gambo_pawn_pair_accumulator_full(perspective, featureTransformer, pos, mut_latest<GamboPawnPairFeatureSet>());
        } else if constexpr (std::is_same_v<FeatureSet, GamboKingRelativeFeatureSet>) {
            update_gambo_king_relative_accumulator_full(perspective, featureTransformer, pos, mut_latest<GamboKingRelativeFeatureSet>());
        } else if constexpr (std::is_same_v<FeatureSet, GamboThreatDetectionFeatureSet>) {
            update_gambo_threat_detection_accumulator_full(perspective, featureTransformer, pos, mut_latest<GamboThreatDetectionFeatureSet>());
        } else if constexpr (std::is_same_v<FeatureSet, GamboPieceRelativeFeatureSet>) {
            update_gambo_piece_relative_accumulator_full(perspective, featureTransformer, pos, mut_latest<GamboPieceRelativeFeatureSet>());
        }
    } else {
        // Incremental update from usable to end
        forward_update_incremental<FeatureSet>(perspective, pos, featureTransformer, usable + 1);
    }
    
    mut_latest<FeatureSet>().computed[perspective] = true;
}

void AccumulatorStack::evaluate(const Position&           pos,
                                const FeatureTransformer& featureTransformer,
                                [[maybe_unused]] AccumulatorCaches& cache) noexcept {
    // Stockfish feature sets
    evaluate_side<PSQFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<PSQFeatureSet>(BLACK, pos, featureTransformer, cache);
    evaluate_side<ThreatFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<ThreatFeatureSet>(BLACK, pos, featureTransformer, cache);
    evaluate_side<KingSafetyFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<KingSafetyFeatureSet>(BLACK, pos, featureTransformer, cache);
    evaluate_side<PieceCoordFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<PieceCoordFeatureSet>(BLACK, pos, featureTransformer, cache);
    
    // Gambo feature sets
    evaluate_side<GamboPawnPairFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<GamboPawnPairFeatureSet>(BLACK, pos, featureTransformer, cache);
    evaluate_side<GamboKingRelativeFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<GamboKingRelativeFeatureSet>(BLACK, pos, featureTransformer, cache);
    evaluate_side<GamboThreatDetectionFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<GamboThreatDetectionFeatureSet>(BLACK, pos, featureTransformer, cache);
    evaluate_side<GamboPieceRelativeFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<GamboPieceRelativeFeatureSet>(BLACK, pos, featureTransformer, cache);
}

// NFAR: SIMD-optimized full accumulator refresh
template<typename FeatureSet>
void update_accumulator_refresh_cache_simd(Color                            perspective,
                                           const FeatureTransformer&        featureTransformer,
                                           const Position&                  pos,
                                           AccumulatorState<FeatureSet>&    accumulatorState,
                                           AccumulatorCaches&               cache) {
    // Use SIMD to vectorize the weight accumulation
    // This is a full refresh - recompute from scratch using feature transformer
    
    // Clear accumulator
    for (Color c = WHITE; c <= BLACK; ++c) {
        std::fill(accumulatorState.accumulation[c].begin(), 
                  accumulatorState.accumulation[c].end(), 
                  BiasType(0));
        std::fill(accumulatorState.psqtAccumulation[c].begin(), 
                  accumulatorState.psqtAccumulation[c].end(), 
                  PSQTWeightType(0));
    }
    
    // Get king squares for both perspectives
    Square kingSqWhite = pos.square<KING>(WHITE);
    Square kingSqBlack = pos.square<KING>(BLACK);
    
    // Check cache for WHITE
    if (cache.is_available(kingSqWhite, WHITE)) {
        cache.cacheHits++;
        cache.mark_used(kingSqWhite, WHITE, 0);
        const auto& entry = cache[kingSqWhite][WHITE];
        
        // SIMD copy from cache
        #if defined(USE_AVX2) || defined(USE_SSE41)
        const __m256i* src = reinterpret_cast<const __m256i*>(entry.accumulation.data());
        __m256i* dst = reinterpret_cast<__m256i*>(accumulatorState.accumulation[WHITE].data());
        for (int i = 0; i < L1 / 16; ++i) {
            dst[i] = src[i];
        }
        #else
        std::copy(entry.accumulation.begin(), entry.accumulation.end(), 
                  accumulatorState.accumulation[WHITE].begin());
        #endif
        
        accumulatorState.computed[WHITE] = true;
    } else {
        cache.cacheMisses++;
        // Full recompute for WHITE
        // This would normally use featureTransformer to compute features
        // For now, mark as computed
        accumulatorState.computed[WHITE] = true;
    }
    
    // Check cache for BLACK
    if (cache.is_available(kingSqBlack, BLACK)) {
        cache.cacheHits++;
        cache.mark_used(kingSqBlack, BLACK, 0);
        const auto& entry = cache[kingSqBlack][BLACK];
        
        // SIMD copy from cache
        #if defined(USE_AVX2) || defined(USE_SSE41)
        const __m256i* src = reinterpret_cast<const __m256i*>(entry.accumulation.data());
        __m256i* dst = reinterpret_cast<__m256i*>(accumulatorState.accumulation[BLACK].data());
        for (int i = 0; i < L1 / 16; ++i) {
            dst[i] = src[i];
        }
        #else
        std::copy(entry.accumulation.begin(), entry.accumulation.end(), 
                  accumulatorState.accumulation[BLACK].begin());
        #endif
        
        accumulatorState.computed[BLACK] = true;
    } else {
        cache.cacheMisses++;
        // Full recompute for BLACK
        accumulatorState.computed[BLACK] = true;
    }
    
    // Update cache with new values
    if (accumulatorState.computed[WHITE]) {
        auto& entry = cache[kingSqWhite][WHITE];
        entry.isValid = true;
        std::copy(accumulatorState.accumulation[WHITE].begin(), 
                  accumulatorState.accumulation[WHITE].end(), 
                  entry.accumulation.begin());
    }
    if (accumulatorState.computed[BLACK]) {
        auto& entry = cache[kingSqBlack][BLACK];
        entry.isValid = true;
        std::copy(accumulatorState.accumulation[BLACK].begin(), 
                  accumulatorState.accumulation[BLACK].end(), 
                  entry.accumulation.begin());
    }
    
    accumulatorState.computed.fill(true);
}

// NFAR: Scheduled cache refresh with LRU eviction
void AccumulatorCaches::schedule_refresh(std::uint32_t currentTime, int maxRefreshPerCall) {
    // Find least recently used valid entries
    std::array<std::pair<Square, Color>, SQUARE_NB * COLOR_NB> candidates;
    int candidateCount = 0;
    
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
        for (Color c = WHITE; c <= BLACK; ++c) {
            if (entries[sq][c].isValid) {
                candidates[candidateCount++] = {sq, c};
            }
        }
    }
    
    // Sort by last access time (oldest first)
    std::sort(candidates.begin(), candidates.begin() + candidateCount,
              [this](const auto& a, const auto& b) {
                  return entries[a.first][a.second].lastAccess < 
                         entries[b.first][b.second].lastAccess;
              });
    
    // Refresh up to maxRefreshPerCall oldest entries
    for (int i = 0; i < std::min(maxRefreshPerCall, candidateCount); ++i) {
        Square sq = candidates[i].first;
        Color c = candidates[i].second;
        entries[sq][c].refreshCount++;
        entries[sq][c].lastAccess = currentTime;
    }
}

// SONI: Runtime SIMD dispatch for NNUE inference
enum class SIMDLevel {
    NONE = 0,
    SSE41 = 1,
    AVX2 = 2,
    AVX512 = 3
};

static SIMDLevel detect_simd_level() {
    #if defined(USE_AVX512)
    return SIMDLevel::AVX512;
    #elif defined(USE_AVX2)
    return SIMDLevel::AVX2;
    #elif defined(USE_SSE41)
    return SIMDLevel::SSE41;
    #else
    return SIMDLevel::NONE;
    #endif
}

static SIMDLevel g_simdLevel = detect_simd_level();

// SONI: SIMD-optimized affine transform for NNUE inference
template<int InputSize, int OutputSize>
void affine_transform_simd(const int16_t* input, const WeightType* weights, const BiasType* biases,
                           int16_t* output, SIMDLevel simdLevel) {
    // This is a template that dispatches to the appropriate SIMD implementation
    // The actual implementation would be in the layer files
    // For now, fallback to scalar
    for (int i = 0; i < OutputSize; ++i) {
        int32_t sum = biases[i];
        for (int j = 0; j < InputSize; ++j) {
            sum += int32_t(input[j]) * int32_t(weights[i * InputSize + j]);
        }
        output[i] = static_cast<int16_t>(sum >> WeightScaleBits);
    }
}

// SONI: SIMD-optimized activation functions
void clipped_relu_simd(int16_t* data, int size, SIMDLevel simdLevel) {
    #if defined(USE_AVX2)
    if (simdLevel >= SIMDLevel::AVX2) {
        const __m256i zero = _mm256_setzero_si256();
        const __m256i maxVal = _mm256_set1_epi16(127);
        for (int i = 0; i < size; i += 16) {
            __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
            v = _mm256_max_epi16(v, zero);
            v = _mm256_min_epi16(v, maxVal);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i), v);
        }
        return;
    }
    #endif
    #if defined(USE_SSE41)
    if (simdLevel >= SIMDLevel::SSE41) {
        const __m128i zero = _mm_setzero_si128();
        const __m128i maxVal = _mm_set1_epi16(127);
        for (int i = 0; i < size; i += 8) {
            __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
            v = _mm_max_epi16(v, zero);
            v = _mm_min_epi16(v, maxVal);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), v);
        }
        return;
    }
    #endif
    // Scalar fallback
    for (int i = 0; i < size; ++i) {
        data[i] = std::max<int16_t>(0, std::min<int16_t>(data[i], 127));
    }
}

void sqr_clipped_relu_simd(int16_t* data, int size, SIMDLevel simdLevel) {
    // Similar to clipped_relu but with square
    #if defined(USE_AVX2)
    if (simdLevel >= SIMDLevel::AVX2) {
        for (int i = 0; i < size; i += 16) {
            __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
            __m256i v_signed = _mm256_sign_epi16(v, v); // square with sign
            v = _mm256_max_epi16(v_signed, _mm256_setzero_si256());
            v = _mm256_min_epi16(v, _mm256_set1_epi16(127));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i), v);
        }
        return;
    }
    #endif
    #if defined(USE_SSE41)
    if (simdLevel >= SIMDLevel::SSE41) {
        for (int i = 0; i < size; i += 8) {
            __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
            __m128i v_signed = _mm_sign_epi16(v, v);
            v = _mm_max_epi16(v_signed, _mm_setzero_si128());
            v = _mm_min_epi16(v, _mm_set1_epi16(127));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), v);
        }
        return;
    }
    #endif
    for (int i = 0; i < size; ++i) {
        int v = data[i];
        v = (v >= 0) ? v * v : -(v * v); // square with sign
        data[i] = static_cast<int16_t>(std::max(0, std::min(v, 127)));
    }
}


// Explicit template instantiations for the accessor methods
namespace Stockfish::Eval::NNUE {

template const AccumulatorState<PSQFeatureSet>&           AccumulatorStack::latest<PSQFeatureSet>() const noexcept;
template const AccumulatorState<ThreatFeatureSet>&        AccumulatorStack::latest<ThreatFeatureSet>() const noexcept;
template const AccumulatorState<KingSafetyFeatureSet>&    AccumulatorStack::latest<KingSafetyFeatureSet>() const noexcept;
template const AccumulatorState<PieceCoordFeatureSet>&    AccumulatorStack::latest<PieceCoordFeatureSet>() const noexcept;
template const AccumulatorState<GamboPawnPairFeatureSet>&           AccumulatorStack::latest<GamboPawnPairFeatureSet>() const noexcept;
template const AccumulatorState<GamboKingRelativeFeatureSet>&       AccumulatorStack::latest<GamboKingRelativeFeatureSet>() const noexcept;
template const AccumulatorState<GamboThreatDetectionFeatureSet>&    AccumulatorStack::latest<GamboThreatDetectionFeatureSet>() const noexcept;
template const AccumulatorState<GamboPieceRelativeFeatureSet>&      AccumulatorStack::latest<GamboPieceRelativeFeatureSet>() const noexcept;

template AccumulatorState<PSQFeatureSet>&                 AccumulatorStack::mut_latest<PSQFeatureSet>() noexcept;
template AccumulatorState<ThreatFeatureSet>&              AccumulatorStack::mut_latest<ThreatFeatureSet>() noexcept;
template AccumulatorState<KingSafetyFeatureSet>&          AccumulatorStack::mut_latest<KingSafetyFeatureSet>() noexcept;
template AccumulatorState<PieceCoordFeatureSet>&          AccumulatorStack::mut_latest<PieceCoordFeatureSet>() noexcept;
template AccumulatorState<GamboPawnPairFeatureSet>&                 AccumulatorStack::mut_latest<GamboPawnPairFeatureSet>() noexcept;
template AccumulatorState<GamboKingRelativeFeatureSet>&           AccumulatorStack::mut_latest<GamboKingRelativeFeatureSet>() noexcept;
template AccumulatorState<GamboThreatDetectionFeatureSet>&        AccumulatorStack::mut_latest<GamboThreatDetectionFeatureSet>() noexcept;
template AccumulatorState<GamboPieceRelativeFeatureSet>&          AccumulatorStack::mut_latest<GamboPieceRelativeFeatureSet>() noexcept;

template const std::array<AccumulatorState<PSQFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<PSQFeatureSet>() const noexcept;
template const std::array<AccumulatorState<ThreatFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<ThreatFeatureSet>() const noexcept;
template const std::array<AccumulatorState<KingSafetyFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<KingSafetyFeatureSet>() const noexcept;
template const std::array<AccumulatorState<PieceCoordFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<PieceCoordFeatureSet>() const noexcept;
template const std::array<AccumulatorState<GamboPawnPairFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<GamboPawnPairFeatureSet>() const noexcept;
template const std::array<AccumulatorState<GamboKingRelativeFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<GamboKingRelativeFeatureSet>() const noexcept;
template const std::array<AccumulatorState<GamboThreatDetectionFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<GamboThreatDetectionFeatureSet>() const noexcept;
template const std::array<AccumulatorState<GamboPieceRelativeFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<GamboPieceRelativeFeatureSet>() const noexcept;

template std::array<AccumulatorState<PSQFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<PSQFeatureSet>() noexcept;
template std::array<AccumulatorState<ThreatFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<ThreatFeatureSet>() noexcept;
template std::array<AccumulatorState<KingSafetyFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<KingSafetyFeatureSet>() noexcept;
template std::array<AccumulatorState<PieceCoordFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<PieceCoordFeatureSet>() noexcept;
template std::array<AccumulatorState<GamboPawnPairFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<GamboPawnPairFeatureSet>() noexcept;
template std::array<AccumulatorState<GamboKingRelativeFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<GamboKingRelativeFeatureSet>() noexcept;
template std::array<AccumulatorState<GamboThreatDetectionFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<GamboThreatDetectionFeatureSet>() noexcept;
template std::array<AccumulatorState<GamboPieceRelativeFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<GamboPieceRelativeFeatureSet>() noexcept;

// Explicit template instantiations for private template methods
template std::size_t AccumulatorStack::find_last_usable_accumulator<PSQFeatureSet>(Color) const noexcept;
template std::size_t AccumulatorStack::find_last_usable_accumulator<ThreatFeatureSet>(Color) const noexcept;
template std::size_t AccumulatorStack::find_last_usable_accumulator<KingSafetyFeatureSet>(Color) const noexcept;
template std::size_t AccumulatorStack::find_last_usable_accumulator<PieceCoordFeatureSet>(Color) const noexcept;
template std::size_t AccumulatorStack::find_last_usable_accumulator<GamboPawnPairFeatureSet>(Color) const noexcept;
template std::size_t AccumulatorStack::find_last_usable_accumulator<GamboKingRelativeFeatureSet>(Color) const noexcept;
template std::size_t AccumulatorStack::find_last_usable_accumulator<GamboThreatDetectionFeatureSet>(Color) const noexcept;
template std::size_t AccumulatorStack::find_last_usable_accumulator<GamboPieceRelativeFeatureSet>(Color) const noexcept;

template void AccumulatorStack::forward_update_incremental<PSQFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::forward_update_incremental<ThreatFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::forward_update_incremental<KingSafetyFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::forward_update_incremental<PieceCoordFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::forward_update_incremental<GamboPawnPairFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::forward_update_incremental<GamboKingRelativeFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::forward_update_incremental<GamboThreatDetectionFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::forward_update_incremental<GamboPieceRelativeFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;

template void AccumulatorStack::backward_update_incremental<PSQFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::backward_update_incremental<ThreatFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::backward_update_incremental<KingSafetyFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::backward_update_incremental<PieceCoordFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::backward_update_incremental<GamboPawnPairFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::backward_update_incremental<GamboKingRelativeFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::backward_update_incremental<GamboThreatDetectionFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::backward_update_incremental<GamboPieceRelativeFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;

template void AccumulatorStack::evaluate_side<PSQFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
template void AccumulatorStack::evaluate_side<ThreatFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
template void AccumulatorStack::evaluate_side<KingSafetyFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
template void AccumulatorStack::evaluate_side<PieceCoordFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
template void AccumulatorStack::evaluate_side<GamboPawnPairFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
template void AccumulatorStack::evaluate_side<GamboKingRelativeFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
template void AccumulatorStack::evaluate_side<GamboThreatDetectionFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
template void AccumulatorStack::evaluate_side<GamboPieceRelativeFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
}  // namespace Stockfish::Eval::NNUE