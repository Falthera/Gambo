/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// NASA — NUMA-Aware Search Allocation
// Detects NUMA topology and allocates threads/memory optimally

#ifndef GAMBO_NASA_H_INCLUDED
#define GAMBO_NASA_H_INCLUDED

#include <array>
#include <cstdint>
#include <vector>

#include "../types.h"

namespace Gambo::NASA {

struct NUMANode {
    int id;
    int cpu_start;
    int cpu_count;
    size_t memory_size;  // in bytes
    std::vector<int> cpus;
};

struct NUMAInfo {
    bool available = false;
    int node_count = 1;
    int current_node = 0;
    std::vector<NUMANode> nodes;
    std::array<int, 256> cpu_to_node;  // maps CPU index to NUMA node
};

class NUMAManager {
public:
    NUMAManager() = default;

    void init();
    void detect_topology();

    const NUMAInfo& get_info() const { return info; }

    // Thread allocation
    int assign_thread_to_node(int thread_idx, int preferred_node = -1) const;
    std::vector<int> get_cpus_for_node(int node) const;

    // Memory allocation helpers
    void* allocate_numa(size_t size, int node);
    void deallocate_numa(void* ptr, size_t size, int node);

    // Check if NUMA is beneficial
    bool is_numa_beneficial() const { return info.available && info.node_count > 1; }

private:
    NUMAInfo info;

    void detect_linux();
    void detect_windows();
    void detect_macos();
    void fallback_single_node();
};

}  // namespace Gambo::NASA

#endif  // GAMBO_NASA_H_INCLUDED