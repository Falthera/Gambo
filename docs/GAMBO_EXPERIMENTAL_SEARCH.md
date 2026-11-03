# GAMBO Experimental Search (Tier 4)

## Overview

Tier 4 features are research-oriented and implemented as isolated, selectable experimental components. **They must remain disabled by default** and cannot destabilize the standard GAMBO engine.

## 33. PG-MCTS — Policy-Guided Monte Carlo Tree Search

### Purpose
Investigate MCTS guided by neural move policy while retaining GAMBO's evaluation.

### Architecture
```
Policy Network → Move Prior → MCTS Selection → Value Backup → GAMBO Evaluation
                    ↑                                    ↓
               GAMBO NNUE                      Alpha-Beta Verification
```

### Components
- **Policy Head**: Multi-head NNUE (see MHNE) or separate lightweight network
- **MCTS Core**: `search/mcts/` - UCT/PUCT selection, simulation, backprop
- **GAMBO Integration**: Alpha-beta verification of MCTS principal variation
- **Resource Budget**: Nodes/time limit per search

### Requirements
- Define selection formula (PUCT)
- Policy prior handling (Dirichlet noise at root)
- Value updates from GAMBO evaluation
- Terminal state handling (mate, draw, TB)
- Independent testability

### UCI Option
`GAMBO.PGMCTS` (default: off)
- `GAMBO.PGMCTS.Nodes` (default: 1000)
- `GAMBO.PGMCTS.CPUCT` (default: 1.25)

---

## 34. PUCT — Predictor Upper Confidence Bound Search

### Purpose
Implement PUCT selection for MCTS subsystem.

### Formula
```
PUCT = Q(s,a) + c_puct * P(s,a) * sqrt(N(s)) / (1 + N(s,a))
```

Where:
- `Q(s,a)`: Mean value from GAMBO evaluation
- `P(s,a)`: Policy prior from policy head
- `N(s)`: Parent visit count
- `N(s,a)`: Edge visit count
- `c_puct`: Exploration constant

### Requirements
- Selection, expansion, simulation, backprop phases
- Virtual loss for parallelization
- Dirichlet noise at root: `P' = (1-ε)P + ε*Dir(α)`

### UCI Options
`GAMBO.PUCT` (default: off)
- `GAMBO.PUCT.CPUCT` (default: 1.25)
- `GAMBO.PUCT.DirichletAlpha` (default: 0.3)

---

## 35. T-AGS — Transposition-Aware Graph Search

### Purpose
Graph-based search reusing equivalent positions.

### Challenges
- Position identity: Zobrist key + game state (castling, EP, 50-move)
- Repetition handling: Path-dependent state
- Safe reuse: Avoid invalid value transfer across contexts
- Memory growth: Node/edge limits

### Implementation Plan
- `search/graph/`: Node = position + context, Edge = move
- Transposition detection: `TT::probe()` + graph node lookup
- Safe reuse: Only reuse if context-compatible (same ply, same player, no path-dependent state)
- Memory limit: LRU eviction, max nodes configurable

### UCI Options
`GAMBO.TAGS` (default: off)
- `GAMBO.TAGS.MaxNodes` (default: 1M)
- `GAMBO.TAGS.ReuseThreshold` (default: exact bound only)

---

## 36. DAG-Search — Directed Acyclic Graph Search

### Purpose
DAG representation of reusable search states.

### Difference from T-AGS
- DAG: No cycles by construction (depth-increasing edges only)
- T-AGS: General graph with cycle detection
- DAG simpler but less reuse

### Requirements
- Node ownership: Thread-local + shared
- Cycle detection: DFS with visited flags
- Memory bounds: Max nodes, LRU

### UCI Options
`GAMBO.DAGSearch` (default: off)

---

## 37. PSPA — Pluggable Search Policy Architecture

### Purpose
Architecture for selectable search policies.

