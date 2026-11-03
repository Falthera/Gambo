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

// Dense affine transform implementation

#include "affine_transform.h"

#include <cstddef>
#include <cstdint>

namespace Gambo::NNUE::Layers {

template<int InputDimensions, int OutputDimensions>
std::size_t AffineTransform<InputDimensions, OutputDimensions>::get_content_hash() const {
    std::size_t h = 0;
    for (int i = 0; i < OutputDimensions; ++i) {
        for (int j = 0; j < InputDimensions; ++j) {
            h = h * 31 + weights[j][i];
        }
        h = h * 31 + bias[i];
    }
    return h;
}

template<int InputDimensions, int OutputDimensions>
bool AffineTransform<InputDimensions, OutputDimensions>::read_parameters(std::istream& stream) {
    for (int i = 0; i < OutputDimensions; ++i) {
        for (int j = 0; j < InputDimensions; ++j) {
            stream.read(reinterpret_cast<char*>(&weights[j][i]), sizeof(WeightType));
        }
        stream.read(reinterpret_cast<char*>(&bias[i]), sizeof(OutputType));
    }
    return true;
}

template<int InputDimensions, int OutputDimensions>
bool AffineTransform<InputDimensions, OutputDimensions>::write_parameters(std::ostream& stream) const {
    for (int i = 0; i < OutputDimensions; ++i) {
        for (int j = 0; j < InputDimensions; ++j) {
            stream.write(reinterpret_cast<const char*>(&weights[j][i]), sizeof(WeightType));
        }
        stream.write(reinterpret_cast<const char*>(&bias[i]), sizeof(OutputType));
    }
    return true;
}

template<int InputDimensions, int OutputDimensions>
void AffineTransform<InputDimensions, OutputDimensions>::propagate(
    const OutputType* input, OutputBuffer& output) const {
    for (int i = 0; i < OutputDimensions; ++i) {
        int32_t sum = bias[i];
        for (int j = 0; j < InputDimensions; ++j) {
            sum += input[j] * weights[j][i];
        }
        output[i] = static_cast<OutputType>(std::clamp(sum, 0, 127));
    }
}

// Explicit template instantiations
template class AffineTransform<62, 32>;
template class AffineTransform<32, 1>;

}  // namespace Gambo::NNUE::Layers