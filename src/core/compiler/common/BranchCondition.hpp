///
/// @file BranchCondition.hpp
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
#ifndef BRANCHCONDITION_HPP
#define BRANCHCONDITION_HPP

#include <cassert>
#include <cstdint>

#include "src/core/common/util.hpp"

namespace vb {

///
/// @brief Abstract branch condition for WebAssembly comparisons
///
enum class BranchCondition : uint8_t {
  NEQZ,
  EQZ,
  EQ,
  NE,
  LT_S,
  LT_U,
  GT_S,
  GT_U,
  LE_S,
  LE_U,
  GE_S,
  GE_U,

  EQ_F,
  NE_F,
  LT_F,
  GT_F,
  LE_F,
  GE_F,

  UNCONDITIONAL
};
using BC = BranchCondition; ///< Shortcut for BranchCondition

///
/// @brief Reverse the branch condition
///
/// GT for example becomes LT (NOT LE; this is no inversion, but rather a reversion/directional switch)
///
/// @param branchCond
/// @return BC
inline BC reverseBC(BC const branchCond) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto reversedBC = make_array(BC::NEQZ, BC::EQZ, BC::EQ, BC::NE, BC::GT_S, BC::GT_U, BC::LT_S, BC::LT_U, BC::GE_S, BC::GE_U, BC::LE_S,
                                         BC::LE_U, BC::EQ_F, BC::NE_F, BC::GT_F, BC::LT_F, BC::GE_F, BC::LE_F, BC::UNCONDITIONAL);
  assert(static_cast<uint8_t>(branchCond) < reversedBC.size() && "Unknown branch condition");
  return reversedBC[static_cast<uint8_t>(branchCond)];
}

///
/// @brief Negate the branch condition
///
/// GT for example becomes LE (NOT LT; this is no reversion, but rather an inversion/negation)
///
/// @param branchCond
/// @return BC
inline BC negateBC(BC const branchCond) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto reversedBC = make_array(BC::EQZ, BC::NEQZ, BC::NE, BC::EQ, BC::GE_S, BC::GE_U, BC::LE_S, BC::LE_U, BC::GT_S, BC::GT_U, BC::LT_S,
                                         BC::LT_U, BC::NE_F, BC::EQ_F, BC::GE_F, BC::LE_F, BC::GT_F, BC::LT_F, BC::UNCONDITIONAL);
  assert(static_cast<uint8_t>(branchCond) < reversedBC.size() && "Unknown branch condition");
  return reversedBC[static_cast<uint8_t>(branchCond)];
}

} // namespace vb

#endif
