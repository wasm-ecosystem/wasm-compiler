///
/// @file BytecodeReader.cpp
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
#include <cassert>
#include <cstdint>

#include "BytecodeReader.hpp"

#include "src/config.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
namespace vb {

BytecodeReader::BytecodeReader(Span<uint8_t const> const &bytecode) VB_NOEXCEPT : ptr_(bytecode.data()), bytecode_(bytecode) {
}

uint32_t BytecodeReader::readLEU32() {
  uint8_t const *const oldPtr{ptr_};
  step(4U);
  uint32_t const value{(static_cast<uint32_t>(*oldPtr)) | (static_cast<uint32_t>(*pAddI(oldPtr, 1)) << 8U) |
                       (static_cast<uint32_t>(*pAddI(oldPtr, 2)) << 16U) | (static_cast<uint32_t>(*pAddI(oldPtr, 3)) << 24U)};
  return value;
}

uint64_t BytecodeReader::readLEU64() {
  uint8_t const *const oldPtr{ptr_};
  step(8U);
  uint64_t const value{(static_cast<uint64_t>(*oldPtr)) | (static_cast<uint64_t>(*pAddI(oldPtr, 1)) << 8U) |
                       (static_cast<uint64_t>(*pAddI(oldPtr, 2)) << 16U) | (static_cast<uint64_t>(*pAddI(oldPtr, 3)) << 24U) |
                       (static_cast<uint64_t>(*pAddI(oldPtr, 4)) << 32U) | (static_cast<uint64_t>(*pAddI(oldPtr, 5)) << 40U) |
                       (static_cast<uint64_t>(*pAddI(oldPtr, 6)) << 48U) | (static_cast<uint64_t>(*pAddI(oldPtr, 7)) << 56U)};
  return value;
}

void BytecodeReader::jumpTo(uint8_t const *const ptr) {
  if ((ptr < bytecode_.data()) || (pToNum(ptr) > pToNum(pAddI(bytecode_.data(), bytecode_.size())))) {
    throw ValidationException(ErrorCode::Bytecode_out_of_range);
  }
  ptr_ = ptr;
}

uint64_t BytecodeReader::readLEB128(bool const signedInt, uint32_t const maxBits) {
  assert(maxBits <= 64U && "maxBits longer than 64 bits");
  uint64_t result{0U};
  uint32_t bitsWritten{0U};
  uint8_t byte{0xFFU};
  while ((static_cast<uint32_t>(byte) & 0x80U) != 0U) {
    byte = readByte<uint8_t>();
    if (bitsWritten >= maxBits) {
      // One full byte too many -> malformed
      throw ValidationException(ErrorCode::Malformed_LEB128_integer__Out_of_bounds_);
    }
    uint32_t const lowByte{static_cast<uint32_t>(byte) & 0x7F_U32};
    result |= static_cast<uint64_t>(lowByte) << static_cast<uint64_t>(bitsWritten);
    bitsWritten = bitsWritten + 7U;
    if (bitsWritten > maxBits) {
      // More bits written than allowed
      if (signedInt && ((static_cast<uint32_t>(byte) & (1_U32 << (6_U32 - (bitsWritten - maxBits)))) != 0_U32)) {
        // If it is signed and negative (sign bit set) "1" padding allowed
        uint32_t const bitMask{(0xFF_U32 << ((6_U32 - (bitsWritten - maxBits)) + 1_U32)) & 0b01111111_U32};
        if ((static_cast<uint32_t>(byte) & bitMask) != bitMask) {
          throw ValidationException(ErrorCode::Malformed_LEB128_signed_integer__Wrong_padding_);
        }
      } else {
        // Zero padding allowed if unsigned or positive signed integer
        uint32_t const bitMask{(0xFF_U32 << ((6_U32 - (bitsWritten - maxBits)) + 1_U32)) & 0b01111111_U32};
        if ((static_cast<uint32_t>(byte) & bitMask) != 0U) {
          throw ValidationException(ErrorCode::Malformed_LEB128_unsigned_integer__Wrong_padding_);
        }
      }
    }
  }

  // Can also be used if bitsWritten > maxBits because valid padding has already been established and is guaranteed to
  // correspond to the sign bit
  if ((signedInt && ((static_cast<uint32_t>(byte) & 0x40U) != 0U)) && (bitsWritten < 64_U32)) {
    // Sign extend
    uint64_t const signExtensionMask{0xFF'FF'FF'FF'FF'FF'FF'FFLLU << bitsWritten};
    result |= signExtensionMask;
  }
  return result;
}

} // namespace vb
