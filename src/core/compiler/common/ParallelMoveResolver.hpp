///
/// @file ParallelMoveResolver.hpp
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
#ifndef PARALLEL_MOVE_RESOLVER_HPP
#define PARALLEL_MOVE_RESOLVER_HPP
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/config.hpp"
#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/implementationlimits.hpp"
#include "src/core/compiler/common/BumpAllocator.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"

namespace vb {

using ParallelMoveEmitter =
    FunctionRef<void(VariableStorage const &, VariableStorage const &)>; ///< Function type to emit a move operation from source to target.
using ParallelMoveTempProvider = FunctionRef<VariableStorage(VariableStorage const &)>; ///< Function type to allocate a temp location for a cycle
                                                                                        ///< header source. Only called once per cycle

/// @brief ParallelMoveResolver is used to do register<->stackMemory move orders to avoid conflicts/overwritten
class ParallelMoveResolver final {
  /// @brief Stores one pending target-source move pair.
  // coverity[autosar_cpp14_a11_0_1_violation]
  struct ParallelMoveRecord final {
    VariableStorage target{}; ///< Target location that will receive the move.
    VariableStorage source{}; ///< Source location that still needs to be moved.
  };

  /// @brief One stack-source usage count keyed by its 8-byte-aligned stack offset.
  // coverity[autosar_cpp14_a11_0_1_violation]
  struct MemorySourceEntry final {
    uint32_t offset; ///< 8-byte-aligned stack offset that is still used as a source.
    uint32_t count;  ///< Number of unresolved moves that still read from this offset.
  };

public:
  /// @brief Construct a resolver with fixed-capacity runtime buffers.
  /// @param compilerMemoryAllocFnc Allocator used for resolver-owned buffers
  /// @param compilerMemoryFreeFnc Deallocator used for resolver-owned buffers
  /// @param ctx User-defined context forwarded to allocator and deallocator
  /// @param recordsCapacity Maximum number of parallel moves the resolver can hold
  /// @throws RuntimeError if backing allocation fails
  explicit ParallelMoveResolver(AllocFnc const compilerMemoryAllocFnc, FreeFnc const compilerMemoryFreeFnc, void *const ctx,
                                uint32_t const recordsCapacity) VB_THROW : freeFnc_(compilerMemoryFreeFnc),
                                                                           ctx_(ctx),
                                                                           recordsCapacity_(recordsCapacity),
                                                                           recordsCount_(0U),
                                                                           memorySourceCount_(0U),
                                                                           records_(nullptr),
                                                                           memorySourceMap_(nullptr),
                                                                           regSourceMap_() {
    assert(recordsCapacity_ <= ImplementationLimits::numParams && "ParallelMoveResolver storage overflow");

    records_ = pCast<ParallelMoveRecord *>(compilerMemoryAllocFnc(static_cast<uint32_t>(sizeof(ParallelMoveRecord)) * recordsCapacity_, ctx_));
    if (records_ == nullptr) {
      throw RuntimeError(ErrorCode::Could_not_extend_memory);
    }
    memorySourceMap_ = pCast<MemorySourceEntry *>(compilerMemoryAllocFnc(static_cast<uint32_t>(sizeof(MemorySourceEntry)) * recordsCapacity_, ctx_));
    if (memorySourceMap_ == nullptr) {
      freeFnc_(records_, ctx_);
      throw RuntimeError(ErrorCode::Could_not_extend_memory);
    }

    for (uint32_t recordsIndex{0U}; recordsIndex < recordsCapacity_; recordsIndex++) {
      static_cast<void>(::new (&records_[recordsIndex]) ParallelMoveRecord());
      static_cast<void>(::new (&memorySourceMap_[recordsIndex]) MemorySourceEntry());
    }
  }

  ParallelMoveResolver(ParallelMoveResolver const &) = delete;
  ParallelMoveResolver &operator=(ParallelMoveResolver const &) & = delete;
  ParallelMoveResolver(ParallelMoveResolver &&) = delete;
  ParallelMoveResolver &operator=(ParallelMoveResolver &&) & = delete;

