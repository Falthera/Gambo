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

// TVS - Tactical Verification Search
// Bounded tactical verification subsystem for critical positions

#ifndef GAMBO_TVS_H_INCLUDED
#define GAMBO_TVS_H_INCLUDED

#include <array>
#include <cstdint>

#include "../types.h"
#include "../position.h"
#include "../search.h"
#include "../evaluate.h"

namespace Stockfish {

class Position;
class Stack;

namespace TVS {

// Verification result statuses
enum class VerificationStatus : uint8_t {
    UNKNOWN     = 0,  // Not yet verified
    VERIFIED    = 1,  // Move confirmed sound
    REFUTED     = 2,  // Move refuted by opponent response
    INCONCLUSIVE = 3  // Time/depth budget exhausted
};

// Configuration for tactical verification
struct TVSConfig {
    Depth   maxDepth = 12;           // Maximum verification depth
    int     timeBudgetMs = 50;       // Time budget per verification
    int     nodeBudget = 10000;      // Node budget per verification
    int     maxCandidates = 3;       // Max moves to verify per position
    bool    enabled = true;
};

// Global config instance
extern TVSConfig tvsConfig;

// Tactical verification context
struct TVSContext {
    const Position* pos = nullptr;
    Stack* ss = nullptr;
    Depth depth = 0;
    Value alpha = 0, beta = 0;
    int timeSpentMs = 0;
    int nodesUsed = 0;
    bool aborted = false;
};

// Verify a candidate move with bounded search
VerificationStatus verify_move(TVSContext& ctx, Move move, Depth verificationDepth);

// Verify top N candidate moves
void verify_candidates(const Position& pos, Stack* ss, Depth depth, Value alpha, Value beta,
                       const Move* candidates, int numCandidates, 
                       VerificationStatus* results);

// Check if position warrants tactical verification
bool should_verify(const Position& pos, Depth depth, int pcScore, Move bestMove);

// Get verification status as string
const char* status_to_string(VerificationStatus status);

// Initialize TVS subsystem
void init_tvs();

// Statistics
struct TVSStats {
    std::uint64_t totalVerifications = 0;
    std::uint64_t verified = 0;
    std::uint64_t refuted = 0;
    std::uint64_t inconclusive = 0;
    std::uint64_t timeAborts = 0;
    std::uint64_t nodeAborts = 0;
    int avgDepth = 0;
    int avgTimeMs = 0;
    int avgNodes = 0;
};

extern TVSStats tvsStats;

}  // namespace TVS

}  // namespace Stockfish

#endif  // GAMBO_TVS_H_INCLUDED