///
/// @file ParallelMoveCost.hpp
/// @copyright Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
/// SPDX-License-Identifier: Apache-2.0
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
#ifndef Parallel_Move_Cost_HPP
#define Parallel_Move_Cost_HPP

#include "src/config.hpp"

#ifdef JIT_TARGET_X86_64

#include <cstdint>

namespace vb {

class Compiler;

namespace x86_64 {
///
/// @brief Cost model for move resolver
///
// coverity[autosar_cpp14_m3_4_1_violation]
class MoveResolverCost final {
public:
  /// @brief Swap type for move resolver
  enum class SwapType : uint8_t {
    XCHG,     ///< Use XCHG instruction to swap two registers
    TEMP_REG, ///< Use a temporary register to handle the cycle
  };
  /// @brief Get the swap type for move resolver
  /// @todo Implement a cost model to determine the best swap type based on the input parameters
  /// @return SwapType swap type
  static inline SwapType getSwapType(/* cost input */) VB_NOEXCEPT {
    return SwapType::XCHG;
  }
};

} // namespace x86_64
} // namespace vb

#endif
#endif /* Parallel_Move_Cost_H */
