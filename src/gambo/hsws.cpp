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

// HSWS - Heterogeneous Search Worker Scheduling implementation

#include "hsws.h"
#include "thread.h"
#include "search.h"
#include "timeman.h"

namespace Stockfish {
namespace HSWS {

Scheduler::Config Scheduler::config;
std::vector<WorkerState> Scheduler::workers;
std::vector<std::thread> Scheduler::workerThreads;
std::mutex Scheduler::schedulerMutex;
std::condition_variable Scheduler::workCV;
std::atomic<bool> Scheduler::shutdownFlag{false};
struct HSWSStats {
    std::uint64_t totalAssignments = 0;
    std::uint64_t successfulCompletions = 0;
    std::uint64_t timeouts = 0;
    std::uint64_t reassignments = 0;
    int avgWaitTimeMs = 0;
} hswsStats;

void Scheduler::init_hsws() {
    config.enabled = true;
    config.minSpecializedWorkers = 2;
    config.maxSpecializedWorkers = 4;
    config.dynamicReassignment = true;
    config.reassignmentIntervalMs = 100;
    
    shutdownFlag = false;
    hswsStats = HSWSStats{};
    
    // Create worker threads (one per available thread - 1 for main)
    int numWorkers = Threads.size() - 1;
    if (numWorkers < 1) numWorkers = 1;
    
    workers.resize(numWorkers);
    for (int i = 0; i < numWorkers; ++i) {
        workers[i].threadId = i;
        workers[i].type = WorkerType::HELPER;
    }
    
    // Start worker threads
    workerThreads.resize(numWorkers);
    for (int i = 0; i < numWorkers; ++i) {
        workerThreads[i] = std::thread(&Scheduler::worker_loop, i);
    }
}

void Scheduler::shutdown() {
    shutdownFlag = true;
    workCV.notify_all();
    
    for (auto& t : workerThreads) {
        if (t.joinable()) t.join();
    }
    
    workerThreads.clear();
    workers.clear();
}

void Scheduler::init_hsws() {
    init_hsws(Threads);
}

void Scheduler::assign_work(WorkerType type, const Position& pos, Stack* ss, 
                           Depth depth, Value alpha, Value beta, Move move) {
    if (!config.enabled) return;
    
    std::lock_guard<std::mutex> lock(schedulerMutex);
    
    int workerId = get_available_worker(type);
    if (workerId >= 0) {
        assign_to_worker(workerId, type, pos, ss, depth, alpha, beta, move);
        hswsStats.totalAssignments++;
    }
}

int Scheduler::get_available_worker(WorkerType type) {
    // Prefer workers of matching type
    for (size_t i = 0; i < workers.size(); ++i) {
        if (!workers[i].busy.load() && 
            (workers[i].type == type || workers[i].type == WorkerType::HELPER)) {
            return static_cast<int>(i);
        }
    }
    
    // Fallback to any available worker
    for (size_t i = 0; i < workers.size(); ++i) {
        if (!workers[i].busy.load()) {
            return static_cast<int>(i);
        }
    }
    
    return -1; // No available workers
}

void Scheduler::reassign_workers(const Position& pos) {
    if (!config.dynamicReassignment) return;
    
    // Analyze position and adjust worker types
    Color us = pos.side_to_move();
    int material = pos.non_pawn_material(us) + pos.non_pawn_material(~us);
    bool isEndgame = material < 10000;
    int tacticalPressure = 0; // Would compute from position
    int kingDanger = 0; // Would compute from position
    
    std::lock_guard<std::mutex> lock(schedulerMutex);
    
    int specializedCount = 0;
    for (size_t i = 0; i < workers.size(); ++i) {
        if (workers[i].busy.load()) continue;
        
        // Reassign based on position
        if (specializedCount == 0) {
            // First available: MAIN or PV_SEARCHER
            workers[i].type = WorkerType::PV_SEARCHER;
        } else if (specializedCount == 1 && (tacticalPressure > 8 || kingDanger > 6)) {
            workers[i].type = WorkerType::TACTICAL;
        } else if (specializedCount == 2 && isEndgame) {
            workers[i].type = WorkerType::ENDGAME;
        } else if (specializedCount == 3) {
            workers[i].type = WorkerType::ROOT_ANALYZER;
        } else {
            workers[i].type = WorkerType::HELPER;
        }
        specializedCount++;
    }
    
    hswsStats.reassignments++;
}

bool Scheduler::wait_for_worker(int workerId, int timeoutMs) {
    if (workerId < 0 || workerId >= static_cast<int>(workers.size())) return false;
    
    auto start = std::chrono::steady_clock::now();
    while (workers[workerId].busy.load()) {
        if (shutdownFlag) return false;
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - std::chrono::steady_clock::now()).count();
        if (elapsed > timeoutMs) {
            hswsStats.timeouts++;
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    hswsStats.avgWaitTimeMs = (hswsStats.avgWaitTimeMs * hswsStats.successfulCompletions + waitTime) / 
                              (hswsStats.successfulCompletions + 1);
    hswsStats.successfulCompletions++;
    
    return true;
}

const WorkerState& Scheduler::get_worker_state(int workerId) {
    static WorkerState emptyState;
    if (workerId < 0 || workerId >= static_cast<int>(workers.size())) return emptyState;
    return workers[workerId];
}

void Scheduler::worker_loop(int workerId) {
    while (!shutdownFlag) {
        std::unique_lock<std::mutex> lock(schedulerMutex);
        workCV.wait_for(lock, std::chrono::milliseconds(10), [workerId, this] {
            return workers[workerId].assigned.load() || shutdownFlag;
        });
        
        if (shutdownFlag) break;
        
        if (workers[workerId].assigned.load()) {
            workers[workerId].assigned.store(false);
            workers[workerId].busy.store(true);
            lock.unlock();
            
            // Execute work based on type
            WorkerState& state = workers[workerId];
            
            if (state.type == WorkerType::PV_SEARCHER || state.type == WorkerType::HELPER) {
                // Search the assigned position
                if (state.currentPos && state.currentStack) {
                    state.resultScore = -Search::Worker::search<NonPV>(
                        *state.currentPos, state.currentStack, state.currentAlpha, state.currentBeta, 
                        state.currentDepth, false);
                    state.resultMove = state.currentMove;
                    state.hasResult = true;
                }
            } else if (state.type == WorkerType::TACTICAL) {
                // Tactical search
                if (state.currentPos && state.currentStack) {
                    // Simplified tactical search
                    state.resultScore = -state.currentPos->psqt_eval();
                    state.hasResult = true;
                }
            } else if (state.type == WorkerType::ENDGAME) {
                // Endgame search
                if (state.currentPos && state.currentStack) {
                    state.resultScore = -state.currentPos->psqt_eval();
                    state.hasResult = true;
                }
            } else if (state.type == WorkerType::ROOT_ANALYZER) {
                // Root candidate analysis
                if (state.currentPos && state.currentStack) {
                    // Simplified analysis
                    state.resultScore = -state.currentPos->psqt_eval();
                    state.hasResult = true;
                }
            }
            
            state.busy.store(false);
        }
    }
}

void Scheduler::assign_to_worker(int workerId, WorkerType type, const Position& pos, Stack* ss,
                                 Depth depth, Value alpha, Value beta, Move move) {
    if (workerId < 0 || workerId >= static_cast<int>(workers.size())) return;
    
    WorkerState& state = workers[workerId];
    state.type = type;
    state.currentPos = const_cast<Position*>(&pos);
    state.currentStack = ss;
    state.currentDepth = depth;
    state.currentAlpha = alpha;
    state.currentBeta = beta;
    state.currentMove = move;
    state.assigned.store(true);
    state.busy.store(true);
    state.hasResult = false;
    state.nodesSearched = 0;
    state.startTime = std::chrono::steady_clock::now().time_since_epoch().count();
    state.timeBudget = 1000; // ms
    state.shouldStop = false;
    
    workCV.notify_one();
}

const HSWS::Stats& Scheduler::get_stats() {
    return hswsStats;
}

}  // namespace HSWS
}  // namespace Stockfish