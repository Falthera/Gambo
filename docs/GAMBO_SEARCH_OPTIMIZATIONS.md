# GAMBO Search Optimizations (Tier 1 & Tier 3)

## Tier 1: Advanced Search Improvements

### 1. TMIHH — Threat-Map Indexed History Heuristic

**Purpose**: Enhance history heuristic with tactical threat context.

**Implementation**:
- 8 threat categories: `NONE(0)`, `KING_RING(1)`, `CHECK(2)`, `SLIDER(3)`, `KNIGHT_FORK(4)`, `PIN(5)`, `DISCOVERED(6)`, `MULTIPLE(7)`
- New `ThreatMapHistory` table: `Stats<int16_t, 30000, 8, PIECE_NB, SQUARE_NB>`
- Threat classification in `threat_category(pos, us, move, ss)` (search.h:285-340)
- Integrated in `update_quiet_histories`, `update_continuation_histories`, capture updates

**UCI Option**: `GAMBO.ThreatMapHistory` (default: on)

### 2. CICH — Colour-Indexed Continuation History

**Purpose**: Account for side-to-move in continuation history.

**Implementation**:
- New `ColourContinuationHistory`: `MultiArray<PieceToHistory, COLOR_NB, COLOR_NB, PIECE_NB, SQUARE_NB>`
- Indexed by `[prev_color][curr_color][prev_piece][prev_to][piece][to]`
- Updated in `update_continuation_histories` with scaled bonus (512/1024)

**UCI Option**: `GAMBO.ColourContinuationHistory` (default: on)

### 3. MPCH — Multi-Ply Continuation History

**Purpose**: 2-ply continuation context for richer move prediction.

**Implementation**:
- `MultiPlyContinuationHistory`: `MultiArray<PieceToHistory, PIECE_NB, SQUARE_NB, PIECE_NB, SQUARE_NB>`
- Reuses `ColourContinuationHistory` storage
- Updated for plies 2-3 with bonus 256/1024

**UCI Option**: `GAMBO.MultiPlyContinuationHistory` (default: off)

### 4. MTCH — Move-Type Conditioned History

**Purpose**: Separate history tables for different move categories.

**Implementation**:
- 7 move types: `QUIET(0)`, `CAPTURE(1)`, `PROMOTION(2)`, `CHECK(3)`, `EVASION(4)`, `CASTLING(5)`, `ENPASSANT(6)`
- `MoveTypeHistory`: `Stats<int16_t, 30000, 7, PIECE_NB, SQUARE_NB>`
- `MoveTypeCaptureHistory`: `Stats<int16_t, 10692, 7, PIECE_NB, SQUARE_NB, PIECE_TYPE_NB>`
- Classification in `move_type_class(pos, move, inCheck)` (search.h:342-370)
- Used in movepick scoring and LMR history

**UCI Option**: `GAMBO.MoveTypeHistory` (default: on)

### 5. SEE-GCP — SEE-Guided Capture Partitioning

**Purpose**: Classify captures by SEE value for better ordering/pruning.

**Implementation**:
- 4 classes: `WINNING(0)`, `LOSING(1)`, `PROMOTION(2)`, `ENPASSANT(3)`
- Classification in `see_class(pos, move, seeValue)` (search.h:372-380)
- Boost winning captures (+1024), penalize losing (-512)
- Promotions/enpassant get priority (+1024)

### 6. CHQO — Capture-History Quiescence Ordering

**Purpose**: SEE-aware capture ordering in qsearch.

**Implementation**:
- `QSearchCaptureHistory`: `Stats<int16_t, 10692, PIECE_NB, SQUARE_NB, PIECE_TYPE_NB>`
- SEE-GCP partitioning in `QCAPTURE_INIT` stage (movepick.cpp)
- Winning captures +2048, promotions/enpassant +1024, losing -1024
- QSearch capture history bonus in scoring

**UCI Option**: `GAMBO.QSearchCaptureHistory` (default: on)

### 7. HGQMP — History-Guided Quiet Move Pruning

**Purpose**: Safe quiet move pruning using history and threat context.

**Implementation**:
- `prune_quiet_move(pos, ss, move, moveCount, depth, history, inCheck, ttPv, threatCat)` (search.h:382-415)
- Conditions: not in check, not TT move, not killer, not good history, not tactical
- Depth threshold: `depth/4 + 2`
- History threshold: `< -1000 * depthThreshold`

### 8. URD — Upcoming Repetition Detection

**Purpose**: Detect moves leading to upcoming repetitions.

**Implementation**:
- `upcoming_repetition(pos, move, depth, ss)` (search.h:417-440)
- Heuristic: balanced positions (< 4000 non-pawn mat, eval < 50cp) with shuffling moves

### 9. EGC — Extension Growth Control

**Purpose**: Limit cumulative extensions along search path.

**Implementation**:
- Stack fields: `extensionAccumulator`, `singularExtensionCount`
- Hard limit: `MAX_EXTENSIONS_PER_PATH = 6`
- Tracks extensions in `update_continuation_histories` and singular search

### 10. SESR — Singular Extension Search Refinement

**Purpose**: Improved singular extension verification.

**Implementation**:
- TT depth bonus for exact scores: `tripleMargin -= ttDepthBonus * 2`
- EGC tracking: `extensionAccumulator`, `singularExtensionCount`
- Verification with TT depth awareness

