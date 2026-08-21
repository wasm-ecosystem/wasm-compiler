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
#include "src/core/compiler/backend/AssemblerAdapter.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_assembler.hpp"
#include "src/core/compiler/backend/tricore/tricore_assembler.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_assembler.hpp"
#include "src/core/compiler/common/BumpAllocator.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"

namespace vb {

using ParallelMoveEmitter =
    FunctionRef<void(VariableStorage const &, VariableStorage const &)>; ///< Function type to emit a move operation from source to target.
using ParallelMoveTempProvider = FunctionRef<VariableStorage(VariableStorage const &)>; ///< Function type to allocate a temp location for a cycle
                                                                                        ///< header source. Only called once per cycle
using SwapEmitter = FunctionRef<void(VariableStorage const &, VariableStorage const &,
                                     bool const)>; ///< Function type to emit a swap operation between source and target

/// @brief Target storage types used by ParallelMoveResolver.
enum class ParallelMoveTargetType : uint8_t {
  Normal,            ///< normal register or stack-memory target
  Extend,            ///< TriCore e[x] low half record
  Extend_Placeholder ///< TriCore e[x] high half placeholder record
};

/// @brief ParallelMoveResolver is used to do register<->stackMemory move orders to avoid conflicts/overwritten
class ParallelMoveResolver final {
  /// @brief Stores one pending target-source move pair.
  // coverity[autosar_cpp14_a11_0_1_violation]
  struct ParallelMoveRecord final {
    VariableStorage target{};                                          ///< Target location that will receive the move.
    VariableStorage source{};                                          ///< Source location that still needs to be moved.
    ParallelMoveTargetType targetType{ParallelMoveTargetType::Normal}; ///< Type of the target location.
  };

  /// @brief One memory-source usage count keyed by its 8-byte stack/linkData slot offset.
  // coverity[autosar_cpp14_a11_0_1_violation]
  struct MemorySourceEntry final {
    uint32_t offset;    ///< Stack/LinkedData slot offset that is still used as a source.
    uint32_t count;     ///< Number of unresolved moves that still read from this memory slot.
    bool isStackMemory; ///< true if the offset used for stack memory. false if used for linkData
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
    assert(recordsCapacity_ <= (ImplementationLimits::numParams * 2U) && "ParallelMoveResolver storage overflow");

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

  /// @brief Resolve the pending parallel moves without cycles.
  /// @param moveEmitter Function to emit a move operation from source to target
  /// @throws Any exception thrown by the moveEmitter
  void resolveLinear(ParallelMoveEmitter const &moveEmitter) VB_THROW {
    assert(moveEmitter.notNull() && "must provide a moveEmitter to resolve moves");
    if (recordsCount_ != 0U) {
      initSourceUsage();
      moveAllWithoutConflict(moveEmitter);
    }
  }

  /// @brief Resolve the pending parallel moves with cycles through an explicit temp location.
  /// @param moveEmitter Function to emit a move operation from source to target
  /// @param tempProvider Function to provide a temp location for the source of the current cycle header
  /// @throws Any exception thrown by the moveEmitter or tempProvider
  void resolveCycle(ParallelMoveEmitter const &moveEmitter, ParallelMoveTempProvider const &tempProvider) VB_THROW {
    assert(tempProvider.notNull() && "ParallelMoveTempProvider must not be null when cycles remain");
    while (recordsCount_ != 0U) {
      resolveCycleImpl(moveEmitter, tempProvider);
      moveAllWithoutConflict(moveEmitter);
    }
  }

  /// @brief Resolve the pending parallel moves with cycles through an explicit swap operation.
  /// @param moveEmitter Function to emit a move operation from source to target
  /// @param swapEmitter Function to emit a swap operation between source and target
  /// @throws Any exception thrown by the moveEmitter or swapEmitter
  void resolveCycle(ParallelMoveEmitter const &moveEmitter, SwapEmitter const &swapEmitter) VB_THROW {
    assert(swapEmitter.notNull() && "SwapEmitter must not be null when cycles remain");
    while (recordsCount_ != 0U) {
      resolveCycleImpl(swapEmitter);
      moveAllWithoutConflict(moveEmitter);
    }
  }

  /// @brief Push a new copy operation onto the resolver.
  /// @param target Target register or memory to copy to
  /// @param source Source register or memory to copy from
  inline void push(VariableStorage const &target, VariableStorage const &source) VB_NOEXCEPT {
    push(target, ParallelMoveTargetType::Normal, source);
  }

