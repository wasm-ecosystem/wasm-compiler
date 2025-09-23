///
/// @file ExecutableMemoryNoMMU.hpp
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
#ifndef EXECUTABLE_MEMORY_NO_MMU_HPP
#define EXECUTABLE_MEMORY_NO_MMU_HPP
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "MemUtils.hpp"

#include "src/core/common/Span.hpp"
#include "src/core/compiler/common/ManagedBinary.hpp"

namespace vb {
///
/// @brief ExecutableMemory on a MCU which doesn't have MMU, such as tricore
///
class ExecutableMemoryNoMMU final {
public:
  ///
  /// @brief Construct a new ExecutableMemoryNoMMU
  ///
  /// @param binary
  ///
  explicit ExecutableMemoryNoMMU(ManagedBinary &binary) : data_(std::move(binary)) {
  }
  ///
  /// @brief Get pointer of executable memory
  ///
  /// @return const uint8_t*
  ///
  inline const uint8_t *data() const VB_NOEXCEPT {
    return data_.data();
  }
  ///
  /// @brief Get size of executable memory
  ///
  /// @return size_t
  ///
  inline size_t size() const VB_NOEXCEPT {
    return data_.size();
  }
  ///
  /// @brief Get a Span of executable memory
  ///
  /// @return Span<uint8_t const>
  ///
  inline Span<uint8_t const> span() const VB_NOEXCEPT {
    return Span<uint8_t const>(data_.data(), data_.size());
  }
  ///
  /// @brief create a ExecutableMemoryNoMMU from ManagedBinary
  ///
  /// @param binary The input ManagedBinary compiled by JIT compiler
  /// @return ExecutableMemoryNoMMU
  ///
  static inline ExecutableMemoryNoMMU make_executable_copy(ManagedBinary &binary) {
    MemUtils::clearInstructionCache(binary.data(), binary.size());
    return ExecutableMemoryNoMMU(binary);
  }

private:
  ManagedBinary data_; ///< Store the ManagedBinary. Due to no memory protection, ManagedBinary can be used as executable memory
};

/// @brief alias of ExecutableMemoryNoMMU
using ExecutableMemory = ExecutableMemoryNoMMU;

} // namespace vb

#endif
