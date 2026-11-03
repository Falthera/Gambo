# GAMBO — Final Search, Evaluation & Performance Optimisation Implementation

## 1. Your role and mission

Act as a principal chess-engine architect, senior C++ performance engineer, NNUE specialist and search-algorithm researcher.

Your task is to take the **existing GAMBO chess-engine repository** and implement the final optimisation and feature roadmap below.

GAMBO is an aggressive, high-performance UCI chess engine derived from Stockfish 18. Its objective is to become an exceptionally strong engine while maintaining its own identity, architecture and research direction.

This is an implementation task, not merely a proposal-writing task. You must inspect the repository, design the integration, modify the code, implement the features, build the project and document the results.

The goal is not to maximise the number of features in the source code. The goal is to improve GAMBO's actual playing strength, search efficiency, evaluation quality, reliability and performance.

**Do not stop after producing an audit or architecture document. Implement the work.**

---

# 2. Critical project rules

### Preserve existing GAMBO functionality

The existing GAMBO implementation contains previous development phases that must be preserved.

Phase 1 includes:
- Objective evaluation and style-bias separation.
- King-safety-aware dynamic aggression.
- Sacrifice compensation detection.
- Compensation-aware pruning.
- Aggressive move ordering.
- Instability-aware time management.

Phase 2 includes:
- Phase-adaptive NNUE blending.
- King Safety.
- Initiative.
- Piece Coordination.
- Defensive Resources.
- Positional Sacrifice.
- Advantage Conversion.
- Evaluation tracing.
- Runtime evaluation ablation options.

Preserve existing functionality, UCI compatibility, evaluation behaviour and existing feature controls.

Do not casually rewrite or remove these systems. Improve them only when there is a clear technical reason and a way to verify the change.

### Original implementation

Write new algorithms and modules independently.

- Do not copy Stockfish 19 source code.
- Do not copy source code from Leela Chess Zero, Berserk, Komodo or other engines.
- Do not transplant external engine implementations and simply rename their symbols.
- You may study publicly documented algorithms, papers and technical concepts.
- Implement the underlying ideas independently and document your design decisions.
- Preserve the existing upstream attribution, copyright notices and licensing obligations of inherited code. Do not remove or misrepresent its provenance.

### No uncontrolled experimentation

Do not introduce arbitrary evaluation bonuses, pruning constants or search reductions without a technical rationale.

Every heuristic must have:
- A clearly defined purpose.
- A documented input and output.
- Explicit activation conditions.
- Safe limits and fallback behaviour.
- Appropriate tests.
- An ablation mechanism wherever practical.

Do not claim an Elo improvement without actual tournament evidence.

### Hardware restrictions

The development machine is not a dedicated engine-testing server.

**You MUST NOT run:**
- Large Cute Chess tournaments.
- Hundreds or thousands of self-play games.
- Large-scale engine-versus-engine testing.
- NNUE training or quantisation-aware training.
- Extensive parameter tuning.
- Resource-intensive performance benchmarks.
- Long-running search experiments designed to estimate Elo.

Do not run these workloads on the user's laptop, even if you think the machine can handle them.

You may run reasonable compilation, static checks, unit tests, limited perft tests, protocol smoke tests and small deterministic correctness checks.

Create reproducible scripts and instructions for full-scale testing on a separate VPS or dedicated test machine. Do not assume that a VPS has a GPU.

---

# 3. Mandatory repository audit

Before modifying anything, inspect the entire repository.

Identify:
- The actual current branch and commit.
- Uncommitted modifications.
- Existing search architecture.
- Existing evaluation architecture.
- NNUE implementation and accumulator handling.
- Move ordering and history heuristics.
- Transposition table implementation.
- Quiescence search.
- Pruning, reductions and extensions.
- Threading and shared-memory structures.
- UCI options and configuration.
- Build system and supported platforms.
- Existing tests and CI workflows.

Create a feature audit containing all 40 requested items.

For every item, classify it as one of:

