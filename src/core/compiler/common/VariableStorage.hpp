///
/// @file VariableStorage.hpp
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
#ifndef VARIABLESTORAGE_HPP
#define VARIABLESTORAGE_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

#include "util.hpp"

#include "src/config.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/RegAdapter.hpp"
#include "src/core/compiler/common/MachineType.hpp"

namespace vb {
///
/// @brief Type of the location of this variable
///
enum class StorageType : uint8_t { STACKMEMORY, LINKDATA, REGISTER, CONSTANT, INVALID, STACK_REG };

///
/// @brief Struct describing the storage location of a variable
///
class VariableStorage final {
public:
  StorageType type = StorageType::INVALID;        ///< Type of the location of this variable
  MachineType machineType = MachineType::INVALID; ///< Actual Bit Width of the stored data

  /// @brief Description of the location of this variable (active member is chosen by type)
  /// This will not be used if type is CONSTANT or INVALID
  union Location {
    TReg reg;                    ///<  CPU register this variable is stored in (Index defined by the backend, if type is REGISTER)
    uint32_t stackFramePosition; ///< Offset in the current stack frame (if type is STACKMEMORY)
    uint32_t linkDataOffset;     ///< Offset in the link data in the job memory (if type is LINKDATA)
    ConstUnion constUnion;       ///< Store a const immediate value
  };
  Location location{}; ///< Actual location of this variable

  ///
  /// @brief Checks whether this VariableStorage is equal to another one
  ///
  /// @param other VariableStorage to compare this one to
  /// @return bool Whether the VariableStorages represent the same storage location
  inline bool equals(VariableStorage const &other) const VB_NOEXCEPT {
    if (((type != other.type) || (machineType != other.machineType))) {
      return false;
    }

    static_assert(sizeof(location) == 8U, "Wrong size for location");
    if (type == StorageType::CONSTANT) {
      static_assert(sizeof(location.constUnion) == 8U, "Wrong size for union");
      size_t const actualConstantWidth{MachineTypeUtil::getSize(machineType)};
      // coverity[autosar_cpp14_a12_0_2_violation]
      return std::memcmp(&location.constUnion, &other.location.constUnion, actualConstantWidth) == 0;
    } else {
      // coverity[autosar_cpp14_a12_0_2_violation]
      return std::memcmp(&location, &other.location, 4U) == 0;
    }

    //  (std::memcmp(&location, &other.location, static_cast<size_t>(sizeof(location))) == 0);
  }

  /// @brief check whether this VariableStorage is in the same location as another one
  /// @note the key difference of this function to equals is that this function does not check the WasmType
  inline bool inSameLocation(VariableStorage const &other) const VB_NOEXCEPT {
    if (type != other.type) {
      return false;
    }

    if (type == StorageType::CONSTANT) {
      static_assert(sizeof(location.constUnion) == 8U, "Wrong size for union");
      size_t const actualConstantWidth{MachineTypeUtil::getSize(machineType)};
      // coverity[autosar_cpp14_a12_0_2_violation]
      return std::memcmp(&location.constUnion, &other.location.constUnion, actualConstantWidth) == 0;
    } else if (type == StorageType::REGISTER) {
      return location.reg == other.location.reg;
    } else if (type == StorageType::STACKMEMORY) {
      return location.stackFramePosition == other.location.stackFramePosition;
    } else if (type == StorageType::LINKDATA) {
      return location.linkDataOffset == other.location.linkDataOffset;
    } else {
      return false;
    }
  }

  ///
  /// @brief Generator function for an i32 CONSTANT VariableStorage
  ///
  /// @param value Value of this constant
  /// @return VariableStorage Generated CONSTANT VariableStorage
  static inline constexpr VariableStorage i32Const(uint32_t const value) VB_NOEXCEPT {
    VariableStorage res{};
    res.type = StorageType::CONSTANT;
    res.machineType = MachineType::I32;
    res.location.constUnion.u32 = value;
    return res;
  }

  ///
  /// @brief Generator function for an i64 CONSTANT VariableStorage
  ///
  /// @param value Value of this constant
  /// @return VariableStorage Generated CONSTANT VariableStorage
  static inline constexpr VariableStorage i64Const(uint64_t const value) VB_NOEXCEPT {
    VariableStorage res{};
    res.type = StorageType::CONSTANT;
    res.machineType = MachineType::I64;
    res.location.constUnion.u64 = value;
    return res;
  }

  ///
  /// @brief Generator function for an f32 CONSTANT VariableStorage
  ///
  /// @param value Value of this constant
  /// @return VariableStorage Generated CONSTANT VariableStorage
  static inline constexpr VariableStorage f32Const(float const value) VB_NOEXCEPT {
    VariableStorage res{};
    res.type = StorageType::CONSTANT;
    res.machineType = MachineType::F32;
    res.location.constUnion.f32 = value;
    return res;
  }

