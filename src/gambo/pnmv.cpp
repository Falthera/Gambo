/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PNMV — Proof-Number Mate Verification (stub implementation)

#include "pnmv.h"

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::PNMV {

using namespace Stockfish;

ProofNumberMateVerifier::ProofNumberMateVerifier() = default;

bool ProofNumberMateVerifier::verify_mate_in(const Position&, Search::Stack*, int, int) {
    stats = {};
    stats.mate_disproven++;
    return false;
}

bool ProofNumberMateVerifier::verify_forced_win(const Position&, Search::Stack*, int) {
    return false;
}

void ProofNumberMateVerifier::set_config(const PNMVConfig& cfg) { config = cfg; }
const PNMVConfig& ProofNumberMateVerifier::get_config() const { return config; }

const PNMVStats& ProofNumberMateVerifier::get_stats() const { return stats; }
void ProofNumberMateVerifier::reset_stats() { stats = {}; }

}  // namespace Gambo::PNMV