1. **Already implemented:** GAMBO already has it in a sufficiently equivalent form.
2. **Partially implemented:** Some functionality exists but needs improvement.
3. **Missing:** A useful implementation needs to be added.
4. **Experimental:** It is research-oriented and should be isolated from the main search.
5. **Unsuitable or redundant:** It duplicates another feature, conflicts with the architecture or has no convincing technical justification.

Do not assume a feature is missing just because it has a different name.

Stockfish-derived engines already contain many sophisticated search and NNUE mechanisms. Inspect the actual code before implementing another version of the same algorithm.

Create:

`docs/GAMBO_FINAL_OPTIMIZATION_AUDIT.md`

The audit is not permission to stop. After completing it, proceed with implementation.

Create a new Git branch named:

`gambo-final-optimizations`

If that branch already exists, choose a new non-destructive branch name. Do not reset, discard or overwrite existing work.

---

# 4. Tier 1 — Advanced search improvements

These features target search efficiency, move ordering, tactical accuracy and reduction safety.

Implement and integrate the following.

### 1. TMIHH — Threat-Map Indexed History Heuristic

Develop a history heuristic that considers the tactical threat context of a position.

Requirements:
- Incorporate relevant attacking and threatened-piece information.
- Distinguish materially different threat situations.
- Avoid excessive history-table memory usage.
- Use bounded, numerically stable updates.
- Integrate with existing move ordering.
- Ensure history updates remain correctly associated with their search context.

### 2. CICH — Colour-Indexed Continuation History

Develop continuation history with appropriate colour-sensitive indexing.

Requirements:
- Account for the side to move and relevant preceding-move context.
- Avoid unnecessary duplication of existing history information.
- Keep storage efficient.
- Integrate with the existing search history framework.

### 3. MPCH — Multi-Ply Continuation History

Extend continuation history to account for multiple preceding plies.

Requirements:
- Investigate useful two-ply and deeper continuation contexts.
- Control memory requirements.
- Prevent sparse contexts from generating misleading bonuses.
- Use appropriate decay and bounded updates.
- Integrate with CICH without double-counting identical information.

### 4. MTCH — Move-Type Conditioned History

Develop history scoring that distinguishes relevant move categories.

Investigate separate treatment of:
- Quiet moves.
- Captures.
- Promotions.
- Checking moves.
- Other meaningful move categories.

Ensure the system does not fragment history into so many categories that useful statistical information becomes unreliable.

### 5. CHQO — Capture-History Quiescence Ordering

Improve capture ordering in quiescence search.

Requirements:
- Incorporate capture-history information.
- Retain SEE information.
- Prioritise tactically promising captures.
- Avoid unnecessary expensive calculations.
- Never incorrectly discard a potentially important tactical capture solely because its history score is low.

### 6. SEE-GCP — SEE-Guided Capture Partitioning

Improve capture ordering and classification using static exchange evaluation.

Investigate:
- Winning captures.
- Approximately equal exchanges.
- Losing captures.
- Special cases involving promotions and king safety.

Use SEE to improve move ordering and selective search decisions without introducing unsafe tactical omissions.

### 7. HGQMP — History-Guided Quiet Move Pruning

Develop quiet-move pruning that incorporates history information.

Requirements:
- Use depth-aware and context-aware conditions.
- Respect PV status, check status and tactical instability.
- Account for TT moves, killer moves and other relevant ordering signals.
- Avoid pruning potentially critical defensive moves.
- Include conservative safety thresholds.

### 8. URD — Upcoming Repetition Detection

Improve detection and handling of moves that may create repetition.

Requirements:
- Correctly handle repetition history.
- Preserve official chess draw rules.
- Respect make/unmake and variation-specific state.
- Avoid false repetition detection.
- Ensure en passant, pins and castling-related edge cases remain correct.

Do not treat an anticipated repetition as an unconditional draw before the required position-history conditions are satisfied.

### 9. EGC — Extension Growth Control

Introduce explicit controls for cumulative search extensions.

