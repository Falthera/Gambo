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

// ASRA - Adaptive Search Resource Allocation implementation

#include "asra.h"
#include "thread.h"
#include "search.h"
#include "gambo/pce.h"
#include "gambo/sid.h"

namespace Stockfish {
namespace ASRA {

ASRAConfig asraConfig;

ResourceManager::ResourceManager(ThreadPool& p) : pool(p) {
    currentAllocations.fill({});
}

void ResourceManager::allocate_for_search(const Position& pos, Depth depth, int numWorkers,
                                          std::array<WorkerAllocation, MAX_THREADS>& allocations) {
    // Initialize default allocations
    for (int i = 0; i < numWorkers; ++i) {
        allocations[i] = WorkerAllocation{};
        allocations[i].threads = 1;
        allocations[i].timeFraction = 1.0 / numWorkers;
        allocations[i].nodeBudget = asraConfig.baseNodeBudget / numWorkers;
        allocations[i].priority = numWorkers - i;  // Higher priority for lower indices
        allocations[i].specialization = WorkerAllocation::Specialization::GENERAL;
    }
    
    // Compute base allocation from PCE/SID
    compute_base_allocation(pos, depth, allocations);
    
    // Adjust for criticality
    adjust_for_criticality(pos, allocations);
    
    // Adjust for instability
    adjust_for_instability(pos, allocations);
    
    // Specialize workers
    specialize_workers(pos, allocations);
    
    // Store current allocations
    currentAllocations = allocations;
}

void ResourceManager::reallocate_runtime(const Position& pos, int completedDepth,
                                         const std::array<WorkerAllocation, MAX_THREADS>& currentAllocations) {
    stats.totalReallocations++;
    
    // Recompute allocations based on current position state
    std::array<WorkerAllocation, MAX_THREADS> newAllocations;
    for (int i = 0; i < MAX_THREADS; ++i) {
        newAllocations[i] = currentAllocations[i];
    }
    
    // Adjust for criticality changes
    adjust_for_criticality(pool.get_main_position(), newAllocations);
    
    // Adjust for instability changes
    adjust_for_instability(pool.get_main_position(), newAllocations);
    
    // Check if any worker needs migration
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (newAllocations[i].specialization != currentAllocations[i].specialization) {
            stats.totalThreadMigrations++;
        }
    }
    
    // Update current allocations
    this->currentAllocations = newAllocations;
}

void ResourceManager::compute_base_allocation(const Position& pos, Depth depth,
                                              std::array<WorkerAllocation, MAX_THREADS>& allocations) {
    // Base allocation: equal distribution
    int numWorkers = 0;
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (allocations[i].timeFraction > 0) numWorkers++;
    }
    
    if (numWorkers == 0) return;
    
    double baseTimeFraction = 1.0 / numWorkers;
    uint64_t baseNodeBudget = asraConfig.baseNodeBudget / numWorkers;
    
    for (int i = 0; i < numWorkers; ++i) {
        allocations[i].timeFraction = baseTimeFraction;
        allocations[i].nodeBudget = baseNodeBudget;
        allocations[i].priority = numWorkers - i;
    }
}

void ResourceManager::adjust_for_criticality(const Position& pos,
                                             std::array<WorkerAllocation, MAX_THREADS>& allocations) {
    // Use PCE to determine criticality
    Gambo::PCE::PositionCriticalityEngine pce;
    auto criticality = pce.compute_criticality(pos);
    
    if (criticality >= Gambo::PCE::CriticalityLevel::HIGH) {
        // High criticality: give more time to main thread and tactical workers
        for (int i = 0; i < MAX_THREADS; ++i) {
            if (allocations[i].timeFraction > 0) {
                double multiplier = asraConfig.criticalityTimeMultiplier;
                
                // Main thread (index 0) gets most boost
                if (i == 0) {
                    multiplier *= 1.5;
                }
                
                // Tactical specialists get boost
                if (allocations[i].specialization == WorkerAllocation::Specialization::TACTICAL ||
                    allocations[i].specialization == WorkerAllocation::Specialization::TACTICAL_VERIFICATION) {
                    multiplier *= 1.3;
                }
                
                allocations[i].timeFraction *= multiplier;
                allocations[i].nodeBudget = uint64_t(allocations[i].nodeBudget * multiplier);
            }
        }
    }
}

