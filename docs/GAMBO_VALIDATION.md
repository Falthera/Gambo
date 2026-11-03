# GAMBO Validation Framework

## Required Correctness Checks

### 1. Move Generation & Perft
```bash
# Standard perft tests
./stockfish
> position startpos
> go perft 6
> position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
> go perft 5
```
**Expected**: Matches known perft values exactly.

### 2. Make/Unmake Consistency
```bash
# Run with debug + sanitize
make debug=yes sanitize=address,undefined
./stockfish bench 16 1 8 default depth
```
**Checks**: No ASAN/UBSAN errors, move list integrity, position key consistency.

### 3. Castling / En Passant / Promotions
```bash
# Test suite with special moves
./stockfish
> position fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1
> go depth 10
> position fen 8/8/8/8/8/5Pp1/8/7K w - f6 0 1
> go depth 10
> position fen 8/P7/8/8/8/8/8/7k w - - 0 1
> go depth 10
```
**Checks**: Correct move generation, legality, evaluation.

### 4. Threefold Repetition / 50-Move Rule
```bash
./stockfish
> position fen 8/8/8/8/8/8/8/8 w - - 0 1
> go depth 20
```
**Checks**: Correct draw detection, no false positives.

### 5. Checkmate / Stalemate
```bash
# Fool's mate
> position fen rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1
> go depth 1
# Stalemate
> position fen 8/8/8/8/8/8/7P/7K w - - 0 1
> go depth 10
```

### 6. TT Bounds & Mate Scores
```bash
# Verify TT stores/retrieves correctly
./stockfish bench 16 1 8 default depth
# Check hashfull output, no crashes
```

### 7. Thread Safety
```bash
# Multi-threaded stress test
./stockfish
> setoption name Threads value 8
> go depth 20
```
**Checks**: No data races (TSAN), consistent results across thread counts.

### 8. NNUE Accumulator Consistency
```bash
# Compare incremental vs full refresh
./stockfish
> setoption name GAMBO.IncrementalNNUE value true
> go depth 20
> setoption name GAMBO.IncrementalNNUE value false
> go depth 20
```
**Checks**: Identical evaluation scores.

### 9. Incremental Cache Consistency
```bash
# For INFU/NFAR: compare cached vs full recalc
./stockfish
> setoption name GAMBO.FastAccumulatorRefresh value true
> go depth 20
> setoption name GAMBO.FastAccumulatorRefresh value false
> go depth 20
```

---

## Required Build Checks

### Configurations
| Config | Command | Purpose |
|--------|---------|---------|
| Release | `make profile-build ARCH=x86-64-bmi2` | Production build |
| Debug | `make debug=yes` | Debugging |
| Sanitize | `make debug=yes sanitize=address,undefined` | Bug detection |
| PGO Train | `make profile-build && ./stockfish bench 16 1 8` | Profile generation |
| PGO Use | `make pgo-build` | Profile-guided build |

### CI Pipeline
```yaml
# .github/workflows/validation.yml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - make debug=yes sanitize=address,undefined
      - ./stockfish bench 16 1 8 default depth
  build-windows:
    runs-on: windows-latest
    steps:
      - make profile-build ARCH=x86-64-bmi2 COMP=mingw
      - ./stockfish bench 16 1 8 default depth
```

### CPU Targets
| Target | Flags | Notes |
|--------|-------|-------|
| x86-64-bmi2 | `-march=x86-64-bmi2 -mbmi2 -DUSE_BMI2` | Default |
| x86-64-avx2 | `-march=x86-64-avx2 -DUSE_AVX2` | AVX2 |
| x86-64-avx512 | `-march=x86-64-avx512 -DUSE_AVX512` | AVX-512 |
| ARM NEON | `-march=armv8-a+simd` | ARM |

---

## External Strength-Testing Infrastructure

