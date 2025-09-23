///
/// @file aarch64_aux.cpp
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
// coverity[autosar_cpp14_a16_2_2_violation]
#include "src/config.hpp"

#ifdef JIT_TARGET_AARCH64
#include <cassert>
#include <cstdint>

#include "aarch64_aux.hpp"

#include "src/core/common/util.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {
namespace aarch64 {

///
/// @brief Checks whether the value is a contiguous sequence of ones starting at the LSB with the remainder being zero
///
/// @param value Value to check
/// @return bool Whether the value fulfills the requirement
static bool isMask_64(uint64_t const value) VB_NOEXCEPT {
  return (value != 0U) && (((value + 1U) & value) == 0U);
}

///
/// @brief Checks whether the value is a contiguous sequence of ones with the remainder being zero
///
/// The sequence of ones does not need to start at the LSB
///
/// @param value Value to check
/// @return bool Whether the value fulfills the requirement
static bool isShiftedMask_64(uint64_t const value) VB_NOEXCEPT {
  return (value != 0U) && isMask_64((value - 1U) | value);
}

bool processLogicalImmediate(uint64_t imm, bool const is64, uint64_t &encoding) VB_NOEXCEPT {
  uint64_t const maxRegValue{is64 ? UINT64_MAX : UINT32_MAX};

  // 0 and ~0 can't be encoded this way
  if ((imm == 0U) || (imm >= maxRegValue)) {
    return false;
  }

  uint64_t elemSize{is64 ? 64_U64 : 32_U64};
  while (elemSize > 2U) {
    elemSize /= 2U;
    uint64_t const mask{(1_U64 << elemSize) - 1U};
    if ((imm & mask) != ((imm >> elemSize) & mask)) {
      elemSize = elemSize * 2U;
      break;
    }
  }

  // Second, determine the rotation to make the element be: 0^m 1^n.
  uint64_t const mask{(elemSize == 64U) ? 0U : ((1_U64 << elemSize) - 1U)};
  imm &= mask;

  // Extract single element
  uint64_t CTO;
  uint64_t I;
  if (isShiftedMask_64(imm)) {
    I = static_cast<uint64_t>(ctzll(imm));
    assert(I < 64 && "undefined behavior");
    CTO = static_cast<uint64_t>(ctzll(~(imm >> I)));
  } else {
    imm |= ~mask;
    if (!isShiftedMask_64(~imm)) {
      return false;
    }
    uint64_t const CLO{static_cast<uint64_t>(clzll(~imm))};
    I = 64_U64 - CLO;
    uint64_t const CTOimm{static_cast<uint64_t>(ctzll(~imm))};
    CTO = (CLO + CTOimm) - (64_U64 - elemSize);
  }

  // Encode in Immr the number of RORs it would take to get *from* 0^m 1^n to
  // our target value, where I is the number of RORs to go the opposite direction.
  assert(elemSize > I && "I should be smaller than element size");
  uint64_t const Immr{(elemSize - I) & (elemSize - 1U)};

  // If size has a 1 in the n'th bit, create a value that has zeroes in bits [0, n] and ones above that.
  uint64_t NImms{~(elemSize - 1U) << 1U};

  // Or the CTO value into the low bits, which must be below the Nth bit mentioned above.
  NImms |= CTO - 1U;

  // Extract the seventh bit and toggle it to create the N field.
  uint64_t const N{((NImms >> 6U) & 1U) ^ 1U};

  encoding = (N << 12U) | (Immr << 6U) | (NImms & 0x3FU);
  return true;
}

} // namespace aarch64
} // namespace vb
#endif
