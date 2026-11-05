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

#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <cstring>

#include "history.h"
#include "misc.h"
#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include "numa.h"
#include "position.h"
#include "score.h"
#include "syzygy/tbprobe.h"
#include "timeman.h"
#include "types.h"

namespace Stockfish {

// Different node types, used as a template parameter
enum NodeType {
    NonPV,
    PV,
    Root
};

class TranspositionTable;
class ThreadPool;
class OptionsMap;

namespace Search {

// Forward declarations
struct PVMoves;

// Stack struct keeps track of the information we need to remember from nodes
// shallower and deeper in the tree during the search. Each search thread has
// its own array of Stack objects, indexed by the current ply.
struct Stack {
    PVMoves*                    pv;
    PieceToHistory*             continuationHistory;
    CorrectionHistory<PieceTo>* continuationCorrectionHistory;
    ThreatMapHistory*           threatMapHistory;
    ColourContinuationHistory*  colourContinuationHistory;
    MoveTypeHistory*            moveTypeHistory;
    MoveTypeCaptureHistory*     moveTypeCaptureHistory;
    QSearchCaptureHistory*      qSearchCaptureHistory;
    int                         ply;
    Move                        currentMove;
    Move                        excludedMove;
    Value                       staticEval;
    int                         statScore;
    int                         moveCount;
    bool                        inCheck;
    bool                        ttPv;
    bool                        ttHit;
    bool                        followPV;
    int                         cutoffCnt;
    int                         reduction;
    int                         failHighCount;
    int                         kingZoneAttackers;
    // EGC: Extension Growth Control
    int                         extensionAccumulator;
    int                         singularExtensionCount;
};

enum class StyleMode : int { Balanced = 0, UltraAggressive = 1 };

inline std::atomic<int>& style_mode_storage() {
    static std::atomic<int> mode{int(StyleMode::Balanced)};
    return mode;
}

inline void set_style_mode(StyleMode mode) {
    style_mode_storage().store(int(mode), std::memory_order_relaxed);
}

inline StyleMode style_mode() {
    return StyleMode(style_mode_storage().load(std::memory_order_relaxed));
}

inline bool ultra_aggressive() {
    return style_mode() == StyleMode::UltraAggressive;
}

inline int style_aggression_bias() {
    return ultra_aggressive() ? 1 : 0;
}

inline Bitboard king_ring(Square kingSq) {
    return attacks_bb<KING>(kingSq) | square_bb(kingSq);
}

inline int attack_pressure(const Position& pos, Color us) {
    if (!ultra_aggressive())
        return 0;

    const Color them = ~us;
    const Square theirKing = pos.square<KING>(them);
    const Square ourKing    = pos.square<KING>(us);

    const Bitboard ourPressure =
      pos.attacks_by<PAWN>(us) | pos.attacks_by<KNIGHT>(us) | pos.attacks_by<BISHOP>(us)
      | pos.attacks_by<ROOK>(us) | pos.attacks_by<QUEEN>(us);

    const Bitboard theirPressure =
      pos.attacks_by<PAWN>(them) | pos.attacks_by<KNIGHT>(them) | pos.attacks_by<BISHOP>(them)
      | pos.attacks_by<ROOK>(them) | pos.attacks_by<QUEEN>(them);

    const int attackZone      = popcount(ourPressure & king_ring(theirKing));
    const int counterZone     = popcount(theirPressure & king_ring(ourKing));
    const int directAttackers = popcount(pos.attackers_to(theirKing) & pos.pieces(us));
    const int directDefenders = popcount(pos.attackers_to(ourKing) & pos.pieces(them));
    const int sliderPressure   = popcount(pos.attackers_to(theirKing) & pos.pieces(us, ROOK, BISHOP, QUEEN));

    return 18 * attackZone + 11 * directAttackers + 7 * sliderPressure - 13 * counterZone
         - 6 * directDefenders;
}

// King safety assessment - returns a score indicating how exposed the king is.
// Higher values = more exposed king = more attacking potential for the opponent.
inline int king_danger(const Position& pos, Color kingColor) {
    const Color them = ~kingColor;
    const Square kingSq = pos.square<KING>(kingColor);
    const Bitboard kingZone = attacks_bb<KING>(kingSq) | square_bb(kingSq);

    const Bitboard enemyPressure =
      pos.attacks_by<PAWN>(them) | pos.attacks_by<KNIGHT>(them) | pos.attacks_by<BISHOP>(them)
      | pos.attacks_by<ROOK>(them) | pos.attacks_by<QUEEN>(them);

    const int zoneAttacks = popcount(enemyPressure & kingZone);
    const int directAttackers = popcount(pos.attackers_to(kingSq) & pos.pieces(them));
    const int pawnShield = popcount(pos.pieces(kingColor, PAWN) & attacks_bb<PAWN>(kingSq, them));

    // More danger = fewer defending pawns, more attackers
    return zoneAttacks * 2 + directAttackers * 3 + (8 - pawnShield) * 2;
}

// Dynamic attack bias that scales with search depth and fail-high history.
// Returns a bias value that decreases as depth increases, and increases
// when the position has a history of failing high (indicating real attacking chances).
// Only provides significant relaxation when pieces are actively attacking the enemy king zone.
// Also considers king safety - if opponent king is in danger, increase bias.
inline int dynamic_attack_bias(const Position& pos, Color us, const Stack* ss, Depth depth) {
    if (!ultra_aggressive())
        return 0;

    const int baseBias = attack_pressure(pos, us);
    if (baseBias <= 0)
        return 0;

    // Scale down relaxation as depth increases
    // At depth 4: ~100% of bias, at depth 12: ~30%, at depth 20: ~15%
    const int depthFactor = std::max(15, 100 - depth * 4);
    int scaledBias = baseBias * depthFactor / 100;

    // Boost bias if we have fail-high history at this node
    if (ss && ss->failHighCount > 0) {
        const int failHighBoost = std::min(ss->failHighCount * 8, 32);
        scaledBias += failHighBoost;
    }

    // Boost bias if moving piece is entering enemy king zone
    if (ss && ss->kingZoneAttackers > 0) {
        scaledBias += ss->kingZoneAttackers * 5;
    }

    // Boost bias if opponent king is in danger (king safety assessment)
    const int enemyKingDanger = king_danger(pos, ~us);
    if (enemyKingDanger > 8) {
        scaledBias += std::min(enemyKingDanger - 8, 16);
    }

    // Cap the bias to prevent over-pruning relaxation
    return std::min(scaledBias, 64);
}

inline int tactical_volatility(const Position& pos, Color us) {
    if (!ultra_aggressive())
        return 0;

    const int attack    = attack_pressure(pos, us);
    const int material  = int(pos.non_pawn_material(us) + pos.non_pawn_material(~us));
    const int imbalance = int(pos.non_pawn_material(us) - pos.non_pawn_material(~us));
    return std::max(0, attack / 4 + material / 1800 - (imbalance >= 0 ? imbalance : -imbalance) / 220);
}

// TMIHH — Threat-Map Indexed History Heuristic
// Returns a threat category [0..7] for the current position and move:
// 0 = no significant threat
// 1 = king ring attack (attacking enemy king zone)
// 2 = checking move
// 3 = slider attack (rook/bishop/queen on open line)
// 4 = knight fork threat
// 5 = pin
// 6 = discovered attack
// 7 = multiple simultaneous threats
inline int threat_category(const Position& pos, Color us, Move m, const Stack* ss) {
    const Color them = ~us;
    const Square theirKing = pos.square<KING>(them);
    const Square ourKing   = pos.square<KING>(us);
    const Square from = m.from_sq();
    const Square to   = m.to_sq();
    const Piece pc = pos.piece_on(from);
    const PieceType pt = type_of(pc);

    // Category 2: Checking move (highest priority for tactical relevance)
    if (pos.gives_check(m))
        return 2;

    // Category 1: King ring attack
    const Bitboard kingRing = attacks_bb<KING>(theirKing) | square_bb(theirKing);
    if (attacks_bb(pc, to, pos.pieces()) & kingRing)
        return 1;

    // Category 3: Slider attack on open line
    if (pt == ROOK || pt == BISHOP || pt == QUEEN) {
        Bitboard sliderAttacks = attacks_bb(pt, to, pos.pieces());
        if (sliderAttacks & pos.pieces(them))
            return 3;
    }

    // Category 4: Knight fork threat
    if (pt == KNIGHT) {
        Bitboard knightAttacks = attacks_bb<KNIGHT>(to);
        if (popcount(knightAttacks & pos.pieces(them, ROOK, BISHOP, QUEEN, KING)) >= 2)
            return 4;
    }

    // Category 5: Pin
    if (pt == BISHOP || pt == ROOK || pt == QUEEN) {
        Bitboard line = attacks_bb(pt, to, pos.pieces());
        if (line & pos.pieces(them, KING)) {
            // Check if there's a piece between attacker and king that could be pinned
            Bitboard between = line & pos.pieces();
            if (between)
                return 5;
        }
    }

    // Category 6: Discovered attack
    if (ss && ss->ply > 0) {
        const Move prevMove = (ss - 1)->currentMove;
        if (prevMove != Move::none()) {
            // If previous move was a piece move that uncovered an attack
            Square prevFrom = prevMove.from_sq();
            Square prevTo   = prevMove.to_sq();
            if (attacks_bb(pos.piece_on(prevTo), prevTo, pos.pieces()) & pos.pieces(them, KING))
                return 6;
        }
    }

    // Category 7: Multiple threats (simplified - check if attack pressure is high)
    const int attackPressure = attack_pressure(pos, us);
    if (attackPressure > 16)
        return 7;

    return 0; // No significant threat
}

// MTCH — Move-Type Conditioned History
// Classifies moves into categories for separate history tracking
enum class MoveTypeClass : int {
    MT_QUIET    = 0,  // Normal quiet move
    MT_CAPTURE  = 1,  // Capture
    MT_PROMOTION = 2, // Promotion
    MT_CHECK    = 3,  // Checking move
    MT_EVASION  = 4,  // Evasion from check
    MT_CASTLING = 5,  // Castling
    MT_ENPASSANT = 6, // En passant
    MT_NUM_TYPES = 7
};

// Returns the move type class for history indexing
inline MoveTypeClass move_type_class(const Position& pos, Move m, bool inCheck) {
    if (m.type_of() == CASTLING)
        return MoveTypeClass::MT_CASTLING;
    if (m.type_of() == EN_PASSANT)
        return MoveTypeClass::MT_ENPASSANT;
    if (m.type_of() == PROMOTION)
        return MoveTypeClass::MT_PROMOTION;
    if (pos.capture_stage(m))
        return MoveTypeClass::MT_CAPTURE;
    if (pos.gives_check(m))
        return MoveTypeClass::MT_CHECK;
    if (inCheck)
        return MoveTypeClass::MT_EVASION;
    return MoveTypeClass::MT_QUIET;
}

// SEE-GCP — SEE-Guided Capture Partitioning
// Classifies captures by SEE value for ordering and pruning decisions
enum class SEEClass : int {
    SEE_WINNING    = 0, // SEE >= 0 (winning or equal)
    SEE_LOSING     = 1, // SEE < 0 (losing)
    SEE_PROMOTION  = 2, // Promotion capture
    SEE_ENPASSANT  = 3, // En passant capture
    SEE_NUM_CLASSES = 4
};

// Returns the SEE class for a capture move
inline SEEClass see_class(const Position& pos, Move m, int seeValue) {
    if (m.type_of() == PROMOTION)
        return SEEClass::SEE_PROMOTION;
    if (m.type_of() == EN_PASSANT)
        return SEEClass::SEE_ENPASSANT;
    if (seeValue >= 0)
        return SEEClass::SEE_WINNING;
    return SEEClass::SEE_LOSING;
}

// Static Exchange Evaluation - returns the SEE value of a move
// Uses binary search with see_ge to find the exact SEE value
inline int see(const Position& pos, Move m) {
    // Quick check: if not a capture or promotion, SEE is 0
    if (!pos.capture_stage(m) && m.type_of() != PROMOTION)
        return 0;
    
    // Binary search for SEE value
    // SEE values are typically in range [-VALUE_INFINITE, VALUE_INFINITE]
    // But practically in [-20000, 20000]
    int lo = -20000, hi = 20000;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pos.see_ge(m, mid))
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return hi;
}

// HGQMP — History-Guided Quiet Move Pruning
// Returns true if a quiet move should be pruned based on history and context
inline bool prune_quiet_move(const Position& pos, const Stack* ss, Move move, int moveCount, Depth depth, int historyScore, bool inCheck, bool ttPv, int threatCat) {
    if (inCheck || ttPv || moveCount <= 2)
        return false;
    
    // Don't prune killer moves
    if (move == (ss - 1)->currentMove || move == (ss - 2)->currentMove)
        return false;
    
    // Don't prune moves with good history
    if (historyScore > 0)
        return false;
    
    // Don't prune in critical tactical positions
    if (threatCat >= 2) // check, slider, fork, pin, discovered, multiple
        return false;
    
    // Depth-based pruning threshold
    int depthThreshold = depth / 4 + 2;
    if (moveCount <= depthThreshold)
        return false;
    
    // History-based pruning: very bad history scores get pruned
    if (historyScore < -1000 * depthThreshold)
        return true;
    
    return false;
}

// URD — Upcoming Repetition Detection
// Detects if a move would lead to an upcoming repetition
inline bool upcoming_repetition(const Position& pos, Move m, int depth, const Stack* ss) {
    if (depth < 4)
        return false;
    
    // Check if the move would repeat a position in the near future
    // This is a simplified check - a full implementation would track the move
    // and resulting position to see if it leads to a 3-fold repetition
    if (pos.is_draw(ss ? ss->ply : 0))
        return true;
    
    // Check if the move is a "waiting" move that could lead to repetition
    if (!pos.capture_stage(m) && !pos.gives_check(m)) {
        // Simple heuristic: if we're shuffling and the position is balanced
        // (no clear advantage), we might be heading towards repetition
        if (pos.non_pawn_material() < 4000)
            return true;
    }
    
    return false;
}
// Returns a score representing the attacking compensation for material sacrificed.
// Positive values indicate compensation (initiative, king safety, piece activity).
// This is used to distinguish sound sacrifices from unsound ones.
inline int sacrifice_compensation(const Position& pos, Color us, int materialSacrificed) {
    if (!ultra_aggressive())
        return 0;

    const Color them = ~us;
    const Square theirKing = pos.square<KING>(them);
    const Square ourKing = pos.square<KING>(us);

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

    // Development lead in early/middlegame (simplified: count developed minor pieces)
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

    // Cap compensation to avoid overestimating
    // For positional sacrifices (materialSacrificed == 0), use a fixed cap
    int cap = materialSacrificed > 0 ? materialSacrificed * 3 : 80;
    return std::min(compensation, cap);
}

struct PVMoves {
    Move        moves[MAX_PLY + 1];
    std::size_t length = 0;

