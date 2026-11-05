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

// IAMM — Incremental Attack Map Maintenance
// Maintains attack information incrementally to avoid full recalculation

#ifndef GAMBO_IAMM_H_INCLUDED
#define GAMBO_IAMM_H_INCLUDED

#include <array>
#include <cstdint>

#include "../bitboard.h"
#include "../position.h"
#include "../types.h"

namespace Gambo::IAMM {

using namespace Stockfish;

struct AttackMap {
    // Per-square attack info
    std::array<Bitboard, SQUARE_NB> attackers;       // pieces attacking this square
    std::array<Bitboard, SQUARE_NB> attacked_by_w;   // squares attacked by white
    std::array<Bitboard, SQUARE_NB> attacked_by_b;   // squares attacked by black
    std::array<Bitboard, SQUARE_NB> pinners;         // pieces pinning to this square
    std::array<Bitboard, SQUARE_NB> discovered;      // discovered attacks through this square

    // King-specific
    Bitboard white_king_attackers;
    Bitboard black_king_attackers;
    Bitboard white_king_zone;
    Bitboard black_king_zone;

    // Pin rays
    std::array<Bitboard, SQUARE_NB> pin_rays;

    AttackMap() = default;

    void clear() {
        for (auto& bb : attackers) bb = 0;
        for (auto& bb : attacked_by_w) bb = 0;
        for (auto& bb : attacked_by_b) bb = 0;
        for (auto& bb : pinners) bb = 0;
        for (auto& bb : discovered) bb = 0;
        for (auto& bb : pin_rays) bb = 0;
        white_king_attackers = 0;
        black_king_attackers = 0;
        white_king_zone = 0;
        black_king_zone = 0;
    }
};

class AttackMapManager {
public:
    AttackMapManager() = default;

    void init(const Position& pos);
    void update_make_move(const Position& pos, Move m);
    void update_unmake_move(const Position& pos, Move m);
    void full_recalc(const Position& pos);

    const AttackMap& get_map() const { return attack_map; }

    // Query helpers
    Bitboard attackers_to(const Position& pos, Square s, Color c) const;
    Bitboard attacked_by(Color c) const;
    bool is_pinned(Square s, Color c) const;
    Bitboard pin_ray(Square s) const;
    Bitboard king_zone(Color c) const;
    Bitboard king_attackers(Color c) const;

private:
    AttackMap attack_map;
    std::array<AttackMap, 256> history;  // for undo support (indexed by ply)

    void add_piece_attacks(const Position& pos, Square from, Piece pc, bool adding);
    void update_slider_attacks(const Position& pos, Square from, Square to, PieceType pt, bool adding);
    void update_pawn_attacks(const Position& pos, Square from, Color c, bool adding);
    void update_king_attacks(const Position& pos, Square from, Color c, bool adding);
    void recalc_pins(const Position& pos);
    void recalc_king_zones(const Position& pos);
};

}  // namespace Gambo::IAMM

#endif  // GAMBO_IAMM_H_INCLUDED