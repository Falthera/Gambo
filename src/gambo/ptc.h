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

// PTC - Persistent Threat Context
// Structured threat context tracking across variations

#ifndef GAMBO_PTC_H_INCLUDED
#define GAMBO_PTC_H_INCLUDED

#include <array>
#include <cstdint>
#include <vector>

#include "../types.h"
#include "../position.h"
#include "../bitboard.h"

namespace Stockfish {
class Position;
class Stack;

namespace PTC {

// Threat types
enum class ThreatType : uint8_t {
    NONE = 0,
    DIRECT_ATTACK = 1,        // Piece directly attacks target
    DISCOVERED_ATTACK = 2,    // Move uncovers attack
    DOUBLE_ATTACK = 3,        // Two pieces attack same target
    PIN = 4,                  // Piece pinned to king
    SKEWER = 5,               // High-value piece behind lower
    FORK = 6,                 // Single piece attacks multiple
    MATE_THREAT = 7,          // Forced mate sequence
    PAWN_PROMOTION = 8,       // Pawn about to promote
    BACK_RANK = 9,            // Back rank mate threat
    SMOTHERED_MATE = 10,      // Smothered mate pattern
    TRAPPED_PIECE = 11,       // Piece with no escape
    KING_EXPOSURE = 12,       // King in danger
    WEAK_PAWN = 13,           // Isolated/backward pawn
    WEAK_SQUARE = 14,         // Unprotected square
    OUTPOST = 15,             // Knight outpost
    OPEN_FILE = 16,           // Rook on open file
    SEVENTH_RANK = 17,        // Rook on 7th rank
    BATTERY = 18,             // Queen+bishop or rook+rook
    XRAY = 19                 // X-ray attack through piece
};

// Threat descriptor
struct Threat {
    ThreatType type = ThreatType::NONE;
    Square targetSquare = SQ_NONE;
    Square attackerSquare = SQ_NONE;
    Piece attacker = NO_PIECE;
    Piece target = NO_PIECE;
    Color attackerColor = WHITE;
    int severity = 0;           // 0-255
    int plyCreated = 0;
    int plyExpires = 0;
    bool persistent = false;    // Persists across variations
    Bitboard attackRay = 0;     // Ray of attack
    std::vector<Move> defendingMoves;  // Moves that neutralize threat
};

// Threat context for a position
struct ThreatContext {
    std::array<Threat, 64> threats;  // Max 64 active threats
    int numThreats = 0;
    int maxSeverity = 0;
    Color mostThreatenedColor = WHITE;
    Bitboard threatenedSquares = 0;
    Bitboard defenderSquares = 0;
    int plyLastUpdated = 0;
    
    // Persistent threat tracking across variations
    struct PersistentThreat {
        Threat threat;
        int variationId = 0;
        int persistenceCount = 0;  // How many variations this persisted
    };
    std::vector<PersistentThreat> persistentThreats;
    int nextVariationId = 1;
};

// Global threat context
extern ThreatContext threatContext;

// Initialize PTC subsystem
void init_ptc();

// Update threat context for a move
void update_threat_context(const Position& pos, const Stack* ss, Move move, int ply);

// Get current threat context
const ThreatContext& get_threat_context();

// Detect threats in position
void detect_threats(const Position& pos, int ply);

// Evaluate threat severity
int evaluate_threat_severity(const Threat& threat, const Position& pos);

// Get persistent threats
const std::vector<ThreatContext::PersistentThreat>& get_persistent_threats();

// Start new variation (for search tree)
int start_new_variation();

// End current variation
void end_variation(int variationId);

// Get threats for a specific square
std::vector<Threat> get_threats_on_square(Square sq, Color perspective);

// Check if square is threatened
bool is_square_threatened(Square sq, Color byColor);

// Get most severe threat
const Threat* get_most_severe_threat(Color perspective);

// Clear threat context
void clear_threat_context();

// Statistics
struct PTCStats {
    std::uint64_t totalThreatsDetected = 0;
    std::uint64_t persistentThreatsTracked = 0;
    std::uint64_t threatsNeutralized = 0;
    int maxConcurrentThreats = 0;
};

const PTCStats& get_ptc_stats();

}  // namespace PTC
}  // namespace Stockfish

#endif  // GAMBO_PTC_H_INCLUDED