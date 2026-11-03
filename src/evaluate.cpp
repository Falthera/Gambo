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

#include "evaluate.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <climits>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#include "nnue/network.h"
#include "nnue/nnue_misc.h"
#include "position.h"
#include "search.h"
#include "types.h"
#include "uci.h"
#include "nnue/nnue_accumulator.h"
#include "gambo/nnue/network.h"
#include "gambo/nnue/nnue_common.h"

namespace Stockfish {

// Debug helper to clamp classical evaluation terms to reasonable bounds
inline int clamp_classical_term(int v, const char* termName) {
    constexpr int MAX_CLASSICAL_TERM = 5000; // Sanity bound: no single term should exceed this
    if (v < -MAX_CLASSICAL_TERM || v > MAX_CLASSICAL_TERM) {
        // In debug builds, assert to catch anomalies; in release, clamp silently
        assert(false && "Classical evaluation term exceeds sanity bounds");
        return std::clamp(v, -MAX_CLASSICAL_TERM, MAX_CLASSICAL_TERM);
    }
    return v;
}

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

// Evaluate is the evaluator for the outer world. It returns a static evaluation
// of the position from the point of view of the side to move.
// This is the OBJECTIVE evaluation, without any style bias.
Value Eval::evaluate(const Eval::NNUE::Network&     network,
                     const Position&                pos,
                     Eval::NNUE::AccumulatorStack&  accumulators,
                     Eval::NNUE::AccumulatorCaches& caches,
                     int                            optimism) {

    assert(!pos.checkers());

    // Select NNUE backend
    GamboNNUEBackend backend = gambod_nnue_backend();
    
    Value psqt = 0, positional = 0;
    
    if (backend == GamboNNUEBackend::Stockfish) {
        // Standard Stockfish NNUE
        auto [sf_psqt, sf_positional] = network.evaluate(pos, accumulators, caches);
        psqt = sf_psqt;
        positional = sf_positional;
    } else {
        // GAMBO NNUE or Blend
        // For GAMBO backend, we need a GAMBO network instance
        // This is a simplified integration - in practice we'd pass the GAMBO network separately
        // For now, fall back to Stockfish NNUE if GAMBO network not available
        auto [sf_psqt, sf_positional] = network.evaluate(pos, accumulators, caches);
        psqt = sf_psqt;
        positional = sf_positional;
        
        if (backend == GamboNNUEBackend::Gambo) {
            // TODO: Integrate GAMBO::NNUE::Network when fully wired
            // For now, use Stockfish NNUE as placeholder
        } else if (backend == GamboNNUEBackend::Blend) {
            // TODO: Blend Stockfish and GAMBO NNUE outputs
            // For now, use Stockfish NNUE as placeholder
        }
    }

    // Phase-adaptive NNUE blending
    int phase = Eval::ClassicalEval::game_phase(pos);
    int psqtWeight, positionalWeight;
    if (use_phase_adaptive_nnue()) {
        // Phase-adaptive: Opening/middlegame trust positional more; endgame trust PSQT more
        psqtWeight = 126 + (256 - phase) / 8;      // 126..158 (more in endgame)
        positionalWeight = 130 - (256 - phase) / 16; // 130..114 (less in endgame)
    } else {
        // Fixed Phase 1 weights (pre-phase-adaptive): PSQT 126, Positional 130
        psqtWeight = 126;
        positionalWeight = 130;
    }
    Value nnue = (psqtWeight * psqt + positionalWeight * positional) / 128;

    // Blend optimism and eval with nnue complexity
    int nnueComplexity = std::abs(psqt - positional);
    optimism += optimism * nnueComplexity / 476;
    nnue -= nnue * nnueComplexity / 18236;

    // Classical evaluation terms for positional understanding
    Color us = pos.side_to_move();
    Color them = ~us;

    int material = 534 * pos.count<PAWN>() + pos.non_pawn_material();

    // King safety (both sides)
    int kingSafetyScore = 0;
    if (use_classical_king_safety()) {
        int ourKingSafety = Eval::ClassicalEval::king_safety(pos, us);
        int theirKingSafety = Eval::ClassicalEval::king_safety(pos, them);
        kingSafetyScore = (ourKingSafety - theirKingSafety) * (256 - phase) / 256;
    }

    // Initiative and attacking potential
    int initiativeScore = 0;
    if (use_classical_initiative()) {
        int ourInitiative = Eval::ClassicalEval::initiative(pos, us);
        int theirInitiative = Eval::ClassicalEval::initiative(pos, them);
        initiativeScore = (ourInitiative - theirInitiative) * phase / 256;
    }

    // Piece coordination
    int coordinationScore = 0;
    if (use_classical_coordination()) {
        int ourCoordination = Eval::ClassicalEval::piece_coordination(pos, us);
        int theirCoordination = Eval::ClassicalEval::piece_coordination(pos, them);
        coordinationScore = (ourCoordination - theirCoordination) * phase / 256;
    }

    // Defensive resources
    int defenseScore = 0;
    if (use_classical_defense()) {
        int ourDefense = Eval::ClassicalEval::defensive_resources(pos, us);
        int theirDefense = Eval::ClassicalEval::defensive_resources(pos, them);
        defenseScore = (ourDefense - theirDefense) * (256 - phase) / 256;
    }

    // Positional sacrifices
    int sacrificeScore = 0;
    if (use_classical_sacrifice()) {
        sacrificeScore = Eval::ClassicalEval::positional_sacrifice(pos, us);
    }

    // Advantage conversion (winning technique)
    int conversionScore = 0;
    if (use_classical_conversion()) {
        conversionScore = Eval::ClassicalEval::advantage_conversion(pos, us);
    }

    // Combine classical terms
    int classicalScore = kingSafetyScore + initiativeScore + coordinationScore
                       + defenseScore + sacrificeScore + conversionScore;

    // Scale classical evaluation by material (less important in endgames)
    int classicalWeight = std::clamp(material / 800, 0, 64);
    // Use int64_t to prevent overflow
    int64_t classicalScore64 = static_cast<int64_t>(classicalScore) * classicalWeight;
    classicalScore64 /= 64;
    int classicalScoreScaled = static_cast<int>(std::clamp<int64_t>(classicalScore64, -32000, 32000));

    // Final evaluation: blend NNUE with classical terms
    // NNUE provides the base, classical terms provide positional corrections
    // Use int64_t for intermediate to prevent overflow (nnue up to ~30000, material up to ~12000)
    int64_t v64 = (int64_t)nnue * (77871 + material) + (int64_t)optimism * (7191 + material);
    v64 /= 77871;
    int v = static_cast<int>(std::clamp<int64_t>(v64, INT_MIN / 2, INT_MAX / 2));
    v += classicalScoreScaled;

    // Damp down the evaluation linearly when shuffling
    // Slightly reduced dampening (199->210) to encourage winning play over draws
    v -= v * pos.rule50_count() / 210;

    // Guarantee evaluation does not hit the tablebase range
    v = std::clamp(v, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);

    return v;
}

// Returns evaluation with aggressive style bonus applied.
// This should be used for UCI output and root move scoring.
Value Eval::evaluate_with_style(const Eval::NNUE::Network&     network,
                                const Position&                pos,
                                Eval::NNUE::AccumulatorStack&  accumulators,
                                Eval::NNUE::AccumulatorCaches& caches,
                                int                            optimism) {
    Value v = evaluate(network, pos, accumulators, caches, optimism);

    if (Search::ultra_aggressive())
    {
        int material = 534 * pos.count<PAWN>() + pos.non_pawn_material();
        const int attack = Search::attack_pressure(pos, pos.side_to_move());
        const int scale   = std::clamp(material / 1500, 0, 2);
        v += attack * scale / 3;
    }

    return v;
}

// Like evaluate(), but instead of returning a value, it returns
// a string (suitable for outputting to stdout) that contains the detailed
// descriptions and values of each evaluation term. Useful for debugging.
// Trace scores are from white's point of view
std::string Eval::trace(Position& pos, const Eval::NNUE::Network& network) {

    if (pos.checkers())
        return "Final evaluation: none (in check)";

    auto accumulators = std::make_unique<Eval::NNUE::AccumulatorStack>();
    auto caches       = std::make_unique<Eval::NNUE::AccumulatorCaches>(network);

    std::stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);
    ss << '\n' << NNUE::trace(pos, network, *caches) << '\n';

