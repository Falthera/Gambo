# GAMBO 2.0 — Phase 2 Empirical Validation Plan (Final)

**Revision:** Phase 2 audit commit `123dfbb` + validation prep commit `f093cc2` (2026-11-02)  
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
| `phase_adaptive_nnue` | Phase-dependent PSQT/Positional blending improves eval | Positive Elo vs fixed Phase 1 weights |

**Primary Question:** Does the *sum* of classical terms + phase-adaptive NNUE outperform pure NNUE (Phase 1 baseline)?

**Secondary Question:** Which components are net positive, neutral, or negative?

**Limitations:** Individual ablations cannot measure higher-order interactions. Combined configurations only where they answer a specific question.

---

## 2. Baseline Revisions

| Label | Git Revision | Description |
|-------|--------------|-------------|
| `GAMBO_BASE` | `d2bd4ee` | Phase 1 complete (before Phase 2 classical terms) |
| `GAMBO_P2_FULL` | `f093cc2` | Phase 2 validation prep — all 7 components enabled (6 classical + phase-adaptive NNUE) |
| `GAMBO_P2_NOKING` | Same binary | `Classical.KingSafety=false` |
| `GAMBO_P2_NOINIT` | Same binary | `Classical.Initiative=false` |
| `GAMBO_P2_NOCOORD` | Same binary | `Classical.Coordination=false` |
| `GAMBO_P2_NODEF` | Same binary | `Classical.Defense=false` |
| `GAMBO_P2_NOSAC` | Same binary | `Classical.Sacrifice=false` |
| `GAMBO_P2_NOCONV` | Same binary | `Classical.Conversion=false` |
| `GAMBO_P2_NOPHASE` | Same binary | `Eval.PhaseAdaptiveNNUE=false` (fixed Phase 1 weights: PSQT=126, Positional=130) |
| `GAMBO_P2_NONE` | Same binary | All 7 disabled (pure NNUE + fixed Phase 1 blending) |

**NNUE Network:** Same network file for all revisions (e.g., `nnue.net` from GAMBO release). Network must be trained on 4-feature-set architecture (HalfKAv2_hm + FullThreats + KingSafety + PieceCoordination).

---

## 3. Experiment Configurations

### 3.1 Core Ablation Matrix (10 configurations, single binary)

| Config | KingSafety | Initiative | Coordination | Defense | Sacrifice | Conversion | PhaseAdaptive |
|--------|------------|------------|--------------|---------|-----------|------------|---------------|
| `FULL` | ON | ON | ON | ON | ON | ON | ON |
| `NOKING` | OFF | ON | ON | ON | ON | ON | ON |
| `NOINIT` | ON | OFF | ON | ON | ON | ON | ON |
| `NOCOORD` | ON | ON | OFF | ON | ON | ON | ON |
| `NODEF` | ON | ON | ON | OFF | ON | ON | ON |
| `NOSAC` | ON | ON | ON | ON | OFF | ON | ON |
| `NOCONV` | ON | ON | ON | ON | ON | OFF | ON |
| `NOPHASE` | ON | ON | ON | ON | ON | ON | OFF |
| `NONE` | OFF | OFF | OFF | OFF | OFF | OFF | OFF |
| `BASE` | N/A | N/A | N/A | N/A | N/A | N/A | N/A (Phase 1 commit) |

### 3.2 Additional Combined Configurations (Only Where Justified)

| Config | Purpose |
|--------|---------|
| `NO_KING_INIT` (KingSafety+Initiative OFF) | Test overlap between two highest-NNUE-overlap components |
| `CLASSICAL_ONLY` (PhaseAdaptive OFF, all classical ON) | Isolate classical contribution without phase-adaptive blending |
| `NNUE_ONLY` (All classical OFF, PhaseAdaptive ON) | Isolate phase-adaptive NNUE contribution |

**No other combinations** — individual ablations + these 3 address all specific experimental questions.

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
| **Primary (Elo)** | 10+0.1s (bullet) | 3000 |
| **Secondary (Elo)** | 60+0.6s (rapid) | 800 |
| **Tactical Suites** | 10s/move | Full suite |
| **Endgame TB** | 1s/move | 100 positions |

**Rationale:** Bullet for throughput; rapid for reduced error. Same hardware → same NPS baseline.

### 4.3 Opening Books

| Test | Book |
|------|------|
| Primary Elo | Balanced 8-move book (e.g., `UHO_XXL_2024.epd` or `Noob_8moves.epd`) |
| Tactical | STS, WAC, LCT2, Arasan tactical suite |
| Endgame | Syzygy 6-man probe positions; specific KPK, KRK, KQK, KBBK, KBNK |

**Reversed colours:** Every opening played twice (White/Black). **Paired design:** Same opening/color sequence for each pairing.

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
- **Model:** Logistic with draw model (Davidson or Rao-Kupper)
- **Confidence:** 95% CI (bootstrap 10000 samples)
- **Significance:** Non-overlapping 95% CI = significant difference

### 5.2 Power Analysis & Game Count Justification

