/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// PTC — Persistent Threat Context
// Tracks threats and obligations across variations

#ifndef GAMBO_PTC_H_INCLUDED
#define GAMBO_PTC_H_INCLUDED

#include <array>
#include <cstdint>
#include <vector>

#include "../bitboard.h"
#include "../position.h"
#include "../types.h"

namespace Gambo::PTC {

using namespace Stockfish;

struct ThreatInfo {
    Square target_square;
    PieceType target_piece;
    Bitboard attackers;
    Bitboard defenders;
    int threat_level;  // 1-5
    int move_count_to_execute;  // moves until threat executes
    bool is_forcing;
};

struct ObligationInfo {
    Square piece_square;
    Square destination_square;
    int obligation_type;  // 1=defend, 2=block, 3=move king, 4=capture
    int urgency;  // 1-5
    Move forced_move;
};

struct ThreatContext {
    std::vector<ThreatInfo> threats[COLOR_NB];
    std::vector<ObligationInfo> obligations[COLOR_NB];
    Bitboard threatened_pieces[COLOR_NB];
    Bitboard defended_pieces[COLOR_NB];
    int generation;
    int ply;

    ThreatContext() : generation(0), ply(0) {
        clear();
    }

    void clear() {
        for (auto& v : threats) v.clear();
        for (auto& v : obligations) v.clear();
        for (auto& bb : threatened_pieces) bb = 0;
        for (auto& bb : defended_pieces) bb = 0;
        generation = 0;
        ply = 0;
    }
};

class ThreatContextManager {
public:
    ThreatContextManager() = default;

    void init(const Position& pos);
    void update_make_move(const Position& pos, Move m);
    void update_unmake_move(const Position& pos, Move m);
    void full_recalc(const Position& pos);

    const ThreatContext& get_context() const { return context; }

    // Queries
    bool has_threat_on(Square s, Color c) const;
    ThreatInfo get_threat_on(Square s, Color c) const;
    bool has_obligation_for(Square s, Color c) const;
    ObligationInfo get_obligation_for(Square s, Color c) const;
    Bitboard all_threats(Color c) const;
    Bitboard all_obligations(Color c) const;

private:
    ThreatContext context;
    std::array<ThreatContext, 256> history;

    void calc_threats(const Position& pos);
    void calc_obligations(const Position& pos);
};

}  // namespace Gambo::PTC

#endif  // GAMBO_PTC_H_INCLUDED