    ss << std::showpoint << std::showpos << std::fixed << std::setprecision(2) << std::setw(15);

    auto [psqt, positional] = network.evaluate(pos, *accumulators, *caches);
    Value v                 = psqt + positional;
    ss << "NNUE evaluation          " << v << " (side to move, internal units)\n";
    v = pos.side_to_move() == WHITE ? v : -v;
    ss << "NNUE evaluation        " << 0.01 * UCIEngine::to_cp(v, pos) << " (white side)\n";

    v = evaluate(network, pos, *accumulators, *caches, VALUE_ZERO);
    v = pos.side_to_move() == WHITE ? v : -v;

    ss << "Final evaluation      ";
    ss << 0.01 * UCIEngine::to_cp(v, pos) << " (white side)";
    ss << " [with scaled NNUE, classical terms]\n";

    // Classical evaluation breakdown
    Color us = pos.side_to_move();
    Color them = ~us;
    int phase = Eval::ClassicalEval::game_phase(pos);

    ss << "\n--- Classical Evaluation Terms ---\n";
    ss << "Game phase: " << phase << "/256\n";
    ss << "PhaseAdaptiveNNUE: " << (use_phase_adaptive_nnue() ? "ON" : "OFF") << "\n";
    ss << "KingSafety: " << (use_classical_king_safety() ? "ON" : "OFF") << "\n";
    ss << "Initiative: " << (use_classical_initiative() ? "ON" : "OFF") << "\n";
    ss << "Coordination: " << (use_classical_coordination() ? "ON" : "OFF") << "\n";
    ss << "Defense: " << (use_classical_defense() ? "ON" : "OFF") << "\n";
    ss << "Sacrifice: " << (use_classical_sacrifice() ? "ON" : "OFF") << "\n";
    ss << "Conversion: " << (use_classical_conversion() ? "ON" : "OFF") << "\n";

