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

// PNMV - Proof-Number Mate Verification implementation

#include "pnmv.h"
#include "position.h"
#include "movegen.h"
#include "search.h"
#include "thread.h"

namespace Stockfish {
namespace PNMV {

PNMVConfig pnmvConfig;
PNMVStats pnmvStats;

void init_pnmv() {
    pnmvConfig.enabled = true;
    pnmvConfig.maxProofNumber = 1000000;
    pnmvConfig.maxDisproofNumber = 1000000;
    pnmvConfig.maxDepth = 20;
    pnmvConfig.maxNodes = 50000;
    
    pnmvStats = PNMVStats{};
}

// PNNode memory management
struct PNNodePool {
    std::vector<PNNode> nodes;
    std::vector<PNNode*> freeList;
    
    PNNode* allocate() {
        if (!freeList.empty()) {
            PNNode* n = freeList.back();
            freeList.pop_back();
            *n = PNNode{};
            return n;
        }
        nodes.emplace_back();
        return &nodes.back();
    }
    
    void deallocate(PNNode* n) {
        n->children.clear();
        n->parent = nullptr;
        freeList.push_back(n);
    }
    
    void clear() {
        nodes.clear();
        freeList.clear();
    }
};

static PNNodePool g_nodePool;
static uint64_t g_nodesExpanded = 0;
static uint64_t g_startTime = 0;
static int g_maxDepth = 0;
static uint32_t g_maxPN = 0;
static uint32_t g_maxDPN = 0;

void reset_pool() {
    g_nodePool.clear();
    g_nodesExpanded = 0;
    g_startTime = 0;
    g_maxDepth = 0;
    g_maxPN = 0;
    g_maxDPN = 0;
}

PNNode* create_node(PNNodeType type, PNNode* parent, int depth) {
    PNNode* node = g_nodePool.allocate();
    node->type = type;
    node->parent = parent;
    node->depth = depth;
    node->proofNumber = (type == PNNodeType::OR) ? 1 : std::numeric_limits<uint32_t>::max();
    node->disproofNumber = (type == PNNodeType::AND) ? 1 : std::numeric_limits<uint32_t>::max();
    return node;
}

void evaluate_node(PNNode* node, Position& pos) {
    // Check for mate
    MoveList<GenType::LEGAL> moves(pos);
    if (moves.empty()) {
        if (pos.checkers()) {
            // Checkmate
            if (node->type == PNNodeType::OR) {
                node->proofNumber = 0;
                node->disproofNumber = std::numeric_limits<uint32_t>::max();
            } else {
                node->proofNumber = std::numeric_limits<uint32_t>::max();
                node->disproofNumber = 0;
            }
        } else {
            // Stalemate
            node->proofNumber = std::numeric_limits<uint32_t>::max();
            node->disproofNumber = 0;
        }
        node->evaluated = true;
        return;
    }
    
    // Generate children
    node->children.reserve(moves.size());
    for (Move m : moves) {
        StateInfo st;
        bool givesCheck = pos.gives_check(m);
        pos.do_move(m, st, givesCheck, {}, {}, nullptr, nullptr);
        
        PNNode* child = create_node(
            (node->type == PNNodeType::OR) ? PNNodeType::AND : PNNodeType::OR,
            node,
            node->depth + 1
        );
        child->bestMove = m;
        node->children.push_back(child);
        
        pos.undo_move(m);
    }
    node->evaluated = true;
    g_nodesExpanded++;
}

void update_ancestors(PNNode* node) {
    while (node->parent) {
        PNNode* parent = node->parent;
        
        if (parent->type == PNNodeType::OR) {
            // OR node: min proof, sum disproof
            uint32_t minProof = std::numeric_limits<uint32_t>::max();
            uint64_t sumDisproof = 0;
            
            for (PNNode* child : parent->children) {
                minProof = std::min(minProof, child->proofNumber);
                sumDisproof += child->disproofNumber;
            }
            
            parent->proofNumber = minProof;
            parent->disproofNumber = std::min<uint64_t>(sumDisproof, std::numeric_limits<uint32_t>::max());
        } else {
            // AND node: sum proof, min disproof
            uint64_t sumProof = 0;
            uint32_t minDisproof = std::numeric_limits<uint32_t>::max();
            
            for (PNNode* child : parent->children) {
                sumProof += child->proofNumber;
                minDisproof = std::min(minDisproof, child->disproofNumber);
            }
            
            parent->proofNumber = std::min<uint64_t>(sumProof, std::numeric_limits<uint32_t>::max());
            parent->disproofNumber = minDisproof;
        }
        
        // Update best move for OR nodes
        if (parent->type == PNNodeType::OR && parent->proofNumber < std::numeric_limits<uint32_t>::max()) {
            for (PNNode* child : parent->children) {
                if (child->proofNumber == parent->proofNumber) {
                    parent->bestMove = child->bestMove;
                    break;
                }
            }
        }
        
        node = parent;
    }
}

PNNode* select_most_proving_node(PNNode* node) {
    while (!node->children.empty() && !node->evaluated) {
        if (node->type == PNNodeType::OR) {
            // Select child with minimum proof number
            PNNode* best = node->children[0];
            for (PNNode* child : node->children) {
                if (child->proofNumber < best->proofNumber) {
                    best = child;
                }
            }
            node = best;
        } else {
            // AND node: select child with minimum disproof number
            PNNode* best = node->children[0];
            for (PNNode* child : node->children) {
                if (child->disproofNumber < best->disproofNumber) {
                    best = child;
                }
            }
            node = best;
        }
    }
    return node;
}

PNMVResult pn_search(Position& pos, int maxDepth, int maxNodes) {
    reset_pool();
    g_maxDepth = maxDepth;
    g_maxPN = pnmvConfig.maxProofNumber;
    g_maxDPN = pnmvConfig.maxDisproofNumber;
    g_nodesExpanded = 0;
    g_startTime = Threads.nodes_searched(); // Use node count as time proxy
    
    // Create root node
    PNNode* root = create_node(PNNodeType::OR, nullptr, 0);
    
    // Evaluate root
    evaluate_node(root, pos);
    update_ancestors(root);
    
    // Check if already solved
    if (root->proofNumber == 0) return PNMVResult::PROVEN;
    if (root->disproofNumber == 0) return PNMVResult::DISPROVEN;
    
    // Main PN search loop
    while (g_nodesExpanded < static_cast<uint64_t>(maxNodes) && 
           root->proofNumber < g_maxPN && 
           root->disproofNumber < g_maxDPN &&
           root->depth < maxDepth) {
        
        // Select most proving node
        PNNode* mostProving = select_most_proving_node(root);
        
        // Evaluate it
        if (!mostProving->evaluated) {
            evaluate_node(mostProving, pos);
            update_ancestors(mostProving);
        }
        
        // Check if solved
        if (root->proofNumber == 0) return PNMVResult::PROVEN;
        if (root->disproofNumber == 0) return PNMVResult::DISPROVEN;
        
        // Check depth limit
        if (mostProving->depth >= maxDepth) {
            // Prune deep nodes
            mostProving->proofNumber = std::numeric_limits<uint32_t>::max();
            mostProving->disproofNumber = std::numeric_limits<uint32_t>::max();
            update_ancestors(mostProving);
        }
    }
    
    pnmvStats.totalSearches++;
    pnmvStats.avgDepth = (pnmvStats.avgDepth * (pnmvStats.totalSearches - 1) + g_nodesExpanded) / pnmvStats.totalSearches;
    pnmvStats.avgNodes = (pnmvStats.avgNodes * (pnmvStats.totalSearches - 1) + g_nodesExpanded) / pnmvStats.totalSearches;
    
    if (root->proofNumber == 0) {
        pnmvStats.proven++;
        return PNMVResult::PROVEN;
    }
    if (root->disproofNumber == 0) {
        pnmvStats.disproven++;
        return PNMVResult::DISPROVEN;
    }
    
    pnmvStats.unknown++;
    return PNMVResult::UNKNOWN;
}

PNMVResult prove_mate(Position& pos, int maxDepth, int maxNodes) {
    if (!pnmvConfig.enabled) return PNMVResult::UNKNOWN;
    
    pnmvStats.totalSearches++;
    return pn_search(pos, maxDepth, maxNodes);
}

PNMVResult prove_mate_with_bounds(Position& pos, Value alpha, Value beta, int maxDepth, int maxNodes) {
    // Use bounds to guide the search
    if (!pnmvConfig.enabled) return PNMVResult::UNKNOWN;
    
    // If we're already outside mate bounds, we can use tighter constraints
    if (beta <= -VALUE_MATE_IN_MAX_PLY || alpha >= VALUE_MATE_IN_MAX_PLY) {
        // Already in mate territory, use smaller search
        return pn_search(pos, maxDepth / 2, maxNodes / 2);
    }
    
    return pn_search(pos, maxDepth, maxNodes);
}

const PNMVStats& get_pnmv_stats() {
    return pnmvStats;
}

uint32_t get_proof_number(const Position& pos) {
    // Create temporary search to get proof number
    PNMVConfig tempConfig = pnmvConfig;
    pnmvConfig.enabled = true;
    pnmvConfig.maxNodes = 1000;
    pnmvConfig.maxDepth = 6;
    
    Position tempPos = pos; // Copy
    PNMVResult result = prove_mate(tempPos, 6, 1000);
    
    pnmvConfig = tempConfig;
    
    if (result == PNMVResult::PROVEN) return 1;
    if (result == PNMVResult::DISPROVEN) return std::numeric_limits<uint32_t>::max();
    return 1000000;
}

uint32_t get_disproof_number(const Position& pos) {
    // Similar to proof number but for disproof
    return get_proof_number(pos); // Simplified
}

}  // namespace PNMV
}  // namespace Stockfish