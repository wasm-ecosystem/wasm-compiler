///
/// @file Stack.cpp
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

#include "Stack.hpp"

#include "src/config.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/common/BumpAllocator.hpp"
#include "src/core/compiler/common/ListIterator.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {

// coverity[autosar_cpp14_a12_1_5_violation] initial sentinel_ with nullptr
Stack::Stack() VB_NOEXCEPT : sentinel_(nullptr), size_(0U) {
}
Stack::Stack(AllocFnc const compilerMemoryAllocFnc, FreeFnc const compilerMemoryFreeFnc, void *const ctx)
    : allocator_(FixedBumpAllocator<sizeof(node)>(compilerMemoryAllocFnc, compilerMemoryFreeFnc, ctx)), sentinel_(nullptr), size_(0U) {
  init();
}

Stack::iterator Stack::push(StackElement const &element) {
  return insert(end(), element);
}

void Stack::pop() VB_NOEXCEPT {
  assert(!empty() && "Must have element on stack after validation");
  static_cast<void>(erase(iterator(sentinel_->prev)));
}

Stack::iterator Stack::erase(iterator const position) VB_NOEXCEPT {
  position.current_->prev->next = position.current_->next;
  position.current_->next->prev = position.current_->prev;
  size_--;
  iterator const next{iterator(position.current_->next)};
  allocator_.freeElem(position.current_);
  return next;
}

Stack::iterator Stack::insert(iterator const position, StackElement const &element) {
  node *const newNode{vb::pCast<node *>(allocator_.step())};
  newNode->value = element;
  newNode->prev = position.current_->prev;
  newNode->next = position.current_;

  position.current_->prev->next = newNode;
  position.current_->prev = newNode;
  size_++;
  return iterator(newNode);
}

Stack::iterator Stack::find(StackElement const *const ptr) VB_NOEXCEPT {
  for (Stack::iterator it{begin()}; it != end(); ++it) {
    if (&(it.current_->value) == ptr) {
      return it;
    }
  }
  return Stack::iterator();
}

Stack::SubChain Stack::split(iterator const position) VB_NOEXCEPT {
  // GCOVR_EXCL_START
  assert(size_ > 0U);
  // GCOVR_EXCL_STOP
  SubChain const subChain{position.next(), last()};

  position.current_->next = sentinel_;
  subChain.begin().current_->prev = nullptr;
  sentinel_->prev = position.current_;

  size_ -= subChain.size();

  return subChain;
}

void Stack::contactAtEnd(SubChain const &chain) VB_NOEXCEPT {
  Stack::iterator const chainStart{chain.begin()};
  Stack::iterator const chainEnd{chain.end()};

  node *const originalLast{sentinel_->prev};
  originalLast->next = chainStart.current_;
  chainStart.current_->prev = originalLast;

  sentinel_->prev = chainEnd.current_;
  chainEnd.current_->next = sentinel_;

  size_ += chain.size();
}

} // namespace vb
