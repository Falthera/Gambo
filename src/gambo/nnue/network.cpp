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

// GAMBO NNUE network implementation

#include "architecture.h"

#include <cassert>
#include <cstdint>

namespace Gambo::NNUE::Layers {

// AffineTransformSparseInput implementation
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
    // Sparse input: only active features have non-zero values
    for (int i = 0; i < OutputDimensions; ++i) {
        int32_t sum = bias[i];
        // In practice, we'd iterate only over active features
        // For now, simplified dense implementation
        for (IndexType j = 0; j < InputDimensions; ++j) {
            sum += input[j] * weights[j][i];
        }
        output[i] = static_cast<OutputType>(sum);
    }
}

// ClippedReLU implementation
template<int Size>
void ClippedReLU<Size>::propagate(const typename AffineTransformSparseInput<TransformedFeatureDimensions, 63>::OutputBuffer& input,
                                  OutputBuffer& output) const {
    for (int i = 0; i < Size; ++i) {
        int32_t v = input[i];
        output[i] = static_cast<OutputType>(std::clamp(v, 0, 127));
    }
}

// SqrClippedReLU implementation
template<int Size>
void SqrClippedReLU<Size>::propagate(const typename AffineTransformSparseInput<TransformedFeatureDimensions, 63>::OutputBuffer& input,
                                     OutputBuffer& output) const {
    for (int i = 0; i < Size; ++i) {
        int32_t v = input[i];
        v = std::clamp(v, 0, 127);
        output[i] = static_cast<OutputType>(v * v);
    }
}

// AffineTransform implementation
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

template<IndexType InputDimensions, int OutputDimensions>
bool AffineTransform<InputDimensions, OutputDimensions>::read_parameters(std::istream& stream) {
    for (int i = 0; i < OutputDimensions; ++i) {
        for (int j = 0; j < InputDimensions; ++j) {
            stream.read(reinterpret_cast<char*>(&weights[j][i]), sizeof(WeightType));
        }
        stream.read(reinterpret_cast<char*>(&bias[i]), sizeof(OutputType));
    }
    return true;
}

template<IndexType InputDimensions, int OutputDimensions>
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
    const typename ClippedReLU<62>::OutputBuffer& input, OutputBuffer& output) const {
    for (int i = 0; i < OutputDimensions; ++i) {
        int32_t sum = bias[i];
        for (int j = 0; j < InputDimensions; ++j) {
            sum += input[j] * weights[j][i];
        }
        output[i] = static_cast<OutputType>(std::clamp(sum, 0, 127));
    }
}

// Explicit template instantiations
template class AffineTransformSparseInput<TransformedFeatureDimensions, 63>;
template class ClippedReLU<63>;
template class ClippedReLU<32>;
template class SqrClippedReLU<63>;
template class AffineTransform<62, 32>;
template class AffineTransform<32, 1>;

}  // namespace Gambo::NNUE::Layers

namespace Gambo::NNUE {

// NetworkArchitecture implementation
bool NetworkArchitecture::read_parameters(std::istream& stream) {
    return fc_0.read_parameters(stream) && ac_sqr_0.read_parameters(stream) &&
           ac_0.read_parameters(stream) && fc_1.read_parameters(stream) &&
           ac_1.read_parameters(stream) && fc_2.read_parameters(stream);
}

bool NetworkArchitecture::write_parameters(std::ostream& stream) const {
    return fc_0.write_parameters(stream) && ac_sqr_0.write_parameters(stream) &&
           ac_0.write_parameters(stream) && fc_1.write_parameters(stream) &&
           ac_1.write_parameters(stream) && fc_2.write_parameters(stream);
}

std::int32_t NetworkArchitecture::propagate(const TransformedFeatureType* transformed_features) const {
    // Buffer for intermediate results
    struct alignas(CacheLineSize) Buffer {
        alignas(CacheLineSize) typename decltype(fc_0)::OutputBuffer fc_0_out;
        alignas(CacheLineSize) typename decltype(ac_sqr_0)::OutputType ac_sqr_0_out[32];
        alignas(CacheLineSize) typename decltype(ac_0)::OutputType ac_0_out[32];
        alignas(CacheLineSize) typename decltype(fc_1)::OutputBuffer fc_1_out;
        alignas(CacheLineSize) typename decltype(ac_1)::OutputBuffer ac_1_out;
        alignas(CacheLineSize) typename decltype(fc_2)::OutputBuffer fc_2_out;
    };
    
    Buffer buffer;
    
    // FC0: Sparse input (1024) -> 63
    fc_0.propagate(transformed_features, buffer.fc_0_out);
    
    // Dual ReLU: SqrClippedReLU on first 31, ClippedReLU on all 63
    ac_sqr_0.propagate(buffer.fc_0_out, buffer.ac_sqr_0_out);
    ac_0.propagate(buffer.fc_0_out, buffer.ac_0_out);
    
    // Concat: first 31 of sqr + first 31 of clip = 62
    // Create combined buffer for FC1
    struct alignas(CacheLineSize) FC1InputBuffer {
        int16_t data[62];
    } fc1_input;
    
    for (int i = 0; i < 31; ++i) {
        fc1_input.data[i] = buffer.ac_sqr_0_out[i];
    }
    for (int i = 0; i < 31; ++i) {
        fc1_input.data[31 + i] = buffer.ac_0_out[i];
    }
    
    // FC1: 62 -> 32
    fc_1.propagate(fc1_input, buffer.fc_1_out);
    
    // ClippedReLU on FC1 output
    ac_1.propagate(buffer.fc_1_out, buffer.ac_1_out);
    
    // FC2: 32 -> 1
    fc_2.propagate(buffer.ac_1_out, buffer.fc_2_out);
    
    // Output scaling
    // buffer.fc_0_out[31] is the bias unit, scaled such that 1.0 = 127 * (1<<WeightScaleBits)
    // We want 1.0 = 600 * OutputScale
    int32_t fwdOut = (buffer.fc_0_out[31]) * (600 * OutputScale) / (127 * (1 << WeightScaleBits));
    int32_t outputValue = buffer.fc_2_out[0] + fwdOut;
    
    return outputValue;
}

std::size_t NetworkArchitecture::get_content_hash() const {
    std::size_t h = 0;
    h = h * 31 + fc_0.get_content_hash();
    h = h * 31 + ac_sqr_0.get_content_hash();
    h = h * 31 + ac_0.get_content_hash();
    h = h * 31 + fc_1.get_content_hash();
    h = h * 31 + ac_1.get_content_hash();
    h = h * 31 + fc_2.get_content_hash();
    h = h * 31 + get_hash_value();
    return h;
}

bool NetworkArchitecture::read_parameters(std::istream& stream) {
    return fc_0.read_parameters(stream) && ac_sqr_0.read_parameters(stream) &&
           ac_0.read_parameters(stream) && fc_1.read_parameters(stream) &&
           ac_1.read_parameters(stream) && fc_2.read_parameters(stream);
}

bool NetworkArchitecture::write_parameters(std::ostream& stream) const {
    return fc_0.write_parameters(stream) && ac_sqr_0.write_parameters(stream) &&
           ac_0.write_parameters(stream) && fc_1.write_parameters(stream) &&
           ac_1.write_parameters(stream) && fc_2.write_parameters(stream);
}

}  // namespace Gambo::NNUE