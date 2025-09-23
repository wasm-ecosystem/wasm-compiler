///
/// @file StackElement.hpp
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
#ifndef STACKELEMENT_HPP
#define STACKELEMENT_HPP

#include <cassert>
#include <cstdint>

#include "OPCode.hpp"
#include "StackType.hpp"
#include "util.hpp"

#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/RegAdapter.hpp"
#include "src/core/compiler/common/ListIterator.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"

namespace vb {
/// @brief control flow related state
struct ControlFlowState {
  uint32_t checkedStackFrameSize; ///< compiler do not need to emit stack check code when increase stack size smaller
                                  ///< than checked stack size.
};

///
/// @brief StackElements that represent elements on a (reduced) operand stack
//
/// These elements can be present on the compiler stack, but can also be used as (more) temporary variables
///
class StackElement final {
public:
  static constexpr uint32_t tempStackSlotSize{8U}; ///< temp stack slot size, currently only 8 since there is no SIMD
  StackType type;                                  ///< Type of this StackElement
  List_iterator<StackElement> parent;              ///< parent node in the valent block tree
  List_iterator<StackElement> sibling;             ///< left sibling node in the valent block tree

  /// @brief DeferredAction as StackElement
  struct DeferredAction final {
    OPCode opcode;       ///< OPCode of the instruction for which emission has been deferred
    uint16_t sideEffect; ///< Side effect of the instruction
    uint32_t dataOffset; ///< offset of memory load, currently only used for memory load instructions
  };

  /// @brief Actual data of this StackElement, the StackType defines which member is active
  // coverity[autosar_cpp14_a11_0_1_violation]
  union Data {
    DeferredAction deferredAction; ///< OPCode of the instruction for which emission has been deferred (active if type is DEFERREDACTION)

    /// @brief Data about the variable that is stored in this StackElement (if type is LOCAL, GLOBAL, SCRATCHREGISTER or
    /// TEMPSTACK)
    class VariableData final {
    public:
      /// @brief Calculation result which storages on storage of Locals and Globals.
      struct CalculationResult final {
        /// @brief Ether register or memory
        union ResultLocation {
          TReg reg;                    ///< Register
          uint32_t stackFramePosition; ///< Offset in the current function stack frame
          uint32_t linkDataOffset;     ///< Offset in the link data in the job memory
        };
        ResultLocation resultLocation; ///< Location where the result of the calculation is stored
        uint32_t referencePosition;    ///< Offset in the reference linked list header
        StorageType storageType;       ///< Storage type of the result (e.g. STACKMEMORY and Register)
        MachineType machineType;       ///< machineType of the result.
      };
      /// @brief Defines where this variable is stored
      /// @note the linked list can't use pointer because the address can be changed by realloc
      union Location {
        uint32_t localIdx;                   ///< Index of this local variable (if type is LOCAL)
        uint32_t globalIdx;                  ///< Index of this global variable (if type is GLOBAL)
        TReg reg;                            ///<  CPU register where this temporary variable is stored (Index defined by the backend, if type is
                                             ///<  SCRATCHREGISTER)
        CalculationResult calculationResult; ///< The calculation result which reuses the storage of local and global
      };
      Location location; ///< Location where this variable is stored

      /// @brief Linked list to quickly iterate copies of variables on the stack (e.g. when spilling variables)
      class IndexData final {
      public:
        List_iterator<StackElement> prevOccurrence;     ///< iterator on the stack of the previous occurrence/copy (not necessarily in order on
                                                        ///< the stack)
        List_iterator<StackElement> nextOccurrence;     ///< iterator on the stack of the next occurrence/copy (not necessarily in order on the stack)
        List_iterator<StackElement> nextLowerTempStack; ///< iterator of the temporary stack variable with the next lower stack offset (only
                                                        ///< active for TEMPSTACK elements)
      };
      IndexData indexData; ///< Data enabling linked-list traversals of copies of variables on the stack
    };
    VariableData variableData; ///< Data defining the variable and the IndexData

    ConstUnion constUnion{}; ///< Value of the constant (if type is CONSTANT)

