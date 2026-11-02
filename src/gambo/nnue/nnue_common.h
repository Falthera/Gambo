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

// Common types and constants for GAMBO NNUE

#ifndef GAMBO_NNUE_COMMON_H_INCLUDED
#define GAMBO_NNUE_COMMON_H_INCLUDED

#include <cstdint>
#include <array>
#include <ostream>
#include <istream>

#include "../../types.h"

namespace Gambo::NNUE {

// Cache line size for alignment
constexpr int CacheLineSize = 64;

// Weight type (quantized int8)
using WeightType = int8_t;

// Transformed feature type (after feature extraction)
using TransformedFeatureType = int16_t;

// Value list for fixed-size arrays
template<typename T, int Size>
struct ValueList {
    T data[Size];
    int size = 0;
    
    T& operator[](int i) { return data[i]; }
    const T& operator[](int i) const { return data[i]; }
    
    void push_back(const T& value) {
        if (size < Size) data[size++] = value;
    }
    
    int size() const { return size; }
    void clear() { size = 0; }
};

// Hash combine utility
inline void hash_combine(std::size_t& h, std::size_t v) {
    h ^= v + 0x9e3779b97f4a7c15 + (h << 6) + (h >> 2);
}

// Type aliases for feature sets
using IndexType = uint32_t;
using ValueType = int16_t;

// Feature set hash values
constexpr std::uint32_t PawnPairHash = 0x1F234CB8u;
constexpr std::uint32_t KingRelativeHash = 0x2F234CB8u;
constexpr std::uint32_t ThreatDetectionHash = 0x3F234CB8u;
constexpr std::uint32_t PieceRelativeHash = 0x4F234CB8u;

// Architecture hash
constexpr std::uint32_t NetworkArchitectureHash = 0xEC42E90Du;

// Quantization parameters
constexpr int WeightScaleBits = 6;
constexpr int ActivationScaleBits = 8;
constexpr int OutputScale = 600;
constexpr int OutputScaleBits = 14;

// Network topology
constexpr int L1 = 1024;        // Transformed feature dimensions
constexpr int L2 = 31;          // FC0 output per half
constexpr int L3 = 32;          // FC1 output

}  // namespace Gambo::NNUE

#endif  // GAMBO_NNUE_COMMON_H_INCLUDED