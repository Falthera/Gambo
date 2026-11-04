/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// UCI option accessor functions - inline so they can be used in multiple translation units

#ifndef UCI_OPTIONS_H_INCLUDED
#define UCI_OPTIONS_H_INCLUDED

#include "uci.h"

namespace Stockfish {

extern OptionsMap* globalOptions;

// Classical evaluation component toggles (for ablation testing)
// Default ON to preserve current behavior
inline bool use_classical_king_safety() {
    return globalOptions ? bool((*globalOptions)["Classical.KingSafety"]) : true;
}
inline bool use_classical_initiative() {
    return globalOptions ? bool((*globalOptions)["Classical.Initiative"]) : true;
}
inline bool use_classical_coordination() {
    return globalOptions ? bool((*globalOptions)["Classical.Coordination"]) : true;
}
inline bool use_classical_defense() {
    return globalOptions ? bool((*globalOptions)["Classical.Defense"]) : true;
}
inline bool use_classical_sacrifice() {
    return globalOptions ? bool((*globalOptions)["Classical.Sacrifice"]) : true;
}
inline bool use_classical_conversion() {
    return globalOptions ? bool((*globalOptions)["Classical.Conversion"]) : true;
}
inline bool use_phase_adaptive_nnue() {
    return globalOptions ? bool((*globalOptions)["Eval.PhaseAdaptiveNNUE"]) : true;
}

// GAMBO NNUE backend selection
enum class GamboNNUEBackend : int { Stockfish = 0, Gambo = 1, Blend = 2 };

inline GamboNNUEBackend gambod_nnue_backend() {
    if (!globalOptions) return GamboNNUEBackend::Stockfish;
    std::string val = std::string((*globalOptions)["Eval.NNUEBackend"]);
    if (val == "Gambo") return GamboNNUEBackend::Gambo;
    if (val == "Blend") return GamboNNUEBackend::Blend;
    return GamboNNUEBackend::Stockfish;
}

inline int gambod_nnue_blend_weight() {
    if (!globalOptions) return 50;
    return int((*globalOptions)["Eval.NNUEBlendWeight"]);
}

// Tier 1: Advanced Search History Options
inline bool use_threat_map_history() {
    return globalOptions ? bool((*globalOptions)["GAMBO.ThreatMapHistory"]) : true;
}
inline bool use_colour_continuation_history() {
    return globalOptions ? bool((*globalOptions)["GAMBO.ColourContinuationHistory"]) : true;
}
inline bool use_multi_ply_continuation_history() {
    return globalOptions ? bool((*globalOptions)["GAMBO.MultiPlyContinuationHistory"]) : false;
}
inline bool use_move_type_history() {
    return globalOptions ? bool((*globalOptions)["GAMBO.MoveTypeHistory"]) : true;
}
inline bool use_qsearch_capture_history() {
    return globalOptions ? bool((*globalOptions)["GAMBO.QSearchCaptureHistory"]) : true;
}

// Tier 2: Performance and Memory Optimisations
inline bool use_incremental_nnue() {
    return globalOptions ? bool((*globalOptions)["GAMBO.IncrementalNNUE"]) : true;
}
inline bool use_fast_accumulator_refresh() {
    return globalOptions ? bool((*globalOptions)["GAMBO.FastAccumulatorRefresh"]) : true;
}
inline bool use_simd_nnue() {
    return globalOptions ? bool((*globalOptions)["GAMBO.SIMDNNUE"]) : true;
}
inline bool use_prefetch_tt() {
    return globalOptions ? bool((*globalOptions)["GAMBO.PrefetchTT"]) : false;
}
inline bool use_compact_tt() {
    return globalOptions ? bool((*globalOptions)["GAMBO.CompactTT"]) : false;
}
inline bool use_numa_alloc() {
    return globalOptions ? bool((*globalOptions)["GAMBO.NUMAAlloc"]) : true;
}
inline std::string pgo_profile_path() {
    if (!globalOptions) return "";
    return std::string((*globalOptions)["GAMBO.PGOProfile"]);
}

// Tier 3: MetaSearch (Implicit - always on)
// PCE, SID are always enabled

// Tier 4: Experimental (GAMBO.*) — ALL DEFAULT OFF
inline bool use_pg_mcts() {
    return globalOptions ? bool((*globalOptions)["GAMBO.PGMCTS"]) : false;
}
inline int pg_mcts_nodes() {
    return globalOptions ? int((*globalOptions)["GAMBO.PGMCTS.Nodes"]) : 1000;
}
inline int pg_mcts_cpuct() {
    return globalOptions ? int((*globalOptions)["GAMBO.PGMCTS.CPUCT"]) : 125;
}
inline bool use_puct() {
    return globalOptions ? bool((*globalOptions)["GAMBO.PUCT"]) : false;
}
inline int puct_cpuct() {
    return globalOptions ? int((*globalOptions)["GAMBO.PUCT.CPUCT"]) : 125;
}
inline int puct_dirichlet_alpha() {
    return globalOptions ? int((*globalOptions)["GAMBO.PUCT.DirichletAlpha"]) : 30;
}
inline bool use_tags() {
    return globalOptions ? bool((*globalOptions)["GAMBO.TAGS"]) : false;
}
inline int tags_max_nodes() {
    return globalOptions ? int((*globalOptions)["GAMBO.TAGS.MaxNodes"]) : 1000000;
}
inline bool use_dag_search() {
    return globalOptions ? bool((*globalOptions)["GAMBO.DAGSearch"]) : false;
}
inline bool use_batched_nnue() {
    return globalOptions ? bool((*globalOptions)["GAMBO.BatchedNNUE"]) : false;
}
inline int batch_size() {
    return globalOptions ? int((*globalOptions)["GAMBO.BatchSize"]) : 8;
}
inline bool use_mhne() {
    return globalOptions ? bool((*globalOptions)["GAMBO.MHNE"]) : false;
}
inline std::string mhne_heads() {
    if (!globalOptions) return "";
    return std::string((*globalOptions)["GAMBO.MHNE.Heads"]);
}
inline bool use_hybrid_search() {
    return globalOptions ? bool((*globalOptions)["GAMBO.HybridSearch"]) : false;
}
inline int hybrid_ab_depth() {
    return globalOptions ? int((*globalOptions)["GAMBO.HybridSearch.ABDepth"]) : 80;
}
inline int hybrid_mcts_budget() {
    return globalOptions ? int((*globalOptions)["GAMBO.HybridSearch.MCTSBudget"]) : 20;
}

}  // namespace Stockfish

#endif  // UCI_OPTIONS_H_INCLUDED