    Move*       begin() { return moves; }
    const Move* begin() const { return moves; }
    Move*       end() { return moves + length; }
    const Move* end() const { return moves + length; }

    Move&       operator[](std::size_t index) { return moves[index]; }
    const Move& operator[](std::size_t index) const { return moves[index]; }

    bool        empty() const { return length == 0; }
    std::size_t size() const { return length; }

    void clear() { length = 0; }

    void push_back(Move move) {
        assert(length < MAX_PLY + 1);
        moves[length++] = move;
    }

    void resize(std::size_t newSize) {
        assert(newSize <= length);
        length = newSize;
    }

    void update(Move move, const PVMoves* childPv) {
        assert(childPv == nullptr || childPv->size() <= MAX_PLY);
        length = childPv ? childPv->length : 0;

        if (childPv)
        {
            std::memcpy(moves + 1, childPv->moves, length * sizeof(Move));
        }

        moves[0] = move;
        ++length;
    }
};


// RootMove struct is used for moves at the root of the tree. For each root move
// we store a score and a PV (really a refutation in the case of moves which
// fail low). Score is normally set at -VALUE_INFINITE for all non-pv moves.
struct RootMove {

    explicit RootMove(Move m) { pv.push_back(m); }
    bool extract_ponder_from_tt(const TranspositionTable& tt, Position& pos);
    bool score_is_bound() const { return scoreLowerbound || scoreUpperbound; }
    void unset_bound_flags() { scoreLowerbound = scoreUpperbound = false; }
    bool operator==(const Move& m) const { return pv[0] == m; }
    // Sort in descending order
    bool operator<(const RootMove& m) const {
        return m.score != score ? m.score < score : m.previousScore < previousScore;
    }

