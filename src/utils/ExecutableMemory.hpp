///
/// @file ExecutableMemory.hpp
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
#ifndef EXECUTABLE_BINARY_REGION_HPP
#define EXECUTABLE_BINARY_REGION_HPP

#include <cstddef>
#include <cstdint>

#include "src/config.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/compiler/common/ManagedBinary.hpp"

namespace vb {
///
/// @brief Allocate and hold executable memory
///
class ExecutableMemory final {
public:
  inline ExecutableMemory() VB_NOEXCEPT : ExecutableMemory(nullptr, 0U, -1) {
  }
  ExecutableMemory(ExecutableMemory const &) = delete;
  ///
  /// @brief Move constructor
  ///
  /// @param other
  ///
  ExecutableMemory(ExecutableMemory &&other) VB_NOEXCEPT;
  ExecutableMemory &operator=(ExecutableMemory const &) & = delete;
  ///
  /// @brief Move assignment
  ///
  /// @param other
  /// @return ExecutableMemory&
  ///
  ExecutableMemory &operator=(ExecutableMemory &&other) & VB_NOEXCEPT;
  ///
  /// @brief user-defined no-throw swap function
  ///
  /// @param lhs Left hand side Object
  /// @param rhs Right hand side Object
  static inline void swap(ExecutableMemory &lhs, ExecutableMemory &&rhs) VB_NOEXCEPT {
    if (&lhs != &rhs) {
      lhs.freeExecutableMemory();
      lhs.data_ = rhs.data_;
      lhs.size_ = rhs.size_;
      lhs.fd_ = rhs.fd_;
      rhs.data_ = nullptr;
      rhs.size_ = 0U;
      rhs.fd_ = -1;
    }
  }
  ///
  /// @brief Destruct and free memory
  ///
  ~ExecutableMemory() VB_NOEXCEPT;
  ///
  /// @brief Get start address of executable memory
  ///
  /// @return uint8_t*
  ///
  inline uint8_t const *data() const VB_NOEXCEPT {
    return data_;
  }
  ///
  /// @brief get size of executable memory
  ///
  /// @return size_t
  ///
  inline size_t size() const VB_NOEXCEPT {
    return size_;
  }
  ///
  /// @brief Get a Span of executable memory
  ///
  /// @return Span<uint8_t const>
  ///
  inline Span<uint8_t const> span() const VB_NOEXCEPT {
    return Span<uint8_t const>(data_, size());
  }
  ///
  /// @brief Create executable copy of a memory region
  ///
  /// @tparam Binary type with data() and size() method
  /// @param binary
  /// @return ExecutableMemory
  /// @throws std::bad_alloc executable memory allocation failed
  ///
  template <typename Binary> static inline ExecutableMemory make_executable_copy(Binary const &binary) VB_THROW {
    return make_executable_copy(binary.data(), binary.size());
  }

  ///
  /// @brief Create executable copy of a memory region
  ///
  /// @param data The start address of memory to copy
  /// @param size The size of memory to copy
  /// @return ExecutableMemory
  /// @throws std::bad_alloc executable memory allocation failed
  ///
  static inline ExecutableMemory make_executable_copy(uint8_t const *const data, size_t const size) {
    return ExecutableMemory(data, size);
  }

private:
  ///
  /// @brief Common constructor for initialization
  ///
  /// @param data start address of executable memory
  /// @param size size of executable memory
  /// @param fd file descriptor
  ///
  inline ExecutableMemory(uint8_t *const data, size_t const size, int32_t const fd) VB_NOEXCEPT : data_(data), size_(size), fd_(fd) {
  }

  ///
  /// @brief Construct a new Executable Memory
  ///
  /// @param data start address of source JIT code
  /// @param size of JIT code
  /// @throws std::bad_alloc executable memory allocation failed
  ///
  ExecutableMemory(uint8_t const *const data, size_t const size);

  ///
  /// @brief initialize the executable memory
  ///
  /// @param data
  ///
  /// s This function will alloc memory -> copy memory -> set executable memory as executable
  /// @throws std::bad_alloc executable memory allocation failed
  ///

  void init(uint8_t const *const data);
  ///
  /// @brief Free executable memory
  ///
  void freeExecutableMemory() const VB_NOEXCEPT;

  uint8_t *data_; ///< The address of executable memory
  size_t size_;   ///< The size of executable
  int32_t fd_;    ///< The file descriptor of the mmap
};

} // namespace vb

#endif
