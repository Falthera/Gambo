# GAMBO 2.0 — Phase 2 Integration Audit

**Date:** 2026-11-02  
**Scope:** Static analysis of Phase 2 evaluation components in `src/evaluate.cpp`  
**Commit Context:** Phase 1 (d2bd4ee) + Phase 2 additions  
**Restriction:** No execution/testing — static analysis only

---

## 1. Executive Summary

Phase 2 introduced six classical evaluation components to complement NNUE:
1. **King Safety** (`king_safety`) — king exposure and pawn shelter
2. **Initiative** (`initiative`) — attacking potential and space control
3. **Piece Coordination** (`piece_coordination`) — batteries, outposts, rook files
4. **Defensive Resources** (`defensive_resources`) — fortresses, blockades, drawing tendencies
5. **Positional Sacrifice** (`positional_sacrifice`) — compensation for material deficits
6. **Advantage Conversion** (`advantage_conversion`) — winning technique, simplification

Plus phase-adaptive NNUE blending and game phase assessment.

**Overall Assessment:** The implementation has **solid conceptual foundations** but contains **multiple confirmed source-level defects** (syntax errors, logic errors, architectural violations) and **significant NNUE double-counting risks**. The objective/style separation from Phase 1 is partially violated.

---

## 2. Confirmed Source-Level Defects (FIXED)

### 2.1 Duplicate Variable Declaration — `king_safety()` (CRITICAL)
**Location:** `src/evaluate.cpp:244, 273` (original)
**Issue:** `Square kingSq` declared twice in same scope — compilation failure.
**Fix Applied:** Removed second declaration; reused first `kingSq`.

### 2.2 Invalid Function Name — `piece_coordination()` (CRITICAL)
**Location:** `src/evaluate.cpp:441` (original)
**Issue:** `squares_between_bb(r1, r2)` — function does not exist. Correct name: `between_bb()`.
**Fix Applied:** Changed to `between_bb(r1, r2)`.

### 2.3 Invalid Method Call — `defensive_resources()` & `advantage_conversion()` (CRITICAL)
**Location:** `src/evaluate.cpp:604` (advantage_conversion), similar in defensive_resources
**Issue:** `pos.attacks_bb<KING>(ourKing)` — `Position` class has no `attacks_bb` method. Only free function `attacks_bb<PieceType>(Square)` exists.
**Fix Applied:** Changed to `attacks_bb<KING>(ourKing)`.

### 2.4 Logic Error — `initiative()` (HIGH)
**Location:** `src/evaluate.cpp:339` (original)
**Issue:** 
```cpp
const Bitboard blockedByPawns = ourMinors & (ourPressure & pos.pieces(us, PAWN));
```
A square cannot contain both a minor piece AND a pawn. `blockedByPawns` always 0. The "active minors" calculation accidentally worked but for wrong reasons.
**Fix Applied:** Replaced with mobility-based check — minors with decent mobility (knight≥4, bishop≥6) counted as active.

### 2.5 Architectural Violation — `positional_sacrifice()` (HIGH)
**Location:** `src/evaluate.cpp:572-578` (original)
**Issue:** Function returns 0 unless `Search::ultra_aggressive()` is true. This leaks **style mode into objective evaluation**, violating Phase 1's explicit separation (line 74: "OBJECTIVE evaluation, without any style bias").
**Fix Applied:** Inlined compensation calculation without style check. Style-specific aggression now only applied in `evaluate_with_style()`.

### 2.6 Inverted Pawn Shield Logic — `king_safety()` (HIGH)
**Location:** `src/evaluate.cpp:257-258` (original)
**Issue:** `attacks_bb<PAWN>(kingSq, them)` returns squares FROM WHICH enemy pawns attack the king. For White king on g1, this gives {f2, h2} — missing g2! Pawn shield should check squares IN FRONT of king (f2, g2, h2).
**Fix Applied:** Implemented proper pawn shield squares based on king rank ±1 and adjacent files.

### 2.7 Incorrect Phase Constant — `game_phase()` (MEDIUM)
**Location:** `src/evaluate.cpp:704` (original)
**Issue:** Max non-pawn material = 16000 claimed, but actual starting non-pawn ≈ 6300, max with promotions ≈ 12500. Using 16000 gives opening phase ≈ 100 (middlegame) instead of ≈ 200-256.
**Fix Applied:** Changed divisor to 8000 → opening phase ≈ 200, middlegame ≈ 100, endgame ≈ 0.

