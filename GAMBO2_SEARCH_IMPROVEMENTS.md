# GAMBO 2.0 — Search Improvements Documentation

**Branch**: `gambo-original-sf19-tech`  
**Status**: Designed / In Progress  
**Date**: 2026-11-02

---

## 1. Overview

This document describes GAMBO-native search improvements inspired by Stockfish 19's advances, implemented independently without copying Stockfish code.

---

## 2. SF19 Search Improvements Identified

Based on Stockfish 19 release notes and public analysis, the following search improvements are relevant:

| Area | SF19 Improvement | GAMBO Implementation |
|------|------------------|---------------------|
| Move Ordering | Improved history/SEE, continuation history | GAMBO-native implementation |
| Pruning | Refined LMP/LMR, futility pruning | GAMBO-native implementation |
| Reductions | Dynamic LMR formulas | GAMBO-native implementation |
| Extensions | Singular, check extensions refined | GAMBO-native implementation |
| History | Correction history, continuation history | GAMBO-native implementation |
| Quiescence | Delta pruning, SEE improvements | GAMBO-native implementation |
| Time Management | Improved allocation | Phase 1 preserved |
| Memory Efficiency | TT clustering, aging | To evaluate |

---

## 2. Move Ordering Improvements

### 2.1 Continuation History (GAMBO-Native)

```cpp
namespace Gambo::Search {

class ContinuationHistory {
    // Continuation history: [piece][to][prev_piece][prev_to]
    // Captures piece-specific follow-up patterns
    
    static constexpr int MAX_PLY = 256;
    static constexpr int HISTORY_MAX = 32768;
    
    // [piece_type][to_square][prev_piece_type][prev_to_square]
    int32_t table[PIECE_TYPE_NB][SQUARE_NB][PIECE_TYPE_NB][SQUARE_NB];
    
    void update(Piece piece, Square to, Piece prev_piece, Square prev_to, int bonus);
    int get(Piece piece, Square to, Piece prev_piece, Square prev_to) const;
    void clear();
    void age();  // Aging for recency
};

}  // namespace Gambo::Search
```

### 2.2 Capture History with SEE Integration

```cpp
namespace Gambo::Search {

class CaptureHistory {
    // [victim_piece][attacker_piece][to_square]
    int32_t table[PIECE_TYPE_NB][PIECE_TYPE_NB][SQUARE_NB];
    
    void update(Piece victim, Piece attacker, Square to, int depth, int see_value);
    int get(Piece victim, Piece attacker, Square to) const;
    void age();
};

}  // namespace Gambo::Search
```

### 2.3 Move Scoring Integration

```cpp
int score_move(const Move& move, const Position& pos, const Search::Stack& ss) {
    int score = 0;
    
    if (move.is_capture()) {
        score += capture_history.get(victim, attacker, to);
        score += see_sign(move) * SEE_WEIGHT;
    } else {
        score += main_history.get(piece, to);
        score += continuation_history.get(piece, to, ss.prev_piece, ss.prev_to);
        score += pawn_history.get(piece, to);
    }
    
    return score;
}
```

---

## 3. Pruning Improvements

### 3.1 Late Move Pruning (LMP) Refinement

```cpp
namespace Gambo::Search {

bool lmp_prune(const Position& pos, const Stack& ss, int depth, int move_count) {
    // SF19-inspired: More nuanced LMP thresholds
    // Based on: depth, move count, position type, static evaluation
    
    if (depth < LMP_DEPTH_LIMIT) return false;
    if (move_count > lmp_move_limit[depth]) return true;
    
    // SF19: Additional checks for quiet positions
    if (ss.static_eval < beta - lmp_margin[depth]) return true;
    
    return false;
}

}  // namespace Gambo::Search
```

### 3.2 Late Move Reductions (LMR) Refinement

```cpp
namespace Gambo::Search {

int lmr_reduction(const Position& pos, const Stack& ss, int depth, int move_count, bool cut_node) {
    // SF19-inspired: More nuanced reduction formula
    // Factors: depth, move_count, cut_node, position type, static eval
    
    int reduction = base_lmr[depth][move_count];
    
    if (cut_node) reduction += LMR_CUT_BONUS;
    if (pos.is_endgame()) reduction -= LMR_EG_REDUCTION;
    if (ss.static_eval > beta - LMR_MARGIN) reduction += LMR_STATIC_BONUS;
    
    return std::clamp(reduction, 1, depth - 1);
}

}  // namespace Gambo::Search
```

---

## 3. Reduction Improvements

### 3.1 Dynamic LMR Formula

```cpp
namespace Gambo::Search {

int compute_reduction(int depth, int move_count, bool cut_node, 
                      bool in_check, int static_eval, int beta) {
    // SF19-inspired dynamic reduction
    // More granular than simple depth/move_count table
    
    double r = log(depth) * log(move_count) * BASE_REDUCTION;
    
    if (cut_node) r *= 1.15;
    if (in_check) r *= 0.7;
    if (static_eval > beta - 100) r *= 1.1;
    
    return std::max(1, int(r));
}

}  // namespace Gambo::Search
```

---

## 4. Extension Improvements

### 4.1 Singular Extension Refinement

```cpp
namespace Gambo::Search {

bool is_singular_extension(const Position& pos, Move move, int depth, int beta) {
    // SF19-inspired: More precise singular detection
    // Search with reduced depth, verify move is singular
    
    if (depth < SINGULAR_DEPTH) return false;
    if (!move.is_capture() && !move.is_promotion()) return false;
    
    int r = 3 + depth / 4;
    int saved_alpha = alpha;
    alpha = beta - 1;
    
    // Search with exclusion
    Value v = search(pos, ss, alpha, beta, r, false, true);
    alpha = saved_alpha;
    
    return v < beta - SINGULAR_MARGIN;
}

}  // namespace Gambo::Search
```

