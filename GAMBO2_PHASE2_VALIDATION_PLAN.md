# GAMBO 2.0 — Phase 2 Empirical Validation Plan

**Revision:** Phase 2 audit commit `123dfbb` (2026-11-02)  
**Purpose:** Rigorous ablation testing to determine if Phase 2 improvements make GAMBO objectively stronger  
**Restriction:** Documentation and source-level preparation only — no execution on this machine

---

## 1. Testing Objectives

Determine the individual and combined contribution of each Phase 2 component:

| Component | Hypothesis | Success Criterion |
|-----------|------------|-------------------|
| `king_safety` | Improves king attack/defense evaluation beyond NNUE | Positive Elo vs baseline; no double-counting inflation |
| `initiative` | Adds meaningful attacking potential signal | Improves tactical/middlegame performance |
| `piece_coordination` | Novel coordination patterns not in NNUE | Positive contribution; PieceCoordFeatureSet already covers batteries/outposts |
| `defensive_resources` | Fortress/blockade/drawing detection fills NNUE gap | Improves endgame/defensive accuracy |
| `positional_sacrifice` | Objective compensation heuristic guides search | Improves sacrifice soundness without tactical blindness |
| `advantage_conversion` | Winning technique (simplification, king activity) | Improves conversion rate in won positions |

**Primary Question:** Does the *sum* of classical terms + NNUE outperform pure NNUE (Phase 1 baseline)?

**Secondary Question:** Which components are net positive, neutral, or negative?

---

## 2. Baseline Revisions

| Label | Git Revision | Description |
|-------|--------------|-------------|
| `GAMBO_BASE` | `d2bd4ee` | Phase 1 complete (before Phase 2 classical terms) |
| `GAMBO_P2_FULL` | `123dfbb` | Phase 2 audit commit — all 6 components enabled |
| `GAMBO_P2_NOKING` | TBD | Phase 2 minus `king_safety` |
| `GAMBO_P2_NOINIT` | TBD | Phase 2 minus `initiative` |
| `GAMBO_P2_NOCOORD` | TBD | Phase 2 minus `piece_coordination` |
| `GAMBO_P2_NODEF` | TBD | Phase 2 minus `defensive_resources` |
| `GAMBO_P2_NOSAC` | TBD | Phase 2 minus `positional_sacrifice` |
| `GAMBO_P2_NOCONV` | TBD | Phase 2 minus `advantage_conversion` |
| `GAMBO_P2_NONE` | TBD | Phase 2 with all 6 disabled (pure NNUE + phase blending) |

**NNUE Network:** Same network file for all revisions (e.g., `nnue.net` from GAMBO release). Network must be trained on 4-feature-set architecture (HalfKAv2_hm + FullThreats + KingSafety + PieceCoordination).

---

## 3. Ablation Configurations

### 3.1 Source-Level Ablation (Recommended)

Add UCI options to `src/evaluate.cpp` to enable/disable each component independently:

```cpp
// In Eval namespace or UCI options
Option("Classical.KingSafety",     true);
Option("Classical.Initiative",     true);
Option("Classical.Coordination",   true);
Option("Classical.Defense",        true);
Option("Classical.Sacrifice",      true);
Option("Classical.Conversion",     true);
```

Each classical function checks its option before computing:

```cpp
int king_safety(const Position& pos, Color us) {
    if (!UCI::option("Classical.KingSafety").value()) return 0;
    // ... existing implementation
}
```

**Benefits:** No recompilation between experiments; single binary tests all configurations.

**Implementation:** Add to `uci.cpp` option registration; read in `evaluate.cpp` via `UCI::option()`.

### 3.2 Compile-Time Ablation (Fallback)

If UCI options are impractical, use preprocessor macros:

```cpp
// In evaluate.h
#define CLASSICAL_KING_SAFETY 1
#define CLASSICAL_INITIATIVE  1
// etc.
```

Requires recompilation per configuration — less flexible.

### 3.3 Recommended Approach

**Implement UCI options** — cleanest for testing. Default all `true` to preserve current behavior.

---

## 4. Experimental Methodology

### 4.1 Hardware & Environment

| Parameter | Specification |
|-----------|---------------|
| CPU | Identical for all tests (e.g., AMD Ryzen 9 7950X / Intel i9-13900K) |
| RAM | ≥32 GB |
| OS | Linux (Ubuntu 22.04+) or Windows 11 |
| Threads | 1 (single-threaded for consistency) or fixed N (e.g., 4) |
| Hash | 256 MB |
| NNUE Network | Identical file for all revisions |
| Syzygy TB | 6-man (optional, for endgame tests) |

