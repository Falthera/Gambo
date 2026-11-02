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

// Affine transform with sparse input

#ifndef GAMBO_NNUE_LAYERS_AFFINE_TRANSFORM_SPARSE_INPUT_H_INCLUDED
#define GAMBO_NNUE_LAYERS_AFFINE_TRANSFORM_SPARSE_INPUT_H_INCLUDED

#include <cstddef>
#include <cstdint>

#include "../nnue_common.h"

namespace Gambo::NNUE::Layers {

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

}  // namespace Gambo::NNUE::Layers

#endif  // GAMBO_NNUE_LAYERS_AFFINE_TRANSFORM_SPARSE_INPUT_H_INCLUDED