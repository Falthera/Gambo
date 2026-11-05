/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// PCE — Position Criticality Engine implementation

#include "pce.h"

#include <algorithm>
#include <cstdint>

#include "../bitboard.h"
#include "../evaluate.h"
#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::PCE {

using namespace Stockfish;

int PositionCriticalityEngine::evaluate(const Position& pos, const Search::Stack* ss, int depth) {
    int score = 0;
    
    // 1. King exposure (0-40)
    score += std::min(king_exposure(pos), 40);
    
    // 2. Forcing move density (0-35)
    score += std::min(forcing_move_density(pos), 35);
    
    // 3. Evaluation volatility (0-35)
    score += std::min(evaluation_volatility(pos, ss), 35);
    
    // 4. PV instability (0-30)
    score += std::min(pv_instability(ss), 30);
    
    // 5. Candidate disagreement (0-30)
    score += std::min(candidate_disagreement(pos), 30);
    
    // 6. Tactical threat density (0-30)
    score += std::min(tactical_threat_density(pos), 30);
    
    // 7. Material instability (0-25)
    score += std::min(material_instability(pos), 25);
    
    // 8. Search tree instability (0-25)
    score += std::min(search_tree_instability(ss), 25);
    
    // Scale by depth - deeper searches are more reliable
    int depthFactor = std::max(100 - depth * 3, 50);
    score = score * depthFactor / 100;
    
    return std::clamp(score, 0, 255);
}

int PositionCriticalityEngine::king_exposure(const Position& pos) {
    int score = 0;
    for (Color c : {WHITE, BLACK}) {
        Square kingSq = pos.square<KING>(c);
        Bitboard kingZone = attacks_bb<KING>(kingSq) | square_bb(kingSq);
        Bitboard enemyPressure = pos.attacks_by<PAWN>(~c) | pos.attacks_by<KNIGHT>(~c)
                               | pos.attacks_by<BISHOP>(~c) | pos.attacks_by<ROOK>(~c)
                               | pos.attacks_by<QUEEN>(~c);
        int zoneAttacks = popcount(enemyPressure & kingZone);
        int pawnShield = popcount(pos.pieces(c, PAWN) & attacks_bb<PAWN>(kingSq, ~c));
        int directAttackers = popcount(pos.attackers_to(kingSq) & pos.pieces(~c));
        
        // More exposure = fewer defending pawns, more attackers
        score += zoneAttacks * 2 + directAttackers * 3 + (8 - pawnShield) * 2;
    }
    return std::min(score, 40);
}

int PositionCriticalityEngine::forcing_move_density(const Position& pos) {
    int forcingMoves = 0;
    int totalMoves = 0;
    
    MoveList<ALL_MOVES> ml(pos);
    for (Move m : ml) {
        totalMoves++;
        if (pos.gives_check(m, pos.checkers()) || pos.capture_stage(m) || m.type_of() == PROMOTION)
            forcingMoves++;
    }
    
    if (totalMoves == 0) return 0;
    return forcingMoves * 35 / totalMoves;
}

int PositionCriticalityEngine::evaluation_volatility(const Position& pos, const Search::Stack* ss) {
    // Estimate evaluation swing potential from static eval and material
    int staticEval = Eval::evaluate(Eval::NNUE::Network{}, pos, *const_cast<Stack*>(ss), *const_cast<AccumulatorCaches*>(nullptr), 0);
    int material = pos.non_pawn_material();
    
    // Higher volatility when static eval is far from material balance
    int imbalance = std::abs(staticEval);
    int relativeImbalance = imbalance * 35 / (material + 1000);
    return std::min(relativeImbalance, 35);
}

int PositionCriticalityEngine::pv_instability(const Search::Stack* ss) {
    // Track PV changes from stack history
    int instability = 0;
    for (int i = 1; i < 4 && (ss - i)->pv; ++i) {
        if ((ss - i)->pv->size() > 0 && (ss - i - 1)->pv && (ss - i - 1)->pv->size() > 0) {
            if ((*(ss - i)->pv)[0] != (*(ss - i - 1)->pv)[0])
                instability += 10;
        }
    }
    return std::min(instability, 30);
}

int PositionCriticalityEngine::candidate_disagreement(const Position& pos) {
    // Simplified: count moves with similar SEE values (indicates choice)
    int candidates = 0;
    int bestSee = -VALUE_INFINITE;
    
    MoveList<ALL_MOVES> ml(pos);
    for (Move m : ml) {
        int see = pos.see(m);
        if (see > bestSee) bestSee = see;
    }
    
    for (Move m : ml) {
        if (pos.see(m) >= bestSee - 100) // Within 100cp of best
            candidates++;
    }
    
    return std::min((candidates - 1) * 5, 30);
}

int PositionCriticalityEngine::tactical_threat_density(const Position& pos) {
    int threats = 0;
    for (Color c : {WHITE, BLACK}) {
        Square kingSq = pos.square<KING>(c);
        Bitboard kingZone = attacks_bb<KING>(kingSq) | square_bb(kingSq);
        Bitboard enemyPressure = pos.attacks_by<PAWN>(~c) | pos.attacks_by<KNIGHT>(~c)
                               | pos.attacks_by<BISHOP>(~c) | pos.attacks_by<ROOK>(~c)
                               | pos.attacks_by<QUEEN>(~c);
        threats += popcount(enemyPressure & kingZone);
    }
    return std::min(threats * 2, 30);
}

int PositionCriticalityEngine::material_instability(const Position& pos) {
    int whiteMat = pos.non_pawn_material(WHITE);
    int blackMat = pos.non_pawn_material(BLACK);
    int diff = std::abs(whiteMat - blackMat);
    int total = whiteMat + blackMat;
    
    if (total == 0) return 0;
    // High instability when material is low and difference is small (endgames)
    if (total < 4000 && diff < 200) return 25;
    if (total < 8000 && diff < 500) return 15;
    return 0;
}

int PositionCriticalityEngine::search_tree_instability(const Search::Stack* ss) {
    // Estimate from fail-high count and cutoff count
    int instability = ss->failHighCount * 3 + ss->cutoffCnt;
    return std::min(instability, 25);
}

}  // namespace Gambo::PCE