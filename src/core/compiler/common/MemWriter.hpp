///
/// @file MemWriter.hpp
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
#ifndef MEMWRITER_HPP
#define MEMWRITER_HPP

#include <cstdint>
#include <utility>

#include "ManagedBinary.hpp"

#include "src/config.hpp"
#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/common/util.hpp"

namespace vb {

///
/// @brief Utility class that can be used to manage and conveniently write machine code or serialized data to an
/// ExtendableMemory
///
class MemWriter final {
public:
  ///
  /// @brief Move constructor
  ///
  /// @param memory ExtendableMemory that should be moved into the MemWriter
  inline explicit MemWriter(ExtendableMemory &&memory) VB_NOEXCEPT {
    memory_ = std::move(memory);
  }

  ///
  /// @brief Construct MemWriter with inactive/empty ExtendableMemory
  ///
  inline MemWriter() VB_NOEXCEPT : MemWriter(ExtendableMemory()) {
  }

  ///
  /// @brief Resize the MemWriter active size
  ///
  /// Will fill new bytes with undefined data
  ///
  /// @param size New active size
  /// @throws vb::RuntimeError If size is increased and reallocation failed
  void resize(uint32_t const size);

  ///
  /// @brief Adjust the end pointer of the MemWriter (resize) so it's properly aligned to store a given datatype or an
  /// array thereof
  ///
  /// New length is always greater or equal than original length. This will never delete any data.
  ///
  /// @tparam ObjectType Datatype which should be able to be stored at the new end
  /// @return uint32_t New length after resize
  /// @throws vb::RuntimeError If not enough memory is available
  // coverity[autosar_cpp14_a15_4_4_violation]
  template <class ObjectType> uint32_t alignForType() {
    return alignTop(static_cast<uint32_t>(alignof(ObjectType)));
  }

  ///
  /// @brief Get the end pointer of the MemWriter (pointer to the end of the active portion of the underlying
  /// ExtendableMemory)
  ///
  /// @return uint8_t* End pointer of the MemWriter
  inline uint8_t *ptr() VB_NOEXCEPT {
    return posToPtr(size_);
  }

  ///
  /// @brief Convert a position in the allocated memory to a pointer
  ///
  /// @param position Position in the memory
  /// @return Resulting pointer
  inline uint8_t *posToPtr(uint32_t const position) const VB_NOEXCEPT {
    return pAddI(base(), position);
  }

  ///
  /// @brief Get a pointer to the start of the underlying ExtendableMemory
  ///
  /// @return uint8_t* Pointer to the start of the underlying data
  inline uint8_t *base() const VB_NOEXCEPT {
    return memory_.data();
  }

  ///
  /// @brief Get the active size/length of the MemWriter
  ///
  /// @return uint32_t Active size of the memory
  inline uint32_t size() const VB_NOEXCEPT {
    return size_;
  }

  ///
  /// @brief Get the size of the underlying allocation
  ///
  /// @return uint32_t Size of the underlying allocation (the ExtendableMemory)
  inline uint32_t capacity() const VB_NOEXCEPT {
    return memory_.size();
  }

  ///
  /// @brief Add (resize) the active memory by the given number of bytes
  ///
  /// Will insert undefined data into the new section, will not change existing data
  ///
  /// @param bytes Number of bytes to add to the buffer
  /// @throws std::range_error If not enough memory is available
  void step(uint32_t const bytes);

  ///
  /// @brief Resize the underlying allocation, will not change the active size
  ///
  /// @param bytes Number of bytes to add (at least) to the underlying allocation
  /// @throws std::range_error If not enough memory is available
  inline void reserve(uint32_t const bytes) {
    step(bytes);
    resize(size() - bytes);
  }

  ///
  /// @brief Change the active size to zero, flushes all data
  ///
  // coverity[autosar_cpp14_a15_5_3_violation]
  // coverity[autosar_cpp14_m15_3_4_violation]
  inline void flush() VB_NOEXCEPT {
    // coverity[autosar_cpp14_a15_4_2_violation] resize with 0 won't lead to memory extend fail
    resize(0U);
  }

  ///
  /// @brief Writes the given value in its full length to the end of the memory and increments the end pointer
  /// accordingly. This overload is for types that are cheap to copy, taking them by value.
  ///
  /// @tparam T Type of data to be written
  /// @param source Data which should be written
  /// @throws vb::RuntimeError If not enough memory is available
  template <class T> inline typename std::enable_if<(sizeof(T) <= sizeof(uintptr_t))>::type write(T const source) VB_THROW {
    static_assert(std::is_trivially_copyable<T>::value, "write requires the source type to be copyable");
    step(sizeof(T));
    static_cast<void>(std::memcpy(pSubI(ptr(), sizeof(T)), &source, sizeof(T)));
  }

  ///
  /// @brief Writes the given value in its full length to the end of the memory and increments the end pointer
  /// accordingly. This overload is for types that are expensive to copy, taking them by const reference.
  ///
  /// @tparam T Type of data to be written
  /// @param source Data which should be written
  /// @throws vb::RuntimeError If not enough memory is available
  template <class T> inline typename std::enable_if<!(sizeof(T) <= sizeof(uintptr_t))>::type write(T const &source) VB_THROW {
    static_assert(std::is_trivially_copyable<T>::value, "write requires the source type to be copyable");
    step(sizeof(T));
    static_cast<void>(std::memcpy(pSubI(ptr(), sizeof(T)), &source, sizeof(T)));
  }

  ///
  /// @brief Write a single byte to the end of the memory and add 1 to the active size
  ///
  /// @param data Data to write to the memory
  /// @throws std::range_error If not enough memory is available
  void writeByte(uint8_t const data);

  ///
  /// @brief Write the given number of bytes (the less significant ones) of data in little endian order to the end of
  /// the memory and add numBytes to the active size
  ///
  /// @param data Data to write in little endian order to the memory
  /// @param numBytes Number of bytes of data to write to the memory
  /// @throws std::range_error If not enough memory is available
  void writeBytesLE(uint64_t const data, uint8_t const numBytes);

  ///
  /// @brief convert the MemoryWriter to ManagedBinary
  ///
  /// @return ManagedBinary base on current MemoryWriter
  /// @note then the MemoryWriter becomes invalid
  inline ManagedBinary toManagedBinary() VB_NOEXCEPT {
    ManagedBinary outputBinary{std::move(memory_), size_};
    return outputBinary;
  }

private:
  uint32_t size_ = 0U;      ///< Current active size of the memory
  ExtendableMemory memory_; ///< Instance of the underlying ExtendableMemory

  ///
  /// @brief Align the top/end pointer of the memory to the given number of bytes
  ///
  /// Will never remove any existing data, will only step forward
  ///
  /// @param bytes Number of bytes to align the memory pointer to
  /// @return uint32_t New active length of the memory
  /// @throws vb::RuntimeError If not enough memory is available
  uint32_t alignTop(uint32_t const bytes);

  ///
  /// @brief Request extension of the underlying ExtendableMemory allocation
  ///
  /// @param accessedLength New minimum length the allocation must have
  /// @throws vb::RuntimeError If not enough memory is available
  inline void requestExtension(uint32_t const accessedLength) {
    memory_.resize(accessedLength);
  }
};

} // namespace vb

#endif