Requirements:
- Monitor extension accumulation along a search path.
- Prevent uncontrolled depth growth.
- Account for checks, singular extensions and other extension sources.
- Preserve tactically important extensions.
- Make extension limits configurable for experiments.

### 10. SESR — Singular-Extension Search Refinement

Improve singular-extension logic.

Requirements:
- Review exclusion-search correctness.
- Improve candidate move verification.
- Account for TT information and search stability.
- Prevent unnecessary extensions.
- Avoid interactions that cause runaway extension growth.

### 11. LMRC — LMR Context Calibration

Improve late-move reduction decisions using richer search context.

Investigate:
- Move history.
- Continuation history.
- Tactical threat context.
- PV status.
- TT information.
- Move type.
- Position criticality.
- Search instability.

The goal is to reduce less promising moves efficiently without reducing critical moves too aggressively.

### 12. QTC — QSearch Transposition Correction

Investigate and implement improved transposition-table handling and evaluation correction in quiescence search.

Requirements:
- Define precisely what information is corrected and why.
- Preserve TT bound semantics.
- Respect depth and search-context differences.
- Avoid treating incomparable qsearch results as equivalent.
- Integrate with existing correction-history and TT mechanisms instead of duplicating them.

If the proposed technique is redundant with existing functionality, improve the existing implementation and document that decision.

---

# 5. Tier 2 — Performance and memory optimisations

These features target execution speed, memory efficiency and NNUE evaluation overhead.

### 13. INFU — Incremental NNUE Feature Update

Improve incremental NNUE feature updates.

Requirements:
- Update only affected features after moves.
- Correctly handle captures, promotions, castling and en passant.
- Preserve exact consistency with full accumulator refreshes.
- Support correct undo operations.
- Avoid stale feature state.

### 14. NFAR — NNUE Fast Accumulator Refresh

Improve accumulator refresh and reconstruction.

Investigate:
- Efficient refresh paths.
- Cached accumulator availability.
- Refresh scheduling.
- SIMD opportunities.
- Avoiding unnecessary full refreshes.

Maintain a trustworthy full-refresh fallback.

### 15. SONI — SIMD-Optimised NNUE Inference

Improve NNUE inference using appropriate CPU vector instructions.

Requirements:
- Identify supported instruction sets at runtime or build time.
- Provide a portable fallback.
- Avoid unsupported instruction execution.
- Validate numerical consistency.
- Keep compiler-specific optimisations isolated where practical.

Do not assume all users have the same CPU architecture.

### 16. SMP — Scheduled Memory Prefetching

Improve memory-access scheduling.

Investigate:
- Transposition-table prefetching.
- NNUE weight access.
- Accumulator access.
- Search-stack memory locality.
- Cache behaviour during move generation.

Prefetch only when it is likely to help. Avoid blindly prefetching more memory.

### 17. CATT — Cache-Aware Transposition Table

Improve transposition-table memory locality.

Investigate:
- Cache-line behaviour.
- Table layout.
- Replacement strategy.
- Probe and store overhead.
- Memory access patterns under multithreaded search.

### 18. CTTEL — Compact TT Entry Layout

Investigate a more efficient transposition-table entry representation.

Requirements:
- Minimise unnecessary memory use.
- Preserve required move, depth, score, bound and generation information.
- Preserve mate-score encoding correctness.
- Avoid alignment and concurrency problems.
- Benchmark memory and search implications externally.

### 19. IAMM — Incremental Attack Map Maintenance

Investigate incremental maintenance of attack information.

Requirements:
- Correct handling of sliding-piece attacks.
- Correct handling of pins and discovered attacks.
- Correct make/unmake behaviour.
- Accurate invalidation rules.
- A safe full-recalculation fallback.

Do not force incremental attack maps into the engine if their maintenance cost exceeds their benefit.

### 20. IPFC — Incremental Position Feature Cache

Develop a cache for reusable position information.

Potential cached information:
- Attack maps.
- King zones.
- Pins.
- Pawn structures.
- Passed pawns.
- Mobility.
- Relevant tactical threats.

