///
/// @file Analytics.hpp
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
#ifndef ANALYTICS_HPP
#define ANALYTICS_HPP

#include <array>
#include <cstdint>
#include <string>

#include "src/extensions/IAnalytics.hpp"

namespace vb {
namespace extension {

///
/// @brief Class to globally collect compile-time analytics
class Analytics final : public extension::IAnalytics {
public:
  ///
  /// @brief Increment the counter for spills to registers or to stack
  ///
  /// @param toStack Whether the spill is to stack (true) or to a register (false9)
  inline void incrementSpillCount(bool const toStack) override {
    if (toStack) {
      spillsToStackCount_++;
    } else {
      spillsToRegCount_++;
    }
  }

  ///
  /// @brief Update the max number of StackElements on the stack
  /// NOTE: Will only update if the new value is larger than the current maximum
  ///
  /// @param stackElementCount New count
  inline void updateMaxStackElementCount(uint32_t const stackElementCount) override {
    maxStackElementCount_ = std::max(maxStackElementCount_, stackElementCount);
  }

  ///
  /// @brief Update the max stackFrameSize
  /// NOTE: Will only update if the new value is larger than the current maximum
  ///
  /// @param stackFrameSize New stackFrameSize
  inline void updateMaxStackFrameSize(uint32_t const stackFrameSize) override {
    maxStackFrameSize_ = std::max(maxStackFrameSize_, stackFrameSize);
  }

  ///
  /// @brief Update the max number of used TempStack slots on the runtime stack
  ///
  /// @param usedTempStackSlots New used slots
  /// @param activeTempStackSlots Total active stack slots (counted from highest element to fixed portion)
  void updateMaxUsedTempStackSlots(uint32_t const usedTempStackSlots, uint32_t const activeTempStackSlots) override;

  ///
  /// @brief Add a sample to the register pressure histogram, this should be called every time a register is allocated
  ///
  /// @param isGPR Whether the register that is to be allocated is a GPR (true) or FPR (false)
  /// @param numFreeRegs Number of free registers when the allocation is triggered (so before the allocation)
  void updateRegPressureHistogram(bool const isGPR, uint32_t const numFreeRegs) override;

  ///
  /// @brief Update the maximum size of the compilerMemory
  /// NOTE: Will only update if the new value is larger than the current maximum
  ///
  /// @param compilerMemorySize New compilerMemory size
  inline void updateMaxCompilerMemorySize(uint32_t const compilerMemorySize) override {
    maxCompilerMemorySize_ = std::max(maxCompilerMemorySize_, compilerMemorySize);
    maxCompilerMemorySizeCurrentSection_ = std::max(maxCompilerMemorySizeCurrentSection_, compilerMemorySize);
  }

  ///
  /// @brief Notify that the parsing and handling of a Wasm section is done
  /// NOTE: Should be called after a section is done, before the new section is parsed
  ///
  /// @param sectionType Type of the section that was just parsed
  /// @param compilerMemorySize Current size of the compilerMemory
  inline void notifySectionParsingDone(SectionType const sectionType, uint32_t const compilerMemorySize) override {
    MemoryUsage &memoryUsage = maxCompilerMemoryUsagePerSection_[static_cast<uint32_t>(sectionType)];
    updateMemoryUsage(memoryUsage, compilerMemorySize);
  }

  ///
  /// @brief Notify that the serialization of the output binary is done
  ///
  /// @param compilerMemorySize Current size of the compilerMemory
  inline void notifySerializationDone(uint32_t const compilerMemorySize) override {
    updateMemoryUsage(maxMemoryUsageForSerialization_, compilerMemorySize);
  }

  ///
  /// @brief Print the analytics to stdout
  void printAnalytics();

  ///
  /// @brief Set the input and output binary sizes
  ///
  /// @param bytecodeSize Input size (Wasm bytecode)
  /// @param jitSize Output size (Compiled binary)
  inline void setBinarySizes(uint32_t const bytecodeSize, uint32_t const jitSize) override {
    bytecodeSize_ = bytecodeSize;
    jitSize_ = jitSize;
  }

