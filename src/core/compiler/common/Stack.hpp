///
/// @file Stack.hpp
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
#ifndef STACK_HPP
#define STACK_HPP

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "StackElement.hpp"

#include "src/config.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/common/BumpAllocator.hpp"
#include "src/core/compiler/common/ListIterator.hpp"

namespace vb {

///
/// @brief Stack for the compiler where not-yet-emitted WebAssembly instructions and variables will be stored during
/// compilation
///
/// This is vaguely related to the operand stack of a WebAssembly module
///
class Stack final {
public:
  using node = List_node<StackElement>;                     ///< node
  using iterator = List_iterator<StackElement>;             ///< iterator
  using const_iterator = List_const_iterator<StackElement>; ///< const_iterator

  /// @brief SubChain of the stack
  class SubChain final {
  public:
    /// @brief Constructor
    /// @param begin Begin iterator of the subchain
    /// @param end End iterator of the subchain
    inline SubChain(iterator const begin, iterator const end) VB_NOEXCEPT : begin_(begin), end_(end), size_(1U) {
      iterator it{begin};
      while (it != end) {
        ++it;
        size_++;
      }
    }

    /// @brief Returns the size of the subchain
    inline uint32_t size() const VB_NOEXCEPT {
      return size_;
    }

    /// @brief Returns the begin iterator of the subchain
    inline iterator begin() const VB_NOEXCEPT {
      return begin_;
    }

    /// @brief Returns the end iterator of the subchain
    inline iterator end() const VB_NOEXCEPT {
      return end_;
    }

  private:
    iterator begin_; ///< Begin iterator of the subchain
    iterator end_;   ///< End iterator of the subchain
    uint32_t size_;  ///< Size of the subchain
  };

  ///
  /// @brief Default constructor
  ///
  Stack() VB_NOEXCEPT;

  ///
  /// @brief Constructor
  ///
  /// @param compilerMemoryAllocFnc AllocFnc for internal compiler memory
  /// @param compilerMemoryFreeFnc FreeFnc for internal compiler memory
  /// @param ctx User defined context
  ///
  explicit Stack(AllocFnc const compilerMemoryAllocFnc, FreeFnc const compilerMemoryFreeFnc, void *const ctx);

  /// @brief first iterator
  iterator begin() VB_NOEXCEPT {
    return iterator{sentinel_->next};
  }

  /// @brief last iterator
  iterator end() VB_NOEXCEPT {
    return iterator(sentinel_);
  }

  /// @brief first const_iterator
  const_iterator cbegin() const VB_NOEXCEPT {
    return const_iterator{sentinel_->next};
  }

  /// @brief last const_iterator
  const_iterator cend() const VB_NOEXCEPT {
    return const_iterator(sentinel_);
  }

  /// @brief is empty
  /// @return true when empty
  bool empty() const VB_NOEXCEPT {
    return cbegin() == cend();
  }

  ///
  /// @brief Pushes a StackElement onto the stack
  ///
  /// @param element StackElement to push onto the stack
  /// @return StackElement* Pointer to the stack element on the stack
  /// @throws std::range_error If not enough memory is available
  iterator push(StackElement const &element);

  ///
  /// @brief Pops a StackElement from the top of the stack and returns it
  void pop() VB_NOEXCEPT;

  ///
  /// @brief last element
  ///
  StackElement &back() VB_NOEXCEPT {
    assert(!empty() && "Must have element on stack after validation");
    iterator tmp{end()};
    --tmp;
    return *tmp;
  }

  ///
  /// @brief last iterator
  ///
  iterator last() VB_NOEXCEPT {
    return end().prev();
  }

  ///
  /// @brief erase iterator
  ///
  /// @param position Iterator to erase
  iterator erase(iterator const position) VB_NOEXCEPT;

  ///
  /// @brief insert given element before specified iterator
  /// @param position Specified iterator
  /// @param element Given StackElement
  /// @return An iterator that points to the inserted element.
  iterator insert(iterator const position, StackElement const &element);

  ///
  /// @brief Find the first occurrence of a StackElement in stack.
  ///
  /// @param ptr Pointer points to the StackElement
  /// @return An valid iterator if the element is on the stack, empty iterator otherwise
  iterator find(StackElement const *const ptr) VB_NOEXCEPT;

  /// @brief split a sub chain from [position.next, end)
  /// @param position Iterator to split the stack at
  SubChain split(iterator const position) VB_NOEXCEPT;

  /// @brief contact a sub chain at the end of the stack
  /// @param chain SubChain to contact at the end of the stack
  void contactAtEnd(SubChain const &chain) VB_NOEXCEPT;

  ///
  /// @brief Init the stack, allocate the sentinel node
  inline void init() {
    sentinel_ = vb::pCast<node *>(allocator_.step());
    sentinel_->value.type = StackType::INVALID;
    sentinel_->prev = sentinel_;
    sentinel_->next = sentinel_;
    size_ = 0U;
  }

  ///
  /// @brief reset the stack
  void reset() {
    allocator_.reset();
    init();
  }

private:
  FixedBumpAllocator<sizeof(node)> allocator_; ///< Underlying allocator manages instance/memory where the StackElements in the Stack are stored
  node *sentinel_;                             ///< The sentinel node hold the head and tail iterator
  uint32_t size_;                              ///< The number of elements on the stack
};

} // namespace vb

#endif // STACK_HPP
