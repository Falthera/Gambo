# GAMBO Final Architecture

## Overview

GAMBO is an aggressive, high-performance UCI chess engine derived from Stockfish 18. Its architecture extends the classical alpha-beta search with NNUE evaluation, aggressive heuristics, and a novel multi-tier optimization framework.

## Core Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        UCI Layer                             │
│  Options, Commands, Protocol Handling                       │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Search Layer                            │
│  Alpha-Beta + Iterative Deepening + Aspiration Windows      │
│  ├─ Move Ordering (TT, History, SEE)                        │
│  ├─ Pruning (LMR, Futility, ProbCut, Null Move)             │
│  ├─ Extensions (Singular, Checks, EGC/SESR controlled)      │
│  ├─ Tier 1: TMIHH, CICH, MPCH, MTCH, SEE-GCP, CHQO, HGQMP  │
│  ├─ Tier 3: PCE, SID, ASRA, TVS, CCA, RCRA                  │
│  └─ Tier 4: PG-MCTS, PUCT, T-AGS (experimental)             │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Evaluation Layer                           │
│  Hybrid: Stockfish NNUE + Classical + GAMBO NNUE            │
│  ├─ Objective Evaluation (style-independent)                │
│  ├─ Style-Biased Evaluation (Balanced / UltraAggressive)    │
│  ├─ Classical Terms (KingSafety, Initiative, Coordination,  │
│  │   Defense, Sacrifice, Conversion)                        │
│  ├─ Phase-Adaptive NNUE Blending                            │
│  └─ GAMBO NNUE (PawnPair, KingRelative, ThreatDetection,    │
│     PieceRelative; SFNNv16 topology)                        │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Infrastructure                             │
│  Transposition Table (CATT/CTTEL)                           │
│  Thread Pool (NAMA-aware)                                   │
│  NNUE Accumulators (INFU/NFAR)                              │
│  Memory Prefetching (SMP)                                   │
└─────────────────────────────────────────────────────────────┘
```

## Module Responsibilities

### Search Module (`search/`)
- **Alpha-Beta Core**: `search.cpp` - Main iterative deepening, aspiration windows
- **Move Ordering**: `movepick.cpp` - TT, captures, quiet moves with Tier 1 heuristics
- **Pruning/Reductions**: LMR, Futility, ProbCut, Null Move, HGQMP
- **Extensions**: Singular (SESR), Checks, EGC-controlled
- **Quiescence**: `qsearch` with CHQO, QTC
- **Tier 1 Heuristics**: TMIHH, CICH, MPCH, MTCH, SEE-GCP, CHQO, HGQMP, URD, EGC, SESR, LMRC, QTC
- **Tier 3 MetaSearch**: PCE, SID, ASRA, TVS, CCA, RCRA

### Evaluation Module (`eval/`)
- **Objective Evaluation**: `evaluate.cpp` - Style-independent static evaluation
- **Style Evaluation**: `evaluate.cpp` - `evaluate_with_style()` with aggression bias
- **Classical Terms**: KingSafety, Initiative, Coordination, Defense, Sacrifice, Conversion
- **NNUE Integration**: `nnue/network.cpp` - Stockfish HalfKAv2 + GAMBO SFNNv16

### NNUE Module (`nnue/` and `gambo/nnue/`)
- **Stockfish NNUE**: HalfKAv2_hm, FullThreats, KingSafety, PieceCoordination
- **GAMBO NNUE** (`gambo/nnue/`):
  - Features: PawnPair, KingRelative, ThreatDetection, PieceRelative
  - Layers: AffineTransform (SIMD), ClippedReLU, SqrClippedReLU
  - Architecture: SFNNv16 (4-layer: sparse→dual ReLU→dense→output)
  - INFU/NFAR: Incremental feature updates for all 4 feature sets

### Infrastructure
- **Transposition Table**: `tt.cpp` - CATT (Compact), CTTEL, SMP prefetch
- **Threading**: `thread.cpp` - NUMA-aware, shared histories
- **Time Management**: `timeman.cpp` - Instability-aware
- **NNUE Accumulators**: `nnue/nnue_accumulator.cpp` - Template-based, 4 feature sets

## Data Flow

```
Position → Move Generation → Move Ordering (Tier 1) → Alpha-Beta Search
                                                      │
                                                      ▼
                                              Evaluation
                                              ┌─────────────────────┐
                                              │ Hybrid Evaluation │
                                              │ NNUE (SF + GAMBO) │
                                              │ Classical Terms   │
                                              │ Phase-Adaptive    │
                                              └─────────────────────┘
                                                      │
                                                      ▼
                                              Return Score → Pruning/Extensions
```

## Feature Interactions

| Feature | Interacts With | Integration Method |
|---------|---------------|-------------------|
| TMIHH | CICH, MPCH, MTCH | Shared history updates in `update_quiet_histories` |
| CICH | MPCH, SESR | Colour-indexed continuation history |
| MTCH | SEE-GCP, CHQO | Move-type conditioned history in movepick |
| SEE-GCP | CHQO, HGQMP | SEE classification for captures |
| HGQMP | LMRC, EGC | History-guided pruning with threat awareness |
| PCE | ASRA, SID, TVS | Criticality score drives resource allocation |
| SID | TVS, CCA, RCRA | Instability level triggers verification |
| INFU/NFAR | SONI | SIMD-optimized incremental updates |
| SMP | CATT | TT prefetching during search |
| CATT | CTTEL | Compact layout with cache awareness |

## UCI Configuration Groups

```
Stockfish.*           # Original Stockfish options
Eval.*                # PhaseAdaptiveNNUE, NNUEBackend, NNUEBlendWeight
Classical.*           # KingSafety, Initiative, Coordination, Defense, Sacrifice, Conversion
Style                 # Balanced, UltraAggressive
GAMBO.ThreatMapHistory (on)
GAMBO.ColourContinuationHistory (on)
GAMBO.MultiPlyContinuationHistory (off)
GAMBO.MoveTypeHistory (on)
GAMBO.QSearchCaptureHistory (on)
GAMBO.IncrementalNNUE (on)
GAMBO.FastAccumulatorRefresh (on)
GAMBO.SIMDNNUE (on)
GAMBO.PrefetchTT (off)
GAMBO.CompactTT (off)
GAMBO.NUMAAlloc (on)
GAMBO.PGOProfile ("")
```

## Build Configurations

| Config | Flags | Purpose |
|--------|-------|---------|
| Release | `-O3 -flto=auto -march=x86-64-bmi2` | Production |
| Debug | `-O0 -g -DDEBUG` | Development |
| Profile | `-fprofile-generate` | PGO training |
| Sanitize | `-fsanitize=address,undefined` | Bug detection |

## Thread Safety

- TT: Racy but correct (Stockfish pattern)
- Histories: Per-thread + NUMA-shared
- NNUE: Thread-local accumulators, shared weights
- PCE/SID: Thread-local detectors
- PCE/SID: Thread-local detectors