    uint64_t effort           = 0;
    Value    score            = -VALUE_INFINITE;
    Value    previousScore    = -VALUE_INFINITE;
    Value    averageScore     = -VALUE_INFINITE;
    Value    meanSquaredScore = -VALUE_INFINITE * VALUE_INFINITE;
    Value    uciScore         = -VALUE_INFINITE;
    bool     scoreLowerbound  = false;
    bool     scoreUpperbound  = false;
    int      selDepth         = 0;
    int      tbRank           = 0;
    Value    tbScore;
    PVMoves  pv;
};

using RootMoves = std::vector<RootMove>;


// LimitsType struct stores information sent by the caller about the analysis required.
struct LimitsType {

    // Init explicitly due to broken value-initialization of non POD in MSVC
    LimitsType() {
        time[WHITE] = time[BLACK] = inc[WHITE] = inc[BLACK] = npmsec = movetime = TimePoint(0);
        movestogo = depth = mate = perft = infinite = 0;
        nodes                                       = 0;
        ponderMode                                  = false;
    }

    bool use_time_management() const { return time[WHITE] || time[BLACK]; }

    std::vector<std::string> searchmoves;
    TimePoint                time[COLOR_NB], inc[COLOR_NB], npmsec, movetime, startTime;
    int                      movestogo, depth, mate, perft, infinite;
    uint64_t                 nodes;
    bool                     ponderMode;
};


// The UCI stores the uci options, thread pool, and transposition table.
// This struct is used to easily forward data to the Search::Worker class.
struct SharedState {
    SharedState(const OptionsMap&                                        optionsMap,
                ThreadPool&                                              threadPool,
                TranspositionTable&                                      transpositionTable,
                std::map<NumaIndex, SharedHistories>&                    sharedHists,
                const LazyNumaReplicatedSystemWide<Eval::NNUE::Network>& net) :
        options(optionsMap),
        threads(threadPool),
        tt(transpositionTable),
        sharedHistories(sharedHists),
        network(net) {}

