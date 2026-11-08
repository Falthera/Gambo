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

// PTC - Persistent Threat Context implementation

#include "ptc.h"
#include "position.h"
#include "bitboard.h"
#include "search.h"

namespace Stockfish {
namespace PTC {

ThreatContext threatContext;
struct PTCStats ptcStats;

void init_ptc() {
    threatContext = ThreatContext{};
    ptcStats = PTCStats{};
}

void clear_threat_context() {
    threatContext = ThreatContext{};
    ptcStats = PTCStats{};
}

int evaluate_threat_severity(const Threat& threat, const Position& pos) {
    int severity = 0;
    
    switch (threat.type) {
        case ThreatType::MATE_THREAT:
            severity = 255;
            break;
        case ThreatType::DOUBLE_ATTACK:
        case ThreatType::FORK:
            severity = 180;
            break;
        case ThreatType::DISCOVERED_ATTACK:
            severity = 150;
            break;
        case ThreatType::PIN:
        case ThreatType::SKEWER:
            severity = 120;
            break;
        case ThreatType::DIRECT_ATTACK:
            severity = 100;
            break;
        case ThreatType::MATE_THREAT:
            severity = 255;
            break;
        case ThreatType::BACK_RANK:
            severity = 200;
            break;
        case ThreatType::SMOTHERED_MATE:
            severity = 220;
            break;
        case ThreatType::TRAPPED_PIECE:
            severity = 90;
            break;
        case ThreatType::KING_EXPOSURE:
            severity = 130;
            break;
        case ThreatType::WEAK_PAWN:
        case ThreatType::WEAK_SQUARE:
            severity = 60;
            break;
        case ThreatType::OUTPOST:
        case ThreatType::OPEN_FILE:
        case ThreatType::SEVENTH_RANK:
        case ThreatType::BATTERY:
        case ThreatType::XRAY:
            severity = 80;
            break;
        default:
            severity = 50;
    }
    
    // Adjust for piece values
    if (threat.target != NO_PIECE) {
        severity += PieceValue[threat.target] / 50;
    }
    
    return std::min(severity, 255);
}

void detect_threats(const Position& pos, int ply) {
    threatContext.numThreats = 0;
    threatContext.maxSeverity = 0;
    threatContext.threatenedSquares = 0;
    threatContext.defenderSquares = 0;
    threatContext.plyLastUpdated = ply;
    threatContext.mostThreatenedColor = pos.side_to_move();
    
    Color us = pos.side_to_move();
    Color them = ~us;
    
    // 1. Detect direct attacks on king
    Square theirKing = pos.square<KING>(them);
    Bitboard attackers = pos.attackers_to(theirKing);
    if (attackers) {
        for (Square sq : attackers) {
            Piece p = pos.piece_on(sq);
            Threat t;
            t.type = ThreatType::DIRECT_ATTACK;
            t.targetSquare = theirKing;
            t.attackerSquare = sq;
            t.attacker = p;
            t.target = pos.piece_on(theirKing);
            t.attackerColor = us;
            t.severity = evaluate_threat_severity(t, pos);
            t.plyCreated = ply;
            t.persistent = true;
            t.attackRay = attacks_bb(type_of(p), sq, pos.pieces()) & square_bb(theirKing);
            
            if (threatContext.numThreats < 64) {
                threatContext.threats[threatContext.numThreats++] = t;
                threatContext.maxSeverity = std::max(threatContext.maxSeverity, t.severity);
                threatContext.threatenedSquares |= square_bb(theirKing);
            }
        }
    }
    
    // 2. Detect mate threats (forced mate sequences)
    // Simplified: check if any move gives checkmate
    MoveList<GenType::LEGAL> moves(pos);
    for (Move m : moves) {
        StateInfo st;
        bool givesCheck = pos.gives_check(m);
        pos.do_move(m, st, givesCheck, {}, {}, nullptr, nullptr);
        
        if (pos.checkers() && pos.is_checkmate()) {
            Threat t;
            t.type = ThreatType::MATE_THREAT;
            t.targetSquare = pos.square<KING>(pos.side_to_move());
            t.attackerSquare = m.from_sq();
            t.attacker = pos.moved_piece(m);
            t.target = pos.piece_on(pos.square<KING>(pos.side_to_move()));
            t.attackerColor = us;
            t.severity = 255;
            t.plyCreated = ply;
            t.persistent = true;
            
            if (threatContext.numThreats < 64) {
                threatContext.threats[threatContext.numThreats++] = t;
                threatContext.maxSeverity = 255;
            }
        }
        pos.undo_move(m);
    }
    
    // 3. Detect forks/double attacks
    Bitboard knights = pos.pieces(us, KNIGHT);
    while (knights) {
        Square sq = pop_lsb(knights);
        Bitboard attacks = attacks_bb<KNIGHT>(sq);
        Bitboard targets = pos.pieces(them, ROOK, BISHOP, QUEEN, KING);
        if (popcount(attacks & targets) >= 2) {
            Threat t;
            t.type = ThreatType::FORK;
            t.targetSquare = pop_lsb(attacks & targets);
            t.attackerSquare = sq;
            t.attacker = pos.piece_on(sq);
            t.target = pos.piece_on(t.targetSquare);
            t.attackerColor = us;
            t.severity = evaluate_threat_severity(Threat{ThreatType::FORK, SQ_NONE, sq, NO_PIECE, NO_PIECE, us, 0, 0, 0, false, 0, {}}, *const_cast<Position*>(&pos));
            t.plyCreated = ply;
            t.persistent = true;
            
            if (threatContext.numThreats < 64) {
                threatContext.threats[threatContext.numThreats++] = t;
                threatContext.maxSeverity = std::max(threatContext.maxSeverity, t.severity);
            }
        }
    }
    
    // 4. Detect pins
    Bitboard pinned = pos.pinned_pieces(them);
    while (pinned) {
        Square sq = pop_lsb(pinned);
        Threat t;
        t.type = ThreatType::PIN;
        t.targetSquare = sq;
        t.target = pos.piece_on(sq);
        t.attackerColor = us;
        t.severity = evaluate_threat_severity(t, pos);
        t.plyCreated = ply;
        t.persistent = true;
        
        if (threatContext.numThreats < 64) {
            threatContext.threats[threatContext.numThreats++] = t;
            threatContext.maxSeverity = std::max(threatContext.maxSeverity, t.severity);
        }
    }
    
    // 5. Detect weak squares
    Bitboard weakSquares = pos.weak_squares(them);
    while (weakSquares) {
        Square sq = pop_lsb(weakSquares);
        Threat t;
        t.type = ThreatType::WEAK_SQUARE;
        t.targetSquare = sq;
        t.attackerColor = us;
        t.severity = evaluate_threat_severity(Threat{ThreatType::WEAK_SQUARE, sq, SQ_NONE, NO_PIECE, NO_PIECE, us, 0, 0, 0, false, 0, {}}, *const_cast<Position*>(&pos));
        t.plyCreated = ply;
        
        if (threatContext.numThreats < 64) {
            threatContext.threats[threatContext.numThreats++] = t;
        }
    }
    
    // Update threatened squares bitboard
    for (int i = 0; i < threatContext.numThreats; ++i) {
        threatContext.threatenedSquares |= square_bb(threatContext.threats[i].targetSquare);
    }
    
    // Update stats
    ptcStats.totalThreatsDetected += threatContext.numThreats;
    ptcStats.maxConcurrentThreats = std::max(ptcStats.maxConcurrentThreats, threatContext.numThreats);
    
    // Determine most threatened color
    int whiteThreats = 0, blackThreats = 0;
    for (int i = 0; i < threatContext.numThreats; ++i) {
        if (threatContext.threats[i].attackerColor == WHITE) whiteThreats++;
        else blackThreats++;
    }
    threatContext.mostThreatenedColor = (whiteThreats > blackThreats) ? BLACK : WHITE;
}

void update_threat_context(const Position& pos, const Stack* ss, Move move, int ply) {
    // Update persistent threats
    for (auto it = threatContext.persistentThreats.begin(); it != threatContext.persistentThreats.end(); ) {
        // Check if threat still exists
        const Threat& t = it->threat;
        bool stillExists = false;
        
        if (t.type == ThreatType::DIRECT_ATTACK) {
            Square target = t.targetSquare;
            if (pos.piece_on(target) == t.target && pos.attackers_to(target) & square_bb(t.attackerSquare)) {
                stillExists = true;
            }
        } else if (t.type == ThreatType::PIN) {
            if (pos.pinned_pieces(~t.attackerColor) & square_bb(t.targetSquare)) {
                stillExists = true;
            }
        }
        
        if (stillExists) {
            it->persistenceCount++;
            ++it;
        } else {
            ptcStats.threatsNeutralized++;
            it = threatContext.persistentThreats.erase(it);
        }
    }
    
    // Add new threats from this move
    detect_threats(pos, ply);
    
    // Add new threats to persistent tracking
    for (int i = 0; i < threatContext.numThreats; ++i) {
        const Threat& t = threatContext.threats[i];
        if (t.persistent) {
            threatContext.persistentThreats.push_back({
                t,
                threatContext.nextVariationId,
                1
            });
            ptcStats.persistentThreatsTracked++;
        }
    }
}

void update_persistent_threats(const Position& pos, int variationId) {
    // Update variation ID for new persistent threats
    for (auto& pt : threatContext.persistentThreats) {
        if (pt.variationId == 0) {
            pt.variationId = variationId;
        }
    }
}

const ThreatContext& get_threat_context() {
    return threatContext;
}

const Threat* get_most_severe_threat(Color perspective) {
    const Threat* mostSevere = nullptr;
    int maxSeverity = -1;
    
    for (int i = 0; i < threatContext.numThreats; ++i) {
        const Threat& t = threatContext.threats[i];
        if (t.attackerColor == perspective && t.severity > maxSeverity) {
            maxSeverity = t.severity;
            mostSevere = &t;
        }
    }
    return mostSevere;
}

std::vector<Threat> get_threats_on_square(Square sq, Color perspective) {
    std::vector<Threat> result;
    for (int i = 0; i < threatContext.numThreats; ++i) {
        const Threat& t = threatContext.threats[i];
        if (t.targetSquare == sq && t.attackerColor == perspective) {
            result.push_back(t);
        }
    }
    return result;
}

bool is_square_threatened(Square sq, Color byColor) {
    for (int i = 0; i < threatContext.numThreats; ++i) {
        const Threat& t = threatContext.threats[i];
        if (t.targetSquare == sq && t.attackerColor == byColor) {
            return true;
        }
    }
    return false;
}

int start_new_variation() {
    return threatContext.nextVariationId++;
}

void end_variation(int variationId) {
    // Remove persistent threats that belonged to this variation
    for (auto it = threatContext.persistentThreats.begin(); it != threatContext.persistentThreats.end(); ) {
        if (it->variationId == variationId) {
            it = threatContext.persistentThreats.erase(it);
        } else {
            ++it;
        }
    }
}

const PTCStats& get_ptc_stats() {
    return ptcStats;
}

void init_ptc() {
    threatContext = ThreatContext{};
    ptcStats = PTCStats{};
}

const std::vector<ThreatContext::PersistentThreat>& get_persistent_threats() {
    return threatContext.persistentThreats;
}

}  // namespace PTC
}  // namespace Stockfish