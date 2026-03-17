///
/// @file ActiveMemoryManager.hpp
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

#ifndef ACTIVEMEMORYMANAGER_HPP
#define ACTIVEMEMORYMANAGER_HPP

#include <cstdint>

#include "src/config.hpp"
#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/runtime/IMemoryManager.hpp"

namespace vb {

///
/// @brief Active-mode IMemoryManager implementation backed by ExtendableMemory
///
/// This manager keeps the formal allowed size (Wasm pages) and the currently usable
/// linear-memory size (bytes) separate. Probing extends usable memory on demand and
/// zero-initializes newly exposed bytes.
class ActiveMemoryManager : public IMemoryManager {
public:
  /// @brief Construct an active memory manager.
  /// @param extensionRequestPtr Optional callback used to grow/shrink underlying memory.
  /// @param ctx Opaque context passed to @p extensionRequestPtr.
  explicit ActiveMemoryManager(ReallocFnc const extensionRequestPtr, void *const ctx) VB_NOEXCEPT;
  /// @brief Copy is disabled because manager owns mutable memory state.
  ActiveMemoryManager(ActiveMemoryManager const &) = delete;
  /// @brief Move constructor.
  ActiveMemoryManager(ActiveMemoryManager &&) VB_NOEXCEPT = default;
  /// @brief Copy assignment is disabled because manager owns mutable memory state.
  ActiveMemoryManager &operator=(ActiveMemoryManager const &) & = delete;
  /// @brief Move assignment.
  ActiveMemoryManager &operator=(ActiveMemoryManager &&) &VB_NOEXCEPT = default;
  /// @brief Destructor.
  ~ActiveMemoryManager() override = default;

  /// @brief Initialize basedata and allowed/usable linear memory state.
  /// @param basedataSize Size of basedata region in bytes.
  /// @param initialLinMemPages Initial allowed linear memory size in Wasm pages.
  void init(uint32_t const basedataSize, uint32_t const initialLinMemPages) override;
  /// @brief Get the current basedata start pointer.
  /// @return Pointer to the first byte of basedata.
  uint8_t *getBasedataStart() const VB_NOEXCEPT override;
  /// @brief Update the allowed linear memory size.
  /// @param newTotalLinMemPages New total allowed size in Wasm pages.
  /// @return true if allowed size was applied.
  bool extend(uint32_t const newTotalLinMemPages) VB_NOEXCEPT override;
  /// @brief Reduce usable linear memory while keeping a minimum length.
  /// @param minimumLength Minimum usable linear-memory length in bytes.
  /// @return true on success.
  bool shrink(uint32_t const minimumLength) VB_NOEXCEPT override;
  /// @brief Probe whether a linear-memory offset is usable.
  /// @param linMemOffset Offset in linear memory.
  /// @return ProbeResult indicating usability/failure reason.
  ProbeResult probe(uint32_t const linMemOffset) VB_NOEXCEPT override;
  /// @brief Get current usable linear-memory size.
  /// @return Usable linear-memory size in bytes.
  uint32_t getLinearMemorySize() const VB_NOEXCEPT override;
  /// @brief Get the maximal desired RAM when a memory extension failed.
  /// @return Maximal requested total RAM in bytes that could not be fulfilled.
  uint64_t getMaxDesiredRamOnMemoryExtendFailed() const VB_NOEXCEPT override;
  /// @brief Get currently allocated backing memory size.
  /// @return Total allocation size in bytes.
  uint32_t getAllocationSize() const VB_NOEXCEPT;
  /// @brief Get address of the basedata-start pointer.
  /// @return Pointer to internal basedata-start pointer.
  uint8_t *const *getBasedataStartPtrPtr() const VB_NOEXCEPT;

private:
  /// @brief Ensure backing allocation can store basedata plus required linear size.
  /// @param requiredLinearBytes Required linear-memory size in bytes.
  /// @return true if backing memory is large enough.
  bool ensureCapacityForLinearSize(uint32_t const requiredLinearBytes) VB_NOEXCEPT;
  /// @brief Make a linear-memory prefix usable and zero-initialize newly exposed bytes.
  /// @param requiredLinearBytes Required usable linear-memory size in bytes.
  /// @return true if requested bytes are usable.
  bool ensureLinearSize(uint32_t const requiredLinearBytes) VB_NOEXCEPT;
  /// @brief Sync cached basedata pointer with current backing memory base.
  void syncBasedataStart() VB_NOEXCEPT;

  /// @brief Backing memory that stores basedata and linear memory.
  ExtendableMemory jobMemory_;
  /// @brief Cached pointer to start of basedata in @ref jobMemory_.
  uint8_t *basedataStart_;
  /// @brief Fixed basedata size in bytes.
  uint32_t basedataSize_;
  /// @brief Allowed linear-memory size in Wasm pages.
  uint32_t allowedLinMemPages_;
  /// @brief Currently usable linear-memory size in bytes.
  uint32_t usableLinMemBytes_;
  /// @brief Maximal requested total RAM in bytes that could not be fulfilled.
  uint64_t maxDesiredRamOnMemoryExtendFailed_;
};

} // namespace vb

#endif
