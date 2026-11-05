/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// HSWS — Heterogeneous Search Worker Scheduling
// Specialised search workers for different tasks

#ifndef GAMBO_HSWS_H_INCLUDED
#define GAMBO_HSWS_H_INCLUDED

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "../position.h"
#include "../search.h"
#include "../thread.h"
#include "../types.h"

namespace Gambo::HSWS {

using namespace Stockfish;

enum class WorkerType {
    MAIN_ALPHABETA = 0,      // Standard alpha-beta search
    TACTICAL_VERIFICATION,   // TVS worker
    ROOT_CANDIDATE_ANALYSIS, // CCA/RCRA worker
    POSITION_CRITICALITY,    // PCE worker
    INSTABILITY_DETECTION,   // SID worker
    MATE_VERIFICATION,       // PNMV worker
    THREAT_CONTEXT,          // PTC worker
};

struct WorkItem {
    WorkerType type;
    Position pos;
    Search::Stack* ss;
    int depth;
    Move move;           // for candidate analysis
    Value alpha, beta;
    int priority;        // higher = more urgent
    int64_t submit_time;
    std::function<void()> callback;
};

struct WorkerStats {
    std::atomic<int> tasks_completed{0};
    std::atomic<int> tasks_failed{0};
    std::atomic<int64_t> total_time_ms{0};
    std::atomic<int> tasks_pending{0};
};

struct HSWSConfig {
    int max_verification_workers = 1;
    int max_candidate_workers = 1;
    int max_criticality_workers = 1;
    int max_instability_workers = 1;
    int max_mate_workers = 1;
    int max_threat_workers = 1;
    int task_queue_size = 256;
    int worker_poll_ms = 1;
};

class HeterogeneousWorkerPool {
public:
    HeterogeneousWorkerPool();
    ~HeterogeneousWorkerPool();

    void init(const HSWSConfig& cfg);
    void start();
    void stop();

    // Submit work to appropriate worker
    bool submit_work(const WorkItem& item);

    // Get results (non-blocking)
    bool try_get_result(WorkItem& result);

    // Main thread interface
    void set_main_thread(Thread* main) { main_thread = main; }

    // Statistics
    struct Stats {
        std::array<WorkerStats, 8> worker_stats;
        std::atomic<int> total_submitted{0};
        std::atomic<int> total_completed{0};
        std::atomic<int> queue_overflows{0};
    };
    const Stats& get_stats() const { return stats; }
    void reset_stats() { stats = {}; }

private:
    HSWSConfig config;
    Stats stats;
    Thread* main_thread = nullptr;

    std::vector<std::thread> workers;
    std::array<std::queue<WorkItem>, 8> work_queues;
    std::array<std::mutex, 8> queue_mutexes;
    std::array<std::condition_variable, 8> queue_cvs;

    std::mutex result_mutex;
    std::queue<WorkItem> result_queue;
    std::condition_variable result_cv;

    std::atomic<bool> running{false};

    void worker_loop(WorkerType type, int worker_id);
    void process_main_alphabeta(const WorkItem& item);
    void process_tactical_verification(const WorkItem& item);
    void process_root_candidate(const WorkItem& item);
    void process_position_criticality(const WorkItem& item);
    void process_instability_detection(const WorkItem& item);
    void process_mate_verification(const WorkItem& item);
    void process_threat_context(const WorkItem& item);

    int get_queue_index(WorkerType type) const { return static_cast<int>(type); }
};

}  // namespace Gambo::HSWS

#endif  // GAMBO_HSWS_H_INCLUDED