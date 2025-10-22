///
/// @file Common.cpp
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
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/core/compiler/common/Common.hpp"

#include "src/config.hpp"
#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/compiler/backend/PlatformAdapter.hpp"
#include "src/core/compiler/backend/RegAdapter.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_backend.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_cc.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_encoding.hpp"
#include "src/core/compiler/backend/tricore/tricore_backend.hpp"
#include "src/core/compiler/backend/tricore/tricore_cc.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_backend.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_cc.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_encoding.hpp"
#include "src/core/compiler/common/BranchCondition.hpp"
#include "src/core/compiler/common/ListIterator.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/OPCode.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {
namespace {} // namespace

uint32_t Common::getArithArity(OPCode const opcode) VB_NOEXCEPT {
  if (opcode == OPCode::SELECT) {
    return 3_U32;
  } else if ((static_cast<uint32_t>(opcode) >= static_cast<uint32_t>(OPCode::I32_LOAD)) &&
             (static_cast<uint32_t>(opcode) <= static_cast<uint32_t>(OPCode::I64_LOAD32_U))) {
    return 1_U32;
  } else {
    assert(opcode >= OPCode::I32_EQZ && opcode <= OPCode::I64_EXTEND32_S);
    return (getArithArgs(opcode).arg1Type == MachineType::INVALID) ? 1_U32 : 2_U32;
  }
}

uint32_t Common::getCurrentMaximumUsedStackFramePosition() const VB_NOEXCEPT {
  Stack::iterator const lastOccurrenceTempStack{compiler_.moduleInfo_.getReferenceToLastOccurrenceOnStackForStackMemory()};
  Stack::iterator const lastBlock{compiler_.moduleInfo_.fnc.lastBlockReference};
  uint32_t const blockResultsStackOffset{(lastBlock.isEmpty()) ? compiler_.moduleInfo_.getFixedStackFrameWidth()
                                                               : lastBlock->data.blockInfo.blockResultsStackOffset};
  uint32_t const stackFramePosition{(lastOccurrenceTempStack.isEmpty())
                                        ? 0U
                                        : lastOccurrenceTempStack->data.variableData.location.calculationResult.resultLocation.stackFramePosition};
  uint32_t const maximumPosition{std::max(blockResultsStackOffset, stackFramePosition)};
  assert(maximumPosition >= compiler_.moduleInfo_.getFixedStackFrameWidth() && maximumPosition <= compiler_.moduleInfo_.fnc.stackFrameSize &&
         "Stack position error");
  return maximumPosition;
}

bool Common::isWritableScratchReg(StackElement const *const pElem) const VB_NOEXCEPT {
  if (pElem == nullptr) {
    return false;
  }
  if (pElem->getBaseType() != StackType::SCRATCHREGISTER) {
    return false;
  }

  Stack::iterator const lastOccurrenceElement{compiler_.moduleInfo_.getReferenceToLastOccurrenceOnStack(*pElem)};
  // Now check whether the stack contains a copy (that is not equal to the given element)
  if (!lastOccurrenceElement.isEmpty()) {
    // Non-empty index, given element on stack
    assert(lastOccurrenceElement->data.variableData.indexData.nextOccurrence.isEmpty() && "Last occurrence element must not have a next one");

    // If the last occurrence is not the given element or the last occurrence has
    // another (previous) occurrence
    if ((pElem != lastOccurrenceElement.raw()) || (!lastOccurrenceElement->data.variableData.indexData.prevOccurrence.isEmpty())) {
      return false;
    }
  }
  return true;
}

void Common::spillFromStackImpl(StackElement const &source, RegMask const protRegs, bool const forceToStack, bool const presFlags,
                                Stack::iterator const pExcludedZoneBottom, Stack::iterator const pExcludedZoneTop) const {
  assert(!source.isStackMemory() && "Cannot spill temporary stack elements");

  bool const hasExcludedZone{(!pExcludedZoneBottom.isEmpty()) || (!pExcludedZoneTop.isEmpty())};

  if (hasExcludedZone) {
    StackElement newElement{StackElement::invalid()};
    uint32_t const sourceRefPos{compiler_.moduleInfo_.getReferencePosition(source)};
    // coverity[autosar_cpp14_a8_5_2_violation]
    auto const visitor = [this, &newElement, sourceRefPos, protRegs, forceToStack, presFlags](Stack::iterator const begin,
                                                                                              Stack::iterator const end) {
      for (Stack::iterator it{begin}; it != end; it++) {
        if (((it->getBaseType() != StackType::LOCAL) && (it->getBaseType() != StackType::GLOBAL)) &&
            (it->getBaseType() != StackType::SCRATCHREGISTER)) {
          continue;
        }
        if (sourceRefPos == compiler_.moduleInfo_.getReferencePosition(*it)) {
          if (newElement.type == StackType::INVALID) {
            StackElement const spillTarget{compiler_.backend_.reqSpillTarget(*it, protRegs, forceToStack, presFlags)};

#if ENABLE_EXTENSIONS
            if (compiler_.getAnalytics() != nullptr) {
              compiler_.getAnalytics()->incrementSpillCount(spillTarget.getBaseType() == StackType::SCRATCHREGISTER);
            }
#endif
            VariableStorage const srcStorage{compiler_.moduleInfo_.getStorage(*it)};
            VariableStorage const dstStorage{compiler_.moduleInfo_.getStorage(spillTarget)};
            compiler_.backend_.emitMoveImpl(dstStorage, srcStorage, false, presFlags);
            newElement = spillTarget;
          }
          replaceAndUpdateReference(it, newElement);
        }
      }
    };
    if (!pExcludedZoneBottom.isEmpty()) {
      visitor(getCurrentFrameBase(), pExcludedZoneBottom);
    }
    if (!pExcludedZoneTop.isEmpty()) {
      visitor(pExcludedZoneTop, compiler_.stack_.end());
    }
  } else {
    StackElement newElement{StackElement::invalid()};
    Stack::iterator nextElemReference{compiler_.moduleInfo_.getReferenceToLastOccurrenceOnStack(source)};
    while (!nextElemReference.isEmpty()) {
      Stack::iterator const currentElement{nextElemReference};
      nextElemReference = nextElemReference->data.variableData.indexData.prevOccurrence;
      if (newElement.type == StackType::INVALID) {
        StackElement const spillTarget{compiler_.backend_.reqSpillTarget(*currentElement, protRegs, forceToStack, presFlags)};

#if ENABLE_EXTENSIONS
        if (compiler_.getAnalytics() != nullptr) {
          compiler_.getAnalytics()->incrementSpillCount(spillTarget.getBaseType() == StackType::SCRATCHREGISTER);
        }
#endif
        VariableStorage const srcStorage{compiler_.moduleInfo_.getStorage(*currentElement)};
        VariableStorage const dstStorage{compiler_.moduleInfo_.getStorage(spillTarget)};
        compiler_.backend_.emitMoveImpl(dstStorage, srcStorage, false, presFlags);
        newElement = spillTarget;
      }

      replaceAndUpdateReference(currentElement, newElement);
    }
  }
}

