///
/// @file LinearMemoryAllocator.hpp
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
#ifndef LINEAR_MEMORY_ALLOCATOR_HPP
#define LINEAR_MEMORY_ALLOCATOR_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "VirtualMemoryAllocator.hpp"

#include "src/config.hpp"
#include "src/core/common/WasmConstants.hpp"
#include "src/core/common/util.hpp"
#include "src/core/runtime/IMemoryManager.hpp"

namespace vb {
///
/// @brief Allocator of Wasm Linear memory in passive linear memory protection mode
///
class LinearMemoryAllocator : public IMemoryManager {
public:
  ///
  /// @brief Construct a new LinearMemoryAllocator
  ///
  explicit LinearMemoryAllocator() VB_NOEXCEPT;
  LinearMemoryAllocator(LinearMemoryAllocator const &) = delete;
  ///
  /// @brief Move constructor
  ///
  /// @param other
  ///
  LinearMemoryAllocator(LinearMemoryAllocator &&other) VB_NOEXCEPT;
  LinearMemoryAllocator &operator=(LinearMemoryAllocator const &) & = delete;
  ///
  /// @brief Move assignment
  ///
  /// @param other
  /// @return LinearMemoryAllocator&
  ///
  LinearMemoryAllocator &operator=(LinearMemoryAllocator &&other) & VB_NOEXCEPT;
  ///
  /// @brief user-defined no-throw swap function
  ///
  /// @param lhs Left hand side Object
  /// @param rhs Right hand side Object
  static inline void swap(LinearMemoryAllocator &lhs, LinearMemoryAllocator &&rhs) VB_NOEXCEPT {
    if (&lhs != &rhs) {
      lhs.virtualMemoryAllocator_ = std::move(rhs.virtualMemoryAllocator_);
      lhs.basedataStart_ = rhs.basedataStart_;
      lhs.pagedBasedataSize_ = rhs.pagedBasedataSize_;
      lhs.linMemPages_ = rhs.linMemPages_;
      lhs.pagedMemoryLimit_.store(std::move(rhs.pagedMemoryLimit_));
    }
  }

  /// @brief Default destructor
  ~LinearMemoryAllocator() override = default;

  /// @brief see @b IMemoryManager
  /// @throws std::runtime_error allocate initial memory failed
  void init(uint32_t const basedataSize, uint32_t const initialLinMemPages) override;

  /// @brief see @b IMemoryManager
  uint8_t *getBasedataStart() const VB_NOEXCEPT override;

  /// @brief see @b IMemoryManager
  bool extend(uint32_t const newTotalLinMemPages) VB_NOEXCEPT override;

  /// @brief see @b IMemoryManager
  bool shrink(uint32_t const minimumLength) VB_NOEXCEPT override;

  /// @brief see @b IMemoryManager
  ProbeResult probe(uint32_t const linMemOffset) VB_NOEXCEPT override;

  ///
  /// @brief Get the Memory Usage
  ///
  /// @return Memory usage in bytes
  ///
  inline uint64_t getMemoryUsage() const VB_NOEXCEPT {
    return virtualMemoryAllocator_.getCommitedSize();
  }

  /// @brief see @b IMemoryManager
  uint32_t getLinearMemorySize() const VB_NOEXCEPT override;

  /// @brief Get the Memory Limit
  /// @return Memory limit in bytes
  inline uint64_t getMemoryLimit() const VB_NOEXCEPT {
    return pagedMemoryLimit_.load();
  }

  /// @brief Set the Memory Limit
  /// @param memoryLimit new memory limit in bytes
  /// @throws std::runtime_error new memory limit is less than already used memory
  void setMemoryLimit(uint64_t const memoryLimit);

  /// @brief get max desired RAM on memory extend failed
  uint64_t getMaxDesiredRamOnMemoryExtendFailed() const VB_NOEXCEPT override {
    return maxDesiredRamOnMemoryExtendFailed_;
  }

  static constexpr size_t offsetGuardRegionSize{WasmConstants::maxLinearMemoryOffset +
                                                (1_U64 << 16_U64)}; ///< The maximal possible accessed size by Wasm

private:
  ///
  /// @brief Let the virtualMemoryAllocator to commit virtual memory
  ///
  /// @param newPagedSize new total size to commit
  /// @return true commit success
  /// @return false new size is larger than memory limit, commit failed.
  /// @throws std::runtime_error commit memory failed
  ///
  bool commit(size_t const newPagedSize);

  VirtualMemoryAllocator virtualMemoryAllocator_; ///< The virtual memory allocator
  uint8_t *basedataStart_;                        ///< Start of the module basedata in job memory
  size_t pagedBasedataSize_;                      ///< The base data size
  uint32_t linMemPages_;                          ///< The pages of the linear memory
  std::atomic<uint64_t> pagedMemoryLimit_;        ///< The OS page aligned memory limit
  uint64_t maxDesiredRamOnMemoryExtendFailed_;    ///< The maximal desired RAM size when memory extension fails
};

} // namespace vb

#endif