---

## 3. NNUE Double-Counting Analysis

### 3.1 NNUE Feature Sets (from `nnue_architecture.h`)
| Feature Set | Purpose | Dimensions |
|------------|---------|------------|
| `KingSafetyFeatureSet` | King-relative piece positions (both kings) | 2 × 64 × 11 = 1408 |
| `PieceCoordFeatureSet` | Pairwise coordination (batteries, outposts, etc.) | 8192 |
| `ThreatFeatureSet` (FullThreats) | Attack/defense relationships | 60720 |
| `PSQFeatureSet` (HalfKAv2_hm) | King-relative piece-square tables | 1024 buckets |

### 3.2 Overlap Matrix

| Classical Component | NNUE Overlap | Risk Level |
|--------------------|--------------|------------|
| `king_safety` | KingSafetyFeatureSet + ThreatFeatureSet | **HIGH** — both evaluate king zone pressure, pawn shield, direct attackers |
| `initiative` | ThreatFeatureSet + FullThreats | **HIGH** — both measure attacking pressure, king zone attacks, checking moves |
| `piece_coordination` | PieceCoordFeatureSet | **HIGH** — batteries, outposts, rook files, bishop pairs explicitly in NNUE |
| `defensive_resources` | KingSafetyFeatureSet + ThreatFeatureSet | **MEDIUM** — blockades, fortresses partially covered; opposite-colored bishops less so |
| `positional_sacrifice` | ThreatFeatureSet + KingSafetyFeatureSet | **HIGH** — king zone pressure, direct attackers, activity advantage all in NNUE |
| `advantage_conversion` | PSQFeatureSet (HalfKAv2_hm) | **MEDIUM** — endgame king activity, passed pawns in PSQT; simplification logic novel |

### 3.3 Double-Counting Mechanism
The evaluation computes:
```
nnue = blend(psqt, positional)           // NNUE already encodes king safety, coordination, threats
classical = king_safety + initiative + ... // Classical re-evaluates SAME concepts
v = nnue * weight + classical             // ADDITIVE combination → double counting
```

**Evidence of inflation:** All six classical terms use similar attack/pressure calculations that NNUE features explicitly encode. The phase-weighted scaling (lines 107, 112, 117, 122) does not mitigate — it only changes WHEN double-counted terms are amplified.

### 3.4 Recommendation
**Reduce or restructure classical terms:**
1. **King Safety:** Remove or heavily downweight (NNUE KingSafety + Threats cover this thoroughly)
2. **Initiative:** Remove or keep only novel aspects (checking moves, development lead)
3. **Piece Coordination:** Remove — PieceCoordFeatureSet explicitly encodes batteries, outposts, rook files
4. **Defensive Resources:** Keep fortress/blockade logic (less NNUE coverage), remove king safety overlap
5. **Positional Sacrifice:** Keep as novel compensation heuristic (not directly in NNUE output)
6. **Advantage Conversion:** Keep simplification/technique logic (endgame technique not in NNUE)

**Alternative:** Make classical terms **correction deltas** (e.g., detect specific patterns NNUE misses) rather than independent evaluations of the same concepts.

---

## 4. Evaluation Correctness Findings

### 4.1 Perspective Consistency ✓
- All components take `Color us` and compute `(our - their)` differences
- Final `classicalScore` combines as `(ourX - theirX) * phase_weight`
- Sign conventions consistent: positive = good for side to move

### 4.2 Phase Interpolation
- Phase weights use `(256 - phase)/256` (middlegame) and `phase/256` (opening)
- Transitions are continuous — no discontinuities
- **Issue:** `game_phase()` divisor was wrong (fixed), causing phase misalignment

### 4.3 Material Scaling
- Line 135: `classicalWeight = clamp(material / 800, 0, 64)`
- Uses TOTAL material (both sides) — reasonable for "less important in endgames"
- Classical terms become negligible below ~51200 total material (≈ endgame)

### 4.4 Edge Cases
- **Missing king:** `pos.square<KING>(us)` asserts count==1 — safe
- **No pieces of type:** Range-based for loops handle empty bitboards correctly
- **Castling detection:** Uses king square (g1/c1/g8/c8) — works for Chess960
- **Rule50 dampening:** Applied AFTER classical addition — correct

