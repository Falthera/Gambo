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

// A class that converts the input features of the NNUE evaluation function

#ifndef NNUE_FEATURE_TRANSFORMER_H_INCLUDED
#define NNUE_FEATURE_TRANSFORMER_H_INCLUDED

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <iterator>

#include "../position.h"
#include "../types.h"
#include "nnue_accumulator.h"
#include "nnue_architecture.h"
#include "nnue_common.h"
#include "simd.h"

namespace Stockfish::Eval::NNUE {

// Returns the inverse of a permutation
template<std::size_t Len>
constexpr std::array<std::size_t, Len>
invert_permutation(const std::array<std::size_t, Len>& order) {
    std::array<std::size_t, Len> inverse{};
    for (std::size_t i = 0; i < order.size(); i++)
        inverse[order[i]] = i;
    return inverse;
}

// Divide a byte region of size TotalSize to chunks of size
// BlockSize, and permute the blocks by a given order
template<std::size_t BlockSize, typename T, std::size_t N, std::size_t OrderSize>
void permute(std::array<T, N>& data, const std::array<std::size_t, OrderSize>& order) {
    constexpr std::size_t TotalSize = N * sizeof(T);

    static_assert(TotalSize % (BlockSize * OrderSize) == 0,
                  "ChunkSize * OrderSize must perfectly divide TotalSize");

    constexpr std::size_t ProcessChunkSize = BlockSize * OrderSize;

    std::array<std::byte, ProcessChunkSize> buffer{};

    std::byte* const bytes = reinterpret_cast<std::byte*>(data.data());

    for (std::size_t i = 0; i < TotalSize; i += ProcessChunkSize)
    {
        std::byte* const values = &bytes[i];

        for (std::size_t j = 0; j < OrderSize; j++)
        {
            auto* const buffer_chunk = &buffer[j * BlockSize];
            auto* const value_chunk  = &values[order[j] * BlockSize];

            std::copy(value_chunk, value_chunk + BlockSize, buffer_chunk);
        }

        std::copy(std::begin(buffer), std::end(buffer), values);
    }
}

// Input feature converter
class FeatureTransformer {
    // Number of output dimensions for one side
    static constexpr IndexType HalfDimensions = L1;

   public:
    // Output type
    using OutputType = TransformedFeatureType;

    // Number of input/output dimensions
    static constexpr IndexType InputDimensions =
      PSQFeatureSet::Dimensions + ThreatFeatureSet::Dimensions
      + KingSafetyFeatureSet::Dimensions + PieceCoordFeatureSet::Dimensions;
    static constexpr IndexType OutputDimensions = HalfDimensions;

    // Size of forward propagation buffer
    static constexpr std::size_t BufferSize = OutputDimensions * sizeof(OutputType);

    // Store the order by which 128-bit blocks of a 1024-bit data must
    // be permuted so that calling packus on adjacent vectors of 16-bit
    // integers loaded from the data results in the pre-permutation order
    static constexpr auto PackusEpi16Order = []() -> std::array<std::size_t, 8> {
#if defined(USE_AVX512)
        // _mm512_packus_epi16 after permutation:
        // |   0   |   2   |   4   |   6   | // Vector 0
        // |   1   |   3   |   5   |   7   | // Vector 1
        // | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | // Packed Result
        return {0, 2, 4, 6, 1, 3, 5, 7};
#elif defined(USE_AVX2)
        // _mm256_packus_epi16 after permutation:
        // |   0   |   2   |  |   4   |   6   | // Vector 0, 2
        // |   1   |   3   |  |   5   |   7   | // Vector 1, 3
        // | 0 | 1 | 2 | 3 |  | 4 | 5 | 6 | 7 | // Packed Result
        return {0, 2, 1, 3, 4, 6, 5, 7};
#else
        return {0, 1, 2, 3, 4, 5, 6, 7};
#endif
    }();

    static constexpr auto InversePackusEpi16Order = invert_permutation(PackusEpi16Order);

    static constexpr std::uint32_t combine_hash(std::initializer_list<std::uint32_t> hashes) {
        std::uint32_t hash = 0;
        for (const auto component_hash : hashes)
        {
            hash = (hash << 1) | (hash >> 31);
            hash ^= component_hash;
        }
        return hash;
    }

    // Hash value embedded in the evaluation file
    static constexpr std::uint32_t get_hash_value() {
        return combine_hash({ThreatFeatureSet::HashValue, PSQFeatureSet::HashValue,
                             KingSafetyFeatureSet::HashValue, PieceCoordFeatureSet::HashValue})
             ^ (OutputDimensions * 2);
    }