  ~ParallelMoveResolver() VB_NOEXCEPT {
    for (uint32_t recordsIndex{0U}; recordsIndex < recordsCapacity_; recordsIndex++) {
      // coverity[autosar_cpp14_a5_2_2_violation]
      static_cast<void>(memorySourceMap_[recordsIndex].~MemorySourceEntry());
      // coverity[autosar_cpp14_a5_2_2_violation]
      static_cast<void>(records_[recordsIndex].~ParallelMoveRecord());
    }
    freeFnc_(records_, ctx_);
    freeFnc_(memorySourceMap_, ctx_);
  }

  /// @brief Resolve the pending parallel moves.
  /// @param moveEmitter Function to emit a move operation from source to target
  /// @param tempProvider Function to provide a temp location for the source of the current cycle header
  /// @throws Any exception thrown by the moveEmitter or tempProvider
  void resolve(ParallelMoveEmitter const &moveEmitter, ParallelMoveTempProvider const &tempProvider) VB_THROW {
    if (recordsCount_ == 0U) {
      return;
    }

    initSourceUsage();
    while (true) {
      moveAllWithoutConflict(moveEmitter);

      if (recordsCount_ == 0U) {
        return;
      }

      assert(tempProvider.notNull() && "ParallelMoveTempProvider must not be null when cycles remain");
      resolveCycle(moveEmitter, tempProvider);
    }
  }

  /// @brief Push a new copy operation onto the resolver.
  /// @param target Target register or memory to copy to
  /// @param source Source register or memory to copy from
  inline void push(VariableStorage const &target, VariableStorage const &source) VB_NOEXCEPT {
    assert((target.type != StorageType::LINKDATA && source.type != StorageType::LINKDATA) && "Not support linkData yet");
    if (target.inSameLocation(source)) {
      return;
    }

    assert((target.type != StorageType::STACKMEMORY) || ((target.location.stackFramePosition % 8U) == 0U));
    assert((source.type != StorageType::STACKMEMORY) || ((source.location.stackFramePosition % 8U) == 0U));

    // GCOVR_EXCL_START
    assert(recordsCount_ < recordsCapacity_ && "ParallelMoveResolver overflow");
    if (recordsCount_ >= recordsCapacity_) {
      UNREACHABLE(return, "ParallelMoveResolver push overflow");
    }
    // GCOVR_EXCL_STOP
    records_[recordsCount_] = ParallelMoveRecord{target, source};
    recordsCount_++;
  }

  /// @brief Check if a temp location is safe for all still pending moves.
  /// @param tempReg Candidate temp reg
  /// @return True if the temp conflict with pending source
  bool regUsedAsSource(TReg const tempReg) const VB_NOEXCEPT {
    for (size_t i{0U}; i < recordsCount_; i++) {
      ParallelMoveRecord const &record{records_[i]};
      if (record.source.location.reg == tempReg) {
        return true;
      }
    }

    return false;
  }

private:
  /// @brief Init the dependency map for all pending records.
  inline void initSourceUsage() VB_NOEXCEPT {
    // The source maps track which locations still need to be read by unresolved moves.
    for (size_t i{0U}; i < recordsCount_; i++) {
      markSourceAsUsed(records_[i].source);
    }
  }

  /// @brief Move all records without conflict.
  /// @param moveEmitter Function to emit a move operation from source to target
  inline void moveAllWithoutConflict(ParallelMoveEmitter const &moveEmitter) VB_THROW {
    while (true) {
      bool const didMove{tryMove(moveEmitter)};
      if (!didMove) {
        break;
      }
    }
  }

  /// @brief Iterate over all records and try to move them if there is no conflict.
  /// @param moveEmitter Function to emit a move operation from source to target
  bool tryMove(ParallelMoveEmitter const &moveEmitter) VB_THROW {
    size_t const count{recordsCount_};
    for (size_t i{0U}; i < count; i++) {
      ParallelMoveRecord const &record{records_[i]};
      // A move is safe once no other unresolved record still needs the target's current value.
      if (!targetIsUsedAsSourceByOtherRecords(record)) {
        VariableStorage const resolvedSource{record.source};
        moveEmitter(record.target, record.source);
        unmarkSourceAsUsed(resolvedSource);
        eraseRecord(i);
        return true; // must return, because the counts has been modified
      }
    }
    return false;
  }