void Common::loadReturnValues(Stack::iterator const returnValuesBase, uint32_t const numReturnValues, StackElement const *const targetBlockElem,
                              bool const presFlags) const {
  static_cast<void>(numReturnValues);
  NBackend::RegStackTracker tracker{};
  Stack::iterator currentIt{returnValuesBase};
  uint32_t const stackFramePosition{(targetBlockElem == nullptr) ? 0U : targetBlockElem->data.blockInfo.blockResultsStackOffset};
  RegMask returnValueRegMask{};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto visitor = [this, stackFramePosition, presFlags, &returnValueRegMask, &tracker, &currentIt](MachineType const machineType) {
    StackElement targetElem{};
    TReg const targetReg{compiler_.backend_.getREGForReturnValue(machineType, tracker)};
    if (targetReg != TReg::NONE) {
      targetElem = StackElement::scratchReg(targetReg, MachineTypeUtil::toStackTypeFlag(machineType));
      compiler_.backend_.spillFromStack(targetElem, returnValueRegMask, false, presFlags, currentIt, currentIt.next());
      returnValueRegMask.mask(compiler_.backend_.mask(targetReg, MachineTypeUtil::is64(machineType)));
    } else {
      uint32_t const targetStackFramePosition{stackFramePosition - TBackend::offsetInStackReturnValues(tracker, machineType)};
      targetElem = StackElement::tempResult(machineType, VariableStorage::stackMemory(machineType, targetStackFramePosition),
                                            compiler_.moduleInfo_.getStackMemoryReferencePosition());
    }
    VariableStorage const srcStorage{compiler_.moduleInfo_.getStorage(*currentIt)};
    VariableStorage const dstStorage{compiler_.moduleInfo_.getStorage(targetElem)};
    compiler_.backend_.emitMoveImpl(dstStorage, srcStorage, false, presFlags);
    ++currentIt;
  };

  if (targetBlockElem == nullptr) {
    // branch to function end
    uint32_t const sigIndex{compiler_.moduleInfo_.getFuncDef(compiler_.moduleInfo_.fnc.index).sigIndex};
    // coverity[autosar_cpp14_a5_1_4_violation]
    compiler_.moduleInfo_.iterateResultsForSignature(sigIndex, FunctionRef<void(MachineType)>(visitor));
  } else {
    uint32_t const sigIndex{targetBlockElem->data.blockInfo.sigIndex};
    if (targetBlockElem->type == StackType::LOOP) {
      // branch to loop
      // coverity[autosar_cpp14_a5_1_4_violation]
      compiler_.moduleInfo_.iterateParamsForSignature(sigIndex, FunctionRef<void(MachineType)>(visitor));
    } else {
      // coverity[autosar_cpp14_a5_1_4_violation]
      compiler_.moduleInfo_.iterateResultsForSignature(sigIndex, FunctionRef<void(MachineType)>(visitor));
    }
  }
}

void Common::popReturnValueElems(Stack::iterator const returnValuesBase, uint32_t const numReturnValues) const VB_NOEXCEPT {
  Stack::iterator currentIt{returnValuesBase};
  for (uint32_t i{0U}; i < numReturnValues; i++) {
    removeReference(currentIt);
    currentIt = compiler_.stack_.erase(currentIt);
  }
}

StackElement Common::evaluateInstruction(Stack::iterator const instructionPtr, Stack::iterator const arg0Ptr, Stack::iterator const arg1Ptr,
                                         Stack::iterator const arg2Ptr, StackElement const *const targetHint) const {
#if ENABLE_EXTENSIONS
  if (compiler_.dwarfGenerator_ != nullptr) {
    compiler_.dwarfGenerator_->startOp(instructionPtr.unwrap());
  }
#endif
  OPCode const opCode{instructionPtr->data.deferredAction.opcode};
  StackElement result{};
  if (opCode == OPCode::SELECT) {
    assert(arg1Ptr.unwrap() && arg2Ptr.unwrap() && "Select needs 3 results");

    result = compiler_.backend_.emitSelect(*arg0Ptr, *arg1Ptr, *arg2Ptr, targetHint);

    removeReference(arg1Ptr);
    static_cast<void>(compiler_.stack_.erase(arg1Ptr));
    removeReference(arg2Ptr);
    static_cast<void>(compiler_.stack_.erase(arg2Ptr));

  } else if ((static_cast<uint32_t>(opCode) >= static_cast<uint32_t>(OPCode::I32_LOAD)) &&
             (static_cast<uint32_t>(opCode) <= static_cast<uint32_t>(OPCode::I64_LOAD32_U))) {
    result = compiler_.backend_.executeLinearMemoryLoad(opCode, instructionPtr->data.deferredAction.dataOffset, arg0Ptr, targetHint);
  } else {
    // Regular deferred action: arithmetic etc.
    // coverity[autosar_cpp14_a5_3_2_violation]
    result = compiler_.backend_.emitDeferredAction(instructionPtr->data.deferredAction.opcode, arg0Ptr.raw(), arg1Ptr.raw(), targetHint);

    if (!arg1Ptr.isEmpty()) {
      removeReference(arg1Ptr);
      static_cast<void>(compiler_.stack_.erase(arg1Ptr));
    }
  }
  removeReference(arg0Ptr);
  static_cast<void>(compiler_.stack_.erase(arg0Ptr));
#if ENABLE_EXTENSIONS
  if (compiler_.dwarfGenerator_ != nullptr) {
    compiler_.dwarfGenerator_->finishOp();
  }
#endif
  return result;
}

BranchCondition Common::evaluateCondition(Stack::iterator const instructionPtr, Stack::iterator const arg0Ptr, Stack::iterator const arg1Ptr) const {
  // Emit comparison and set the corresponding branch condition for a later
  // branch or select Invert condition flag if the comparison was reversed
  // coverity[autosar_cpp14_a5_3_2_violation]
  bool const reversed{compiler_.backend_.emitComparison(instructionPtr->data.deferredAction.opcode, arg0Ptr.raw(), arg1Ptr.raw())};
  BC const branchCond{reversed ? reverseBC(BCforOPCode(instructionPtr->data.deferredAction.opcode))
                               : BCforOPCode(instructionPtr->data.deferredAction.opcode)};

  // Remove from stack completely, no result
  if (!arg1Ptr.isEmpty()) {
    removeReference(arg1Ptr);
    static_cast<void>(compiler_.stack_.erase(arg1Ptr));
  }
  removeReference(arg0Ptr);
  static_cast<void>(compiler_.stack_.erase(arg0Ptr));

  return branchCond;
}

Stack::iterator Common::getCurrentFrameBase() const VB_NOEXCEPT {
  if (!compiler_.moduleInfo_.fnc.lastBlockReference.isEmpty()) {
    return compiler_.moduleInfo_.fnc.lastBlockReference.next();
  } else {
    return compiler_.stack_.begin();
  }
}

Stack::iterator Common::findBaseOfValentBlock(Stack::iterator const rootNode) VB_NOEXCEPT {
  Stack::iterator valentBlockTop{rootNode};

  while (valentBlockTop->type == StackType::DEFERREDACTION) {
    valentBlockTop = getFirstOperand(valentBlockTop);
  }

  return valentBlockTop;
}

Stack::iterator Common::findBaseOfValentBlockBelow(Stack::iterator const belowIt) const VB_NOEXCEPT {
  return findBaseOfValentBlock(belowIt.prev());
}

Stack::iterator Common::condenseMultipleValentBlocksBelow(Stack::iterator const belowIt, uint32_t const valentBlockCount) const {
  assert((valentBlockCount > 0) && "Number of valent blocks to condense is zero");
  uint32_t skipCount{valentBlockCount - 1U};
  Stack::iterator resultBase{};

  for (uint32_t i{0U}; i < valentBlockCount; i++) {
    Stack::iterator baseIt{belowIt};
    // coverity[autosar_cpp14_a6_5_1_violation] fake positive
    for (uint32_t j{0U}; j < skipCount; j++) {
      baseIt = findBaseOfValentBlockBelow(baseIt);
    }

    Stack::iterator const condenseResult{condenseValentBlockBelow(baseIt)};

    if (resultBase.isEmpty()) {
      resultBase = condenseResult;
    }
    skipCount--;
  }

  return resultBase;
}

Stack::iterator Common::condenseMultipleValentBlocksWithTargetHintBelow(Stack::iterator const belowIt, uint32_t const sigIndex,
                                                                        bool const isLoop) const {
  NBackend::RegStackTracker tracker{};

  uint32_t numValentBlocks{};
  if (isLoop) {
    numValentBlocks = compiler_.moduleInfo_.getNumParamsForSignature(sigIndex);
  } else {
    numValentBlocks = compiler_.moduleInfo_.getNumReturnValuesForSignature(sigIndex);
  }

  // GCOVR_EXCL_START
  assert(numValentBlocks > 0U);
  // GCOVR_EXCL_STOP

  uint32_t skipCount{numValentBlocks - 1U};
  Stack::iterator resultBase{};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const condenseVisitor = [this, &skipCount, belowIt, &resultBase, &tracker](MachineType const machineType) {
    TReg const targetHintReg{compiler_.backend_.getREGForReturnValue(machineType, tracker)};

    Stack::iterator baseIt{belowIt};
    // coverity[autosar_cpp14_a6_5_1_violation] fake positive
    for (uint32_t i{0U}; i < skipCount; i++) {
      baseIt = findBaseOfValentBlockBelow(baseIt);
    }

    Stack::iterator condenseResult{};
    if (targetHintReg == TReg::NONE) {
      condenseResult = condenseValentBlockBelow(baseIt);
    } else {
      StackElement const targetHint{StackElement::scratchReg(targetHintReg, MachineTypeUtil::toStackTypeFlag(machineType))};
      condenseResult = condenseValentBlockBelow(baseIt, &targetHint);
    }

    if (resultBase.isEmpty()) {
      resultBase = condenseResult;
    }

    skipCount--;
  };

  if (isLoop) {
    // coverity[autosar_cpp14_a5_1_4_violation]
    compiler_.moduleInfo_.iterateParamsForSignature(sigIndex, FunctionRef<void(MachineType)>(condenseVisitor));
  } else {
    // coverity[autosar_cpp14_a5_1_4_violation]
    compiler_.moduleInfo_.iterateResultsForSignature(sigIndex, FunctionRef<void(MachineType)>(condenseVisitor));
  }

  return resultBase;
}