    /// @brief Data for structural StackElements (if type is BLOCK, LOOP or IFBLOCK)
    class BlockInfo final {
    public:
      /// @brief Reference to positions in the output binary related to this structural element
      union BinaryPosition {
        uint32_t loopStartOffset; ///< Offset in the output binary at the start of a LOOP
        uint32_t lastBlockBranch; ///< Offset in the output binary that encodes the last (forward) branch targeting this
                                  ///< BLOCK or IFBLOCK
      };
      BinaryPosition binaryPosition; ///< Data of this structural element related to the output binary

      uint32_t blockResultsStackOffset;               ///< Stack frame offset that stores the results of this structural element
      List_iterator<StackElement> prevBlockReference; ///< iterator on the stack of the previous structural element ("block" is
      ///< used generally for BLOCK, LOOP and IFBLOCK elements)

      uint32_t sigIndex; ///< Index of the function type this structural element is conforming to

      bool blockUnreachable;        ///< Whether this frame defined by this structural element is marked as unreachable
      uint32_t entryStackFrameSize; ///< Stack frame size at entry of this structural frame

      ControlFlowState endState; ///< control flow information at the end of block
    };
    BlockInfo blockInfo; ///< General information about this structural element (if type is BLOCK, LOOP or IFBLOCK)

    uint32_t skipCount; ///< StackElements to be skipped when traversing the compile time stack (if type is SKIP)
  };
  Data data = {}; ///< Data of this StackElement

  ///
  /// @brief Get the base StackType
  ///
  /// @return StackType Base StackType (SCRATCHREGISTER, TEMPRESULT, CONSTANT, LOCAL, GLOBAL etc.) without type flag
  StackType getBaseType() const VB_NOEXCEPT {
    return type & StackType::BASEMASK;
  }

  /// @brief check whether this StackElement is a stack memory
  /// @return true if this StackElement is a stack memory, false otherwise
  bool isStackMemory() const VB_NOEXCEPT {
    return (getBaseType() == StackType::TEMP_RESULT) && (data.variableData.location.calculationResult.storageType == StorageType::STACKMEMORY);
  }

  ///
  /// @brief Checks whether two StackElements are representing the same data
  ///
  /// @param lhs Left hand side StackElement
  /// @param rhs Right hand side StackElement
  /// @return bool Whether the StackElements are representing the same data
  static constexpr bool equalsVariable(StackElement const *const lhs, StackElement const *const rhs) VB_NOEXCEPT {
    if ((lhs == nullptr) || (rhs == nullptr)) {
      return false;
    }
    if (lhs->type == StackType::INVALID) {
      return false;
    }

    if (lhs->type == rhs->type) {
      StackType const baseType{lhs->getBaseType()};
      if (static_cast<uint32_t>(baseType) <= StackType::GLOBAL) {
        if (baseType == StackType::CONSTANT) {
          static_assert(sizeof(rhs->data.constUnion) == 8U, "Wrong size for union");
          size_t const actualConstantWidth{MachineTypeUtil::getSize(MachineTypeUtil::fromStackTypeFlag(lhs->type))};
          // coverity[autosar_cpp14_a12_0_2_violation]
          return std::memcmp(&lhs->data.constUnion, &rhs->data.constUnion, actualConstantWidth) == 0;
        } else if (baseType == StackType::TEMP_RESULT) {
          Data::VariableData::CalculationResult const &lhsResult{lhs->data.variableData.location.calculationResult};
          Data::VariableData::CalculationResult const &rhsResult{rhs->data.variableData.location.calculationResult};
          if (lhsResult.storageType == rhsResult.storageType) {
            if (lhsResult.storageType == StorageType::REGISTER) {
              return lhsResult.resultLocation.reg == rhsResult.resultLocation.reg;
            } else if (lhsResult.storageType == StorageType::LINKDATA) {
              return lhsResult.resultLocation.linkDataOffset == rhsResult.resultLocation.linkDataOffset;
            } else {
              return lhsResult.resultLocation.stackFramePosition == rhsResult.resultLocation.stackFramePosition;
            }
          }
          return false;
        } else {
          // coverity[autosar_cpp14_a12_0_2_violation]
          return std::memcmp(&lhs->data.variableData.location, &rhs->data.variableData.location, static_cast<size_t>(4U)) == 0;
        }
      }
    }
    return false;
  }

