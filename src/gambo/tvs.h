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

#include <array>
#include <cstdint>
#include <vector>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::TVS {

using namespace Stockfish;

enum class VerificationResult {
    VERIFIED,      // Claim verified (mate, win, etc.)
    REFUTED,       // Claim refuted (defense found)
    UNKNOWN,       // Inconclusive within budget
    TIMEOUT        // Search timeout
};

struct VerificationConfig {
    int max_depth = 16;
    int max_nodes = 50000;
    int max_time_ms = 50;
    int mate_verification_depth = 24;
    int win_verification_depth = 16;
    int draw_verification_depth = 12;
};

struct VerificationContext {
    Position* pos = nullptr;
    Search::Stack* ss = nullptr;
    Value claimed_score = VALUE_NONE;
    int depth = 0;
    int ply = 0;
    int budget_nodes = 0;
    int budget_time_ms = 0;
    TimePoint start_time;
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

    // Main verification entry points
    VerificationResult verify_mate_claim(const Position& pos, Search::Stack* ss, int depth, int mate_in);
    VerificationResult verify_win_claim(const Position& pos, Search::Stack* ss, int depth, Value score);
    VerificationResult verify_draw_claim(const Position& pos, Search::Stack* ss, int depth);
    VerificationResult verify_tactical_claim(const Position& pos, Search::Stack* ss, int depth, Value score, Move claim_move);

    // Configuration
    void set_config(const VerificationConfig& cfg) { config = cfg; }
    const VerificationConfig& get_config() const { return config; }

    // Statistics
    const VerificationStats& get_stats() const { return stats; }
    void reset_stats() { stats = {}; }

private:
    VerificationConfig config;
    VerificationStats stats;
    TimeManagement time_manager;

    // Internal verification search
    VerificationResult search_verification(VerificationContext& ctx, bool is_mate_search);

    // Mate search with proof-number-like approach
    VerificationResult mate_search(VerificationContext& ctx, int mate_in);

    // Defensive resource search
    VerificationResult defense_search(VerificationContext& ctx);

    // Check if time/nodes budget exhausted
    bool budget_exhausted(const VerificationContext& ctx) const;

    // Evaluate position for verification
    Value verify_evaluate(const Position& pos, Search::Stack* ss);
};

}  // namespace Gambo::TVS

#endif  // GAMBO_TVS_H_INCLUDED