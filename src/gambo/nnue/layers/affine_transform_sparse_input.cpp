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

// Sparse input affine transform implementation

#include "affine_transform_sparse_input.h"

#include <cstddef>
#include <cstdint>

namespace Gambo::NNUE::Layers {

template<IndexType InputDimensions, int OutputDimensions>
std::size_t AffineTransformSparseInput<InputDimensions, OutputDimensions>::get_content_hash() const {
    std::size_t h = 0;
    for (int i = 0; i < OutputDimensions; ++i) {
        for (IndexType j = 0; j < InputDimensions; ++j) {
            h = h * 31 + weights[j][i];
        }
        h = h * 31 + bias[i];
    }
    return h;
}

template<IndexType InputDimensions, int OutputDimensions>
bool AffineTransformSparseInput<InputDimensions, OutputDimensions>::read_parameters(std::istream& stream) {
    for (int i = 0; i < OutputDimensions; ++i) {
        for (IndexType j = 0; j < InputDimensions; ++j) {
            stream.read(reinterpret_cast<char*>(&weights[j][i]), sizeof(WeightType));
        }
        stream.read(reinterpret_cast<char*>(&bias[i]), sizeof(OutputType));
    }
    return true;
}

template<IndexType InputDimensions, int OutputDimensions>
bool AffineTransformSparseInput<InputDimensions, OutputDimensions>::write_parameters(std::ostream& stream) const {
    for (int i = 0; i < OutputDimensions; ++i) {
        for (IndexType j = 0; j < InputDimensions; ++j) {
            stream.write(reinterpret_cast<const char*>(&weights[j][i]), sizeof(WeightType));
        }
        stream.write(reinterpret_cast<const char*>(&bias[i]), sizeof(OutputType));
    }
    return true;
}

template<IndexType InputDimensions, int OutputDimensions>
void AffineTransformSparseInput<InputDimensions, OutputDimensions>::propagate(
    const TransformedFeatureType* input, OutputBuffer& output) const {
    for (int i = 0; i < OutputDimensions; ++i) {
        int32_t sum = bias[i];
        for (IndexType j = 0; j < InputDimensions; ++j) {
            sum += input[j] * weights[j][i];
        }
        output[i] = static_cast<OutputType>(sum);
    }
}

// Explicit template instantiation
template class AffineTransformSparseInput<TransformedFeatureDimensions, 63>;

}  // namespace Gambo::NNUE::Layers