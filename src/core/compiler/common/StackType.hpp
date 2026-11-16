///
/// @file StackType.hpp
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
#ifndef SRC_CORE_COMPILER_COMMON_STACKTYPE_HPP
#define SRC_CORE_COMPILER_COMMON_STACKTYPE_HPP

#include <cassert>
#include <cstdint>
#include <type_traits>

#include "src/config.hpp"

namespace vb {
///
/// @brief Type of a StackElement
///
// coverity[autosar_cpp14_m14_5_3_violation]
class StackType final {
public:
  constexpr inline StackType() VB_NOEXCEPT : StackType(0U) {
  }

  /// @brief Constructs a StackType object with a given raw value.
  ///
  /// @param raw The raw uint32_t value of this stack type.
  constexpr inline explicit StackType(uint32_t const raw) VB_NOEXCEPT : raw_(raw) {
  }

  ///
  /// @brief Assignment operator to assign a StackType or uint32_t to this StackType
  /// @tparam RHS The type of the right-hand side operand, which can be either StackType or uint32_t.
  /// @param rhs Right hand side
  /// @return StackType& Reference to this StackType
  template <typename RHS> inline constexpr StackType &operator=(RHS const rhs) & VB_NOEXCEPT {
    static_assert(std::is_same<RHS, StackType>::value || std::is_same<RHS, uint32_t>::value, "RHS must be either StackType or uint32_t");
    raw_ = static_cast<uint32_t>(rhs);
    return *this;
  }

  static constexpr uint32_t INVALID{0U};     ///< Invalid StackElement, not representing any actual operand
  static constexpr uint32_t SANULL{INVALID}; ///< StackElement with undefined Type

  static constexpr uint32_t SCRATCHREGISTER{1U}; ///< StackElement representing a variable in a scratch register
  static constexpr uint32_t TEMP_RESULT{2U};     ///< StackElement representing a calculation result

  static constexpr uint32_t CONSTANT{5U}; ///< StackElement representing a constant

  static constexpr uint32_t LOCAL{6U};  ///< StackElement representing a local variable (Can be on stack or in a register, actual location defined in
                                        ///< the corresponding LocalDef)
  static constexpr uint32_t GLOBAL{7U}; ///< StackElement representing a global variable (Actual location defined in the corresponding GlobalDef)

  static constexpr uint32_t DEFERREDACTION{8U}; ///< StackElement representing a deferred action, i.e. an arithmetic instruction, conversion etc. that
                                                ///< has not been emitted yet
  static constexpr uint32_t BLOCK{9U};          ///< StackElement representing the opening of a structural block
  static constexpr uint32_t LOOP{10U};          ///< StackElement representing the opening of a structural loop
  static constexpr uint32_t IFBLOCK{11U};       ///< StackElement representing a synthetic block that is inserted to properly realize branches for IF
  ///< statements

  static constexpr uint32_t SKIP{12U}; ///< StackElements that will be skipped when traversing; inserted when iteratively condensing valent blocks

  // Flags for scratchRegister, temp result and constant