  /// @brief Push a new copy operation onto the resolver.
  /// @param target Target register or memory to copy to
  /// @param targetType Type of the target location
  /// @param source Source register or memory to copy from
  void push(VariableStorage const &target, ParallelMoveTargetType const targetType, VariableStorage const &source) VB_NOEXCEPT {
    if (target.inSameLocation(source)) {
      return;
    }

    // GCOVR_EXCL_START
    assert(recordsCount_ < recordsCapacity_ && "ParallelMoveResolver overflow");
    if (recordsCount_ >= recordsCapacity_) {
      UNREACHABLE(return, "ParallelMoveResolver push overflow");
    }
    // GCOVR_EXCL_STOP
    records_[recordsCount_] = ParallelMoveRecord{target, source, targetType};
    recordsCount_++;
  }

  /// @brief Check if a temp location is safe for all still pending moves.
  /// @param tempReg Candidate temp reg
  /// @return True if the temp conflict with pending source
  bool regUsedAsSource(TReg const tempReg) const VB_NOEXCEPT {
    for (size_t i{0U}; i < recordsCapacity_; i++) {
      ParallelMoveRecord const &record{records_[i]};
      if ((record.source.type == StorageType::REGISTER) && (record.source.location.reg == tempReg)) {
        return true;
      }
    }

    return false;
  }

  /// @brief Get the number of pending move records.
  /// @return Number of pending move records
  inline size_t getRecordsCount() const VB_NOEXCEPT {
    return recordsCount_;
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
    for (size_t i{0U}; i < recordsCapacity_; i++) {
      ParallelMoveRecord const record{records_[i]};
      // A move is safe once no other unresolved record still needs the target's current value.
      if (isOperationalRecord(record) && (!targetIsUsedAsSource(record.target))) {
        VariableStorage const resolvedSource{record.source};
        moveEmitter(record.target, record.source);
        unmarkSourceAsUsed(resolvedSource);
        eraseRecordWithExtendPlaceholder(i);
        storageReplacement(record);
        return true; // must return, because the counts has been modified
      }
    }
    return false;
  }

  /// @brief Resolve one remaining cycle through an explicit temp location.
  /// @param moveEmitter Function to emit a move operation from source to target
  /// @param tempProvider Function to provide the temp location for the cycle header source
  void resolveCycleImpl(ParallelMoveEmitter const &moveEmitter, ParallelMoveTempProvider const &tempProvider) VB_THROW {
    size_t const cycleHeadIndex{getFirstOperationalRecord()};
    // GCOVR_EXCL_START
    assert(cycleHeadIndex != notFound && "Remaining moves must contain an operational record");
    if (cycleHeadIndex == notFound) {
      return;
    }
    // GCOVR_EXCL_STOP

    ParallelMoveRecord const cycleHeadCopy{records_[cycleHeadIndex]};
    VariableStorage const tempStorage{tempProvider(cycleHeadCopy.source)};
    // Preserve the cycle head source first so its location becomes the initial writable hole.
    assert(tempStorage.type == StorageType::REGISTER && "Must get register");
    moveEmitter(tempStorage, cycleHeadCopy.source);
    unmarkSourceAsUsed(cycleHeadCopy.source);
    eraseRecordWithExtendPlaceholder(cycleHeadIndex); // erase the cycle header record

    size_t nextIndex{findRecordTargetingFreedLocation(cycleHeadCopy.source)};
    assert(nextIndex != notFound && "Remaining moves must contain a cycle");

    while (nextIndex != notFound) {
      ParallelMoveRecord const record{records_[nextIndex]};
      // Each step writes into the location freed by the previous step and creates the next hole at record.source.
      moveEmitter(record.target, record.source);
      unmarkSourceAsUsed(record.source);
      eraseRecordWithExtendPlaceholder(nextIndex);
      nextIndex = findRecordTargetingFreedLocation(record.source);
    }

    moveEmitter(cycleHeadCopy.target, tempStorage);
  }