### Policies
| Policy | Description | Status |
|--------|-------------|--------|
| `Standard` | Alpha-beta (default) | Stable |
| `PG-MCTS` | Policy-guided MCTS | Experimental |
| `PUCT` | PUCT selection | Experimental |
| `Hybrid` | Alpha-beta + MCTS | Experimental |

### Architecture
```
SearchPolicy (interface)
    ├─ StandardPolicy (alpha-beta)
    ├─ MCTSPolicy (PG-MCTS/PUCT)
    └─ HybridPolicy (alpha-beta + MCTS)
```

### Requirements
- Policy selection at runtime (UCI)
- Strict thread-safety
- Single-thread mode preservation
- Fallback to Standard on failure

### UCI Option
`GAMBO.SearchPolicy` (default: Standard)
- Values: `Standard`, `PG-MCTS`, `PUCT`, `Hybrid`

---

## 38. BNNI — Batched Neural Network Inference

### Purpose
Batch NNUE inference for suitable workloads.

### Use Cases
- MultiPV root move evaluation
- MCTS simulation batching
- ProbCut verification searches

### Requirements
- Batching mechanism: Accumulate positions, infer together
- Latency budget: < 1ms per batch
- CPU + optional accelerator (no GPU assumption)
- Portable fallback: Sequential inference

### Implementation
```
BatchAccumulator
    add_position(pos)
    infer_batch() → vector<Value>
```

### UCI Options
`GAMBO.BatchedNNUE` (default: off)
- `GAMBO.BatchSize` (default: 8)

---

## 39. MHNE — Multi-Head Neural Evaluation

### Purpose
Optional multi-head neural evaluation architecture.

### Potential Heads
| Head | Output | Purpose |
|------|--------|---------|
| Value | Scalar | Position evaluation (centipawns) |
| WDL | 3-class | Win/Draw/Loss probabilities |
| Policy | 4672/1968 | Move probabilities |
| Uncertainty | Scalar | Position uncertainty estimate |
| Auxiliary | Various | Tactical, structural predictions |

### Requirements
- Architecture definition (shared trunk + heads)
- Model loading/validation (ONNX/Stockfish format)
- Training data requirements documented
- Existing network path preserved

### UCI Options
`GAMBO.MHNE` (default: off)
- `GAMBO.MHNE.Heads` (comma-separated list)

---

## 40. HA-MS — Hybrid Alpha-Beta + MCTS Search

### Purpose
Combine alpha-beta depth with MCTS breadth.

### Architecture
```
Root
  ├─ Alpha-Beta (depth N) → Principal Variation
  └─ MCTS (budget B) → Exploration
       ↓
  Merge: Alpha-beta guides MCTS policy, MCTS verifies alpha-beta
```

### Information Flow
- Alpha-beta → MCTS: Policy prior, value bounds, PV moves
- MCTS → Alpha-beta: Verification of critical nodes, alternative moves

### Requirements
- Strict resource limits (time/nodes split)
- No duplicated analysis
- Standard alpha-beta as reliable fallback

### UCI Options
`GAMBO.HybridSearch` (default: off)
- `GAMBO.HybridSearch.ABDepth` (default: 80% of total)
- `GAMBO.HybridSearch.MCTSBudget` (default: 20% of nodes)

---

## Safety & Isolation Requirements

| Requirement | Implementation |
|-------------|----------------|
| Default OFF | All `GAMBO.*` experimental options default `false` |
| Thread Safety | Thread-local state, no shared mutable state |
| Single-Thread Mode | All experimental paths work with `Threads=1` |
| Fallback | Auto-revert to Standard on error/timeout |
| Resource Limits | Hard node/time bounds per component |
| No State Corruption | Experimental paths cannot modify main search state |

## Testing Experimental Features

```bash
# Enable experimental policy
./stockfish
> setoption name GAMBO.SearchPolicy value PG-MCTS
> setoption name GAMBO.PGMCTS.Nodes value 5000
> go depth 20

# Batched inference
> setoption name GAMBO.BatchedNNUE value true
> setoption name GAMBO.BatchSize value 16
> bench 16 1 8 default depth
```