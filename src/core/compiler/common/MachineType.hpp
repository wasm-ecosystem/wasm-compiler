///
/// @file MachineType.hpp
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
#ifndef MACHINETYPE_HPP
#define MACHINETYPE_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

#include "StackType.hpp"

#include "src/config.hpp"
#include "src/core/common/SignatureType.hpp"
#include "src/core/common/WasmType.hpp"
#include "src/core/common/util.hpp"

namespace vb {

///
/// @brief Machine Type of of this variable
///
enum class MachineType : uint8_t { INVALID, I32, I64, F32, F64 };

namespace MachineTypeUtil {

/// @brief get size of machine type
inline constexpr uint32_t getSize(MachineType const machineType) VB_NOEXCEPT {
  switch (machineType) {
  case MachineType::I32:
  case MachineType::F32:
    return 4U;
  case MachineType::I64:
  case MachineType::F64:
    return 8U;
  default:
    return 0U; // INVALID or other unsupported machineTypes
  }
}

/// @brief Converts a WasmType to its MachineType
inline constexpr MachineType from(WasmType const wasmType) VB_NOEXCEPT {
  switch (wasmType) {
  case WasmType::I32:
    return MachineType::I32;
  case WasmType::I64:
    return MachineType::I64;
  case WasmType::F32:
    return MachineType::F32;
  case WasmType::F64:
    return MachineType::F64;
  default:
    UNREACHABLE(return MachineType::INVALID, "Invalid or unsupported WasmType");
  }
}

/// @brief Converts a MachineType to WasmType
/// @note normally we should not convert it back, since wasm type should only be used as input / validation
inline constexpr WasmType to(MachineType const machineType) VB_NOEXCEPT {
  switch (machineType) {
  case MachineType::I32:
    return WasmType::I32;
  case MachineType::I64:
    return WasmType::I64;
  case MachineType::F32:
    return WasmType::F32;
  case MachineType::F64:
    return WasmType::F64;
  default:
    UNREACHABLE(return WasmType::INVALID, "Invalid or unsupported MachineType");
  }
}

/// @brief Converts a MachineType to StackType
inline StackType toStackTypeFlag(MachineType const machineType) VB_NOEXCEPT {
  switch (machineType) {
  case MachineType::I32:
    return StackType{StackType::I32};
  case MachineType::I64:
    return StackType{StackType::I64};
  case MachineType::F32:
    return StackType{StackType::F32};
  case MachineType::F64:
    return StackType{StackType::F64};
  default:
    UNREACHABLE(return StackType{StackType::INVALID}, "Invalid or unsupported MachineType");
  }
}

///
/// @brief Convert a StackType to the corresponding MachineType
///
/// @param stackType StackType to convert to its corresponding MachineType
/// @return Resulting MachineType
inline MachineType fromStackTypeFlag(StackType const stackType) VB_NOEXCEPT {
  uint32_t const rawType{static_cast<uint32_t>(stackType & StackType::TYPEMASK) >> 4_U32};
  if (rawType == 0_U32) {
    return MachineType::INVALID;
  }
  uint32_t const log2{log2I32(rawType)};
  switch (log2) {
  case 0U:
    return MachineType::I32;
  case 1U:
    return MachineType::I64;
  case 2U:
    return MachineType::F32;
  case 3U:
    return MachineType::F64;
  default:
    UNREACHABLE(return MachineType::INVALID, "Invalid or unsupported StackType");
  }
}

///
/// @brief Converts a SignatureType to its corresponding MachineType. Undefined for invalid SignatureTypes,
/// SignatureType::PARAMSTART, SignatureType::PARAMEND and SignatureType::FORWARD
///
/// @param signatureType SignatureType to convert
/// @return MachineType Corresponding MachineType
inline MachineType fromSignatureType(SignatureType const signatureType) VB_NOEXCEPT {
  switch (signatureType) {
  case SignatureType::I32:
    return MachineType::I32;
  case SignatureType::I64:
    return MachineType::I64;
  case SignatureType::F32:
    return MachineType::F32;
  case SignatureType::F64:
    return MachineType::F64;
  case SignatureType::PARAMSTART:
  case SignatureType::PARAMEND:
  case SignatureType::FORWARD:
  default:
    // GCOVR_EXCL_START
    UNREACHABLE(return MachineType{}, "SignatureType cannot be converted to MachineType");
    // GCOVR_EXCL_STOP
  }
}

/// @brief is integer type
inline constexpr bool isInt(MachineType const machineType) VB_NOEXCEPT {
  return (machineType == MachineType::I32) || (machineType == MachineType::I64);
}

/// @brief is 64bit type
inline constexpr bool is64(MachineType const machineType) VB_NOEXCEPT {
  return (machineType == MachineType::I64) || (machineType == MachineType::F64);
}

} // namespace MachineTypeUtil

} // namespace vb

#endif