### 4.5 Integer Overflow Risk
- Max `classicalScore` components: each term ~few hundred, sum < 2000
- `classicalWeight` max 64, division by 64 → max contribution ~2000
- `nnue` ~ few thousand, multiplication by `(77871 + material)` fits in 32-bit
- **No overflow risk** with current scales

---

## 5. Computational Efficiency Findings

### 5.1 Repeated Calculations (Per Evaluation Call)

| Calculation | Called In | Frequency | Optimization |
|-------------|-----------|-----------|--------------|
| `pos.square<KING>(us)` | king_safety, initiative, defensive_resources, advantage_conversion | 4-6x | **Cache once** in evaluate() |
| `pos.attacks_by<PT>(color)` | king_safety (5x), initiative (5x), positional_sacrifice (5x) | 15-20x | **Cache all 10** (5 pieces × 2 colors) |
| `popcount(enemyPressure & kingZone)` | king_safety, initiative, positional_sacrifice | 3x | Reuse cached attacks |
| Passed pawn detection | defensive_resources, advantage_conversion | 2x | **Unify** into helper |
| `between_bb()` | piece_coordination (batteries, connected rooks) | O(N²) pairs | Acceptable (max 2-4 rooks/queens) |

### 5.2 Expensive Operations
- **Nested loops in `piece_coordination`:** O(rooks × queens) + O(bishops × queens) + O(knights) + O(rooks²) — max ~50 iterations, acceptable
- **Checking moves loop:** 5 piece types × popcount — cheap
- **Passed pawn detection:** O(pawns × ranks) — up to 8×8 = 64 iterations per side, called twice → **unify**
- **Fortress detection:** Not implemented (only pawn shield check) — cheap

### 5.3 Recommended Optimizations
1. **Cache king squares** in `evaluate()` — pass to all classical functions
2. **Cache attack bitboards** (10 calls to `attacks_by`) — compute once per position
3. **Unify passed pawn logic** into `ClassicalEval::passed_pawns(pos, color)`
4. **Consider memoization** for `game_phase()` (called 3x per eval: evaluate, trace, evaluate_with_style)

---

## 6. Phase 1 & Phase 2 Integration Findings

### 6.1 Objective/Style Separation
| Component | Phase 1 Design | Phase 2 Implementation | Status |
|-----------|----------------|------------------------|--------|
| `evaluate()` | Pure objective | Added classical terms (objective) | ✓ OK (after fix) |
| `evaluate_with_style()` | Adds `attack_pressure * scale` | Also inherits classical terms | ✓ OK |
| `positional_sacrifice()` | N/A | **Leaked style check** | **FIXED** |
| `sacrifice_compensation()` | Search-only, style-gated | Called by classical (was gated) | **DECOUPLED** |

### 6.2 `attack_pressure` vs `initiative` Overlap
- `Search::attack_pressure()` (search.h:88): Used for style bonus, pruning bias
- `ClassicalEval::initiative()` (evaluate.cpp:310): Used in objective evaluation
- **Both compute nearly identical metrics:** king zone pressure, direct attackers, slider pressure
- **Result:** Same positional concept evaluated in search (style) AND evaluation (objective) → architectural confusion

### 6.3 `sacrifice_compensation` vs `positional_sacrifice`
- `Search::sacrifice_compensation()` (search.h:186): Search heuristic, style-gated, takes `materialSacrificed`
- `ClassicalEval::positional_sacrifice()` (evaluate.cpp:571): Now independent, fixed cap 80
- **Fixed:** Decoupled; classical version evaluates positional compensation objectively

### 6.4 Time Management Integration
- Phase 1: `tactical_volatility()` influences time allocation
- Phase 2: Classical terms increase eval complexity → may affect `tactical_volatility` indirectly
- No direct coupling — acceptable

---

## 7. Individual Heuristic Review

### 7.1 King Safety
**Strengths:** Proper pawn shield (after fix), zone attacks, direct attackers, open files, castling bonus
**Weaknesses:** 
- No storm attack detection (enemy pawns advancing on king)
- No piece-specific weights (queen pressure = pawn pressure)
- **Double-counts NNUE KingSafety + Threats**

