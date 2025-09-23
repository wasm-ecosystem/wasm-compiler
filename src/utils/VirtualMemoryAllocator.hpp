///
/// @file VirtualMemoryAllocator.hpp
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
#ifndef VIRTUAL_MEMORY_ALLOCATOR
#define VIRTUAL_MEMORY_ALLOCATOR

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "src/config.hpp"

namespace vb {
///
/// @brief Allocator to allocate and commit virtual memory from OS without libc heap
///
///
class VirtualMemoryAllocator final {
public:
  ///
  /// @brief Construct a new VirtualMemoryAllocator
  ///
  VirtualMemoryAllocator() VB_NOEXCEPT;
  ///
  /// @brief Construct a new Virtual Memory Allocator
  ///
  /// @param totalSize The virtual memory size to be reserved from OS
  ///
  explicit VirtualMemoryAllocator(size_t const totalSize) VB_NOEXCEPT;
  VirtualMemoryAllocator(VirtualMemoryAllocator &) = delete;
  ///
  /// @brief Move constructor
  ///
  /// @param other
  ///
  VirtualMemoryAllocator(VirtualMemoryAllocator &&other) VB_NOEXCEPT;
  VirtualMemoryAllocator &operator=(VirtualMemoryAllocator const &) & = delete;
  ///
  /// @brief Move assignment
  ///
  /// @param other
  /// @return VirtualMemoryAllocator&
  ///
  VirtualMemoryAllocator &operator=(VirtualMemoryAllocator &&other) & VB_NOEXCEPT;
  ///
  /// @brief user-defined no-throw swap function
  ///
  /// @param lhs Left hand side Object
  /// @param rhs Right hand side Object
  static inline void swap(VirtualMemoryAllocator &lhs, VirtualMemoryAllocator &&rhs) VB_NOEXCEPT {
    if (&lhs != &rhs) {
      lhs.freeVirtualMemory();
      lhs.data_ = rhs.data_;
      // coverity[autosar_cpp14_a8_4_5_violation]
      lhs.committedSize_ = rhs.committedSize_.load();
      // coverity[autosar_cpp14_a8_4_5_violation]
      lhs.totalSize_ = rhs.totalSize_.load();

      rhs.data_ = nullptr;
      // coverity[autosar_cpp14_a8_4_5_violation]
      rhs.committedSize_ = 0U;
      // coverity[autosar_cpp14_a8_4_5_violation]
      rhs.totalSize_ = 0U;
    }
  }
  ///
  /// @brief Destruct the VirtualMemoryAllocator and free virtual memory
  ///
  ~VirtualMemoryAllocator() VB_NOEXCEPT;
  ///
  /// @brief Get start address of virtual memory
  ///
  /// @return uint8_t*
  ///
  inline uint8_t *data() const VB_NOEXCEPT {
    // coverity[autosar_cpp14_m9_3_1_violation]
    // coverity[autosar_cpp14_a9_3_1_violation]
    return data_;
  }
  ///
  /// @brief Get the commited size
  ///
  /// @return size_t
  ///
  inline size_t getCommitedSize() const VB_NOEXCEPT {
    return committedSize_;
  }
  ///
  /// @brief get the total size
  ///
  /// @return size_t
  ///
  inline size_t getTotalSize() const VB_NOEXCEPT {
    return totalSize_;
  }
  ///
  /// @brief resize the committed memory
  ///
  /// @param size The new size. This size must be aligned to OS page size
  /// @return size_t finally committed size. The size should be equals to given size if commit success
  /// @throws std::runtime_error Virtual memory commit failed
  ///
  size_t resize(size_t const size);
  ///
  /// @brief Up align the input size to OS page and commit virtual memory by the aligned size
  ///
  /// @param recommendSize recommend size/minimal required size
  /// @return size_t The finally committed size
  /// @throws std::runtime_error Virtual memory commit failed
  ///
  size_t roundUpResize(size_t const recommendSize);
  ///
  /// @brief free virtual memory
  ///
  void freeVirtualMemory() const VB_NOEXCEPT;

private:
  uint8_t *data_;                     ///< Start address of virtual memory
  std::atomic<size_t> committedSize_; ///< committed virtual memory size
  std::atomic<size_t> totalSize_;     ///< total virtual memory size
};

} // namespace vb

#endif