    if (use_classical_king_safety()) {
        int ourKingSafety = Eval::ClassicalEval::king_safety(pos, us);
        int theirKingSafety = Eval::ClassicalEval::king_safety(pos, them);
        ss << "King safety (us): " << ourKingSafety << " (them): " << theirKingSafety << "\n";
    }
    if (use_classical_initiative()) {
        int ourInitiative = Eval::ClassicalEval::initiative(pos, us);
        int theirInitiative = Eval::ClassicalEval::initiative(pos, them);
        ss << "Initiative (us): " << ourInitiative << " (them): " << theirInitiative << "\n";
    }
    if (use_classical_coordination()) {
        int ourCoord = Eval::ClassicalEval::piece_coordination(pos, us);
        int theirCoord = Eval::ClassicalEval::piece_coordination(pos, them);
        ss << "Piece coordination (us): " << ourCoord << " (them): " << theirCoord << "\n";
    }
    if (use_classical_defense()) {
        int ourDef = Eval::ClassicalEval::defensive_resources(pos, us);
        int theirDef = Eval::ClassicalEval::defensive_resources(pos, them);
        ss << "Defensive resources (us): " << ourDef << " (them): " << theirDef << "\n";
    }
    if (use_classical_sacrifice()) {
        int sacScore = Eval::ClassicalEval::positional_sacrifice(pos, us);
        ss << "Positional sacrifice: " << sacScore << "\n";
    }
    if (use_classical_conversion()) {
        int convScore = Eval::ClassicalEval::advantage_conversion(pos, us);
        ss << "Advantage conversion: " << convScore << "\n";
    }

    return ss.str();
}

// Classical evaluation implementation
namespace Eval::ClassicalEval {

// King safety evaluation - assesses king exposure and pawn shelter
int king_safety(const Position& pos, Color us) {
    const Color them = ~us;
    const Square kingSq = pos.square<KING>(us);
    const Bitboard kingZone = attacks_bb<KING>(kingSq) | square_bb(kingSq);

    // Enemy pressure on our king zone
    const Bitboard enemyPressure =
      pos.attacks_by<PAWN>(them) | pos.attacks_by<KNIGHT>(them) | pos.attacks_by<BISHOP>(them)
      | pos.attacks_by<ROOK>(them) | pos.attacks_by<QUEEN>(them);

    const int zoneAttacks = popcount(enemyPressure & kingZone);
    const int directAttackers = popcount(pos.attackers_to(kingSq) & pos.pieces(them));

    // Pawn shield evaluation - squares in front of king that our pawns should occupy
    const Bitboard ourPawns = pos.pieces(us, PAWN);
    Bitboard pawnShieldSquares = 0;
    if (us == WHITE) {
        // For white king, pawn shield is on rank 2 (one rank ahead)
        Rank kingRank = rank_of(kingSq);
        if (kingRank <= RANK_7) {
            Rank shieldRank = Rank(int(kingRank) + 1);
            File kingFile = file_of(kingSq);
            for (File f = FILE_A; f <= FILE_H; ++f) {
                if (std::abs(int(f) - int(kingFile)) <= 1) {
                    pawnShieldSquares |= square_bb(make_square(f, shieldRank));
                }
            }
        }
    } else {
        // For black king, pawn shield is on rank 7 (one rank behind)
        Rank kingRank = rank_of(kingSq);
        if (kingRank >= RANK_2) {
            Rank shieldRank = Rank(int(kingRank) - 1);
            File kingFile = file_of(kingSq);
            for (File f = FILE_A; f <= FILE_H; ++f) {
                if (std::abs(int(f) - int(kingFile)) <= 1) {
                    pawnShieldSquares |= square_bb(make_square(f, shieldRank));
                }
            }
        }
    }
    const int pawnShield = popcount(ourPawns & pawnShieldSquares);

    // Open files near king
    int openFilesNearKing = 0;
    for (File f = FILE_A; f <= FILE_H; ++f) {
        if ((file_bb(f) & kingZone) && !(pos.pieces(PAWN) & file_bb(f))) {
            openFilesNearKing++;
        }
    }

    // King safety score: higher = safer (negative = danger)
    // Pawn shield is most important, then zone attacks, then direct attackers
    int safety = pawnShield * 16 - zoneAttacks * 8 - directAttackers * 12 - openFilesNearKing * 10;

    // Bonus for castled king (check king position)
    if (us == WHITE) {
        if (kingSq == SQ_G1) safety += 20; // Kingside castle
        else if (kingSq == SQ_C1) safety += 10; // Queenside castle
    } else {
        if (kingSq == SQ_G8) safety += 20; // Kingside castle
        else if (kingSq == SQ_C8) safety += 10; // Queenside castle
    }

    return clamp_classical_term(safety, "king_safety");
}

// Initiative and attacking potential - measures attacking chances
int initiative(const Position& pos, Color us) {
    const Color them = ~us;
    const Square theirKing = pos.square<KING>(them);
    const Bitboard theirKingZone = attacks_bb<KING>(theirKing) | square_bb(theirKing);

    // Our attacking pressure on enemy king
    const Bitboard ourPressure =
      pos.attacks_by<PAWN>(us) | pos.attacks_by<KNIGHT>(us) | pos.attacks_by<BISHOP>(us)
      | pos.attacks_by<ROOK>(us) | pos.attacks_by<QUEEN>(us);

    const int attackZone = popcount(ourPressure & theirKingZone);
    const int directAttackers = popcount(pos.attackers_to(theirKing) & pos.pieces(us));
    const int sliderPressure = popcount(pos.attackers_to(theirKing) & pos.pieces(us, ROOK, BISHOP, QUEEN));

    // Checking moves available
    int checkingMoves = 0;
    for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
        checkingMoves += popcount(pos.check_squares(pt) & pos.pieces(us, pt));
    }