  /// @brief Resolve one remaining cycle through an explicit temp location.
  /// @param moveEmitter Function to emit a move operation from source to target
  /// @param tempProvider Function to provide the temp location for the cycle header source
  void resolveCycle(ParallelMoveEmitter const &moveEmitter, ParallelMoveTempProvider const &tempProvider) VB_THROW {
    ParallelMoveRecord const cycleHeadCopy{records_[0U]};
    VariableStorage const tempStorage{tempProvider(cycleHeadCopy.source)};

    // Preserve the cycle head source first so its location becomes the initial writable hole.
    assert(tempStorage.type == StorageType::REGISTER && "Must get register for now");
    moveEmitter(tempStorage, cycleHeadCopy.source);
    unmarkSourceAsUsed(cycleHeadCopy.source);
    eraseRecord(0U); // erase the cycle header record

    size_t nextIndex{findRecordTargetingFreedLocation(cycleHeadCopy.source)};
    assert(nextIndex != notFound && "Remaining moves must contain a cycle");

    while (nextIndex != notFound) {
      ParallelMoveRecord const record{records_[nextIndex]};
      // Each step writes into the location freed by the previous step and creates the next hole at record.source.
      moveEmitter(record.target, record.source);
      unmarkSourceAsUsed(record.source);
      eraseRecord(nextIndex);
      nextIndex = findRecordTargetingFreedLocation(record.source);
    }

    moveEmitter(cycleHeadCopy.target, tempStorage);
  }

  /// @brief Find the remaining record whose target writes into the location freed by the previous cycle step.
  /// @param freedLocation Location whose old value has already been preserved and can now be overwritten
  /// @return The index of the matching record, or notFound if there is no pending match
  size_t findRecordTargetingFreedLocation(VariableStorage const &freedLocation) const VB_NOEXCEPT {
    for (size_t i{0U}; i < recordsCount_; i++) {
      ParallelMoveRecord const &record{records_[i]};
      if (record.target.overlapsWith(freedLocation)) {
        // Stack align to 8 bytes, so partial overlap should not happen
        return i;
      }
    }
    return notFound;
  }

  /// @brief Erase one resolved record from the active move list by shifting the tail down.
  /// @param index Index of the record to erase
  inline void eraseRecord(size_t const index) VB_NOEXCEPT {
    // GCOVR_EXCL_START
    assert(index < recordsCount_);
    // GCOVR_EXCL_STOP
    records_[index] = records_[(recordsCount_ - 1U)]; // Move the last record into the erased slot
    recordsCount_--;
  }

  /// @brief Record one pending source in the fast usage maps.
  /// @param sourceStorage Source location to track
  void markSourceAsUsed(VariableStorage const &sourceStorage) VB_NOEXCEPT {
    if (sourceStorage.type == StorageType::CONSTANT) {
      return;
    }
    if (sourceStorage.type == StorageType::REGISTER) {
      size_t const regIndex{static_cast<size_t>(sourceStorage.location.reg)};
      // GCOVR_EXCL_START
      assert(regIndex < static_cast<size_t>(TReg::NUMREGS));
      // GCOVR_EXCL_STOP
      regSourceMap_[regIndex] += 1U;
      return;
    }

    assert(sourceStorage.type == StorageType::STACKMEMORY);
    incrementMemorySource(sourceStorage.location.stackFramePosition);
  }

