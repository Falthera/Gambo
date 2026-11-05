/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// HSWS — Heterogeneous Search Worker Scheduling implementation

#include "hsws.h"

#include <chrono>

#include "../position.h"
#include "../search.h"
#include "../thread.h"
#include "../types.h"

namespace Gambo::HSWS {

using namespace Stockfish;

HeterogeneousWorkerPool::HeterogeneousWorkerPool() = default;

HeterogeneousWorkerPool::~HeterogeneousWorkerPool() {
    stop();
}

void HeterogeneousWorkerPool::init(const HSWSConfig& cfg) {
    config = cfg;
}

void HeterogeneousWorkerPool::start() {
    running = true;

    // Start worker threads based on config
    int worker_id = 0;

    // Main alpha-beta is handled by main thread, not a worker

    // Tactical verification workers
    for (int i = 0; i < config.max_verification_workers; ++i) {
        workers.emplace_back(&HeterogeneousWorkerPool::worker_loop, this,
                            WorkerType::TACTICAL_VERIFICATION, worker_id++);
    }

    // Root candidate analysis workers
    for (int i = 0; i < config.max_candidate_workers; ++i) {
        workers.emplace_back(&HeterogeneousWorkerPool::worker_loop, this,
                            WorkerType::ROOT_CANDIDATE_ANALYSIS, worker_id++);
    }

    // Position criticality workers
    for (int i = 0; i < config.max_criticality_workers; ++i) {
        workers.emplace_back(&HeterogeneousWorkerPool::worker_loop, this,
                            WorkerType::POSITION_CRITICALITY, worker_id++);
    }

    // Instability detection workers
    for (int i = 0; i < config.max_instability_workers; ++i) {
        workers.emplace_back(&HeterogeneousWorkerPool::worker_loop, this,
                            WorkerType::INSTABILITY_DETECTION, worker_id++);
    }

    // Mate verification workers
    for (int i = 0; i < config.max_mate_workers; ++i) {
        workers.emplace_back(&HeterogeneousWorkerPool::worker_loop, this,
                            WorkerType::MATE_VERIFICATION, worker_id++);
    }

    // Threat context workers
    for (int i = 0; i < config.max_threat_workers; ++i) {
        workers.emplace_back(&HeterogeneousWorkerPool::worker_loop, this,
                            WorkerType::THREAT_CONTEXT, worker_id++);
    }
}

void HeterogeneousWorkerPool::stop() {
    running = false;

    // Wake all workers
    for (auto& cv : queue_cvs) cv.notify_all();

    // Join all threads
    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }
    workers.clear();
}

bool HeterogeneousWorkerPool::submit_work(const WorkItem& item) {
    int idx = get_queue_index(item.type);
    if (idx < 0 || idx >= 8) return false;

    std::lock_guard<std::mutex> lock(queue_mutexes[idx]);
    if (work_queues[idx].size() >= config.task_queue_size) {
        stats.total_submitted++;
        stats.queue_overflows++;
        return false;
    }

    work_queues[idx].push(item);
    stats.worker_stats[idx].tasks_pending++;
    stats.total_submitted++;

    queue_cvs[idx].notify_one();
    return true;
}

bool HeterogeneousWorkerPool::try_get_result(WorkItem& result) {
    std::lock_guard<std::mutex> lock(result_mutex);
    if (result_queue.empty()) return false;

    result = result_queue.front();
    result_queue.pop();
    return true;
}

void HeterogeneousWorkerPool::worker_loop(WorkerType type, int worker_id) {
    int idx = get_queue_index(type);

    while (running) {
        WorkItem item;
        bool has_work = false;

        {
            std::unique_lock<std::mutex> lock(queue_mutexes[idx]);
            queue_cvs[idx].wait_for(lock, std::chrono::milliseconds(config.worker_poll_ms),
                                   [&] { return !work_queues[idx].empty() || !running; });

            if (!running && work_queues[idx].empty()) break;

            if (!work_queues[idx].empty()) {
                item = work_queues[idx].front();
                work_queues[idx].pop();
                stats.worker_stats[idx].tasks_pending--;
                has_work = true;
            }
        }

        if (has_work) {
            auto start = std::chrono::steady_clock::now();

            try {
                switch (type) {
                    case WorkerType::TACTICAL_VERIFICATION:
                        process_tactical_verification(item);
                        break;
                    case WorkerType::ROOT_CANDIDATE_ANALYSIS:
                        process_root_candidate(item);
                        break;
                    case WorkerType::POSITION_CRITICALITY:
                        process_position_criticality(item);
                        break;
                    case WorkerType::INSTABILITY_DETECTION:
                        process_instability_detection(item);
                        break;
                    case WorkerType::MATE_VERIFICATION:
                        process_mate_verification(item);
                        break;
                    case WorkerType::THREAT_CONTEXT:
                        process_threat_context(item);
                        break;
                    default:
                        break;
                }

                stats.worker_stats[idx].tasks_completed++;
            } catch (...) {
                stats.worker_stats[idx].tasks_failed++;
            }

            auto end = std::chrono::steady_clock::now();
            stats.worker_stats[idx].total_time_ms +=
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            // Push result
            if (item.callback) {
                std::lock_guard<std::mutex> lock(result_mutex);
                result_queue.push(item);
                result_cv.notify_one();
            }

            stats.total_completed++;
        }
    }
}

void HeterogeneousWorkerPool::process_tactical_verification(const WorkItem& item) {
    // Would call TVS::TacticalVerification here
    // Placeholder
    if (item.callback) item.callback();
}

void HeterogeneousWorkerPool::process_root_candidate(const WorkItem& item) {
    // Would call CCA::CounterfactualAnalyzer or RCRA::RootCandidateAnalyzer here
    if (item.callback) item.callback();
}

void HeterogeneousWorkerPool::process_position_criticality(const WorkItem& item) {
    // Would call PCE::PositionCriticalityEngine here
    if (item.callback) item.callback();
}

void HeterogeneousWorkerPool::process_instability_detection(const WorkItem& item) {
    // Would call SID::SearchInstabilityDetector here
    if (item.callback) item.callback();
}

void HeterogeneousWorkerPool::process_mate_verification(const WorkItem& item) {
    // Would call PNMV::ProofNumberMateVerifier here
    if (item.callback) item.callback();
}

void HeterogeneousWorkerPool::process_threat_context(const WorkItem& item) {
    // Would call PTC::ThreatContextManager here
    if (item.callback) item.callback();
}

}  // namespace Gambo::HSWS