  static constexpr uint32_t TVOID{0b0000'0000U};                        ///< void
  static constexpr uint32_t I32{0b0001'0000U};                          ///< int32
  static constexpr uint32_t SCRATCHREGISTER_I32{SCRATCHREGISTER | I32}; ///< int32 in scratch register
  static constexpr uint32_t CONSTANT_I32{CONSTANT | I32};               ///< int32 const
  static constexpr uint32_t TEMP_RESULT_I32{TEMP_RESULT | I32};         ///< int32 result in local

  static constexpr uint32_t I64{0b0010'0000U};                          ///< int64
  static constexpr uint32_t SCRATCHREGISTER_I64{SCRATCHREGISTER | I64}; ///< int64 in scratch register
  static constexpr uint32_t CONSTANT_I64{CONSTANT | I64};               ///< int64 const
  static constexpr uint32_t TEMP_RESULT_I64{TEMP_RESULT | I64};         ///< int64 result in local

  static constexpr uint32_t F32{0b0100'0000U};                          ///< float32
  static constexpr uint32_t SCRATCHREGISTER_F32{SCRATCHREGISTER | F32}; ///< float32 in scratch register
  static constexpr uint32_t CONSTANT_F32{CONSTANT | F32};               ///< float32 const
  static constexpr uint32_t TEMP_RESULT_F32{TEMP_RESULT | F32};         ///< float32 result in local

  static constexpr uint32_t F64{0b1000'0000U};                          ///< float64
  static constexpr uint32_t SCRATCHREGISTER_F64{SCRATCHREGISTER | F64}; ///< float64 in scratch register
  static constexpr uint32_t CONSTANT_F64{CONSTANT | F64};               ///< float64 const
  static constexpr uint32_t TEMP_RESULT_F64{TEMP_RESULT | F64};         ///< float64 result in local
  static constexpr uint32_t BASEMASK{0b0000'1111U};                     ///< mask of base type
  static constexpr uint32_t TYPEMASK{0b1111'0000U};                     ///< mask of type(i32/i64/f32/f64)

  ///
  /// @brief Binary AND operator to quickly compare StackTypes
  ///
  /// @tparam RHS Type of the right-hand side operand, must be either StackType or uint32_t
  /// @param rhs Right hand side operand
  /// @return StackType Resulting, combined stack type
  template <typename RHS> inline constexpr StackType operator&(RHS const rhs) const VB_NOEXCEPT {
    return StackType{this->raw_ & static_cast<uint32_t>(rhs)};
  }

  ///
  /// @brief Binary OR operator to quickly combine StackTypes
  ///
  /// @tparam RHS Type of the right-hand side operand, must be either StackType or uint32_t
  /// @param rhs Right hand side operand
  /// @return StackType Resulting, combined stack type
  template <typename RHS> inline constexpr StackType operator|(RHS const rhs) const VB_NOEXCEPT {
    static_assert(std::is_same<RHS, StackType>::value || std::is_same<RHS, uint32_t>::value, "RHS must be either StackType or uint32_t");
    return StackType{this->raw_ | static_cast<uint32_t>(rhs)};
  }

  ///
  /// @brief Equality operator to compare two StackTypes
  ///
  /// @tparam RHS Type of the right-hand side operand, must be either StackType or uint32_t
  /// @param rhs Right hand side operand
  /// @return bool True if both StackTypes are equal, false otherwise
  // coverity[autosar_cpp14_a13_5_5_violation]
  template <typename RHS> inline constexpr bool operator==(RHS const rhs) const VB_NOEXCEPT {
    static_assert(std::is_same<RHS, StackType>::value || std::is_same<RHS, uint32_t>::value, "RHS must be either StackType or uint32_t");
    return this->raw_ == static_cast<uint32_t>(rhs);
  }

  ///
  /// @brief Less than or equal operator to compare two StackTypes
  ///
  /// @tparam RHS Type of the right-hand side operand, must be either StackType or uint32_t
  /// @param rhs Right hand side operand
  /// @return bool True if raw_ is less than or equal to rhs, false otherwise
  // coverity[autosar_cpp14_a13_5_5_violation]
  template <typename RHS> inline constexpr bool operator<=(RHS const rhs) const VB_NOEXCEPT {
    static_assert(std::is_same<RHS, StackType>::value || std::is_same<RHS, uint32_t>::value, "RHS must be either StackType or uint32_t");
    return this->raw_ <= static_cast<uint32_t>(rhs);
  }

  ///
  /// @brief Inequality operator to compare two StackTypes
  ///
  /// @tparam RHS Type of the right-hand side operand, must be either StackType or uint32_t
  /// @param rhs Right hand side operand
  /// @return bool True if both StackTypes are not equal, false otherwise
  // coverity[autosar_cpp14_a13_5_5_violation]
  template <typename RHS> inline constexpr bool operator!=(RHS const rhs) const VB_NOEXCEPT {
    static_assert(std::is_same<RHS, StackType>::value || std::is_same<RHS, uint32_t>::value, "RHS must be either StackType or uint32_t");
    return this->raw_ != static_cast<uint32_t>(rhs);
  }

  ///
  /// @brief Implicit conversion operator to uint32_t
  ///
  /// @return uint32_t The raw value of the StackType
  explicit inline constexpr operator uint32_t() const VB_NOEXCEPT {
    return raw_;
  }

private:
  uint32_t raw_; ///< storage of the type value
};
} // namespace vb

#endif // SRC_CORE_COMPILER_COMMON_STACKTYPE_HPP
