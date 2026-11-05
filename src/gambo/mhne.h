/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// MHNE — Multi-Head Neural Evaluation (Experimental)

#ifndef GAMBO_MHNE_H_INCLUDED
#define GAMBO_MHNE_H_INCLUDED

#include <cstdint>
#include <string>
#include <vector>

#include "../nnue/nnue_common.h"
#include "../position.h"
#include "../types.h"

namespace Gambo::MHNE {

using namespace Stockfish;

enum class HeadType {
    VALUE = 0,
    WDL = 1,           // Win/Draw/Loss probabilities
    POLICY = 2,        // Move policy
    UNCERTAINTY = 3,   // Position uncertainty
    AUXILIARY = 4,     // Auxiliary predictions
};

struct MHNEConfig {
    std::vector<HeadType> active_heads = {HeadType::VALUE};
    std::string model_path;
    bool enabled = false;
};

struct HeadOutput {
    HeadType type;
    std::vector<float> data;  // Output from this head
};

struct MHNEResult {
    std::vector<HeadOutput> outputs;
    Value value;  // From VALUE head
    float wdl_win = 0, wdl_draw = 0, wdl_loss = 0;  // From WDL head
    std::vector<std::pair<Move, float>> policy;     // From POLICY head
    float uncertainty = 0;                           // From UNCERTAINTY head
};

class MultiHeadNeuralEvaluation {
public:
    MultiHeadNeuralEvaluation() = default;

    void init(const MHNEConfig& cfg) { config = cfg; enabled = cfg.enabled; }

    MHNEResult evaluate(const Position& pos);

    bool is_enabled() const { return enabled; }
    void set_enabled(bool e) { enabled = e; }

private:
    MHNEConfig config;
    bool enabled = false;

    void load_model();
    void forward_pass(const Position& pos);
    Value extract_value() const;
    void extract_wdl(float& w, float& d, float& l) const;
    void extract_policy(std::vector<std::pair<Move, float>>& pol) const;
    float extract_uncertainty() const;
};

}  // namespace Gambo::MHNE

#endif  // GAMBO_MHNE_H_INCLUDED