### 4.2 Check Extension Refinement

```cpp
namespace Gambo::Search {

int check_extension(const Position& pos, const Stack& ss, int depth) {
    // SF19: More selective check extensions
    // Only extend if check is non-trivial
    
    if (depth < CHECK_EXT_DEPTH) return 0;
    if (pos.checkers_count() > 1) return 1;  // Double check
    
    // Single check: extend only if forcing
    if (ss.move_count < CHECK_EXT_MOVE_LIMIT) return 1;
    
    return 0;
}

}  // namespace Gambo::Search
```

---

## 4. History Heuristic Improvements

### 4.1 Correction History (GAMBO-Native)

```cpp
namespace Gambo::Search {

class CorrectionHistory {
    // [piece][to][depth] - how much was this move underestimated?
    
    static constexpr int MAX_DEPTH = 64;
    int16_t table[PIECE_TYPE_NB][SQUARE_NB][MAX_DEPTH];
    
    void update(Piece piece, Square to, int depth, int actual_score, int predicted_score) {
        int correction = actual_score - predicted_score;
        table[piece][to][std::min(depth, MAX_DEPTH-1)] += correction / 4;
    }
    
    int get_bonus(Piece piece, Square to, int depth) const {
        return table[piece][to][std::min(depth, MAX_DEPTH-1)];
    }
    
    void clear() { memset(table, 0, sizeof(table)); }
    void age() { /* divide by 2 */ }
};

}  // namespace Gambo::Search
```

### 4.2 Combined History Score

```cpp
int history_score(const Move& move, const Position& pos, const Stack& ss, int depth) {
    int score = 0;
    
    score += main_history.get(move.piece(), move.to());
    score += continuation_history.get(move.piece(), move.to(), ss.prev_piece, ss.prev_to());
    score += correction_history.get_bonus(move.piece(), move.to(), depth);
    
    if (move.is_capture()) {
        score += capture_history.get(victim, attacker, move.to());
    } else {
        score += pawn_history.get(move.piece(), move.to());
    }
    
    return score;
}
```

---

## 5. Quiescence Search Improvements

### 5.1 Delta Pruning with SEE

```cpp
namespace Gambo::Search {

bool delta_prune(const Position& pos, int alpha, int beta, int depth) {
    // SF19-inspired: SEE-based delta pruning
    // Only prune if even best capture won't raise score enough
    
    if (depth <= 0) {
        int best_see = max_see_capture(pos);
        if (best_see < beta - DELTA_MARGIN) return true;
    }
    return false;
}

int max_see_capture(const Position& pos) {
    int best = 0;
    for (Move m : pos.legal_captures()) {
        best = std::max(best, see(pos, m));
    }
    return best;
}

}  // namespace Gambo::Search
```

### 5.2 Futility Pruning in QSearch

```cpp
namespace Gambo::Search {

bool futility_qsearch(const Position& pos, int alpha, int beta) {
    // SF19: Extended futility to qsearch
    
    if (!pos.checkers() && pos.static_eval() > beta - FUTILITY_QS_MARGIN) {
        // Try standing pat
        return true;
    }
    return false;
}

}  // namespace Gambo::Search
```

---

## 6. Transposition Table Improvements

### 6.1 TT Clustering (GAMBO-Native)

```cpp
namespace Gambo::Search {

class ClusteredTT {
    // Group related positions for better cache locality
    
    struct Cluster {
        uint64_t key_prefix;  // High bits of Zobrist key
        TTEntry entries[CLUSTER_SIZE];
        uint8_t ages[CLUSTER_SIZE];
    };
    
    std::vector<Cluster> clusters;
    
    TTEntry* probe(uint64_t key);
    void store(uint64_t key, const TTEntry& entry);
    void age_clusters();
};

}  // namespace Gambo::Search
```

---

## 7. Integration with GAMBO Phase 1

### 7.1 Preserved Phase 1 Features

All Phase 1 aggressive intelligence must remain functional:

```cpp
// These must continue to work with new search improvements:
- evaluate_with_style() / evaluate() separation
- dynamic_attack_bias() with king safety
- sacrifice_compensation() detection
- compensation_aware_pruning()
- aggressive_move_ordering()
- instability_aware_time_management()
```

### 7.2 Integration Points

| Search Component | Phase 1 Integration |
|-----------------|---------------------|
| Move Ordering | Aggressive bonuses for king-zone moves |
| Pruning | Compensation-aware pruning preserved |
| Time Management | Instability-aware allocation preserved |
| Extensions | Aggressive extensions for king attacks |

---

## 8. Implementation Status

| Component | Status | File |
|-----------|--------|------|
| Continuation History | 📋 Designed | `src/gambo/search/continuation_history.h` |
| Capture History | 📋 Designed | `src/gambo/search/capture_history.h` |
| LMP Refinement | 📋 Designed | `src/gambo/search/pruning.h` |
| LMR Refinement | 📋 Designed | `src/gambo/search/reductions.h` |
| Singular Extensions | 📋 Designed | `src/gambo/search/extensions.h` |
| Correction History | 📋 Designed | `src/gambo/search/correction_history.h` |
| Quiescence Improvements | 📋 Designed | `src/gambo/search/quiescence.h` |
| TT Clustering | 📋 Designed | `src/gambo/search/tt_clustered.h` |
| Phase 1 Integration | ✅ Preserved | Existing code |

---

*End of Search Improvements Document*