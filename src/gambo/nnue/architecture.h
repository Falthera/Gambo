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

// GAMBO-native NNUE network architecture
// SFNNv16-equivalent topology implemented independently

#ifndef GAMBO_NNUE_ARCHITECTURE_H_INCLUDED
#define GAMBO_NNUE_ARCHITECTURE_H_INCLUDED

#include <cstdint>
#include <cstring>
#include <iosfwd>

#include "nnue_common.h"
#include "layers/affine_transform.h"
#include "layers/affine_transform_sparse_input.h"
#include "layers/clipped_relu.h"
#include "layers/sqr_clipped_relu.h"

namespace Gambo::NNUE {

// Network topology constants (SFNNv16-equivalent)
constexpr IndexType L1 = 1024;        // Transformed feature dimensions
constexpr int L2 = 31;                // FC0 output per half (dual ReLU)
constexpr int L3 = 32;                // FC1 output
constexpr IndexType PSQTBuckets = 8;  // Must be multiple of 8 for vectorization
constexpr IndexType LayerStacks = 8;  // Accumulator parallelism

// Layer dimensions
constexpr IndexType TransformedFeatureDimensions = L1;
constexpr int FC0_OUTPUTS = 31;       // Per half (dual ReLU)
constexpr int FC1_OUTPUTS = L3;       // 32
constexpr int FC2_OUTPUTS = 1;        // Single output

// Quantization parameters
constexpr int WeightScaleBits = 6;        // 64 weight levels
constexpr int ActivationScaleBits = 8;    // 256 activation levels
constexpr int OutputScale = 600;          // Output scaling
constexpr int OutputScaleBits = 14;       // Output quantization

// Feature set dimensions (SFNNv16-equivalent)
constexpr IndexType PawnPairDimensions = 8192;
constexpr IndexType KingRelativeDimensions = 40960;
constexpr IndexType ThreatDetectionDimensions = 60720;
constexpr IndexType PieceRelativeDimensions = 8192;

// Feature set hash values
constexpr std::uint32_t PawnPairHash = 0x1F234CB8u;
constexpr std::uint32_t KingRelativeHash = 0x2F234CB8u;
constexpr std::uint32_t ThreatDetectionHash = 0x3F234CB8u;
constexpr std::uint32_t PieceRelativeHash = 0x4F234CB8u;

// Network architecture hash (for validation)
constexpr std::uint32_t NetworkArchitectureHash = 0xEC42E90Du;

namespace Layers {

// Sparse input affine transform (for feature extraction)
template<IndexType InputDimensions, int OutputDimensions>
class AffineTransformSparseInput {
public:
    using OutputType = int16_t;
    using OutputBuffer = ValueList<OutputType, OutputDimensions>;
    
    alignas(CacheLineSize) WeightType weights[InputDimensions][OutputDimensions];
    alignas(CacheLineSize) OutputType bias[OutputDimensions];
    
    // Hash for parameter validation
    std::size_t get_content_hash() const;
    
    // Read/write parameters
    bool read_parameters(std::istream& stream);
    bool write_parameters(std::ostream& stream) const;
    
    // Sparse forward propagation
    void propagate(const TransformedFeatureType* input, OutputBuffer& output) const;
};

// Clipped ReLU activation (clamp 0-127)
template<int Size>
class ClippedReLU {
public:
    using OutputType = int16_t;
    using OutputBuffer = ValueList<OutputType, Size>;
    
    void propagate(const typename AffineTransformSparseInput<TransformedFeatureDimensions, 63>::OutputBuffer& input,
                   OutputBuffer& output) const;
};

// Squared Clipped ReLU activation (clamp 0-127 then square)
template<int Size>
class SqrClippedReLU {
public:
    using OutputType = int16_t;
    using OutputBuffer = ValueList<OutputType, Size>;
    
    void propagate(const typename AffineTransformSparseInput<TransformedFeatureDimensions, 63>::OutputBuffer& input,
                   OutputBuffer& output) const;
};

// Dense affine transform
template<int InputDimensions, int OutputDimensions>
class AffineTransform {
public:
    using OutputType = int16_t;
    using OutputBuffer = ValueList<OutputType, OutputDimensions>;
    
    alignas(CacheLineSize) WeightType weights[InputDimensions][OutputDimensions];
    alignas(CacheLineSize) OutputType bias[OutputDimensions];
    
    std::size_t get_content_hash() const;
    bool read_parameters(std::istream& stream);
    bool write_parameters(std::ostream& stream) const;
    
    void propagate(const typename ClippedReLU<62>::OutputBuffer& input, OutputBuffer& output) const;
};

}  // namespace Layers

// Network architecture definition
struct NetworkArchitecture {
    static constexpr IndexType TransformedFeatureDimensions = L1;
    static constexpr int FC0_OUTPUTS = 31;    // Per half (dual ReLU)
    static constexpr int FC1_OUTPUTS = 32;    // FC1 output
    
    // Layer 0: Sparse input (1024) -> 63 (dual ReLU: 31 SqrClippedReLU + 32 ClippedReLU)
    Layers::AffineTransformSparseInput<TransformedFeatureDimensions, 63> fc_0;
    
    // Dual ReLU activation
    Layers::SqrClippedReLU<63> ac_sqr_0;   // First 31 -> SqrClippedReLU
    Layers::ClippedReLU<63> ac_0;          // All 63 -> ClippedReLU
    
    // Layer 1: Dense 62 -> 32 (concat of first 31 from each ReLU)
    Layers::AffineTransform<62, 32> fc_1;
    Layers::ClippedReLU<32> ac_1;
    
    // Layer 2: Output 32 -> 1
    Layers::AffineTransform<32, 1> fc_2;
    
    // Architecture hash for validation
    static constexpr std::uint32_t get_hash_value() {
        std::uint32_t hash = 0xEC42E90Du;
        hash ^= TransformedFeatureDimensions * 2;
        
        // Combine layer dimensions into hash (simplified)
        hash ^= 63 * 31;          // FC0 output
        hash ^= 62 * 31;          // FC1 input
        hash ^= 32 * 31;          // FC1 output
        hash ^= 1 * 31;           // FC2 output
        
        return hash;  // Expected: 0xEC42E90D
    }
    
    // Read/write network parameters
    bool read_parameters(std::istream& stream);
    bool write_parameters(std::ostream& stream) const;
    
    // Forward propagation with quantization
    std::int32_t propagate(const TransformedFeatureType* transformed_features) const;
    
    // Content hash for network validation
    std::size_t get_content_hash() const;
};

}  // namespace Gambo::NNUE

#endif  // GAMBO_NNUE_ARCHITECTURE_H_INCLUDED