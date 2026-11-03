# GAMBO UCI Options Reference

## Complete Option List

### Stockfish Core Options (Preserved)
| Option | Type | Default | Description |
|--------|------|---------|-------------|
| Hash | spin | 16 | Transposition table size (MB) |
| Threads | spin | 1 | Number of search threads |
| MultiPV | spin | 1 | Number of principal variations |
| Skill Level | spin | 20 | Playing strength (0-20) |
| UCI_LimitStrength | check | false | Enable Elo limiting |
| UCI_Elo | spin | 1350 | Target Elo when limited |
| SyzygyPath | string | "" | Syzygy tablebase path |
| SyzygyProbeDepth | spin | 1 | Min depth for TB probing |
| Syzygy50MoveRule | check | true | Apply 50-move rule for TB |
| SyzygyProbeLimit | spin | 7 | Max TB probes per move |
| EvalFile | string | nn-*.nnue | NNUE network file |
| NumaPolicy | combo | auto | NUMA allocation policy |
| MoveOverhead | spin | 10 | Time overhead (ms) |
| MinimumThinkingTime | spin | 20 | Min time per move (ms) |
| SlowMover | spin | 100 | Time scaling factor |
| nodestime | check | false | Use nodes instead of time |
| UCI_ShowWDL | check | false | Show WDL instead of score |
| UCI_Chess960 | check | false | Chess960 mode |
| UCI_AnalyseMode | check | false | Analysis mode |
| UCI_Opponent | string | "" | Opponent name |

---

### Classical Evaluation Ablation (Phase 2)
| Option | Type | Default | Description |
|--------|------|---------|-------------|
| Classical.KingSafety | check | true | King safety evaluation |
| Classical.Initiative | check | true | Attack/initiative evaluation |
| Classical.Coordination | check | true | Piece coordination evaluation |
| Classical.Defense | check | true | Defensive resources evaluation |
| Classical.Sacrifice | check | true | Positional sacrifice compensation |
| Classical.Conversion | check | true | Advantage conversion technique |

### NNUE Evaluation
| Option | Type | Default | Description |
|--------|------|---------|-------------|
| Eval.PhaseAdaptiveNNUE | check | true | Phase-adaptive NNUE blending |
| Eval.NNUEBackend | combo | Stockfish | NNUE backend: Stockfish, Gambo, Blend |
| Eval.NNUEBlendWeight | spin | 50 | Stockfish weight in Blend mode (0-100) |

---

### Tier 1: Advanced Search History (GAMBO.*)

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| GAMBO.ThreatMapHistory | check | true | 8-category threat-indexed history (TMIHH) |
| GAMBO.ColourContinuationHistory | check | true | Colour-indexed continuation history (CICH) |
| GAMBO.MultiPlyContinuationHistory | check | false | 2-ply continuation history (MPCH) |
| GAMBO.MoveTypeHistory | check | true | Move-type conditioned history (MTCH) |
| GAMBO.QSearchCaptureHistory | check | true | SEE-aware qsearch capture history (CHQO) |

### Tier 2: Performance & Memory (GAMBO.*)

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| GAMBO.IncrementalNNUE | check | true | Incremental feature updates (INFU) |
| GAMBO.FastAccumulatorRefresh | check | true | Fast accumulator refresh (NFAR) |
| GAMBO.SIMDNNUE | check | true | SIMD-optimized NNUE (SONI) |
| GAMBO.PrefetchTT | check | false | TT memory prefetching (SMP) |
| GAMBO.CompactTT | check | false | Compact TT layout (CTTEL/CATT) |
| GAMBO.NUMAAlloc | check | true | NUMA-aware allocation (NASA) |
| GAMBO.PGOProfile | string | "" | PGO profile data path |

### Tier 3: MetaSearch (Implicit - always on)
| Feature | Description |
|---------|-------------|
| PCE | Position Criticality Engine |
| SID | Search Instability Detection |
| (ASRA, TVS, CCA, RCRA) | Planned/implicit |

