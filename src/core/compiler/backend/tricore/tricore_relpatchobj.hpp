///
/// @file tricore_relpatchobj.hpp
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
#ifndef TRICORE_RELPATCHOBJ_HPP
#define TRICORE_RELPATCHOBJ_HPP

#include <cstdint>

#include "src/core/common/util.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {
namespace tc {

///
/// @brief RelPatchObj class
///
/// An object storing a reference to instructions like branch or call instructions encoding relative offsets,
/// where the offset is not yet known and can via this method be patched later. Works for conditional and unconditional
/// branches.
///
class RelPatchObj final {
public:
  ///
  /// @brief Construct an empty RelPatchObj, effectively a dummy
  /// NOTE: This will not properly initialize the RelPatchObj and calls to its member functions will lead to undefined
  /// behavior
  ///
  RelPatchObj() VB_NOEXCEPT;

  ///
  /// @brief Construct a RelPatchObj
  ///
  /// @param position Offset of the start of the instruction in the binary
  /// @param binary Reference to the output binary
  /// @param isBranch Whether this is a branch or an ADR instruction
  RelPatchObj(uint32_t const position, MemWriter &binary, bool const isBranch = true) VB_NOEXCEPT;

  ///
  /// @brief Link the referenced instruction in such a way that it will target "here", i.e. the end of the currently
  /// entered instructions in the output binary
  ///
  void linkToHere() const;

  ///
  /// @brief Link the referenced instruction in such a way that it will target a specific position in the output binary
  ///
  /// @param binaryPosition Target position in the output binary
  void linkToBinaryPos(uint32_t const binaryPosition) const;

  ///
  /// @brief Get the currently encoded target position in the output binary from the referenced instruction
  ///
  /// @return uint32_t Current target position of the referenced instruction in the output binary
  uint32_t getLinkedBinaryPos() const VB_NOEXCEPT;

  ///
  /// @brief Whether this RelPatchObj was initialized or is a dummy RelPatchObj
  ///
  /// @return bool Whether it was initialized
  inline bool isInitialized() const VB_NOEXCEPT {
    return initialized_;
  };

  ///
  /// @brief Get the position of the referenced instruction in the output binary
  ///
  /// @return uint32_t Position of the referenced instruction in the output binary
  inline uint32_t getPosOffsetBeforeInstr() const VB_NOEXCEPT {
    return position_;
  }

private:
  ///
  /// @brief Position of the start of the referenced instruction in the output binary
  uint32_t position_;

  ///
  /// @brief Reference to the output binary
  MemWriter *binary_;

  ///
  /// @brief Whether this RelPatchObj has been initialized or not
  ///
  /// Non-initialized RelPatchObjs have been created as dummies and do not reference anything, initialized RelPatchObjs
  /// always reference an actual instruction
  bool initialized_;

  ///
  /// @brief Whether this is a branch or a LEA instruction (i.e. preparePCToA11)
  bool isBranch_;
};

} // namespace tc
} // namespace vb

#endif