Requirements:
- Correct cache invalidation.
- No stale data between search branches.
- Efficient memory usage.
- Integration with evaluation and search.
- Make/unmake correctness.

### 21. NASA — NUMA-Aware Search Allocation

Investigate NUMA-aware thread and memory allocation.

Requirements:
- Detect whether multiple NUMA nodes exist.
- Use topology-aware allocation when supported.
- Avoid expensive topology management on single-node systems.
- Provide a portable fallback.
- Preserve existing thread correctness.

This must not add unnecessary overhead to ordinary consumer machines.

### 22. PGO — Profile-Guided Optimisation

Prepare GAMBO for reproducible profile-guided optimisation.

Requirements:
- Document suitable profiling workloads.
- Provide scripts for profile generation and profile use.
- Keep build configurations reproducible.
- Document compiler and platform assumptions.
- Avoid committing machine-specific performance claims.

Do not run extensive profiling workloads on the user's laptop.

---

# 6. Tier 3 — GAMBO MetaSearch and position intelligence

This tier develops GAMBO's own higher-level search intelligence.

The architecture should allow these systems to coordinate without replacing the correctness of the existing alpha-beta search.

### 23. PCE — Position Criticality Estimation

Develop a Position Criticality Engine.

Estimate how sensitive a position is to search mistakes.

Investigate signals such as:
- Evaluation volatility.
- PV instability.
- Candidate-score disagreement.
- King exposure.
- Forcing-move density.
- Tactical threat density.
- Material instability.
- Search-tree instability.

Produce a bounded criticality estimate.

Use it to inform search allocation, reductions, extensions and verification.

Do not confuse an uncertain position with a position that is objectively losing or winning.

### 24. ASRA — Adaptive Search Resource Allocation

Build a resource-allocation system controlled by position information.

Investigate dynamically allocating additional search effort to:
- Critical positions.
- Unstable candidate moves.
- Tactical positions.
- Uncertain evaluations.
- Positions with major candidate disagreement.

Prevent resource allocation from becoming unbounded or starving ordinary positions.

### 25. SID — Search Instability Detection

Create an independent instability-detection system.

Monitor:
- PV changes across iterations.
- Evaluation swings.
- Best-move changes.
- Search disagreement.
- Tactical volatility.

Use the result to inform time management and search allocation.

Avoid duplicating GAMBO's existing instability-aware time-management system. Integrate with and improve it.

### 26. TVS — Tactical Verification Search

Create a bounded tactical verification subsystem.

Its job is to investigate potentially critical tactical claims made by the main search.

Requirements:
- Define when verification is triggered.
- Use bounded search budgets.
- Investigate tactical refutations and defensive resources.
- Return clearly defined statuses such as `VERIFIED`, `REFUTED` and `UNKNOWN`.
- Never describe an incomplete search as a mathematical proof.
- Prevent recursive verification from consuming unlimited resources.

### 27. PNMV — Proof-Number Mate Verification

Investigate a specialised proof-number search for forced-mate verification.

Requirements:
- Isolate it from ordinary evaluation.
- Define its scope and resource budget.
- Handle attacker and defender nodes correctly.
- Respect legal chess rules.
- Do not claim a forced mate has been proven unless the verification procedure actually establishes it.
- Provide a safe fallback when the search is incomplete.

### 28. PTC — Persistent Threat Context

Create a structured threat-context system that tracks relevant threats and obligations across variations.

Investigate:
- Threatened pieces.
- King attacks.
- Tactical obligations.
- Forced responses.
- Important defensive requirements.

Requirements:
- Correct branch isolation.
- Correct state restoration.
- Efficient context updates.
- No accidental transfer of threats between unrelated variations.

### 29. CCA — Counterfactual Candidate Analysis

Develop a system for examining the robustness of important root candidates.

Instead of considering only the selected principal variation, analyse how different plausible defensive responses affect candidate quality.

