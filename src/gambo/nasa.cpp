/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// NASA — NUMA-Aware Search Allocation implementation

#include "nasa.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <malloc.h>
#elif defined(__linux__)
#include <numa.h>
#include <numaif.h>
#include <sched.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#endif

namespace Gambo::NASA {

using namespace Stockfish;

void NUMAManager::init() {
    detect_topology();
}

void NUMAManager::detect_topology() {
#if defined(__linux__)
    detect_linux();
#elif defined(_WIN32)
    detect_windows();
#elif defined(__APPLE__)
    detect_macos();
#else
    fallback_single_node();
#endif
}

void NUMAManager::detect_linux() {
#if defined(__linux__) && defined(HAVE_NUMA)
    if (numa_available() < 0) {
        fallback_single_node();
        return;
    }

    info.available = true;
    int max_node = numa_max_node();
    info.node_count = max_node + 1;

    info.nodes.resize(info.node_count);
    for (int i = 0; i < info.node_count; ++i) {
        info.nodes[i].id = i;
        info.nodes[i].cpu_count = 0;
        info.nodes[i].memory_size = numa_node_size64(i, nullptr);

        // Get CPUs for this node
        struct bitmask* cpumask = numa_allocate_cpumask();
        if (numa_node_to_cpus(i, cpumask) == 0) {
            for (int cpu = 0; cpu < cpumask->size; ++cpu) {
                if (numa_bitmask_isbitset(cpumask, cpu)) {
                    info.nodes[i].cpus.push_back(cpu);
                    info.cpu_to_node[cpu] = i;
                    info.nodes[i].cpu_count++;
                }
            }
        }
        numa_free_cpumask(cpumask);
    }

    // Determine current node
    int cpu = sched_getcpu();
    if (cpu >= 0) info.current_node = info.cpu_to_node[cpu];
#else
    fallback_single_node();
#endif
}

void NUMAManager::detect_windows() {
#if defined(_WIN32)
    // Windows NUMA support
    ULONG highest_node = 0;
    if (GetNumaHighestNodeNumber(&highest_node)) {
        info.available = true;
        info.node_count = highest_node + 1;

        info.nodes.resize(info.node_count);
        for (ULONG i = 0; i <= highest_node; ++i) {
            info.nodes[i].id = i;
            info.nodes[i].cpu_count = 0;
            info.nodes[i].memory_size = 0;

            GROUP_AFFINITY affinity = {};
            affinity.Group = 0;
            affinity.Mask = 1ULL << i;  // simplified

            // Get processors for this node
            UCHAR buffer[1024];
            PNUMA_NODE_RELATIONSHIP node_rel = (PNUMA_NODE_RELATIONSHIP)buffer;
            DWORD length = 1024;
            (void)node_rel;
            (void)length;
            if (GetNumaNodeProcessorMaskEx(i, &affinity)) {
                // Count bits
                for (int cpu = 0; cpu < 64; ++cpu) {
                    if (affinity.Mask & (1ULL << cpu)) {
                        info.nodes[i].cpus.push_back(cpu);
                        info.cpu_to_node[cpu] = i;
                        info.nodes[i].cpu_count++;
                    }
                }
            }
        }

        // Current thread's node
        PROCESSOR_NUMBER proc_num;
        GetCurrentProcessorNumberEx(&proc_num);
        info.current_node = info.cpu_to_node[proc_num.Number];
    } else {
        fallback_single_node();
    }
#else
    fallback_single_node();
#endif
}

void NUMAManager::detect_macos() {
#if defined(__APPLE__)
    // macOS doesn't have traditional NUMA but has core complexes
    int cores = 0;
    size_t len = sizeof(cores);
    if (sysctlbyname("hw.physicalcpu", &cores, &len, nullptr, 0) == 0) {
        info.available = false;  // No NUMA on macOS typically
        info.node_count = 1;
        info.current_node = 0;
        info.nodes.resize(1);
        info.nodes[0].id = 0;
        info.nodes[0].cpu_count = cores;
        info.nodes[0].memory_size = 0;
        for (int i = 0; i < cores; ++i) {
            info.nodes[0].cpus.push_back(i);
            info.cpu_to_node[i] = 0;
        }
    } else {
        fallback_single_node();
    }
#else
    fallback_single_node();
#endif
}

void NUMAManager::fallback_single_node() {
    info.available = false;
    info.node_count = 1;
    info.current_node = 0;
    info.nodes.resize(1);
    info.nodes[0].id = 0;
    info.nodes[0].cpu_count = std::thread::hardware_concurrency();
    info.nodes[0].memory_size = 0;
    for (int i = 0; i < info.nodes[0].cpu_count; ++i) {
        info.nodes[0].cpus.push_back(i);
        info.cpu_to_node[i] = 0;
    }
}

int NUMAManager::assign_thread_to_node(int thread_idx, int preferred_node) const {
    if (!info.available || info.node_count <= 1) return 0;

    if (preferred_node >= 0 && preferred_node < info.node_count) {
        return preferred_node;
    }

    // Round-robin assignment
    return thread_idx % info.node_count;
}

std::vector<int> NUMAManager::get_cpus_for_node(int node) const {
    if (node >= 0 && node < (int)info.nodes.size()) {
        return info.nodes[node].cpus;
    }
    return {};
}

void* NUMAManager::allocate_numa(size_t size, int node) {
#if defined(__linux__) && defined(HAVE_NUMA)
    if (info.available && node >= 0 && node < info.node_count) {
        return numa_alloc_onnode(size, node);
    }
#endif

#if defined(_WIN32)
    if (info.available && node >= 0 && node < info.node_count) {
        return VirtualAllocExNuma(GetCurrentProcess(), nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE, node);
    }
    return _aligned_malloc(size, 64);
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, 64, size) == 0) {
        return ptr;
    }
    return nullptr;
#endif
}

void NUMAManager::deallocate_numa(void* ptr, size_t size, int node) {
    (void)size;
    (void)node;
#if defined(__linux__) && defined(HAVE_NUMA)
    if (info.available && node >= 0 && node < info.node_count) {
        numa_free(ptr, size);
        return;
    }
#endif

#if defined(_WIN32)
    if (info.available) {
        VirtualFree(ptr, 0, MEM_RELEASE);
        return;
    }
    _aligned_free(ptr);
    return;
#else
    std::free(ptr);
#endif
}

}  // namespace Gambo::NASA