  ///
  /// @brief Update the max functionJitSize
  /// NOTE: Will only update if the new value is larger than the current maximum
  ///
  /// @param functionJitSize New functionJitSize
  inline void updateMaxFunctionJitSize(uint32_t const functionJitSize) override {
    maxFunctionJitSize_ = std::max(maxFunctionJitSize_, functionJitSize);
  }

  ///@brief  see @b jitSize_
  inline uint32_t getJitSize() const {
    return jitSize_;
  }

  ///@brief  see @b spillsToStackCount_
  inline uint32_t getSpillsToStackCount() const {
    return spillsToStackCount_;
  }

  ///@brief  see @b spillsToRegCount_
  inline uint32_t getSpillsToRegCount() const {
    return spillsToRegCount_;
  }

private:
  uint32_t bytecodeSize_ = 0U; ///< Input size (Wasm bytecode)
  uint32_t jitSize_ = 0U;      ///< Output size (Compiled binary)

  uint32_t spillsToStackCount_ = 0U; ///< Number of spills to stack
  uint32_t spillsToRegCount_ = 0U;   ///< Number of spills to registers

  uint32_t maxStackElementCount_ = 0U;    ///< Maximum number of StackElements on the compile-time stack
  uint32_t maxStackFrameSize_ = 0U;       ///< Maximum stack frame size
  uint32_t maxUsedTempStackSlots_ = 0U;   ///< Maximum number of used TempStack slots on the runtime stack
  uint32_t maxActiveTempStackSlots_ = 0U; ///< Maximum number of active (used or not used) TempStack slots above stack pointer
  uint32_t stackSlotSamples_ = 0U;        ///< Number of samples for stackSlot statistics
  float avgFragmentation_ = 0.0F;         ///< Average fragmentation of stack slots
  float maxFragmentation_ = 0.0F;         ///< Maximum fragmentation of stack slots

  uint32_t maxFunctionJitSize_ = 0U; ///< Maximum size of a function, in bytes (JIT code)

  uint32_t maxCompilerMemorySize_ = 0U; ///< Maximum size of compilerMemory

  uint32_t maxCompilerMemorySizeCurrentSection_ = 0U;   ///< Maximum size of compilerMemory during the current section
  uint32_t compilerMemorySizeCurrentSectionStart_ = 0U; ///< Size of compilerMemory at start of the current section

  ///
  /// @brief Memory usage for a section
  struct MemoryUsage final {
    uint32_t dyn = 0U;  ///< Dynamic part of the memory used, this is only temporary and will not be required after the
                        ///< end of the section
    uint32_t stat = 0U; ///< Static part of the memory used, this will stay allocated after the end of the section
  };

  std::array<MemoryUsage, 12> maxCompilerMemoryUsagePerSection_ = {}; ///< MemoryUsage for each Wasm section
  MemoryUsage maxMemoryUsageForSerialization_;                        ///< MemoryUsage during serialization

  static constexpr size_t maxRegsPerType = 32U; ///< Maximum number of registers per type for each backend
  std::array<uint32_t, maxRegsPerType + 1U> gprPressureHistogram_ = {
      0U}; ///< Histogram describing during how many reg allocs how many registers (index of array) were free (GPR)
  std::array<uint32_t, maxRegsPerType + 1U> fprPressureHistogram_ = {
      0U}; ///< Histogram describing during how many reg allocs how many registers (index of array) were free (FPR)

  ///
  /// @brief Update a memory usage and reset the recorded sizes for "current" section
  ///
  /// @param memoryUsage MemoryUsage instance to update
  /// @param compilerMemorySize Current size of compilerMemory
  void updateMemoryUsage(MemoryUsage &memoryUsage, uint32_t const compilerMemorySize);

  ///
  /// @brief Print information for MemoryUsage
  ///
  /// @param title Title (e.g. section name)
  /// @param memoryUsage MemoryUsage for which to print info
  void printMemoryUsage(std::string const &title, MemoryUsage const &memoryUsage);

  ///
  /// @brief Print information for memory usage for a specific section
  ///
  /// @param title Title (e.g. section name)
  /// @param sectionType SectionType for which to print info
  void printMemoryUsageForSection(std::string const &title, SectionType sectionType);
};

} // namespace extension
} // namespace vb

#endif
