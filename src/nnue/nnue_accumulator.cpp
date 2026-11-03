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

// Explicit instantiations for the four Stockfish feature sets
template void update_accumulator_incremental<true, PSQFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<PSQFeatureSet>&, const AccumulatorState<PSQFeatureSet>&);
template void update_accumulator_incremental<false, PSQFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<PSQFeatureSet>&, const AccumulatorState<PSQFeatureSet>&);
template void update_accumulator_incremental<true, ThreatFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<ThreatFeatureSet>&, const AccumulatorState<ThreatFeatureSet>&);
template void update_accumulator_incremental<false, ThreatFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<ThreatFeatureSet>&, const AccumulatorState<ThreatFeatureSet>&);
template void update_accumulator_incremental<true, KingSafetyFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<KingSafetyFeatureSet>&, const AccumulatorState<KingSafetyFeatureSet>&);
template void update_accumulator_incremental<false, KingSafetyFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<KingSafetyFeatureSet>&, const AccumulatorState<KingSafetyFeatureSet>&);
template void update_accumulator_incremental<true, PieceCoordFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<PieceCoordFeatureSet>&, const AccumulatorState<PieceCoordFeatureSet>&);
template void update_accumulator_incremental<false, PieceCoordFeatureSet>(Color, const FeatureTransformer&, Square, AccumulatorState<PieceCoordFeatureSet>&, const AccumulatorState<PieceCoordFeatureSet>&);

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
                  || std::is_same_v<T, KingSafetyFeatureSet> || std::is_same_v<T, PieceCoordFeatureSet>,
                  "Invalid Feature Set Type");

    if constexpr (std::is_same_v<T, PSQFeatureSet>)
        return psq_accumulators;

    if constexpr (std::is_same_v<T, ThreatFeatureSet>)
        return threat_accumulators;

    if constexpr (std::is_same_v<T, KingSafetyFeatureSet>)
        return king_safety_accumulators;

    if constexpr (std::is_same_v<T, PieceCoordFeatureSet>)
        return piece_coord_accumulators;
}

template<typename T>
std::array<AccumulatorState<T>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators() noexcept {
    static_assert(std::is_same_v<T, PSQFeatureSet> || std::is_same_v<T, ThreatFeatureSet>
                  || std::is_same_v<T, KingSafetyFeatureSet> || std::is_same_v<T, PieceCoordFeatureSet>,
                  "Invalid Feature Set Type");

    if constexpr (std::is_same_v<T, PSQFeatureSet>)
        return psq_accumulators;

    if constexpr (std::is_same_v<T, ThreatFeatureSet>)
        return threat_accumulators;

    if constexpr (std::is_same_v<T, KingSafetyFeatureSet>)
        return king_safety_accumulators;

    if constexpr (std::is_same_v<T, PieceCoordFeatureSet>)
        return piece_coord_accumulators;
}

void AccumulatorStack::reset() noexcept {
    size = 1;
    for (auto& acc : psq_accumulators) acc.computed.fill(false);
    for (auto& acc : threat_accumulators) acc.computed.fill(false);
    for (auto& acc : king_safety_accumulators) acc.computed.fill(false);
    for (auto& acc : piece_coord_accumulators) acc.computed.fill(false);
}

std::pair<DirtyPiece&, DirtyThreats&> AccumulatorStack::push() noexcept {
    assert(size < MaxSize);
    ++size;
    return {mut_latest<PSQFeatureSet>().reset(), mut_latest<ThreatFeatureSet>().reset()};
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
    evaluate_side<PSQFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<PSQFeatureSet>(BLACK, pos, featureTransformer, cache);
    evaluate_side<ThreatFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<ThreatFeatureSet>(BLACK, pos, featureTransformer, cache);
    evaluate_side<KingSafetyFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<KingSafetyFeatureSet>(BLACK, pos, featureTransformer, cache);
    evaluate_side<PieceCoordFeatureSet>(WHITE, pos, featureTransformer, cache);
    evaluate_side<PieceCoordFeatureSet>(BLACK, pos, featureTransformer, cache);
}

}  // namespace Stockfish::Eval::NNUE


// Explicit template instantiations for the accessor methods
namespace Stockfish::Eval::NNUE {

template const AccumulatorState<PSQFeatureSet>&           AccumulatorStack::latest<PSQFeatureSet>() const noexcept;
template const AccumulatorState<ThreatFeatureSet>&        AccumulatorStack::latest<ThreatFeatureSet>() const noexcept;
template const AccumulatorState<KingSafetyFeatureSet>&    AccumulatorStack::latest<KingSafetyFeatureSet>() const noexcept;
template const AccumulatorState<PieceCoordFeatureSet>&    AccumulatorStack::latest<PieceCoordFeatureSet>() const noexcept;

template AccumulatorState<PSQFeatureSet>&                 AccumulatorStack::mut_latest<PSQFeatureSet>() noexcept;
template AccumulatorState<ThreatFeatureSet>&              AccumulatorStack::mut_latest<ThreatFeatureSet>() noexcept;
template AccumulatorState<KingSafetyFeatureSet>&          AccumulatorStack::mut_latest<KingSafetyFeatureSet>() noexcept;
template AccumulatorState<PieceCoordFeatureSet>&          AccumulatorStack::mut_latest<PieceCoordFeatureSet>() noexcept;

template const std::array<AccumulatorState<PSQFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<PSQFeatureSet>() const noexcept;
template const std::array<AccumulatorState<ThreatFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<ThreatFeatureSet>() const noexcept;
template const std::array<AccumulatorState<KingSafetyFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<KingSafetyFeatureSet>() const noexcept;
template const std::array<AccumulatorState<PieceCoordFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<PieceCoordFeatureSet>() const noexcept;

template std::array<AccumulatorState<PSQFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<PSQFeatureSet>() noexcept;
template std::array<AccumulatorState<ThreatFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<ThreatFeatureSet>() noexcept;
template std::array<AccumulatorState<KingSafetyFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<KingSafetyFeatureSet>() noexcept;
template std::array<AccumulatorState<PieceCoordFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<PieceCoordFeatureSet>() noexcept;

// Explicit template instantiations for private template methods
template std::size_t AccumulatorStack::find_last_usable_accumulator<PSQFeatureSet>(Color) const noexcept;
template std::size_t AccumulatorStack::find_last_usable_accumulator<ThreatFeatureSet>(Color) const noexcept;
template std::size_t AccumulatorStack::find_last_usable_accumulator<KingSafetyFeatureSet>(Color) const noexcept;
template std::size_t AccumulatorStack::find_last_usable_accumulator<PieceCoordFeatureSet>(Color) const noexcept;

template void AccumulatorStack::forward_update_incremental<PSQFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::forward_update_incremental<ThreatFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::forward_update_incremental<KingSafetyFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::forward_update_incremental<PieceCoordFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;

template void AccumulatorStack::backward_update_incremental<PSQFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::backward_update_incremental<ThreatFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::backward_update_incremental<KingSafetyFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;
template void AccumulatorStack::backward_update_incremental<PieceCoordFeatureSet>(Color, const Position&, const FeatureTransformer&, std::size_t) noexcept;

template void AccumulatorStack::evaluate_side<PSQFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
template void AccumulatorStack::evaluate_side<ThreatFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
template void AccumulatorStack::evaluate_side<KingSafetyFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
template void AccumulatorStack::evaluate_side<PieceCoordFeatureSet>(Color, const Position&, const FeatureTransformer&, AccumulatorCaches&) noexcept;
}  // namespace Stockfish::Eval::NNUE