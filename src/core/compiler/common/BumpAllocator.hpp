///
/// @file BumpAllocator.hpp
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
#ifndef BUMP_ALLOCATOR_HPP
#define BUMP_ALLOCATOR_HPP

#include <cstdint>
#include <utility>

#include "src/config.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"

namespace vb {

///
/// @brief Type of an alloc-like function with user context
///
/// First argument will be the allocation size while the second argument is pointer points to the user-defined context
///
using AllocFnc = void *(*)(uint32_t, void *);

///
/// @brief Type of a free-like function with user context
///
/// First argument will be the pointer to be freed while the second argument is pointer points to the user-defined context
///
using FreeFnc = void (*)(void *, void *);

///
/// @brief A fixed size memory allocator using slab-based allocation.
///
/// Manages fixed-size elements(ElementSize) by allocating memory in pre-sized slabs.
/// All elements allocated by this allocator remain valid until the allocator's destruction,
/// when all memory is bulk-freed
///
template <uint32_t ElementSize, uint32_t SlabSize = 64> class FixedBumpAllocator final {
public:
  /// @brief constructor
  // coverity[autosar_cpp14_a12_1_5_violation]
  FixedBumpAllocator() VB_NOEXCEPT : current_(nullptr), head_(nullptr), allocPtr_(nullptr), freePtr_(nullptr), ctx_(nullptr) {
  }

  /// @brief constructor
  explicit FixedBumpAllocator(AllocFnc const compilerMemoryAllocFnc, FreeFnc const compilerMemoryFreeFnc, void *const ctx) VB_NOEXCEPT
      : current_(nullptr),
        head_(nullptr),
        allocPtr_(compilerMemoryAllocFnc),
        freePtr_(compilerMemoryFreeFnc),
        ctx_(ctx) {
  }

  FixedBumpAllocator(const FixedBumpAllocator &) = delete;
  FixedBumpAllocator &operator=(const FixedBumpAllocator &) & = delete;

  /// @brief move constructor
  FixedBumpAllocator(FixedBumpAllocator &&other) VB_NOEXCEPT : current_(other.current_),
                                                               head_(other.head_),
                                                               allocPtr_(other.allocPtr_),
                                                               freePtr_(other.freePtr_),
                                                               ctx_(other.ctx_) {
    other.allocPtr_ = nullptr;
    other.freePtr_ = nullptr;
    other.ctx_ = nullptr;
  }

  FixedBumpAllocator &operator=(FixedBumpAllocator &&other) & = delete;

  ~FixedBumpAllocator() VB_NOEXCEPT {
    freeSlab(current_);
    current_ = nullptr;
    head_ = nullptr;
  }

  /// @brief allocate a new element
  void *step() VB_THROW {
    if ((current_ == nullptr) || (head_ == nullptr)) {
      allocateSlab();
    }

    void *const result{head_};
    head_ = *pCast<void **>(result);
    return result;
  }

  /// @brief free an element, link to freeList
  void freeElem(void *const elem) VB_NOEXCEPT {
    *pCast<void **>(elem) = head_;
    head_ = elem;
  }

  ///
  /// @brief deallocate all but the current slab and reset the current pointer
  ///
  void reset() VB_NOEXCEPT {
    freeSlab(current_->next);
    current_->next = nullptr;
    head_ = nullptr;
    initSlab(current_->ptr);
  }

private:
  /// @brief slab
  struct Slab final {
    Slab *next;   ///< points to next slab
    uint8_t *ptr; ///< points to current allocated memory
  };

  Slab *current_; ///< current slab
  void *head_;    ///< free elem list

  AllocFnc allocPtr_; ///< AllocFnc for internal compiler memory
  FreeFnc freePtr_;   ///< FreeFnc for internal compiler memory
  void *ctx_;         ///< Pointer points to user-defined context

  /// @brief init freeList
  void initSlab(uint8_t *const memory) VB_THROW {
    assert(head_ == nullptr && "InitSlab should only be triggered when there is no free elems");
    assert(memory != nullptr);

    size_t cursor{0U};
    for (; cursor < ((SlabSize - 1U) * ElementSize); cursor += ElementSize) {
      void **const current{pCast<void **>(pAddI(memory, cursor))};
      void *const next{pAddI(memory, cursor + ElementSize)};
      *current = next;
    }
    // last elem of freeList
    void **const last{pCast<void **>(pAddI(memory, cursor))};
    *last = nullptr;

    head_ = memory;
  }

  /// @brief allocate a new slab
  void allocateSlab() VB_THROW {
    constexpr uint32_t slabByteSize{static_cast<uint32_t>(sizeof(Slab)) + (ElementSize * SlabSize)};
    void *const mem{allocPtr_(slabByteSize, ctx_)};
    if (mem == nullptr) {
      throw RuntimeError(ErrorCode::Could_not_extend_memory);
    }

    Slab *const newSlab{pCast<Slab *>(mem)};

    newSlab->next = current_;
    newSlab->ptr = pAddI(pCast<uint8_t *>(mem), sizeof(Slab));
    initSlab(newSlab->ptr);

    current_ = newSlab;
  }

  /// @brief free slab
  void freeSlab(Slab *slab) const VB_NOEXCEPT {
    while (slab != nullptr) {
      Slab *const next{slab->next};
      freePtr_(slab, ctx_);
      slab = next;
    }
  }
};

} // namespace vb

#endif // BUMP_ALLOCATOR_HPP
