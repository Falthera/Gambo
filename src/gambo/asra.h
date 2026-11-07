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

// ASRA - Adaptive Search Resource Allocation
// Dynamic resource allocation based on PCE/SID

#ifndef GAMBO_ASRA_H_INCLUDED
#define GAMBO_ASRA_H_INCLUDED

#include <array>
#include <atomic>
#include <cstdint>

#include "../types.h"

namespace Stockfish {
class Position;
class Stack;
class ThreadPool;

namespace ASRA {

// Resource allocation configuration
struct ASRAConfig {
    bool enabled = true;
    
    // Thread allocation
    int minThreadsPerWorker = 1;
    int maxThreadsPerWorker = 4;
    
    // Time allocation
    double minTimeFraction = 0.1;    // Minimum fraction of time per worker
    double maxTimeFraction = 0.8;    // Maximum fraction of time per worker
    
    // Criticality-based allocation
    double criticalityTimeMultiplier = 2.0;  // Time multiplier for high criticality
    double instabilityTimeMultiplier = 1.5;  // Time multiplier for high instability
    
    // Node budget
    uint64_t baseNodeBudget = 1000000;
    uint64_t maxNodeBudget = 100000000;
};

// Worker resource allocation
struct WorkerAllocation {
    int threads = 1;
    double timeFraction = 1.0;
    uint64_t nodeBudget = 1000000;
    int priority = 0;  // Higher = more resources
    
    // Specialization
    enum class Specialization : uint8_t {
        GENERAL = 0,
        TACTICAL = 1,
        POSITIONAL = 2,
        ENDGAME = 3,
        ROOT_ANALYSIS = 4,
        TACTICAL_VERIFICATION = 5
    } specialization = Specialization::GENERAL;
};

// Global config
extern ASRAConfig asraConfig;

// Thread pool resource manager
class ResourceManager {
public:
    explicit ResourceManager(ThreadPool& pool);
    
    // Allocate resources for a search
    void allocate_for_search(const Position& pos, Depth depth, int numWorkers,
                             std::array<WorkerAllocation, MAX_THREADS>& allocations);
    
    // Reallocate based on runtime feedback
    void reallocate_runtime(const Position& pos, int completedDepth,
                            const std::array<WorkerAllocation, MAX_THREADS>& currentAllocations);
    
    // Get resource statistics
    struct Stats {
        std::uint64_t totalReallocations = 0;
        std::uint64_t totalThreadMigrations = 0;
        double avgTimeEfficiency = 0.0;
        double avgNodeEfficiency = 0.0;
    };
    
    const Stats& get_stats() const { return stats; }
    
private:
    ThreadPool& pool;
    struct Stats stats;
    std::array<WorkerAllocation, MAX_THREADS> currentAllocations;
    
    // Compute base allocation from PCE/SID
    void compute_base_allocation(const Position& pos, Depth depth,
                                 std::array<WorkerAllocation, MAX_THREADS>& allocations);
    
    // Adjust for criticality
    void adjust_for_criticality(const Position& pos, 
                                std::array<WorkerAllocation, MAX_THREADS>& allocations);
    
    // Adjust for instability
    void adjust_for_instability(const Position& pos,
                                std::array<WorkerAllocation, MAX_THREADS>& allocations);
    
    // Specialize workers based on position type
    void specialize_workers(const Position& pos,
                            std::array<WorkerAllocation, MAX_THREADS>& allocations);
};

// Initialize ASRA subsystem
void init_asra();

// Get current config
const ASRAConfig& asra_config();

}  // namespace ASRA
}  // namespace Stockfish

#endif  // GAMBO_ASRA_H_INCLUDED