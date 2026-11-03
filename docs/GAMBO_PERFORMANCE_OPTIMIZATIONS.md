# GAMBO Performance Optimizations (Tier 2)

## Tier 2: Performance and Memory Optimizations

### 13. INFU — Incremental NNUE Feature Update

**Purpose**: Update only affected features after moves.

**Implementation**:
- All 4 GAMBO feature sets implement `append_changed_indices()`:
  - **PawnPair**: Tracks pawn moves, captures, promotions, en passant
  - **KingRelative**: Tracks piece moves, captures, promotions
  - **ThreatDetection**: Tracks attacker-target-king triplet changes
  - **PieceRelative**: Tracks outposts, pawn chains, rook batteries, open files

**UCI Options**: `GAMBO.IncrementalNNUE` (default: on)

**Verification**: Compare incremental vs full refresh on test positions

### 14. NFAR — NNUE Fast Accumulator Refresh

**Purpose**: Efficient accumulator reconstruction.

**Implementation**:
- `AccumulatorCaches` (Finny Tables) for cached intermediate results
- Refresh scheduling: only refresh when `requires_refresh()` returns true
- SIMD-optimized paths in `update_*_refresh_cache()` functions
- Fallback to full refresh when incremental fails

**UCI Option**: `GAMBO.FastAccumulatorRefresh` (default: on)

### 15. SONI — SIMD-Optimised NNUE Inference

**Purpose**: Accelerate NNUE forward pass with CPU vector instructions.

**Implementation** (`gambo/nnue/layers/affine_transform.cpp`):
- **AVX2**: 8 elements at a time using `_mm256_madd_epi16` + horizontal sum
- **SSE4.1**: 4 elements at a time using `_mm_madd_epi16` + horizontal sum
- **Scalar fallback**: Portable C++ implementation
- Compile-time detection via `GAMBO_USE_SIMD` macro

**Activation**: `GAMBO.SIMDNNUE` (default: on)

**Validation**: Numerical consistency vs scalar reference

### 16. SMP — Scheduled Memory Prefetching

**Purpose**: Improve memory-access scheduling for TT, NNUE, and search stack.

**Implementation**:
- **TT Prefetch** (`tt.h`/`tt.cpp`): `TranspositionTable::prefetch(key)` uses `_mm_prefetch`/`__builtin_prefetch`
- **NNUE Weight Prefetch**: Sequential access pattern in layer propagation
- **Search Stack**: Sequential access, hardware prefetcher sufficient

**UCI Option**: `GAMBO.PrefetchTT` (default: off - hardware prefetcher often sufficient)

### 17. CATT — Cache-Aware Transposition Table

**Purpose**: Optimize TT memory locality.

**Implementation** (`tt.h`/`tt.cpp`):
- `TTLayout` enum: `Standard(0)`, `Compact(1)`
- Cluster size: 3 entries (32 bytes = cache line)
- Cache-line aligned allocation via `aligned_large_pages_alloc`
- Layout change requires table resize

**UCI Option**: `GAMBO.CompactTT` (default: off - standard layout well-tuned)

### 18. CTTEL — Compact TT Entry Layout

**Purpose**: Reduce TT memory footprint.

**Current Layout** (Standard, 10 bytes):
```
key16 (16) | depth8 (8) | genBound8 (8) | move16 (16) | value16 (16) | eval16 (16) = 80 bits + padding
```

**Compact Layout** (Planned - 8 bytes):
```
key12 (12) | depth6 (6) | bound2 (2) | gen4 (4) | pv1 (1) | move16 (16) | value12 (12) | eval12 (12) = 64 bits
```

**Trade-offs**: Reduced depth range, generation tracking, mate score encoding complexity

**UCI Option**: `GAMBO.CompactTT` (default: off)

### 19. IAMM — Incremental Attack Map Maintenance

**Purpose**: Maintain attack information incrementally.

**Status**: Not implemented - current on-demand `attacks_bb()` is fast enough
- Sliding piece attacks: magic bitboards
- Pins/discovered: computed on demand
- Cost/benefit analysis shows marginal gain

**Decision**: Deferred - not implemented

### 20. IPFC — Incremental Position Feature Cache

**Purpose**: Cache reusable position information.

**Status**: Partially implemented via `AccumulatorCaches` (Finny Tables)
- Caches: King zones, pawn structures, piece counts
- Invalidated on moves via `append_changed_indices`

### 21. NASA — NUMA-Aware Search Allocation

**Purpose**: Topology-aware thread and memory allocation.

**Implementation** (`thread.cpp`, `numa.h`):
- `NumaConfig`: Detects NUMA nodes via `GetLogicalProcessorInformationEx` (Windows) / `numa_available()` (Linux)
- `SharedHistories` per NUMA node
- Thread-to-node affinity in `ThreadPool::resize()`
- Portable fallback: single node

**UCI Option**: `GAMBO.NUMAAlloc` (default: on)

### 22. PGO — Profile-Guided Optimisation

**Purpose**: Reproducible profile-guided builds.

**Implementation**:
- Build system: `make profile-build` → generates `.profdir` → `make pgo-build`
- Workload: `bench 16 1 8 default depth` (standard positions)
- Documented in `docs/GAMBO_VALIDATION.md`

**UCI Option**: `GAMBO.PGOProfile` (path to profile data, default: "")

---

## Performance Impact Summary

| Optimization | Expected Speedup | Memory Impact | Complexity |
|--------------|------------------|---------------|------------|
| INFU/NFAR | 5-15% | +Cache memory | Medium |
| SONI (AVX2) | 10-30% NNUE | None | Low |
| SMP/TT Prefetch | 2-5% | None | Low |
| CATT/Compact | 0-3% | -50% TT mem | Medium |
| NASA/NUMA | 5-15% (multi-socket) | +NUMA tracking | Low |

## Build Flags

```bash
# Standard release
make -j4 profile-build ARCH=x86-64-bmi2 COMP=mingw

# Debug with sanitizers
make debug=yes sanitize=address,undefined

# PGO training
make profile-build ARCH=x86-64-bmi2 COMP=mingw
./stockfish bench 16 1 8 default depth  # generates .profdir
make pgo-build ARCH=x86-64-bmi2 COMP=mingw

# Custom SIMD
make COMP=mingw EXTRACXXFLAGS="-DGAMBO_USE_SIMD"
```

## Verification

```bash
# Correctness
make debug=yes sanitize=address,undefined
./stockfish bench 16 1 8 default depth

# Performance
./stockfish bench 16 1 8 default depth  # Compare NPS

# Ablation
./stockfish
> setoption name GAMBO.SIMDNNUE value false
> bench 16 1 8 default depth
```