**Assumptions for bullet (10+0.1):**
- Draw rate: ~55% (typical for strong engines at bullet)
- Minimum practical effect: 5 Elo
- Target power: 80% at α = 0.05

**Required games per pairing (paired t-test approximation):**

| Effect Size (Elo) | Games for 80% power (α=0.05) | 95% CI Width |
|-------------------|------------------------------|--------------|
| 5 | ~3,500 | ±8 |
| 8 | ~1,400 | ±6 |
| 10 | ~900 | ±5 |
| 15 | ~400 | ±4 |

**Decision:** **3,000 games per pairing** for primary bullet tests.
- Provides ~75% power to detect 5 Elo difference
- 95% CI width ~±8 Elo at 55% draw rate
- Balances statistical rigour with wall time

**Multiple Comparison Correction:** Bonferroni-Holm for 10 primary comparisons (9 ablations + FULL vs BASE). Adjusted α = 0.05/10 ≈ 0.005 per test. Report both raw and adjusted p-values.

### 5.3 Sequential Probability Ratio Testing (SPRT)

**Optional acceleration:** Use SPRT for early stopping during round-robin.
- H0: Elo diff = 0; H1: Elo diff = ±10
- α = 0.05, β = 0.10
- Stop early if strong evidence for H0 or H1
- Report SPRT likelihood ratios alongside final CI

### 5.4 Metrics Beyond Elo

| Metric | Purpose |
|--------|---------|
| **Win/Draw/Loss %** | Style impact (more decisive?) |
| **Average game length** | Endgame conversion |
| **Tactical suite solve rate** | Tactical accuracy |
| **Endgame TB accuracy** | Tablebase match rate |
| **NPS (nodes/sec)** | Computational overhead |
| **Search instability (PV changes)** | Time management interaction |
| **Eval correlation (NNUE vs Classical)** | Double-counting proxy |
| **Mean |eval| inflation** | Direct double-counting measure |

### 5.5 Ablation Statistical Test

For each component C:
1. Run `FULL` vs `NO_C` (paired, same openings, reversed colors)
2. Compute Elo difference with 95% CI
3. **Retain C if:** CI lower bound > +5 Elo (after multiple-comparison adjustment)
4. **Neutral if:** CI includes [-5, +5]
5. **Remove C if:** CI upper bound < -5 Elo

---

## 6. Primary Comparisons (Pre-Registered)

| Comparison | Question | Statistical Test |
|------------|----------|------------------|
| `FULL` vs `BASE` | Does Phase 2 as a whole improve on Phase 1? | Paired Elo diff, 95% CI |
| `FULL` vs `NONE` | Do classical terms + phase-adaptive NNUE add net value? | Paired Elo diff, 95% CI |
| `FULL` vs `NO_C` (each C) | Is component C individually beneficial? | Paired Elo diff, 95% CI (10 tests) |
| `FULL` vs `NOPHASE` | Does phase-adaptive NNUE blending help? | Paired Elo diff, 95% CI |
| `CLASSICAL_ONLY` vs `NNUE_ONLY` | Classical vs phase-adaptive NNUE contribution | Paired Elo diff, 95% CI |

**All comparisons use identical hardware, threads, hash, network, time control, openings.**

---

## 7. Acceptance Criteria for Phase 3

**Phase 3 authorized ONLY if ALL of:**

1. **Overall improvement:** `FULL` beats `BASE` by ≥10 Elo (95% CI lower bound > +5 Elo after correction)
2. **Net positive signal:** `FULL` beats `NONE` (CI lower bound > 0)
3. **Component quality:** ≥3 of 7 components show clear positive contribution (CI lower bound > +5 Elo)
4. **No catastrophic double-counting:** Mean |eval| of `FULL` ≤ 1.2× `NONE` (no inflation without Elo gain)
5. **Efficiency:** NPS overhead < 10% vs `NONE`
6. **No regression:** Tactical suite solve rate and endgame TB accuracy not significantly worse

**If criteria not met:** Return to Phase 2 — modify weights, remove components, or redesign.

---

## 8. Required Hardware & Dependencies

### 8.1 Testing Machine (Separate from Development)

- **CPU:** Modern x86-64 with AVX2/AVX-512
- **OS:** Linux preferred (reproducible, no thermal throttling)
- **GPU:** Not required (CPU-only NNUE)
- **Network:** Isolated (no external connections during tests)

### 8.2 Software Dependencies

```
g++ >= 11 (or clang >= 14)
make
cutechess-cli >= 1.2.0
bayeselo / ordo
python3 (for analysis scripts)
syzygy-tb (optional, for endgame tests)
```

### 8.3 Build Configuration

```bash
# Profile build with PGO (optional but recommended)
make profile-build ARCH=native COMP=gcc
# Or standard release
make -j$(nproc) release ARCH=native
```

**Critical:** Same binary used for all tests — only UCI options change.

---

## 9. Expected Output Data

### 9.1 Per-Experiment Logs