bool Common::checkIfEnforcedTargetIsOnlyInArgs(Span<Stack::iterator> const &args, StackElement const *const enforcedTarget) const VB_NOEXCEPT {
  if (enforcedTarget == nullptr) {
    return true;
  }
  // Now check whether the stack contains a copy (that is not equal to the given element)
  Stack::iterator currentOccurrenceElement{compiler_.moduleInfo_.getReferenceToLastOccurrenceOnStack(*enforcedTarget)};

  while (!currentOccurrenceElement.isEmpty()) {
    if (!args.contains(currentOccurrenceElement)) {
      return false;
    }
    currentOccurrenceElement = currentOccurrenceElement->data.variableData.indexData.prevOccurrence;
  }

  return true;
}

Stack::iterator Common::condenseValentBlockBelow(Stack::iterator const belowIt, StackElement const *const enforcedTarget) const {
  assert(belowIt != compiler_.stack_.begin());
  return condenseValentBlockCoreBelow(false, belowIt, enforcedTarget).base;
}

BC Common::condenseComparisonBelow(Stack::iterator const belowIt) const {
  ConditionResult const result{condenseValentBlockCoreBelow(true, belowIt, nullptr)};
  if (result.base == belowIt) {
    return result.branchCond;
  }
  // we still have a value on the stack, convert it to branch condition
  // Emit comparison to zero
  static_cast<void>(compiler_.backend_.emitComparison(OPCode::I32_EQZ, result.base.unwrap(), nullptr));
  // Condition is positive if the element is not equal to zero
  removeReference(result.base);
  static_cast<void>(compiler_.stack_.erase(result.base));
  return BC::NEQZ;
}

bool Common::scratchRegOnlyOnceOnStack(Stack::iterator const element) VB_NOEXCEPT {
  StackType const baseType{element->getBaseType()};
  if ((baseType == StackType::SCRATCHREGISTER) ||
      ((baseType == StackType::TEMP_RESULT) && (element->data.variableData.location.calculationResult.storageType == StorageType::REGISTER))) {
    StackElement::Data::VariableData::IndexData const &indexData{element->data.variableData.indexData};
    bool const onlyOnceOnStack{indexData.prevOccurrence.isEmpty() && indexData.nextOccurrence.isEmpty()};
    // GCOVR_EXCL_START
    assert((baseType != StackType::TEMP_RESULT) || onlyOnceOnStack);
    // GCOVR_EXCL_STOP
    return onlyOnceOnStack;
  } else {
    return false;
  }
}

void Common::condenseSideEffectInstructionBelow(Stack::iterator const rootNode, StackElement const *const enforcedTarget) const {
  if (rootNode->type != StackType::DEFERREDACTION) {
    return;
  }
  Stack::iterator const valentBlockTop{findBaseOfValentBlock(rootNode)};

  Stack::iterator currentIt{valentBlockTop};

  while (currentIt != rootNode) {
    if ((currentIt->type == StackType::DEFERREDACTION) && (currentIt->data.deferredAction.sideEffect != 0U)) {
      Stack::iterator const instructionPtr{currentIt};

      if (compiler_.backend_.hasEnoughScratchRegForScheduleInstruction(instructionPtr->data.deferredAction.opcode)) {
        condenseValentBlockBasic(instructionPtr, enforcedTarget);
      } else {
        break;
      }
    }

    currentIt = currentIt.next();
  }
}

void Common::condenseValentBlockBasic(Stack::iterator const rootNode, StackElement const *const enforcedTarget) const {
  assert(rootNode->type == StackType::DEFERREDACTION);

  Stack::iterator const valentBlockTop{findBaseOfValentBlock(rootNode)};

  Stack::iterator currentIt{valentBlockTop};

  while (true) {
    if (currentIt->type == StackType::DEFERREDACTION) {
      Stack::iterator const instructionPtr{currentIt};
      uint32_t const instructionArity{getArithArity(currentIt->data.deferredAction.opcode)};
      std::array<Stack::iterator, 3U> args{{Stack::iterator(), Stack::iterator(), Stack::iterator()}};
      Stack::iterator param{instructionPtr.prev()};
      for (uint32_t i{0U}; i < instructionArity; i++) {
        uint32_t const index{instructionArity - i - 1U};
        args[index] = param;
        param = param->sibling;
      }

      bool const propagateTargetHint{compiler_.backend_.checkIfEnforcedTargetIsOnlyInArgs(
          Span<Stack::iterator>{args.data(), static_cast<size_t>(instructionArity)}, enforcedTarget)};
      StackElement const *const targetHint{propagateTargetHint ? enforcedTarget : nullptr};
      StackElement const result{evaluateInstruction(instructionPtr, args[0], args[1], args[2], targetHint)};
      replaceAndUpdateReference(instructionPtr, result);
    }
    if (currentIt != rootNode) {
      currentIt = currentIt.next();
    } else {
      break;
    }
  }
}

