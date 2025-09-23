///
/// @file util.hpp
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
#ifndef COMPILER_UTIL_HPP
#define COMPILER_UTIL_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/config.hpp"
#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"

namespace vb {

constexpr uint32_t UnknownIndex{0xFF'FF'FF'FFU}; ///< Flag for unknown/no index flags

///
/// @brief Represents a constant
///
union ConstUnion {
  uint32_t u32; ///< 32-bit integer
  uint64_t u64; ///< 64-bit integer
  float f32;    ///< 32-bit float
  double f64;   ///< 64-bit float

  ///
  /// @brief Get the raw, reinterpreted value of the float as an integer
  ///
  /// @return uint32_t Raw, reinterpreted value of the float
  inline uint32_t rawF32() const VB_NOEXCEPT {
    return bit_cast<uint32_t>(f32);
  }

  ///
  /// @brief Get the raw, reinterpreted value of the float as an integer
  ///
  /// @return uint64_t Raw, reinterpreted value of the float
  inline uint64_t rawF64() const VB_NOEXCEPT {
    return bit_cast<uint64_t>(f64);
  }
};

///
/// @brief Checks whether the integral input value fits into an integral datatype of given width of same signedness
///
/// @tparam bits_target Number of bits of the target integer
/// @tparam Source Integral input datatype
/// @param source Input data
/// @return bool Whether the input fits into an integer of the given width
template <size_t bits_target, class Source> inline bool in_range(Source const source) VB_NOEXCEPT {
  static_assert(std::is_integral<Source>::value, "in_range only works for integers");

  constexpr size_t bits_source{static_cast<size_t>(sizeof(Source)) * 8U};

  // A smaller (or equally large) datatype always fits into a larger (or equally large) datatype of same signedness
#ifdef _MSC_VER
// coverity[autosar_cpp14_a16_7_1_violation]
#pragma warning(disable : 4127) // disable warning of MSVC
#endif
  if VB_IFCONSTEXPR (bits_target >= bits_source) {
    return true;
  }
#ifdef _MSC_VER
// coverity[autosar_cpp14_a16_7_1_violation]
#pragma warning(default : 4127)
#endif

  using Unsigned_Source = typename std::make_unsigned<Source>::type;

  // Otherwise Source is a larger datatype than Target, so we cast target limits to Source
  if VB_IFCONSTEXPR (std::is_signed<Source>::value) {
    // Confirm the upper bound is satisfied
    constexpr Source maxTargetSigned{static_cast<Source>((static_cast<Unsigned_Source>(1U) << (bits_target - 1U)) - 1U)};
    if (source > maxTargetSigned) {
      return false;
    }

    // If it's signed, we also need to check if the lower bound is satisfied
    constexpr Source minTargetSigned{-maxTargetSigned - 1};
    if (source < minTargetSigned) {
      return false;
    }
  } else {
    // Confirm the upper bound is satisfied
    constexpr Unsigned_Source maxTargetUnsigned{(static_cast<Unsigned_Source>(1U) << bits_target) - 1U};
    if (static_cast<Unsigned_Source>(source) > maxTargetUnsigned) {
      return false;
    }
  }

  return true;
}

///
/// @brief Checks whether the integral input value fits into a given integral datatype of same signedness
///
/// @tparam Target Target integral type, must be of the same signedness as the input data
/// @tparam SourceType Integral input datatype
/// @param data Input data
/// @return bool Whether the input fits into the integral target type
template <class Target, class SourceType> inline bool in_range(SourceType const data) VB_NOEXCEPT {
  static_assert(((std::is_integral<SourceType>::value && std::is_integral<Target>::value) && std::is_signed<SourceType>::value) ==
                    std::is_signed<Target>::value,
                "in_range only works for integers with same signedness");
  return in_range<sizeof(Target) * 8U>(data);
}

} // namespace vb

#endif // UTIL_H