### Tier 4: Experimental (GAMBO.*) — ALL DEFAULT OFF

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| GAMBO.SearchPolicy | combo | Standard | Search policy: Standard, PG-MCTS, PUCT, Hybrid |
| GAMBO.PGMCTS | check | false | Policy-guided MCTS |
| GAMBO.PGMCTS.Nodes | spin | 1000 | MCTS node budget |
| GAMBO.PGMCTS.CPUCT | spin | 125 | PUCT constant (×100) |
| GAMBO.PUCT | check | false | PUCT selection for MCTS |
| GAMBO.PUCT.CPUCT | spin | 125 | PUCT constant (×100) |
| GAMBO.PUCT.DirichletAlpha | spin | 30 | Dirichlet noise (×100) |
| GAMBO.TAGS | check | false | Transposition-aware graph search |
| GAMBO.TAGS.MaxNodes | spin | 1000000 | Max graph nodes |
| GAMBO.DAGSearch | check | false | DAG search |
| GAMBO.BatchedNNUE | check | false | Batched NNUE inference |
| GAMBO.BatchSize | spin | 8 | Batch size for NNUE |
| GAMBO.MHNE | check | false | Multi-head neural eval |
| GAMBO.MHNE.Heads | string | Value | Comma-separated heads: Value,WDL,Policy,Uncertainty |
| GAMBO.HybridSearch | check | false | Hybrid alpha-beta + MCTS |
| GAMBO.HybridSearch.ABDepth | spin | 80 | Alpha-beta depth % |
| GAMBO.HybridSearch.MCTSBudget | spin | 20 | MCTS node budget % |

---

## Option Interaction Rules

1. **Mutual Exclusion**: `Eval.NNUEBackend = Gambo` requires trained GAMBO network
2. **Dependencies**: `GAMBO.BlendedNNUE` requires both backends available
3. **Exclusion**: Experimental policies (`GAMBO.SearchPolicy != Standard`) disable some Tier 1 heuristics
3. **Resource**: `GAMBO.PGMCTS.Nodes` + `GAMBO.HybridSearch.MCTSBudget` must not exceed total node budget
4. **Hardware**: `GAMBO.SIMDNNUE` requires AVX2/SSE4.1; `GAMBO.NUMAAlloc` requires multi-socket

---

## Setting Options via UCI

```bash
# Enable GAMBO NNUE backend
setoption name Eval.NNUEBackend value Gambo

# Blend Stockfish + GAMBO (70% Stockfish)
setoption name Eval.NNUEBackend value Blend
setoption name Eval.NNUEBlendWeight value 70

# Disable classical evaluation for pure NNUE
setoption name Classical.KingSafety value false
setoption name Classical.Initiative value false
setoption name Classical.Coordination value false
setoption name Classical.Defense value false
setoption name Classical.Sacrifice value false
setoption name Classical.Conversion value false

# Enable experimental PG-MCTS
setoption name GAMBO.SearchPolicy value PG-MCTS
setoption name GAMBO.PGMCTS.Nodes value 5000

# Enable batched NNUE for MultiPV
setoption name GAMBO.BatchedNNUE value true
setoption name GAMBO.BatchSize value 16

# Compact TT for memory-constrained environments
setoption name GAMBO.CompactTT value true
```

---

## Option Validation

| Option | Validation |
|--------|------------|
| `Eval.NNUEBackend` | Must be "Stockfish", "Gambo", or "Blend" |
| `GAMBO.SearchPolicy` | Must be "Standard", "PG-MCTS", "PUCT", or "Hybrid" |
| Spin ranges | Enforced by UCI protocol (min/max) |
| Combo options | Enum validation in option registration |

---

## Recommended Configurations

### Pure NNUE (Maximum Speed)
```
setoption name Classical.KingSafety value false
setoption name Classical.Initiative value false
setoption name Classical.Coordination value false
setoption name Classical.Defense value false
setoption name Classical.Sacrifice value false
setoption name Classical.Conversion value false
setoption name Eval.PhaseAdaptiveNNUE value true
```

### Aggressive Style (GAMBO Default)
```
setoption name Style value UltraAggressive
setoption name Classical.Sacrifice value true
setoption name Eval.PhaseAdaptiveNNUE value true
```

### Maximum Strength (All Features)
```
setoption name Classical.KingSafety value true
setoption name Classical.Initiative value true
setoption name Classical.Coordination value true
setoption name Classical.Defense value true
setoption name Classical.Sacrifice value true
setoption name Classical.Conversion value true
setoption name Eval.PhaseAdaptiveNNUE value true
setoption name GAMBO.ThreatMapHistory value true
setoption name GAMBO.ColourContinuationHistory value true
setoption name GAMBO.MoveTypeHistory value true
setoption name GAMBO.QSearchCaptureHistory value true
setoption name GAMBO.IncrementalNNUE value true
setoption name GAMBO.FastAccumulatorRefresh value true
setoption name GAMBO.SIMDNNUE value true
setoption name GAMBO.NUMAAlloc value true
```

### Memory-Constrained (e.g., 64MB Hash)
```
setoption name Hash value 64
setoption name GAMBO.CompactTT value true
setoption name GAMBO.MultiPlyContinuationHistory value false
```

### Tournament (Recommended)
```
setoption name Hash value 1024
setoption name Threads value 8
setoption name SyzygyPath value /path/to/syzygy
setoption name Eval.NNUEBackend value Blend
setoption name Eval.NNUEBlendWeight value 60
```