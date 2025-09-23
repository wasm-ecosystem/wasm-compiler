///
/// @file IAnalytics.hpp
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
#ifndef SRC_CORE_COMPILER_EXTENSIONS_IANALYTICS_HPP
#define SRC_CORE_COMPILER_EXTENSIONS_IANALYTICS_HPP

#include <cstdint>

#include "src/core/compiler/frontend/SectionType.hpp"

namespace vb {
namespace extension {

///
/// @brief Class to globally collect compile-time analytics
class IAnalytics {
public:
  /// @brief default constructor
  IAnalytics() = default;
  /// @brief copy constructor
  IAnalytics(IAnalytics const &) = default;
  /// @brief move constructor
  IAnalytics(IAnalytics &&) = default;
  /// @brief copy operator
  IAnalytics &operator=(IAnalytics const &) & = default;
  /// @brief move operator
  IAnalytics &operator=(IAnalytics &&) & = default;
  /// @brief destructor
  virtual ~IAnalytics() = default;
  ///
  /// @brief Increment the counter for spills to registers or to stack
  ///
  /// @param toStack Whether the spill is to stack (true) or to a register (false9)
  virtual void incrementSpillCount(bool const toStack) = 0;

  ///
  /// @brief Update the max number of StackElements on the stack
  /// NOTE: Will only update if the new value is larger than the current maximum
  ///
  /// @param stackElementCount New count
  virtual void updateMaxStackElementCount(uint32_t const stackElementCount) = 0;

  ///
  /// @brief Update the max stackFrameSize
  /// NOTE: Will only update if the new value is larger than the current maximum
  ///
  /// @param stackFrameSize New stackFrameSize
  virtual void updateMaxStackFrameSize(uint32_t const stackFrameSize) = 0;

  ///
  /// @brief Update the max number of used TempStack slots on the runtime stack
  ///
  /// @param usedTempStackSlots New used slots
  /// @param activeTempStackSlots Total active stack slots (counted from highest element to fixed portion)
  virtual void updateMaxUsedTempStackSlots(uint32_t const usedTempStackSlots, uint32_t const activeTempStackSlots) = 0;

  ///
  /// @brief Add a sample to the register pressure histogram, this should be called every time a register is allocated
  ///
  /// @param isGPR Whether the register that is to be allocated is a GPR (true) or FPR (false)
  /// @param numFreeRegs Number of free registers when the allocation is triggered (so before the allocation)
  virtual void updateRegPressureHistogram(bool const isGPR, uint32_t const numFreeRegs) = 0;

  ///
  /// @brief Update the maximum size of the compilerMemory
  /// NOTE: Will only update if the new value is larger than the current maximum
  ///
  /// @param compilerMemorySize New compilerMemory size
  virtual void updateMaxCompilerMemorySize(uint32_t const compilerMemorySize) = 0;
  ///
  /// @brief Notify that the parsing and handling of a Wasm section is done
  /// NOTE: Should be called after a section is done, before the new section is parsed
  ///
  /// @param sectionType Type of the section that was just parsed
  /// @param compilerMemorySize Current size of the compilerMemory
  virtual void notifySectionParsingDone(SectionType const sectionType, uint32_t const compilerMemorySize) = 0;

  ///
  /// @brief Notify that the serialization of the output binary is done
  ///
  /// @param compilerMemorySize Current size of the compilerMemory
  virtual void notifySerializationDone(uint32_t const compilerMemorySize) = 0;
  ///
  /// @brief Set the input and output binary sizes
  ///
  /// @param bytecodeSize Input size (Wasm bytecode)
  /// @param jitSize Output size (Compiled binary)
  virtual void setBinarySizes(uint32_t const bytecodeSize, uint32_t const jitSize) = 0;

  ///
  /// @brief Update the max functionJitSize
  /// NOTE: Will only update if the new value is larger than the current maximum
  ///
  /// @param functionJitSize New functionJitSize
  virtual void updateMaxFunctionJitSize(uint32_t const functionJitSize) = 0;
};

} // namespace extension
} // namespace vb

#endif // SRC_CORE_COMPILER_EXTENSIONS_IANALYTICS_HPP
