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

// GAMBO-native NNUE network architecture implementation

#include "architecture.h"

#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>

namespace Gambo::NNUE {

// NetworkArchitecture method implementations

bool NetworkArchitecture::read_parameters(std::istream& stream) {
    if (!fc_0.read_parameters(stream)) return false;
    if (!fc_1.read_parameters(stream)) return false;
    if (!fc_2.read_parameters(stream)) return false;
    return true;
}

bool NetworkArchitecture::write_parameters(std::ostream& stream) const {
    if (!fc_0.write_parameters(stream)) return false;
    if (!fc_1.write_parameters(stream)) return false;
    if (!fc_2.write_parameters(stream)) return false;
    return true;
}

std::int32_t NetworkArchitecture::propagate(const TransformedFeatureType* transformed_features) const {
    // Layer 0: Sparse input (1024) -> 63
    typename Layers::AffineTransformSparseInput<TransformedFeatureDimensions, 63>::OutputBuffer fc0_out;
    fc_0.propagate(transformed_features, fc0_out);

    // Dual ReLU activation
    Layers::SqrClippedReLU<63>::OutputBuffer sqr_out;
    ac_sqr_0.propagate(fc0_out.data, sqr_out);

    Layers::ClippedReLU<63>::OutputBuffer clipped_out;
    ac_0.propagate(fc0_out.data, clipped_out);

    // Layer 1: Dense 62 -> 32 (concat first 31 from each ReLU)
    typename Layers::AffineTransform<62, 32>::OutputBuffer fc1_in;
    for (int i = 0; i < 31; ++i) fc1_in[i] = sqr_out[i];
    for (int i = 31; i < 63; ++i) fc1_in[i] = clipped_out[i];
    fc1_in.count = 62;

    typename Layers::AffineTransform<62, 32>::OutputBuffer fc1_out;
    fc_1.propagate(fc1_in.data, fc1_out);

    Layers::ClippedReLU<32>::OutputBuffer ac1_out;
    ac_1.propagate(fc1_out.data, ac1_out);

    // Layer 2: Output 32 -> 1
    typename Layers::AffineTransform<32, 1>::OutputBuffer fc2_out;
    fc_2.propagate(ac1_out.data, fc2_out);

    // Return scaled output
    return fc2_out[0] * OutputScale;
}

std::size_t NetworkArchitecture::get_content_hash() const {
    std::size_t h = fc_0.get_content_hash();
    h = h * 31 + fc_1.get_content_hash();
    h = h * 31 + fc_2.get_content_hash();
    return h;
}

}  // namespace Gambo::NNUE