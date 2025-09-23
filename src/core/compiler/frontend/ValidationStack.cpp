///
/// @file ValidationStack.cpp
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
#include <cassert>
#include <cstdint>

#include "src/core/compiler/frontend/ValidationStack.hpp"

#include "src/config.hpp"
#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/common/BumpAllocator.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/ListIterator.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/OPCode.hpp"
#include "src/core/compiler/frontend/ValidateElement.hpp"
namespace vb {
ValidationStack::ValidationStack(ModuleInfo &moduleInfo, AllocFnc const compilerMemoryAllocFnc, FreeFnc const compilerMemoryFreeFnc, void *const ctx)
    : currentBlock_(nullptr), moduleInfo_(moduleInfo),
      allocator_(FixedBumpAllocator<sizeof(node)>(compilerMemoryAllocFnc, compilerMemoryFreeFnc, ctx)), sentinel_(nullptr), size_(0U) {
  init();
}

void ValidationStack::erase(iterator const position) VB_NOEXCEPT {
  position.current_->prev->next = position.current_->next;
  position.current_->next->prev = position.current_->prev;
  size_--;
  allocator_.freeElem(position.current_);
}

void ValidationStack::insertFront(iterator const position, ValidateElement const &element) {
  node *const newNode{vb::pCast<node *>(allocator_.step())};
  newNode->value = element;
  newNode->prev = position.current_->prev;
  newNode->next = position.current_;

  position.current_->prev->next = newNode;
  position.current_->prev = newNode;
  size_++;
}
void ValidationStack::insertBack(iterator const position, ValidateElement const &element) {
  assert(!position.isEmpty());
  node *const newNode{vb::pCast<node *>(allocator_.step())};
  newNode->value = element;
  newNode->prev = position.current_;
  newNode->next = position.current_->next;
  position.current_->next = newNode;
  newNode->next->prev = newNode;
  size_++;
}

void ValidationStack::markCurrentBlockUnreachable() VB_NOEXCEPT {
  assert(!currentBlock_.isEmpty());
  currentBlock_->blockInfo.formallyUnreachable = true;
  while ((last()->validateType_ != ValidateType::ELSE_FENCE) && (last() != currentBlock_)) {
    assert(last()->isNumber());
    unsafePop();
  }
}
void ValidationStack::validateArithmeticElement(OPCode const opCode) {
  ArithArg const arithArg{getArithArgs(opCode)};
  if (arithArg.arg1Type != MachineType::INVALID) {
    validateLastNumberType(arithArg.arg1Type, true);
  }
  validateLastNumberType(arithArg.arg0Type, true);

  assert(WasmTypeUtil::validateWasmType(MachineTypeUtil::to(arithArg.resultType)));
  push(ValidateElement::variable(arithArg.resultType));
}

void ValidationStack::validateAndPrepareBlock(uint32_t const sigIndex) {
  iterator const beforeFirstParamsPos{validateParams(sigIndex)};
  insertBack(beforeFirstParamsPos, ValidateElement::block(currentBlock_, sigIndex));
  currentBlock_ = beforeFirstParamsPos.next();
}
void ValidationStack::validateAndPrepareLoop(uint32_t const sigIndex) {
  iterator const beforeFirstParamsPos{validateParams(sigIndex)};
  insertBack(beforeFirstParamsPos, ValidateElement::loop(currentBlock_, sigIndex));
  currentBlock_ = beforeFirstParamsPos.next();
}

void ValidationStack::validateAndPrepareIfBlock(uint32_t const sigIndex) {
  // condition should be i32
  validateLastNumberType(MachineType::I32, true);
  iterator const beforeFirstParamsPos{validateParams(sigIndex)};

  insertBack(beforeFirstParamsPos, ValidateElement::ifblock(currentBlock_, sigIndex));
  // Set if-block as current working block
  currentBlock_ = beforeFirstParamsPos.next();
  push(ValidateElement::elseFence());
  // Prepare params group. Element Layout: IF_BLOCK | params_group1(p1) | Else_fence | params_group2(p2)
  // if-true branch consumes p2.
  // 1. When meet END directly -> validate return vals -> pop retVals, IF_BLOCK and p1 -> push return vals back
  // 2. Meet ELSE -> validate and pop return vals -> if-false branch validate p1 before IF_BLOCK -> when meet END -> validate and pop return vals ->
  // pop IF_BLOCK and p1 -> push return vals back
  iterator const end{last()};
  // coverity[autosar_cpp14_a7_1_1_violation]
  iterator cursor{currentBlock_.next()}; // First param or elseFence(no params)
  while (!cursor.isEmpty() && (cursor.raw() != end.raw())) {
    ValidateElement const e{*cursor};
    push(e);
    cursor++;
  }
  assert(cursor->validateType_ == ValidateType::ELSE_FENCE);
}

void ValidationStack::validateElse() {
  // IF_BLOCK | params_group1(p1) | Else_fence | if-true branch results
  iterator const ifBlock{currentBlock_};
  if (ifBlock.isEmpty() || (ifBlock->validateType_ != ValidateType::IF)) {
    throw ValidationException(ErrorCode::Validation_failed);
  }
  // handle results of IF_TRUE branch
  uint32_t const ifBlockSigIndex{ifBlock->blockInfo.sigIndex};
  moduleInfo_.iterateResultsForSignature(ifBlockSigIndex, FunctionRef<void(MachineType)>([this](MachineType const machineType) {
                                           this->validateLastNumberType(machineType, true);
                                         }),
                                         true);
  // IF_BLOCK | params_group1(p1) | Else_fence
  validateLastValidationType(ValidateType::ELSE_FENCE, true);
  // IF_BLOCK | params_group1(p1)
  ifBlock->blockInfo.formallyUnreachable = false; // reset for else-branch
}

void ValidationStack::validateEnd() {
  assert(!currentBlock_.isEmpty());
  switch (currentBlock_->validateType_) {
  case ValidateType::FUNC: {
    ValidateType const functionType{validateResults(currentBlock_->blockInfo.sigIndex, true)->validateType_};
    if (functionType != ValidateType::FUNC) {
      // Has element not consumed
      throw ValidationException(ErrorCode::Validation_failed);
    }
    validateLastValidationType(ValidateType::FUNC, true);
    if (!empty()) {
      // element stack should be empty after parsing the function
      throw ValidationException(ErrorCode::Validation_failed);
    }
    break;
  }
  case ValidateType::BLOCK:
  case ValidateType::IF:
  case ValidateType::LOOP: {
    iterator beforeResultsPos{validateResults(currentBlock_->blockInfo.sigIndex, false)}; // keep result

    // Case1(if-else): IF_BLOCK(pointed) | results
    // Case2(if): IF_BLOCK | params_group1(p1) | Else_fence(pointed) | results
    if ((currentBlock_->validateType_ == ValidateType::IF) && (beforeResultsPos->validateType_ == ValidateType::ELSE_FENCE)) {
      while (!empty() && (beforeResultsPos != currentBlock_)) {
        iterator const needPop{beforeResultsPos};
        beforeResultsPos--;
        erase(needPop);
      }
      // Align to IF_BLOCK(pointed) | results
    }

    if (empty() || (beforeResultsPos != currentBlock_)) {
      throw ValidationException(ErrorCode::Validation_failed);
    }
    currentBlock_ = currentBlock_->blockInfo.prevBlock;
    erase(beforeResultsPos); // erase block element
    break;
  }
  default:
    throw ValidationException(ErrorCode::Validation_failed);
  }
}
void ValidationStack::validateReturn() {
  if (empty() || begin().isEmpty()) {
    throw ValidationException(ErrorCode::Validation_failed);
  }
  static_cast<void>(validateResults(begin()->blockInfo.sigIndex, true));
  markCurrentBlockUnreachable();
}
void ValidationStack::validateBranch(OPCode const branchOpcode, uint32_t const branchDepth) {
  assert((branchOpcode == OPCode::BR_IF || branchOpcode == OPCode::BR_TABLE || branchOpcode == OPCode::BR) && "should be branch opcode");
  if ((branchOpcode == OPCode::BR_IF) || (branchOpcode == OPCode::BR_TABLE)) {
    validateLastNumberType(MachineType::I32, true);
  }
  iterator const targetBlock{findTargetBlock(branchDepth)};
  if (targetBlock.isEmpty()) {
    throw ValidationException(ErrorCode::Validation_failed);
  }
  // Only br_if is conditional here
  bool const unconditional{branchOpcode != OPCode::BR_IF};
  uint32_t const sigIndex{targetBlock->blockInfo.sigIndex};
  if (targetBlock->validateType_ == ValidateType::LOOP) {
    static_cast<void>(validateParams(sigIndex, unconditional));
  } else {
    static_cast<void>(validateResults(sigIndex, unconditional));
  }

  if (unconditional) {
    markCurrentBlockUnreachable();
  }
}
void ValidationStack::validateCall(uint32_t const sigIndex) {
  static_cast<void>(validateParams(sigIndex, true));
  // push call results
  moduleInfo_.iterateResultsForSignature(sigIndex, FunctionRef<void(MachineType)>([this](MachineType const machineType) {
                                           push(ValidateElement::variable(machineType));
                                         }),
                                         false);
}
void ValidationStack::validateSelect() {
  validateLastNumberType(MachineType::I32, true);
  if (currentBlock_->blockInfo.formallyUnreachable) {
    if (!last()->isNumber()) {
      // No number type can consume
      push(ValidateElement::variable(ValidateType::ANY)); // push dummy result of select
      return;
    }
    // If last type is number type. Prev element must exist(FUNC is the first element)
    assert(!last().prev().isEmpty());
    if (!last().prev()->isNumber()) {
      // Only one number type can consume, keep as result
      return;
    }
    // Two number type can consume, must be the same type
    ValidateType const arg1Type{last()->validateType_};
    unsafePop(); // keep another one as result
    ValidateType const arg0Type{last()->validateType_};
    assert(ValidateElement::isNumber(arg0Type) && ValidateElement::isNumber(arg1Type));

    if ((arg0Type != ValidateType::ANY) && (arg1Type != ValidateType::ANY)) {
      if (arg0Type != arg1Type) {
        throw ValidationException(ErrorCode::Validation_failed);
      }
      return; // keep arg0Type as result
    }
    // If one of them is ANY, keep the other one as result(May still ANY)
    if (arg0Type == ValidateType::ANY) {
      last()->validateType_ = arg1Type;
    }
  } else {
    if (!last()->isNumber()) {
      throw ValidationException(ErrorCode::Validation_failed);
    }
    ValidateType const arg1Type{last()->validateType_};
    unsafePop();
    ValidateType const arg0Type{last()->validateType_};
    if (!last()->isNumber() || (arg1Type != arg0Type)) {
      throw ValidationException(ErrorCode::Validation_failed);
    }
    // keep another one as result
  }
}

ValidationStack::iterator ValidationStack::validateResults(uint32_t const sigIndex, bool const needPop) {
  iterator beforeFirstResultPos{last()};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const validateVisitor = [this, needPop, &beforeFirstResultPos](MachineType const machineType) {
    makeupVariableOnFormallyUnreachable(beforeFirstResultPos, machineType);
    if (!beforeFirstResultPos->numberMatch(machineType)) {
      throw ValidationException(ErrorCode::Validation_failed);
    }
    --beforeFirstResultPos;
    if (needPop) {
      erase(beforeFirstResultPos.next());
    }
  };
  // coverity[autosar_cpp14_a5_1_4_violation]
  moduleInfo_.iterateResultsForSignature(sigIndex, FunctionRef<void(MachineType)>(validateVisitor), true);
  return beforeFirstResultPos;
}

ValidationStack::iterator ValidationStack::validateParams(uint32_t const sigIndex, bool const needPop) {
  iterator beforeFirstParamsPos{last()};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const validateParamsVisitor = [this, &beforeFirstParamsPos, needPop](MachineType const machineType) {
    assert(!beforeFirstParamsPos.isEmpty());
    makeupVariableOnFormallyUnreachable(beforeFirstParamsPos, machineType);
    if (!beforeFirstParamsPos->numberMatch(machineType)) {
      throw ValidationException(ErrorCode::Validation_failed);
    }
    --beforeFirstParamsPos;
    if (needPop) {
      erase(beforeFirstParamsPos.next());
    }
  };
  // coverity[autosar_cpp14_a5_1_4_violation]
  moduleInfo_.iterateParamsForSignature(sigIndex, FunctionRef<void(MachineType)>(validateParamsVisitor), true);

  return beforeFirstParamsPos;
}

ValidationStack::iterator ValidationStack::findTargetBlock(uint32_t const branchDepth) const {
  iterator targetBlockElem{currentBlock_};
  for (uint32_t i{0U}; i < branchDepth; i++) {
    if (targetBlockElem.isEmpty()) {
      throw ValidationException(ErrorCode::Validation_failed);
    }
    targetBlockElem = targetBlockElem->blockInfo.prevBlock;
  }
  return targetBlockElem;
}
void ValidationStack::drop() {
  assert(!empty());
  if (!last()->isNumber()) {
    if (!currentBlock_->blockInfo.formallyUnreachable) {
      throw ValidationException(ErrorCode::validateAndDrop__Stack_frame_underflow);
    }
    // No number variable can consume && block is unreachable -> Make up missing variables
    return;
  }
  unsafePop();
}
void ValidationStack::validateLastNumberType(MachineType const machineType, bool const needPop) {
  assert(!empty());
  if (currentBlock_->blockInfo.formallyUnreachable) {
    if (!last()->isNumber()) {
      if (!needPop) {
        pushNumberVariable(machineType);
      }
      return;
    }
    if (!last()->numberMatch(machineType)) {
      throw ValidationException(ErrorCode::Validation_failed);
    }
  } else {
    if (!last()->numberMatch(machineType)) {
      throw ValidationException(ErrorCode::Validation_failed);
    }
  }
  if (needPop) {
    unsafePop();
  }
}
void ValidationStack::validateLastValidationType(ValidateType const vType, bool const needPop) {
  if (!last()->equals(vType)) {
    throw ValidationException(ErrorCode::Validation_failed);
  }
  if (needPop) {
    unsafePop();
  }
}
} // namespace vb