    void permute_weights() {
        permute<16>(biases, PackusEpi16Order);
        permute<16>(weights, PackusEpi16Order);

        permute<8>(threatWeights, PackusEpi16Order);
        permute<8>(kingSafetyWeights, PackusEpi16Order);
        permute<8>(pieceCoordWeights, PackusEpi16Order);
    }

    void unpermute_weights() {
        permute<16>(biases, InversePackusEpi16Order);
        permute<16>(weights, InversePackusEpi16Order);
        permute<8>(threatWeights, InversePackusEpi16Order);
        permute<8>(kingSafetyWeights, InversePackusEpi16Order);
        permute<8>(pieceCoordWeights, InversePackusEpi16Order);
    }

    // Read network parameters
    bool read_parameters(std::istream& stream) {
        read_leb_128(stream, biases);

        read_little_endian<ThreatWeightType>(stream, threatWeights.data(),
                                             ThreatFeatureSet::Dimensions * HalfDimensions);
        read_little_endian<WeightType>(stream, kingSafetyWeights.data(),
                                             KingSafetyFeatureSet::Dimensions * HalfDimensions);
        read_little_endian<WeightType>(stream, pieceCoordWeights.data(),
                                             PieceCoordFeatureSet::Dimensions * HalfDimensions);
        read_leb_128(stream, weights);
        read_leb_128(stream, threatPsqtWeights, psqtWeights);
        read_leb_128(stream, kingSafetyPsqtWeights, pieceCoordPsqtWeights);

        permute_weights();

        return !stream.fail();
    }

    // Write network parameters
    bool write_parameters(std::ostream& stream) const {
        std::unique_ptr<FeatureTransformer> copy = std::make_unique<FeatureTransformer>(*this);

        copy->unpermute_weights();

        write_leb_128<BiasType>(stream, copy->biases);

        write_little_endian<ThreatWeightType>(stream, copy->threatWeights.data(),
                                              ThreatFeatureSet::Dimensions * HalfDimensions);
        write_little_endian<WeightType>(stream, copy->kingSafetyWeights.data(),
                                              KingSafetyFeatureSet::Dimensions * HalfDimensions);
        write_little_endian<WeightType>(stream, copy->pieceCoordWeights.data(),
                                              PieceCoordFeatureSet::Dimensions * HalfDimensions);
        write_leb_128<WeightType>(stream, copy->weights);

        auto combinedPsqtWeights =
          std::make_unique<std::array<PSQTWeightType, InputDimensions * PSQTBuckets>>();

        std::copy(std::begin(copy->threatPsqtWeights),
                  std::begin(copy->threatPsqtWeights) + ThreatFeatureSet::Dimensions * PSQTBuckets,
                  combinedPsqtWeights->begin());

        std::copy(std::begin(copy->psqtWeights),
                  std::begin(copy->psqtWeights) + PSQFeatureSet::Dimensions * PSQTBuckets,
                  combinedPsqtWeights->begin() + ThreatFeatureSet::Dimensions * PSQTBuckets);

        std::copy(std::begin(copy->kingSafetyPsqtWeights),
                  std::begin(copy->kingSafetyPsqtWeights) + KingSafetyFeatureSet::Dimensions * PSQTBuckets,
                  combinedPsqtWeights->begin() + ThreatFeatureSet::Dimensions * PSQTBuckets + PSQFeatureSet::Dimensions * PSQTBuckets);

        std::copy(std::begin(copy->pieceCoordPsqtWeights),
                  std::begin(copy->pieceCoordPsqtWeights) + PieceCoordFeatureSet::Dimensions * PSQTBuckets,
                  combinedPsqtWeights->begin() + ThreatFeatureSet::Dimensions * PSQTBuckets + PSQFeatureSet::Dimensions * PSQTBuckets + KingSafetyFeatureSet::Dimensions * PSQTBuckets);

        write_leb_128<PSQTWeightType>(stream, *combinedPsqtWeights);

        return !stream.fail();
    }

