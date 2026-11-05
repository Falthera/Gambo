/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// TVS — Tactical Verification Search
// Bounded tactical verification subsystem to investigate critical claims

#ifndef GAMBO_TVS_H_INCLUDED
#define GAMBO_TVS_H_INCLUDED

#include <cstdint>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::TVS {

using namespace Stockfish;

enum class VerificationResult {
    VERIFIED,
    REFUTED,
    UNKNOWN,
    TIMEOUT
};

struct VerificationConfig {
    int max_depth = 16;
    int max_nodes = 50000;
    int max_time_ms = 50;
    int mate_verification_depth = 24;
    int win_verification_depth = 16;
    int draw_verification_depth = 12;
};

struct VerificationStats {
    int total_verifications = 0;
    int verified = 0;
    int refuted = 0;
    int unknown = 0;
    int timeouts = 0;
    int nodes_searched = 0;
};

class TacticalVerification {
public:
    TacticalVerification() = default;

    VerificationResult verify_mate_claim(const Position&, Search::Stack*, int, int);
    VerificationResult verify_win_claim(const Position&, Search::Stack*, int, Value);
    VerificationResult verify_draw_claim(const Position&, Search::Stack*, int);
    VerificationResult verify_tactical_claim(const Position&, Search::Stack*, int, Value, Move);

    void set_config(const VerificationConfig& cfg);
    const VerificationConfig& get_config() const;

    const VerificationStats& get_stats() const;
    void reset_stats();

private:
    VerificationConfig config;
    VerificationStats stats;
};

}  // namespace Gambo::TVS

#endif  // GAMBO_TVS_H_INCLUDED