**Critical:** Pin CPU frequency, disable turbo boost, set performance governor.

### 4.2 Time Controls

| Test Type | Time Control | Games per Pairing |
|-----------|--------------|-------------------|
| **Primary (Elo)** | 10+0.1s (bullet) | 2000-4000 |
| **Secondary (Elo)** | 60+0.6s (rapid) | 500-1000 |
| **Tactical Suites** | 10s/move | Full suite |
| **Endgame TB** | 1s/move | 100 positions |

**Rationale:** Bullet for throughput; rapid for reduced error. Same hardware → same NPS baseline.

### 4.3 Opening Books

| Test | Book |
|------|------|
| Primary Elo | Balanced 8-move book (e.g., `UHO_XXL_2024.epd` or `Noob_8moves.epd`) |
| Tactical | STS, WAC, LCT2, Arasan tactical suite |
| Endgame | Syzygy 6-man probe positions; specific KPK, KRK, KQK, KBBK, KBNK |
| Style | Aggressive openings (King's Gambit, Sicilian Najdorf, etc.) |

**Reversed colours:** Every opening played twice (White/Black).

### 4.4 Opponents

| Opponent | Version | Purpose |
|----------|---------|---------|
| Stockfish | 17 or latest dev | Absolute strength reference |
| Leela Chess Zero | 0.30.0 + T80 net | Neural net comparison |
| Berserk | Latest | Aggressive engine reference |
| GAMBO_BASE | `d2bd4ee` | Phase 1 baseline |
| GAMBO_PREV | Previous release | Regression check |

**Round-robin** preferred over gauntlet for statistical efficiency.

---

## 5. Statistical Analysis Methodology

### 5.1 Elo Estimation

- **Tool:** `bayeselo` or `ordo` with paired results
- **Model:** Logistic with draw model
- **Confidence:** 95% CI (bootstrap 10000 samples)
- **Significance:** Non-overlapping 95% CI = significant difference

### 5.2 Required Game Counts

| Expected Elo Diff | Games for 95% CI ±10 Elo |
|-------------------|-------------------------|
| 0-5 | 4000+ |
| 5-15 | 2000 |
| 15-30 | 1000 |
| 30+ | 500 |

**Minimum:** 2000 games per pairing for primary bullet tests.

### 5.3 Metrics Beyond Elo

| Metric | Purpose |
|--------|---------|
| **Win/Draw/Loss %** | Style impact (more decisive?) |
| **Average game length** | Endgame conversion |
| **Tactical suite solve rate** | Tactical accuracy |
| **Endgame TB accuracy** | Tablebase match rate |
| **NPS (nodes/sec)** | Computational overhead |
| **Search instability (PV changes)** | Time management interaction |
| **Eval correlation (NNUE vs Classical)** | Double-counting proxy |

### 5.4 Ablation Statistical Test

For each component C:
1. Run `FULL` vs `NO_C` (paired, same openings, reversed colors)
2. Compute Elo difference with 95% CI
3. **Retain C if:** CI lower bound > +5 Elo
4. **Neutral if:** CI includes [-5, +5]
5. **Remove C if:** CI upper bound < -5 Elo

---

## 6. Required Hardware & Dependencies

### 6.1 Testing Machine (Separate from Development)

- **CPU:** Modern x86-64 with AVX2/AVX-512
- **OS:** Linux preferred (reproducible, no thermal throttling)
- **GPU:** Not required (CPU-only NNUE)
- **Network:** Isolated (no external connections during tests)

### 6.2 Software Dependencies

```
g++ >= 11 (or clang >= 14)
make
cutechess-cli >= 1.2.0
bayeselo / ordo
python3 (for analysis scripts)
syzygy-tb (optional, for endgame tests)
```

### 6.3 Build Configuration

```bash
# Profile build with PGO (optional but recommended)
make profile-build ARCH=native COMP=gcc
# Or standard release
make -j$(nproc) release ARCH=native
```

**Critical:** Same binary used for all tests — only UCI options change.

---

## 7. Expected Output Data

### 7.1 Per-Experiment Logs

| File | Content |
|------|---------|
| `results_<config>_<date>.pgn` | All games in PGN |
| `results_<config>_<date>.log` | Cutechess output |
| `elo_<config>_<date>.txt` | Bayeselo output |
| `stats_<config>_<date>.json` | Parsed metrics (W/D/L, length, NPS) |

### 7.2 Ablation Matrix

| Config | vs BASE | vs FULL | vs NONE | Tactical % | Endgame % | NPS |
|--------|---------|---------|---------|------------|-----------|-----|
| FULL | +X ±Y | — | +Z | ... | ... | ... |
| NOKING | ... | ... | ... | ... | ... | ... |
| NOINIT | ... | ... | ... | ... | ... | ... |
| ... | ... | ... | ... | ... | ... | ... |

### 7.3 Final Report

`GAMBO2_PHASE2_VALIDATION_REPORT.md` with:
- Executive summary (which components retained/removed)
- Full Elo tables with confidence intervals
- Tactical/endgame breakdown
- NPS overhead analysis
- Recommendation for Phase 3

---

## 8. Criteria for Component Decisions

| Outcome | Action |
|---------|--------|
| **Clear positive** (CI > +5 vs baseline, positive vs NONE) | **Retain** — promote to default |
| **Marginal positive** (CI [+1, +5]) | **Retain with tuning** — optimize weights |
| **Neutral** (CI [-5, +5]) | **Default ON** but mark for removal if maintenance burden |
| **Negative** (CI < -5) | **Disable by default** — keep code for future |
| **Causes instability** (high PV changes, time mgmt issues) | **Disable** regardless of Elo |

**Double-counting specific check:** If `FULL` shows eval inflation (mean |eval| significantly higher than `NONE` without Elo gain) → reduce weights or remove overlapping components.

---

## 9. Testing Protocol Checklist

### Pre-Testing
- [ ] Testing machine provisioned and benchmarked
- [ ] Identical NNUE network copied to test machine
- [ ] GAMBO_BASE, GAMBO_P2_FULL binaries built and verified
- [ ] UCI ablation options implemented and tested
- [ ] Opening books downloaded and verified
- [ ] Opponent engines installed and configured
- [ ] Cutechess config files prepared

### During Testing
- [ ] Run round-robin at bullet (10+0.1) — 2000 games/pairing
- [ ] Run tactical suites on all configs
- [ ] Run endgame TB tests on all configs
- [ ] Monitor NPS, temperature, stability
- [ ] Log all raw data

### Post-Testing
- [ ] Parse PGNs → Bayeselo
- [ ] Generate ablation matrix
- [ ] Statistical significance testing
- [ ] Write validation report
- [ ] Decision: retain/modify/remove each component
- [ ] Update PROGRESS_REPORT.md
- [ ] Tag validation commit

---

## 10. Source-Level Preparation (This Machine)

### 10.1 Add UCI Options to `uci.cpp`

```cpp
// In UCI::init() option registration
o.add("Classical.KingSafety",     true);
o.add("Classical.Initiative",     true);
o.add("Classical.Coordination",   true);
o.add("Classical.Defense",        true);
o.add("Classical.Sacrifice",      true);
o.add("Classical.Conversion",     true);
```

### 10.2 Modify `evaluate.cpp` to Check Options

```cpp
// In ClassicalEval namespace or as lambda captures
namespace {
    bool useKingSafety()     { return UCI::option("Classical.KingSafety").value(); }
    bool useInitiative()     { return UCI::option("Classical.Initiative").value(); }
    // ...
}

// In evaluate():
if (useKingSafety()) {
    int ourKingSafety = ClassicalEval::king_safety(pos, us);
    // ...
} else { kingSafetyScore = 0; }
// Repeat for each component
```

### 10.3 Default Behavior Preserved

All options default `true` — no behavior change unless explicitly disabled.

---

## 11. Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Network not trained on 4 feature sets | Use latest GAMBO release network; document limitation |
| Thermal throttling on test machine | Pin CPU freq, monitor temps, pause if >80°C |
| Opening book bias | Balanced book + reversed colors + large N |
| Statistical noise | Sufficient games; bootstrap CI; report uncertainty |
| Component interaction effects | Test all 6 single-ablations; full factorial if resources allow |

---

## 12. Timeline Estimate

| Phase | Duration |
|-------|----------|
| Source prep (UCI options) | 1-2 hours |
| Build & verify binaries | 30 min |
| Transfer to test machine | 15 min |
| Bullet round-robin (9 configs × 8 opponents × 2000 games) | ~72 hours |
| Tactical suites | ~4 hours |
| Endgame TB tests | ~2 hours |
| Analysis & report | 4-8 hours |
| **Total** | **~90 hours wall time** |

---

## 13. Decision Gate for Phase 3

**Phase 3 authorized ONLY if:**

1. At least 3 of 6 components show clear positive contribution
2. `FULL` configuration beats `BASE` by ≥10 Elo (95% CI)
3. No catastrophic double-counting (eval inflation without Elo gain)
4. NPS overhead < 10%
5. No regression in tactical suites or endgame accuracy

**If criteria not met:** Return to Phase 2 — modify weights, remove components, or redesign.

---

*End of Validation Plan — Ready for Implementation on Testing Machine*