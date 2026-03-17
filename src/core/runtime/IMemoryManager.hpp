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

/// @brief Common memory manager interface for Wasm linear memory handling
///
/// This interface defines the linear-memory contract used by the runtime and is intended to be shared across
/// different bounds-check modes (active and passive).
///
/// Two sizes are used by this contract:
/// - Allowed size: Size permitted by the Wasm specification for the module (formal size, page-based).
/// - Usable size: Size that is currently available for safe access.
///
/// These sizes are intentionally different. The usable size is smaller than the allowed size.
class IMemoryManager {
public:
  /// @brief Result of probing a linear-memory offset.
  enum class ProbeResult : uint8_t {
    Ok,                ///< Offset is usable.
    AllocationFailure, ///< Offset is in allowed range but required allocation failed.
    OutOfBounds,       ///< Offset exceeds the current allowed linear-memory range.
  };

  IMemoryManager() VB_NOEXCEPT = default;
  /// @brief destructor
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
  /// @brief Initialize memory state for one WebAssembly module instance
  ///
  /// Implementations set up basedata and the initial allowed/usable linear-memory configuration.
  ///
  /// @param basedataSize Size of the module's basedata section in the job memory (Part before the linear memory starts)
  /// @param initialLinMemPages Initial allowed linear memory size in Wasm pages
  virtual void init(uint32_t const basedataSize, uint32_t const initialLinMemPages) = 0;

  ///
  /// @brief Get the start of the WebAssembly basedata
  ///
  /// @return uint8_t* Start of the WebAssembly basedata
  virtual uint8_t *getBasedataStart() const = 0;

  ///
  /// @brief Increase the allowed linear memory size
  ///
  /// This changes the formal limit that the module is permitted to access.
  /// It does not imply that the same amount is already usable.
  ///
  /// @param newTotalLinMemPages New total allowed linear memory size in Wasm pages
  /// @return true The new allowed size was applied successfully
  /// @return false The allowed size could not be increased
  ///
  virtual bool extend(uint32_t const newTotalLinMemPages) = 0;

  ///
  /// @brief Decrease the currently used linear memory size
  ///
  /// This changes the usable size, while keeping at least the given minimum length available.
  ///
  /// @param minimumLength Minimum required linear-memory length in bytes that must remain usable
  /// @return true Shrinking succeeded
  /// @return false Shrinking failed
  ///
  virtual bool shrink(uint32_t const minimumLength) = 0;

  ///
  /// @brief Ensure a linear-memory offset can be safely used
  ///
  /// This operation may increase usable size up to what is required for the given offset,
  /// but never beyond the current allowed size.
  ///
  /// @param linMemOffset Linear-memory offset to validate and make usable if needed
  /// @return ProbeResult::Ok The offset is usable after probing
  /// @return ProbeResult::AllocationFailure The offset is in allowed range but memory could not be made usable
  /// @return ProbeResult::OutOfBounds The offset exceeds the currently allowed linear-memory range
  ///
  virtual ProbeResult probe(uint32_t const linMemOffset) = 0;

  ///
  /// @brief Get the current usable linear-memory size
  ///
  /// @return uint32_t Current usable linear-memory size in bytes
  ///
  virtual uint32_t getLinearMemorySize() const = 0;

  ///
  /// @brief Get the maximal desired RAM when a memory extension failed
  ///
  /// @return uint64_t Maximal requested total RAM in bytes that could not be fulfilled
  ///
  virtual uint64_t getMaxDesiredRamOnMemoryExtendFailed() const VB_NOEXCEPT = 0;
};

} // namespace vb

#endif // IMEMORYMANAGER_HPP