    std::size_t get_content_hash() const {
        std::size_t h = 0;

        hash_combine(h, get_raw_data_hash(biases));
        hash_combine(h, get_raw_data_hash(weights));
        hash_combine(h, get_raw_data_hash(psqtWeights));

        hash_combine(h, get_raw_data_hash(threatWeights));
        hash_combine(h, get_raw_data_hash(threatPsqtWeights));
        hash_combine(h, get_raw_data_hash(kingSafetyWeights));
        hash_combine(h, get_raw_data_hash(kingSafetyPsqtWeights));
        hash_combine(h, get_raw_data_hash(pieceCoordWeights));
        hash_combine(h, get_raw_data_hash(pieceCoordPsqtWeights));

        hash_combine(h, get_hash_value());

        return h;
    }

// Convert input features
    std::int32_t transform(const Position&    pos,
                           AccumulatorStack&  accumulatorStack,
                           AccumulatorCaches& cache,
                           OutputType*        output,
                           int                bucket) const {

        using namespace SIMD;
        accumulatorStack.evaluate(pos, *this, cache);
        const auto& accumulatorState         = accumulatorStack.latest<PSQFeatureSet>();
        const auto& threatAccumulatorState   = accumulatorStack.latest<ThreatFeatureSet>();
        const auto& kingSafetyAccumulatorState = accumulatorStack.latest<KingSafetyFeatureSet>();
        const auto& pieceCoordAccumulatorState = accumulatorStack.latest<PieceCoordFeatureSet>();

        const Color perspectives[2]  = {pos.side_to_move(), ~pos.side_to_move()};
        const auto& psqtAccumulation = accumulatorState.psqtAccumulation;
        auto        psqt =
          (psqtAccumulation[perspectives[0]][bucket] - psqtAccumulation[perspectives[1]][bucket]);

        const auto& threatPsqtAccumulation   = threatAccumulatorState.psqtAccumulation;
        const auto& kingSafetyPsqtAccumulation = kingSafetyAccumulatorState.psqtAccumulation;
        const auto& pieceCoordPsqtAccumulation = pieceCoordAccumulatorState.psqtAccumulation;
        psqt                               = (psqt + threatPsqtAccumulation[perspectives[0]][bucket]
                - threatPsqtAccumulation[perspectives[1]][bucket]
                + kingSafetyPsqtAccumulation[perspectives[0]][bucket]
                - kingSafetyPsqtAccumulation[perspectives[1]][bucket]
                + pieceCoordPsqtAccumulation[perspectives[0]][bucket]
                - pieceCoordPsqtAccumulation[perspectives[1]][bucket])
             / 4;

        const auto& accumulation         = accumulatorState.accumulation;
        const auto& threatAccumulation   = threatAccumulatorState.accumulation;
        const auto& kingSafetyAccumulation = kingSafetyAccumulatorState.accumulation;
        const auto& pieceCoordAccumulation = pieceCoordAccumulatorState.accumulation;

        for (IndexType p = 0; p < 2; ++p)
        {
            const IndexType offset = (HalfDimensions / 2) * p;

#if defined(VECTOR)

            constexpr IndexType OutputChunkSize = MaxChunkSize;
            static_assert((HalfDimensions / 2) % OutputChunkSize == 0);
            constexpr IndexType NumOutputChunks = HalfDimensions / 2 / OutputChunkSize;

            const vec_t Zero = vec_zero();
            const vec_t One  = vec_set_16(255);

            const vec_t* in0 = reinterpret_cast<const vec_t*>(&(accumulation[perspectives[p]][0]));
            const vec_t* in1 =
              reinterpret_cast<const vec_t*>(&(accumulation[perspectives[p]][HalfDimensions / 2]));
            vec_t* out = reinterpret_cast<vec_t*>(output + offset);

            constexpr int shift =
#if defined(USE_SSE2)
              7;
#else
              6;
#endif

            const vec_t* tin0 =
              reinterpret_cast<const vec_t*>(&(threatAccumulation[perspectives[p]][0]));
            const vec_t* tin1 = reinterpret_cast<const vec_t*>(
              &(threatAccumulation[perspectives[p]][HalfDimensions / 2]));
            const vec_t* kin0 =
              reinterpret_cast<const vec_t*>(&(kingSafetyAccumulation[perspectives[p]][0]));
            const vec_t* kin1 = reinterpret_cast<const vec_t*>(
              &(kingSafetyAccumulation[perspectives[p]][HalfDimensions / 2]));
            const vec_t* pin0 =
              reinterpret_cast<const vec_t*>(&(pieceCoordAccumulation[perspectives[p]][0]));
            const vec_t* pin1 = reinterpret_cast<const vec_t*>(
              &(pieceCoordAccumulation[perspectives[p]][HalfDimensions / 2]));
            for (IndexType j = 0; j < NumOutputChunks; ++j)
            {
                const vec_t acc0a = vec_add_16(vec_add_16(in0[j * 2 + 0], tin0[j * 2 + 0]),
                                               vec_add_16(kin0[j * 2 + 0], pin0[j * 2 + 0]));
                const vec_t acc0b = vec_add_16(vec_add_16(in0[j * 2 + 1], tin0[j * 2 + 1]),
                                               vec_add_16(kin0[j * 2 + 1], pin0[j * 2 + 1]));
                const vec_t acc1a = vec_add_16(vec_add_16(in1[j * 2 + 0], tin1[j * 2 + 0]),
                                               vec_add_16(kin1[j * 2 + 0], pin1[j * 2 + 0]));
                const vec_t acc1b = vec_add_16(vec_add_16(in1[j * 2 + 1], tin1[j * 2 + 1]),
                                               vec_add_16(kin1[j * 2 + 1], pin1[j * 2 + 1]));

                const vec_t sum0a = vec_slli_16(vec_max_16(vec_min_16(acc0a, One), Zero), shift);
                const vec_t sum0b = vec_slli_16(vec_max_16(vec_min_16(acc0b, One), Zero), shift);
                const vec_t sum1a = vec_min_16(acc1a, One);
                const vec_t sum1b = vec_min_16(acc1b, One);

                const vec_t pa = vec_mulhi_16(sum0a, sum1a);
                const vec_t pb = vec_mulhi_16(sum0b, sum1b);

                out[j] = vec_packus_16(pa, pb);
            }

#else

            for (IndexType j = 0; j < HalfDimensions / 2; ++j)
            {
                BiasType sum0 = accumulation[static_cast<int>(perspectives[p])][j + 0];
                BiasType sum1 =
                  accumulation[static_cast<int>(perspectives[p])][j + HalfDimensions / 2];

                sum0 += threatAccumulation[static_cast<int>(perspectives[p])][j + 0];
                sum1 +=
                  threatAccumulation[static_cast<int>(perspectives[p])][j + HalfDimensions / 2];
                sum0 += kingSafetyAccumulation[static_cast<int>(perspectives[p])][j + 0];
                sum1 +=
                  kingSafetyAccumulation[static_cast<int>(perspectives[p])][j + HalfDimensions / 2];
                sum0 += pieceCoordAccumulation[static_cast<int>(perspectives[p])][j + 0];
                sum1 +=
                  pieceCoordAccumulation[static_cast<int>(perspectives[p])][j + HalfDimensions / 2];

                sum0 = std::clamp<BiasType>(sum0, 0, 255);
                sum1 = std::clamp<BiasType>(sum1, 0, 255);

                output[offset + j] = static_cast<OutputType>(unsigned(sum0 * sum1) / 512);
            }

#endif
        }

        return psqt;
    }  // end of function transform()

