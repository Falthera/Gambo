# GAMBO 2.0 — NNUE Implementation Documentation

**Branch**: `gambo-original-sf19-tech`  
**Status**: In Progress  
**Date**: 2026-11-02

---

## 1. Overview

This document describes the GAMBO-native NNUE implementation that reproduces SFNNv16-equivalent capabilities through independently authored code.

---

## 2. Feature Extraction System

### 2.1 Feature Set Overview

| Feature Set | Class | Purpose | Dimensions | Max Active |
|-------------|-------|---------|------------|------------|
| PawnPair | `Gambo::NNUE::Features::PawnPair` | Pawn structure relationships | ~8,192 | 128 |
| KingRelative | `Gambo::NNUE::Features::KingRelative` | King-relative piece positions | ~40,960 | 64 |
| ThreatDetection | `Gambo::NNUE::Features::ThreatDetection` | Attack/defense relationships | ~60,000 | 128 |
| PieceRelative | `Gambo::NNUE::Features::PieceRelative` | Piece-piece coordination | ~8,192 | 128 |

### 2.2 Common Feature Interface

```cpp
namespace Gambo::NNUE::Features {

template<typename FeatureSet>
concept FeatureExtractor = requires {
    FeatureSet::Dimensions;
    FeatureSet::MaxActiveDimensions;
    FeatureSet::HashValue;
    { FeatureSet::make_index(...) } -> std::convertible_to<IndexType>;
    { FeatureSet::append_active_indices(Color, const Position&, IndexList&) };
    { FeatureSet::append_changed_indices(Color, Square, DiffType&, IndexList&, IndexList&) };
    { FeatureSet::requires_refresh(DiffType, Color) } -> std::convertible_to<bool>;
};

}  // namespace Gambo::NNUE::Features
```

---

## 3. Pawn-Pair Feature System (Core SFNNv16 Innovation)

### 3.1 Pawn-Pair Encoding

```cpp
namespace Gambo::NNUE::Features {

enum class PawnPairType : uint8_t {
    CONNECTED   = 0,  // Adjacent files, same/adjacent rank
    ISOLATED    = 1,  // No friendly pawn on adjacent files
    DOUBLED     = 2,  // Two pawns on same file
    BACKWARD    = 3,  // Behind adjacent friendly pawns
    PASSED      = 4,  // No enemy pawns blocking promotion
    CANDIDATE   = 5,  // Potential passed pawn
    PROTECTED   = 6,  // Defended by friendly pawn
    PHALANX     = 7,  // Same rank, adjacent files
    PAIR_NB     = 8
};

class PawnPair {
public:
    static constexpr const char* Name = "PawnPair";
    static constexpr uint32_t HashValue = 0x1F234CB8u;
    static constexpr IndexType Dimensions = 8192;
    static constexpr IndexType MaxActiveDimensions = 128;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType = DirtyPiece;

    static IndexType make_index(Color perspective, Square king_sq,
                                Square pawn1, Square pawn2, PawnPairType type);
    
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);
    static void append_changed_indices(Color perspective, Square ksq,
                                       const DiffType& diff, IndexList& removed, IndexList& added);
    static bool requires_refresh(const DiffType& diff, Color perspective);
};

}  // namespace Gambo::NNUE::Features
```

### 3.3 Pawn-Pair Type Detection

```cpp
PawnPairType detect_pawn_pair_type(Color perspective, 
                                   const Position& pos,
                                   Square pawn1, Square pawn2) {
    // Connected: adjacent files, same or adjacent rank
    // Doubled: same file
    // Isolated: no friendly pawn on adjacent files
    // Backward: behind adjacent friendly pawns, cannot advance safely
    // Passed: no enemy pawns on same/adjacent files ahead
    // Candidate: potential passed pawn (blocked by 1 enemy pawn)
    // Protected: defended by friendly pawn
    // Phalanx: same rank, adjacent files
}
```

### 2.4 Incremental Updates

When a pawn moves:
1. Identify all pawn pairs involving the moved pawn
2. Remove old indices from accumulator
3. Compute new pawn-pair types with new position
4. Add new indices to accumulator
5. Handle special cases: captures, promotions, en passant

---

## 3. King-Relative Feature System (Replaces HalfKAv2_hm)

```cpp
namespace Gambo::NNUE::Features {

class KingRelative {
public:
    static constexpr const char* Name = "KingRelative";
    static constexpr uint32_t HashValue = 0x2F234CB8u;
    static constexpr IndexType Dimensions = 40960;  // 64 king squares × 640 piece positions
    static constexpr IndexType MaxActiveDimensions = 64;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType = DirtyPiece;

    static IndexType make_index(Color perspective, Square king_sq, Square piece_sq, Piece piece);
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);
    static void append_changed_indices(Color perspective, Square ksq,
                                       const DiffType& diff, IndexList& removed, IndexList& added);
    static bool requires_refresh(const DiffType& diff, Color perspective);
};

}  // namespace Gambo::NNUE::Features
```

