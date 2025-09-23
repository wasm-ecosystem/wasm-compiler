///
/// @file SignatureType.hpp
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
#ifndef SIGNATURETYPE_HPP
#define SIGNATURETYPE_HPP

#include <cassert>
#include <cstdint>

#include "WasmType.hpp"
#include "util.hpp"

#include "src/core/common/util.hpp"

namespace vb {

///
/// @brief Signature types that can be used in a string describing function signatures like (iI)f.
///
/// I and F represent 64-bit integers and floats whereas i and f represent their 32-bit variant.
/// A function signature always starts with a parenthesis (SignatureType::PARAMSTART), followed by a list of parameters
/// in the correct order, then by a closing parenthesis (SignatureType::PARAMEND) followed by a type denoting the return
/// type (iI)f corresponds to a function taking a 32-bit integer and a 64-bit integer as arguments and returning a
/// 32-bit float
///
/// SignatureType::FORWARD is only used for internal bookkeeping purposes
enum class SignatureType : uint8_t {
  I32 = 'i',
  I64 = 'I',
  F32 = 'f',
  F64 = 'F',
  PARAMSTART = '(',
  PARAMEND = ')',
  FORWARD = '>' // Do not use in signatures passed to the runtime
};

namespace WasmTypeUtil {

///
/// @brief Converts a WasmType to its corresponding SignatureType. Undefined for invalid WasmTypes or WasmType::TVOID
///
/// @param wasmType WasmType to convert
/// @return SignatureType Corresponding SignatureType
inline SignatureType toSignatureType(WasmType const wasmType) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto correspondingSignatureType = make_array(SignatureType::I32, SignatureType::I64, SignatureType::F32, SignatureType::F64);
  assert(validateWasmType(wasmType, false) && "Invalid WasmType");
  // coverity[autosar_cpp14_a5_2_5_violation]
  return correspondingSignatureType[toIndexFlag(wasmType)]; // NOLINT(clang-analyzer-core.uninitialized.UndefReturn)
}

} // namespace WasmTypeUtil

///
/// @brief Used for validating whether a given SignatureType corresponds to a C++ type
///
/// @tparam Type C++ type to compare it to
template <class Type> class ValidateSignatureType final {
  static_assert(std::is_enum<Type>::value || std::is_arithmetic<Type>::value, "Template parameter Type must be an enum class or a numeric type");

public:
  ///
  /// @brief Validates whether a given SignatureType corresponds to a C++ type
  ///
  /// @param signatureType SignatureType to validate
  ///
  /// @return bool Whether the types correspond to each other
  static bool validate(SignatureType const signatureType) VB_NOEXCEPT {
    return validateImpl(signatureType, std::integral_constant<bool, std::is_enum<Type>::value>{});
  }

  ///
  /// @brief Validates whether a given SignatureType corresponds to a C++ type
  ///
  /// SignatureType::I32 corresponds to any integer with size 4 (irrespective of signedness)
  /// SignatureType::I64 corresponds to any integer with size 8 (irrespective of signedness)
  /// SignatureType::F32 and SignatureType::F64 correspond to float and double, respectively
  ///
  /// @param signatureType SignatureType to validate
  ///
  /// @return bool Whether the types correspond to each other
  static bool validateImpl(SignatureType const signatureType, std::integral_constant<bool, false> /*unused*/) VB_NOEXCEPT {
    return ((signatureType == SignatureType::I32) && (std::is_integral<Type>::value && (sizeof(Type) == 4))) ||
           ((signatureType == SignatureType::I64) && (std::is_integral<Type>::value && (sizeof(Type) == 8))) ||
           ((signatureType == SignatureType::F32) && std::is_same<Type, float>::value) ||
           ((signatureType == SignatureType::F64) && std::is_same<Type, double>::value);
  }

  ///
  /// @brief Validates whether a given SignatureType corresponds to a C++ type
  ///
  /// SignatureType::I32 corresponds to any integer or enum with underlying type of size 4 (irrespective of signedness)
  /// SignatureType::I64 corresponds to any integer or enum with underlying type of size 8 (irrespective of signedness)
  /// SignatureType::F32 and SignatureType::F64 correspond to float and double, respectively
  ///
  /// @param signatureType SignatureType to validate
  ///
  /// @return bool Whether the types correspond to each other
  static bool validateImpl(SignatureType const signatureType, std::integral_constant<bool, true> /*unused*/) VB_NOEXCEPT {
    using UnderlyingType = typename std::underlying_type<Type>::type;
    return ValidateSignatureType<UnderlyingType>::validate(signatureType);
  }
};

} // namespace vb

#endif
