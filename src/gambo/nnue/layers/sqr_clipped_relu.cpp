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

// Squared Clipped ReLU activation implementation

#include "sqr_clipped_relu.h"

#include <algorithm>

namespace Gambo::NNUE::Layers {

template<int Size>
void SqrClippedReLU<Size>::propagate(const OutputType* input,
                                     OutputBuffer& output) const {
    for (int i = 0; i < Size; ++i) {
        int32_t v = input[i];
        v = std::clamp(v, 0, 127);
        output[i] = static_cast<OutputType>(v * v);
    }
}

// Explicit template instantiation
template class SqrClippedReLU<63>;

}  // namespace Gambo::NNUE::Layers