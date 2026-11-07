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

// GAMBO-native NNUE network wrapper implementation

#include "network.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>

#include "features/pawn_pair.h"
#include "features/king_relative.h"
#include "features/threat_detection.h"
#include "features/piece_relative.h"
#include "../../position.h"
#include "../../types.h"

namespace Gambo::NNUE {

// Feature extraction: populate transformedFeatures from position using GAMBO feature sets
static void extract_features(const Position& pos, TransformedFeatureType* out_features) {
    // Clear output buffer
    for (int i = 0; i < TransformedFeatureDimensions; ++i) {
        out_features[i] = 0;
    }
    
    Color us = pos.side_to_move();
    Color them = ~us;
    
    // Feature set indices (matching SFNNv16 dimensions)
    // PawnPair: 8192, KingRelative: 40960, ThreatDetection: 60720, PieceRelative: 8192
    // Total: 118064 -> compressed to 1024 via feature transformer
    
    // For now, we use a simplified feature extraction that maps to 1024 dimensions
    // This should be replaced with proper feature transformer when trained network exists
    
    // Extract PawnPair features (simplified: king + pawn pairs)
    int idx = 0;
    const Square ourKingSq = pos.square<KING>(us);
    const Square theirKingSq = pos.square<KING>(them);
    
    Bitboard ourPawns = pos.pieces(us, PAWN);
    Bitboard theirPawns = pos.pieces(them, PAWN);
    
    // Simple pawn pair features (sample)
    int pairCount = 0;
    Bitboard pawnsCopy = ourPawns;
    while (pawnsCopy && pairCount < 32) {
        Square p1 = pop_lsb(pawnsCopy);
        Bitboard pawnsCopy2 = ourPawns & ~square_bb(p1);
        while (pawnsCopy2 && pairCount < 32) {
            Square p2 = pop_lsb(pawnsCopy2);
            Features::PawnPairType type = Features::PawnPair::detect_pair_type(us, pos, p1, p2);
            if (type != Features::PawnPairType::CONNECTED) {
                IndexType featureIdx = Features::PawnPair::make_index(us, ourKingSq, p1, p2, type);
                if (featureIdx < TransformedFeatureDimensions) {
                    out_features[featureIdx] = 127;  // Max activation
                }
                pairCount++;
            }
        }
    }
    
    // Extract KingRelative features (simplified: piece-square relative to king)
    Bitboard ourPieces = pos.pieces(us);
    Bitboard theirPieces = pos.pieces(them);
    
    Bitboard piecesCopy = ourPieces;
    while (piecesCopy && idx < TransformedFeatureDimensions) {
        Square sq = pop_lsb(piecesCopy);
        Piece pc = pos.piece_on(sq);
        if (type_of(pc) != KING) {
            IndexType featureIdx = Features::KingRelative::make_index(us, ourKingSq, sq, pc);
            if (featureIdx < TransformedFeatureDimensions) {
                out_features[featureIdx] = 127;
            }
        }
    }
    
    piecesCopy = theirPieces;
    while (piecesCopy && idx < TransformedFeatureDimensions) {
        Square sq = pop_lsb(piecesCopy);
        Piece pc = pos.piece_on(sq);
        if (type_of(pc) != KING) {
            IndexType featureIdx = Features::KingRelative::make_index(us, ourKingSq, sq, pc);
            if (featureIdx < TransformedFeatureDimensions) {
                out_features[featureIdx] = 127;
            }
        }
    }
    
    // Extract ThreatDetection features (simplified)
    // This would detect attacker-target-king threat patterns
    for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN}) {
        Bitboard attackers = pos.pieces(us, pt);
        Bitboard attackersCopy = attackers;
        while (attackersCopy && idx < TransformedFeatureDimensions) {
            Square attSq = pop_lsb(attackersCopy);
            Bitboard targets = attacks_bb(make_piece(us, pt), attSq, pos.pieces()) & pos.pieces(them);
            Bitboard targetsCopy = targets;
            while (targetsCopy && idx < TransformedFeatureDimensions) {
                Square tgtSq = pop_lsb(targetsCopy);
                IndexType featureIdx = Features::ThreatDetection::make_index(us, make_piece(us, pt), attSq, tgtSq, pos.piece_on(tgtSq), theirKingSq);
                if (featureIdx < TransformedFeatureDimensions) {
                    out_features[featureIdx] = 127;
                }
            }
        }
    }
    
    // Extract PieceRelative features (simplified: piece coordination patterns)
    // Knight outposts
    Bitboard ourKnights = pos.pieces(us, KNIGHT);
    Bitboard enemyPawnAttacks = pos.attacks_by<PAWN>(them);
    Bitboard knightsCopy = ourKnights;
    while (knightsCopy && idx < TransformedFeatureDimensions) {
        Square n = pop_lsb(knightsCopy);
        if (!(enemyPawnAttacks & square_bb(n)) && (ourPawns & pos.attacks_by<PAWN>(us) & square_bb(n))) {
            IndexType featureIdx = Features::PieceRelative::make_index(Features::PieceRelative::CoordType::COORD_OUTPOST, n, n, us);
            if (featureIdx < TransformedFeatureDimensions) {
                out_features[featureIdx] = 127;
            }
        }
    }
    
    // Rook on open files
    Bitboard ourRooks = pos.pieces(us, ROOK);
    Bitboard allPawns = pos.pieces(PAWN);
    Bitboard rooksCopy = ourRooks;
    while (rooksCopy && idx < TransformedFeatureDimensions) {
        Square r = pop_lsb(rooksCopy);
        File f = file_of(r);
        if (!(allPawns & file_bb(f))) {
            IndexType featureIdx = Features::PieceRelative::make_index(Features::PieceRelative::CoordType::COORD_ROOK_OPEN_FILE, r, r, us);
            if (featureIdx < TransformedFeatureDimensions) {
                out_features[featureIdx] = 127;
            }
        }
    }
}

bool Network::load(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    return architecture.read_parameters(stream);
}

bool Network::save(std::ostream& stream) const {
    return architecture.write_parameters(stream);
}

NetworkOutput Network::evaluate(const Position&                       pos,
                                Stockfish::Eval::NNUE::AccumulatorStack&         /*accumulatorStack*/,
                                Stockfish::Eval::NNUE::AccumulatorCaches&        /*cache*/) const {
    // Extract features
    extract_features(pos, transformedFeatures);
    
    // Propagate through network
    std::int32_t rawOutput = architecture.propagate(transformedFeatures);
    
    // Convert to Stockfish-compatible output (psqt, positional)
    // GAMBO network produces single output; split into two for compatibility
    Value psqt = static_cast<Value>(rawOutput / 2);
    Value positional = static_cast<Value>(rawOutput - psqt);
    
    return NetworkOutput(psqt, positional);
}

bool Network::verify(const std::string& path) const {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    
    std::uint32_t header;
    stream.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!stream || header != NetworkArchitecture::get_hash_value()) {
        return false;
    }
    return true;
}

std::size_t Network::get_content_hash() const {
    return architecture.get_content_hash();
}

std::string Network::description() const {
    std::ostringstream oss;
    oss << "GAMBO SFNNv16-native network (PawnPair+KingRelative+ThreatDetection+PieceRelative)";
    return oss.str();
}

}  // namespace Gambo::NNUE