| File | Content |
|------|---------|
| `results_<config>_<date>.pgn` | All games in PGN |
| `results_<config>_<date>.log` | Cutechess output |
| `elo_<config>_<date>.txt` | Bayeselo output |
| `stats_<config>_<date>.json` | Parsed metrics (W/D/L, length, NPS) |

### 9.2 Ablation Matrix

| Config | vs BASE | vs FULL | vs NONE | Tactical % | Endgame % | NPS |
|--------|---------|---------|---------|------------|-----------|-----|
| FULL | +X ±Y | — | +Z | ... | ... | ... |
| NOKING | ... | ... | ... | ... | ... | ... |
| NOINIT | ... | ... | ... | ... | ... | ... |
| NOCOORD | ... | ... | ... | ... | ... | ... |
| NODEF | ... | ... | ... | ... | ... | ... |
| NOSAC | ... | ... | ... | ... | ... | ... |
| NOCONV | ... | ... | ... | ... | ... | ... |
| NOPHASE | ... | ... | ... | ... | ... | ... |
| NONE | ... | ... | ... | ... | ... | ... |

### 9.3 Final Report

`GAMBO2_PHASE2_VALIDATION_REPORT.md` with:
- Executive summary (which components retained/removed)
- Full Elo tables with confidence intervals (raw and adjusted)
- Tactical/endgame breakdown
- NPS overhead analysis
- Eval inflation analysis
- Recommendation for Phase 3

---

## 10. Source-Level Preparation Status (Complete)

### 10.1 UCI Options Implemented (`f093cc2`)

```cpp
// In engine.cpp - all default true
options.add("Classical.KingSafety", Option(true));
options.add("Classical.Initiative", Option(true));
options.add("Classical.Coordination", Option(true));
options.add("Classical.Defense", Option(true));
options.add("Classical.Sacrifice", Option(true));
options.add("Classical.Conversion", Option(true));
options.add("Eval.PhaseAdaptiveNNUE", Option(true));
```

### 10.2 Evaluate.cpp Modifications

- `use_phase_adaptive_nnue()` accessor added
- Phase-adaptive blending conditional on option
- Disabled state restores Phase 1 fixed weights (PSQT=126, Positional=130)
- All classical components conditional on their options
- `Eval::trace()` reports all option states

### 10.3 Default Behavior Preserved

All 7 options default `true` — **no behavior change** unless explicitly disabled.

---

## 11. What This Experiment CANNOT Establish

| Limitation | Impact |
|------------|--------|
| Higher-order interactions | Only 3 combined configs tested; full factorial (2⁷=128) infeasible |
| Optimal weight tuning | Ablation is binary; continuous weight optimization not tested |
| Long time controls | Bullet/rapid only; classical TCEC-style TCs untested |
| Multi-thread scaling | Single-thread only; NUMA/thread scaling untested |
| Different NNUE networks | Single network; results may vary with network architecture |
| Opening book dependence | Results specific to chosen book; generalization unknown |

---

## 12. Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Network not trained on 4 feature sets | Use latest GAMBO release network; document limitation |
| Thermal throttling | Pin CPU freq, monitor temps, pause if >80°C |
| Opening book bias | Balanced book + reversed colors + large N |
| Statistical noise | Sufficient games; bootstrap CI; report uncertainty |
| Component interaction effects | Test 3 key combined configs; document limitation |
| Multiple comparison inflation | Bonferroni-Holm correction; report both raw and adjusted |

---

## 13. Timeline Estimate

| Phase | Duration |
|-------|----------|
| Source prep (UCI options) | Complete (`f093cc2`) |
| Build & verify binaries | 30 min |
| Transfer to test machine | 15 min |
| Bullet round-robin (10 configs × 5 opponents × 3000 games) | ~100 hours |
| Tactical suites | ~4 hours |
| Endgame TB tests | ~2 hours |
| Analysis & report | 8 hours |
| **Total** | **~115 hours wall time** |

---

## 14. Final Validation-Readiness Statement

### Ready for Testing Machine Transfer

✅ **Source frozen** at commit `f093cc2` (Phase 2 audit + validation prep)  
✅ **7 ablation options** implemented (6 classical + phase-adaptive NNUE)  
✅ **Default behavior preserved** — all options ON  
✅ **10 experiment configurations** defined (no recompilation needed)  
✅ **Primary comparisons pre-registered** with statistical criteria  
✅ **Power analysis justifies** 3,000 games/pairing  
✅ **Multiple comparison correction** specified  
✅ **Acceptance criteria for Phase 3** explicitly defined  

### What This Will Establish

1. Whether Phase 2 as a whole improves on Phase 1
2. Which of the 7 individual components contribute positively
3. Whether phase-adaptive NNUE blending helps vs fixed weights
4. Whether classical terms double-count NNUE (via eval inflation metric)
5. Computational overhead of each component

### What This Will NOT Establish

- Optimal continuous weights for any component
- Performance at classical time controls
- Multi-thread scaling
- Generalization across NNUE networks

### Evidence Required Before Phase 3

All 6 acceptance criteria in Section 7 must be met. No exceptions.

---

*End of Final Validation Plan — Ready for Testing Machine*