void ResourceManager::adjust_for_instability(const Position& pos,
                                             std::array<WorkerAllocation, MAX_THREADS>& allocations) {
    // Use SID to determine instability
    Gambo::SID::SearchInstabilityDetector sid;
    // Note: In real implementation, we'd pass the actual SID state
    // For now, use a placeholder instability level
    auto instability = Gambo::SID::InstabilityLevel::LOW;  // Placeholder
    
    if (instability >= Gambo::SID::InstabilityLevel::MEDIUM) {
        double multiplier = asraConfig.instabilityTimeMultiplier;
        
        for (int i = 0; i < MAX_THREADS; ++i) {
            if (allocations[i].timeFraction > 0) {
                // Verification workers get more resources in unstable positions
                if (allocations[i].specialization == WorkerAllocation::Specialization::TACTICAL_VERIFICATION) {
                    allocations[i].timeFraction *= multiplier * 1.5;
                    allocations[i].nodeBudget = uint64_t(allocations[i].nodeBudget * multiplier * 1.5);
                } else {
                    allocations[i].timeFraction *= multiplier;
                    allocations[i].nodeBudget = uint64_t(allocations[i].nodeBudget * multiplier);
                }
            }
        }
    }
}

void ResourceManager::specialize_workers(const Position& pos,
                                         std::array<WorkerAllocation, MAX_THREADS>& allocations) {
    // Analyze position characteristics
    Color us = pos.side_to_move();
    int material = pos.non_pawn_material(us) + pos.non_pawn_material(~us);
    bool isEndgame = material < 10000;
    int tacticalPressure = attack_pressure(pos, us);
    int kingDanger = king_danger(pos, us);
    
    int numWorkers = 0;
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (allocations[i].timeFraction > 0) numWorkers++;
    }
    
    if (numWorkers <= 1) return;
    
    // Worker 0: Main thread - always GENERAL (handles root moves)
    // Worker 1: Tactical specialist if position is tactical
    if (numWorkers >= 2 && (tacticalPressure > 10 || kingDanger > 8)) {
        allocations[1].specialization = WorkerAllocation::Specialization::TACTICAL;
        allocations[1].priority += 5;
    }
    
    // Worker 2: Endgame specialist if endgame
    if (numWorkers >= 3 && isEndgame) {
        allocations[2].specialization = WorkerAllocation::Specialization::ENDGAME;
        allocations[2].priority += 3;
    }
    
    // Worker 3: Tactical verification if unstable
    if (numWorkers >= 4) {
        // In real implementation, check SID instability
        allocations[3].specialization = WorkerAllocation::Specialization::TACTICAL_VERIFICATION;
        allocations[3].priority += 4;
    }
    
    // Worker 4+: Root analysis
    for (int i = 5; i < numWorkers; ++i) {
        allocations[i].specialization = WorkerAllocation::Specialization::ROOT_ANALYSIS;
    }
}

void init_asra() {
    asraConfig.enabled = true;
    asraConfig.minThreadsPerWorker = 1;
    asraConfig.maxThreadsPerWorker = 4;
    asraConfig.minTimeFraction = 0.1;
    asraConfig.maxTimeFraction = 0.8;
    asraConfig.criticalityTimeMultiplier = 2.0;
    asraConfig.instabilityTimeMultiplier = 1.5;
    asraConfig.baseNodeBudget = 1000000;
    asraConfig.maxNodeBudget = 100000000;
}

const ASRAConfig& asra_config() {
    return asraConfig;
}

}  // namespace ASRA
}  // namespace Stockfish