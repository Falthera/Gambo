/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Sparse input affine transform implementation

#include "affine_transform_sparse_input.h"

#include <cstddef>
#include <cstdint>

#if defined(__AVX2__) && defined(GAMBO_USE_SIMD)
#include <immintrin.h>
#elif defined(__SSE4_1__) && defined(GAMBO_USE_SIMD)
#include <smmintrin.h>
#elif defined(__SSE2__) && defined(GAMBO_USE_SIMD)
#include <emmintrin.h>
#endif

namespace Gambo::NNUE::Layers {

template<IndexType InputDimensions, int OutputDimensions>
std::size_t AffineTransformSparseInput<InputDimensions, OutputDimensions>::get_content_hash() const {
    std::size_t h = 0;
    for (int i = 0; i < OutputDimensions; ++i) {
        for (IndexType j = 0; j < InputDimensions; ++j) {
            h = h * 31 + weights[j][i];
        }
        h = h * 31 + bias[i];
    }
    return h;
}

template<IndexType InputDimensions, int OutputDimensions>
bool AffineTransformSparseInput<InputDimensions, OutputDimensions>::read_parameters(std::istream& stream) {
    for (int i = 0; i < OutputDimensions; ++i) {
        for (IndexType j = 0; j < InputDimensions; ++j) {
            stream.read(reinterpret_cast<char*>(&weights[j][i]), sizeof(WeightType));
        }
        stream.read(reinterpret_cast<char*>(&bias[i]), sizeof(OutputType));
    }
    return true;
}

template<IndexType InputDimensions, int OutputDimensions>
bool AffineTransformSparseInput<InputDimensions, OutputDimensions>::write_parameters(std::ostream& stream) const {
    for (int i = 0; i < OutputDimensions; ++i) {
        for (IndexType j = 0; j < InputDimensions; ++j) {
            stream.write(reinterpret_cast<const char*>(&weights[j][i]), sizeof(WeightType));
        }
        stream.write(reinterpret_cast<const char*>(&bias[i]), sizeof(OutputType));
    }
    return true;
}

// SONI: SIMD-Optimised Sparse Input Affine Transform
template<IndexType InputDimensions, int OutputDimensions>
void AffineTransformSparseInput<InputDimensions, OutputDimensions>::propagate(
    const TransformedFeatureType* input, OutputBuffer& output) const {

#if defined(__AVX2__) && defined(GAMBO_USE_SIMD)
    // AVX2 implementation: process 8 weights at a time
    constexpr int SIMD_WIDTH = 8;
    
    for (int i = 0; i < OutputDimensions; ++i) {
        int32_t sum = bias[i];
        IndexType j = 0;
        
        // Vectorized accumulation
        __m256i vsum = _mm256_setzero_si256();
        for (; j + SIMD_WIDTH <= InputDimensions; j += SIMD_WIDTH) {
            __m256i vinput = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&input[j]));
            __m256i vweights = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&weights[j][i]));
            __m256i vprod = _mm256_madd_epi16(vinput, vweights);
            vsum = _mm256_add_epi32(vsum, vprod);
        }
        
        // Horizontal sum
        __m128i vsum_low = _mm256_castsi256_si128(vsum);
        __m128i vsum_high = _mm256_extracti128_si256(vsum, 1);
        __m128i vsum_total = _mm_add_epi32(vsum_low, vsum_high);
        vsum_total = _mm_hadd_epi32(vsum_total, vsum_total);
        vsum_total = _mm_hadd_epi32(vsum_total, vsum_total);
        sum += _mm_cvtsi128_si32(vsum_total);
        
        // Scalar remainder
        for (; j < InputDimensions; ++j) {
            sum += input[j] * weights[j][i];
        }
        
        output[i] = static_cast<OutputType>(std::clamp(sum, 0, 127));
    }
    
#elif defined(__SSE4_1__) && defined(GAMBO_USE_SIMD)
    // SSE4.1 implementation: process 4 weights at a time
    constexpr int SIMD_WIDTH = 4;
    
    for (int i = 0; i < OutputDimensions; ++i) {
        int32_t sum = bias[i];
        IndexType j = 0;
        
        __m128i vsum = _mm_setzero_si128();
        for (; j + SIMD_WIDTH <= InputDimensions; j += SIMD_WIDTH) {
            __m128i vinput = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[j]));
            __m128i vweights = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&weights[j][i]));
            __m128i vprod = _mm_madd_epi16(vinput, vweights);
            vsum = _mm_add_epi32(vsum, vprod);
        }
        
        // Horizontal sum
        vsum = _mm_hadd_epi32(vsum, vsum);
        vsum = _mm_hadd_epi32(vsum, vsum);
        sum += _mm_cvtsi128_si32(vsum);
        
        for (; j < InputDimensions; ++j) {
            sum += input[j] * weights[j][i];
        }
        
        output[i] = static_cast<OutputType>(std::clamp(sum, 0, 127));
    }
    
#else
    // Scalar fallback
    for (int i = 0; i < OutputDimensions; ++i) {
        int32_t sum = bias[i];
        for (IndexType j = 0; j < InputDimensions; ++j) {
            sum += input[j] * weights[j][i];
        }
        output[i] = static_cast<OutputType>(std::clamp(sum, 0, 127));
    }
#endif
}

// Runtime SIMD dispatch
template<IndexType InputDimensions, int OutputDimensions>
void AffineTransformSparseInput<InputDimensions, OutputDimensions>::propagate_dispatch(
    const TransformedFeatureType* input, OutputBuffer& output) const {
    static const bool use_simd = 
#if defined(GAMBO_USE_SIMD)
        true;
#else
        false;
#endif
    
    if (use_simd) {
        propagate(input, output);
    } else {
        for (int i = 0; i < OutputDimensions; ++i) {
            int32_t sum = bias[i];
            for (IndexType j = 0; j < InputDimensions; ++j) {
                sum += input[j] * weights[j][i];
            }
            output[i] = static_cast<OutputType>(std::clamp(sum, 0, 127));
        }
    }
}

// Explicit template instantiation
template class AffineTransformSparseInput<TransformedFeatureDimensions, 63>;

}  // namespace Gambo::NNUE::Layers