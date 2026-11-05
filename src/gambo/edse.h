/*
  Gambo, a UCI chess playing engine with an aggressive style.
  Copyright (C) 2024-2026 The Gambo developers.

  Gambo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// EDSE — Event-Driven Search Extensions
// Extensions triggered by significant search events

#ifndef GAMBO_EDSE_H_INCLUDED
#define GAMBO_EDSE_H_INCLUDED

#include <array>
#include <cstdint>

#include "../position.h"
#include "../search.h"
#include "../types.h"

namespace Gambo::EDSE {

using namespace Stockfish;

enum class ExtensionEvent {
    NONE = 0,
    CRITICAL_CHECK = 1,        // Check that threatens mate or major material
    SINGULAR_TACTICAL = 2,     // Only one good capture/move
    MAJOR_THREAT = 3,          // New major threat detected
    FORCED_RESPONSE = 4,       // Opponent has only one legal reply
    TACTICAL_DISCOVERY = 5,    // Discovered attack/check
    PROMOTION_THREAT = 6,      // Pawn about to promote
    KING_SAFETY_CRISIS = 7,    // King exposure suddenly increases
};

struct EventInfo {
    ExtensionEvent event;
    Square trigger_square;
    Move trigger_move;
    int severity;  // 1-5
    int ply;
};

struct EDSEConfig {
    int max_extensions_per_node = 2;
    int max_total_extensions = 8;
    int critical_check_extension = 1;
    int singular_tactical_extension = 1;
    int major_threat_extension = 1;
    int forced_response_extension = 1;
    int tactical_discovery_extension = 1;
    int promotion_threat_extension = 1;
    int king_safety_extension = 1;
};

class EventDrivenExtensions {
public:
    EventDrivenExtensions() = default;

    // Check for extension events and return extension amount
    int check_events(const Position& pos, const Search::Stack* ss, Move move, int depth, int ply);

    // Called after move is made to detect new events
    void on_move_made(const Position& pos, const Search::Stack* ss, Move move, int ply);

    // Configuration
    void set_config(const EDSEConfig& cfg) { config = cfg; }
    const EDSEConfig& get_config() const { return config; }

    // Statistics
    struct Stats {
        int critical_checks = 0;
        int singular_tactical = 0;
        int major_threats = 0;
        int forced_responses = 0;
        int tactical_discoveries = 0;
        int promotion_threats = 0;
        int king_safety_crisis = 0;
        int total_extensions = 0;
    };
    const Stats& get_stats() const { return stats; }
    void reset_stats() { stats = {}; }

private:
    EDSEConfig config;
    Stats stats;
    std::array<EventInfo, 256> event_history;
    int event_count = 0;

    int check_critical_check(const Position& pos, Move move) const;
    int check_singular_tactical(const Position& pos, Move move) const;
    int check_major_threat(const Position& pos, const Search::Stack* ss, Move move) const;
    int check_forced_response(const Position& pos, Move move) const;
    int check_tactical_discovery(const Position& pos, Move move) const;
    int check_promotion_threat(const Position& pos, Move move) const;
    int check_king_safety_crisis(const Position& pos, Move move) const;
};

}  // namespace Gambo::EDSE

#endif  // GAMBO_EDSE_H_INCLUDED