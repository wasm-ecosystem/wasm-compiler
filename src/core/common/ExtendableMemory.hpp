///
/// @file ExtendableMemory.hpp
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
#ifndef EXTENDABLEMEMORY_HPP
#define EXTENDABLEMEMORY_HPP

#include <cstdint>
#include <utility>

#include "src/config.hpp"

namespace vb {

class Runtime;
class ExtendableMemory;

///
/// @brief Type of a realloc-like function for a lambda that can reallocate an ExtendableMemory and modify it in place
///
/// First argument of the lambda function will be the current ExtendableMemory while the second argument is the minimum
/// memory size needed. After reallocation, the ExtendableMemory's base pointer and length should be changed
/// accordingly. On failure, either a nullptr as base or a length less than the minimum memory size should be stored in
/// the ExtendableMemory.
///
using ReallocFnc = void (*)(ExtendableMemory &, uint32_t, void *ctx);

///
/// @brief A memory region with a base pointer, a size and an optional ReallocFnc that can be called to request
/// enlargement of the region.
///
class ExtendableMemory final {
public:
  ///
  /// @brief Construct a new ExtendableMemory
  ///
  ExtendableMemory() VB_NOEXCEPT;

  ///
  /// @brief Construct a new ExtendableMemory
  ///
  /// @param extensionRequestPtr Optional ReallocFnc (pass nullptr if not extensible)
  /// @param data Pointer to the start of the memory region
  /// @param size Size of the memory region in bytes
  /// @param ctx Context for the ReallocFnc, can be used to pass additional information to the ReallocFnc
  explicit ExtendableMemory(ReallocFnc const extensionRequestPtr, uint8_t *const data = nullptr, uint32_t const size = 0U,
                            void *const ctx = nullptr) VB_NOEXCEPT;
  ExtendableMemory(ExtendableMemory &) = delete;

  ///
  /// @brief Move constructor for ExtendableMemory
  ///
  /// @param other Object to be moved
  ExtendableMemory(ExtendableMemory &&other) VB_NOEXCEPT;
  ExtendableMemory &operator=(const ExtendableMemory &) & = delete;

  ///
  /// @brief user-defined no-throw swap function
  ///
  /// @param lhs Left hand side Object
  /// @param rhs Right hand side Object
  static inline void swap(ExtendableMemory &lhs, ExtendableMemory &&rhs) VB_NOEXCEPT {
    if (&lhs != &rhs) {
      lhs.freeExtendableMemory();
      lhs.data_ = rhs.data_;
      lhs.size_ = rhs.size_;
      lhs.extensionRequestPtr_ = rhs.extensionRequestPtr_;
      lhs.ctx_ = rhs.ctx_;

      rhs.data_ = nullptr;
      rhs.size_ = 0U;
      rhs.extensionRequestPtr_ = nullptr;
      rhs.ctx_ = nullptr;
    }
  }

  ///
  /// @brief Destructor for the ExtendableMemory
  ///
  /// Will call the ReallocFnc (if it is available) with minimumLength zero in order to free the memory
  ///
  ~ExtendableMemory() VB_NOEXCEPT;

  ///
  /// @brief Moves the ExtendableMemory
  ///
  /// @param original ExtendableMemory to move
  /// @return ExtendableMemory& Target reference
  ExtendableMemory &operator=(ExtendableMemory &&original) & VB_NOEXCEPT;

  ///
  /// @brief Set the minimum size needed for the memory region and try to allocate more memory
  ///
  /// Tries to allocate more memory if the current memory is not enough, does nothing otherwise. If no ReallocFnc is
  /// available, the return pointer is nullptr or the new size is smaller than the new size
  ///
  /// @param size minimum requested size of the memory region
  ///
  /// @throws std::range_error If reallocation failed due to one of the reasons outlined above
  void resize(uint32_t const size);

  ///
  /// @brief Get the base of the memory region
  ///
  /// @return uint8_t* Pointer to the start of the memory region
  inline uint8_t *data() const VB_NOEXCEPT {
    return data_;
  }

  ///
  /// @brief Get the length of the memory region
  ///
  /// @return uint32_t Length of the memory region
  inline uint32_t size() const VB_NOEXCEPT {
    return size_;
  }

  ///
  /// @brief Resets the memory object with a new pointer and size
  ///
  /// @param data New base pointer
  /// @param size New size
  inline void reset(uint8_t *const data, uint32_t const size) VB_NOEXCEPT {
    data_ = data;
    size_ = size;
  }

  ///
  /// @brief Call the stored ReallocFnc to extend the memory region
  ///
  /// This function will be called whenever the memory region's size is not enough for whatever should be stored inside
  /// and more memory is needed. This way allocation can be delegated to the integrator, while the actual runtime and
  /// compiler do not allocate any memory themselves. This will unconditionally be called, irrespective of whether the
  /// minimum length is smaller or larger of the current length.
  ///
  /// @param minimumLength Minimum memory size to request
  // coverity[autosar_cpp14_m9_3_3_violation]
  inline void extensionRequest(uint32_t const minimumLength) VB_NOEXCEPT {
    extensionRequestPtr_(*this, minimumLength, ctx_);
  }

  ///
  /// @brief Checks whether the ExtendableMemory has a ReallocFnc that can be used to request extension of the memory
  /// region
  ///
  /// @return Whether the ExtendableMemory has a ReallocFnc (ReallocFnc is not nullptr)
  inline bool hasExtensionRequest() const VB_NOEXCEPT {
    return extensionRequestPtr_ != nullptr;
  }

private:
  ///
  /// @brief Pointer to the start of the memory region
  ///
  uint8_t *data_;

  ///
  /// @brief Size of the referenced memory
  ///
  uint32_t size_;

  ///
  /// @brief Pointer to a function or lambda function that will be called whenever more memory is needed
  ///
  ReallocFnc extensionRequestPtr_;

  void *ctx_; ///< Context for the ReallocFnc, can be used to pass additional information to the ReallocFnc

  ///
  /// @brief Free the memory held by the ExtendableMemory
  ///
  void freeExtendableMemory() VB_NOEXCEPT;

  ///
  /// @brief Runtime is friend so it can get the pointer to the data_ member
  ///
  friend Runtime;
};

} // namespace vb

#endif
