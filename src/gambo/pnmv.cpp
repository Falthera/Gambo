/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PNMV — Proof-Number Mate Verification implementation

#include "pnmv.h"

#include <algorithm>
#include <chrono>

#include "../evaluate.h"
#include "../movepick.h"
#include "../position.h"
#include "../search.h"
#include "../thread.h"
#include "../timeman.h"
#include "../types.h"

namespace Gambo::PNMV {

using namespace Stockfish;

PNNode* ProofNumberMateVerifier::new_node(Move m, PNNodeType type, PNNode* parent) {
    if (node_pool_idx >= node_pool.size()) {
        node_pool.emplace_back();
    }
    PNNode* node = &node_pool[node_pool_idx++];
    node->move = m;
    node->type = type;
    node->proof_number = 1;
    node->disproof_number = 1;
    node->evaluated = false;
    node->eval = VALUE_NONE;
    node->children.clear();
    node->parent = parent;
    stats.nodes_created++;
    return node;
}

PNNode* ProofNumberMateVerifier::create_node(Move m, PNNodeType type, PNNode* parent) {
    return new_node(m, type, parent);
}

void ProofNumberMateVerifier::evaluate_node(PNNode* node, Position& pos, Search::Stack* ss, int depth) {
    if (depth <= 0 || is_terminal(pos)) {
        node->eval = terminal_value(pos);
        node->evaluated = true;

        if (node->eval >= VALUE_MATE_IN_MAX_PLY) {
            // Proven mate
            node->proof_number = 0;
            node->disproof_number = config.infinity;
        } else if (node->eval <= -VALUE_MATE_IN_MAX_PLY) {
            // Disproven mate (mated)
            node->proof_number = config.infinity;
            node->disproof_number = 0;
        } else {
            // Not proven - set high numbers
            node->proof_number = config.infinity;
            node->disproof_number = config.infinity;
        }
        stats.nodes_evaluated++;
        return;
    }

    // Generate moves
    MoveList<GenType::LEGAL> moves(pos);
    if (moves.size() == 0) {
        evaluate_node(node, pos, ss, 0);
        return;
    }

    node->type = (pos.side_to_move() == WHITE) ? PNNodeType::ATTACKER : PNNodeType::DEFENDER;

    for (Move m : moves) {
        StateInfo st;
        pos.do_move(m, st);
        PNNode* child = create_node(m, (node->type == PNNodeType::ATTACKER) ? PNNodeType::DEFENDER : PNNodeType::ATTACKER, node);
        node->children.push_back(child);
        pos.undo_move(m);
    }

    // Evaluate children
    for (PNNode* child : node->children) {
        StateInfo st;
        pos.do_move(child->move, st);
        evaluate_node(child, pos, ss, depth - 1);
        pos.undo_move(child->move);
    }

    // Update proof/disproof numbers
    if (node->type == PNNodeType::ATTACKER) {
        // AND node: proof = sum of children's proof, disproof = min of children's disproof
        uint64_t proof_sum = 0;
        uint32_t min_disproof = config.infinity;
        for (PNNode* child : node->children) {
            proof_sum += child->proof_number;
            min_disproof = std::min(min_disproof, child->disproof_number);
        }
        node->proof_number = std::min<uint64_t>(proof_sum, config.infinity);
        node->disproof_number = min_disproof;
    } else {
        // OR node: proof = min of children's proof, disproof = sum of children's disproof
        uint32_t min_proof = config.infinity;
        uint64_t disproof_sum = 0;
        for (PNNode* child : node->children) {
            min_proof = std::min(min_proof, child->proof_number);
            disproof_sum += child->disproof_number;
        }
        node->proof_number = min_proof;
        node->disproof_number = std::min<uint64_t>(disproof_sum, config.infinity);
    }

    node->evaluated = true;
}

void ProofNumberMateVerifier::update_ancestors(PNNode* node) {
    while (node->parent) {
        PNNode* parent = node->parent;
        if (parent->type == PNNodeType::ATTACKER) {
            uint64_t proof_sum = 0;
            uint32_t min_disproof = config.infinity;
            for (PNNode* child : parent->children) {
                proof_sum += child->proof_number;
                min_disproof = std::min(min_disproof, child->disproof_number);
            }
            parent->proof_number = std::min<uint64_t>(proof_sum, config.infinity);
            parent->disproof_number = min_disproof;
        } else {
            uint32_t min_proof = config.infinity;
            uint64_t disproof_sum = 0;
            for (PNNode* child : parent->children) {
                min_proof = std::min(min_proof, child->proof_number);
                disproof_sum += child->disproof_number;
            }
            parent->proof_number = min_proof;
            parent->disproof_number = std::min<uint64_t>(disproof_sum, config.infinity);
        }
        node = parent;
    }
}

PNNode* ProofNumberMateVerifier::select_most_proving_node(PNNode* root) {
    PNNode* current = root;
    while (!current->children.empty()) {
        if (current->type == PNNodeType::ATTACKER) {
            // Select child with minimum disproof number
            PNNode* best = current->children[0];
            for (PNNode* child : current->children) {
                if (child->disproof_number < best->disproof_number) {
                    best = child;
                }
            }
            current = best;
        } else {
            // Select child with minimum proof number
            PNNode* best = current->children[0];
            for (PNNode* child : current->children) {
                if (child->proof_number < best->proof_number) {
                    best = child;
                }
            }
            current = best;
        }
    }
    return current;
}

bool ProofNumberMateVerifier::is_terminal(const Position& pos) const {
    return pos.checkers() || pos.pieces(WHITE, KING) == 0 || pos.pieces(BLACK, KING) == 0;
}

Value ProofNumberMateVerifier::terminal_value(Position& pos) const {
    if (pos.checkers()) {
        // Side to move is in check - check for mate
        MoveList<GenType::LEGAL> moves(pos);
        if (moves.size() == 0) {
            // Checkmate
            return pos.side_to_move() == WHITE ? -VALUE_MATE : VALUE_MATE;
        }
    }
    // Not terminal or stalemate
    return VALUE_DRAW;
}

bool ProofNumberMateVerifier::verify_mate_in(const Position& pos, Search::Stack* ss, int mate_in, int max_depth) {
    stats = {};
    reset_pool();
    start_time = TimePoint(0);

    // Create root node (attacker tries to prove mate)
    PNNode* root = new_node(Move::none(), PNNodeType::ATTACKER, nullptr);

    // Evaluate with proof-number search
    Position mutable_pos = pos;
    evaluate_node(root, mutable_pos, ss, max_depth);

    // Check if proven
    bool proven = (root->proof_number == 0);
    if (proven) stats.mate_proven++;
    else stats.mate_disproven++;

    return proven;
}

bool ProofNumberMateVerifier::verify_forced_win(const Position& pos, Search::Stack* ss, int max_depth) {
    return verify_mate_in(pos, ss, 0, max_depth);
}

}  // namespace Gambo::PNMV