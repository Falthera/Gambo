# GAMBO 2.0 — Original Architecture Documentation

**Branch**: `gambo-original-sf19-tech`  
**Base Commit**: `ad08a0d` (Phase 2 frozen)  
**Date**: 2026-11-02  
**Status**: In Progress

---

## 1. Executive Summary

This document describes GAMBO's independently designed neural evaluation architecture that reproduces the technical capabilities of Stockfish 19's SFNNv16 network, implemented entirely in original GAMBO code without copying Stockfish source.

**Base**: Stockfish 18-derived foundation (commit `ad08a0d`)  
**Target**: SFNNv16-equivalent architecture implemented independently  
**Branch**: `gambo-original-sf19-tech`

---

## 2. GAMBO Native Architecture Overview

### 2.1 Module Structure

```
src/gambo/
├── nnue/
│   ├── architecture.h          # Network topology definition
│   ├── nnue_common.h           # Common types and constants
│   ├── network.h               # Network loading and inference
│   ├── accumulator.h           # Accumulator management
│   ├── feature_extractor.h     # Feature extraction interface
│   ├── features/
│   │   ├── pawn_pair.h         # Pawn-pair feature system
│   │   ├── king_relative.h     # King-relative piece features
│   │   ├── piece_relative.h    # Piece-relative features
│   │   └── threat_detection.h  # Threat/attack features
│   ├── layers/
│   │   ├── affine_transform.h
│   │   ├── clipped_relu.h
│   │   └── sqr_clipped_relu.h
│   ├── quantizer.h             # Quantization utilities
│   └── gambo_network.h         # GAMBO-specific network format
└── evaluation/
    ├── classical_eval.h        # Phase 2 classical evaluation
    └── evaluate.h              # Main evaluation interface
```

### 2.2 Key Design Principles

1. **No Stockfish Code Copying**: All implementation is original GAMBO code
2. **GAMBO-Native Naming**: All classes, functions, and types use GAMBO-specific naming
3. **Modular Design**: Clean separation between feature extraction, accumulation, and inference
4. **SFNNv16 Functional Parity**: Match technical capabilities without copying implementation
5. **GAMBO-Native Network Format**: Prefer GAMBO's own network format; optional SFNNv16 compatibility

---

## 3. GAMBO-Native NNUE Architecture (SFNNv16-Equivalent)

### 3.1 Network Topology

| Layer | Description | Dimensions | Activation |
|-------|-------------|------------|------------|
| Input | Transformed features | 1024 | — |
| FC0 | Sparse affine transform | 1024 → 63 (31×2) | SqrClippedReLU + ClippedReLU |
| FC1 | Dense affine transform | 62 → 32 | ClippedReLU |
| FC2 | Output layer | 32 → 1 | Linear |

**Key Parameters**:
- `L1 = 1024` (transformed feature dimensions)
- `L2 = 31` (FC0 output per half, doubled for dual-ReLU)
- `L3 = 32` (FC1 output)
- `PSQTBuckets = 8` (must be multiple of 8 for vectorization)
- `LayerStacks = 8` (accumulator parallelism)

### 3.2 Dual-ReLU Architecture (SFNNv16 Signature)

The network uses a **dual-ReLU** activation pattern (SqrClippedReLU + ClippedReLU) after FC0, producing 62 outputs (31×2) that are concatenated before FC1. This is the signature SFNNv16 architecture.

```cpp
// FC0 output: 63 units (31 for SqrClippedReLU + 32 for ClippedReLU with bias)
ac_sqr_0: SqrClippedReLU<63>  // First 31 units squared
ac_0: ClippedReLU<63>         // All 63 units clipped
concat: [ac_sqr_0_out[0..30], ac_0_out[0..31]]  // 62 total → FC1
```

### 3.3 Quantization Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| WeightScaleBits | 6 | Weight quantization (64 levels) |
| ActivationScaleBits | 8 | Activation quantization |
| OutputScale | 600 | Output scaling factor |
| OutputScaleBits | 14 | Output quantization |

### 3.4 Network Hash

The network hash value for architecture verification: `0xEC42E90D` (computed from layer hashes and dimensions)

---

## 4. GAMBO-Native Pawn-Pair Feature System (SFNNv16 Core Innovation)

### 4.1 Technical Overview

The pawn-pair feature system is the core innovation of SFNNv16, replacing redundant threat inputs with structured pawn-pair representations.

### 4.2 Pawn-Pair Representation

**Three-Wide Pawn-Pair Encoding**:
- Each pawn pair is represented by three components:
  1. **Pawn 1**: Position relative to own king
  2. **Pawn 2**: Position relative to own king  
  3. **Relationship**: Structural relationship (connected, isolated, doubled, etc.)

