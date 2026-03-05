///
/// @file IMemoryManager.hpp
/// @copyright Copyright (C) 2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

#ifndef IMEMORYMANAGER_HPP
#define IMEMORYMANAGER_HPP

#include <cstdint>

#include "src/config.hpp"

namespace vb {

/// @brief memory manager interface for wasm linear memory management
class IMemoryManager {
public:
  IMemoryManager() VB_NOEXCEPT = default;
  /// @brief deconstructor
  // coverity[autosar_cpp14_a12_8_6_violation]
  virtual ~IMemoryManager() = default;
  /// @brief constructor
  // coverity[autosar_cpp14_a12_8_6_violation]
  IMemoryManager(IMemoryManager const &) VB_NOEXCEPT = default;
  /// @brief constructor
  // coverity[autosar_cpp14_a12_8_6_violation]
  IMemoryManager(IMemoryManager &&) VB_NOEXCEPT = default;
  /// @brief constructor
  // coverity[autosar_cpp14_a12_8_6_violation]
  IMemoryManager &operator=(IMemoryManager const &) &VB_NOEXCEPT = default;
  /// @brief constructor
  // coverity[autosar_cpp14_a12_8_6_violation]
  IMemoryManager &operator=(IMemoryManager &&) &VB_NOEXCEPT = default;

  ///
  /// @brief Initialize the memory for a WebAssembly module so that the linear memory after the basedata starts at a
  /// memory page boundary
  ///
  /// @param basedataSize Size of the module's basedata section in the job memory (Part before the linear memory starts)
  /// @param initialLinMemPages Maximum size of the module's linear memory (in Wasm pages)
  /// @return uint8_t* Start of the WebAssembly basedata
  virtual uint8_t *init(uint32_t const basedataSize, uint32_t const initialLinMemPages) = 0;

  ///
  /// @brief extend linear memory
  ///
  /// @param newTotalLinMemPages new total linear memory pages
  /// @return true extend success
  /// @return false extend failed
  ///
  virtual bool extend(uint32_t const newTotalLinMemPages) = 0;

  ///
  /// @brief shrink linear memory to minimal length
  ///
  /// @param minimumLength the minimal length needed by linear memory
  /// @return true shrink success
  /// @return false shrink failed
  ///
  virtual bool shrink(uint32_t const minimumLength) = 0;

  ///
  /// @brief probe access the linear memory
  ///
  /// @param linMemOffset the linear memory offset to probe
  /// @return true The probe address is ready commited or commit success
  /// @return false Commit failed during probe
  ///
  virtual bool probe(uint32_t const linMemOffset) = 0;

  ///
  /// @brief Calculate linear memory size
  ///
  /// @param baseDataSize base data size
  /// @return uint32_t Actual linear memory size
  ///
  virtual uint32_t getLinearMemorySize(uint32_t const baseDataSize) const = 0;
};

} // namespace vb

#endif // IMEMORYMANAGER_HPP