Requirements:
- Operate primarily at the root or selected critical nodes.
- Use strict search budgets.
- Avoid duplicating ordinary MultiPV.
- Identify fragile candidate lines.
- Feed useful information back into candidate evaluation and search allocation.

### 30. EDSE — Event-Driven Search Extensions

Improve extensions using significant search events.

Investigate events such as:
- Critical checks.
- Singular tactical opportunities.
- Major threats.
- Forced defensive responses.
- Important tactical discoveries.

Requirements:
- Define extension triggers precisely.
- Combine safely with EGC.
- Prevent extension explosions.
- Avoid extending merely because an event is frequent.

### 31. RCRA — Root Candidate Robustness Analysis

Develop a root-level system that evaluates candidate reliability.

Investigate:
- Stability across iterative-deepening iterations.
- Candidate-score differences.
- Defensive response quality.
- Tactical vulnerability.
- PV consistency.

Use this information to improve root move selection, time allocation and candidate verification.

Do not replace objective search scores with arbitrary style bonuses.

### 32. HSWS — Heterogeneous Search Worker Scheduling

Investigate specialised search workers.

Potential responsibilities:
- Main alpha-beta search.
- Tactical verification.
- Root candidate analysis.
- Position criticality analysis.

Requirements:
- Strict thread-safety.
- Bounded work allocation.
- Avoid excessive synchronisation.
- Preserve a reliable single-thread mode.
- Ensure that auxiliary workers cannot corrupt main-search state.

---

# 7. Tier 4 — Experimental search and neural architecture

This tier is research-oriented.

Implement these systems as isolated, selectable experimental components. Do not allow unfinished experiments to destabilise the default GAMBO engine.

### 33. PG-MCTS — Policy-Guided Monte Carlo Tree Search

Investigate a policy-guided MCTS subsystem.

Explore how a move policy could guide exploration while retaining GAMBO's existing evaluation capabilities.

### 34. PUCT — Predictor Upper Confidence Bound Search

Implement or prototype PUCT selection for the experimental MCTS subsystem.

Requirements:
- Define the selection formula.
- Define policy-prior handling.
- Define value updates.
- Handle terminal states correctly.
- Keep the implementation independently testable.

### 35. T-AGS — Transposition-Aware Graph Search

Investigate graph-based search where equivalent positions can be recognised and reused safely.

Requirements:
- Define position identity correctly.
- Handle repetition and path-dependent state.
- Avoid invalid reuse of values across incompatible contexts.
- Measure memory growth.

### 36. DAG-Search — Directed Acyclic Graph Search

Investigate a DAG-based representation of reusable search states.

Requirements:
- Define node and edge ownership.
- Handle cycles and repetition safely.
- Preserve search correctness.
- Prevent uncontrolled memory growth.

### 37. PSPA — Pluggable Search Policy Architecture

Create an architecture that allows experimental search policies to be selected independently.

Potential policies:
- Standard alpha-beta.
- Policy-guided MCTS.
- PUCT.
- Hybrid search.

The default engine must remain stable even if an experimental policy fails.

### 38. BNNI — Batched Neural Network Inference

Investigate batched neural inference for suitable search workloads.

Requirements:
- Define the batching mechanism.
- Avoid excessive inference latency.
- Investigate CPU and optional accelerator execution.
- Preserve a portable CPU fallback.
- Avoid assuming GPU availability.

### 39. MHNE — Multi-Head Neural Evaluation

Investigate an optional multi-head neural evaluation architecture.

Potential heads:
- Position value.
- WDL information.
- Policy information.
- Position uncertainty.
- Relevant auxiliary predictions.

Requirements:
- Clearly define the architecture.
- Maintain network-version compatibility.
- Define model loading and validation.
- Document training-data requirements.
- Keep the existing network path functional.

Do not pretend that a new neural architecture improves playing strength without a compatible trained network and proper evaluation.

### 40. HA-MS — Hybrid Alpha-Beta and MCTS Search

Investigate a hybrid search architecture.

