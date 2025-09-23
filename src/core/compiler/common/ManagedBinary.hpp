///
/// @file ManagedBinary.hpp
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
#ifndef MANAGED_BINARY_HPP
#define MANAGED_BINARY_HPP

#include <cstdint>
#include <utility>

#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/common/Span.hpp"

namespace vb {

///
/// @brief Class that manages deallocation of a ExtendableMemory instance and its underlying actual memory in a RAII
/// style
///
class ManagedBinary final {
public:
  ///
  /// @brief Constructor a new ManagedBinary
  ///
  inline ManagedBinary() VB_NOEXCEPT : ManagedBinary(ExtendableMemory(), 0U){};

  ///
  /// @brief Constructor of active ManagedBinary
  ///
  /// @param extendableMemory ExtendableMemory instance that should be moved into the ManagedBinary
  /// @param length Current active length of the ExtendableMemory (Less than is currently allocated/available could be
  /// actually active/have data in it)
  inline ManagedBinary(ExtendableMemory &&extendableMemory, uint32_t const length) VB_NOEXCEPT : extendableMemory_(std::move(extendableMemory)),
                                                                                                 activeLength_(length) {
  }

  ManagedBinary(ManagedBinary &) = delete;
  ManagedBinary &operator=(const ManagedBinary &) & = delete;

  ///
  /// @brief Move constructor
  ///
  /// @param other ManagedBinary that should be moved from
  inline ManagedBinary(ManagedBinary &&other) VB_NOEXCEPT : extendableMemory_(std::move(other.extendableMemory_)),
                                                            activeLength_(other.activeLength_) {
    other.activeLength_ = 0U;
  }

  ///
  /// @brief Default Move operator
  ///
  /// @param other ManagedBinary that should be moved from
  /// @return ManagedBinary& Reference to the assigned instance
  ManagedBinary &operator=(ManagedBinary &&other) & = default;

  ///
  /// @brief Default destructor for ManagedBinary
  ///
  ~ManagedBinary() VB_NOEXCEPT = default;

  ///
  /// @brief Get a pointer to the start of the underlying data
  ///
  /// @return uint8_t* Pointer to the data
  inline uint8_t *data() const VB_NOEXCEPT {
    return extendableMemory_.data();
  };

  ///
  /// @brief Get the active length of the underlying data
  ///
  /// Not the allocated size of the underlying ExtendableMemory
  ///
  /// @return uint32_t Length of the data
  inline uint32_t size() const VB_NOEXCEPT {
    return activeLength_;
  };

  ///
  /// @brief Get a Span consisting of base pointer and size for the underlying data
  ///
  /// @return Span<uint8_t const> Span/view onto the underlying data
  inline Span<uint8_t const> span() const VB_NOEXCEPT {
    return Span<uint8_t const>(data(), size());
  }

private:
  ExtendableMemory extendableMemory_; ///< Underlying ExtendableMemory object/allocation
  uint32_t activeLength_;             ///< Length of the actually active/written portion of the underlying memory
};

} // namespace vb

#endif