    // Space advantage (control of central squares)
    const Bitboard centerSquares = 0x0000001818000000ULL; // e4,d4,e5,d5
    const Bitboard extendedCenter = 0x00003C7E7E3C0000ULL; // c3-f6
    int centerControl = popcount(ourPressure & centerSquares) * 2
                      + popcount(ourPressure & extendedCenter);

    // Piece activity (pieces not blocked by own pawns)
    const Bitboard ourMinors = pos.pieces(us, KNIGHT, BISHOP);
    // Count minors that are developed (not on home ranks) and have decent mobility
    int activeMinors = 0;
    Bitboard minorsCopy = ourMinors;
    while (minorsCopy) {
        Square s = pop_lsb(minorsCopy);
        int mobility = popcount(attacks_bb(pos.piece_on(s), s, pos.pieces()));
        // Knights: mobility >= 4 is decent, Bishops: mobility >= 6 is decent
        if ((type_of(pos.piece_on(s)) == KNIGHT && mobility >= 4) ||
            (type_of(pos.piece_on(s)) == BISHOP && mobility >= 6)) {
            activeMinors++;
        }
    }

    // Development lead (knights/bishops not on home squares)
    int developmentLead = 0;
    Bitboard homeSquares = (us == WHITE) ? (Rank1BB | Rank2BB) : (Rank7BB | Rank8BB);
    developmentLead = popcount(pos.pieces(us, KNIGHT, BISHOP) & ~homeSquares);

    // Initiative score: higher = more initiative
    int initiativeScore = attackZone * 12
                        + directAttackers * 20
                        + sliderPressure * 15
                        + checkingMoves * 10
                        + centerControl * 3
                        + activeMinors * 8
                        + developmentLead * 5;

    return clamp_classical_term(initiativeScore, "initiative");
}

