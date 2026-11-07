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

// EDSE - Event-Driven Search Extensions
// Event-driven extension system for tactical awareness

#ifndef GAMBO_EDSE_H_INCLUDED
#define GAMBO_EDSE_H_INCLUDED

#include <array>
#include <cstdint>
#include <functional>

#include "../types.h"
#include "../position.h"
#include "../search.h"

namespace Stockfish {
class Position;
class Stack;

namespace EDSE {

// Extension event types
enum class ExtensionEvent : uint8_t {
    NONE = 0,
    
    // Tactical events
    CHECK = 1,
    CAPTURE = 2,
    PROMOTION = 3,
    EN_PASSANT = 4,
    CASTLING = 5,
    
    // Threat events
    NEW_THREAT = 6,
    MATE_THREAT = 7,
    FORCING_MOVE = 8,
    
    // Positional events
    PAWN_BREAK = 9,
    PIECE_SACRIFICE = 10,
    KING_EXPOSURE = 11,
    PIN_CREATED = 12,
    FORK_CREATED = 13,
    DISCOVERED_ATTACK = 14,
    
    // Structural events
    PAWN_STRUCTURE_CHANGE = 15,
    KING_SAFETY_CHANGE = 16,
    PIECE_ACTIVITY_CHANGE = 17,
    
    // Meta events
    PV_CHANGE = 18,
    SCORE_SWING = 19,
    DEPTH_MILESTONE = 20,
    TIME_TROUBLE = 21,
    
    NUM_EVENTS = 22
};

// Event data structure
struct ExtensionEventData {
    ExtensionEvent type = ExtensionEvent::NONE;
    Move triggeringMove = Move::none();
    Square from = SQ_NONE;
    Square to = SQ_NONE;
    Piece piece = NO_PIECE;
    Color side = WHITE;
    int severity = 0;      // 0-255
    int depth = 0;         // Depth where event occurred
    Value scoreDelta = 0;  // Score change
};

// Extension rule
struct ExtensionRule {
    ExtensionEvent event;
    std::function<int(const Position&, const Stack*, const ExtensionEventData&)> condition;
    int extensionAmount;  // In plies (1 = extend by 1 ply)
    int maxExtensionsPerPath = 3;  // Max times this rule can fire per path
    bool requiresPvNode = false;
    bool onlyInCheck = false;
};

// Event-driven extension engine
class ExtensionEngine {
public:
    ExtensionEngine();
    
    // Register an extension rule
    void add_rule(const ExtensionRule& rule);
    
    // Process events and return total extension amount
    int process_events(const Position& pos, const Stack* ss, 
                       const std::array<ExtensionEventData, 32>& events, int numEvents);
    
    // Check if we can extend further
    bool can_extend(const Stack* ss, int currentExtensions) const;
    
    // Get statistics
    struct Stats {
        std::uint64_t totalExtensions = 0;
        std::uint64_t extensionsByEvent[22] = {};
        int maxExtensionsInPath = 0;
    };
    
    const Stats& get_stats() const { return stats; }
    
    // Reset per-search state
    void reset();

private:
    std::array<ExtensionRule, 32> rules;
    int numRules = 0;
    
    struct PathExtensionTracker {
        int extensions = 0;
        std::array<int, 22> eventCounts = {};
    };
    
    std::array<PathExtensionTracker, 256> pathTrackers;  // Per-thread tracking
    
    struct Stats {
        std::uint64_t totalExtensions = 0;
        std::uint64_t extensionsByEvent[22] = {};
        int maxExtensionsInPath = 0;
    } stats;
    
    // Check if rule condition is met
    bool check_condition(const ExtensionRule& rule, const Position& pos, const Stack* ss,
                         const ExtensionEventData& event) const;
    
    // Get path index from stack
    int get_path_index(const Stack* ss) const;
};

// Global extension engine
extern ExtensionEngine edseEngine;

// Built-in event detectors
namespace Detectors {

// Detect check events
void detect_check_events(const Position& pos, const Stack* ss, Move move,
                         std::array<ExtensionEventData, 32>& events, int& numEvents);

// Detect capture events
void detect_capture_events(const Position& pos, const Stack* ss, Move move,
                           std::array<ExtensionEventData, 32>& events, int& numEvents);

// Detect promotion events
void detect_promotion_events(const Position& pos, const Stack* ss, Move move,
                             std::array<ExtensionEventData, 32>& events, int& numEvents);

// Detect mate threats
void detect_mate_threats(const Position& pos, const Stack* ss,
                         std::array<ExtensionEventData, 32>& events, int& numEvents);

// Detect forcing moves
void detect_forcing_moves(const Position& pos, const Stack* ss,
                          std::array<ExtensionEventData, 32>& events, int& numEvents);

// Detect pawn breaks
void detect_pawn_breaks(const Position& pos, const Stack* ss, Move move,
                        std::array<ExtensionEventData, 32>& events, int& numEvents);

// Detect piece sacrifices
void detect_piece_sacrifices(const Position& pos, const Stack* ss, Move move,
                             std::array<ExtensionEventData, 32>& events, int& numEvents);

// Detect king exposure
void detect_king_exposure(const Position& pos, const Stack* ss,
                          std::array<ExtensionEventData, 32>& events, int& numEvents);

// Detect pins/forks
void detect_tactical_patterns(const Position& pos, const Stack* ss, Move move,
                              std::array<ExtensionEventData, 32>& events, int& numEvents);

// Detect score swings
void detect_score_swings(const Position& pos, const Stack* ss, Value prevScore, Value newScore,
                         std::array<ExtensionEventData, 32>& events, int& numEvents);

}  // namespace Detectors

// Initialize EDSE subsystem
void init_edse();

// Process all events for a move
void process_move_events(const Position& pos, const Stack* ss, Move move, Value scoreDelta);

// Get total extension for a move
int get_move_extension(const Position& pos, const Stack* ss, Move move, Value scoreDelta);

}  // namespace EDSE
}  // namespace Stockfish

#endif  // GAMBO_EDSE_H_INCLUDED