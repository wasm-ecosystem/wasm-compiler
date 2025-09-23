///
/// @file ValidationStack.hpp
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
#ifndef VALIDATION_STACK_HPP
#define VALIDATION_STACK_HPP
#include <cassert>
#include <cstdint>

#include "src/core/compiler/common/ListIterator.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/OPCode.hpp"
#include "src/core/compiler/frontend/ValidateElement.hpp"

namespace vb {
/// @brief Init FUNC(frame) element as sentinel node
class ValidationStack final {
public:
  using node = List_node<ValidateElement>;         ///< node
  using iterator = List_iterator<ValidateElement>; ///< iterator

  ///
  /// @brief Constructor
  ///
  /// @param moduleInfo Reference of moduleInfo
  /// @param compilerMemoryAllocFnc AllocFnc for internal compiler memory
  /// @param compilerMemoryFreeFnc FreeFnc for internal compiler memory
  /// @param ctx User defined context
  /// @throws std::range_error if not enough memory is available(propagate from @a init)
  ///
  explicit ValidationStack(ModuleInfo &moduleInfo, AllocFnc const compilerMemoryAllocFnc, FreeFnc const compilerMemoryFreeFnc, void *const ctx);
  ///
  /// @brief last iterator
  ///
  inline iterator last() VB_NOEXCEPT {
    assert(sentinel_ != nullptr);
    return iterator(sentinel_->prev);
  }
  ///
  /// @brief insertBack given element after specified iterator
  /// @param position Specified iterator
  /// @param element Given ValidateElement
  /// @throws std::range_error if not enough memory is available
  void insertBack(iterator const position, ValidateElement const &element);
  ///
  /// @brief Init the stack, allocate the sentinel node with FUNC start
  /// @throws std::range_error if not enough memory is available(propagate from @a step)
  inline void init() {
    sentinel_ = vb::pCast<node *>(allocator_.step());
    sentinel_->value.validateType_ = ValidateType::FUNC;
    sentinel_->value.blockInfo.prevBlock = iterator{nullptr};
    sentinel_->value.blockInfo.formallyUnreachable = false;
    sentinel_->prev = sentinel_;
    sentinel_->next = sentinel_;
    size_ = 1U;
    currentBlock_ = begin();
  }
  ///
  /// @brief reset the stack
  ///
  /// @param currentFuncSigIndex Init function signature index
  /// @throws std::range_error if not enough memory is available(propagate from @a init)
  void reset(uint32_t const currentFuncSigIndex) {
    allocator_.reset();
    init();
    currentBlock_->blockInfo.sigIndex = currentFuncSigIndex;
  }
  ///
  /// @brief push number variable with machineType
  ///
  /// @param machineType element machineType
  /// @throws std::range_error if not enough memory is available(propagate from @a push)
  inline void pushNumberVariable(MachineType const machineType) {
    push(ValidateElement::variable(machineType));
  }
  ///
  /// @brief pop a number type element from stack
  /// @throws throw validation exception if last element is not number type && block is reachable
  void drop();
  /// @brief Set Unreachable flag after unconditional branch(br,br_table,return and Unreachable)
  void markCurrentBlockUnreachable() VB_NOEXCEPT;
  ///
  /// @brief Validate last stack element
  ///
  /// @param machineType element machineType
  /// @param needPop If true pop the last element which is number type
  /// @throws throw validation exception if validate failed
  void validateLastNumberType(MachineType const machineType, bool const needPop);
  ///
  /// @brief validate block signature
  ///
  /// @param sigIndex Index of the function type that current block is conforming to
  /// @throws throw validation exception if validate failed
  void validateAndPrepareBlock(uint32_t const sigIndex);
  /// @brief validate loop signature
  ///
  /// @param sigIndex Index of the function type that current loop is conforming to
  /// @throws throw validation exception if validate failed
  void validateAndPrepareLoop(uint32_t const sigIndex);
  /// @brief validate if signature
  ///
  /// @param sigIndex Index of the function type that current if-block is conforming to
  /// @throws throw validation exception if validate failed
  void validateAndPrepareIfBlock(uint32_t const sigIndex);
  /// @brief validate params for arithmetic opcode and push result
  ///
  /// @param opCode Wasm arithmetic opcode that need to validate
  /// @throws throw validation exception if validate failed
  void validateArithmeticElement(OPCode const opCode);
  /// @brief validate for opcode else
  /// @throws throw validation exception if validate failed
  void validateElse();
  /// @brief validate for opcode end
  /// @throws throw validation exception if validate failed
  void validateEnd();
  /// @brief validate for branch opcode
  ///
  /// @param branchOpcode Wasm branch opcode that need to validate
  /// @param branchDepth Branch depth of a branch instruction
  /// @throws throw validation exception if validate failed
  void validateBranch(OPCode const branchOpcode, uint32_t const branchDepth);
  /// @brief validate for return opcode
  /// @throws throw validation exception if validate failed
  void validateReturn();
  /// @brief validate for call
  /// @param sigIndex Index of the function signature/type
  /// @throws throw validation exception if validate failed
  void validateCall(uint32_t const sigIndex);
  /// @brief validate for opcode select
  /// @throws throw validation exception if validate failed
  void validateSelect();

private:
  /// @brief first iterator
  iterator begin() VB_NOEXCEPT {
    return iterator{sentinel_};
  }
  /// @brief is empty
  /// @return true when empty
  bool empty() const VB_NOEXCEPT {
    return size_ == 0U;
  }
  ///
  /// @brief erase iterator
  ///
  /// @param position Iterator to erase
  void erase(iterator const position) VB_NOEXCEPT;
  ///
  /// @brief insert given element before specified iterator
  /// @param position Specified iterator
  /// @param element Given ValidateElement
  /// @throws std::range_error if not enough memory is available(propagate from @a step)
  void insertFront(iterator const position, ValidateElement const &element);
  ///
  /// @brief Pops a ValidateElement from the top of the stack and returns it
  inline void unsafePop() VB_NOEXCEPT {
    static_cast<void>(erase(iterator(sentinel_->prev)));
  }
  ///
  /// @brief Pushes a ValidateElement onto the stack
  ///
  /// @param element ValidateElement to push onto the stack
  /// @throws std::range_error If not enough memory is available
  inline void push(ValidateElement const &element) {
    // insert before begin(last slot)
    insertFront(begin(), element);
  }
  ///
  /// @param sigIndex Index of the signature type
  /// @param needPop If true, pop the validated element from stack
  /// @throws throw validation exception if validate failed
  iterator validateResults(uint32_t const sigIndex, bool const needPop);
  ///
  /// @param sigIndex Index of the signature type
  /// @param needPop If true, pop the validated element from stack. Default not pop
  /// @return Return empty iterator if no params. Otherwise, return the first param
  /// @throws throw validation exception if validate failed
  iterator validateParams(uint32_t const sigIndex, bool const needPop = false);
  /// @brief Validate last stack element
  ///
  /// @param vType Last validateElement should matched type
  /// @param needPop If true, pop the validated element from stack.
  /// @throws throw validation exception if validate failed
  void validateLastValidationType(ValidateType const vType, bool const needPop);
  ///
  /// @brief Peer existence of the expected type on stack. Compensate a element if unreachable and type matched.
  ///
  /// @param currentIt Reference of current visit validation element
  /// @param expectedType Expected wasm type
  /// @throws std::range_error if not enough memory is available(propagate from @a insertBack)
  inline void makeupVariableOnFormallyUnreachable(iterator &currentIt, MachineType const expectedType) {
    if (currentBlock_->blockInfo.formallyUnreachable && !currentIt->isNumber()) {
      // Number type not enough. Make up missing variables
      insertBack(currentIt, ValidateElement::variable(expectedType));
      ++currentIt; // Point to the newly added variable
    }
  }
  ///
  /// @brief Find the targeted frame/block/function for a given branch depth
  ///
  /// @param branchDepth Branch depth of a branch instruction
  /// @throws throw validation exception if validate failed
  iterator findTargetBlock(uint32_t const branchDepth) const;

private:
  iterator currentBlock_;                      ///< Point the current processing frame block
  ModuleInfo &moduleInfo_;                     ///< Reference to the ModuleInfo
  FixedBumpAllocator<sizeof(node)> allocator_; ///< Underlying allocator manages instance/memory where the StackElements in the Stack are stored
  node *sentinel_;                             ///< The sentinel node hold the head and tail iterator
  uint32_t size_;                              ///< The number of elements on the stack
};
} // namespace vb

#endif // VALIDATION_STACK_HPP
