///
/// @file x86_64_relpatchobj.cpp
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

#ifdef JIT_TARGET_X86_64
#include <cassert>
#include <cstdint>

#include "x86_64_relpatchobj.hpp"

#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/common/MemWriter.hpp"

namespace vb {
namespace x86_64 {

// Optional dummy RelPatchObj to allow empty constructor initialization and to
// be able to check whether it's "empty" via isDummy() RelPatchObj will allow
// to emit a JMP with zero-offset that can be patched later on simply by
// calling set...() methods A proper RelPatchObj consists of a flag saying
// whether it is a "short" (8-bit) or a "long" (32-bit) jmp instruction, the
// position offset of binary AFTER the instruction so we can go back and easily
// patch it, knowing the offset is at the end and whether its 8-bit or 32-bit
// ShortJmp should only be used when it is guaranteed (by the programmer's
// logic) that only offsets within 8-bit signed offsets will be patched,
// otherwise use a "long" jmp
// coverity[autosar_cpp14_a12_1_5_violation] initial binary_ with nullptr
RelPatchObj::RelPatchObj() VB_NOEXCEPT : positionAfterInstruction_(0U), binary_(nullptr), initialized_(false), short_(false) {
}
RelPatchObj::RelPatchObj(bool const isShort, uint32_t const positionAfterInstruction, MemWriter &binary) VB_NOEXCEPT
    : positionAfterInstruction_(positionAfterInstruction),
      binary_(&binary),
      initialized_(true),
      short_(isShort) {
}

// Sets the target to the current end of the binary
void RelPatchObj::linkToHere() const {
  assert(initialized_ && "Trying to write to an uninitialized jump");
  linkToBinaryPos(binary_->size());
}

// Sets the target of the instruction to a certain machine code offset
// (binaryPosition is counted from the base of the machine code binary)
void RelPatchObj::linkToBinaryPos(uint32_t const binaryPosition) const {
  assert(initialized_ && "Trying to write to an uninitialized jump");
  uint8_t *const ptrAfterInstruction{binary_->posToPtr(positionAfterInstruction_)};
  int64_t const delta{static_cast<int64_t>(binaryPosition) - static_cast<int64_t>(positionAfterInstruction_)};

  if (short_) {
    // Programmer error, should never happen. A developer should only pass
    // isShort if he is 100% sure it will be in range
    assert(delta >= INT8_MIN && delta <= INT8_MAX && "JMP offset does not fit into 8-bits");
    // Guaranteed not to overflow due to check above, replace the last four
    // bytes of the instruction by the new offset (delta)
    writeToPtr<int8_t>(pSubI(ptrAfterInstruction, 1U), static_cast<int8_t>(delta));
  } else {
    // Could technically happen if the module is HUUUUGE, we don't need to support that
    if ((delta <= INT32_MIN) || (delta >= INT32_MAX)) {
      throw ImplementationLimitationException(ErrorCode::BrHANDLE_ERRORanches_can_only_maximally_target_32_bit_signed_offsets);
    } // GCOVR_EXCL_LINE
    writeToPtr<int32_t>(pSubI(ptrAfterInstruction, 4U), static_cast<int32_t>(delta));
  }
}

// Returns the currently targeted machine code offset (so the offset from the
// start of the machine code binary of the instruction this JMP targets)
uint32_t RelPatchObj::getLinkedBinaryPos() const VB_NOEXCEPT {
  assert(initialized_ && "Trying to read from an uninitialized jump");
  uint8_t const *const ptrAfterInstruction{binary_->posToPtr(positionAfterInstruction_)};
  int64_t returnValue;
  // GCOVR_EXCL_START
  // Currently not implemented
  if (short_) {
    returnValue = static_cast<int64_t>(positionAfterInstruction_) + readFromPtr<int8_t>(pSubI(ptrAfterInstruction, 1));
  } else {
    // GCOVR_EXCL_STOP
    returnValue = static_cast<int64_t>(positionAfterInstruction_) + readFromPtr<int32_t>(pSubI(ptrAfterInstruction, 4));
  }
  assert(returnValue <= UINT32_MAX && "Binary position out of bounds");
  return static_cast<uint32_t>(returnValue);
}

} // namespace x86_64
} // namespace vb
#endif
