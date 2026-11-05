/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PNMV — Proof-Number Mate Verification
// Specialized proof-number search for forced-mate verification

#ifndef GAMBO_PNMV_H_INCLUDED
#define GAMBO_PNMV_H_INCLUDED

#include <array>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::PNMV {

using namespace Stockfish;

enum class PNNodeType {
    ATTACKER,  // AND node - all children must be proven
    DEFENDER   // OR node - at least one child must be proven
};

struct PNNode {
    Move move;
    PNNodeType type;
    uint32_t proof_number;      // min nodes to prove
    uint32_t disproof_number;   // min nodes to disprove
    bool evaluated = false;
    Value eval = VALUE_NONE;
    std::vector<PNNode*> children;
    PNNode* parent = nullptr;

    PNNode() : proof_number(1), disproof_number(1) {}
    PNNode(Move m, PNNodeType t) : move(m), type(t), proof_number(1), disproof_number(1) {}
};

struct PNMVConfig {
    int max_nodes = 100000;
    int max_depth = 30;
    int max_time_ms = 100;
    uint32_t infinity = std::numeric_limits<uint32_t>::max() / 4;
};

struct PNMVStats {
    int nodes_created = 0;
    int nodes_evaluated = 0;
    int mate_proven = 0;
    int mate_disproven = 0;
    int timeouts = 0;
};

class ProofNumberMateVerifier {
public:
    ProofNumberMateVerifier() = default;

    // Verify if position is a forced mate in N
    bool verify_mate_in(const Position& pos, Search::Stack* ss, int mate_in, int max_depth);

    // Verify if position is a forced win
    bool verify_forced_win(const Position& pos, Search::Stack* ss, int max_depth);

    // Configuration
    void set_config(const PNMVConfig& cfg) { config = cfg; }
    const PNMVConfig& get_config() const { return config; }

    // Statistics
    const PNMVStats& get_stats() const { return stats; }
    void reset_stats() { stats = {}; }

private:
    PNMVConfig config;
    PNMVStats stats;
    std::vector<PNNode> node_pool;
    size_t node_pool_idx = 0;
    std::int64_t start_time = 0;

    PNNode* create_node(Move m, PNNodeType type, PNNode* parent);
    void evaluate_node(PNNode* node, Position& pos, Search::Stack* ss, int depth);
    void update_ancestors(PNNode* node);
    PNNode* select_most_proving_node(PNNode* root);
    bool is_terminal(const Position& pos) const;
    Value terminal_value(Position& pos) const;

    void reset_pool() { node_pool.clear(); node_pool_idx = 0; }
    PNNode* new_node(Move m, PNNodeType type, PNNode* parent);
};

}  // namespace Gambo::PNMV

#endif  // GAMBO_PNMV_H_INCLUDED