### King Bucket Mapping (Same as HalfKAv2_hm)

```cpp
static constexpr IndexType KingBuckets[SQUARE_NB] = {
    B(28), B(29), B(30), B(31), B(31), B(30), B(29), B(28),
    B(24), B(25), B(26), B(27), B(27), B(26), B(25), B(24),
    B(20), B(21), B(22), B(23), B(23), B(22), B(21), B(20),
    B(16), B(17), B(18), B(19), B(19), B(18), B(17), B(16),
    B(12), B(13), B(14), B(15), B(15), B(14), B(13), B(12),
    B( 8), B( 9), B(10), B(11), B(11), B(10), B( 9), B( 8),
    B( 4), B( 5), B( 6), B( 7), B( 7), B( 6), B( 5), B( 4),
    B( 0), B( 1), B( 2), B( 3), B( 3), B( 2), B( 1), B( 0),
};
```

---

## 3. Threat Detection Feature System (Replaces FullThreats)

```cpp
namespace Gambo::NNUE::Features {

class ThreatDetection {
public:
    static constexpr const char* Name = "ThreatDetection";
    static constexpr uint32_t HashValue = 0x3F234CB8u;
    static constexpr IndexType Dimensions = 60720;
    static constexpr IndexType MaxActiveDimensions = 128;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType = DirtyThreats;

    static IndexType make_index(Color perspective, Piece attacker, Square from, 
                                Square to, Piece target, Square king_sq);
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);
    static void append_changed_indices(Color perspective, Square ksq,
                                       const DiffType& diff, IndexList& removed, 
                                       IndexList& added, FusedUpdateData* fd = nullptr,
                                       bool first = false);
    static bool requires_refresh(const DiffType& diff, Color perspective);
};

}  // namespace Gambo::NNUE::Features
```

---

## 4. Piece-Relative Feature System (Replaces PieceCoordination)

```cpp
namespace Gambo::NNUE::Features {

class PieceRelative {
public:
    enum CoordType : uint8_t {
        COORD_NONE = 0,
        COORD_BATTERY_ROOK = 1,
        COORD_BATTERY_BISHOP = 2,
        COORD_KNIGHT_OUTPOST = 3,
        COORD_PAWN_CHAIN = 4,
        COORD_ROOK_OPEN_FILE = 5,
        COORD_BISHOP_PAIR = 6,
        COORD_KNIGHT_PAIR = 7,
        COORD_QUEEN_KNIGHT = 8,
        COORD_ROOK_ROOK = 9,
        COORD_NB = 10
    };

    static constexpr const char* Name = "PieceRelative";
    static constexpr uint32_t HashValue = 0x4F234CB8u;
    static constexpr IndexType Dimensions = 8192;
    static constexpr IndexType MaxActiveDimensions = 128;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType = DirtyPiece;

    static IndexType make_index(CoordType type, Square sq1, Square sq2, Color perspective);
    static void append_active_indices(Color perspective, const Position& pos, IndexList& active);
    static void append_changed_indices(Color perspective, Square ksq,
                                       const DiffType& diff, IndexList& removed, IndexList& added);
    static bool requires_refresh(const DiffType& diff, Color perspective);
};

}  // namespace Gambo::NNUE::Features
```

---

## 4. NNUE Network Architecture (SFNNv16 Topology)

### 4.1 Layer Configuration

```cpp
namespace Gambo::NNUE {

constexpr IndexType L1 = 1024;        // Transformed feature dimensions
constexpr int L2 = 31;                // FC0 output per half (dual ReLU)
constexpr int L3 = 32;                // FC1 output
constexpr IndexType PSQTBuckets = 8;  // Must be multiple of 8
constexpr IndexType LayerStacks = 8;  // Accumulator parallelism

// Layer dimensions
constexpr IndexType TransformedFeatureDimensions = L1;
constexpr int FC0_OUTPUTS = 31;       // Per half (SqrClippedReLU + ClippedReLU)
constexpr int FC1_OUTPUTS = L3;       // 32
constexpr int FC2_OUTPUTS = 1;        // Single output

}  // namespace Gambo::NNUE
```

### 4.2 Network Architecture Definition

```cpp
namespace Gambo::NNUE {

struct NetworkArchitecture {
    static constexpr IndexType TransformedFeatureDimensions = L1;
    static constexpr int FC0_OUTPUTS = 31;
    static constexpr int FC1_OUTPUTS = 32;
    
    // Layers
    Layers::AffineTransformSparseInput<TransformedFeatureDimensions, 63> fc_0;
    Layers::SqrClippedReLU<63> ac_sqr_0;
    Layers::ClippedReLU<63> ac_0;
    Layers::AffineTransform<62, 32> fc_1;
    Layers::ClippedReLU<32> ac_1;
    Layers::AffineTransform<32, 1> fc_2;
    
    // Hash for architecture verification
    static constexpr std::uint32_t get_hash_value() {
        std::uint32_t hash = 0xEC42E90Du;
        hash ^= TransformedFeatureDimensions * 2;
        // ... layer hashes
        return hash;  // Expected: 0xEC42E90D
    }
    
    std::int32_t propagate(const TransformedFeatureType* features) const;
    bool read_parameters(std::istream&);
    bool write_parameters(std::ostream&) const;
    std::size_t get_content_hash() const;
};

}  // namespace Gambo::NNUE
```

