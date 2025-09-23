///
/// @file aarch64_relpatchobj.cpp
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

#include "aarch64_assembler.hpp"
#include "aarch64_relpatchobj.hpp"

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_encoding.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_instruction.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/SafeInt.hpp"

namespace vb {
namespace aarch64 {
using Assembler = AArch64_Assembler;

// coverity[autosar_cpp14_a12_1_5_violation] initial binary_ with nullptr
RelPatchObj::RelPatchObj() VB_NOEXCEPT : position_{0U}, binary_{nullptr}, initialized_{false}, isBranch_(true) {
}
RelPatchObj::RelPatchObj(uint32_t const position, MemWriter &binary, bool const isBranch) VB_NOEXCEPT : position_{position},
                                                                                                        binary_{&binary},
                                                                                                        initialized_{true},
                                                                                                        isBranch_(isBranch) {
}

void RelPatchObj::linkToHere() const {
  assert(initialized_ && "Trying to write to an uninitialized jump");

  linkToBinaryPos(binary_->size());
}

void RelPatchObj::linkToBinaryPos(uint32_t const binaryPosition) const {
  assert(initialized_ && "Trying to write to an uninitialized jump");

  int64_t const delta{static_cast<int64_t>(binaryPosition) - static_cast<int64_t>(position_)};

  Assembler::patchInstructionAtOffset(*binary_, position_, FunctionRef<void(Instruction & instruction)>([delta, this](Instruction &instruction) {
    if (isBranch_) {
      if (instruction.isImm19ls2BranchOffset()) {
        SignedInRangeCheck<21U> const rangeCheck{SignedInRangeCheck<21U>::check(delta)};
        if (rangeCheck.inRange()) {
          static_cast<void>(instruction.setImm19ls2BranchOffset(rangeCheck.safeInt()));
        } else {
          throw ImplementationLimitationException(ErrorCode::Conditional_branches_or_adr_can_only_target_offsets_in_the_range___1MB);
        }
      } else {
        SignedInRangeCheck<28U> const rangeCheck{SignedInRangeCheck<28U>::check(delta)};
        if (rangeCheck.inRange()) {
          static_cast<void>(instruction.setImm26ls2BranchOffset(rangeCheck.safeInt()));
        } else {
          throw ImplementationLimitationException(ErrorCode::Branches_can_only_target_offsets_in_the_range___128MB);
        }
      }
    } else {
      SignedInRangeCheck<21U> const rangeCheck{SignedInRangeCheck<21U>::check(delta)};
      if (rangeCheck.inRange()) {
        static_cast<void>(instruction.setSigned21AddressOffset(rangeCheck.safeInt()));
      } else {
        throw ImplementationLimitationException(ErrorCode::Conditional_branches_or_adr_can_only_target_offsets_in_the_range___1MB);
      }
    }
  }));
}

uint32_t RelPatchObj::getLinkedBinaryPos() const VB_NOEXCEPT {
  assert(initialized_ && "Trying to read from an uninitialized jump");
  assert(isBranch_ && "Can only read the linked position for branches");

  OPCodeTemplate const opTemplate{readFromPtr<OPCodeTemplate>(binary_->posToPtr(position_))};
  Instruction const instruction{Instruction(opTemplate, *binary_).setEmitted()};
  int64_t const linkedPosition{static_cast<int64_t>(position_) + instruction.readImm19o26ls2BranchOffset()};
  assert(linkedPosition < UINT32_MAX && "Linked position out of bounds");
  return static_cast<uint32_t>(linkedPosition);
}

} // namespace aarch64
} // namespace vb
#endif
