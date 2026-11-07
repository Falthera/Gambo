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

// EDSE - Event-Driven Search Extensions implementation

#include "edse.h"
#include "position.h"
#include "search.h"
#include "evaluate.h"
#include "bitboard.h"

namespace Stockfish {
namespace EDSE {

ExtensionEngine edseEngine;

ExtensionEngine::ExtensionEngine() : numRules(0) {
    // Initialize default rules
    
    // Rule 1: Check extension (standard)
    add_rule({
        ExtensionEvent::CHECK,
        [](const Position& pos, const Stack* ss, const ExtensionEventData& event) {
            return pos.gives_check(event.triggeringMove, pos.checkers());
        },
        1,  // 1 ply extension
        3,
        false,
        true
    });
    
    // Rule 2: Mate threat extension
    add_rule({
        ExtensionEvent::MATE_THREAT,
        [](const Position& pos, const Stack* ss, const ExtensionEventData& event) {
            return event.severity > 100;
        },
        2,
        2,
        false,
        false
    });
    
    // Rule 3: Forcing move extension
    add_rule({
        ExtensionEvent::FORCING_MOVE,
        [](const Position& pos, const Stack* ss, const ExtensionEventData& event) {
            return event.severity > 50;
        },
        1,
        4,
        false,
        false
    );
    
    // Rule 4: Piece sacrifice extension
    add_rule({
        ExtensionEvent::PIECE_SACRIFICE,
        [](const Position& pos, const Stack* ss, const ExtensionEventData& event) {
            return event.severity > 50;
        },
        1,
        2,
        true,
        false
    });
    
    // Rule 5: King exposure
    add_rule({
        ExtensionEvent::KING_EXPOSURE,
        [](const Position& pos, const Stack* ss, const ExtensionEventData& event) {
            return event.severity > 80;
        },
        1,
        2,
        false,
        false
    );
    
    // Rule 6: Pawn break
    add_rule({
        ExtensionEvent::PAWN_BREAK,
        [](const Position& pos, const Stack* ss, const ExtensionEventData& event) {
            return event.severity > 30;
        },
        1,
        2,
        false,
        false
    );
    
    // Rule 7: Tactical patterns (pins, forks)
    add_rule({
        ExtensionEvent::FORK_CREATED,
        [](const Position& pos, const Stack* ss, const ExtensionEventData& event) {
            return event.severity > 60;
        },
        1,
        2,
        false,
        false
    });
    
    // Rule 8: Discovered attack
    add_rule({
        ExtensionEvent::DISCOVERED_ATTACK,
        [](const Position& pos, const Stack* ss, const ExtensionEventData& event) {
            return event.severity > 40;
        },
        1,
        2,
        false,
        false
    });
    
    numRules = 8;
}

void ExtensionEngine::add_rule(const ExtensionRule& rule) {
    if (numRules < 32) {
        rules[numRules++] = rule;
    }
}

int ExtensionEngine::process_events(const Position& pos, const Stack* ss, 
                                    const std::array<ExtensionEventData, 32>& events, int numEvents) {
    int totalExtension = 0;
    int pathIdx = get_path_index(ss);
    
    for (int i = 0; i < numEvents; ++i) {
        const auto& event = events[i];
        
        for (int r = 0; r < numRules; ++r) {
            const auto& rule = rules[r];
            
            if (rule.event != event.type) continue;
            
            // Check conditions
            if (rule.onlyInCheck && !pos.gives_check(event.triggeringMove, pos.checkers())) continue;
            if (rule.requiresPvNode && !ss->followPV) continue;
            
            // Check path limit
            if (pathTrackers[pathIdx].eventCounts[static_cast<int>(event.type)] >= rule.maxExtensionsPerPath) continue;
            
            // Check condition
            if (!check_condition(rule, pos, ss, event)) continue;
            
            // Apply extension
            totalExtension += rule.extensionAmount;
            pathTrackers[pathIdx].extensions += rule.extensionAmount;
            pathTrackers[pathIdx].eventCounts[static_cast<int>(event.type)]++;
            
            stats.totalExtensions++;
            stats.extensionsByEvent[static_cast<int>(event.type)]++;
            stats.maxExtensionsInPath = std::max(stats.maxExtensionsInPath, pathTrackers[pathIdx].extensions);
        }
    }
    
    return totalExtension;
}

bool ExtensionEngine::can_extend(const Stack* ss, int currentExtensions) const {
    // Maximum 8 extensions per path
    return currentExtensions < 8;
}

bool ExtensionEngine::check_condition(const ExtensionRule& rule, const Position& pos, const Stack* ss,
                                      const ExtensionEventData& event) const {
    if (rule.condition) {
        return rule.condition(pos, ss, event);
    }
    return true;
}

int ExtensionEngine::get_path_index(const Stack* ss) const {
    // Simple hash of stack pointer for path tracking
    return (reinterpret_cast<uintptr_t>(ss) >> 4) & 0xFF;
}

void ExtensionEngine::reset() {
    for (auto& tracker : pathTrackers) {
        tracker.extensions = 0;
        tracker.eventCounts.fill(0);
    }
    stats = Stats{};
}

void ExtensionEngine::Stats::operator=(const Stats& other) {
    totalExtensions = other.totalExtensions;
    for (int i = 0; i < 22; ++i) extensionsByEvent[i] = other.extensionsByEvent[i];
    maxExtensionsInPath = other.maxExtensionsInPath;
}

// Built-in event detectors
namespace Detectors {

void detect_check_events(const Position& pos, const Stack* ss, Move move,
                         std::array<ExtensionEventData, 32>& events, int& numEvents) {
    if (pos.gives_check(move, pos.checkers())) {
        events[numEvents++] = {
            ExtensionEvent::CHECK,
            move,
            move.from_sq(),
            move.to_sq(),
            pos.moved_piece(move),
            pos.side_to_move(),
            100,  // High severity for checks
            0,
            0
        };
    }
}

void detect_capture_events(const Position& pos, const Stack* ss, Move move,
                           std::array<ExtensionEventData, 32>& events, int& numEvents) {
    if (pos.capture_stage(move)) {
        Piece captured = pos.piece_on(move.to_sq());
        int severity = PieceValue[type_of(captured)] / 50;  // Scale by piece value
        
        events[numEvents++] = {
            ExtensionEvent::CAPTURE,
            move,
            move.from_sq(),
            move.to_sq(),
            pos.moved_piece(move),
            pos.side_to_move(),
            std::min(severity, 255),
            0,
            0
        };
    }
}

void detect_promotion_events(const Position& pos, const Stack* ss, Move move,
                             std::array<ExtensionEventData, 32>& events, int& numEvents) {
    if (move.type_of() == PROMOTION) {
        events[numEvents++] = {
            ExtensionEvent::PROMOTION,
            move,
            move.from_sq(),
            move.to_sq(),
            pos.moved_piece(move),
            pos.side_to_move(),
            120,  // High severity for promotions
            0,
            0
        };
    }
}

void detect_mate_threats(const Position& pos, const Stack* ss,
                         std::array<ExtensionEventData, 32>& events, int& numEvents) {
    // Check if move creates mate threat
    MoveList<GenType::LEGAL> moves(pos);
    int mateThreats = 0;
    
    for (Move m : moves) {
        StateInfo st;
        bool givesCheck = pos.gives_check(m);
        pos.do_move(m, *new StateInfo(), givesCheck, {}, {}, nullptr, nullptr);
        
        if (pos.is_checkmate()) mateThreats++;
        pos.undo_move(m);
    }
    
    if (mateThreats > 0) {
        events[numEvents++] = {
            ExtensionEvent::MATE_THREAT,
            Move::none(),
            SQ_NONE, SQ_NONE,
            NO_PIECE,
            pos.side_to_move(),
            std::min(mateThreats * 50, 255),
            0,
            0
        };
    }
}

void detect_forcing_moves(const Position& pos, const Stack* ss,
                          std::array<ExtensionEventData, 32>& events, int& numEvents) {
    // Forcing moves: checks, captures, threats
    int forcingCount = 0;
    MoveList<GenType::LEGAL> moves(pos);
    
    for (Move m : moves) {
        if (pos.gives_check(m) || pos.capture_stage(m) || pos.see(m) > 200) {
            forcingCount++;
        }
    }
    
    if (forcingCount >= 3) {
        events[numEvents++] = {
            ExtensionEvent::FORCING_MOVE,
            Move::none(),
            SQ_NONE, SQ_NONE,
            NO_PIECE,
            pos.side_to_move(),
            std::min(forcingCount * 10, 255),
            0,
            0
        };
    }
}

void detect_pawn_breaks(const Position& pos, const Stack* ss, Move move,
                        std::array<ExtensionEventData, 32>& events, int& numEvents) {
    // Detect pawn moves that break pawn structure
    Piece pc = pos.moved_piece(move);
    if (type_of(pc) == PAWN) {
        Square from = move.from_sq();
        Square to = move.to_sq();
        
        // Check if pawn move opens lines or creates passed pawn
        Bitboard pawnAttacks = pos.attacks_by<PAWN>(pos.side_to_move());
        if (pawnAttacks & pos.pieces(~pos.side_to_move(), KING)) {
            events[numEvents++] = {
                ExtensionEvent::PAWN_BREAK,
                move,
                from, to,
                pc,
                pos.side_to_move(),
                80,
                0,
                0
            };
        }
    }
}

void detect_piece_sacrifices(const Position& pos, const Stack* ss, Move move,
                             std::array<ExtensionEventData, 32>& events, int& numEvents) {
    if (pos.capture_stage(move)) {
        Piece ourPiece = pos.moved_piece(move);
        Piece theirPiece = pos.piece_on(move.to_sq());
        
        int ourValue = PieceValue[ourPiece];
        int theirValue = PieceValue[theirPiece];
        
        if (ourValue > theirValue + PieceValue[PAWN]) {
            int severity = std::min((ourValue - theirValue) / 10, 255);
            events[numEvents++] = {
                ExtensionEvent::PIECE_SACRIFICE,
                move,
                move.from_sq(), move.to_sq(),
                ourPiece,
                pos.side_to_move(),
                severity,
                0,
                0
            };
        }
    }
}

void detect_king_exposure(const Position& pos, const Stack* ss,
                          std::array<ExtensionEventData, 32>& events, int& numEvents) {
    Color us = pos.side_to_move();
    Color them = ~us;
    Square theirKing = pos.square<KING>(them);
    
    // Check if move attacks king zone
    Bitboard kingZone = attacks_bb<KING>(theirKing) | square_bb(theirKing);
    if (pos.attacks_by<pos.moved_piece(move)>(move.to_sq()) & kingZone) {
        int severity = king_danger(pos, them);
        events[numEvents++] = {
            ExtensionEvent::KING_EXPOSURE,
            move,
            move.from_sq(), move.to_sq(),
            pos.moved_piece(move),
            us,
            severity,
            0,
            0
        };
    }
}

void detect_tactical_patterns(const Position& pos, const Stack* ss, Move move,
                              std::array<ExtensionEventData, 32>& events, int& numEvents) {
    // Detect forks
    Piece pc = pos.moved_piece(move);
    if (type_of(pc) == KNIGHT) {
        Bitboard knightAttacks = attacks_bb<KNIGHT>(move.to_sq());
        Bitboard targets = pos.pieces(~pos.side_to_move(), ROOK, BISHOP, QUEEN, KING);
        if (popcount(knightAttacks & targets) >= 2) {
            events[numEvents++] = {
                ExtensionEvent::FORK_CREATED,
                move,
                move.from_sq(), move.to_sq(),
                pc, pos.side_to_move(),
                100, 0, 0
            };
        }
    }
    
    // Detect pins
    if (type_of(pc) == BISHOP || type_of(pc) == ROOK || type_of(pc) == QUEEN) {
        // Simplified pin detection
        Bitboard line = attacks_bb(type_of(pc), move.to_sq(), pos.pieces());
        if (line & pos.pieces(~pos.side_to_move(), KING)) {
            events[numEvents++] = {
                ExtensionEvent::PIN_CREATED,
                move,
                move.from_sq(), move.to_sq(),
                pc, pos.side_to_move(),
                70, 0, 0
            };
        }
    }
    
    // Detect discovered attacks
    // This is complex - simplified check
    if (move.type_of() != CASTLING && move.type_of() != EN_PASSANT) {
        // Check if move uncovers attack on enemy king
        StateInfo st;
        bool givesCheck = pos.gives_check(move);
        pos.do_move(move, st, givesCheck, {}, {}, nullptr, nullptr);
        bool kingAttacked = pos.checkers();
        pos.undo_move(move);
        
        if (kingAttacked && !givesCheck) {
            events[numEvents++] = {
                ExtensionEvent::DISCOVERED_ATTACK,
                move,
                move.from_sq(), move.to_sq(),
                pos.moved_piece(move),
                pos.side_to_move(),
                90, 0, 0
            };
        }
    }
}

void detect_score_swings(const Position& pos, const Stack* ss, Value prevScore, Value newScore,
                         std::array<ExtensionEventData, 32>& events, int& numEvents) {
    int delta = std::abs(newScore - prevScore);
    if (delta > 100) {  // More than 1 pawn swing
        events[numEvents++] = {
            ExtensionEvent::SCORE_SWING,
            Move::none(),
            SQ_NONE, SQ_NONE,
            NO_PIECE,
            pos.side_to_move(),
            std::min(delta / 10, 255),
            0,
            newScore - prevScore
        };
    }
}

}  // namespace Detectors

ExtensionEngine edseEngine;

int ExtensionEngine::get_move_extension(const Position& pos, const Stack* ss, Move move, Value scoreDelta) {
    std::array<ExtensionEventData, 32> events;
    int numEvents = 0;
    
    // Detect all events for this move
    Detectors::detect_check_events(pos, ss, move, events, numEvents);
    Detectors::detect_capture_events(pos, ss, move, events, numEvents);
    Detectors::detect_promotion_events(pos, ss, move, events, numEvents);
    Detectors::detect_mate_threats(pos, ss, events, numEvents);
    Detectors::detect_forcing_moves(pos, ss, events, numEvents);
    Detectors::detect_pawn_breaks(pos, ss, move, events, numEvents);
    Detectors::detect_piece_sacrifices(pos, ss, move, events, numEvents);
    Detectors::detect_king_exposure(pos, ss, events, numEvents);
    Detectors::detect_tactical_patterns(pos, ss, move, events, numEvents);
    Detectors::detect_score_swings(pos, ss, 0, scoreDelta, events, numEvents);
    
    return process_events(pos, ss, events, numEvents);
}

void init_edse() {
    // Reset engine
    edseEngine.reset();
}

void process_move_events(const Position& pos, const Stack* ss, Move move, Value scoreDelta) {
    std::array<ExtensionEventData, 32> events;
    int numEvents = 0;
    
    // Detect all events for this move
    Detectors::detect_check_events(pos, ss, move, events, numEvents);
    Detectors::detect_capture_events(pos, ss, move, events, numEvents);
    Detectors::detect_promotion_events(pos, ss, move, events, numEvents);
    Detectors::detect_mate_threats(pos, ss, events, numEvents);
    Detectors::detect_forcing_moves(pos, ss, events, numEvents);
    Detectors::detect_pawn_breaks(pos, ss, move, events, numEvents);
    Detectors::detect_piece_sacrifices(pos, ss, move, events, numEvents);
    Detectors::detect_king_exposure(pos, ss, events, numEvents);
    Detectors::detect_tactical_patterns(pos, ss, move, events, numEvents);
    Detectors::detect_score_swings(pos, ss, 0, scoreDelta, events, numEvents);
    
    // Process events
    edseEngine.process_events(pos, ss, events, numEvents);
}

int get_move_extension(const Position& pos, const Stack* ss, Move move, Value scoreDelta) {
    return edseEngine.get_move_extension(pos, ss, move, scoreDelta);
}

void init_edse() {
    // Initialize engine
    edseEngine = ExtensionEngine();
}

}  // namespace EDSE
}  // namespace Stockfish