    alignas(CacheLineSize) std::array<BiasType, HalfDimensions> biases;
    alignas(
      CacheLineSize) std::array<WeightType, HalfDimensions * PSQFeatureSet::Dimensions> weights;
    alignas(CacheLineSize)
      std::array<ThreatWeightType, HalfDimensions * ThreatFeatureSet::Dimensions> threatWeights;
    alignas(CacheLineSize)
      std::array<PSQTWeightType, PSQFeatureSet::Dimensions * PSQTBuckets> psqtWeights;
    alignas(CacheLineSize)
      std::array<PSQTWeightType, ThreatFeatureSet::Dimensions * PSQTBuckets> threatPsqtWeights;
    alignas(CacheLineSize)
      std::array<WeightType, HalfDimensions * KingSafetyFeatureSet::Dimensions> kingSafetyWeights;
    alignas(CacheLineSize)
      std::array<WeightType, HalfDimensions * PieceCoordFeatureSet::Dimensions> pieceCoordWeights;
    alignas(CacheLineSize)
      std::array<PSQTWeightType, KingSafetyFeatureSet::Dimensions * PSQTBuckets> kingSafetyPsqtWeights;
    alignas(CacheLineSize)
      std::array<PSQTWeightType, PieceCoordFeatureSet::Dimensions * PSQTBuckets> pieceCoordPsqtWeights;
};

}  // namespace Stockfish::Eval::NNUE

template<>
struct std::hash<Stockfish::Eval::NNUE::FeatureTransformer> {
    std::size_t operator()(const Stockfish::Eval::NNUE::FeatureTransformer& ft) const noexcept {
        return ft.get_content_hash();
    }
};

#endif  // #ifndef NNUE_FEATURE_TRANSFORMER_H_INCLUDED
