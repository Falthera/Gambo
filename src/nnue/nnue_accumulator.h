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

// Class for difference calculation of NNUE evaluation function

#ifndef NNUE_ACCUMULATOR_H_INCLUDED
#define NNUE_ACCUMULATOR_H_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "../types.h"
#include "nnue_architecture.h"
#include "nnue_common.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE {

struct alignas(CacheLineSize) Accumulator;

class FeatureTransformer;

// Class that holds the result of affine transformation of input features
struct alignas(CacheLineSize) Accumulator {
    std::array<std::array<std::int16_t, L1>, COLOR_NB>          accumulation;
    std::array<std::array<std::int32_t, PSQTBuckets>, COLOR_NB> psqtAccumulation;
    std::array<bool, COLOR_NB>                                  computed = {};
};


// AccumulatorCaches struct provides per-thread accumulator caches, where each
// cache contains multiple entries for each of the possible king squares.
// When the accumulator needs to be refreshed, the cached entry is used to more
// efficiently update the accumulator, instead of rebuilding it from scratch.
// This idea, was first described by Luecx (author of Koivisto) and
// is commonly referred to as "Finny Tables".
struct AccumulatorCaches {
    template<typename Network>
    AccumulatorCaches(const Network& network) {
        clear(network);
    }

    struct alignas(CacheLineSize) Entry {
        std::array<BiasType, L1>                accumulation;
        std::array<PSQTWeightType, PSQTBuckets> psqtAccumulation;
        std::array<Piece, SQUARE_NB>            pieces;
        Bitboard                                pieceBB;
        // NFAR: Cache metadata for refresh scheduling
        std::uint32_t                           lastAccess = 0;
        std::uint32_t                           refreshCount = 0;
        bool                                    isValid = false;

        // To initialize a refresh entry, we set all its bitboards empty,
        // so we put the biases in the accumulation, without any weights on top
        void clear(const std::array<BiasType, L1>& biases) {
            accumulation = biases;
            std::memset(reinterpret_cast<std::byte*>(this) + offsetof(Entry, psqtAccumulation), 0,
                        sizeof(Entry) - offsetof(Entry, psqtAccumulation));
            lastAccess = 0;
            refreshCount = 0;
            isValid = false;
        }
    };

    template<typename Network>
    void clear(const Network& network) {
        for (auto& entries1D : entries)
            for (auto& entry : entries1D)
                entry.clear(network.featureTransformer.biases);
    }

    // NFAR: Get cache entry with availability tracking
    std::array<Entry, COLOR_NB>& operator[](Square sq) { 
        return entries[sq]; 
    }

    // NFAR: Check if cached entry is available for given perspective
    [[nodiscard]] bool is_available(Square sq, Color perspective) const {
        return entries[sq][perspective].isValid;
    }

    // NFAR: Mark cache entry as used (updates LRU timestamp)
    void mark_used(Square sq, Color perspective, std::uint32_t timestamp) {
        auto& entry = entries[sq][perspective];
        entry.lastAccess = timestamp;
    }

    // NFAR: Schedule refresh for least recently used entries
    void schedule_refresh(std::uint32_t currentTime, int maxRefreshPerCall = 4) {
        // Find least recently used valid entries
        std::array<std::pair<Square, Color>, SQUARE_NB * COLOR_NB> candidates;
        int candidateCount = 0;
        
        for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
            for (int c = 0; c < COLOR_NB; ++c) {
                Color color = Color(c);
                if (entries[sq][color].isValid) {
                    candidates[candidateCount++] = {sq, color};
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

    // NFAR: Statistics
    std::uint64_t cacheHits = 0;
    std::uint64_t cacheMisses = 0;
    std::uint64_t totalRefreshes = 0;

    std::array<std::array<Entry, COLOR_NB>, SQUARE_NB> entries;
};


template<typename FeatureSet>
struct AccumulatorState: public Accumulator {
    typename FeatureSet::DiffType diff;

    void reset(const typename FeatureSet::DiffType& dp) noexcept {
        diff = dp;
        computed.fill(false);
    }

    typename FeatureSet::DiffType& reset() noexcept {
        computed.fill(false);
        return diff;
    }
};

class AccumulatorStack {
   public:
    static constexpr std::size_t MaxSize = MAX_PLY + 1;

    template<typename T>
    [[nodiscard]] const AccumulatorState<T>& latest() const noexcept;

    void                                  reset() noexcept;
    std::pair<DirtyPiece&, DirtyThreats&> push() noexcept;
    void                                  pop() noexcept;

    void evaluate(const Position&           pos,
                  const FeatureTransformer& featureTransformer,
                  // Silence spurious warning on GCC 10
                  [[maybe_unused]] AccumulatorCaches& cache) noexcept;

   private:
    template<typename T>
    [[nodiscard]] AccumulatorState<T>& mut_latest() noexcept;

    template<typename T>
    [[nodiscard]] const std::array<AccumulatorState<T>, MaxSize>& accumulators() const noexcept;

    template<typename T>
    [[nodiscard]] std::array<AccumulatorState<T>, MaxSize>& mut_accumulators() noexcept;

    template<typename FeatureSet>
    void evaluate_side(Color                     perspective,
                       const Position&           pos,
                       const FeatureTransformer& featureTransformer,
                       // Silence spurious warning on GCC 10
                       [[maybe_unused]] AccumulatorCaches& cache) noexcept;

    template<typename FeatureSet>
    [[nodiscard]] std::size_t find_last_usable_accumulator(Color perspective) const noexcept;

    template<typename FeatureSet>
    void forward_update_incremental(Color                     perspective,
                                    const Position&           pos,
                                    const FeatureTransformer& featureTransformer,
                                    const std::size_t         begin) noexcept;

    template<typename FeatureSet>
    void backward_update_incremental(Color                     perspective,
                                     const Position&           pos,
                                     const FeatureTransformer& featureTransformer,
                                     const std::size_t         end) noexcept;

    std::array<AccumulatorState<PSQFeatureSet>, MaxSize>          psq_accumulators;
    std::array<AccumulatorState<ThreatFeatureSet>, MaxSize>       threat_accumulators;
    std::array<AccumulatorState<KingSafetyFeatureSet>, MaxSize>   king_safety_accumulators;
    std::array<AccumulatorState<PieceCoordFeatureSet>, MaxSize>   piece_coord_accumulators;
    // Gambo feature accumulators
    std::array<AccumulatorState<GamboPawnPairFeatureSet>, MaxSize>       gambo_pawn_pair_accumulators;
    std::array<AccumulatorState<GamboKingRelativeFeatureSet>, MaxSize>   gambo_king_relative_accumulators;
    std::array<AccumulatorState<GamboThreatDetectionFeatureSet>, MaxSize> gambo_threat_detection_accumulators;
    std::array<AccumulatorState<GamboPieceRelativeFeatureSet>, MaxSize>   gambo_piece_relative_accumulators;
    std::size_t                                                   size = 1;
};

}  // namespace Stockfish::Eval::NNUE

#endif  // NNUE_ACCUMULATOR_H_INCLUDED
