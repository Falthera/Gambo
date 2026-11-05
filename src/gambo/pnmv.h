/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PNMV — Proof-Number Mate Verification
// Specialized proof-number search for forced-mate verification

#ifndef GAMBO_PNMV_H_INCLUDED
#define GAMBO_PNMV_H_INCLUDED

#include <cstdint>
#include <limits>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::PNMV {

using namespace Stockfish;

enum class PNNodeType {
    ATTACKER,
    DEFENDER
};

struct PNMVConfig {
    int max_nodes = 100000;
    int max_depth = 30;
    int max_time_ms = 100;
    uint32_t infinity = std::numeric_limits<uint32_t>::max() / 4;
};

struct PNMVStats {
    int nodes_created = 0;
    int nodes_evaluated = 0;
    int mate_proven = 0;
    int mate_disproven = 0;
    int timeouts = 0;
};

class ProofNumberMateVerifier {
public:
    ProofNumberMateVerifier() = default;

    bool verify_mate_in(const Position&, Search::Stack*, int, int);
    bool verify_forced_win(const Position&, Search::Stack*, int);

    void set_config(const PNMVConfig& cfg);
    const PNMVConfig& get_config() const;

    const PNMVStats& get_stats() const;
    void reset_stats();

private:
    PNMVConfig config;
    PNMVStats stats;
};

}  // namespace Gambo::PNMV

#endif  // GAMBO_PNMV_H_INCLUDED