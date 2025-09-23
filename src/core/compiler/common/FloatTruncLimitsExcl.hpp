///
/// @file FloatTruncLimitsExcl.hpp
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
#ifndef FLOATTRUNCLIMITSEXCL_HPP
#define FLOATTRUNCLIMITSEXCL_HPP

#include <cstdint>

#include "src/core/common/util.hpp"

namespace vb {
/// @brief Limit of float truncate
// coverity[autosar_cpp14_m3_4_1_violation]
class FloatTruncLimitsExcl final {
public:
  static constexpr uint32_t I32_F32_U_MAX{0x4F800000_U32}; ///< Maximum F32 value that can be converted to U32
  static constexpr uint32_t I32_F32_U_MIN{0xBF800000_U32}; ///< Minimum F32 value that can be converted to U32
  static constexpr uint32_t I32_F32_S_MAX{0x4F000000_U32}; ///< Maximum F32 value that can be converted to I32
  static constexpr uint32_t I32_F32_S_MIN{0xCF000001_U32}; ///< Minimum F32 value that can be converted to I32

  static constexpr uint32_t I64_F32_U_MAX{0x5F800000_U32}; ///< Maximum F32 value that can be converted to U64
  static constexpr uint32_t I64_F32_U_MIN{0xBF800000_U32}; ///< Minimum F32 value that can be converted to U64
  static constexpr uint32_t I64_F32_S_MAX{0x5F000000_U32}; ///< Maximum F32 value that can be converted to I64
  static constexpr uint32_t I64_F32_S_MIN{0xDF000001_U32}; ///< Minimum F32 value that can be converted to I64

  static constexpr uint64_t I32_F64_U_MAX{0x41F0000000000000_U64}; ///< Maximum F64 value that can be converted to U32
  static constexpr uint64_t I32_F64_U_MIN{0xBFF0000000000000_U64}; ///< Minimum F64 value that can be converted to U32
  static constexpr uint64_t I32_F64_S_MAX{0x41E0000000000000_U64}; ///< Maximum F64 value that can be converted to I32
  static constexpr uint64_t I32_F64_S_MIN{0xC1E0000000200000_U64}; ///< Minimum F64 value that can be converted to I32

  static constexpr uint64_t I64_F64_U_MAX{0x43F0000000000000_U64}; ///< Maximum F64 value that can be converted to U64
  static constexpr uint64_t I64_F64_U_MIN{0xBFF0000000000000_U64}; ///< Minimum F64 value that can be converted to U64
  static constexpr uint64_t I64_F64_S_MAX{0x43E0000000000000_U64}; ///< Maximum F64 value that can be converted to I64
  static constexpr uint64_t I64_F64_S_MIN{0xC3E0000000000001_U64}; ///< Minimum F64 value that can be converted to I64

  ///
  /// @brief Struct for requested min and max limits
  class RawLimits final {
  public:
    uint64_t min = 0U; ///< Min raw limit
    uint64_t max = 0U; ///< Max raw limit
  };

  ///
  /// @brief Get the min and max raw limits for float conversions
  ///
  /// @param isSigned Whether the target integer type is signed
  /// @param srcIs64 Whether the float source type is 64-bit
  /// @param dstIs64 Whether the target integer type is 64-bit
  /// @return Raw limits
  inline static constexpr RawLimits getRawLimits(bool const isSigned, bool const srcIs64, bool const dstIs64) VB_NOEXCEPT {
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto rawUpperLimits = make_array(
        make_array(make_array(RawLimits{I64_F64_S_MIN, I64_F64_S_MAX} /* dst64 */,
                              RawLimits{static_cast<uint64_t>(I32_F64_S_MIN), static_cast<uint64_t>(I32_F64_S_MAX)} /* dst32 */), // signed, src64
                   make_array(RawLimits{I64_F32_S_MIN, I64_F32_S_MAX} /* dst64 */,
                              RawLimits{static_cast<uint64_t>(I32_F32_S_MIN), static_cast<uint64_t>(I32_F32_S_MAX)} /* dst32 */) // signed, src32
                   ),
        make_array(make_array(RawLimits{I64_F64_U_MIN, I64_F64_U_MAX} /* dst64 */,
                              RawLimits{static_cast<uint64_t>(I32_F64_U_MIN), static_cast<uint64_t>(I32_F64_U_MAX)} /* dst32 */), // unsigned, src64
                   make_array(RawLimits{I64_F32_U_MIN, I64_F32_U_MAX} /* dst64 */,
                              RawLimits{static_cast<uint64_t>(I32_F32_U_MIN), static_cast<uint64_t>(I32_F32_U_MAX)} /* dst32 */) // unsigned, src32
                   ));
    return rawUpperLimits[isSigned ? 0 : 1][srcIs64 ? 0 : 1][dstIs64 ? 0 : 1];
  }
};
} // namespace vb

#endif