// Piece coordination - evaluates piece harmony, outposts, batteries
int piece_coordination(const Position& pos, Color us) {
    const Color them = ~us;
    int score = 0;

    // Our pieces
    Bitboard ourKnights = pos.pieces(us, KNIGHT);
    Bitboard ourBishops = pos.pieces(us, BISHOP);
    Bitboard ourRooks = pos.pieces(us, ROOK);
    Bitboard ourQueens = pos.pieces(us, QUEEN);
    Bitboard ourPawns = pos.pieces(us, PAWN);

    // 1. Rook + Queen batteries (same file/rank)
    Bitboard rooksCopy = ourRooks;
    while (rooksCopy) {
        Square r = pop_lsb(rooksCopy);
        Bitboard queensCopy = ourQueens;
        while (queensCopy) {
            Square q = pop_lsb(queensCopy);
            if (file_of(r) == file_of(q) || rank_of(r) == rank_of(q)) {
                // Check if path is clear
                Bitboard between = between_bb(r, q);
                if (!(pos.pieces() & between)) {
                    score += 25; // Strong battery
                }
            }
        }
    }

    // 2. Bishop + Queen batteries (same diagonal)
    Bitboard bishopsCopy = ourBishops;
    while (bishopsCopy) {
        Square b = pop_lsb(bishopsCopy);
        Bitboard queensCopy2 = ourQueens;
        while (queensCopy2) {
            Square q = pop_lsb(queensCopy2);
            if ((file_of(b) + rank_of(b) == file_of(q) + rank_of(q)) ||
                (file_of(b) - rank_of(b) == file_of(q) - rank_of(q))) {
                Bitboard between = between_bb(b, q);
                if (!(pos.pieces() & between)) {
                    score += 20;
                }
            }
        }
    }

    // 3. Knight outposts (protected by pawn, not attackable by enemy pawns)
    const Bitboard enemyPawnAttacks = pos.attacks_by<PAWN>(them);
    const Bitboard ourPawnAttacks = pos.attacks_by<PAWN>(us);
    Bitboard knightsCopy = ourKnights;
    while (knightsCopy) {
        Square n = pop_lsb(knightsCopy);
        if (!(enemyPawnAttacks & square_bb(n)) && (ourPawnAttacks & square_bb(n))) {
            // Bonus increases with rank advancement
            int rankAdvancement = (us == WHITE) ? rank_of(n) : 7 - rank_of(n);
            score += 15 + rankAdvancement * 3;
            // Extra bonus if outpost attacks enemy king zone
            Square enemyKing = pos.square<KING>(them);
            Bitboard enemyKingZone = attacks_bb<KING>(enemyKing) | square_bb(enemyKing);
            if (attacks_bb(make_piece(us, KNIGHT), n, pos.pieces()) & enemyKingZone) {
                score += 10;
            }
        }
    }

    // 4. Pawn chains (mutually protecting pawns)
    Bitboard protectedPawns = ourPawns & ourPawnAttacks;
    score += popcount(protectedPawns) * 5;

    // 5. Rooks on open/semi-open files
    Bitboard rooksCopy2 = ourRooks;
    while (rooksCopy2) {
        Square r = pop_lsb(rooksCopy2);
        File f = file_of(r);
        Bitboard fileBB = file_bb(f);
        bool openFile = !(pos.pieces(PAWN) & fileBB);
        bool semiOpen = !(ourPawns & fileBB);

        if (openFile) {
            score += 20;
            // Extra bonus if rook penetrates to 7th/8th rank
            int rank = (us == WHITE) ? rank_of(r) : 7 - rank_of(r);
            if (rank >= 6) score += (rank - 5) * 10;
        } else if (semiOpen) {
            score += 10;
        }
    }

    // 6. Bishop pair bonus
    if (popcount(ourBishops) >= 2) {
        score += 30;
        // Extra bonus in open positions
        if (popcount(pos.pieces(PAWN)) < 12) score += 15;
    }

    // 7. Knight pair bonus (less than bishop pair)
    if (popcount(ourKnights) >= 2) {
        score += 10;
    }

    // 8. Queen + Knight coordination (fork potential near enemy king)
    Square enemyKing = pos.square<KING>(them);
    Bitboard enemyKingZone = attacks_bb<KING>(enemyKing) | square_bb(enemyKing);
    Bitboard queensCopy3 = ourQueens;
    while (queensCopy3) {
        Square q = pop_lsb(queensCopy3);
        Bitboard knightsCopy2 = ourKnights;
        while (knightsCopy2) {
            Square n = pop_lsb(knightsCopy2);
            Bitboard queenAttacks = attacks_bb(make_piece(us, QUEEN), q, pos.pieces());
            Bitboard knightAttacks = attacks_bb(make_piece(us, KNIGHT), n, pos.pieces());
            if (popcount(queenAttacks & knightAttacks & enemyKingZone) >= 2) {
                score += 15;
            }
        }
    }

    // 9. Connected rooks (on same rank/file, clear path)
    if (popcount(ourRooks) >= 2) {
        Square r1 = pop_lsb(ourRooks);
        Square r2 = pop_lsb(ourRooks);
        if ((file_of(r1) == file_of(r2) || rank_of(r1) == rank_of(r2))) {
            Bitboard between = between_bb(r1, r2);
            if (!(pos.pieces() & between)) {
                score += 20;
            }
        }
    }

    return clamp_classical_term(score, "piece_coordination");
}