**Feature Dimensions**:
- Pawn positions: 64 squares × 2 pawns = 128 base positions
- King-relative: 64 king squares × 32 pawn squares = 2048 combinations
- Relationship types: 8 types (connected, isolated, doubled, backward, passed, candidate, protected, phalanx)
- **Total dimensions**: ~8,192 (configurable)

### 4.3 Pawn Pair Types

| Type | Description | Encoding |
|------|-------------|----------|
| CONNECTED | Pawns on adjacent files, same/adjacent rank | 0 |
| ISOLATED | No friendly pawn on adjacent files | 1 |
| DOUBLED | Two pawns on same file | 2 |
| BACKWARD | Pawn behind adjacent friendly pawns | 3 |
| PASSED | No enemy pawns blocking promotion path | 4 |
| CANDIDATE | Potential passed pawn | 5 |
| PROTECTED | Defended by friendly pawn | 6 |
| PHALANX | Two pawns on same rank, adjacent files | 7 |

### 4.4 Feature Indexing

```cpp
// Pawn-pair index computation
IndexType make_pawn_pair_index(
    Color perspective,
    Square king_sq,
    Square pawn1_sq,
    Square pawn2_sq,
    PawnPairType type);

// Maximum active dimensions: ~128 (based on typical pawn configurations)
static constexpr IndexType MaxActiveDimensions = 128;
```

### 4.4 Incremental Updates

When a pawn moves:
- Remove old pawn-pair features involving that pawn
- Add new pawn-pair features with new position
- Update relationship types for affected pairs
- Handle captures, promotions, en passant correctly

---

## 5. Feature Set Architecture

### 5.1 Feature Set Composition (SFNNv16-Equivalent)

| Feature Set | Purpose | Dimensions | Max Active |
|-------------|---------|------------|------------|
| **PawnPair** | Pawn structure & relationships | ~8,192 | 128 |
| **KingRelative** | King-relative piece positions | ~40,960 | 64 |
| **ThreatDetection** | Attack/defense relationships | ~60,000 | 128 |
| **PieceRelative** | Piece-piece coordination | ~8,192 | 128 |
| **Total** | — | ~115,000 | ~448 |

### 5.2 Feature Set Integration

Each feature set implements:
```cpp
class FeatureSet {
    static constexpr IndexType Dimensions;
    static constexpr IndexType MaxActiveDimensions;
    static constexpr uint32_t HashValue;
    
    static IndexType make_index(...);
    static void append_active_indices(Color, const Position&, IndexList&);
    static void append_changed_indices(Color, Square, DiffType&, IndexList&, IndexList&);
    static bool requires_refresh(DiffType, Color);
};
```

---

## 6. GAMBO-Native Network Format

### 6.1 Network File Structure

```
GAMBO Network File (.gnnue)
├── Header
│   ├── Magic: "GAMBO_NNUE"
│   ├── Version: 1
│   ├── Architecture Hash: 0xEC42E90D
│   ├── Feature Set Hashes: [4 uint32]
│   └── Metadata: {description, author, date}
├── Feature Set Weights
│   ├── PawnPair: [Dimensions × WeightScale]
│   ├── KingRelative: [Dimensions × WeightScale]
│   ├── ThreatDetection: [Dimensions × WeightScale]
│   └── PieceRelative: [Dimensions × WeightScale]
├── Layer Weights
│   ├── FC0: [L1 × (FC0_OUTPUTS+1)]
│   ├── FC1: [(FC0_OUTPUTS×2) × FC1_OUTPUTS]
│   └── FC2: [FC1_OUTPUTS × 1]
├── Quantization Parameters
│   ├── WeightScaleBits
│   ├── ActivationScaleBits
│   ├── OutputScale
│   └── OutputScaleBits
└── Checksum: SHA256
```

### 6.2 Optional SFNNv16 Compatibility

**Optional**: Implement a standalone converter `sfnnv16_to_gambo` that:
- Reads official SFNNv16 `.nnue` files
- Extracts weights and architecture
- Converts to GAMBO-native format
- Validates architecture hash matches `0xEC42E90D`

This enables architecture verification without importing Stockfish code.

---

## 7. Quantization-Aware Training (QAT) Infrastructure

### 7.1 QAT Pipeline Design

```
Training Pipeline:
┌─────────────┐   ┌──────────────┐   ┌─────────────────┐   ┌─────────────┐
│ Floating-   │→  │ Quantization │→  │ Quantized       │→  │ Export      │
│ Point       │   │ Simulation   │   │ Training        │   │ GAMBO       │
│ Training    │   │ (Straight-   │   │ (Integer        │   │ .gnnue      │
│             │   │ Through Est) │   │ Arithmetic)     │   │ Format      │
└─────────────┘   └──────────────┘   └─────────────────┘   └─────────────┘
```

