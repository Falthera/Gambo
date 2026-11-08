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

// Clipped ReLU activation implementation

#include "clipped_relu.h"

#include <algorithm>

#if defined(__AVX2__) && defined(GAMBO_USE_SIMD)
#include <immintrin.h>
#elif defined(__SSE4_1__) && defined(GAMBO_USE_SIMD)
#include <smmintrin.h>
#elif defined(__SSE2__) && defined(GAMBO_USE_SIMD)
#include <emmintrin.h>
#endif

namespace Gambo::NNUE::Layers {

// SONI: SIMD-Optimised Clipped ReLU
template<int Size>
void ClippedReLU<Size>::propagate(const OutputType* input,
                                  OutputBuffer& output) const {

#if defined(__AVX2__) && defined(GAMBO_USE_SIMD)
    // AVX2 implementation: process 16 elements at a time (int16_t = 2 bytes, 256/16 = 16)
    constexpr int SIMD_WIDTH = 16;
    int i = 0;
    
    for (; i + SIMD_WIDTH <= Size; i += SIMD_WIDTH) {
        __m256i vinput = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&input[i]));
        
        // Clamp to [0, 127]
        __m256i vmin = _mm256_setzero_si256();
        __m256i vmax = _mm256_set1_epi16(127);
        __m256i voutput = _mm256_max_epi16(vinput, vmin);
        voutput = _mm256_min_epi16(voutput, vmax);
        
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&output[i]), voutput);
    }
    
    // Scalar remainder
    for (int i = Size - (Size % 16); i < Size; ++i) {
        int32_t v = input[i];
        output[i] = static_cast<OutputType>(std::clamp(v, 0, 127));
    }
    
#elif defined(__SSE4_1__) && defined(GAMBO_USE_SIMD)
    // SSE4.1 implementation: process 8 elements at a time
    constexpr int SIMD_WIDTH = 8;
    int i = 0;
    
    const __m128i vmin = _mm_setzero_si128();
    const __m128i vmax = _mm_set1_epi16(127);
    
    for (; i + SIMD_WIDTH <= Size; i += SIMD_WIDTH) {
        __m128i vinput = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[i]));
        __m128i voutput = _mm_max_epi16(vinput, vmin);
        voutput = _mm_min_epi16(voutput, vmax);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&output[i]), voutput);
    }
    
    for (int i = Size - (Size % 8); i < Size; ++i) {
        int32_t v = input[i];
        output[i] = static_cast<OutputType>(std::clamp(v, 0, 127));
    }
    
#else
    // Scalar fallback
    for (int i = 0; i < Size; ++i) {
        int32_t v = input[i];
        output[i] = static_cast<OutputType>(std::clamp(v, 0, 127));
    }
#endif
}

// Runtime SIMD dispatch
template<int Size>
void ClippedReLU<Size>::propagate_dispatch(const OutputType* input,
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
            output[i] = static_cast<OutputType>(std::clamp(v, 0, 127));
        }
    }
}

// Explicit template instantiations
template class ClippedReLU<63>;
template class ClippedReLU<32>;

}  // namespace Gambo::NNUE::Layers