  ///
  /// @brief Generator function for a BLOCK StackElement
  ///
  /// @param lastBlockBranch Offset in the output binary that encodes the last (forward) branch targeting this BLOCK
  /// @param blockResultsStackOffset Stack frame offset that stores the results of this structural element
  /// @param prevBlockReference The reference of last structural element
  /// @param sigIndex Index of the function type this BLOCK is conforming to
  /// @param entryStackFrameSize Stack frame size at entry of this BLOCK
  /// @param unreachable Whether this block is unreachable
  /// @return StackElement Generated BLOCK StackElement
  static inline constexpr StackElement block(uint32_t const lastBlockBranch, uint32_t const blockResultsStackOffset,
                                             List_iterator<StackElement> const prevBlockReference, uint32_t const sigIndex,
                                             uint32_t const entryStackFrameSize, bool const unreachable) VB_NOEXCEPT {
    StackElement block{};
    block.type = StackType::BLOCK;
    block.data.blockInfo.binaryPosition.lastBlockBranch = lastBlockBranch;
    block.data.blockInfo.blockResultsStackOffset = blockResultsStackOffset;
    block.data.blockInfo.prevBlockReference = prevBlockReference;
    block.data.blockInfo.sigIndex = sigIndex;
    block.data.blockInfo.blockUnreachable = unreachable;
    block.data.blockInfo.entryStackFrameSize = entryStackFrameSize;
    block.data.blockInfo.endState.checkedStackFrameSize = UINT32_MAX;
    return block;
  }

  ///
  /// @brief Generator function for a LOOP StackElement
  ///
  /// @param loopStartOffset Offset in the output binary at the start of this LOOP
  /// @param blockResultsStackOffset Stack frame offset that stores the results of this structural element
  /// @param prevBlockReference The reference of last structural element
  /// @param sigIndex Index of the function type this LOOP is conforming to
  /// @param entryStackFrameSize Stack frame size at entry of this LOOP
  /// @param unreachable Whether this block is unreachable
  /// @return StackElement Generated LOOP StackElement
  static inline constexpr StackElement loop(uint32_t const loopStartOffset, uint32_t const blockResultsStackOffset,
                                            List_iterator<StackElement> const prevBlockReference, uint32_t const sigIndex,
                                            uint32_t const entryStackFrameSize, bool const unreachable) VB_NOEXCEPT {
    StackElement block{};
    block.type = StackType::LOOP;
    block.data.blockInfo.binaryPosition.loopStartOffset = loopStartOffset;
    block.data.blockInfo.blockResultsStackOffset = blockResultsStackOffset;
    block.data.blockInfo.prevBlockReference = prevBlockReference;
    block.data.blockInfo.sigIndex = sigIndex;
    block.data.blockInfo.blockUnreachable = unreachable;
    block.data.blockInfo.entryStackFrameSize = entryStackFrameSize;
    block.data.blockInfo.endState.checkedStackFrameSize = UINT32_MAX;
    return block;
  }

  ///
  /// @brief Generator function for an IFBLOCK StackElement
  ///
  /// @param lastBlockBranch Offset in the output binary that encodes the last (forward) branch targeting this IFBLOCK
  /// @param blockResultsStackOffset Stack frame offset that stores the results of this structural element
  /// @param prevBlockReference The reference of last structural element
  /// @param sigIndex Index of the function type this IFBLOCK is conforming to
  /// @param entryStackFrameSize Stack frame size at entry of this IFBLOCK
  /// @param unreachable Whether outer block is unreachable
  /// @return StackElement Generated IFBLOCK StackElement
  static inline constexpr StackElement ifblock(uint32_t const lastBlockBranch, uint32_t const blockResultsStackOffset,
                                               List_iterator<StackElement> const prevBlockReference, uint32_t const sigIndex,
                                               uint32_t const entryStackFrameSize, bool const unreachable) VB_NOEXCEPT {
    StackElement block{};
    block.type = StackType::IFBLOCK;
    block.data.blockInfo.binaryPosition.lastBlockBranch = lastBlockBranch;
    block.data.blockInfo.blockResultsStackOffset = blockResultsStackOffset;
    block.data.blockInfo.prevBlockReference = prevBlockReference;
    block.data.blockInfo.sigIndex = sigIndex;
    block.data.blockInfo.blockUnreachable = unreachable;
    block.data.blockInfo.entryStackFrameSize = entryStackFrameSize;
    block.data.blockInfo.endState.checkedStackFrameSize = UINT32_MAX;
    return block;
  }