    const OptionsMap&                                        options;
    ThreadPool&                                              threads;
    TranspositionTable&                                      tt;
    std::map<NumaIndex, SharedHistories>&                    sharedHistories;
    const LazyNumaReplicatedSystemWide<Eval::NNUE::Network>& network;
};

class Worker;

// Null Object Pattern, implement a common interface for the SearchManagers.
// A Null Object will be given to non-mainthread workers.
class ISearchManager {
   public:
    virtual ~ISearchManager() {}
    virtual void check_time(Search::Worker&) = 0;
};

struct InfoShort {
    int   depth;
    Score score;
};

struct InfoFull: InfoShort {
    int              selDepth;
    size_t           multiPV;
    std::string_view wdl;
    std::string_view bound;
    size_t           timeMs;
    size_t           nodes;
    size_t           nps;
    size_t           tbHits;
    std::string_view pv;
    int              hashfull;
};

struct InfoIteration {
    int              depth;
    std::string_view currmove;
    size_t           currmovenumber;
};

// Skill structure is used to implement strength limit. If we have a UCI_Elo,
// we convert it to an appropriate skill level, anchored to the Stash engine.
// This method is based on a fit of the Elo results for games played between
// Stockfish at various skill levels and various versions of the Stash engine.
// Skill 0 .. 19 now covers CCRL Blitz Elo from 1320 to 3190, approximately
// Reference: https://github.com/vondele/Stockfish/commit/a08b8d4e9711c2
struct Skill {
    // Lowest and highest Elo ratings used in the skill level calculation
    constexpr static int LowestElo  = 1320;
    constexpr static int HighestElo = 3190;

