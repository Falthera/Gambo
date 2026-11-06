/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// TVS — Tactical Verification Search (stub implementation)

#include "tvs.h"

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::TVS {

using namespace Stockfish;

TacticalVerification::TacticalVerification() = default;

VerificationResult TacticalVerification::verify_mate_claim(const Position&, Search::Stack*, int, int) {
    stats.total_verifications++;
    stats.unknown++;
    return VerificationResult::UNKNOWN;
}

VerificationResult TacticalVerification::verify_win_claim(const Position&, Search::Stack*, int, Value) {
    stats.total_verifications++;
    stats.unknown++;
    return VerificationResult::UNKNOWN;
}

VerificationResult TacticalVerification::verify_draw_claim(const Position&, Search::Stack*, int) {
    stats.total_verifications++;
    stats.unknown++;
    return VerificationResult::UNKNOWN;
}

VerificationResult TacticalVerification::verify_tactical_claim(const Position&, Search::Stack*, int, Value, Move) {
    stats.total_verifications++;
    stats.unknown++;
    return VerificationResult::UNKNOWN;
}

void TacticalVerification::set_config(const VerificationConfig& cfg) { config = cfg; }
const VerificationConfig& TacticalVerification::get_config() const { return config; }

const VerificationStats& TacticalVerification::get_stats() const { return stats; }
void TacticalVerification::reset_stats() { stats = {}; }

}  // namespace Gambo::TVS