  ///
  /// @brief Generator function for an i32 CONSTANT StackElement
  ///
  /// @param value Value of this constant
  /// @return StackElement Generated CONSTANT StackElement
  static inline constexpr StackElement i32Const(uint32_t const value) VB_NOEXCEPT {
    StackElement res{};
    res.type = StackType::CONSTANT_I32;
    res.data.constUnion.u32 = value;
    return res;
  }

  ///
  /// @brief Generator function for an i64 CONSTANT StackElement
  ///
  /// @param value Value of this constant
  /// @return StackElement Generated CONSTANT StackElement
  static inline constexpr StackElement i64Const(uint64_t const value) VB_NOEXCEPT {
    StackElement res{};
    res.type = StackType::CONSTANT_I64;
    res.data.constUnion.u64 = value;
    return res;
  }

  ///
  /// @brief Generator function for an f32 CONSTANT StackElement
  ///
  /// @param value Value of this constant
  /// @return StackElement Generated CONSTANT StackElement
  static inline constexpr StackElement f32Const(float const value) VB_NOEXCEPT {
    StackElement res{};
    res.type = StackType::CONSTANT_F32;
    res.data.constUnion.f32 = value;
    return res;
  }

  ///
  /// @brief Generator function for an f64 CONSTANT StackElement
  ///
  /// @param value Value of this constant
  /// @return StackElement Generated CONSTANT StackElement
  static inline constexpr StackElement f64Const(double const value) VB_NOEXCEPT {
    StackElement res{};
    res.type = StackType::CONSTANT_F64;
    res.data.constUnion.f64 = value;
    return res;
  }

  ///
  /// @brief Generate a dummy constant (i32.const, i64.const ...) StackElement corresponding to the given MachineType
  ///
  /// @param type Corresponding MachineType
  /// @return StackElement Dummy constant StackElement
  ///
  static inline StackElement dummyConst(MachineType const type) VB_NOEXCEPT {
    switch (type) {
    case MachineType::F64:
      return StackElement::f64Const(0.0);
    case MachineType::F32:
      return StackElement::f32Const(0.0F);
    case MachineType::I64:
      return StackElement::i64Const(0U);
    case MachineType::I32:
      return StackElement::i32Const(0U);
    default:
      UNREACHABLE(return StackElement{}, "unknown dummy const type");
    }
  }

  ///
  /// @brief Generator function for a SCRATCHREGISTER StackElement
  /// @tparam T Type of the right-hand side operand, must be either StackType or uint32_t
  /// @param reg CPU register where this temporary variable is stored (Index defined by the backend)
  /// @param typeFlag StackType for the underlying variable
  /// @return StackElement Generated SCRATCHREGISTER StackElement
  template <typename T> static inline constexpr StackElement scratchReg(TReg const reg, T const typeFlag) VB_NOEXCEPT {
    static_assert(std::is_same<T, StackType>::value || std::is_same<T, uint32_t>::value, "T must be either StackType or uint32_t");
    StackElement res{};
    res.type = StackType::SCRATCHREGISTER | static_cast<uint32_t>(typeFlag);
    res.data.variableData.location.reg = reg;
    return res;
  }

  ///
  /// @brief Generator function for a LOCAL StackElement
  ///
  /// @param localIdx Index of this local variable
  /// @return StackElement Generated LOCAL StackElement
  static inline constexpr StackElement local(uint32_t const localIdx) VB_NOEXCEPT {
    StackElement res{};
    res.type = StackType::LOCAL;
    res.data.variableData.location.localIdx = localIdx;
    return res;
  }