### Test Framework Structure
```
testing/
├── config/
│   ├── baseline.yaml          # Stockfish baseline
│   ├── gambo_tier1.yaml       # Tier 1 only
│   ├── gambo_tier2.yaml       # Tier 2 only
│   ├── gambo_tier12.yaml      # Tier 1+2
│   ├── gambo_tier123.yaml     # Tier 1+2+3
│   └── gambo_experimental.yaml # Tier 4 modes
├── scripts/
│   ├── run_match.py           # Cute Chess CLI wrapper
│   ├── run_bench.py           # Benchmark runner
│   ├── analyze_pgn.py         # PGN analysis
│   └── sprt.py                # SPRT implementation
├── openings/
│   ├── 8moves_v3.pgn          # Standard opening suite
│   ├── noob_3moves.pgn        # Quick opening suite
│   └── uho_xxx.pgn            # Balanced opening books
└── results/
    └── (auto-generated)
```

### Comparison Matrix
| Match | Engine A | Engine B | Purpose |
|-------|----------|----------|---------|
| 1 | Stockfish baseline | GAMBO Tier 1 | Tier 1 impact |
| 2 | Stockfish baseline | GAMBO Tier 2 | Tier 2 impact |
| 3 | Stockfish baseline | GAMBO Tier 1+2 | Combined 1+2 |
| 4 | GAMBO Tier 1+2 | GAMBO Tier 1+2+3 | Tier 3 impact |
| 5 | GAMBO Tier 1+2+3 | GAMBO Tier 4 | Experimental impact |

### Match Configuration
```python
# config/match_template.yaml
tc: "10+0.1"           # Time control
rounds: 200            # Games per pairing
openings: "8moves_v3.pgn"
concurrency: 4         # Parallel games
sprt:
  elo0: 0
  elo1: 10
  alpha: 0.05
  beta: 0.05
```

### Running Tests
```bash
# Quick validation (local)
cd testing
python3 scripts/run_bench.py --config config/gambo_tier12.yaml

# Full tournament (VPS)
cd testing
python3 scripts/run_match.py --config config/tournament.yaml --output results/

# SPRT test
cd testing
python3 scripts/sprt.py --engine-a gambo_tier12 --engine-b gambo_tier123 --tc 10+0.1
```

---

## Validation Checklist

### Pre-Commit
- [ ] `make debug=yes sanitize=address,undefined` passes
- [ ] `./stockfish bench 16 1 8 default depth` completes
- [ ] `./stockfish` → `go perft 6` matches known values
- [ ] No compiler warnings (`-Wall -Wextra -pedantic`)

### Pre-Release
- [ ] All CI builds pass (Linux, Windows, macOS)
- [ ] All CPU targets compile (x86-64-bmi2, avx2, arm)
- [ ] PGO build completes and runs
- [ ] Multi-threaded bench (1, 2, 4, 8 threads) consistent
- [ ] NNUE accumulator consistency verified
- [ ] All UCI options registered and documented

### Regression Tests
```bash
# Automated test suite
./run_regression_tests.sh

# Test cases:
# 1. Perft (depth 1-7)
# 2. Move legality (all special moves)
# 3. Evaluation symmetry (pos vs flipped)
# 4. TT consistency (probe/write)
# 5. NNUE accumulator (inc vs full)
# 6. Search determinism (same pos, same depth, same result)
# 7. UCI protocol compliance
```

---

## Known Issues & Limitations

1. **GAMBO NNUE**: No trained network available; falls back to Stockfish NNUE
2. **Experimental features**: Default OFF; not validated for strength
3. **PGO profiles**: Machine-specific; regenerate per CPU
4. **AVX-512**: Requires `-DUSE_AVX512` and compatible CPU
5. **ARM NEON**: SIMD paths not fully optimized
6. **Large pages**: Requires admin/root on Windows/Linux

---

## Reporting Issues

Template:
```
**Issue**: [Description]
**Config**: [OS, CPU, compiler, build flags]
**Steps**: [Minimal reproduction]
**Expected**: [Behavior]
**Actual**: [Behavior]
**Logs**: [Sanitizer output, bench output]
```