// Defensive resources - identifies fortresses, counterplay, defensive cohesion
int defensive_resources(const Position& pos, Color us) {
    const Color them = ~us;
    int score = 0;

    // 1. Fortress detection: blocked pawn structures with no entry points
    // Look for closed positions where our king is safe behind pawns
    const Square ourKing = pos.square<KING>(us);
    const Bitboard ourKingZone = attacks_bb<KING>(ourKing) | square_bb(ourKing);
    const Bitboard ourPawns = pos.pieces(us, PAWN);

    // Pawn shield integrity
    const Bitboard pawnShield = ourPawns & ourKingZone;
    int shieldCount = popcount(pawnShield);
    if (shieldCount >= 2) {
        score += shieldCount * 8;
    }

    // 2. Blockade: enemy passed pawns blocked by our pieces
    Bitboard enemyPassedPawns = 0;
    Bitboard enemyPawns = pos.pieces(them, PAWN);
    Bitboard enemyPawnsCopy = enemyPawns;
    while (enemyPawnsCopy) {
        Square p = pop_lsb(enemyPawnsCopy);
        // Forward squares on same file
        Bitboard frontSquares = 0;
        File f = file_of(p);
        Rank r = rank_of(p);
        if (them == WHITE) {
            for (Rank r2 = Rank(int(r) + 1); r2 <= RANK_8; ++r2) {
                frontSquares |= square_bb(make_square(f, r2));
            }
        } else {
            for (Rank r2 = Rank(int(r) - 1); r2 >= RANK_1; --r2) {
                frontSquares |= square_bb(make_square(f, r2));
            }
        }
        if (!(pos.pieces(us, PAWN) & frontSquares)) {
            enemyPassedPawns |= square_bb(p);
        }
    }
    // Our pieces blockading passed pawns - collect attackers for each passed pawn
    Bitboard ourBlockaders = 0;
    Bitboard passedCopy = enemyPassedPawns;
    while (passedCopy) {
        Square p = pop_lsb(passedCopy);
        ourBlockaders |= pos.attackers_to(p) & pos.pieces(us);
    }
    score += popcount(ourBlockaders) * 12;

    // 3. Counterplay potential: our pieces have active roles
    const Bitboard ourMinors = pos.pieces(us, KNIGHT, BISHOP);
    const Bitboard ourMajors = pos.pieces(us, ROOK, QUEEN);
    int activePieces = 0;
    Bitboard minorsCopy = ourMinors;
    while (minorsCopy) {
        Square s = pop_lsb(minorsCopy);
        if (popcount(attacks_bb(pos.piece_on(s), s, pos.pieces())) >= 6) activePieces++;
    }
    Bitboard majorsCopy = ourMajors;
    while (majorsCopy) {
        Square s = pop_lsb(majorsCopy);
        if (popcount(attacks_bb(pos.piece_on(s), s, pos.pieces())) >= 10) activePieces++;
    }
    score += activePieces * 8;

    // 4. Defensive cohesion: pieces defend each other
    Bitboard defendedPieces = 0;
    Bitboard ourPiecesCopy = pos.pieces(us);
    while (ourPiecesCopy) {
        Square s = pop_lsb(ourPiecesCopy);
        if (pos.attackers_to(s) & pos.pieces(us)) {
            defendedPieces |= square_bb(s);
        }
    }
    score += popcount(defendedPieces) * 3;

    // 5. King activity in endgame (when safe)
    int material = pos.non_pawn_material() + 534 * pos.count<PAWN>();
    if (material < 8000) { // Endgame
        // King centralization is good when queens are off
        if (!pos.pieces(QUEEN)) {
            int kingCentralization = 0;
            const Bitboard center16 = 0x00003C3C3C3C0000ULL;
            if (center16 & square_bb(ourKing)) kingCentralization = 1;
            score += kingCentralization * 20;
        }
    }

    // 6. Drawing resources: opposite-colored bishops, rook vs minor, etc.
    if (popcount(pos.pieces(us, BISHOP)) == 1 && popcount(pos.pieces(them, BISHOP)) == 1) {
        Square ourB = pos.square<BISHOP>(us);
        Square theirB = pos.square<BISHOP>(them);
        if (color_of(pos.piece_on(ourB)) != color_of(pos.piece_on(theirB))) {
            score += 25; // Opposite-colored bishops drawing tendency
        }
    }

    return clamp_classical_term(score, "defensive_resources");
}

// Positional sacrifice evaluation - compensation for material given up
// This is the OBJECTIVE evaluation of positional compensation, independent of style mode.
// Style-specific aggression is applied in evaluate_with_style().
int positional_sacrifice(const Position& pos, Color us) {
    const Color them = ~us;
    const Square theirKing = pos.square<KING>(them);

    // King safety compensation - enemy king exposure
    const Bitboard enemyKingZone = attacks_bb<KING>(theirKing) | square_bb(theirKing);
    const Bitboard ourPressure =
      pos.attacks_by<PAWN>(us) | pos.attacks_by<KNIGHT>(us) | pos.attacks_by<BISHOP>(us)
      | pos.attacks_by<ROOK>(us) | pos.attacks_by<QUEEN>(us);
    const int kingZonePressure = popcount(ourPressure & enemyKingZone);

    // Direct attackers on enemy king
    const int directAttackers = popcount(pos.attackers_to(theirKing) & pos.pieces(us));

    // Piece activity compensation - our pieces are active
    const int ourMinorActivity = popcount(pos.pieces(us, KNIGHT, BISHOP) & ~pos.pieces(us, PAWN));
    const int theirMinorActivity = popcount(pos.pieces(them, KNIGHT, BISHOP) & ~pos.pieces(them, PAWN));
    const int activityAdvantage = ourMinorActivity - theirMinorActivity;

    // Initiative - we have checks or forcing moves
    int checkingMoves = 0;
    for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN})
        checkingMoves += popcount(pos.check_squares(pt) & pos.pieces(us, pt));

    // Development lead in early/middlegame
    int developmentLead = 0;
    const Bitboard ourMinors = pos.pieces(us, KNIGHT, BISHOP);
    const Bitboard homeSquares = (us == WHITE) ? (Rank1BB | Rank2BB) : (Rank7BB | Rank8BB);
    developmentLead = popcount(ourMinors & ~homeSquares);

    // Compensation formula: king safety is most important
    int compensation = kingZonePressure * 15
                     + directAttackers * 25
                     + activityAdvantage * 8
                     + checkingMoves * 12
                     + developmentLead * 3;

    // Cap compensation to avoid overestimating (fixed cap for positional evaluation)
    return std::min(compensation, 80);
}