    Skill(int skill_level, int uci_elo) {
        if (uci_elo)
        {
            double e = double(uci_elo - LowestElo) / (HighestElo - LowestElo);
            level = std::clamp((((37.2473 * e - 40.8525) * e + 22.2943) * e - 0.311438), 0.0, 19.0);
        }
        else
            level = double(skill_level);
    }
    bool enabled() const { return level < 20.0; }
    bool time_to_pick(Depth depth) const { return depth == 1 + int(level); }
    Move pick_best(const RootMoves&, size_t multiPV);

    double level;
    Move   best = Move::none();
};

// SearchManager manages the search from the main thread. It is responsible for
// keeping track of the time, and storing data strictly related to the main thread.
class SearchManager: public ISearchManager {
   public:
    using UpdateShort    = std::function<void(const InfoShort&)>;
    using UpdateFull     = std::function<void(const InfoFull&)>;
    using UpdateIter     = std::function<void(const InfoIteration&)>;
    using UpdateBestmove = std::function<void(std::string_view, std::string_view)>;

    struct UpdateContext {
        UpdateShort    onUpdateNoMoves;
        UpdateFull     onUpdateFull;
        UpdateIter     onIter;
        UpdateBestmove onBestmove;
    };


    SearchManager(const UpdateContext& updateContext) :
        updates(updateContext) {}

