///
/// @file aarch64_memory_addr_imm.hpp
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
#ifndef AARCH64_MEMORY_ADDR_IMM_HPP
#define AARCH64_MEMORY_ADDR_IMM_HPP
#include <cstdint>

#include "src/core/compiler/common/SafeInt.hpp"
#include "src/core/compiler/common/StackElement.hpp"
namespace vb {
namespace aarch64 {
/// @brief Union representing different interpretations of a 12-bit memory address immediate value in AArch64
// coverity[autosar_cpp14_a11_0_1_violation]
union Aarch64MemoryAddrImm final {
  SafeUInt<12U> imm12;    ///< Unsigned immediate value for 12-bit unsigned immediate fields
  SafeUInt<13U> imm12ls1; ///< Unsigned immediate value for 12-bit unsigned immediate fields scaled by 1
  SafeUInt<14U> imm12ls2; ///< Unsigned immediate value for 12-bit unsigned immediate fields scaled by 2
  SafeUInt<15U> imm12ls3; ///< Unsigned immediate value for 12-bit unsigned immediate fields scaled by 3
};

/// @brief Types of AArch64 memory address immediate encodings
enum class Aarch64MemoryAddrImmType : uint8_t {
  IMM12,    ///< 12-bit unsigned immediate
  IMM12LS1, ///< 12-bit unsigned immediate scaled by 1
  IMM12LS2, ///< 12-bit unsigned immediate scaled by 2
  IMM12LS3  ///< 12-bit unsigned immediate scaled by 3
};

/// @brief Convert memory object size to Aarch64MemoryAddrImmType, shall only be 1,2,4,8
/// @param memoryObjectSize Size of the memory object in bytes
inline constexpr Aarch64MemoryAddrImmType memoryObjectSizeToImmType(uint8_t const memoryObjectSize) VB_NOEXCEPT {
  switch (memoryObjectSize) {
  case 1U:
    return Aarch64MemoryAddrImmType::IMM12;
  case 2U:
    return Aarch64MemoryAddrImmType::IMM12LS1;
  case 4U:
    return Aarch64MemoryAddrImmType::IMM12LS2;
  case 8U:
    return Aarch64MemoryAddrImmType::IMM12LS3;
    // GCOVR_EXCL_START
  default:
    UNREACHABLE(return Aarch64MemoryAddrImmType::IMM12, "Invalid memory object size");
  }
  // GCOVR_EXCL_STOP
}
/// @brief Checker for AArch64 memory address immediate encoding possibilities
class Aarch64MemoryAddrImmChecker final {
public:
  /// @brief Default constructor
  inline Aarch64MemoryAddrImmChecker() VB_NOEXCEPT : imm_{} {
  }

  /// @brief Get the 12-bit immediate value
  inline SafeUInt<12U> getImm12() const VB_NOEXCEPT {
    return imm_.imm12;
  }

  /// @brief Get the 13-bit immediate value scaled by 1
  inline SafeUInt<13U> getImm12ls1() const VB_NOEXCEPT {
    return imm_.imm12ls1;
  }

  /// @brief Get the 14-bit immediate value scaled by 2
  inline SafeUInt<14U> getImm12ls2() const VB_NOEXCEPT {
    return imm_.imm12ls2;
  }

  /// @brief Get the 15-bit immediate value scaled by 3
  inline SafeUInt<15U> getImm12ls3() const VB_NOEXCEPT {
    return imm_.imm12ls3;
  }

  /// @brief Check if the given address can be encoded as an immediate offset for memory access instructions
  inline bool addressCanBeImmEncoded(Aarch64MemoryAddrImmType const immType, Stack::iterator const addrElem, uint32_t const offset) VB_NOEXCEPT {
    if (addrElem->type != StackType::CONSTANT_I32) {
      return false;
    }

    int64_t const constAddr{static_cast<int64_t>(addrElem->data.constUnion.u32) + static_cast<int64_t>(offset)};

    if (!in_range<int32_t>(constAddr)) {
      return false;
    }

    uint32_t const value{static_cast<uint32_t>(constAddr)};

    switch (immType) {
    case Aarch64MemoryAddrImmType::IMM12: {
      vb::UnsignedInRangeCheck<12UL> const checkResult{UnsignedInRangeCheck<12U>::check(value)};
      if (checkResult.inRange()) {
        imm_.imm12 = checkResult.safeInt();
        return true;
      }
      break;
    }
    case Aarch64MemoryAddrImmType::IMM12LS1: {
      if ((value % 2U) != 0U) {
        return false;
      }
      vb::UnsignedInRangeCheck<13UL> const checkResult{UnsignedInRangeCheck<13U>::check(value)};
      if (checkResult.inRange()) {
        imm_.imm12ls1 = checkResult.safeInt();
        return true;
      }

      break;
    }
    case Aarch64MemoryAddrImmType::IMM12LS2: {
      if ((value % 4U) != 0U) {
        return false;
      }
      vb::UnsignedInRangeCheck<14UL> const checkResult{UnsignedInRangeCheck<14U>::check(value)};
      if (checkResult.inRange()) {
        imm_.imm12ls2 = checkResult.safeInt();
        return true;
      }

      break;
    }
    case Aarch64MemoryAddrImmType::IMM12LS3: {
      if ((value % 8U) != 0U) {
        return false;
      }
      vb::UnsignedInRangeCheck<15UL> const checkResult{UnsignedInRangeCheck<15U>::check(value)};
      if (checkResult.inRange()) {
        imm_.imm12ls3 = checkResult.safeInt();
        return true;
      }
      break;
    }
    default:
      break;
    }
    return false;
  }

private:
  Aarch64MemoryAddrImm imm_; ///< The memory address immediate value
};

} // namespace aarch64
} // namespace vb

#endif
