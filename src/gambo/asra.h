/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// ASRA — Adaptive Search Resource Allocation
// Dynamically allocates search effort based on position criticality and instability

#ifndef GAMBO_ASRA_H_INCLUDED
#define GAMBO_ASRA_H_INCLUDED

#include <array>
#include <cstdint>

#include "pce.h"
#include "sid.h"
#include "../position.h"
#include "../search.h"
#include "../thread.h"
#include "../types.h"

namespace Gambo::ASRA {

using namespace Stockfish;

struct ResourceAllocation {
    int depth_bonus;           // Additional depth to search
    int node_budget_mult;      // Node budget multiplier (percent)
    int time_budget_mult;      // Time budget multiplier (percent)
    bool enable_verification;  // Enable tactical verification
    int verification_depth;    // Depth for verification search

    ResourceAllocation() : depth_bonus(0), node_budget_mult(100), time_budget_mult(100),
                          enable_verification(false), verification_depth(0) {}
};

struct AllocationConfig {
    // PCE thresholds
    int pce_low_threshold = 64;
    int pce_medium_threshold = 128;
    int pce_high_threshold = 192;

    // SID thresholds
    int sid_stable_threshold = 20;
    int sid_minor_threshold = 40;
    int sid_moderate_threshold = 80;
    int sid_high_threshold = 140;

    // Resource limits
    int max_depth_bonus = 4;
    int max_node_mult = 200;
    int max_time_mult = 150;

    // Verification settings
    int verification_base_depth = 8;
    int verification_max_depth = 16;
};

class ResourceAllocator {
public:
    ResourceAllocator() = default;

    // Compute resource allocation for current position
    ResourceAllocation compute_allocation(const Position& pos, const Search::Stack* ss,
                                         int depth, const Search::PVMoves* pv);

    // Apply allocation to search parameters
    void apply_allocation(ResourceAllocation& alloc, Search::SearchParams& params,
                         Threads& threads);

    // Update from PCE and SID
    void update_pce(const PCE::PositionCriticalityEngine& pce, int score, PCE::CriticalityLevel level);
    void update_sid(const SID::SearchInstabilityDetector& sid, SID::InstabilityLevel level);

    // Configuration
    void set_config(const AllocationConfig& cfg) { config = cfg; }
    const AllocationConfig& get_config() const { return config; }

    // Statistics
    struct Stats {
        int critical_positions = 0;
        int unstable_positions = 0;
        int verification_triggered = 0;
        int depth_bonus_applied = 0;
    };
    const Stats& get_stats() const { return stats; }
    void reset_stats() { stats = {}; }

private:
    AllocationConfig config;
    Stats stats;

    int last_pce_score = 0;
    PCE::CriticalityLevel last_pce_level = PCE::CriticalityLevel::LOW;
    SID::InstabilityLevel last_sid_level = SID::InstabilityLevel::STABLE;

    ResourceAllocation compute_from_pce(const Position& pos);
    ResourceAllocation compute_from_sid(const Position& pos);
    ResourceAllocation combine_allocations(const ResourceAllocation& a, const ResourceAllocation& b);
};

}  // namespace Gambo::ASRA

#endif  // GAMBO_ASRA_H_INCLUDED