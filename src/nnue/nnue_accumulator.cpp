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
                                    const AccumulatorState<FeatureSet>& computed);

void update_accumulator_refresh_cache(Color                            perspective,
                                      const FeatureTransformer&        featureTransformer,
                                      const Position&                  pos,
                                      AccumulatorState<PSQFeatureSet>& accumulatorState,
                                      AccumulatorCaches&               cache);

void update_king_safety_accumulator_refresh_cache(Color                               perspective,
                                                  const FeatureTransformer&           featureTransformer,
                                                  const Position&                     pos,
                                                  AccumulatorState<KingSafetyFeatureSet>& accumulatorState);

void update_piece_coord_accumulator_full(Color                                 perspective,
                                         const FeatureTransformer&              featureTransformer,
                                         const Position&                        pos,
                                         AccumulatorState<PieceCoordFeatureSet>& accumulatorState);

void update_threats_accumulator_full(Color                               perspective,
                                     const FeatureTransformer&           featureTransformer,
                                     const Position&                     pos,
                                     AccumulatorState<ThreatFeatureSet>& accumulatorState);
}  // namespace

namespace Stockfish::Eval::NNUE {

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
}  // namespace Stockfish::Eval::NNUE

// Explicit template instantiations
namespace Stockfish::Eval::NNUE {

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
}  // namespace Stockfish::Eval::NNUE

// Explicit template instantiations
namespace Stockfish::Eval::NNUE {

template <> const AccumulatorState<PSQFeatureSet>&           AccumulatorStack::latest<PSQFeatureSet>() const noexcept;
template <> const AccumulatorState<ThreatFeatureSet>&        AccumulatorStack::latest<ThreatFeatureSet>() const noexcept;
template <> const AccumulatorState<KingSafetyFeatureSet>&    AccumulatorStack::latest<KingSafetyFeatureSet>() const noexcept;
template <> const AccumulatorState<PieceCoordFeatureSet>&    AccumulatorStack::latest<PieceCoordFeatureSet>() const noexcept;

template <> AccumulatorState<PSQFeatureSet>&    AccumulatorStack::mut_latest<PSQFeatureSet>() noexcept;
template <> AccumulatorState<ThreatFeatureSet>& AccumulatorStack::mut_latest<ThreatFeatureSet>() noexcept;
template <> AccumulatorState<KingSafetyFeatureSet>& AccumulatorStack::mut_latest<KingSafetyFeatureSet>() noexcept;
template <> AccumulatorState<PieceCoordFeatureSet>& AccumulatorStack::mut_latest<PieceCoordFeatureSet>() noexcept;

template <> const std::array<AccumulatorState<PSQFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<PSQFeatureSet>() const noexcept;
template <> const std::array<AccumulatorState<ThreatFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<ThreatFeatureSet>() const noexcept;
template <> const std::array<AccumulatorState<KingSafetyFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<KingSafetyFeatureSet>() const noexcept;
template <> const std::array<AccumulatorState<PieceCoordFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::accumulators<PieceCoordFeatureSet>() const noexcept;

template <> std::array<AccumulatorState<PSQFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<PSQFeatureSet>() noexcept;
template <> std::array<AccumulatorState<ThreatFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<ThreatFeatureSet>() noexcept;
template <> std::array<AccumulatorState<KingSafetyFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<KingSafetyFeatureSet>() noexcept;
template <> std::array<AccumulatorState<PieceCoordFeatureSet>, AccumulatorStack::MaxSize>&
AccumulatorStack::mut_accumulators<PieceCoordFeatureSet>() noexcept;
}  // namespace Stockfish::Eval::NNUE