Requirements:
- Define how information moves between alpha-beta and MCTS.
- Establish strict resource limits.
- Prevent duplicated or wasteful analysis.
- Keep search modes independently selectable.
- Preserve standard alpha-beta as a reliable fallback.

---

# 8. Architecture and integration requirements

Do not implement all of this as one enormous search function.

Organise the architecture into coherent modules, reusing existing GAMBO structures where sensible.

Suggested conceptual modules:

- `search/heuristics/`
- `search/criticality/`
- `search/verification/`
- `search/candidates/`
- `search/mcts/`
- `search/graph/`
- `eval/nnue/`
- `eval/features/`
- `position/cache/`
- `memory/`
- `threading/`

These are suggested architectural boundaries, not permission to create needless files. Follow the conventions of the actual repository.

### Feature interactions

Explicitly audit and manage interactions between:
- TMIHH, CICH, MPCH and MTCH.
- CHQO and SEE-GCP.
- HGQMP and LMRC.
- SESR, EDSE and EGC.
- PCE, SID and ASRA.
- TVS, PNMV and CCA.
- IAMM and IPFC.
- INFU, NFAR and SONI.
- PG-MCTS, PUCT, T-AGS and DAG-Search.
- BNNI and MHNE.

Avoid double-counting the same information in multiple systems.

Do not introduce a feature simply because another feature depends on its name. Every module needs a real and clearly defined function.

### UCI configuration

Add clean UCI controls for new capabilities.

Requirements:
- Consistent naming.
- Sensible defaults.
- Valid option types and ranges.
- No duplicate option names.
- Correct behaviour when options are disabled.
- No requirement to recompile just to switch ordinary experimental features.

Use a consistent prefix such as `GAMBO.` for new options where appropriate.

Organise options into sensible groups instead of creating an unusable list of dozens of unrelated settings.

### Default behaviour

Keep the standard alpha-beta engine reliable.

- Proven, low-risk performance improvements may be enabled by default after correctness validation.
- New strength-sensitive heuristics should have independent toggles.
- Experimental MCTS, graph-search and hybrid systems must remain disabled by default until separately validated.
- New neural architectures must not break existing network loading.
- Preserve the existing Phase 1 and Phase 2 ablation controls.

Document every new option and its default value.

---

# 9. Correctness and validation

Strength improvements are meaningless if they introduce subtle chess errors.

Build a proper validation framework.

### Required correctness checks

Test:
- Move generation.
- Perft.
- Make/unmake consistency.
- Castling.
- En passant.
- Promotions.
- Pins and discovered attacks.
- Threefold repetition.
- Fifty-move rule.
- Checkmate and stalemate.
- TT bounds and mate-score handling.
- Thread safety.
- NNUE accumulator consistency.
- Incremental-cache consistency.
- UCI protocol behaviour.

For INFU and NFAR, compare incremental accumulator results against a full refresh across suitable test positions.

For IPFC and IAMM, compare cached information against full recalculation.

For search modifications, include tactical test positions and edge cases.

### Required build checks

Build the project using the repository's supported build system.

Where practical, validate:
- Release build.
- Debug build.
- Supported CPU-target builds.
- Portable fallback paths.
- UCI engine startup.
- UCI option registration.
- Basic `uci`, `isready`, `position`, `go` and `stop` behaviour.

Use compiler warnings and sanitizers where supported.

### External strength-testing infrastructure

Prepare a separate testing framework for a dedicated test machine or VPS.

Include:
- GAMBO baseline versus optimised GAMBO.
- Individual feature ablations.
- Grouped feature comparisons.
- Opening-suite configuration.
- Paired colour games.
- PGN output.
- Reproducible match configuration.
- SPRT support where appropriate.
- Result summaries.
- Elo confidence intervals.
- NPS and resource measurements.

The framework must be able to compare:

1. Original baseline.
2. Tier 1 only.
3. Tier 2 only.
4. Tier 1 + Tier 2.
5. Tier 1 + Tier 2 + Tier 3.
6. Experimental Tier 4 modes independently.