  /// @brief Resolve one remaining cycle through swap operations.
  /// @param swapEmitter Function to emit a swap operation between source and target
  void resolveCycleImpl(SwapEmitter const &swapEmitter) VB_THROW {
    size_t swapIndex{getFirstOperationalRecord()};
    // GCOVR_EXCL_START
    assert(swapIndex != notFound && "Remaining moves must contain an operational record");
    if (swapIndex == notFound) {
      return;
    }
    // GCOVR_EXCL_STOP

    bool const swapContains64{remainingCycleContains64BitTarget()};
    while (swapIndex != notFound) {
      ParallelMoveRecord const record{records_[swapIndex]};
      swapEmitter(record.target, record.source, swapContains64);
      markAsSwapped(swapIndex);

      size_t const next{findRecordTargetingFreedLocation(record.source)};
      // GCOVR_EXCL_START
      assert(next != notFound && "Remaining swap moves must contain a cycle");
      if (next == notFound) {
        swapIndex = getFirstOperationalRecord();
        continue;
      }
      // GCOVR_EXCL_STOP

      ParallelMoveRecord const nextRecord{records_[next]};
      size_t const nextNext{findRecordTargetingFreedLocation(nextRecord.source)};
      if (nextNext == notFound) {
        markAsSwapped(next);
        swapIndex = getFirstOperationalRecord();
      } else {
        swapIndex = next;
      }
    }
  }

  /// @brief Find the remaining record whose target writes into the location freed by the previous cycle step.
  /// @param freedLocation Location whose old value has already been preserved and can now be overwritten
  /// @return The index of the matching record, or notFound if there is no pending match
  size_t findRecordTargetingFreedLocation(VariableStorage const &freedLocation) const VB_NOEXCEPT {
    for (size_t i{0U}; i < recordsCapacity_; i++) {
      ParallelMoveRecord const &record{records_[i]};
      if (isOperationalRecord(record) && record.target.inSameLocation(freedLocation)) {
        return i;
      }
    }
    return notFound;
  }

  /// @brief Check if one of the remaining cycle targets is 64-bit.
  /// @return true if a remaining operational target is 64-bit
  bool remainingCycleContains64BitTarget() const VB_NOEXCEPT {
    for (size_t i{0U}; i < recordsCapacity_; i++) {
      ParallelMoveRecord const &record{records_[i]};
      if (isOperationalRecord(record) && MachineTypeUtil::is64(record.target.machineType)) {
        return true;
      }
    }
    return false;
  }

  /// @brief Mark a record resolved by a swap operation.
  /// @param index Index of the swapped record
  void markAsSwapped(size_t const index) VB_NOEXCEPT {
    // A swap consumes the original value stored in the target location, which is tracked as a pending source elsewhere in the cycle.
    unmarkSourceAsUsed(records_[index].target);
    if (records_[index].targetType == ParallelMoveTargetType::Extend) {
      size_t const placeholderIndex{index + 1U};
      // GCOVR_EXCL_START
      assert(placeholderIndex < recordsCapacity_ && "Extend target must have a placeholder");
      assert(records_[placeholderIndex].targetType == ParallelMoveTargetType::Extend_Placeholder);
      // GCOVR_EXCL_STOP
      unmarkSourceAsUsed(records_[placeholderIndex].target);
      eraseRecord(placeholderIndex);
    }
    eraseRecord(index);
  }

  /// @brief Erase one resolved record from the active move list by invalidating its slot.
  /// @param index Index of the record to erase
  inline void eraseRecord(size_t const index) VB_NOEXCEPT {
    // GCOVR_EXCL_START
    assert(index < recordsCapacity_);
    assert(records_[index].target.type != StorageType::INVALID);
    // GCOVR_EXCL_STOP
    records_[index] = ParallelMoveRecord{};
    recordsCount_--;
  }

  /// @brief Erase a resolved operational record and its optional Extend placeholder.
  /// @param index Index of the operational record to erase
  void eraseRecordWithExtendPlaceholder(size_t const index) VB_NOEXCEPT {
    if (records_[index].targetType != ParallelMoveTargetType::Extend) {
      eraseRecord(index);
      return;
    }

    // Extend_Placeholder is always at index+1 when paired with Extend
    size_t const placeholderIndex{index + 1U};
    // GCOVR_EXCL_START
    assert(placeholderIndex < recordsCapacity_ && "Extend target must have a placeholder");
    assert(records_[placeholderIndex].targetType == ParallelMoveTargetType::Extend_Placeholder);
    assert(records_[placeholderIndex].target.type != StorageType::INVALID);
    // GCOVR_EXCL_STOP
    unmarkSourceAsUsed(records_[placeholderIndex].source);
    eraseRecord(placeholderIndex);
    eraseRecord(index);
  }

  /// @brief Record one pending source in the fast usage maps.
  /// @param sourceStorage Source location to track
  void markSourceAsUsed(VariableStorage const &sourceStorage) VB_NOEXCEPT {
    if (sourceStorage.type == StorageType::CONSTANT) {
      return;
    }
    if (sourceStorage.type == StorageType::REGISTER) {
      incrementRegisterSource(sourceStorage.location.reg);
    } else {
      incrementMemorySource(sourceStorage);
    }
  }

