/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// BNNI — Batched Neural Network Inference (Experimental)

#ifndef GAMBO_BNNI_H_INCLUDED
#define GAMBO_BNNI_H_INCLUDED

#include <cstdint>
#include <vector>

#include "../nnue/nnue_common.h"
#include "../position.h"
#include "../types.h"

namespace Gambo::BNNI {

using namespace Stockfish;

struct BNNIConfig {
    int max_batch_size = 32;
    bool use_simd = true;
    bool enabled = false;
};

struct BNNIResult {
    std::vector<Value> values;
    std::vector<int> batches;
    int total_inferences;
    int64_t time_us;
};

class BatchedNNUEInference {
public:
    BatchedNNUEInference() = default;

    void init(const BNNIConfig& cfg) { config = cfg; enabled = cfg.enabled; }

    BNNIResult infer_batch(const std::vector<Position*>& positions);

    bool is_enabled() const { return enabled; }
    void set_enabled(bool e) { enabled = e; }

private:
    BNNIConfig config;
    bool enabled = false;

    void infer_simd_batch(const std::vector<Position*>& positions, std::vector<Value>& outputs);
    void infer_scalar_batch(const std::vector<Position*>& positions, std::vector<Value>& outputs);
};

}  // namespace Gambo::BNNI

#endif  // GAMBO_BNNI_H_INCLUDED