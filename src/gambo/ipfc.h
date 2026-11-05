/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// IPFC — Incremental Position Feature Cache
// Caches reusable position information: attack maps, king zones, pins, pawn structures, etc.

#ifndef GAMBO_IPFC_H_INCLUDED
#define GAMBO_IPFC_H_INCLUDED

#include <array>
#include <cstdint>

#include "../bitboard.h"
#include "../position.h"
#include "../types.h"
#include "iamm.h"

namespace Gambo::IPFC {

using namespace Stockfish;

struct PawnStructureCache {
    Bitboard passed_pawns[COLOR_NB];
    Bitboard isolated_pawns[COLOR_NB];
    Bitboard backward_pawns[COLOR_NB];
    Bitboard doubled_pawns[COLOR_NB];
    Bitboard pawn_islands[COLOR_NB];
    Bitboard candidate_pawns[COLOR_NB];
    Bitboard pawn_attacks[COLOR_NB];
    uint8_t pawn_count[COLOR_NB];
    uint16_t pawn_score[COLOR_NB];  // weighted pawn structure score
};

struct KingSafetyCache {
    Bitboard king_zone[COLOR_NB];
    Bitboard king_attackers[COLOR_NB];
    Bitboard king_defenders[COLOR_NB];
    uint16_t king_safety_score[COLOR_NB];
    bool in_check[COLOR_NB];
    Bitboard checkers[COLOR_NB];
    Bitboard pinned_pieces[COLOR_NB];
    std::array<Bitboard, SQUARE_NB> pin_rays;
};

struct MobilityCache {
    uint8_t knight_mobility[COLOR_NB];
    uint8_t bishop_mobility[COLOR_NB];
    uint8_t rook_mobility[COLOR_NB];
    uint8_t queen_mobility[COLOR_NB];
    Bitboard knight_targets[COLOR_NB];
    Bitboard bishop_targets[COLOR_NB];
    Bitboard rook_targets[COLOR_NB];
    Bitboard queen_targets[COLOR_NB];
};

struct ThreatCache {
    Bitboard tactical_threats[COLOR_NB];
    Bitboard hanging_pieces[COLOR_NB];
    Bitboard defended_pieces[COLOR_NB];
    Bitboard xray_attacks[COLOR_NB];
    uint16_t threat_score[COLOR_NB];
};

struct PositionFeatureCache {
    PawnStructureCache pawn_structure;
    KingSafetyCache king_safety;
    MobilityCache mobility;
    ThreatCache threats;
    IAMM::AttackMap attack_map;

    uint32_t generation;  // for cache validation
    bool valid;

    PositionFeatureCache() : generation(0), valid(false) {}

    void clear() {
        valid = false;
        generation = 0;
    }
};

class PositionFeatureCacheManager {
public:
    PositionFeatureCacheManager() = default;

    void init(const Position& pos);
    void update_make_move(const Position& pos, Move m);
    void update_unmake_move(const Position& pos, Move m);
    void full_recalc(const Position& pos);

    const PositionFeatureCache& get_cache() const { return cache; }

    // Query helpers
    const PawnStructureCache& pawn_structure() const { return cache.pawn_structure; }
    const KingSafetyCache& king_safety() const { return cache.king_safety; }
    const MobilityCache& mobility() const { return cache.mobility; }
    const ThreatCache& threats() const { return cache.threats; }
    const IAMM::AttackMap& attack_map() const { return cache.attack_map; }

    // Specific queries
    Bitboard passed_pawns(Color c) const { return cache.pawn_structure.passed_pawns[c]; }
    Bitboard king_zone(Color c) const { return cache.king_safety.king_zone[c]; }
    Bitboard king_attackers(Color c) const { return cache.king_safety.king_attackers[c]; }
    Bitboard tactical_threats(Color c) const { return cache.threats.tactical_threats[c]; }
    bool is_pinned(Square s) const { return cache.king_safety.pinned_pieces[WHITE] & square_bb(s) || cache.king_safety.pinned_pieces[BLACK] & square_bb(s); }
    Bitboard pin_ray(Square s) const { return cache.king_safety.pin_rays[s]; }

private:
    PositionFeatureCache cache;
    IAMM::AttackMapManager attack_manager;

    void calc_pawn_structure(const Position& pos);
    void calc_king_safety(const Position& pos);
    void calc_mobility(const Position& pos);
    void calc_threats(const Position& pos);
    void update_pawn_structure_make(const Position& pos, Move m);
    void update_pawn_structure_unmake(const Position& pos, Move m);
    void update_king_safety_make(const Position& pos, Move m);
    void update_mobility_make(const Position& pos, Move m);
    void update_threats_make(const Position& pos, Move m);
};

}  // namespace Gambo::IPFC

#endif  // GAMBO_IPFC_H_INCLUDED