/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// ASRA — Adaptive Search Resource Allocation implementation

#include "asra.h"

#include <algorithm>

#include "../position.h"
#include "../search.h"
#include "../thread.h"
#include "../types.h"
#include "pce.h"
#include "sid.h"

namespace Gambo::ASRA {

using namespace Stockfish;

ResourceAllocation ResourceAllocator::compute_allocation(const Position& pos, const Search::Stack* ss,
                                                        int depth, const Search::PVMoves* pv) {
    ResourceAllocation alloc;

    // Get PCE-based allocation
    ResourceAllocation pce_alloc = compute_from_pce(pos);

    // Get SID-based allocation
    ResourceAllocation sid_alloc = compute_from_sid(pos);

    // Combine both
    alloc = combine_allocations(pce_alloc, sid_alloc);

    // Apply limits
    alloc.depth_bonus = std::min(alloc.depth_bonus, config.max_depth_bonus);
    alloc.node_budget_mult = std::min(alloc.node_budget_mult, config.max_node_mult);
    alloc.time_budget_mult = std::min(alloc.time_budget_mult, config.max_time_mult);

    // Update stats
    if (last_pce_level >= PCE::CriticalityLevel::HIGH) stats.critical_positions++;
    if (last_sid_level >= SID::InstabilityLevel::MODERATE) stats.unstable_positions++;
    if (alloc.enable_verification) stats.verification_triggered++;
    if (alloc.depth_bonus > 0) stats.depth_bonus_applied++;

    return alloc;
}

ResourceAllocation ResourceAllocator::compute_from_pce(const Position& pos) {
    ResourceAllocation alloc;

    if (last_pce_score < config.pce_low_threshold) {
        // LOW - standard search
        alloc.depth_bonus = 0;
        alloc.node_budget_mult = 100;
    } else if (last_pce_score < config.pce_medium_threshold) {
        // MEDIUM - slight increase
        alloc.depth_bonus = 1;
        alloc.node_budget_mult = 110;
    } else if (last_pce_score < config.pce_high_threshold) {
        // HIGH - significant increase
        alloc.depth_bonus = 2;
        alloc.node_budget_mult = 130;
        alloc.enable_verification = true;
        alloc.verification_depth = config.verification_base_depth;
    } else {
        // EXTREME - maximum resources
        alloc.depth_bonus = 3;
        alloc.node_budget_mult = 150;
        alloc.time_budget_mult = 120;
        alloc.enable_verification = true;
        alloc.verification_depth = config.verification_max_depth;
    }

    return alloc;
}

ResourceAllocation ResourceAllocator::compute_from_sid(const Position& pos) {
    ResourceAllocation alloc;

    switch (last_sid_level) {
        case SID::InstabilityLevel::STABLE:
            alloc.depth_bonus = 0;
            alloc.node_budget_mult = 100;
            break;

        case SID::InstabilityLevel::MINOR:
            alloc.depth_bonus = 1;
            alloc.node_budget_mult = 110;
            break;

        case SID::InstabilityLevel::MODERATE:
            alloc.depth_bonus = 1;
            alloc.node_budget_mult = 120;
            alloc.enable_verification = true;
            alloc.verification_depth = config.verification_base_depth;
            break;

        case SID::InstabilityLevel::HIGH:
            alloc.depth_bonus = 2;
            alloc.node_budget_mult = 140;
            alloc.time_budget_mult = 110;
            alloc.enable_verification = true;
            alloc.verification_depth = config.verification_base_depth + 4;
            break;

        case SID::InstabilityLevel::CRITICAL:
            alloc.depth_bonus = 3;
            alloc.node_budget_mult = 160;
            alloc.time_budget_mult = 130;
            alloc.enable_verification = true;
            alloc.verification_depth = config.verification_max_depth;
            break;
    }

    return alloc;
}

ResourceAllocation ResourceAllocator::combine_allocations(const ResourceAllocation& a,
                                                         const ResourceAllocation& b) {
    ResourceAllocation result;

    // Take maximum of depth bonuses
    result.depth_bonus = std::max(a.depth_bonus, b.depth_bonus);

    // Multiply node budgets (but cap at 200%)
    result.node_budget_mult = std::min(200, (a.node_budget_mult * b.node_budget_mult) / 100);

    // Take maximum time multiplier
    result.time_budget_mult = std::max(a.time_budget_mult, b.time_budget_mult);

    // Enable verification if either suggests it
    result.enable_verification = a.enable_verification || b.enable_verification;
    result.verification_depth = std::max(a.verification_depth, b.verification_depth);

    return result;
}

void ResourceAllocator::apply_allocation(ResourceAllocation& alloc, ThreadPool& threads) {
    // Apply depth bonus to search parameters
    // This would integrate with the actual search parameters
    // For now, just store for reference
    (void)alloc;
    (void)threads;
}

void ResourceAllocator::update_pce(const PCE::PositionCriticalityEngine& pce, int score, PCE::CriticalityLevel level) {
    last_pce_score = score;
    last_pce_level = level;
}

void ResourceAllocator::update_sid(const SID::SearchInstabilityDetector& sid, SID::InstabilityLevel level) {
    last_sid_level = level;
}

}  // namespace Gambo::ASRA