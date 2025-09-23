///
/// @file WasmType.hpp
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
#ifndef WASMTYPE_HPP
#define WASMTYPE_HPP

#include <cassert>
#include <cstdint>

#include "VbExceptions.hpp"

#include "src/core/common/util.hpp"

namespace vb {

///
/// @brief Type representing the different types available in WebAssembly as their binary encoding plus a
/// WasmType::INVALID encoding used by the compiler for internal purposes
///
enum class WasmType : uint8_t {
  EXTERN_REF = 0x6F,
  FUNC_REF = 0x70,
  VEC_TYPE = 0x7B,
  F64 = 0x7C,
  F32 = 0x7D,
  I64 = 0x7E,
  I32 = 0x7F,
  TVOID = 0x40,
  INVALID = 0x00
};

namespace WasmTypeUtil {

///
/// @brief Calculate an index onto an array for quick selection
///
/// WasmTypes will be converted according to: I32 -> 0, I64 -> 1, F32 -> 2, F64 -> 4
/// Undefined behavior for invalid WasmTypes or WasmType::TVOID
///
/// @param wasmType Type to convert
/// @return uint32_t Corresponding index
inline uint32_t toIndexFlag(WasmType const wasmType) VB_NOEXCEPT {
  assert(static_cast<uint8_t>(wasmType) <= 0x7F && "Cannot convert WasmType to index flag");
  uint32_t const indexFlag{static_cast<uint32_t>(WasmType::I32) - static_cast<uint32_t>(wasmType)};
  assert(indexFlag < 4 && "Index flag out of range");
  return indexFlag;
}

///
/// @brief Validate whether the given WasmType is a valid type in WebAssembly
///
/// @b WasmType::INVALID is, by definition, not valid. Any value not corresponding to the WasmType enum is also not
/// valid.
///
/// @param type Type to validate
/// @param canBeVoid Whether WasmType::TVOID should be valid
/// @return bool Whether the given type is a valid WebAssembly type
inline bool validateWasmType(WasmType const type, bool const canBeVoid = false) {
  if (((type == WasmType::EXTERN_REF) || (type == WasmType::FUNC_REF)) || (type == WasmType::VEC_TYPE)) {
    throw FeatureNotSupportedException(ErrorCode::Reference_type_feature_not_implemented);
  }
  return ((canBeVoid && (type == WasmType::TVOID)) || ((type >= WasmType::F64) && (type <= WasmType::I32)));
}

///
/// @brief Validate whether the given WasmType is a valid reftype in WebAssembly
///
/// @param type Type to validate
/// @return bool Whether the given type is a valid WebAssembly reftype
inline constexpr bool isRefType(WasmType const type) VB_NOEXCEPT {
  return (type == WasmType::EXTERN_REF) || (type == WasmType::FUNC_REF);
}

///
/// @brief Get the number of bytes for the given WasmType
///
/// Undefined behavior for invalid WasmTypes but defined for WasmType::TVOID
///
/// @param wasmType Type for which to calculate the size
/// @return Number of bytes for the given WasmType. WasmType::TVOID returns 0.
inline uint32_t getSize(WasmType const wasmType) VB_NOEXCEPT {
  assert(validateWasmType(wasmType, true) && "Invalid WasmType");
  if (wasmType == WasmType::TVOID) {
    return 0_U32;
  }
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto sizeArr = make_array(4_U32, 8_U32, 4_U32, 8_U32);
  // coverity[autosar_cpp14_a5_2_5_violation]
  return sizeArr[toIndexFlag(wasmType)];
}

///
/// @brief Determine if the Wasm type is a 64-bit type
///
/// @param wasmType Input Wasm type
/// @return Whether the Wasm type is a 64-bit type
///
inline bool is64(WasmType const wasmType) VB_NOEXCEPT {
  return getSize(wasmType) == 8_U32;
}

///
/// @brief Whether the WasmType is an integer type (either WasmType::I32 or WasmType::I64)
///
/// @param wasmType Type to check
/// @return bool Whether type is an integer type
inline bool isInt(WasmType const wasmType) VB_NOEXCEPT {
  assert(validateWasmType(wasmType, false) && "Invalid WasmType");
  return (wasmType == WasmType::I32) || (wasmType == WasmType::I64);
}
} // namespace WasmTypeUtil
} // namespace vb

#endif