  /// @brief Remove one resolved source from the usage maps.
  /// @param sourceStorage Source location that is no longer pending
  void unmarkSourceAsUsed(VariableStorage const &sourceStorage) VB_NOEXCEPT {
    if (sourceStorage.type == StorageType::CONSTANT) {
      return;
    }
    if (sourceStorage.type == StorageType::REGISTER) {
      decrementRegisterSource(sourceStorage.location.reg);
    } else {
      assert((sourceStorage.type == StorageType::STACKMEMORY) || (sourceStorage.type == StorageType::LINKDATA));
      decrementMemorySource(sourceStorage);
    }
  }

  /// @brief Check whether the target location is still used as a source by another pending record.
  /// @param targetStorage Target location to inspect
  /// @return True if another pending source uses the target location
  bool targetIsUsedAsSource(VariableStorage const &targetStorage) const VB_NOEXCEPT {
    if (targetStorage.type == StorageType::REGISTER) {
      return registerUsedAsSource(targetStorage.location.reg);
    }

    assert((targetStorage.type == StorageType::STACKMEMORY) || (targetStorage.type == StorageType::LINKDATA));
    size_t const index{findMemorySource(targetStorage)};
    return (index != notFound) && (memorySourceMap_[index].count > 0U);
  }

  /// @brief Mark one physical register as used by an unresolved source.
  /// @param reg Register to mark
  void incrementRegisterSource(TReg const reg) VB_NOEXCEPT {
    size_t const regIndex{static_cast<size_t>(reg)};
    // GCOVR_EXCL_START
    assert(regIndex < static_cast<size_t>(TReg::NUMREGS));
    // GCOVR_EXCL_STOP
    regSourceMap_[regIndex] += 1U;
  }

  /// @brief Remove one physical register source usage.
  /// @param reg Register to unmark
  void decrementRegisterSource(TReg const reg) VB_NOEXCEPT {
    size_t const regIndex{static_cast<size_t>(reg)};
    // GCOVR_EXCL_START
    assert(regIndex < static_cast<size_t>(TReg::NUMREGS));
    assert(regSourceMap_[regIndex] > 0U);
    // GCOVR_EXCL_STOP
    regSourceMap_[regIndex] -= 1U;
  }

  /// @brief Check whether one physical register is used by an unresolved source.
  /// @param reg Register to check
  /// @return True if the register is still used as source
  bool registerUsedAsSource(TReg const reg) const VB_NOEXCEPT {
    size_t const regIndex{static_cast<size_t>(reg)};
    // GCOVR_EXCL_START
    assert(regIndex < static_cast<size_t>(TReg::NUMREGS));
    // GCOVR_EXCL_STOP
    return regSourceMap_[regIndex] > 0U;
  }

  /// @brief Check if a record is operational, i.e. it has a valid target and is not an extend placeholder.
  /// @param record Record to check
  /// @return true if the record is operational, false otherwise
  inline bool isOperationalRecord(ParallelMoveRecord const &record) const VB_NOEXCEPT {
    return (record.target.type != StorageType::INVALID) && (record.targetType != ParallelMoveTargetType::Extend_Placeholder);
  }

  /// @brief Check whether a constant source needs to be encoded into multiple instructions.
  /// @param sourceStorage Source location to inspect
  /// @return true when sourceStorage is a one-instruction constant
  static bool multiInstructionImmediate(VariableStorage const &sourceStorage) VB_NOEXCEPT {
    if (sourceStorage.type != StorageType::CONSTANT) {
      // Not immediate at all
      return false;
    }
    uint64_t rawImmediate{0U};
    switch (sourceStorage.machineType) {
    case MachineType::I32:
      rawImmediate = static_cast<uint64_t>(sourceStorage.location.constUnion.u32);
      break;
    case MachineType::I64:
      rawImmediate = sourceStorage.location.constUnion.u64;
      break;
    case MachineType::F32:
      rawImmediate = static_cast<uint64_t>(sourceStorage.location.constUnion.rawF32());
      break;
    case MachineType::F64:
      rawImmediate = sourceStorage.location.constUnion.rawF64();
      break;
    case MachineType::INVALID:
    default:
      UNREACHABLE(return false, "Invalid constant MachineType");
    }
    return !TAssembler::isImmediateEncodableInOneInstruction(sourceStorage.machineType, rawImmediate);
  }

