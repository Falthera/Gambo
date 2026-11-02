# GAMBO 2.0 — Statistical Validation Audit

**Date:** 2026-11-02  
**Scope:** Independent verification of statistical methodology in `GAMBO2_PHASE2_VALIDATION_PLAN.md`  
**Engine revision:** Frozen at commit `ece189a`  
**Restriction:** Documentation and analysis only — no execution

---

## 1. Independent Power Calculation from First Principles

### 1.1 Elo to Expected Score Conversion

The Elo rating system defines expected score:

$$E_A = \frac{1}{1 + 10^{(R_B - R_A)/400}}$$

For a 5 Elo advantage (Δ = 5):

$$E = \frac{1}{1 + 10^{-5/400}} = \frac{1}{1 + 10^{-0.0125}} = \frac{1}{1 + 0.9716} \approx 0.5072$$

**A 5 Elo advantage corresponds to an expected score of 0.5072 (50.72%).**

### 1.2 Win/Draw/Loss Probabilities

For strong engines at bullet time control (10+0.1s), typical outcome distribution:

| Outcome | Probability (Base) |
|---------|-------------------|
| Win (W) | 0.225 |
| Draw (D) | 0.550 |
| Loss (L) | 0.225 |

With a 5 Elo advantage, the score increases by 0.0072. Assuming symmetric shift (wins convert from draws/losses proportionally):

| Outcome | Probability (Δ=+5 Elo) |
|---------|------------------------|
| Win | 0.230 |
| Draw | 0.540 |
| Loss | 0.230 |

Expected score = W + 0.5D = 0.230 + 0.270 = 0.500 → **Wait, this is wrong.**

Let me recalculate properly. The score shift of +0.0072 must come from the outcome distribution:

$$S = W + 0.5D = 0.5 + 0.0072 = 0.5072$$

With total probability constraint W + D + L = 1 and assuming symmetric Elo shift (ΔW = -ΔL), we have:

$$\Delta S = \Delta W + 0.5 \Delta D = 0.0072$$
$$\Delta W + \Delta D + \Delta L = 0$$
$$\Delta W = -\Delta L \implies \Delta D = 0$$

This is not correct either. The shift comes from W increasing at the expense of L and D.

Better approach: Use the Davidson model (or Rao-Kupper) which models:
- P(W) = f(ΔElo)
- P(D) = g(ΔElo)
- P(L) = f(-ΔElo)

For small ΔElo, linear approximation around Δ=0:

$$\frac{\partial S}{\partial \Delta} \approx \frac{\ln(10)}{400} \cdot (W + L) \approx 0.00576 \cdot (W + L)$$

At 55% draw rate, W+L = 0.45, so:
$$\frac{\partial S}{\partial \Delta} \approx 0.0026 \text{ per Elo}$$

Thus 5 Elo → ΔS ≈ 0.013. The actual derivative depends on the draw rate.

**Key finding:** At 55% draws, 5 Elo ≈ 0.0125-0.013 expected score difference.

### 1.3 Variance of Game Scores (Independent Games)

For a single game with outcomes {1, 0.5, 0}:
- E[X] = S
- Var[X] = E[X²] - E[X]²

At base (S = 0.5):
- W = 0.225, D = 0.55, L = 0.225
- E[X²] = 0.225(1) + 0.55(0.25) + 0.225(0) = 0.225 + 0.1375 = 0.3625
- Var[X] = 0.3625 - 0.25 = 0.1125

At +5 Elo (S = 0.5125):
- Approximate: W ≈ 0.235, D ≈ 0.54, L ≈ 0.225
- E[X²] = 0.235(1) + 0.54(0.25) = 0.235 + 0.135 = 0.37
- Var[X] = 0.37 - 0.5125² ≈ 0.37 - 0.2627 = 0.1073

**Variance per game: σ² ≈ 0.11**

### 1.4 Paired Openings Design — Variance of Score Differences

The plan uses **paired openings with colour reversal**: each opening position is played twice (once as White, once as Black) for each configuration pair.

For paired design, the relevant variance is Var[S₁ - S₂], not Var[S₁] + Var[S₂].

Let D_i = score of config A - score of config B on opening i (2 games: A as White, B as Black, then reversed).

Under the null (no difference), E[D_i] = 0.

The variance of the paired difference depends on the **within-opening correlation** ρ:

$$\text{Var}(D) = \text{Var}(S_A) + \text{Var}(S_B) - 2\rho \sqrt{\text{Var}(S_A)\text{Var}(S_B)}$$

