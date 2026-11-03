# GAMBO Roadmap Status Matrix

## Overall Progress

| Phase | Items | Implemented | Partial | Missing | Experimental |
|-------|-------|-------------|---------|---------|--------------|
| Phase 1/2 (Existing) | 16 | 16 | 0 | 0 | 0 |
| Tier 1 | 12 | 12 | 0 | 0 | 0 |
| Tier 2 | 10 | 7 | 3 | 0 | 0 |
| Tier 3 | 10 | 2 | 3 | 5 | 0 |
| Tier 4 | 8 | 0 | 0 | 0 | 8 |
| **Total** | **56** | **37** | **6** | **5** | **8** |

---

## Detailed Status

### ✅ Phase 1 & 2 (Existing - Preserved)

| # | Feature | Status | Location |
|---|---------|--------|----------|
| 1 | Objective evaluation + style-bias separation | ✅ | `evaluate.cpp:64-187` |
| 2 | King-safety-aware dynamic aggression | ✅ | `search.h:114-196` |
| 3 | Sacrifice compensation detection | ✅ | `search.h:212-257`, `evaluate.cpp:633-672` |
| 4 | Compensation-aware pruning | ✅ | `search.h:164-196` |
| 5 | Aggressive move ordering | ✅ | `movepick.cpp`, `search.h` |
| 6 | Instability-aware time management | ✅ | `timeman.cpp`, `search.h` |
| 7 | Phase-adaptive NNUE blending | ✅ | `evaluate.cpp:78-89` |
| 8 | Classical King Safety | ✅ | `evaluate.cpp:271-336` |
| 9 | Classical Initiative | ✅ | `evaluate.cpp:339-395` |
| 10 | Classical Coordination | ✅ | `evaluate.cpp:398-527` |
| 11 | Classical Defensive Resources | ✅ | `evaluate.cpp:530-628` |
| 11 | Classical Positional Sacrifice | ✅ | `evaluate.cpp:633-672` |
| 12 | Classical Advantage Conversion | ✅ | `evaluate.cpp:675-757` |
| 13 | Evaluation tracing | ✅ | `evaluate.cpp:193-265` |
| 14 | Runtime evaluation ablation (7 UCI options) | ✅ | `engine.cpp:130-139` |
| 15 | Stockfish NNUE (HalfKAv2, etc.) | ✅ | `src/nnue/` |
| 16 | GAMBO-native NNUE features + SFNNv16 | ✅ | `src/gambo/nnue/` |

---

### ✅ Tier 1: Advanced Search Improvements (12/12)

| # | Feature | Status | UCI Option | Key Files |
|---|---------|--------|------------|-----------|
| 1 | TMIHH | ✅ Implemented | `GAMBO.ThreatMapHistory` (on) | `search.h:285-340`, `search.cpp`, `movepick.cpp` |
| 2 | CICH | ✅ Implemented | `GAMBO.ColourContinuationHistory` (on) | `history.h:158-161`, `search.cpp:1955-2020` |
| 3 | MPCH | ✅ Implemented | `GAMBO.MultiPlyContinuationHistory` (off) | `history.h:163-165`, `search.cpp` |
| 4 | MTCH | ✅ Implemented | `GAMBO.MoveTypeHistory` (on) | `history.h:168-175`, `movepick.cpp`, `search.cpp` |
| 5 | SEE-GCP | ✅ Implemented | (implicit) | `search.h:372-380`, `movepick.cpp` |
| 6 | CHQO | ✅ Implemented | `GAMBO.QSearchCaptureHistory` (on) | `history.h:177-178`, `movepick.cpp:344-370` |
| 7 | HGQMP | ✅ Implemented | (implicit) | `search.h:382-415`, `search.cpp:1149-1158` |
| 8 | URD | ✅ Implemented | (implicit) | `search.h:417-440` |
| 9 | EGC | ✅ Implemented | (implicit) | `search.h:67-90`, `search.cpp:1230-1255` |
| 10 | SESR | ✅ Implemented | (implicit) | `search.cpp:1230-1255` |
| 11 | LMRC | ✅ Implemented | (implicit) | `search.cpp:1876-1915`, `search.cpp:1092-1112` |
| 12 | QTC | ✅ Implemented | (implicit) | `search.cpp:1702-1720` |

---

### 🔄 Tier 2: Performance & Memory (7/10 Implemented, 3 Partial)

| # | Feature | Status | UCI Option | Notes |
|---|---------|--------|------------|-------|
| 13 | INFU | ✅ Implemented | `GAMBO.IncrementalNNUE` (on) | All 4 feature sets have `append_changed_indices` |
| 14 | NFAR | ✅ Implemented | `GAMBO.FastAccumulatorRefresh` (on) | `AccumulatorCaches`, SIMD refresh paths |
| 15 | SONI | ✅ Implemented | `GAMBO.SIMDNNUE` (on) | AVX2/SSE4.1 + scalar fallback |
| 16 | SMP | ✅ Implemented | `GAMBO.PrefetchTT` (off) | TT prefetch, hardware prefetcher for stack |
| 17 | CATT | ✅ Implemented | `GAMBO.CompactTT` (off) | `TTLayout` enum, cache-line clusters |
| 18 | CTTEL | 🔄 Partial | `GAMBO.CompactTT` (off) | Layout defined, compact encoding deferred |
| 19 | IAMM | ❌ Deferred | - | Cost/benefit analysis: not implemented |
| 20 | IPFC | 🔄 Partial | (implicit) | Via `AccumulatorCaches` (Finny Tables) |
| 21 | NASA | ✅ Implemented | `GAMBO.NUMAAlloc` (on) | `NumaConfig`, per-node shared histories |
| 22 | PGO | ✅ Implemented | `GAMBO.PGOProfile` ("") | `make profile-build` + `pgo-build` |