    void check_time(Search::Worker& worker) override;

    void pv(Search::Worker&           worker,
            const ThreadPool&         threads,
            const TranspositionTable& tt,
            Depth                     depth);

    Stockfish::TimeManagement tm;
    double                    originalTimeAdjust;
    int                       callsCnt;
    std::atomic_bool          ponder;

    std::array<Value, 4> iterValue;
    double               previousTimeReduction;
    Value                bestPreviousScore;
    Value                bestPreviousAverageScore;
    bool                 stopOnPonderhit;

    size_t id;

    const UpdateContext& updates;
};

class NullSearchManager: public ISearchManager {
   public:
    void check_time(Search::Worker&) override {}
};

// Search::Worker is the class that does the actual search.
// It is instantiated once per thread, and it is responsible for keeping track
// of the search history, and storing data required for the search.
class Worker {
   public:
    Worker(SharedState&,
           std::unique_ptr<ISearchManager>,
           size_t,
           size_t,
           size_t,
           NumaReplicatedAccessToken);

    // Called at instantiation to initialize reductions tables.
    // Reset histories, usually before a new game.
    void clear();

    // Called when the program receives the UCI 'go' command.
    // It searches from the root position and outputs the "bestmove".
    void start_searching();

    bool is_mainthread() const { return threadIdx == 0; }

    void ensure_network_replicated();

    // Public because they need to be updatable by the stats
    ButterflyHistory mainHistory;
    LowPlyHistory    lowPlyHistory;

    CapturePieceToHistory           captureHistory;
    ContinuationHistory             continuationHistory[2][2];
    CorrectionHistory<Continuation> continuationCorrectionHistory;

    TTMoveHistory    ttMoveHistory;
    SharedHistories& sharedHistory;

   private:
    bool iterative_deepening();

    void do_move(Position& pos, const Move move, StateInfo& st, Stack* const ss);
    void
    do_move(Position& pos, const Move move, StateInfo& st, const bool givesCheck, Stack* const ss);
    void do_null_move(Position& pos, StateInfo& st, Stack* const ss);
    void undo_move(Position& pos, const Move move);
    void undo_null_move(Position& pos);

    // This is the main search function, for both PV and non-PV nodes
    Value search(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode, NodeType nodeType);

    // Quiescence search function, which is called by the main search
    Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta, NodeType nodeType);

    int reduction(bool i, Depth d, int mn, int delta) const;

    // LMRC: Context-aware LMR reduction
    int contextual_reduction(const Position& pos, const Stack* ss, bool improving, Depth depth, int moveCount, int delta, int history, int threatCat, bool ttPv) const;

    // Pointer to the search manager, only allowed to be called by the main thread
    SearchManager* main_manager() const {
        assert(threadIdx == 0);
        return static_cast<SearchManager*>(manager.get());
    }

    TimePoint elapsed() const;
    TimePoint elapsed_time() const;

    Value evaluate(const Position&);

    LimitsType limits;

    size_t                pvIdx, pvLast;
    std::atomic<uint64_t> nodes, tbHits, bestMoveChanges;
    int                   selDepth, nmpMinPly;

    Value optimism[COLOR_NB];

    Position  rootPos;
    StateInfo rootState;
    RootMoves rootMoves;
    Depth     rootDepth;
    Value     rootDelta;

    PVMoves lastIterationPV;

    size_t                    threadIdx, numaThreadIdx, numaTotal;
    NumaReplicatedAccessToken numaAccessToken;

    // Reductions lookup table initialized at startup
    std::array<int, MAX_MOVES> reductions;  // [depth or moveNumber]

    // The main thread has a SearchManager, the others have a NullSearchManager
    std::unique_ptr<ISearchManager> manager;

    Tablebases::Config tbConfig;

    const OptionsMap&                                        options;
    ThreadPool&                                              threads;
    TranspositionTable&                                      tt;
    const LazyNumaReplicatedSystemWide<Eval::NNUE::Network>& network;

    // Used by NNUE
    Eval::NNUE::AccumulatorStack  accumulatorStack;
    Eval::NNUE::AccumulatorCaches refreshTable;

    friend class Stockfish::ThreadPool;
    friend class SearchManager;
};

struct ConthistBonus {
    int index;
    int weight;
};


}  // namespace Search

}  // namespace Stockfish

#endif  // #ifndef SEARCH_H_INCLUDED