// Advantage conversion - winning technique, simplification, endgame technique
int advantage_conversion(const Position& pos, Color us) {
    const Color them = ~us;
    int score = 0;

    // Only relevant when we have a significant advantage
    int ourMaterial = pos.non_pawn_material(us) + 534 * pos.count<PAWN>(us);
    int theirMaterial = pos.non_pawn_material(them) + 534 * pos.count<PAWN>(them);
    int materialAdv = ourMaterial - theirMaterial;

    if (materialAdv < 300) return 0; // Not enough advantage to convert

    // 1. Simplification: trade pieces when ahead
    int totalMaterial = ourMaterial + theirMaterial;
    if (totalMaterial < 16000) { // Late middlegame/endgame
        // Bonus for trading queens when ahead
        if (pos.pieces(us, QUEEN) && pos.pieces(them, QUEEN)) {
            score += 15;
        }
        // Bonus for trading rooks when ahead
        score += (popcount(pos.pieces(us, ROOK)) - popcount(pos.pieces(them, ROOK))) * 8;
    }

    // 2. King activity: our king should be active in endgame
    if (totalMaterial < 10000) {
        Square ourKing = pos.square<KING>(us);
        const Bitboard center16 = 0x00003C3C3C3C0000ULL;
        if (center16 & square_bb(ourKing)) {
            score += 20;
        }
        // King approaching enemy weaknesses
        Bitboard enemyWeaknesses = pos.pieces(them, PAWN); // Simplified
        if (attacks_bb<KING>(ourKing) & enemyWeaknesses) {
            score += 10;
        }
    }

    // 3. Passed pawn creation and support
    Bitboard ourPassedPawns = 0;
    Bitboard ourPawns = pos.pieces(us, PAWN);
    Bitboard pawnsCopy = ourPawns;
    while (pawnsCopy) {
        Square p = pop_lsb(pawnsCopy);
        File f = file_of(p);
        Rank r = rank_of(p);
        Bitboard frontSquares = 0;
        if (us == WHITE) {
            for (Rank r2 = Rank(int(r) + 1); r2 <= RANK_8; ++r2) {
                frontSquares |= square_bb(make_square(f, r2));
            }
        } else {
            for (Rank r2 = Rank(int(r) - 1); r2 >= RANK_1; --r2) {
                frontSquares |= square_bb(make_square(f, r2));
            }
        }
        if (!(pos.pieces(them, PAWN) & frontSquares)) {
            ourPassedPawns |= square_bb(p);
            // Bonus for advanced passed pawns
            int rank = (us == WHITE) ? rank_of(p) : 7 - rank_of(p);
            score += rank * 5;
            // Bonus for supported passed pawns
            if (pos.attackers_to(p) & pos.pieces(us)) {
                score += 15;
            }
        }
    }

    // 4. Piece restriction: limit enemy piece activity
    Bitboard enemyMinors = pos.pieces(them, KNIGHT, BISHOP);
    Bitboard enemyMinorsCopy = enemyMinors;
    while (enemyMinorsCopy) {
        Square s = pop_lsb(enemyMinorsCopy);
        int mobility = popcount(attacks_bb(pos.piece_on(s), s, pos.pieces()));
        if (mobility <= 4) score += 5; // Restricted enemy piece
    }

    // 5. Zugzwang potential in endgames
    if (totalMaterial < 6000 && !pos.pieces(QUEEN)) {
        // Positions where opponent has no good moves
        score += 10;
    }

    return clamp_classical_term(score, "advantage_conversion");
}

// Phase assessment - returns game phase [0..256] (0=endgame, 256=opening)
int game_phase(const Position& pos) {
    int phase = 0;
    // Phase based on non-pawn material
    // Opening: ~256, Middlegame: ~128, Endgame: ~0
    int npm = pos.non_pawn_material();
    // Max practical non-pawn material (starting ~6300, max with promotions ~12500)
    // Using 8000 gives opening phase ~200, middlegame ~100, endgame ~0
    phase = std::clamp(npm * 256 / 8000, 0, 256);
    return phase;
}