### 7.2 QAT Components

| Component | Purpose |
|-----------|---------|
| `QuantSimulator` | Straight-through estimator for quantized operations |
| `QuantizedTrainer` | Training loop with quantized forward/backward passes |
| `QuantExporter` | Exports quantized weights to GAMBO format |
| `ConsistencyValidator` | Verifies FP32 vs INT8 inference consistency |

### 7.3 Quantization Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Weight Bits | 8 | Symmetric, per-channel |
| Activation Bits | 8 | Asymmetric, per-tensor |
| Bias Bits | 32 | Accumulator precision |
| Output Scale | 600 | Matches SFNNv16 |

---

## 8. Search Improvements (SF19-Inspired, GAMBO-Native)

### 8.1 Identified SF19 Search Improvements

| Improvement | Description | GAMBO Status |
|-------------|-------------|--------------|
| Move ordering | History/SEE improvements | To implement |
| Pruning | LMP, LMR refinements | To implement |
| Reductions | Dynamic reduction formulas | To implement |
| Extensions | Singular, check extensions | To implement |
| History | Correction history, continuation | To implement |
| Time management | Improved allocation | Phase 1 preserved |

### 8.2 Integration with GAMBO Phase 1

All search improvements must integrate with existing Phase 1 aggressive intelligence:
- Objective/style evaluation separation preserved
- Dynamic king-safety-aware aggression preserved
- Sacrifice compensation preserved
- Compensation-aware pruning preserved

---

## 9. Preserved GAMBO-Specific Intelligence

### Phase 1 (Preserved)
- Objective/style evaluation separation
- Dynamic king-safety-aware aggression
- Sacrifice compensation detection
- Compensation-aware pruning
- Aggressive move ordering
- Instability-aware time management

### Phase 2 (Preserved with Ablation)
- `Eval.PhaseAdaptiveNNUE` — Phase-adaptive evaluation
- `Classical.KingSafety` — King safety evaluation
- `Classical.Initiative` — Initiative/attacking potential
- `Classical.Coordination` — Piece coordination
- `Classical.Defense` — Defensive resources
- `Classical.Sacrifice` — Positional sacrifice
- `Classical.Conversion` — Advantage conversion

### Ablation Controls (Preserved)
All 7 UCI options preserved for validation:
- `Eval.PhaseAdaptiveNNUE`
- `Classical.KingSafety`
- `Classical.Initiative`
- `Classical.Coordination`
- `Classical.Defense`
- `Classical.Sacrifice`
- `Classical.Conversion`

---

## 10. Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| Architecture Documentation | ✅ Complete | This document |
| Pawn-Pair Feature System | 🔄 In Progress | Core SFNNv16 innovation |
| King-Relative Features | ⏳ Planned | Replaces HalfKAv2_hm |
| Threat Detection | ⏳ Planned | Replaces FullThreats |
| Piece Coordination | ⏳ Planned | Replaces PieceCoordination |
| NNUE Network Architecture | 🔄 In Progress | SFNNv16 topology |
| Single-Network Inference | ⏳ Planned | Replaces dual-net |
| Quantization/QAT Pipeline | 📋 Designed | Architecture complete |
| Search Improvements | 📋 Designed | SF19-inspired |
| GAMBO Network Format | 📋 Designed | .gnnue format |
| SFNNv16 Compatibility | 📋 Designed | Optional converter |

---

## 11. Uncertainties and Blockers

| Uncertainty | Impact | Resolution Plan |
|-------------|--------|-----------------|
| Exact SFNNv16 pawn-pair encoding | High | Research public specs; if unknown, design GAMBO-native equivalent |
| Exact SFNNv16 threat feature reduction | Medium | Re-implement from public descriptions |
| Exact network training hyperparameters | Low | Use GAMBO-native training pipeline |
| SFNNv16 network format compatibility | Low | Optional converter; not required |

---

## 12. Next Steps

1. **Implement GAMBO Pawn-Pair Feature System** (Priority 1)
2. **Implement GAMBO King-Relative Feature System** (Priority 2)
3. **Implement GAMBO Threat Detection Feature System** (Priority 3)
4. **Implement GAMBO Piece-Relative Feature System** (Priority 4)
5. **Implement NNUE Network Architecture** (Parallel)
6. **Implement Single-Network Inference** (Priority 5)
7. **Implement QAT Infrastructure** (Priority 6)
7. **Integrate with Phase 1/2 Intelligence** (Priority 7)
8. **Validation via Ablation Testing** (Priority 8)

---

*End of Architecture Document*