  ///
  /// @brief Generator function for an f64 CONSTANT VariableStorage
  ///
  /// @param value Value of this constant
  /// @return VariableStorage Generated CONSTANT VariableStorage
  static inline constexpr VariableStorage f64Const(double const value) VB_NOEXCEPT {
    VariableStorage res{};
    res.type = StorageType::CONSTANT;
    res.machineType = MachineType::F64;
    res.location.constUnion.f64 = value;
    return res;
  }

  ///
  /// @brief Generator function for a register VariableStorage
  ///
  /// @param machineType MachineType that is referenced
  /// @param reg CPU register where this temporary variable is stored (Index defined by the backend)
  /// @return VariableStorage Generated register VariableStorage
  static inline constexpr VariableStorage reg(MachineType const machineType, TReg const reg) VB_NOEXCEPT {
    return VariableStorage::reg(reg, machineType);
  }

  ///
  /// @brief Generator function for a register VariableStorage
  ///
  /// @param reg CPU register where this temporary variable is stored (Index defined by the backend)
  /// @param machineType MachineType of this variable storage
  /// @return VariableStorage Generated register VariableStorage
  static inline constexpr VariableStorage reg(TReg const reg, MachineType const machineType) VB_NOEXCEPT {
    VariableStorage res{};
    res.type = StorageType::REGISTER;
    res.machineType = machineType;
    res.location.reg = reg;
    return res;
  }

  ///
  /// @brief Generator function for a linkData storage
  ///
  /// @param machineType MachineType that is referenced
  /// @param linkDataOffset Offset of the data in linkData
  /// @return VariableStorage Generated linkData VariableStorage
  static inline constexpr VariableStorage linkData(MachineType const machineType, uint32_t const linkDataOffset) VB_NOEXCEPT {
    return VariableStorage::linkData(linkDataOffset, machineType);
  }

  ///
  /// @brief Generator function for a linkData storage
  ///
  /// @param linkDataOffset Offset of the data in linkData
  /// @param machineType MachineType of this variable storage
  /// @return VariableStorage Generated linkData VariableStorage
  static inline constexpr VariableStorage linkData(uint32_t const linkDataOffset, MachineType const machineType) VB_NOEXCEPT {
    VariableStorage res{};
    res.type = StorageType::LINKDATA;
    res.machineType = machineType;
    res.location.linkDataOffset = linkDataOffset;
    return res;
  }

  ///
  /// @brief Generator function for a stack memory storage
  ///
  /// @param machineType MachineType that is referenced
  /// @param stackFramePosition Position of the data in the current stack frame
  /// @return VariableStorage Generated linkData VariableStorage
  static inline constexpr VariableStorage stackMemory(MachineType const machineType, uint32_t const stackFramePosition) VB_NOEXCEPT {
    return VariableStorage::stackMemory(stackFramePosition, machineType);
  }

  ///
  /// @brief Generator function for a stack memory storage
  ///
  /// @param stackFramePosition Position of the data in the current stack frame
  /// @param machineType MachineType of this variable storage
  /// @return VariableStorage Generated linkData VariableStorage
  static inline constexpr VariableStorage stackMemory(uint32_t const stackFramePosition, MachineType const machineType) VB_NOEXCEPT {
    VariableStorage res{};
    res.type = StorageType::STACKMEMORY;
    res.machineType = machineType;
    res.location.stackFramePosition = stackFramePosition;
    return res;
  }
  ///
  /// @brief Generator function for an zero CONSTANT VariableStorage
  ///
  /// @param type Value of this constant
  /// @return VariableStorage Generated zero VariableStorage
  static inline constexpr VariableStorage zero(MachineType const type) VB_NOEXCEPT {
    VariableStorage res{};
    res.type = StorageType::CONSTANT;
    res.machineType = type;
    switch (type) {
    case MachineType::F64:
      res.location.constUnion.f64 = 0.0;
      break;
    case MachineType::F32:
      res.location.constUnion.f32 = 0.0F;
      break;
    case MachineType::I64:
      res.location.constUnion.u64 = 0U;
      break;
    case MachineType::I32:
      res.location.constUnion.u32 = 0U;
      break;
    default:
      UNREACHABLE(break, "invalid zero type")
    }
    return res;
  }

  /// @brief Checks whether this VariableStorage is in memory
  bool inMemory() const VB_NOEXCEPT {
    return (type == StorageType::STACKMEMORY) || (type == StorageType::LINKDATA);
  }
};

} // namespace vb

#endif