### 11. LMRC — LMR Context Calibration

**Purpose**: Context-aware late-move reductions.

**Implementation**:
- `contextual_reduction(pos, ss, improving, depth, moveCount, delta, history, threatCat, ttPv)` (search.cpp:1876-1915)
- Reduces less for: good history, tactical positions (threatCat≥2), PV nodes, fail-high history
- Reduces more for: late moves with bad history
- Minimum reduction clamp at 0

### 12. QTC — QSearch Transposition Correction

**Purpose**: Correct qsearch TT values using static/TT eval delta.

**Implementation**:
- Track `qsearchCorrection = ttData.eval - ttData.value` (search.cpp:1702-1710)
- Apply if `|correction| < 200`: `correctedValue = ttData.value + correction / 4`

---

## Tier 3: GAMBO MetaSearch and Position Intelligence

### 23. PCE — Position Criticality Engine

**Purpose**: Estimate position sensitivity to search mistakes.

**Signals** (weighted, max 255):
| Signal | Weight | Description |
|--------|--------|-------------|
| King Exposure | 40 | Enemy pressure on king zones |
| Forcing Move Density | 35 | % of moves that are checks/captures/promos |
| Evaluation Volatility | 35 | Static eval vs material imbalance |
| PV Instability | 30 | PV move changes across iterations |
| Candidate Disagreement | 30 | Multiple moves within 100cp of best |
| Tactical Threat Density | 30 | Enemy pressure on king zones |
| Material Instability | 25 | Low material, small difference |
| Search Tree Instability | 25 | Fail-highs, cutoffs, checks |

**Output**: `CriticalityLevel` (LOW/MEDIUM/HIGH/EXTREME)

**Integration**: Drives aspiration window, triggers TVS/TVS

**UCI Option**: Implicit (always on)

### 24. ASRA — Adaptive Search Resource Allocation

**Purpose**: Dynamic search effort based on position criticality.

**Implementation**: (Planned - uses PCE output)
- HIGH/EXTREME criticality → wider aspiration windows
- EXTREME → additional verification time

### 25. SID — Search Instability Detection

**Purpose**: Independent monitoring of search reliability.

**Metrics**:
- `pvChanges`: PV move changes × 15
- `evalSwings`: Score swings > 100cp × 10
- `tacticalVolatility`: fail-highs, checks × 5

**Levels**: STABLE(0) → MINOR(1) → MODERATE(2) → HIGH(3) → CRITICAL(4)

**Integration**: `needs_verification()` triggers TVS

**UCI Option**: Implicit (always on)

### 26. TVS — Tactical Verification Search

**Purpose**: Bounded verification of critical tactical claims.

**Implementation**: (Planned)
- Triggered by SID `needs_verification()` or PCE EXTREME
- Bounded depth/search nodes
- Returns: `VERIFIED`, `REFUTED`, `UNKNOWN`

### 27. PNMV — Proof-Number Mate Verification

**Purpose**: Specialized forced-mate verification.

**Implementation**: (Planned - experimental)

### 28. PTC — Persistent Threat Context

**Purpose**: Track threats across variations.

**Implementation**: (Planned)

### 29. CCA — Counterfactual Candidate Analysis

**Purpose**: Root candidate robustness analysis.

**Implementation**: (Planned)

### 30. EDSE — Event-Driven Search Extensions

**Purpose**: Extensions triggered by significant events.

**Implementation**: (Planned)

### 31. RCRA — Root Candidate Robustness Analysis

**Purpose**: Evaluate candidate reliability at root.

**Implementation**: (Planned)

### 32. HSWS — Heterogeneous Search Worker Scheduling

**Purpose**: Specialized thread roles.

**Implementation**: (Planned)

---

## Interaction Matrix

| Feature | TMIHH | CICH | MPCH | MTCH | SEE-GCP | CHQO | HGQMP | PCE | SID |
|---------|-------|------|------|------|---------|------|-------|-----|-----|
| TMIHH   | -     | ✓    | ✓    | ✓    | ✓       | ✓    | ✓     | ✓   | ✓   |
| CICH    | ✓     | -    | ✓    |      |         |      |       | ✓   | ✓   |
| MPCH    | ✓     | ✓    | -    |      |         |      |       |     |     |
| MTCH    | ✓     |      |      | -    | ✓       | ✓    | ✓     |     |     |
| SEE-GCP | ✓     |      |      | ✓    | -       | ✓    | ✓     |     |     |
| CHQO    | ✓     |      |      | ✓    | ✓       | -    | ✓     |     |     |
| HGQMP   | ✓     |      |      | ✓    | ✓       | ✓    | -     | ✓   | ✓   |
| PCE     | ✓     | ✓    |      |      |         |      | ✓     | -   | ✓   |
| SID     | ✓     | ✓    |      |      |         |      | ✓     | ✓   | -   |

---

## Testing Recommendations

1. **Unit Tests**: Each heuristic with known positions
2. **Regression**: `bench` with all options on/off
3. **Ablation**: Individual UCI option toggles
4. **Tactical Suite**: WAC, EPD test positions
5. **Time Control**: 10+0.1, 60+0.6, 180+1.8