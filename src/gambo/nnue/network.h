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

// GAMBO-native NNUE network wrapper for integration with Stockfish evaluation pipeline

#ifndef GAMBO_NNUE_NETWORK_H_INCLUDED
#define GAMBO_NNUE_NETWORK_H_INCLUDED

#include <cstdint>
#include <iosfwd>
#include <string>

#include "../nnue_common.h"
#include "architecture.h"

namespace Gambo::NNUE {

// Output structure compatible with Stockfish's NetworkOutput
struct NetworkOutput {
    Value psqt;       // PSQT-style evaluation (compatible with Stockfish)
    Value positional; // Positional evaluation (compatible with Stockfish)
    
    NetworkOutput() : psqt(0), positional(0) {}
    NetworkOutput(Value p, Value pos) : psqt(p), positional(pos) {}
};

// GAMBO-native network wrapper
class Network {
public:
    Network() = default;
    
    // Load network from file
    bool load(const std::string& path);
    
    // Save network to file
    bool save(std::ostream& stream) const;
    
    // Main evaluation function - compatible with Stockfish interface
    NetworkOutput evaluate(const Position&           pos,
                           AccumulatorStack&         accumulatorStack,
                           AccumulatorCaches&        cache) const;
    
    // Verify network compatibility
    bool verify(const std::string& path) const;
    
    // Get content hash for validation
    std::size_t get_content_hash() const;
    
    // Get architecture reference for advanced usage
    const NetworkArchitecture& get_architecture() const { return architecture; }
    
    // Network description
    std::string description() const;

private:
    NetworkArchitecture architecture;
    
    // Feature extractor state (populated during evaluation)
    mutable TransformedFeatureType transformedFeatures[TransformedFeatureDimensions];
};

}  // namespace Gambo::NNUE

#endif  // GAMBO_NNUE_NETWORK_H_INCLUDED