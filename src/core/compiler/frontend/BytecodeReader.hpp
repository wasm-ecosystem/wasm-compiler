///
/// @file BytecodeReader.hpp
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
#ifndef BYTECODEREADER_HPP
#define BYTECODEREADER_HPP

#include <cstdint>

#include "src/core/common/Span.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {

///
/// @brief Class that facilitates reading OPCodes and various numbers from WebAssembly bytecode
///
class BytecodeReader final {
public:
  ///
  /// @brief Construct a BytecodeReader instance from a view onto a readonly bytecode binary
  ///
  /// @param bytecode Span describing the start pointer and length of an input bytecode binary
  explicit BytecodeReader(Span<uint8_t const> const &bytecode) VB_NOEXCEPT;

  ///
  /// @brief Get the current offset within the bytecode binary
  ///
  /// @return size_t Current offset within the bytecode binary
  inline size_t getOffset() const VB_NOEXCEPT {
    static_assert(sizeof(size_t) >= sizeof(ptrdiff_t), "Type mismatch");
    return static_cast<size_t>(pSubAddr(ptr_, bytecode_.data()));
  }

  ///
  /// @brief Get the number of bytes still left in the binary
  ///
  /// @return size_t Number of bytes left in the binary
  inline size_t getBytesLeft() const VB_NOEXCEPT {
    return bytecode_.size() - getOffset();
  }

  ///
  /// @brief Whether there is at least one byte left in the binary, i.e. getBytesLeft is greater than 1
  ///
  /// @return bool Whether there is at least one byte left in the binary
  inline bool hasNextByte() const VB_NOEXCEPT {
    return getBytesLeft() > 0U;
  }

  ///
  /// @brief Read a byte as given datatype from the binary
  ///
  /// @tparam Dest Datatype the single byte data should be read as
  /// @return Dest Single byte data that has been read
  /// @throws ValidationException if the new pointer is out of bounds
  template <class Dest> Dest readByte() VB_THROW {
    static_assert(std::is_trivially_copyable<Dest>::value, "readByte requires the destination type to be copyable");
    static_assert(sizeof(Dest) == 1, "Size of type of readByte needs to be 1");
    Dest dest;
    uint8_t const *const oldPtr{ptr_};
    step(1U);
    static_cast<void>(std::memcpy(&dest, oldPtr, static_cast<size_t>(sizeof(Dest))));
    return dest;
  }

  ///
  /// @brief Read fixed 4 bytes from the binary (host-endian-independently from little endian) into a uint32_t
  ///
  /// @return uint32_t Data that has been read
  /// @throws ValidationException if the new pointer is out of bounds
  uint32_t readLEU32();

  ///
  /// @brief Read fixed 8 bytes from the binary (host-endian-independently from little endian) into a uint64_t
  ///
  /// @return uint64_t Data that has been read
  /// @throws ValidationException if the new pointer is out of bounds
  uint64_t readLEU64();

  ///
  /// @brief Read the next LEB128 encoded variable length integer from the current cursor
  ///
  /// @tparam Type Integer type to read (can be signed, unsigned and of an arbitrary standardized length)
  /// @return Type Data that has been read
  /// @throws ValidationException if the new pointer is out of bounds or the LEB128-encoded integer is malformed
  template <class Type> Type readLEB128() VB_THROW {
    static_assert(std::is_integral<Type>::value, "readLEB128 can only read variable length integers");
    using IntermediateType = typename std::conditional<std::is_signed<Type>::value, int64_t, uint64_t>::type;
    return static_cast<Type>(bit_cast<IntermediateType>(readLEB128(std::is_signed<Type>::value, sizeof(Type) * 8U)));
  }

  ///
  /// @brief Skip the given number of bytes in the binary without reading
  ///
  /// @param count Number of bytes to skip
  /// @throws ValidationException if the new pointer is out of bounds
  inline void step(uint32_t const count) {
    jumpTo(pAddI(ptr_, count));
  }

  ///
  /// @brief Jump to a given pointer pointing to a spot inside the binary
  ///
  /// @param ptr New cursor pointer
  /// @throws ValidationException if the new pointer is out of bounds
  void jumpTo(uint8_t const *const ptr);

  ///
  /// @brief Get the current pointer in the binary the cursor points to
  ///
  /// @return uint8_t const* Current pointer the cursor points to
  inline uint8_t const *getPtr() const VB_NOEXCEPT {
    return ptr_;
  };

private:
  uint8_t const *ptr_;                  ///< Pointer to the current cursor spot of the BytecodeReader
  Span<uint8_t const> const &bytecode_; ///< Underlying bytecode binary to read from

  ///
  /// @brief Read a LEB128 integer from the current cursor of the BytecodeReader
  ///
  /// @param signedInt Whether the integer that should be read is signed
  /// @param maxBits How many bits the integer that should be read has
  /// @return uint64_t Sign-extended data that has been read, maximally the maxBits least significant bits are populated
  /// with actual data, rest is sign-extended
  /// @throws ValidationException if the new pointer is out of bounds or the LEB128-encoded integer is malformed
  uint64_t readLEB128(bool const signedInt, uint32_t const maxBits);
};
} // namespace vb

#endif // WASM_VB_BYTECODEREADER_H