---

## 5. Network Inference

### 5.1 Quantization Parameters

```cpp
namespace Gambo::NNUE {

constexpr int WeightScaleBits = 6;        // 64 weight levels
constexpr int ActivationScaleBits = 8;    // 256 activation levels
constexpr int OutputScale = 600;          // Output scaling
constexpr int OutputScaleBits = 14;       // Output quantization

// Forward propagation with quantization
std::int32_t propagate(const TransformedFeatureType* features) {
    // FC0: sparse input → dense 63
    // SqrClippedReLU + ClippedReLU → 62 values
    // FC1: 62 → 32
    // ClippedReLU → 32
    // FC2: 32 → 1
    // Output scaling: fc_0_out[FC0_OUTPUTS] * 600*OutputScale / (127 * (1<<WeightScaleBits))
}

}  // namespace Gambo::NNUE
```

---

## 6. GAMBO-Native Network Format (.gnnue)

### 6.1 File Structure

```cpp
struct GamboNetworkHeader {
    char magic[12] = "GAMBO_NNUE\0";
    uint32_t version = 1;
    uint32_t arch_hash = 0xEC42E90D;
    uint32_t feature_hashes[4];  // PawnPair, KingRelative, Threat, PieceRelative
    uint32_t metadata_size;
    char metadata[256];  // JSON: {description, author, date, git_sha}
};

struct GamboNetworkFile {
    GamboNetworkHeader header;
    // Feature weights: [Dimensions] per feature set
    std::vector<WeightType> pawn_pair_weights;
    std::vector<WeightType> king_relative_weights;
    std::vector<WeightType> threat_weights;
    std::vector<WeightType> piece_relative_weights;
    
    // Layer weights
    std::vector<WeightType> fc0_weights;   // [L1 x 63]
    std::vector<WeightType> fc0_bias;      // [63]
    std::vector<WeightType> fc1_weights;   // [62 x 32]
    std::vector<WeightType> fc1_bias;      // [32]
    std::vector<WeightType> fc2_weights;   // [32 x 1]
    std::vector<WeightType> fc2_bias;      // [1]
    
    // Quantization params
    uint8_t weight_scale_bits = 6;
    uint8_t activation_scale_bits = 8;
    uint16_t output_scale = 600;
    uint8_t output_scale_bits = 14;
    
    // SHA256 checksum
    std::array<uint8_t, 32> checksum;
};
```

---

## 7. Quantization-Aware Training (QAT) Pipeline

### 7.1 QAT Architecture

```cpp
namespace Gambo::NNUE::Training {

class QuantSimulator {
public:
    // Straight-through estimator for quantized operations
    template<typename T>
    static T quantize_forward(T x, int bits, float scale);
    
    template<typename T>
    static T quantize_backward(T grad, int bits, float scale);
};

class QuantizedTrainer {
public:
    void train_epoch(Dataset& dataset, NetworkArchitecture& network);
    void validate_fp32_vs_int8(const NetworkArchitecture& fp32_net,
                                const NetworkArchitecture& int8_net);
};

class QuantExporter {
public:
    bool export_network(const NetworkArchitecture& network, 
                        const std::string& filepath);
};

class ConsistencyValidator {
public:
    bool validate_consistency(const NetworkArchitecture& fp32,
                              const NetworkArchitecture& int8,
                              float tolerance = 1e-3);
};

}  // namespace Gambo::NNUE::Training
```

---

## 8. Implementation Status

| Component | File | Status |
|-----------|------|--------|
| PawnPair Features | `src/gambo/nnue/features/pawn_pair.h/.cpp` | 🔄 In Progress |
| KingRelative Features | `src/gambo/nnue/features/king_relative.h/.cpp` | ⏳ Planned |
| ThreatDetection Features | `src/gambo/nnue/features/threat_detection.h/.cpp` | ⏳ Planned |
| PieceRelative Features | `src/gambo/nnue/features/piece_relative.h/.cpp` | ⏳ Planned |
| Network Architecture | `src/gambo/nnue/architecture.h` | 🔄 In Progress |
| Network Inference | `src/gambo/nnue/network.h/.cpp` | ⏳ Planned |
| Accumulator | `src/gambo/nnue/accumulator.h/.cpp` | ⏳ Planned |
| Network Format | `src/gambo/nnue/gambo_network.h/.cpp` | ⏳ Planned |
| QAT Pipeline | `src/gambo/nnue/training/` | 📋 Designed |
| Feature Extractor | `src/gambo/nnue/feature_extractor.h/.cpp` | ⏳ Planned |

---

*End of NNUE Implementation Document*