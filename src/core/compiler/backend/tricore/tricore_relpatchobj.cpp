///
/// @file tricore_relpatchobj.cpp
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

#ifdef JIT_TARGET_TRICORE
#include <cassert>
#include <cstdint>

#include "tricore_assembler.hpp"
#include "tricore_relpatchobj.hpp"

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"
#include "src/core/compiler/backend/tricore/tricore_instruction.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/SafeInt.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {
namespace tc {
using Assembler = Tricore_Assembler;

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
    if (is16BitInstr(instruction.getOPCode())) {
      assert(isBranch_ && "Must be branch");
      // Only disp4zx2 can be encoded
      UnsignedInRangeCheck<5U> const rangeCheck{UnsignedInRangeCheck<5U>::check(static_cast<uint32_t>(delta))};
      // 16bits branch instruction can only be used for compiler internal jumps within very short distances.
      assert(rangeCheck.inRange());
      static_cast<void>(instruction.setDisp4zx2(rangeCheck.safeInt()));
      return;
    }

    if (isBranch_) {
      if (instruction.isDisp15x2BranchOffset()) {
        SignedInRangeCheck<16U> const rangeCheck{SignedInRangeCheck<16U>::check(static_cast<int32_t>(delta))};
        if (!rangeCheck.inRange()) {
          throw ImplementationLimitationException(ErrorCode::Conditional_branches_or_lea_can_only_target_offsets_in_the_range___32kB);
        }
        static_cast<void>(instruction.setDisp15sx2(rangeCheck.safeInt()));
      } else {
        SignedInRangeCheck<25U> const rangeCheck{SignedInRangeCheck<25U>::check(static_cast<int32_t>(delta))};
        if (!rangeCheck.inRange()) {
          throw ImplementationLimitationException(ErrorCode::Branches_can_only_target_offsets_in_the_range___16MB);
        }
        static_cast<void>(instruction.setDisp24sx2(rangeCheck.safeInt()));
      }
    } else {
      SignedInRangeCheck<16U> const rangeCheck{SignedInRangeCheck<16U>::check(static_cast<int32_t>(delta))};
      if (!rangeCheck.inRange()) {
        throw ImplementationLimitationException(ErrorCode::Conditional_branches_or_lea_can_only_target_offsets_in_the_range___32kB);
      }
      static_cast<void>(instruction.setOff16sx(rangeCheck.safeInt()));
    }
  }));
}

uint32_t RelPatchObj::getLinkedBinaryPos() const VB_NOEXCEPT {
  assert(initialized_ && "Trying to read from an uninitialized jump");

  OPCodeTemplate const opTemplate{readFromPtr<OPCodeTemplate>(binary_->posToPtr(position_))};
  Instruction const instruction{Instruction(opTemplate, *binary_).setEmitted()};

  int64_t linkedPosition;
  if (is16BitInstr(instruction.getOPCode())) {
    linkedPosition = static_cast<int64_t>(position_) + static_cast<int64_t>(instruction.readDisp4zx2BranchOffset());
  } else {
    linkedPosition = static_cast<int64_t>(position_) + instruction.readDisp15oDisp24x2BranchOffset();
  }
  assert(linkedPosition < UINT32_MAX && "Linked position out of bounds");

  return static_cast<uint32_t>(linkedPosition);
}

} // namespace tc
} // namespace vb
#endif