  /// @brief Remove one resolved source from the usage maps.
  /// @param sourceStorage Source location that is no longer pending
  void unmarkSourceAsUsed(VariableStorage const &sourceStorage) VB_NOEXCEPT {
    if (sourceStorage.type == StorageType::CONSTANT) {
      return;
    }
    if (sourceStorage.type == StorageType::REGISTER) {
      size_t const regIndex{static_cast<size_t>(sourceStorage.location.reg)};
      // GCOVR_EXCL_START
      assert(regIndex < static_cast<size_t>(TReg::NUMREGS));
      assert(regSourceMap_[regIndex] > 0U);
      // GCOVR_EXCL_STOP
      regSourceMap_[regIndex] -= 1U;
      return;
    }

    assert(sourceStorage.type == StorageType::STACKMEMORY);
    decrementMemorySource(sourceStorage.location.stackFramePosition);
  }

  /// @brief Check whether the target of a record is still used as a source by another pending record.
  /// @param record Record to inspect
  /// @return True if another pending source overlaps the record target
  bool targetIsUsedAsSourceByOtherRecords(ParallelMoveRecord const &record) const VB_NOEXCEPT {
    VariableStorage const &targetStorage{record.target};
    if (targetStorage.type == StorageType::REGISTER) {
      size_t const regIndex{static_cast<size_t>(targetStorage.location.reg)};
      // GCOVR_EXCL_START
      assert(regIndex < static_cast<size_t>(TReg::NUMREGS));
      // GCOVR_EXCL_STOP
      return regSourceMap_[regIndex] > 0U;
    }

    assert(targetStorage.type == StorageType::STACKMEMORY);
    return findMemorySource(targetStorage.location.stackFramePosition) != notFound;
  }

  /// @brief Find a stack-source usage entry by its offset.
  /// @param offset 8-byte-aligned stack offset to look up
  /// @return The index of the matching entry, or notFound if there is none
  size_t findMemorySource(uint32_t const offset) const VB_NOEXCEPT {
    for (size_t i{0U}; i < memorySourceCount_; i++) {
      if (memorySourceMap_[i].offset == offset) {
        return i;
      }
    }
    return notFound;
  }

  /// @brief Increment the usage count for a stack-source offset, inserting it if new.
  /// @param offset 8-byte-aligned stack offset
  void incrementMemorySource(uint32_t const offset) VB_NOEXCEPT {
    size_t const index{findMemorySource(offset)};
    if (index != notFound) {
      memorySourceMap_[index].count += 1U;
      return;
    }

    // GCOVR_EXCL_START
    assert(memorySourceCount_ < recordsCapacity_);
    // GCOVR_EXCL_STOP
    if (memorySourceCount_ < recordsCapacity_) {
      memorySourceMap_[memorySourceCount_] = MemorySourceEntry{offset, 1U};
      memorySourceCount_++;
    }
  }

  /// @brief Decrement the usage count for a stack-source offset, removing it when it reaches zero.
  /// @param offset 8-byte-aligned stack offset
  void decrementMemorySource(uint32_t const offset) VB_NOEXCEPT {
    size_t const index{findMemorySource(offset)};
    if (index != notFound) {
      assert(memorySourceMap_[index].count > 0U);
      memorySourceMap_[index].count -= 1U;
      if (memorySourceMap_[index].count == 0U) {
        // Remove the entry by shifting the tail down
        memorySourceMap_[index] = memorySourceMap_[(memorySourceCount_ - 1U)];
        memorySourceCount_--;
      }
    }
  }

  FreeFnc freeFnc_; ///< Deallocator for resolver-owned buffers.
  void *ctx_;       ///< User-defined allocator context.

  static constexpr size_t notFound{SIZE_MAX}; ///< Alias when a record is not found
  uint32_t recordsCapacity_;                  ///< Maximum number of active records supported by @ref records_ and @ref memorySourceMap_.
  size_t recordsCount_;                       ///< Number of active records in @ref records_.
  size_t memorySourceCount_;                  ///< Number of active entries in @ref memorySourceMap_.

  ParallelMoveRecord *records_;                                           ///< Fixed-capacity array of pending move operations to resolve.
  MemorySourceEntry *memorySourceMap_;                                    ///< Fixed-capacity array of active stack-source usage counts.
  std::array<uint32_t, static_cast<size_t>(TReg::NUMREGS)> regSourceMap_; ///< Register-source usage counts indexed by register number.
};
} // namespace vb

#endif
