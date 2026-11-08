/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// PNMV - Proof-Number Mate Verification
// Bounded proof-number search for mate verification

#ifndef GAMBO_PNMV_H_INCLUDED
#define GAMBO_PNMV_H_INCLUDED

#include <cstdint>
#include <vector>
#include <limits>

#include "../types.h"
#include "../position.h"

namespace Stockfish {
class Position;

namespace PNMV {

enum class PNNodeType : uint8_t {
    AND = 0,  // All children must be proven (opponent's move)
    OR  = 1   // At least one child proven (our move)
};

struct PNNode {
    uint32_t proofNumber = 0;
    uint32_t disproofNumber = 0;
    PNNodeType type = PNNodeType::OR;
    Move bestMove = Move::none();
    std::vector<PNNode*> children;
    PNNode* parent = nullptr;
    int depth = 0;
    bool evaluated = false;
};

struct PNMVConfig {
    uint32_t maxProofNumber = 1000000;
    uint32_t maxDisproofNumber = 1000000;
    int maxDepth = 20;
    int maxNodes = 50000;
    bool enabled = true;
};

extern PNMVConfig pnmvConfig;

struct PNMVStats {
    std::uint64_t totalSearches = 0;
    std::uint64_t proven = 0;
    std::uint64_t disproven = 0;
    std::uint64_t unknown = 0;
    int avgDepth = 0;
    int avgNodes = 0;
};

extern PNMVStats pnmvStats;

// Initialize PNMV subsystem
void init_pnmv();

// Proof-number search for mate verification
enum class PNMVResult : uint8_t {
    PROVEN = 0,      // Mate proven
    DISPROVEN = 1,   // Mate disproven (defense found)
    UNKNOWN = 2      // Inconclusive within budget
};

PNMVResult prove_mate(Position& pos, int maxDepth = 12, int maxNodes = 50000);
PNMVResult prove_mate_with_bounds(Position& pos, Value alpha, Value beta, int maxDepth, int maxNodes);

// Get proof/disproof numbers for a position
uint32_t get_proof_number(const Position& pos);
uint32_t get_disproof_number(const Position& pos);

// Statistics
const PNMVStats& get_pnmv_stats();

}  // namespace PNMV
}  // namespace Stockfish

#endif  // GAMBO_PNMV_H_INCLUDED