Do not run these tournaments yourself on the user's laptop.

Prepare the scripts and documentation so the tests can be run externally when dedicated hardware is available.

---

# 10. Documentation deliverables

Create and maintain these documents:

### `docs/GAMBO_FINAL_OPTIMIZATION_AUDIT.md`
The original audit and implementation status of every requested feature.

### `docs/GAMBO_FINAL_ARCHITECTURE.md`
Overall architecture, module responsibilities, data flow and feature interactions.

### `docs/GAMBO_SEARCH_OPTIMIZATIONS.md`
Detailed explanations of Tier 1 and Tier 3 search systems.

### `docs/GAMBO_PERFORMANCE_OPTIMIZATIONS.md`
NNUE, caching, memory layout, SIMD, prefetching and threading improvements.

### `docs/GAMBO_EXPERIMENTAL_SEARCH.md`
MCTS, PUCT, graph search, neural architecture and hybrid search.

### `docs/GAMBO_UCI_OPTIONS.md`
All new UCI options, descriptions, defaults and supported values.

### `docs/GAMBO_VALIDATION.md`
Correctness checks, local smoke tests and external test procedures.

### `docs/GAMBO_ROADMAP_STATUS.md`
A clear completion matrix covering all 40 items.

Documentation must describe what the code actually does. Do not document planned functionality as though it has already been implemented.

---

# 11. Implementation order

Work in the following order.

**Phase A — Repository audit and architecture**

Inspect the codebase, identify existing capabilities, map dependencies and design the implementation.

**Phase B — Tier 1 search improvements**

Implement and integrate the search improvements. Preserve tactical safety and existing GAMBO behaviour.

**Phase C — Tier 2 performance improvements**

Implement NNUE, memory, cache, SIMD and threading optimisations.

**Phase D — Tier 3 MetaSearch**

Implement position criticality, resource allocation, instability detection, verification and candidate analysis.

**Phase E — Tier 4 experimental systems**

Build isolated research implementations and selectable experimental modes. Do not force them into the standard search path.

**Phase F — Integration and validation**

Build the engine, run permitted local correctness checks, repair defects and document external testing.

Do not sacrifice correctness just to finish more items.

---

# 12. Final acceptance criteria

The work is only considered complete when:

1. All 40 requested features have an explicit audit status.
2. Suitable missing features have been implemented.
3. Existing equivalent implementations have been preserved or improved rather than duplicated.
4. All experimental features are clearly isolated.
5. Existing GAMBO Phase 1 and Phase 2 functionality remains intact.
6. The engine builds successfully in the supported configuration.
7. Required lightweight correctness checks pass.
8. UCI controls are documented and function correctly.
9. Experimental systems have clear fallback behaviour.
10. External tournament and benchmark scripts are prepared.
11. No prohibited resource-intensive experiments were run on the user's laptop.
12. All documentation accurately reflects the actual implementation.
13. All changes are committed on the new development branch without overwriting existing work.

At completion, provide a report containing:

- The actual commit and branch used.
- The features already inherited from the original codebase.
- The features newly implemented.
- The features improved or refactored.
- The experimental features implemented.
- Anything deferred, with a technical explanation.
- Build results.
- Correctness-test results.
- Known bugs and limitations.
- New UCI options.
- Documentation paths.
- Instructions for running the external tournament and benchmark suite.

**Do not claim GAMBO is stronger than Stockfish, PRAL v1 or any other engine based on implementation alone.** Report measured strength only when valid external match results exist.

---

# Final objective

Build GAMBO into a technically sophisticated, highly optimised, aggressive-but-sound chess engine.

The standard engine should remain fast, reliable and competitive. The new intelligence, verification, candidate-analysis, neural and graph-search systems should give GAMBO a foundation for further original research.

Do not optimise for the number of features implemented.

**Optimise for playing strength, tactical reliability, search efficiency, execution speed and measurable engineering quality.**

Start by inspecting the repository, then implement the complete roadmap.