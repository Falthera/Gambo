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

// HSWS - Heterogeneous Search Worker Scheduling
// Specialized workers for different search tasks

#ifndef GAMBO_HSWS_H_INCLUDED
#define GAMBO_HSWS_H_INCLUDED

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

#include "../types.h"
#include "../position.h"
#include "../thread.h"

namespace Stockfish {
class Position;
class Stack;

namespace HSWS {

// Worker specialization types
enum class WorkerType : uint8_t {
    MAIN = 0,                 // Main thread - handles root
    PV_SEARCHER = 1,          // Searches principal variation
    TACTICAL = 2,             // Tactical verification, sacrifices
    ENDGAME = 3,              // Endgame specialist
    ROOT_ANALYZER = 4,        // Root candidate analysis
    TACTICAL_VERIFIER = 5,    // TVS verification
    HELPER = 6                // General helper
};

// Worker state
struct WorkerState {
    WorkerType type = WorkerType::HELPER;
    int threadId = -1;
    std::atomic<bool> busy{false};
    std::atomic<bool> assigned{false};
    Position* currentPos = nullptr;
    Stack* currentStack = nullptr;
    Depth currentDepth = 0;
    Value currentAlpha = 0, currentBeta = 0;
    Move currentMove = Move::none();
    int64_t nodesSearched = 0;
    int64_t startTime = 0;
    int64_t timeBudget = 0;
    bool hasResult = false;
    Value resultScore = 0;
    Move resultMove = Move::none();
    bool shouldStop = false;
};

// Heterogeneous scheduler
class Scheduler {
public:
    struct Config {
        bool enabled = true;
        int minSpecializedWorkers = 2;
        int maxSpecializedWorkers = 4;
        bool dynamicReassignment = true;
        int reassignmentIntervalMs = 100;
    };
    
    static Config config;
    
    // Initialize scheduler
    static void init_hsws(ThreadPool& pool);
    
    // Assign work to specialized workers
    static void assign_work(WorkerType type, const Position& pos, Stack* ss, 
                           Depth depth, Value alpha, Value beta, Move move);
    
    // Get available worker of given type
    static int get_available_worker(WorkerType type);
    
    // Reassign workers based on position characteristics
    static void reassign_workers(const Position& pos);
    
    // Wait for worker completion
    static bool wait_for_worker(int workerId, int timeoutMs);
    
    // Get worker state
    static const WorkerState& get_worker_state(int workerId);
    
    // Statistics
    struct Stats {
        std::uint64_t totalAssignments = 0;
        std::uint64_t successfulCompletions = 0;
        std::uint64_t timeouts = 0;
        std::uint64_t reassignments = 0;
        int avgWaitTimeMs = 0;
    };
    
    static const Stats& get_stats();
    
    static void init_hsws();
    static void shutdown();
    
private:
    static std::vector<WorkerState> workers;
    static std::vector<std::thread> workerThreads;
    static std::mutex schedulerMutex;
    static std::condition_variable workCV;
    static std::atomic<bool> shutdownFlag;
    
    static void worker_loop(int workerId);
    static void assign_to_worker(int workerId, WorkerType type, const Position& pos, Stack* ss,
                                 Depth depth, Value alpha, Value beta, Move move);
};

}  // namespace HSWS
}  // namespace Stockfish

#endif  // GAMBO_HSWS_H_INCLUDED