void Common::condenseScratchRegBelow(Stack::iterator const rootNode, StackElement const *const enforcedTarget) const {
  if (rootNode->type != StackType::DEFERREDACTION) {
    return;
  }
  Stack::iterator const valentBlockTop{findBaseOfValentBlock(rootNode)};

  Stack::iterator currentIt{valentBlockTop};

  while (currentIt != rootNode) {
    if (currentIt->type == StackType::DEFERREDACTION) {
      Stack::iterator const instructionPtr{currentIt};
      uint32_t const instructionArity{getArithArity(currentIt->data.deferredAction.opcode)};
      std::array<Stack::iterator, 3U> args{{Stack::iterator(), Stack::iterator(), Stack::iterator()}};
      Stack::iterator param{instructionPtr.prev()};
      for (uint32_t i{0U}; i < instructionArity; i++) {
        uint32_t const index{instructionArity - i - 1U};
        args[index] = param;
        param = param->sibling;
      }

      bool needCondense{false};
      if (instructionArity == 1U) {
        if (args[0]->getBaseType() == StackType::SCRATCHREGISTER) {
          needCondense = true;
        }
      } else {
        bool argsHasScratchReg{scratchRegOnlyOnceOnStack(args[0]) || scratchRegOnlyOnceOnStack(args[1])};
        bool argsAreRegOrConst{stackElementInRegOrConst(args[1]) && stackElementInRegOrConst(args[0])};
        if (instructionArity == 3U) {
          argsHasScratchReg = argsHasScratchReg || scratchRegOnlyOnceOnStack(args[2]);
          argsAreRegOrConst = argsAreRegOrConst && stackElementInRegOrConst(args[2]);
        }

        needCondense = argsHasScratchReg && argsAreRegOrConst;
      }

      if (needCondense) {
        bool const propagateTargetHint{compiler_.backend_.checkIfEnforcedTargetIsOnlyInArgs(
            Span<Stack::iterator>{args.data(), static_cast<size_t>(instructionArity)}, enforcedTarget)};
        StackElement const *const targetHint{propagateTargetHint ? enforcedTarget : nullptr};
        StackElement const result{evaluateInstruction(instructionPtr, args[0], args[1], args[2], targetHint)};
        replaceAndUpdateReference(instructionPtr, result);
      }
    }

    currentIt = currentIt.next();
  }
}
/// @details The condense is performed from stack bottom to top.
/// For better using the CPU pipeline, long instructions int-div and load are condensed first.
/// Then to reduce the register pressure of follow up code emit, instructions with side effects are condensed.
/// Finally, the remaining instructions are condensed.
Common::ConditionResult Common::condenseValentBlockCoreBelow(bool const comparison, Stack::iterator const belowIt,
                                                             StackElement const *const enforcedTarget) const {
  assert(!(comparison && enforcedTarget) && "No target allowed for comparison");
  Stack::iterator const lastBlock{compiler_.moduleInfo_.fnc.lastBlockReference};
  const bool unreachable{(lastBlock.isEmpty()) ? compiler_.moduleInfo_.fnc.unreachable : lastBlock->data.blockInfo.blockUnreachable};
  if (unreachable) {
    // Never condense on unreachable, since only dummy constant can push to stack after unreachable
    return {belowIt, BC::UNCONDITIONAL};
  }

  if (enforcedTarget != nullptr) {
    assert(!enforcedTarget->isStackMemory() && "TEMPSTACK not allowed as enforced target");
    compiler_.backend_.spillFromStack(*enforcedTarget, RegMask::none(), false, false, findBaseOfValentBlockBelow(belowIt), belowIt);
  }
  Stack::iterator const frameBase{getCurrentFrameBase()};
  static_cast<void>(frameBase);
  BC branchCond{BC::UNCONDITIONAL};

  Stack::iterator const vbBase{belowIt.prev()};
  // coverity[autosar_cpp14_a5_3_2_violation]
  condenseSideEffectInstructionBelow(vbBase, enforcedTarget);
  // coverity[autosar_cpp14_a5_3_2_violation]
  condenseScratchRegBelow(vbBase, enforcedTarget);

  Stack::iterator currentIt{findBaseOfValentBlock(vbBase)};
  Stack::iterator condenseResult{currentIt};

  while (true) {
    if (currentIt->type == StackType::DEFERREDACTION) {
      Stack::iterator const instructionPtr{currentIt};
      uint32_t const instructionArity{getArithArity(currentIt->data.deferredAction.opcode)};
      std::array<Stack::iterator, 3U> args{{Stack::iterator(), Stack::iterator(), Stack::iterator()}};
      Stack::iterator param{instructionPtr.prev()};
      for (uint32_t i{0U}; i < instructionArity; i++) {
        uint32_t const index{instructionArity - i - 1U};
        args[index] = param;
        param = param->sibling;
      }

      bool const isConditionStart{comparison && (instructionPtr == vbBase)};

      if ((isConditionStart && (instructionPtr->data.deferredAction.opcode >= OPCode::I32_EQZ)) &&
          (instructionPtr->data.deferredAction.opcode <= OPCode::F64_GE)) {
        branchCond = evaluateCondition(instructionPtr, args[0], args[1]);
        condenseResult = instructionPtr->parent;
        static_cast<void>(compiler_.stack_.erase(instructionPtr));
        break;
      } else {
        bool const propagateTargetHint{compiler_.backend_.checkIfEnforcedTargetIsOnlyInArgs(
            Span<Stack::iterator>{args.data(), static_cast<size_t>(instructionArity)}, enforcedTarget)};
        StackElement const *const targetHint{propagateTargetHint ? enforcedTarget : nullptr};
        StackElement const result{evaluateInstruction(instructionPtr, args[0], args[1], args[2], targetHint)};
        replaceAndUpdateReference(instructionPtr, result);
        condenseResult = instructionPtr;
      }
    }
    if (currentIt != vbBase) {
      currentIt = currentIt.next();
    } else {
      break;
    }
  }

  if (enforcedTarget != nullptr) {
    VariableStorage srcStorage{compiler_.moduleInfo_.getStorage(*condenseResult)};
    VariableStorage const dstStorage{compiler_.moduleInfo_.getStorage(*enforcedTarget)};
    if (srcStorage.machineType != dstStorage.machineType) {
      assert(MachineTypeUtil::isInt(srcStorage.machineType) && MachineTypeUtil::isInt(dstStorage.machineType));
      srcStorage.machineType = dstStorage.machineType;
    }
    compiler_.backend_.emitMoveImpl(dstStorage, srcStorage, false, false);

    replaceAndUpdateReference(condenseResult, *enforcedTarget);
  }
  return {condenseResult, branchCond};
}

void Common::dropValentBlock() const VB_NOEXCEPT {
  Stack::iterator const lastBlock{compiler_.moduleInfo_.fnc.lastBlockReference};
  const bool unreachable{(lastBlock.isEmpty()) ? compiler_.moduleInfo_.fnc.unreachable : lastBlock->data.blockInfo.blockUnreachable};
  if (unreachable) {
    Stack::iterator const dropTarget{compiler_.stack_.last()};
    if (!dropTarget.isEmpty()) {
      StackType const constantOrCondensedResult{dropTarget->getBaseType()};
      static_cast<void>(constantOrCondensedResult);
      assert((constantOrCondensedResult == StackType::CONSTANT) || (constantOrCondensedResult == StackType::TEMP_RESULT) ||
             (constantOrCondensedResult == StackType::SCRATCHREGISTER));
      removeReference(dropTarget);
      compiler_.stack_.pop();
    }
    return;
  }

  Stack::iterator const vbBase{compiler_.stack_.last()};
  Stack::iterator currentIt{findBaseOfValentBlock(vbBase)};
  while (true) {
    bool const dropEnd{currentIt == vbBase};
    removeReference(currentIt);
    static_cast<void>(compiler_.stack_.erase(currentIt));
    if (dropEnd) {
      break;
    }
    currentIt = currentIt.next();
  }
}

void Common::pushAndUpdateReference(StackElement const &element) const {
  Stack::iterator const returnElemPtr{pushOperandsToStack(element)};
  addReference(returnElemPtr);
}

void Common::popAndUpdateReference() const VB_NOEXCEPT {
  removeReference(compiler_.stack_.last());
  compiler_.stack_.pop();
}

void Common::replaceInCondenseTree(StackElement &originElement, StackElement const &newElement) VB_NOEXCEPT {
  Stack::iterator const parent{originElement.parent};
  Stack::iterator const sibling{originElement.sibling};
  originElement = newElement;
  originElement.parent = parent;
  originElement.sibling = sibling;
}

void Common::replaceAndUpdateReference(Stack::iterator const originalElement, StackElement const &newElement) const VB_NOEXCEPT {
  removeReference(originalElement);
  replaceInCondenseTree(*originalElement, newElement);
  addReference(originalElement);
}

void Common::addReference(Stack::iterator const element) const VB_NOEXCEPT {
  StackType const elementBaseType{element->getBaseType()};
  if (elementBaseType == StackType::CONSTANT) {
    return; // For reinterpretations, which can stay constant
  }
  assert((elementBaseType == StackType::LOCAL || elementBaseType == StackType::GLOBAL || elementBaseType == StackType::SCRATCHREGISTER ||
          (elementBaseType == StackType::TEMP_RESULT)) &&
         "Only variables can be referenced");

  Stack::iterator &majorReference{compiler_.moduleInfo_.getReferenceToLastOccurrenceOnStack(*element)};
  Stack::iterator *pTopGroupReference{&majorReference};

  bool const elementIsTempStackElement{element->isStackMemory()};
  if (elementIsTempStackElement) {
    if (!pTopGroupReference->isEmpty()) {
      // Top element of potential targeted group
      Stack::iterator targetedTopElement{*pTopGroupReference};

      if (element->data.variableData.location.calculationResult.resultLocation.stackFramePosition >
          targetedTopElement->data.variableData.location.calculationResult.resultLocation.stackFramePosition) {
        // Inserting a new group with highest stack offset, so link to previously highest stack offset group
        element->data.variableData.indexData.nextLowerTempStack = *pTopGroupReference;

        // Reset major index
        majorReference = Stack::iterator();
      } else {
        while (true) {
          if (element->data.variableData.location.calculationResult.resultLocation.stackFramePosition ==
              targetedTopElement->data.variableData.location.calculationResult.resultLocation.stackFramePosition) {
            // Found it, make it a new top element of the group
            element->data.variableData.indexData.nextLowerTempStack = targetedTopElement->data.variableData.indexData.nextLowerTempStack;
          } else {
            assert(element->data.variableData.location.calculationResult.resultLocation.stackFramePosition <
                       targetedTopElement->data.variableData.location.calculationResult.resultLocation.stackFramePosition &&
                   "Missed index group");

            // New stackFramePosition is lower than this group so look for the next group
            pTopGroupReference = &targetedTopElement->data.variableData.indexData.nextLowerTempStack;
            if (pTopGroupReference->isEmpty()) {
              // No group with a lower stackFramePosition, so create one (new group has no neighbor yet)
              element->data.variableData.indexData.nextLowerTempStack = Stack::iterator();
            } else {
              // Check group with next lower stackFramePosition
              Stack::iterator const pNextTargetedTopElement{*pTopGroupReference};
              if (element->data.variableData.location.calculationResult.resultLocation.stackFramePosition >
                  pNextTargetedTopElement->data.variableData.location.calculationResult.resultLocation.stackFramePosition) {
                // Between next targeted and the current group
                element->data.variableData.indexData.nextLowerTempStack = *pTopGroupReference;
                *pTopGroupReference = Stack::iterator();
              } else {
                // Check next lower group by changing the top element
                targetedTopElement = *pTopGroupReference;
                continue;
              }
            }
          }
          break;
        }
      }
    } else {
      // No neighbor yet
      element->data.variableData.indexData.nextLowerTempStack = Stack::iterator();
    }
  }

  element->data.variableData.indexData.nextOccurrence = Stack::iterator();
  element->data.variableData.indexData.prevOccurrence = *pTopGroupReference;
  Stack::iterator const pPreviousTopElement{*pTopGroupReference};
  if (!pPreviousTopElement.isEmpty()) {
    pPreviousTopElement->data.variableData.indexData.nextOccurrence = element;
  }
  *pTopGroupReference = element;

#if ENABLE_EXTENSIONS
  if (compiler_.getAnalytics() != nullptr) {
    if (elementIsTempStackElement) {
      // Count number of used TempStack slots
      uint32_t const numUsedTempStackSlots = getNumUsedTempStackSlots();
      Stack::iterator const highestTempStack = compiler_.moduleInfo_.getReferenceToLastOccurrenceOnStack(*element);
      uint32_t const activeTempStackBytes = highestTempStack->data.variableData.location.calculationResult.resultLocation.stackFramePosition -
                                            compiler_.moduleInfo_.getFixedStackFrameWidth();
      uint32_t const activeSlots = activeTempStackBytes / 8U;
      compiler_.getAnalytics()->updateMaxUsedTempStackSlots(numUsedTempStackSlots, activeSlots);
    }
  }
#endif
}