---

### 🔄 Tier 3: MetaSearch (2/10 Implemented, 3 Partial, 5 Planned)

| # | Feature | Status | Notes |
|---|---------|--------|-------|
| 23 | PCE | ✅ Implemented | `src/gambo/pce.h/cpp`, 8 signals |
| 24 | ASRA | 🔄 Partial | Uses PCE output; aspiration window adjustment |
| 25 | SID | ✅ Implemented | `src/gambo/sid.h/cpp`, 3 metrics |
| 26 | TVS | ❌ Planned | Triggered by SID/PCE |
| 27 | PNMV | ❌ Planned | Experimental |
| 28 | PTC | ❌ Planned | Cross-variation threat tracking |
| 29 | CCA | ❌ Planned | Root candidate robustness |
| 30 | EDSE | ❌ Planned | Event-driven extensions |
| 31 | RCRA | ❌ Planned | Root candidate robustness |
| 32 | HSWS | ❌ Planned | Heterogeneous workers |

---

### 🧪 Tier 4: Experimental (0/8 Implemented, All Default OFF)

| # | Feature | Status | Architecture |
|---|---------|--------|--------------|
| 33 | PG-MCTS | 📋 Designed | `GAMBO.SearchPolicy=PG-MCTS` |
| 34 | PUCT | 📋 Designed | `GAMBO.PUCT` |
| 35 | T-AGS | 📋 Designed | `GAMBO.TAGS` |
| 36 | DAG-Search | 📋 Designed | `GAMBO.DAGSearch` |
| 37 | PSPA | 📋 Designed | `GAMBO.SearchPolicy` |
| 38 | BNNI | 📋 Designed | `GAMBO.BatchedNNUE` |
| 39 | MHNE | 📋 Designed | `GAMBO.MHNE` |
| 40 | HA-MS | 📋 Designed | `GAMBO.HybridSearch` |

---

## Documentation Status

| Document | Status | Path |
|----------|--------|------|
| Final Optimization Audit | ✅ | `docs/GAMBO_FINAL_OPTIMIZATION_AUDIT.md` |
| Final Architecture | ✅ | `docs/GAMBO_FINAL_ARCHITECTURE.md` |
| Search Optimizations | ✅ | `docs/GAMBO_SEARCH_OPTIMIZATIONS.md` |
| Performance Optimizations | ✅ | `docs/GAMBO_PERFORMANCE_OPTIMIZATIONS.md` |
| Experimental Search | ✅ | `docs/GAMBO_EXPERIMENTAL_SEARCH.md` |
| UCI Options Reference | ✅ | `docs/GAMBO_UCI_OPTIONS.md` |
| Validation Framework | ✅ | `docs/GAMBO_VALIDATION.md` |
| Roadmap Status | ✅ | `docs/GAMBO_ROADMAP_STATUS.md` |

---

## Build & Test Status

| Check | Status |
|-------|--------|
| Release build (Windows/mingw) | ✅ CI passing |
| Debug build | ⏳ Untested |
| Sanitize build (ASAN/UBSAN) | ⏳ Untested |
| PGO build | ✅ CI passing |
| Multi-thread bench | ⏳ Untested |
| Perft tests | ⏳ Untested |
| NNUE accumulator consistency | ⏳ Untested |
| UCI option registration | ✅ Verified |

---

## Next Milestones

| Milestone | Target | Dependencies |
|-----------|--------|--------------|
| Complete Tier 2 (CTTEL, IAMM decision) | Week 2 | CTTEL encoding |
| Tier 3 ASRA/TVS/CCA/RCRA | Week 3-4 | PCE/SID integration |
| Tier 4 PSPA framework | Week 5 | Policy interface |
| Tier 4 PG-MCTS/PUCT prototype | Week 6-7 | MCTS core |
| External tournament testing | Week 8+ | VPS access |

---

## Risk Register

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| GAMBO NNUE no trained network | HIGH | HIGH | Fallback to Stockfish NNUE; document clearly |
| Experimental features break main search | HIGH | MEDIUM | All default OFF; isolated behind UCI |
| CTTEL encoding breaks TT | MEDIUM | MEDIUM | Default OFF; extensive testing |
| PGO profiles not portable | LOW | HIGH | Document machine-specific |
| Tier 3 features add search overhead | MEDIUM | MEDIUM | Profile-guided tuning |
| Thread safety in new heuristics | HIGH | LOW | TSAN testing |