### 7.2 Initiative
**Strengths:** Multi-factor (zone pressure, direct attackers, sliders, checks, center, activity, development)
**Weaknesses:**
- Checking moves uses `pos.check_squares()` — duplicates search's move generation info
- Center control bitboard constants hardcoded (could use `CenterBB` / `ExtendedCenterBB`)
- **Double-counts NNUE Threats**

### 7.3 Piece Coordination
**Strengths:** Comprehensive — batteries, outposts, pawn chains, open files, bishop/knight pairs, Q+N, connected rooks
**Weaknesses:**
- Outpost detection: `ourPawnAttacks & square_bb(n)` — only checks if OUR pawn attacks the square, not if pawn PROTECTS it (could be enemy pawn attack too)
- **Double-counts NNUE PieceCoordFeatureSet explicitly**

### 7.4 Defensive Resources
**Strengths:** Fortress-ish (pawn shield), blockade, counterplay, cohesion, endgame king, opposite bishops
**Weaknesses:**
- Fortress detection minimal (only pawn shield count ≥2)
- Passed pawn detection duplicated with advantage_conversion
- Opposite-colored bishops bonus (+25) may overstate drawing chances in winning positions

### 7.5 Positional Sacrifice (Fixed)
**Now:** Objective compensation heuristic — king pressure, direct attackers, activity, checks, development
**Cap:** 80 (fixed) — reasonable for "positional" compensation without material count
**Integration:** Added to classical score unconditionally — may reward attacking positions even when down material

### 7.6 Advantage Conversion
**Strengths:** Simplification (trade queens/rooks), king activity, passed pawns, enemy restriction, zugzwang
**Weaknesses:**
- Hard threshold `materialAdv < 300` → discontinuous at 300cp advantage
- Passed pawn detection uses simplified logic (ignores enemy king, blocked squares)
- Zugzwang bonus (+10) applied unconditionally in queenless endgames < 6000 material

---

## 8. Files Modified

| File | Changes |
|------|---------|
| `src/evaluate.cpp` | 7 confirmed defects fixed (see Section 2) |

**All fixes are source-level only — no compilation or testing performed.**

---

## 9. Remaining Concerns (Deferred for Future Validation)

| Concern | Severity | Requires Runtime Validation |
|---------|----------|----------------------------|
| NNUE double-counting inflation | **HIGH** | Yes — measure eval magnitude vs pure NNUE |
| Classical term weighting (phase/material scaling) | **HIGH** | Yes — tune weights, check endgame scaling |
| `positional_sacrifice` unconditional addition | **MEDIUM** | Yes — verify doesn't reward unsound attacks |
| `advantage_conversion` 300cp threshold discontinuity | **MEDIUM** | Yes — test smooth scaling alternatives |
| Fortress detection completeness | **MEDIUM** | Yes — verify doesn't mis-evaluate winning positions |
| Computational overhead (15-20 attack generations/eval) | **LOW** | Yes — benchmark NPS impact |
| Opposite-colored bishop drawing bonus | **LOW** | Yes — verify in winning endgames |

---

## 10. Runtime Validation Required (Future)

Before considering Phase 2 complete, the following **must** be validated empirically:

1. **Baseline Comparison:** Pure NNUE vs NNUE+Classical at fixed depth — measure eval correlation, magnitude
2. **Double-Counting Test:** Disable each classical term individually — measure Elo impact
3. **Phase Weight Tuning:** Grid search phase/material scaling parameters
4. **Style Mode Interaction:** Verify `evaluate_with_style()` aggression bonus doesn't compound with classical initiative
5. **NPS Benchmark:** Measure evaluation speed vs pure NNUE
6. **Endgame Accuracy:** Test tablebase positions with classical terms enabled/disabled
7. **Sacrifice Soundness:** Generate positions with known sound/unsound sacrifices — verify `positional_sacrifice` discriminates

---

## 11. Conclusion

Phase 2 adds **conceptually valuable** classical evaluation components but the implementation **as written had critical defects** preventing compilation and contained **architectural violations** of Phase 1's objective/style separation.

**Confirmed defects have been fixed in source.** However, the **fundamental double-counting risk with NNUE remains unresolved** and requires empirical validation to determine appropriate term weights or component removal.

**Recommendation:** Do not proceed to Phase 3 until runtime validation confirms the classical terms provide net positive signal beyond what NNUE already encodes. Consider a minimal subset (defensive resources, advantage conversion) rather than all six components.

---

*End of Audit — Static Analysis Only*