  ///
  /// @brief Generator function for a Stack Element which uses storage(register) of a local variable
  ///
  /// @param machineType MachineType of the variable
  /// @param storage Storage of the variable
  /// @param referencePosition Offset in the reference linked list header
  /// @return StackElement Generated StackElement
  static inline StackElement tempResult(MachineType const machineType, VariableStorage const &storage, uint32_t const referencePosition) VB_NOEXCEPT {
    StackElement res{};
    res.type = StackType::TEMP_RESULT | static_cast<uint32_t>(MachineTypeUtil::toStackTypeFlag(machineType));
    res.data.variableData.location.calculationResult.referencePosition = referencePosition;
    res.data.variableData.location.calculationResult.storageType = storage.type;
    res.data.variableData.location.calculationResult.machineType = storage.machineType;
    if (storage.type == StorageType::REGISTER) {
      res.data.variableData.location.calculationResult.resultLocation.reg = storage.location.reg;
    } else if (storage.type == StorageType::LINKDATA) {
      res.data.variableData.location.calculationResult.resultLocation.linkDataOffset = storage.location.linkDataOffset;
    } else {
      assert(storage.type == StorageType::STACKMEMORY);
      res.data.variableData.location.calculationResult.resultLocation.stackFramePosition = storage.location.stackFramePosition;
    }
    return res;
  }

  ///
  /// @brief Generator function for a GLOBAL StackElement
  ///
  /// @param globalIdx Index of this global variable
  /// @return StackElement Generated GLOBAL StackElement
  static inline constexpr StackElement global(uint32_t const globalIdx) VB_NOEXCEPT {
    StackElement res{};
    res.type = StackType::GLOBAL;
    res.data.variableData.location.globalIdx = globalIdx;
    return res;
  }

  ///
  /// @brief Generator function for an INVALID StackElement
  ///
  /// @return StackElement Generated INVALID StackElement
  static inline constexpr StackElement invalid() VB_NOEXCEPT {
    StackElement res{};
    res.type = StackType::INVALID;
    return res;
  }

  ///
  /// @brief Generator function for a DEFERREDACTION StackElement with only opcode
  ///
  /// @param instruction OPCode of the instruction for which emission has been deferred
  /// @return StackElement Generated DEFERREDACTION StackElement
  static inline constexpr StackElement action(OPCode const instruction) VB_NOEXCEPT {
    return action(instruction, 0U, 0U);
  }

  ///
  /// @brief Generator function for a DEFERREDACTION StackElement
  ///
  /// @param instruction OPCode of the instruction for which emission has been deferred
  /// @param sideEffect Side effect of the instruction
  /// @param dataOffset Offset of memory load
  /// @return StackElement Generated DEFERREDACTION StackElement
  static inline constexpr StackElement action(OPCode const instruction, uint16_t const sideEffect, uint32_t const dataOffset) VB_NOEXCEPT {
    StackElement res{};
    res.type = StackType::DEFERREDACTION;
    res.data.deferredAction.opcode = instruction;
    res.data.deferredAction.sideEffect = sideEffect;
    res.data.deferredAction.dataOffset = dataOffset;
    return res;
  }

  /// @brief check stack element is a constant with value 0
  /// @return bool true if the stack element is a constant with value 0, false otherwise
  bool isConstantZero() const VB_NOEXCEPT {
    if (getBaseType() == StackType::CONSTANT) {
      switch (static_cast<uint32_t>(type & StackType::TYPEMASK)) {
      case StackType::I32:
        return data.constUnion.u32 == 0U;
      case StackType::I64:
        return data.constUnion.u64 == 0U;
      case StackType::F32:
        return bit_cast<uint32_t>(data.constUnion.f32) == 0U;
      case StackType::F64:
        return bit_cast<uint64_t>(data.constUnion.f64) == 0U;
      default:
        break;
      }
    }
    return false;
  }
};
} // namespace vb

#endif