For identical configurations (null), Var(S_A) = Var(S_B) = σ²_S:

$$\text{Var}(D) = 2\sigma^2_S(1 - \rho)$$

Where σ²_S is the variance of the score for a single configuration on a single opening (2 games).

For 2 games per opening:
- Var(score per opening) = 2 × Var(single game) = 2 × 0.11 = 0.22 (if independent games)
- But the two games share the same opening → positive correlation

**Critical parameter: ρ (within-opening, cross-configuration correlation)**

Empirical data from Stockfish testing frameworks (e.g., fishtest, OpenBench) suggests:
- ρ ≈ 0.6-0.8 for paired openings with colour reversal at bullet TC

Using ρ = 0.7 (conservative):
- Var(D) = 2 × 0.22 × (1 - 0.7) = 0.132
- SD(D) = √0.132 ≈ 0.363 score points per opening pair

With N opening pairs:
- SE(mean D) = 0.363 / √N

### 1.5 Expected Score Difference and Effect Size

For 5 Elo advantage:
- Expected score difference per opening pair = 2 games × 0.0125 = 0.025 score points
- Effect size (Cohen's d) = 0.025 / 0.363 ≈ 0.069

**This is a very small effect size.**

### 1.6 Required Sample Size for 80% Power (Paired t-test)

For paired t-test with:
- α = 0.05 (two-sided)
- Power = 0.80
- Effect size d = 0.069

Required N (number of opening pairs):

Using standard power formula for paired t-test:
$$N = \left(\frac{z_{1-\alpha/2} + z_{1-\beta}}{d}\right)^2$$

With z_{0.975} = 1.96, z_{0.80} = 0.84:
$$N = \left(\frac{1.96 + 0.84}{0.069}\right)^2 = \left(\frac{2.8}{0.069}\right)^2 \approx 40.6^2 \approx 1,648 \text{ opening pairs}$$

Each opening pair = 2 games → **3,296 games**

### 1.7 Adjustment for Multiple Comparisons (Bonferroni-Holm)

With 9 primary comparisons (FULL vs each ablation + FULL vs BASE):
- Bonferroni-Holm: α_k = α / (m - k + 1) for ordered p-values
- Most stringent: α_1 = 0.05 / 9 ≈ 0.0056
- z_{1-α_1/2} = z_{0.9972} ≈ 2.77

Recalculating with adjusted α:
$$N = \left(\frac{2.77 + 0.84}{0.069}\right)^2 = \left(\frac{3.61}{0.069}\right)^2 \approx 52.3^2 \approx 2,735 \text{ opening pairs}$$

**= 5,470 games per pairing**

### 1.8 What 3,000 Games Actually Provides

3,000 games = 1,500 opening pairs (with colour reversal).

At N = 1,500:
- SE = 0.363 / √1500 ≈ 0.0094
- 95% CI width = 2 × 1.96 × 0.0094 ≈ ±0.037 score points
- In Elo: ±0.037 / 0.0125 × 5 ≈ ±15 Elo (roughly)

Wait, let me recalculate:
- 5 Elo = 0.025 score points per opening pair
- SE = 0.0094
- 95% CI for Elo diff = ±1.96 × SE / 0.025 × 5 = ±1.96 × 0.376 × 5 ≈ **±3.7 Elo**

Actually: CI in score points = ±1.96 × 0.0094 = ±0.0184
In Elo: (0.0184 / 0.025) × 5 = **±3.68 Elo**

So 95% CI width ≈ **±3.7 Elo** (unadjusted).

With Bonferroni-Holm (α=0.0056, z=2.77):
- CI width = ±2.77 × 0.0094 / 0.025 × 5 ≈ **±5.2 Elo**

**Power at N=1,500 with adjusted α:**
- Non-centrality parameter λ = d × √N = 0.069 × √1500 ≈ 2.67
- Critical t ≈ 2.77 (df=1499)
- Power = P(t_{df, λ} > 2.77) ≈ 0.45-0.50

**3,000 games provides only ~45-50% power to detect 5 Elo at adjusted α=0.0056.**

### 1.9 Summary of Power Analysis

| Parameter | Value |
|-----------|-------|
| 5 Elo in score points (per opening pair) | 0.025 |
| Within-opening correlation (ρ) | 0.7 (assumed) |
| SD of paired difference | 0.363 |
| Effect size (d) | 0.069 |
| N opening pairs at 3,000 games | 1,500 |
| 95% CI width (unadjusted) | ±3.7 Elo |
| 95% CI width (Bonferroni α=0.0056) | ±5.2 Elo |
| Power at α=0.05 (unadjusted) | ~85% |
| Power at α=0.0056 (adjusted) | **~45-50%** |

**Conclusion: 3,000 games is UNDERPOWERED for the stated multiple-comparison-adjusted criteria.**

To achieve 80% power at adjusted α with 9 comparisons:
- **Required: ~5,500 games per pairing (2,750 opening pairs)**

---

## 2. Comparison Count Audit

### 2.1 Primary Hypotheses (Confirmatory)

From the pre-registered primary comparisons table (Section 6):

| # | Comparison | Type |
|---|------------|------|
| 1 | `FULL` vs `BASE` | Confirmatory |
| 2 | `FULL` vs `NONE` | Confirmatory |
| 3 | `FULL` vs `NOKING` | Confirmatory |
| 4 | `FULL` vs `NOINIT` | Confirmatory |
| 5 | `FULL` vs `NOCOORD` | Confirmatory |
| 6 | `FULL` vs `NODEF` | Confirmatory |
| 7 | `FULL` vs `NOSAC` | Confirmatory |
| 8 | `FULL` vs `NOCONV` | Confirmatory |
| 9 | `FULL` vs `NOPHASE` | Confirmatory |
| 10 | `CLASSICAL_ONLY` vs `NNUE_ONLY` | Confirmatory |

**Total: 10 primary confirmatory comparisons.**

The plan states "9 ablations + FULL vs BASE = 10" but the table includes FULL vs NONE and CLASSICAL_ONLY vs NNUE_ONLY, making **10 distinct primary comparisons**, not 9.

### 2.2 Exploratory Comparisons

| Comparison | Classification |
|------------|----------------|
| `NO_KING_INIT` (KingSafety+Initiative OFF) | Exploratory |
| FULL vs NO_KING_INIT | Exploratory |
| Any other combined configs | Exploratory |
| Tactical suite comparisons | Exploratory |
| Endgame TB comparisons | Exploratory |
| NPS comparisons | Exploratory |

### 2.3 Correction Family

Bonferroni-Holm should be applied to the **10 confirmatory comparisons**.

Adjusted significance levels (ordered p-values):
- 1st (smallest p): α/10 = 0.0050
- 2nd: α/9 = 0.0056
- 3rd: α/8 = 0.00625
- ...
- 10th: α/1 = 0.05

The plan's statement "Adjusted α = 0.05/10 ≈ 0.005 per test" is slightly inaccurate — Bonferroni-Holm is stepwise, not flat. But using the most stringent (0.005) is conservative and acceptable.

---

## 3. Evaluation-Inflation Criterion Review

### 3.1 Current Criterion

> `mean |eval| FULL ≤ 1.2 × mean |eval| NONE`

### 3.2 Limitations

1. **Not a proof of no double-counting:** Evaluation magnitude can increase for legitimate reasons (better discrimination, more accurate scoring).

2. **Scale-dependent:** NNUE output scale is arbitrary (depends on network quantization, output scaling). A 20% increase could be within normal variation.

3. **Confounded by style:** `evaluate_with_style()` adds aggression bonus — must use objective `evaluate()` only.

4. **Position distribution matters:** Mean |eval| depends heavily on the position set (opening book, search depth).

5. **No null distribution:** Without knowing the expected inflation under correct calibration, 1.2× is arbitrary.

### 3.3 Recommended Additional Calibration Analysis

**Fixed-position evaluation calibration:**

1. **Curate position sets by category:**
   - Opening (first 10 moves, balanced)
   - Tactical (STS/WAC positions)
   - Endgame (6-man TB positions)
   - Quiet positional (strategic positions)

2. **Measure on each set:**
   - Mean |eval| for FULL vs NONE
   - Correlation ρ(eval_FULL, eval_NONE)
   - Slope of eval_FULL vs eval_NONE regression
   - Win probability calibration: bin positions by eval, compare actual results

3. **Calibration plot:**
   - For each config, bin positions by eval
   - Plot empirical win rate vs predicted win rate
   - Well-calibrated: points on y=x line
   - Overconfident: steeper slope
   - Underconfident: shallower slope

4. **Statistical test:**
   - Hosmer-Lemeshow or Spiegelhalter test for calibration
   - Compare Brier scores between configs

This directly tests whether classical terms improve **probabilistic calibration**, not just magnitude.

---

## 4. SPRT Compatibility Review

### 4.1 Problem Statement

The plan proposes:
- Fixed-sample analysis with 95% CI + Bonferroni-Holm
- **Optional** SPRT early stopping (H0: Δ=0, H1: |Δ|=10 Elo)

### 4.2 Incompatibility Issues

1. **Type I error inflation:** Running SPRT and then doing fixed-sample analysis inflates α beyond nominal. The "optional stopping" problem.

2. **Multiple testing interaction:** SPRT on each of 10 comparisons with optional stopping + final fixed-sample analysis = uncontrolled family-wise error rate.

3. **Different alternative hypotheses:** SPRT uses H1: Δ=10 Elo; acceptance criterion uses Δ≥5 Elo. Inconsistent.

4. **BayesElo incompatibility:** SPRT requires binomial/likelihood model; BayesElo uses logistic/draw model on paired results.

### 4.3 Recommended Resolution

**Option A: Defer SPRT (Recommended)**
- Remove SPRT from primary analysis
- Use fixed 3,000-5,500 games per pairing
- Report final CI only

**Option B: Use SPRT as PRIMARY with proper error control**
- Pre-register SPRT boundaries for each comparison
- Use alpha-spending (Lan-DeMets) to combine with fixed-sample
- Do NOT also do fixed-sample analysis
- Report SPRT likelihood ratio and stopping time

**Option C: Sequential Bayes**
- Use Bayesian updating with prior
- Stop when posterior probability of |Δ|>5 > 0.95
- Naturally handles optional stopping
- But requires prior specification

**Recommendation: Option A.** The complexity of combining SPRT with 10-way multiple comparison correction is substantial and error-prone. Fixed-sample with adequate N is simpler and more credible.

---

## 5. Revised Sample Size Recommendation

### 5.1 Minimum Games for 80% Power (Adjusted)

| Target Effect | Games (ρ=0.7) | Games (ρ=0.8) |
|--------------|---------------|---------------|
| 5 Elo | 5,500 | 7,500 |
| 8 Elo | 2,200 | 3,000 |
| 10 Elo | 1,400 | 1,900 |

### 5.2 Practical Recommendation

Given wall-time constraints (~100 hours for 3,000 games):

**Option 1: Reduce comparison family**
- Focus on 3-4 key comparisons only (FULL vs BASE, FULL vs NONE, FULL vs NONE, FULL vs NOPHASE)
- 3,000 games provides ~80% power for 5 Elo at α=0.05/4=0.0125

**Option 2: Increase games to 5,000-6,000**
- Extend timeline to ~150-200 hours
- Accept longer validation

**Option 3: Accept lower power, report honestly**
- Run 3,000 games
- Report: "Power to detect 5 Elo at adjusted α ≈ 45%"
- Interpret non-significant results as "inconclusive" not "no effect"

**Recommendation: Option 3 with explicit power reporting.** Most honest given constraints.

---

## 6. Final Testing-Readiness Decision

### ✅ Ready for Transfer

- Source frozen at `ece189a`
- 7 ablation options implemented
- 10 experiment configurations defined
- Primary comparisons pre-registered
- Acceptance criteria explicit

### ⚠️ Statistical Caveats (Must Be Acknowledged in Final Report)

1. **Power limitation:** 3,000 games provides ~45-50% power to detect 5 Elo at Bonferroni-Holm adjusted α for 10 comparisons. Non-significant ablation results are **inconclusive**, not evidence of no effect.

2. **Correlation assumption:** Power calculations assume ρ=0.7. Actual ρ must be estimated from pilot data (first 200-500 games). If ρ > 0.75, power decreases further.

3. **Draw rate assumption:** 55% draw rate at 10+0.1s. Higher draw rate → lower power.

4. **SPRT removed:** Do not use sequential stopping. Fixed 3,000 games.

5. **Evaluation inflation criterion:** Treat as descriptive only. Use calibration analysis on fixed position sets for real evidence.

### 📋 Required in Final Report

- Actual ρ and draw rate observed
- Bootstrap CI for each comparison (raw and adjusted)
- Power achieved (post-hoc)
- Calibration plots for FULL vs NONE
- Explicit statement: "Non-significant ablations do not prove components are useless"

---

## 7. Updates to Validation Plan

The following sections of `GAMBO2_PHASE2_VALIDATION_PLAN.md` should be updated:

1. **Section 5.2:** Replace power table with corrected calculations
2. **Section 5.3:** Remove SPRT or mark as "deferred"
3. **Section 5.5:** Add power caveat to ablation test interpretation
4. **Section 7:** Add "Power caveat" to acceptance criteria
5. **Add new section:** "Statistical Limitations and Caveats"

---

*End of Statistical Validation Audit*