#if ENABLE_EXTENSIONS
uint32_t Common::getNumUsedTempStackSlots() const VB_NOEXCEPT {
  uint32_t usedTempStackSlots = 0U;
  Stack::iterator currentElem = compiler_.moduleInfo_.getReferenceToLastOccurrenceOnStackForStackMemory();
  while (true) {
    if (currentElem.isEmpty()) {
      break;
    }
    usedTempStackSlots++;
    currentElem = currentElem->data.variableData.indexData.nextLowerTempStack;
  }
  return usedTempStackSlots;
}
#endif

void Common::removeReference(Stack::iterator const element) const VB_NOEXCEPT {
  StackType const elementBaseType{element->getBaseType()};

  if (((elementBaseType == StackType::CONSTANT) || (elementBaseType == StackType::INVALID)) || (elementBaseType == StackType::DEFERREDACTION)) {
    return;
  }
  assert((elementBaseType == StackType::SCRATCHREGISTER || elementBaseType == StackType::LOCAL || elementBaseType == StackType::GLOBAL ||
          elementBaseType == StackType::TEMP_RESULT) &&
         "Only variables can be occurrence");

  Stack::iterator *pTopGroupReference{&compiler_.moduleInfo_.getReferenceToLastOccurrenceOnStack(*element)};
  assert(!pTopGroupReference->isEmpty() && "Reference is empty");

  bool const elementIsTempStackElement{element->isStackMemory()};
  if (elementIsTempStackElement) {
    while (true) {
      // Top element of potential targeted group
      Stack::iterator const targetedTopElement{*pTopGroupReference};
      assert(targetedTopElement != Stack::iterator());

      if (element->data.variableData.location.calculationResult.resultLocation.stackFramePosition ==
          targetedTopElement->data.variableData.location.calculationResult.resultLocation.stackFramePosition) {
        // Found the group we need to remove it from
        break;
      } else {
        assert(element->data.variableData.location.calculationResult.resultLocation.stackFramePosition <
                   targetedTopElement->data.variableData.location.calculationResult.resultLocation.stackFramePosition &&
               "Missed index group");
        // Check the next neighbor
        pTopGroupReference = &targetedTopElement->data.variableData.indexData.nextLowerTempStack;
        continue;
      }
    }
  }

  if ((element->data.variableData.indexData.prevOccurrence.isEmpty()) &&
      (element->data.variableData.indexData.nextOccurrence.isEmpty())) { // Removing the only entry in this group
    assert(element == *pTopGroupReference && "Not linked, but not the only one occurrence");
    if (elementIsTempStackElement) {
      *pTopGroupReference = element->data.variableData.indexData.nextLowerTempStack; // Forward the next neighbor
    } else {
      *pTopGroupReference = Stack::iterator(); // Empty reference
    }
  } else {
    Stack::iterator const pNextElement{element->data.variableData.indexData.nextOccurrence};
    if (!pNextElement.isEmpty()) {
      pNextElement->data.variableData.indexData.prevOccurrence = element->data.variableData.indexData.prevOccurrence;
    }
    Stack::iterator const pPrevElement{element->data.variableData.indexData.prevOccurrence};
    if (!pPrevElement.isEmpty()) {
      pPrevElement->data.variableData.indexData.nextOccurrence = element->data.variableData.indexData.nextOccurrence;
    }
    if (*pTopGroupReference == element) {
      // We are removing the top element from a group
      if (elementIsTempStackElement && (!pPrevElement.isEmpty())) {
        pPrevElement->data.variableData.indexData.nextLowerTempStack = element->data.variableData.indexData.nextLowerTempStack;
      }
      *pTopGroupReference = element->data.variableData.indexData.prevOccurrence;
    }
  }
}

Common::LiftedReg Common::liftToRegInPlaceProt(StackElement &element, bool const targetNeedsToBeWritable, StackElement const *const targetHint,
                                               RegAllocTracker &regAllocTracker) const {
  VariableStorage const originalStorage{compiler_.moduleInfo_.getStorage(element)};
  MachineType const type{originalStorage.machineType};

  TReg chosenReg{TReg::NONE};
  bool writable{false};

  // Return original element if it's already in a register
  if (originalStorage.type == StorageType::REGISTER) {
    regAllocTracker.futureLifts.unmask(compiler_.backend_.mask(originalStorage.location.reg, MachineTypeUtil::is64(type)));

    RegMask const readWriteProt{regAllocTracker.readWriteMask()};
    if (!readWriteProt.contains(originalStorage.location.reg)) {
      // Neither read nor write protected
      TReg const suitableTargetHintReg{compiler_.backend_.getUnderlyingRegIfSuitable(targetHint, type, readWriteProt)};
      // Only a scratch register may be overwritten (only if there is no copy on the stack except for the given)
      // or a variable that doesn't need to be written to
      bool const sourceIsAlreadyWritable{isWritableScratchReg(&element) || (suitableTargetHintReg == originalStorage.location.reg)};
      if ((!targetNeedsToBeWritable) || sourceIsAlreadyWritable) {
        chosenReg = originalStorage.location.reg;
        writable = sourceIsAlreadyWritable;
      }
    } else {
      if (!regAllocTracker.writeProtRegs.contains(originalStorage.location.reg)) {
        // Only read protected
        assert(regAllocTracker.readProtRegs.contains(originalStorage.location.reg));
        if (!targetNeedsToBeWritable) {
          chosenReg = originalStorage.location.reg;
          writable = false;
        }
      }
    }
  }

  if (chosenReg == TReg::NONE) {
    RegAllocTracker tempRegAllocTracker{regAllocTracker};
    RegElement const newRegElem{reqScratchRegProt(type, targetHint, tempRegAllocTracker, false)};
    VariableStorage srcStorage{compiler_.moduleInfo_.getStorage(element)};
    VariableStorage dstStorage{compiler_.moduleInfo_.getStorage(newRegElem.elem)};
    if (srcStorage.machineType != dstStorage.machineType) {
      srcStorage.machineType = type;
      dstStorage.machineType = type;
    }
    compiler_.backend_.emitMoveImpl(dstStorage, srcStorage, false, false);

    assert(isWritableScratchReg(&newRegElem.elem) || inSameReg(&newRegElem.elem, targetHint, false));

    Stack::iterator const prevOccurrence{compiler_.stack_.find(&element)};
    if (!prevOccurrence.isEmpty()) {
      Common::replaceAndUpdateReference(prevOccurrence, newRegElem.elem);
    } else {
      element = newRegElem.elem;
    }

    chosenReg = newRegElem.reg;
    // reqScratchRegProt always returns writable
    writable = true;
  }

  if (targetNeedsToBeWritable) {
    regAllocTracker.writeProtRegs.mask(compiler_.backend_.mask(chosenReg, MachineTypeUtil::is64(type)));
  } else {
    regAllocTracker.readProtRegs.mask(compiler_.backend_.mask(chosenReg, MachineTypeUtil::is64(type)));
  }
  return {chosenReg, writable};
}