  /// @brief Get the first operational record.
  /// @return Index of the first operational record, or notFound if there is none
  size_t getFirstOperationalRecord() const VB_NOEXCEPT {
    for (size_t i{0U}; i < recordsCapacity_; i++) {
      if (isOperationalRecord(records_[i])) {
        return i;
      }
    }
    return notFound;
  }

  /// @brief Find a stack-source usage entry by its slot offset.
  /// @param sourceStorage source storage to look up
  /// @return The index of the matching entry, or notFound if there is none
  size_t findMemorySource(VariableStorage const &sourceStorage) const VB_NOEXCEPT {
    bool const isStackMemory{sourceStorage.type == StorageType::STACKMEMORY};
    uint32_t const offset{isStackMemory ? sourceStorage.location.stackFramePosition : sourceStorage.location.linkDataOffset};
    for (size_t i{0U}; i < memorySourceCount_; i++) {
      if ((memorySourceMap_[i].isStackMemory == isStackMemory) && (memorySourceMap_[i].offset == offset)) {
        return i;
      }
    }
    return notFound;
  }

  /// @brief Increment the usage count for a stack-source location, inserting it if new.
  /// @param sourceStorage Stack source to track
  void incrementMemorySource(VariableStorage const &sourceStorage) VB_NOEXCEPT {
    size_t const index{findMemorySource(sourceStorage)};
    if (index != notFound) {
      memorySourceMap_[index].count += 1U;
      return;
    }

    // GCOVR_EXCL_START
    assert(memorySourceCount_ < recordsCapacity_);
    // GCOVR_EXCL_STOP
    if (memorySourceCount_ < recordsCapacity_) {
      bool const isStackMemory{sourceStorage.type == StorageType::STACKMEMORY};
      memorySourceMap_[memorySourceCount_] =
          MemorySourceEntry{isStackMemory ? sourceStorage.location.stackFramePosition : sourceStorage.location.linkDataOffset, 1U, isStackMemory};
      memorySourceCount_++;
    }
  }

  /// @brief Decrement the usage count for a stack-source location, removing it when it reaches zero.
  /// @param sourceStorage Memory source to untrack
  void decrementMemorySource(VariableStorage const &sourceStorage) VB_NOEXCEPT {
    size_t const index{findMemorySource(sourceStorage)};
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

  ///
  /// @brief Replace pending sources with the register populated by a resolved stack-to-register move.
  ///
  /// Reusing the register avoids redundant loads from the same stack slot for later pending moves and keeps
  /// the register and memory source usage maps consistent with the rewritten records.
  ///
  /// @param record Resolved move record whose stack source has been copied to its register target
  void storageReplacement(ParallelMoveRecord const &record) VB_NOEXCEPT {
    if (record.target.type != StorageType::REGISTER) {
      return;
    }
    StorageType const sourceType{record.source.type};
    if ((sourceType == StorageType::STACKMEMORY) || ((sourceType == StorageType::LINKDATA) || multiInstructionImmediate(record.source))) {
      for (size_t i{0U}; i < recordsCapacity_; i++) {
        ParallelMoveRecord &pendingRecord{records_[i]};
        bool const sameMachineType{pendingRecord.source.machineType == record.source.machineType};
        if (isOperationalRecord(pendingRecord) && (pendingRecord.source.inSameLocation(record.source) && sameMachineType)) {
          unmarkSourceAsUsed(pendingRecord.source);
          pendingRecord.source = VariableStorage::reg(record.target.location.reg, record.source.machineType);
          incrementRegisterSource(pendingRecord.source.location.reg);
        }
      }
    }
  }

  FreeFnc freeFnc_; ///< Deallocator for resolver-owned buffers.
  void *ctx_;       ///< User-defined allocator context.

  static constexpr size_t notFound{SIZE_MAX}; ///< Alias when a record is not found
  uint32_t recordsCapacity_;                  ///< Maximum number of active records supported by @ref records_ and @ref memorySourceMap_.
  size_t recordsCount_;                       ///< Number of active records in @ref records_.
  size_t memorySourceCount_;                  ///< Number of active entries in @ref memorySourceMap_. Includes both stack and linkData sources.

  ParallelMoveRecord *records_;        ///< Fixed-capacity array of pending move operations to resolve. Will not fill in the blank slot with tail.
  MemorySourceEntry *memorySourceMap_; ///< Fixed-capacity array of active memory-source usage counts. Will fill in the blank slot with tail.
  std::array<uint32_t, static_cast<size_t>(TReg::NUMREGS)> regSourceMap_; ///< Register-source usage counts indexed by register number.
};
} // namespace vb

#endif
