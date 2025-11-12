///
/// @file RegisterCopyResolver.hpp
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
#ifndef REGISTER_COPY_RESOLVER_HPP
#define REGISTER_COPY_RESOLVER_HPP
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/config.hpp"
#include "src/core/common/FunctionRef.hpp"
#include "src/core/compiler/backend/RegAdapter.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"

namespace vb {

/// @brief ResolverRecord is used to store the information about a register copy operation
/// @details Usage of this class:
/// 1. push the records which contains the reg may conflict during value assignment
/// 2. call resolve() to emit the moves and swaps
// coverity[autosar_cpp14_a11_0_1_violation]
struct ResolverRecord final {
  /// @brief target register types
  enum class TargetType : uint8_t {
    Normal,            ///< registers in x86_64, arm64 and tricore d[x]
    Extend,            ///< tricore e[x]
    Extend_Placeholder ///< higher half of tricore e[x]
  };
  VariableStorage target; ///< Target register to copy to

  VariableStorage source;                    ///< Source register or memory to copy from
  TargetType targetType{TargetType::Normal}; ///< Type of the target register
};
using MoveEmitter =
    FunctionRef<void(VariableStorage const &, VariableStorage const &)>; ///< Function type to emit a move operation from source to target
using SwapEmitter = FunctionRef<void(VariableStorage const &, VariableStorage const &,
                                     bool const)>; ///< Function type to emit a swap operation between source and target

/// @brief RegisterCopyResolver is used to calculate register/memory->register copy orders to avoid conflicts/overwritten
/// @tparam NumberOfTargetRegs number of registers used as parameters according to ABI
template <size_t NumberOfTargetRegs> class RegisterCopyResolver final {
public:
  ///@brief constructor
  inline RegisterCopyResolver() VB_NOEXCEPT : records_{}, usedAsSourceMap_{}, cursor_{0U}, movedCounter_{0U} {
  }

  /// @brief Resolve the register copies and emit the moves and swaps
  /// @param moveEmitter Function to emit a move operation from source to target
  /// @param swapEmitter Function to emit a swap operation between source and target
  /// @throws Any exception thrown by the moveEmitter or swapEmitter
  void resolve(MoveEmitter const &moveEmitter, SwapEmitter const &swapEmitter) VB_THROW {
    // Algorithm:
    // 1. iterate over all records and check if the source is used as a source
    // 2. If a source is used as target, this reg is regarded as conflict
    // 3. Move all registers without conflict, free the source registers, until no more moves can be done
    // 4. If there are still records left, it means there is cyclic reference, so we need to swap the registers
    if (cursor_ == 0U) {
      return;
    }

    for (size_t i{0U}; i < cursor_; i++) {
      ResolverRecord const &record{records_[i]};
      if (record.source.type == StorageType::REGISTER) {
        TReg const sourceReg{record.source.location.reg};
        size_t const sourceRegIndex{static_cast<size_t>(sourceReg)};
        // GCOVR_EXCL_START
        assert(sourceRegIndex < usedAsSourceMap_.size());
        // GCOVR_EXCL_STOP
        usedAsSourceMap_[sourceRegIndex] += 1U;
      }
    }

    moveAllWithoutConflict(moveEmitter);

    uint32_t remainingCounter{0U};
    for (size_t i{0U}; i < cursor_; i++) {
      ResolverRecord const &record{records_[i]};
      if (isOperationalRecord(record)) {
        remainingCounter++;
      }
    }
    if (remainingCounter > 0U) {
      // GCOVR_EXCL_START
      assert(swapEmitter.notNull() && "SwapEmitter must not be null");
      assert(remainingCounter > 1U);
      assert(cursor_ >= 1U);
      // GCOVR_EXCL_STOP
      size_t swapIndex{getFirstOperationalRecord()};

      bool swapContains64{false};
      for (size_t i{swapIndex}; i < cursor_; i++) {
        ResolverRecord const &record{records_[i]};
        if (isOperationalRecord(record)) {
          if (MachineTypeUtil::is64(record.target.machineType)) {
            swapContains64 = true;
            break;
          }
        }
      }

      while (swapIndex != notFound) {
        // coverity[autosar_cpp14_a8_5_2_violation] tasking compiler must init record in this way
        ResolverRecord &record = records_[swapIndex];
        swapEmitter(record.target, record.source, swapContains64);
        markAsSwapped(record);

        size_t const next{getNextSwap(record.source.location.reg)};
        // coverity[autosar_cpp14_a8_5_2_violation] tasking compiler must init record in this way
        ResolverRecord &nextRecord = records_[next];
        size_t const nextNext{getNextSwap(nextRecord.source.location.reg)};
        if (nextNext == notFound) { // no next next means current cycle has been finished
          markAsSwapped(nextRecord);
          // look for next cycle
          swapIndex = getFirstOperationalRecord();
        } else {
          swapIndex = next;
        }
      }
    }
  }

  /// @brief Push a new register copy operation onto the resolver
  /// @note the target must be a register
  /// @param target Target register to copy to
  /// @param source Source register or memory to copy from
  inline void push(VariableStorage const &target, VariableStorage const &source) VB_NOEXCEPT {
    push(target, ResolverRecord::TargetType::Normal, source);
  }

  /// @brief Push a new register copy operation onto the resolver
  /// @param target Target register to copy to
  /// @param targetType Type of the target register
  /// @param source Source register or memory to copy from
  void push(VariableStorage const &target, ResolverRecord::TargetType const targetType, VariableStorage const &source) VB_NOEXCEPT {
    // GCOVR_EXCL_START
    assert(cursor_ < NumberOfTargetRegs && "RegisterCopyResolver overflow");
    // GCOVR_EXCL_STOP
    records_[cursor_].target = target;
    records_[cursor_].source = source;
    records_[cursor_].targetType = targetType;
    cursor_++;
  }

private:
  /// @brief Check if a register is used as a source in the resolver
  /// @param target Register to check
  ///
  inline bool usedAsSource(VariableStorage const &target) const VB_NOEXCEPT {
    if (target.type == StorageType::REGISTER) {
      TReg const targetReg{target.location.reg};
      size_t const targetRegIndex{static_cast<size_t>(targetReg)};
      // GCOVR_EXCL_START
      assert(targetRegIndex < usedAsSourceMap_.size());
      // GCOVR_EXCL_STOP
      return usedAsSourceMap_[targetRegIndex] != 0U;
    } else {
      return false;
    }
  }

  /// @brief mark a register as unused
  /// @param target Register to mark
  inline void setAsUnused(VariableStorage const &target) VB_NOEXCEPT {
    if (target.type == StorageType::REGISTER) {
      TReg const targetReg{target.location.reg};
      // coverity[autosar_cpp14_a4_5_1_violation]
      size_t const targetRegIndex{static_cast<size_t>(targetReg)};
      // GCOVR_EXCL_START
      assert(targetRegIndex < usedAsSourceMap_.size());
      // GCOVR_EXCL_STOP
      usedAsSourceMap_[targetRegIndex] -= 1U;
    }
  }

  /// @brief Move all registers without conflict
  /// @param moveEmitter Function to emit a move operation from source to target
  inline void moveAllWithoutConflict(MoveEmitter const &moveEmitter) VB_THROW {
    while (true) {
      bool const didMove{tryMove(moveEmitter)};
      if (!didMove) {
        break;
      }
    }
  }

  /// @brief Iterate over all records and try to move them if there is no conflict
  /// @param moveEmitter Function to emit a move operation from source to target
  bool tryMove(MoveEmitter const &moveEmitter) VB_THROW {
    bool didMove{false};
    for (size_t i{0U}; i < cursor_; i++) {
      // coverity[autosar_cpp14_a8_5_2_violation] tasking compiler must init record in this way
      ResolverRecord &record = records_[i];
      VariableStorage const &target{record.target};
      if (isOperationalRecord(record)) {
        if (!usedAsSource(target)) {
          moveEmitter(target, record.source);

          setAsUnused(record.source);
          record.target = VariableStorage{};
          if (record.targetType == ResolverRecord::TargetType::Extend) {
            markExtendRegAsUnused(i);
          }
          didMove = true;
          movedCounter_++;
        }
      }
    }
    return didMove;
  }

  /// @brief Mark the extend register as unused
  /// @param index Index of the base register in the records array
  void markExtendRegAsUnused(size_t const index) VB_NOEXCEPT {
    size_t const extendIndex{index + 1U};
    // GCOVR_EXCL_START
    assert(extendIndex < cursor_ && "Extend target must have a placeholder");
    // GCOVR_EXCL_STOP
    setAsUnused(records_[extendIndex].source);
    records_[extendIndex].source = VariableStorage{};
  }

  /// @brief Get the next swap index
  /// @param targetReg target register to find the next swap for
  size_t getNextSwap(TReg const targetReg) const VB_NOEXCEPT {
    for (size_t i{0U}; i < cursor_; i++) {
      ResolverRecord const &record{records_[i]};
      if (isOperationalRecord(record) && (record.target.location.reg == targetReg)) {
        return i;
      }
    }
    return notFound;
  }

  /// @brief Check if a record is operational, i.e., it has a valid target and is not an extend placeholder
  /// @param record Record to check
  /// @return true if the record is operational, false otherwise
  inline bool isOperationalRecord(ResolverRecord const &record) const VB_NOEXCEPT {
    return (record.target.type != StorageType::INVALID) && (record.targetType != ResolverRecord::TargetType::Extend_Placeholder);
  }

  /// @brief Get the index of the first operational record
  /// @return Index of the first operational record
  size_t getFirstOperationalRecord() const VB_NOEXCEPT {
    size_t index{0U};
    for (; index < static_cast<size_t>(cursor_); index++) {
      ResolverRecord const &record{records_[index]};
      if (isOperationalRecord(record)) {
        return index;
      }
    }
    return notFound;
  }

  /// @brief Mark a record as swapped
  /// @param record Record to mark
  void markAsSwapped(ResolverRecord &record) VB_NOEXCEPT {
    setAsUnused(record.target);
    record.target = VariableStorage{};
  }

  std::array<ResolverRecord, NumberOfTargetRegs> records_;                   ///< Array of register copy operations to resolve
  std::array<uint32_t, static_cast<size_t>(TReg::NUMREGS)> usedAsSourceMap_; ///< Map to check if a register is used as a source
  uint32_t cursor_{0U};                                                      ///< Current cursor in the records array
  uint32_t movedCounter_;                     ///< Counter for the number of moved registers, used to check if all registers were moved
  static constexpr size_t notFound{SIZE_MAX}; ///< alias def when a record is not found
};
} // namespace vb

#endif