RegElement Common::reqScratchRegProt(MachineType const type, StackElement const *const targetHint, RegAllocTracker &regAllocTracker,
                                     bool const presFlags) const {
  RegMask const allMask{regAllocTracker.readWriteFutureLiftMask()};

  if (MachineTypeUtil::isInt(type)) {
    assert((allMask.maskedRegsCount(static_cast<RegMask::Type>(0xFFFF'FFFF_U64)) < NBackend::WasmABI::resScratchRegsGPR) && "Too many regs masked");
  } else {
    assert((NBackend::WasmABI::resScratchRegsFPR == 0U ||
            (allMask.maskedRegsCount(static_cast<RegMask::Type>(0xFFFF'FFFF_U64 << 32_U64)) < NBackend::WasmABI::resScratchRegsFPR)) &&
           "Too many regs masked");
  }
  // TargetHint is only suitable if it's a register, correct MachineType and not protected
  TReg const suitableTargetHintReg{compiler_.backend_.getUnderlyingRegIfSuitable(targetHint, type, allMask)};

  RegElement res{};
  if (suitableTargetHintReg != TReg::NONE) {
    StackElement const resultElement{getResultStackElement(targetHint, type)};
    res = {resultElement, suitableTargetHintReg};
    assert(!allMask.contains(res.reg));
    regAllocTracker.writeProtRegs.mask(compiler_.backend_.mask(res.reg, MachineTypeUtil::is64(type)));
  } else {
    res = reqScratchRegProt(type, regAllocTracker, presFlags);
  }

  return res;
}

RegElement Common::reqScratchRegProt(MachineType const type, RegAllocTracker &regAllocTracker, bool const presFlags) const {
  RegMask const allMask{regAllocTracker.readWriteFutureLiftMask()};

  RegAllocCandidate const candidate{compiler_.backend_.getRegAllocCandidate(type, allMask)};
  StackElement const elem{StackElement::scratchReg(candidate.reg, MachineTypeUtil::toStackTypeFlag(type))};

  if (candidate.currentlyInUse) {
    // On stack, so we must spill it to stack before using it, note RegMask::all()
    compiler_.backend_.spillFromStack(elem, RegMask::all(), true, presFlags);
  }
  RegElement const res{elem, candidate.reg};
  assert(!allMask.contains(res.reg));
  regAllocTracker.writeProtRegs.mask(compiler_.backend_.mask(res.reg, MachineTypeUtil::is64(type)));
  return res;
}

TReg Common::reqFreeScratchRegProt(MachineType const type, RegAllocTracker &regAllocTracker) const VB_NOEXCEPT {
  RegMask const allMask{regAllocTracker.readWriteFutureLiftMask()};
  if (allMask.allMarked()) {
    return TReg::NONE;
  }
  RegAllocCandidate const candidate{compiler_.backend_.getRegAllocCandidate(type, allMask)};

  if (candidate.currentlyInUse) {
    return TReg::NONE;
  } else {
    regAllocTracker.writeProtRegs.mask(compiler_.backend_.mask(candidate.reg, MachineTypeUtil::is64(type)));
    return candidate.reg;
  }
}

RegMask Common::saveLocalsAndParamsForFuncCall(bool const onlySaveVolatileReg) const {
  RegMask inRegMask{RegMask::none()};
  for (uint32_t i{0U}; i < compiler_.moduleInfo_.fnc.numLocals; i++) {
    ModuleInfo::LocalDef &localDef{compiler_.moduleInfo_.localDefs[i]};
    if ((localDef.reg != TReg::NONE) &&
        ((localDef.currentStorageType == StorageType::REGISTER) || (localDef.currentStorageType == StorageType::STACK_REG))) {
      TReg const reg{localDef.reg};
      if (((!onlySaveVolatileReg) || NBackend::NativeABI::isVolatileReg(reg)) || (NBackend::NativeABI::canBeParam(reg) || isCallScrReg(reg))) {
        if (localDef.currentStorageType == StorageType::REGISTER) {
          compiler_.backend_.emitMoveImpl(VariableStorage::stackMemory(localDef.type, localDef.stackFramePosition),
                                          VariableStorage::reg(localDef.type, reg), false);
        }
        localDef.currentStorageType = StorageType::STACKMEMORY;
        inRegMask.mask(compiler_.backend_.mask(reg, MachineTypeUtil::is64(localDef.type)));
      }
    }
  }
  return inRegMask;
}

void Common::initializedLocal(uint32_t const localIdx) const {
  ModuleInfo::LocalDef &localDef{compiler_.moduleInfo_.localDefs[localIdx]};
  if (localDef.currentStorageType == StorageType::CONSTANT) {
    localDef.markLocalInitialized();
    if (localDef.reg != TReg::NONE) {
      compiler_.backend_.emitMoveImpl(VariableStorage::reg(localDef.type, localDef.reg), VariableStorage::zero(localDef.type), false, true);
      localDef.currentStorageType = StorageType::REGISTER;
    } else {
      compiler_.backend_.emitMoveImpl(VariableStorage::stackMemory(localDef.type, localDef.stackFramePosition), VariableStorage::zero(localDef.type),
                                      false, true);
      localDef.currentStorageType = StorageType::STACKMEMORY;
    }
  }
}

void Common::initializedAllLocal() const {
  for (uint32_t i{0U}; i < compiler_.moduleInfo_.fnc.numLocals; i++) {
    initializedLocal(i);
  }
}

void Common::recoverLocalToReg(uint32_t const localIdx, bool const isReachable) const {
  // Called via local.get
  ModuleInfo::LocalDef &localDef{compiler_.moduleInfo_.localDefs[localIdx]};
  if ((localDef.reg != TReg::NONE) && (localDef.currentStorageType == StorageType::STACKMEMORY)) {
    if (isReachable) {
      compiler_.backend_.emitMoveImpl(VariableStorage::reg(localDef.type, localDef.reg),
                                      VariableStorage::stackMemory(localDef.type, localDef.stackFramePosition), false);
    }
    localDef.currentStorageType = StorageType::STACK_REG;
  }
}

void Common::recoverAllLocalsToRegForBranch(uint32_t const localIdx, bool const isReachable) const {
  // Called during branching
  ModuleInfo::LocalDef &localDef{compiler_.moduleInfo_.localDefs[localIdx]};
  // Regardless of the initial location, convert it to STACK_REG, so it would have the same starting location at diverge point
  // STACK_REG can achieve an average 5.3% smaller score size than REGISTER
  // this is presumably due to less spill happening and the fact that few function calls are inlined when compiled
  if (localDef.reg != TReg::NONE) {
    if (isReachable) {
      if (localDef.currentStorageType == StorageType::STACKMEMORY) {
        compiler_.backend_.emitMoveImpl(VariableStorage::reg(localDef.type, localDef.reg),
                                        VariableStorage::stackMemory(localDef.type, localDef.stackFramePosition), false);
      } else if (localDef.currentStorageType == StorageType::REGISTER) {
        compiler_.backend_.emitMoveImpl(VariableStorage::stackMemory(localDef.type, localDef.stackFramePosition),
                                        VariableStorage::reg(localDef.type, localDef.reg), false);
      } else {
        assert(localDef.currentStorageType == StorageType::STACK_REG && "Unexpected storage type");
      }
    }
    localDef.currentStorageType = StorageType::STACK_REG;
  }
}

void Common::prepareLocalForSetValue(uint32_t const localIdx) const {
  ModuleInfo::LocalDef &localDef{compiler_.moduleInfo_.localDefs[localIdx]};
  if (localDef.currentStorageType == StorageType::CONSTANT) {
    localDef.markLocalInitialized();
    // there are no local usage before, no need to check recover
    return;
  }
  if (localDef.reg != TReg::NONE) {
    StackElement const targetElem{StackElement::local(localIdx)};
    Stack::iterator const lastOccurrence{compiler_.moduleInfo_.getReferenceToLastOccurrenceOnStack(targetElem)};
    if (lastOccurrence.isEmpty()) {
      // local is no used before, we can mark it storage in register and do not need to emit recover code
      localDef.currentStorageType = StorageType::REGISTER;
    } else {
      // There are local usage in stack, for performance reason, we recover local to register
      recoverLocalToReg(localIdx, true);
      localDef.currentStorageType = StorageType::REGISTER;
    }
  }
}

void Common::recoverAllLocalsToRegBranch(bool const isReachable) const {
  for (uint32_t i{0U}; i < compiler_.moduleInfo_.fnc.numLocals; i++) {
    recoverAllLocalsToRegForBranch(i, isReachable);
  }
}

void Common::recoverGlobalsToRegs() const {
  for (uint32_t i{0U}; i < compiler_.moduleInfo_.numNonImportedGlobals; i++) {
    ModuleInfo::GlobalDef const &globalDef{compiler_.moduleInfo_.globals[i]};
    if (globalDef.reg != TReg::NONE) {
      VariableStorage const memoryStorage{VariableStorage::linkData(globalDef.type, globalDef.linkDataOffset)};
      compiler_.backend_.emitMoveImpl(compiler_.moduleInfo_.getStorage(StackElement::global(i)), memoryStorage, false);
    }
  }
}
void Common::moveGlobalsToLinkData() const {
  for (uint32_t i{0U}; i < compiler_.moduleInfo_.numNonImportedGlobals; i++) {
    ModuleInfo::GlobalDef const &globalDef{compiler_.moduleInfo_.globals[i]};
    if (globalDef.reg != TReg::NONE) {
      VariableStorage const memoryStorage{VariableStorage::linkData(globalDef.type, globalDef.linkDataOffset)};
      compiler_.backend_.emitMoveImpl(memoryStorage, compiler_.moduleInfo_.getStorage(StackElement::global(i)), false);
    }
  }
}

void Common::emitGenericTrapHandler() {
  assert(compiler_.output_.size() == 0 && "Trap wrapper can only be positioned at the start of the binary");
  compiler_.backend_.emitNativeTrapAdapter();
  // generic trap handler should be placed after
  compiler_.moduleInfo_.helperFunctionBinaryPositions.genericTrapHandler = compiler_.output_.size();
  if (compiler_.isStacktraceEnabled()) {
    uint32_t const stacktraceRecordCount{compiler_.getStacktraceRecordCount()};
    compiler_.backend_.emitStackTraceCollector(stacktraceRecordCount);
  }
  compiler_.backend_.emitTrapHandler();
}

VariableStorage Common::getOptimizedSourceStorage(StackElement const &elem, RegMask const availableLocalsRegMask) const VB_NOEXCEPT {
  // If target is reg which contains local, we should move local to stack first
  if (elem.getBaseType() == StackType::LOCAL) {
    ModuleInfo::LocalDef const localDef{compiler_.moduleInfo_.localDefs[elem.data.variableData.location.localIdx]};
    TReg const sourceReg{localDef.reg};
    if ((sourceReg != TReg::NONE) && availableLocalsRegMask.contains(sourceReg)) {
      return VariableStorage::reg(localDef.type, sourceReg);
    }
  }
  return compiler_.moduleInfo_.getStorage(elem);
}

/// @brief merge control flow state
/// @param a control flow state a
/// @param b control flow state b
static inline ControlFlowState mergeControlFlowState(ControlFlowState const a, ControlFlowState const b) VB_NOEXCEPT {
  ControlFlowState newState{};

  // checkedStackFrameSize = std::min(checkedStackFrameSize of potential branch)
  newState.checkedStackFrameSize = (a.checkedStackFrameSize < b.checkedStackFrameSize) ? a.checkedStackFrameSize : b.checkedStackFrameSize;
  return newState;
}

void Common::emitBranchMergePoint(bool const isReachable, StackElement const *const finishedBlock) const {
  // we cannot store local states without dynamically memory allocation, so we recover them in each cases.
  initializedAllLocal();
  recoverAllLocalsToRegBranch(isReachable);
  if (finishedBlock != nullptr) {
    if (isReachable) {
      compiler_.moduleInfo_.currentState = mergeControlFlowState(compiler_.moduleInfo_.currentState, finishedBlock->data.blockInfo.endState);
    } else {
      compiler_.moduleInfo_.currentState = finishedBlock->data.blockInfo.endState;
    }
  }
}

/// @brief merge state
static inline void mergeStateAtBranchDivergePoint(bool const isReachable, Stack::iterator const targetBlock,
                                                  ControlFlowState const currentState) VB_NOEXCEPT {
  if ((isReachable && (!targetBlock.isEmpty())) && (targetBlock->type != StackType::LOOP)) {
    // in loop it will jump to begin instead of end.
    targetBlock->data.blockInfo.endState = mergeControlFlowState(targetBlock->data.blockInfo.endState, currentState);
  }
}

void Common::emitBranchDivergePoint(bool const isReachable, Stack::iterator const targetBlock) const {
  // we cannot store local states without dynamically memory allocation, so we recover them in each cases.
  initializedAllLocal();
  recoverAllLocalsToRegBranch(isReachable);
  mergeStateAtBranchDivergePoint(isReachable, targetBlock, compiler_.moduleInfo_.currentState);
}

void Common::emitBranchDivergePoint(bool const isReachable, uint32_t const targetBlockNum,
                                    FunctionRef<Stack::iterator()> const &targetBlockFunc) const {
  initializedAllLocal();
  recoverAllLocalsToRegBranch(isReachable);
  for (uint32_t i{0U}; i < targetBlockNum; i++) {
    Stack::iterator const targetBlock{targetBlockFunc()};
    mergeStateAtBranchDivergePoint(isReachable, targetBlock, compiler_.moduleInfo_.currentState);
  }
}

uint32_t Common::findFreeTempStackSlot(uint32_t const slotSize) const VB_NOEXCEPT {
  // coverity[autosar_cpp14_a7_1_2_violation] C++ 14 does not eval constexpr of union, need p1330r0 of C++20
  Stack::iterator currentElem{compiler_.moduleInfo_.getReferenceToLastOccurrenceOnStackForStackMemory()};
  while (true) {
    if (currentElem.isEmpty()) {
      break;
    }
    Stack::iterator const nextElement{currentElem->data.variableData.indexData.nextLowerTempStack};
    uint32_t nextUsedOffset;
    if (!nextElement.isEmpty()) {
      nextUsedOffset = nextElement->data.variableData.location.calculationResult.resultLocation.stackFramePosition;
    } else {
      nextUsedOffset = compiler_.moduleInfo_.getFixedStackFrameWidth();
    }

    uint32_t const delta{currentElem->data.variableData.location.calculationResult.resultLocation.stackFramePosition - nextUsedOffset};
    if (delta >= (StackElement::tempStackSlotSize + slotSize)) {
      uint32_t const freeSlotOffset{nextUsedOffset + slotSize};
      assert(freeSlotOffset > compiler_.moduleInfo_.getFixedStackFrameWidth());
      Stack::iterator const lastBlock{compiler_.moduleInfo_.fnc.lastBlockReference};
      if ((!lastBlock.isEmpty()) && (freeSlotOffset < lastBlock->data.blockInfo.entryStackFrameSize)) {
        break;
      }
      return freeSlotOffset;
    }

    currentElem = nextElement;
  }

  return getCurrentMaximumUsedStackFramePosition() + StackElement::tempStackSlotSize;
}

uint32_t Common::getStackReturnValueWidth(uint32_t const sigIndex, bool const isLoop) const VB_NOEXCEPT {
  NBackend::RegStackTracker tracker{};
  uint32_t returnValueWidth{0U};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const visitor = [this, &tracker, &returnValueWidth](MachineType const returnValueType) VB_NOEXCEPT {
    TReg const targetReg{compiler_.backend_.getREGForReturnValue(returnValueType, tracker)};
    if (targetReg == TReg::NONE) {
      returnValueWidth += 8U;
    }
  };
  if (isLoop) {
    // coverity[autosar_cpp14_a5_1_4_violation]
    compiler_.moduleInfo_.iterateParamsForSignature(sigIndex, FunctionRef<void(MachineType)>(visitor));
  } else {
    // coverity[autosar_cpp14_a5_1_4_violation]
    compiler_.moduleInfo_.iterateResultsForSignature(sigIndex, FunctionRef<void(MachineType)>(visitor));
  }
  return returnValueWidth;
}

void Common::emitIsFunctionLinkedCompileTimeOpt(Stack::iterator const fncTableIdxElementPtr) VB_NOEXCEPT {
  uint32_t const functionTableIndex{fncTableIdxElementPtr->data.constUnion.u32};

  bool linkStatus{false};
  if (functionTableIndex < compiler_.moduleInfo_.tableInitialSize) {
    uint32_t const fncIndex{compiler_.moduleInfo_.tableElements[functionTableIndex].fncIndex};
    if (fncIndex != 0xFFFFFFFFU) {
      linkStatus = compiler_.moduleInfo_.functionIsLinked(fncIndex);
    } else {
      linkStatus = false;
    }
  } else {
    linkStatus = false;
  }

  StackElement const returnElement{StackElement::i32Const(linkStatus ? 1_U32 : 0_U32)};
  replaceAndUpdateReference(fncTableIdxElementPtr, returnElement);
}

bool Common::inSameReg(StackElement const *const lhs, StackElement const *const rhs, bool const requestWasmTypeMatch) const VB_NOEXCEPT {
  if ((lhs == nullptr) || (rhs == nullptr)) {
    return false;
  }
  VariableStorage const lStorage{compiler_.moduleInfo_.getStorage(*lhs)};
  VariableStorage const rStorage{compiler_.moduleInfo_.getStorage(*rhs)};

  if (requestWasmTypeMatch && (lStorage.machineType != rStorage.machineType)) {
    return false;
  }

  return ((lStorage.type == StorageType::REGISTER) && (rStorage.type == StorageType::REGISTER)) && (lStorage.location.reg == rStorage.location.reg);
}

StackElement Common::getResultStackElement(StackElement const *const stackElement, MachineType const type) const VB_NOEXCEPT {
  vb::StackType const baseType{stackElement->getBaseType()};
  if (stackElement->getBaseType() == StackType::SCRATCHREGISTER) {
    return StackElement::scratchReg(stackElement->data.variableData.location.reg, MachineTypeUtil::toStackTypeFlag(type));
  } else if ((stackElement->type == StackType::LOCAL) || (stackElement->type == StackType::GLOBAL)) {
    VariableStorage const storage{compiler_.moduleInfo_.getStorage(*stackElement)};
    uint32_t const referencePosition{compiler_.moduleInfo_.getReferencePosition(*stackElement)};
    return StackElement::tempResult(type, storage, referencePosition);
  } else if (baseType == StackType::TEMP_RESULT) {
    StackElement res{*stackElement};
    res.type = MachineTypeUtil::toStackTypeFlag(type) | StackType::TEMP_RESULT;
    return res;
  } else {
    return *stackElement;
  }
}

void Common::condenseCurrentValentBlockIfSideEffect() {
  /// iterator over the stack from current frame base to the end
  /// If there is side effect instruction, we will condense the valent block, Otherwise do nothing
  Stack::iterator const end{compiler_.stack_.end()};
  for (Stack::iterator cursor{findBaseOfValentBlockBelow(end)}; cursor != end; ++cursor) {
    if ((cursor->type == StackType::DEFERREDACTION) && (cursor->data.deferredAction.sideEffect != 0U)) {
      static_cast<void>(condenseValentBlockBelow(compiler_.stack_.end()));
      break;
    }
  }
}

void Common::condenseSideEffectInstructionToFrameBase() {
  condenseSideEffectInstructionToFrameBase(compiler_.stack_.end());
}

void Common::condenseSideEffectInstructionToFrameBase(Stack::iterator const belowIt) {
  if (!hasPendingSideEffectInstructions_) {
    return; // No side effect, skip
  }

  Stack::iterator cursor{getCurrentFrameBase()};
  while (cursor != belowIt) {
    if ((cursor->type == StackType::DEFERREDACTION) && (cursor->data.deferredAction.sideEffect != 0U)) {
      cursor++;
      cursor = condenseValentBlockBelow(cursor);
      cursor++;
    } else {
      cursor++;
    }
  }

  // Here the hasPendingSideEffectInstructions_ can be set to false, because
  // 1. The sideEffect instruction blew the current frame base has already be condensed before enter current block
  // 2. There are maybe still side effect instructions skipped by condenseSideEffectInstructionBlewValentBlock. But they will be condensed soon in
  // follow up condense with target hint.
  hasPendingSideEffectInstructions_ = false;
}

void Common::condenseSideEffectInstructionBlewValentBlock(uint32_t const count) {
  Stack::iterator const it{skipValentBlock(count)};

  condenseSideEffectInstructionToFrameBase(it);
}

bool Common::currentFrameEmpty() const VB_NOEXCEPT {
  Stack::iterator const frameBase{getCurrentFrameBase()};
  return (frameBase == compiler_.stack_.end());
}

Stack::iterator Common::skipValentBlock(uint32_t const count) const VB_NOEXCEPT {
  Stack::iterator cursor{compiler_.stack_.end()};
  for (uint32_t i{0U}; i < count; i++) {
    cursor = findBaseOfValentBlockBelow(cursor);
  }
  return cursor;
}

Stack::iterator Common::getFirstOperand(Stack::iterator const instruction) VB_NOEXCEPT {
  // GCOVR_EXCL_START
  assert(instruction->type == StackType::DEFERREDACTION);
  // GCOVR_EXCL_STOP

  Stack::iterator paramIt{instruction.prev()};

  while (!paramIt->sibling.isEmpty()) {
    paramIt = paramIt->sibling;
  }

  return paramIt;
}

Stack::iterator Common::pushDeferredAction(StackElement const &deferredAction) {
  hasPendingSideEffectInstructions_ = (hasPendingSideEffectInstructions_ || (deferredAction.data.deferredAction.sideEffect != 0U));
  uint32_t const instructionArity{getArithArity(deferredAction.data.deferredAction.opcode)};
  Stack::iterator const paramStart{compiler_.stack_.last()};

  Stack::iterator const instructionIt{compiler_.stack_.push(deferredAction)};

  Stack::iterator currentIt{paramStart};
  // coverity[autosar_cpp14_a6_5_1_violation] fake positive
  for (uint32_t i{0U}; i < (instructionArity - 1U); i++) {
    currentIt->parent = instructionIt;
    currentIt = currentIt->sibling;
  }
  Stack::iterator const paramEnd{currentIt};
  paramEnd->parent = instructionIt;
  instructionIt->sibling = paramEnd->sibling;
  paramEnd->sibling = Stack::iterator();
  instructionIt->parent = compiler_.stack_.end();
  return instructionIt;
}

bool Common::stackElementInRegOrConst(Stack::iterator const it) const VB_NOEXCEPT {
  if (it->getBaseType() == StackType::DEFERREDACTION) {
    return false;
  } else if (it->getBaseType() == StackType::CONSTANT) {
    return true;
  } else {
    VariableStorage const storage{compiler_.moduleInfo_.getStorage(*it)};
    assert(((storage.type != StorageType::INVALID) && (storage.type != StorageType::STACK_REG)) &&
           "Invalid storage type for stack element in reg or const check");
    return storage.type == StorageType::REGISTER;
  }
}

Stack::iterator Common::pushOperandsToStack(StackElement const &arg) const {
  Stack::iterator const previous{compiler_.stack_.last()};
  Stack::iterator const argIt{compiler_.stack_.push(arg)};
  argIt->sibling = previous;
  return argIt;
}

Stack::iterator Common::prepareCallParamsAndSpillContext(uint32_t const sigIndex, bool const isIndirectCall) {
  uint32_t const numParams{compiler_.moduleInfo_.getNumParamsForSignature(sigIndex)};
  uint32_t const numVBsToResolve{isIndirectCall ? (numParams + 1U) : numParams};

  Stack::iterator paramsBase{};
  if (numVBsToResolve > 0U) {
    paramsBase = condenseMultipleValentBlocksBelow(compiler_.stack_.end(), numVBsToResolve);
  }

  // coverity[autosar_cpp14_a5_1_4_violation]
  compiler_.backend_.iterateScratchRegsAndGlobals(FunctionRef<void(StackElement const &)>([this, paramsBase](StackElement const &element) {
    compiler_.backend_.spillFromStack(element, RegMask::none(), true, false, paramsBase, Stack::iterator{});
  }));

  return paramsBase;
}
} // namespace vb
