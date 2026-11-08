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

// Squared Clipped ReLU activation implementation

#include "sqr_clipped_relu.h"

#include <algorithm>

#if defined(__AVX2__) && defined(GAMBO_USE_SIMD)
#include <immintrin.h>
#elif defined(__SSE4_1__) && defined(GAMBO_USE_SIMD)
#include <smmintrin.h>
#elif defined(__SSE2__) && defined(GAMBO_USE_SIMD)
#include <emmintrin.h>
#endif

namespace Gambo::NNUE::Layers {

// SONI: SIMD-Optimised Squared Clipped ReLU
template<int Size>
void SqrClippedReLU<Size>::propagate(const OutputType* input,
                                     OutputBuffer& output) const {

#if defined(__AVX2__) && defined(GAMBO_USE_SIMD)
    // AVX2 implementation: process 16 elements at a time
    constexpr int SIMD_WIDTH = 16;
    int i = 0;
    
    const __m256i vmin = _mm256_setzero_si256();
    const __m256i vmax = _mm256_set1_epi16(127);
    const __m256i vzero = _mm256_setzero_si256();
    
    for (; i + 16 <= Size; i += 16) {
        __m256i vinput = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&input[i]));
        
        // Clamp to [0, 127]
        __m256i vclamped = _mm256_max_epi16(vinput, vmin);
        vclamped = _mm256_min_epi16(vclamped, vmax);
        
        // Square: multiply with itself (using 32-bit intermediate)
        // Convert to 32-bit, square, then clamp
        __m256i vclamped_lo = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(vclamped));
        __m256i vclamped_hi = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(vclamped, 1));
        
        __m256i vsquared_lo = _mm256_mullo_epi32(vclamped_lo, vclamped_lo);
        __m256i vsquared_hi = _mm256_mullo_epi32(vclamped_hi, vclamped_hi);
        
        // Clamp squared result to 127
        const __m256i vmax32 = _mm256_set1_epi32(127);
        vsquared_lo = _mm256_min_epi32(vsquared_lo, vmax32);
        vsquared_hi = _mm256_min_epi32(vsquared_hi, vmax32);
        
        // Pack back to 16-bit
        __m128i vpacked_lo = _mm256_cvtepi32_epi16(vsquared_lo);
        __m128i vpacked_hi = _mm256_cvtepi32_epi16(vsquared_hi);
        __m256i vpacked = _mm256_inserti128_si256(_mm256_castsi128_si256(vpacked_lo), vpacked_hi, 1);
        
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&output[i]), vpacked);
    }
    
    // Scalar remainder
    for (int i = Size - (Size % 16); i < Size; ++i) {
        int32_t v = input[i];
        v = std::clamp(v, 0, 127);
        output[i] = static_cast<OutputType>(v * v);
    }
    
#elif defined(__SSE4_1__) && defined(GAMBO_USE_SIMD)
    // SSE4.1 implementation: process 8 elements at a time
    constexpr int SIMD_WIDTH = 8;
    int i = 0;
    
    const __m128i vmin = _mm_setzero_si128();
    const __m128i vmax = _mm_set1_epi16(127);
    
    for (; i + 8 <= Size; i += 8) {
        __m128i vinput = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[i]));
        
        // Clamp to [0, 127]
        __m128i vclamped = _mm_max_epi16(vinput, vmin);
        vclamped = _mm_min_epi16(vclamped, vmax);
        
        // Square using 32-bit intermediate
        __m128i vclamped_lo = _mm_cvtepi16_epi32(_mm_cvtepi32_epi64(vclamped));
        __m128i vclamped_hi = _mm_cvtepi16_epi32(_mm_unpackhi_epi64(vclamped, vclamped));
        
        __m128i vsquared_lo = _mm_mullo_epi32(vclamped_lo, vclamped_lo);
        __m128i vsquared_hi = _mm_mullo_epi32(vclamped_hi, vclamped_hi);
        
        // Clamp squared result
        const __m128i vmax32 = _mm_set1_epi32(127);
        vsquared_lo = _mm_min_epi32(vsquared_lo, vmax32);
        vsquared_hi = _mm_min_epi32(vsquared_hi, vmax32);
        
        // Pack back to 16-bit
        __m128i vpacked = _mm_packs_epi32(vsquared_lo, vsquared_hi);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&output[i]), vpacked);
    }
    
    for (int i = Size - (Size % 8); i < Size; ++i) {
        int32_t v = input[i];
        v = std::clamp(v, 0, 127);
        output[i] = static_cast<OutputType>(v * v);
    }
    
#else
    // Scalar fallback
    for (int i = 0; i < Size; ++i) {
        int32_t v = input[i];
        v = std::clamp(v, 0, 127);
        output[i] = static_cast<OutputType>(v * v);
    }
#endif
}

// Runtime SIMD dispatch
template<int Size>
void SqrClippedReLU<Size>::propagate_dispatch(const OutputType* input,
                                              OutputBuffer& output) const {
    static const bool use_simd = 
#if defined(GAMBO_USE_SIMD)
        true;
#else
        false;
#endif
    
    if (use_simd) {
        propagate(input, output);
    } else {
        // Scalar fallback
        for (int i = 0; i < Size; ++i) {
            int32_t v = input[i];
            v = std::clamp(v, 0, 127);
            output[i] = static_cast<OutputType>(v * v);
        }
    }
}

// Explicit template instantiation
template class SqrClippedReLU<63>;

}  // namespace Gambo::NNUE::Layers