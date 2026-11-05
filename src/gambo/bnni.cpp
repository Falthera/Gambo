/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// BNNI implementation (Experimental)

#include "bnni.h"

#include <chrono>

#include "../position.h"
#include "../types.h"

namespace Gambo::BNNI {

using namespace Stockfish;

BNNIResult BatchedNNUEInference::infer_batch(const std::vector<Position*>& positions) {
    BNNIResult result;
    result.values.resize(positions.size(), VALUE_NONE);
    result.batches = {static_cast<int>(positions.size())};
    result.total_inferences = positions.size();
    result.time_us = 0;

    if (!enabled) return result;

    auto start = std::chrono::high_resolution_clock::now();

    if (config.use_simd) {
        infer_simd_batch(positions, result.values);
    } else {
        infer_scalar_batch(positions, result.values);
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return result;
}

void BatchedNNUEInference::infer_simd_batch(const std::vector<Position*>& positions, std::vector<Value>& outputs) {
    // Placeholder for SIMD batched inference
    // Would use AVX2/AVX512 for parallel NNUE evaluation
    for (size_t i = 0; i < positions.size(); ++i) {
        outputs[i] = VALUE_DRAW;  // placeholder
    }
}

void BatchedNNUEInference::infer_scalar_batch(const std::vector<Position*>& positions, std::vector<Value>& outputs) {
    // Fallback scalar implementation
    for (size_t i = 0; i < positions.size(); ++i) {
        outputs[i] = VALUE_DRAW;  // placeholder
    }
}

}  // namespace Gambo::BNNI