// Pawn structure evaluation - passed pawns, weaknesses, islands
int pawn_structure(const Position& pos, Color us) {
    const Color them = ~us;
    int score = 0;

    Bitboard ourPawns = pos.pieces(us, PAWN);
    Bitboard theirPawns = pos.pieces(them, PAWN);

    // Pawn islands (fewer is better)
    int ourIslands = 0;
    Bitboard temp = ourPawns;
    while (temp) {
        Square s = pop_lsb(temp);
        File f = file_of(s);
        // Clear all pawns on this file and adjacent files
        temp &= ~(file_bb(f));
        if (f > FILE_A) temp &= ~file_bb(File(int(f) - 1));
        if (f < FILE_H) temp &= ~file_bb(File(int(f) + 1));
        ourIslands++;
    }
    score -= ourIslands * 10;

    // Backward pawns
    Bitboard pawnsCopy = ourPawns;
    while (pawnsCopy) {
        Square p = pop_lsb(pawnsCopy);
        File f = file_of(p);
        Rank r = rank_of(p);
        Bitboard frontSquares = 0;
        if (us == WHITE) {
            for (Rank r2 = Rank(int(r) + 1); r2 <= RANK_8; ++r2) {
                frontSquares |= square_bb(make_square(f, r2));
            }
        } else {
            for (Rank r2 = Rank(int(r) - 1); r2 >= RANK_1; --r2) {
                frontSquares |= square_bb(make_square(f, r2));
            }
        }
        Bitboard adjacentFiles = 0;
        if (f > FILE_A) adjacentFiles |= file_bb(File(int(f) - 1));
        if (f < FILE_H) adjacentFiles |= file_bb(File(int(f) + 1));
        Bitboard support = ourPawns & adjacentFiles & frontSquares;
        if (!support && (theirPawns & adjacentFiles & frontSquares)) {
            score -= 15; // Backward pawn
        }
    }

    // Doubled pawns
    for (File f = FILE_A; f <= FILE_H; ++f) {
        int count = popcount(ourPawns & file_bb(f));
        if (count > 1) score -= (count - 1) * 15;
    }

    // Isolated pawns
    Bitboard pawnsCopy2 = ourPawns;
    while (pawnsCopy2) {
        Square p = pop_lsb(pawnsCopy2);
        File f = file_of(p);
        Bitboard adjacentFiles = 0;
        if (f > FILE_A) adjacentFiles |= file_bb(File(int(f) - 1));
        if (f < FILE_H) adjacentFiles |= file_bb(File(int(f) + 1));
        if (!(ourPawns & adjacentFiles)) {
            score -= 10; // Isolated pawn
        }
    }

    // Passed pawns
    Bitboard pawnsCopy3 = ourPawns;
    while (pawnsCopy3) {
        Square p = pop_lsb(pawnsCopy3);
        File f = file_of(p);
        Rank r = rank_of(p);
        Bitboard frontSquares = 0;
        if (us == WHITE) {
            for (Rank r2 = Rank(int(r) + 1); r2 <= RANK_8; ++r2) {
                frontSquares |= square_bb(make_square(f, r2));
            }
        } else {
            for (Rank r2 = Rank(int(r) - 1); r2 >= RANK_1; --r2) {
                frontSquares |= square_bb(make_square(f, r2));
            }
        }
        if (!(theirPawns & frontSquares)) {
            int rank = (us == WHITE) ? rank_of(p) : 7 - rank_of(p);
            score += rank * 8;
            if (rank >= 6) score += 20; // Advanced passed pawn
        }
    }

    return score;
}

// Piece activity - mobility, centralization, scope
int piece_activity(const Position& pos, Color us) {
    int score = 0;

    // Knight mobility and centralization
    Bitboard knightsCopy = pos.pieces(us, KNIGHT);
    while (knightsCopy) {
        Square n = pop_lsb(knightsCopy);
        int mobility = popcount(attacks_bb(make_piece(us, KNIGHT), n, pos.pieces()));
        score += mobility * 3;
        // Centralization bonus
        const Bitboard center16 = 0x00003C3C3C3C0000ULL;
        if (center16 & square_bb(n)) score += 10;
    }

    // Bishop mobility and diagonals
    Bitboard bishopsCopy = pos.pieces(us, BISHOP);
    while (bishopsCopy) {
        Square b = pop_lsb(bishopsCopy);
        int mobility = popcount(attacks_bb(make_piece(us, BISHOP), b, pos.pieces()));
        score += mobility * 2;
        // Long diagonal bonus
        if ((b == SQ_A1 || b == SQ_H8) && us == WHITE) score += 5;
        if ((b == SQ_A8 || b == SQ_H1) && us == BLACK) score += 5;
    }

    // Rook mobility and open files
    Bitboard rooksCopy = pos.pieces(us, ROOK);
    while (rooksCopy) {
        Square r = pop_lsb(rooksCopy);
        int mobility = popcount(attacks_bb(make_piece(us, ROOK), r, pos.pieces()));
        score += mobility;
        File f = file_of(r);
        if (!(pos.pieces(PAWN) & file_bb(f))) score += 15; // Open file
    }

    // Queen mobility
    Bitboard queensCopy = pos.pieces(us, QUEEN);
    while (queensCopy) {
        Square q = pop_lsb(queensCopy);
        int mobility = popcount(attacks_bb(make_piece(us, QUEEN), q, pos.pieces()));
        score += mobility / 2;
    }

    return score;
}

}  // namespace Eval::ClassicalEval

}  // namespace Stockfish