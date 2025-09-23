///
/// @file SafeInt.hpp
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
#ifndef VB_SAFE_INT_HPP
#define VB_SAFE_INT_HPP

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "src/config.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {

template <size_t> class SafeInt;
template <size_t> class UnsignedInRangeCheck;
template <size_t> class SignedInRangeCheck;

///
/// @brief Wrapper class to check if an unsigned integer is safe to be encoded into instructions.
/// @tparam range The bit range for the SafeUInt.
template <size_t range> class SafeUInt final {
public:
  using ValueType = uint32_t; ///< Type alias for the value type.

  /// @brief Maximum value that can be represented by this SafeUInt
  static constexpr ValueType maxValue{(range < 32U) ? ((static_cast<ValueType>(1U) << (static_cast<ValueType>(range))) - 1U) : UINT32_MAX};
  static_assert(range > 0U, "range too small");
  static_assert(range <= (sizeof(ValueType) * static_cast<size_t>(8U)),
                "Safe int current can't handle more than 32 bit, if you see this error, please refactor this class to "
                "increase the limit");

  ///
  /// @brief Get the value of the SafeUInt.
  /// @return The value of the SafeUInt.
  ///
  inline ValueType value() const VB_NOEXCEPT {
    return value_;
  }

  ///
  /// @brief Create a SafeUInt from a constant value.
  ///
  /// @tparam constValue The constant value
  /// @return A SafeUIn
  ///
  // coverity[autosar_cpp14_m3_2_2_violation]
  template <ValueType constValue> static constexpr SafeUInt<range> fromConst() VB_NOEXCEPT {
    static_assert(constValue <= maxValue, "const value out of range");
    SafeUInt<range> safeInt{};
    safeInt.value_ = constValue;
    return safeInt;
  }
  /// @brief Create a SafeUInt from an unsafe value.
  /// @param val The unsafe value.
  /// @return A SafeUInt object.
  static SafeUInt<range> fromUnsafe(uint32_t const val) VB_NOEXCEPT {
    SafeUInt<range> safeInt{};
    safeInt.value_ = val;
    return safeInt;
  }

  /// @brief Get the maximum value of the SafeUInt.
  /// @return A SafeUInt object with the maximum value.
  static SafeUInt<range> max() VB_NOEXCEPT {
    SafeUInt<range> safeInt{};
    safeInt.value_ = maxValue;
    return safeInt;
  }

  ///
  /// @brief Create a SafeUint base on the size of Source
  /// @tparam Source source type
  /// @param val input
  /// @return SafeUInt which can hold the source
  ///
  template <typename Source> static SafeUInt<sizeof(Source) * 8U> fromAny(Source const val) VB_NOEXCEPT {
    static_assert((sizeof(Source) * 8U) <= 32U, "Source too large");
    SafeUInt<sizeof(Source) * 8U> safeInt{};
    safeInt.value_ = static_cast<ValueType>(val);
    return safeInt;
  }

  /// @brief Bitwise AND operator.
  /// @param val The value to AND with.
  /// @return A SafeUInt with same range.
  SafeUInt<range> operator&(uint32_t const val) const VB_NOEXCEPT {
    SafeUInt<range> safeInt{};
    safeInt.value_ = this->value_ & val;
    return safeInt;
  }

  /// @brief Left shift operator.
  /// @tparam shift The number of bits to shift.
  /// @return A SafeUInt with new_range = range + shift
  template <size_t shift> SafeUInt<range + shift> leftShift() const VB_NOEXCEPT {
    SafeUInt<range + shift> safeInt{};
    safeInt.value_ = this->value_ << static_cast<uint32_t>(shift);
    return safeInt;
  }

  /// @brief right shift operator.
  /// @tparam shift The number of bits to shift.
  /// @return A SafeUInt with new_range = range - shift
  template <size_t shift> SafeUInt<range - shift> rightShift() const VB_NOEXCEPT {
    SafeUInt<range - shift> safeInt{};
    safeInt.value_ = this->value_ >> static_cast<uint32_t>(shift);
    return safeInt;
  }

  /// @brief Addition operator.
  /// @tparam range2 The bit range of the other SafeUInt.
  /// @param other The other SafeUInt to add.
  /// @return A SafeUInt with range + 1 because operator+ can overflow
  template <size_t range2> SafeUInt<(range > range2) ? (range + 1U) : (range2 + 1U)> operator+(SafeUInt<range2> const other) const VB_NOEXCEPT {
    SafeUInt<(range > range2) ? (range + 1U) : (range2 + 1U)> safeInt{};
    safeInt.value_ = this->value_ + other.value_;
    return safeInt;
  }

  /// @brief Subtraction operator.
  /// @tparam range2 The bit range of the other SafeUInt.
  /// @param other The other SafeUInt to subtract.
  /// @return A SafeUInt with same range as current SafeUInt.
  /// @note The range of the current SafeUInt must be greater than the range of the other SafeUInt.
  template <size_t range2> SafeUInt<range> operator-(SafeUInt<range2> const other) const VB_NOEXCEPT {
    static_assert(range > range2, "must high range sub low range");
    SafeUInt<range> safeInt{};
    safeInt.value_ = this->value_ - other.value_;
    return safeInt;
  }

  /// @brief Explicit cast operator to another SafeUInt.
  /// @tparam newRange The new bit range.
  /// @return A SafeUInt with eq or later range
  template <size_t newRange> explicit operator SafeUInt<newRange>() const VB_NOEXCEPT {
    static_assert(newRange >= range, "dangerous cast");
    SafeUInt<newRange> safeInt{};
    safeInt.value_ = this->value_;
    return safeInt;
  }

  /// @brief Explicit cast operator to SafeInt.
  /// @tparam newRange The new bit range.
  /// @return A SafeInt with same range
  template <size_t newRange> explicit operator SafeInt<newRange>() const VB_NOEXCEPT;

private:
  // The constructor should be private, but due to msvc bug
  // https://developercommunity.visualstudio.com/t/compile-error-with-nested-private-conste/1360866 it need to leave
  // public for now SafeUInt() VB_NOEXCEPT = default;
  ValueType value_ = 0U; ///< hold the actual value
  template <size_t> friend class SafeUInt;
  template <size_t> friend class SafeInt;
  template <size_t> friend class UnsignedInRangeCheck;
};

/// @brief Wrapper class to check if a signed integer is safe to be encoded into instructions.
/// @tparam range The bit range for the SafeInt.
template <size_t range> class SafeInt final {
public:
  using ValueType = int32_t; ///< Type alias for the value type.
  static_assert(range > 0U, "range too small");
  static_assert(range < (sizeof(ValueType) * static_cast<size_t>(8U)),
                "Safe int current can't handle more than 31 bit, if you see this error, please refactor this class to "
                "increase the limit");

  /// @brief Get the value of the SafeInt.
  /// @return The value of the SafeInt.
  inline ValueType value() const VB_NOEXCEPT {
    return value_;
  }

  /// @brief Create a SafeInt from a constant value.
  /// @tparam constValue The constant value.
  /// @return A SafeInt.
  // coverity[autosar_cpp14_m3_2_2_violation]
  template <ValueType constValue> constexpr static SafeInt<range> fromConst() VB_NOEXCEPT {
    using Unsigned_Value = typename std::make_unsigned<ValueType>::type;
    constexpr ValueType maxTargetSigned{static_cast<ValueType>((static_cast<Unsigned_Value>(1U) << (range - 1U)) - 1U)};
    constexpr ValueType minTargetSigned{-maxTargetSigned - 1};
    static_assert((constValue <= maxTargetSigned) && (constValue >= minTargetSigned), "const value out of range");
    SafeInt<range> safeInt{};
    safeInt.value_ = constValue;
    return safeInt;
  }

  /// @brief Create a SafeInt from an unsafe value.
  /// @param val The unsafe value.
  /// @return A SafeInt.
  static SafeInt<range> fromUnsafe(int32_t const val) VB_NOEXCEPT {
    SafeInt<range> safeInt{};
    safeInt.value_ = val;
    return safeInt;
  }

  ///
  /// @brief Create a SafeInt base on the size of Source
  /// @tparam Source source type
  /// @param val input
  /// @return SafeInt which can hold the source
  ///
  template <typename Source> static SafeInt<sizeof(Source) * 8U> fromAny(Source const val) VB_NOEXCEPT {
    static_assert((sizeof(Source) * 8U) <= 32U, "Source too large");
    SafeInt<sizeof(Source) * 8U> safeInt{};
    safeInt.value_ = static_cast<ValueType>(val);
    return safeInt;
  }

  /// @brief Addition operator.
  /// @tparam range2 The bit range of the other SafeUInt.
  /// @param other The other SafeUInt to add.
  /// @return A SafeUInt with range + 1 because operator+ can overflow
  template <size_t range2> SafeInt<(range > range2) ? (range + 1U) : (range2 + 1U)> operator+(SafeInt<range2> const other) const VB_NOEXCEPT {
    SafeInt<(range > range2) ? (range + 1U) : (range2 + 1U)> safeInt{};
    safeInt.value_ = this->value_ + other.value_;
    return safeInt;
  }

  /// @brief Negates the value of the current SafeInt object.
  /// @return A new SafeInt with size range+1, because signed value can only represent [-x, x), then -(-x) can out of
  /// range
  SafeInt<range + 1U> operator-() const VB_NOEXCEPT {
    SafeInt<range + 1U> safeInt{};
    safeInt.value_ = -this->value_;
    return safeInt;
  }

  /// @brief Explicit cast operator to SafeUInt.
  /// @tparam newRange The new bit range.
  /// @return A SafeUInt with eq or larger range
  template <size_t newRange> explicit operator SafeUInt<newRange>() const VB_NOEXCEPT {
    static_assert(newRange >= range, "dangerous cast");
    SafeUInt<newRange> safeInt{};
    safeInt.value_ = static_cast<typename SafeUInt<newRange>::ValueType>(this->value_);
    return safeInt;
  }

  /// @brief Explicit cast operator to SafeInt.
  /// @tparam newRange The new bit range.
  /// @return A SafeInt with eq or larger range
  template <size_t newRange> explicit operator SafeInt<newRange>() const VB_NOEXCEPT {
    static_assert(newRange >= range, "dangerous cast");
    SafeInt<newRange> safeInt{};
    safeInt.value_ = this->value_;
    return safeInt;
  }

private:
  // The constructor should be private, but due to msvc bug
  // https://developercommunity.visualstudio.com/t/compile-error-with-nested-private-conste/1360866 it need to leave
  // public for now SafeInt() VB_NOEXCEPT = default;
  ValueType value_ = 0; ///< hold the actual value
  template <size_t> friend class SafeUInt;
  template <size_t> friend class SafeInt;
  template <size_t> friend class SignedInRangeCheck;
};

// coverity[autosar_cpp14_a3_1_5_violation] must declare here otherwise SafeInt is undefined
template <size_t range> template <size_t newRange> SafeUInt<range>::operator SafeInt<newRange>() const VB_NOEXCEPT {
  static_assert(newRange >= range, "dangerous cast");
  SafeInt<newRange> safeInt{};
  safeInt.value_ = static_cast<typename SafeInt<newRange>::ValueType>(this->value_);
  return safeInt;
}

// @brief class to check if an unsigned integer is in range of a SafeUInt.
/// @tparam bits_target The bit range for the SafeUInt.
template <size_t bits_target> class UnsignedInRangeCheck final {
public:
  /// @brief Check if a source value is in range.
  /// @tparam Source The type of the source value.
  /// @param source The source value.
  /// @return An UnsignedInRangeCheck refers to if the value is in range.
  template <typename Source> static UnsignedInRangeCheck<bits_target> check(Source const source) VB_NOEXCEPT {
    static_assert(std::is_integral<Source>::value, "check only works for integers");
    static_assert(std::is_unsigned<Source>::value, "check only works for unsigned integers");
    UnsignedInRangeCheck<bits_target> checkResult{};
    if (source <= SafeUInt<bits_target>::maxValue) {
      checkResult.inRange_ = true;
      checkResult.safeInt_.value_ = static_cast<typename SafeUInt<bits_target>::ValueType>(source);
    } else {
      checkResult.inRange_ = false;
    }
    return checkResult;
  }

  /// @brief Check if a source value is in range.
  /// @tparam Source The type of the source value.
  /// @param source The source value.
  /// @param limit max limit of the value
  /// @return An UnsignedInRangeCheck refers to if the value is in range.
  template <typename Source> static UnsignedInRangeCheck<bits_target> check(Source const source, Source const limit) VB_NOEXCEPT {
    UnsignedInRangeCheck<bits_target> checkResult{};

    if ((source <= limit) && (limit <= SafeUInt<bits_target>::maxValue)) {
      checkResult.inRange_ = true;
      checkResult.safeInt_.value_ = static_cast<typename SafeUInt<bits_target>::ValueType>(source);
    } else {
      checkResult.inRange_ = false;
    }

    return checkResult;
  }

  ///
  /// @brief Create a invalid UnsignedInRangeCheck
  ///
  /// @return UnsignedInRangeCheck with in range false
  ///
  static UnsignedInRangeCheck<bits_target> invalid() VB_NOEXCEPT {
    UnsignedInRangeCheck<bits_target> checkResult{};
    checkResult.inRange_ = false;
    return checkResult;
  }

  ///@see safeInt_
  inline SafeUInt<bits_target> const &safeInt() const VB_NOEXCEPT {
    return safeInt_;
  }

  ///@see inRange_
  inline bool inRange() const VB_NOEXCEPT {
    return inRange_;
  }

private:
  SafeUInt<bits_target> safeInt_; ///< The SafeUint after check, it's only hold valid value when inRange==true
  bool inRange_ = false;          ///< If the check result is in range
};

/// @brief Class to check if a signed integer is in range.
/// @tparam bits_target The bit range for the SafeInt.
template <size_t bits_target> class SignedInRangeCheck final {
public:
  /// @brief Check if a source value is in range of a SafeInt.
  /// @tparam Source The type of the source value.
  /// @param source The source value.
  /// @return A SignedInRangeCheck refer to if the check is in range
  template <typename Source> static SignedInRangeCheck<bits_target> check(Source const source) VB_NOEXCEPT {
    static_assert(std::is_integral<Source>::value, "check only works for integers");
    static_assert(std::is_signed<Source>::value, "check only works for signed integers");
    SignedInRangeCheck<bits_target> checkResult{};
    if (in_range<bits_target>(source)) {
      checkResult.inRange_ = true;
      checkResult.safeInt_.value_ = static_cast<typename SafeInt<bits_target>::ValueType>(source);
    } else {
      checkResult.inRange_ = false;
    }
    return checkResult;
  }

  /// @brief Check if a source value is in range.
  /// @tparam Source The type of the source value.
  /// @param source The source value.
  /// @param lowerLimit lower limit for checking
  /// @param upperLimit upper limit for checking
  /// @return A SignedInRangeCheck refer to if the check is in range
  template <typename Source>
  static SignedInRangeCheck<bits_target> check(Source const source, Source const lowerLimit, Source const upperLimit) VB_NOEXCEPT {
    SignedInRangeCheck<bits_target> checkResult{};
    if ((in_range<bits_target>(source) && (source >= lowerLimit)) && (source <= upperLimit)) {
      checkResult.inRange_ = true;
      checkResult.safeInt_.value_ = bit_cast<typename SafeInt<bits_target>::ValueType>(source);
    } else {
      checkResult.inRange_ = false;
    }
    return checkResult;
  }

  ///
  /// @brief Create a invalid SignedInRangeCheck
  ///
  /// @return SignedInRangeCheck with in range false
  ///
  static SignedInRangeCheck<bits_target> invalid() VB_NOEXCEPT {
    SignedInRangeCheck<bits_target> checkResult{};
    checkResult.inRange_ = false;
    return checkResult;
  }

  ///@see safeInt_
  inline SafeInt<bits_target> const &safeInt() const VB_NOEXCEPT {
    return safeInt_;
  }

  ///@see inRange_
  inline bool inRange() const VB_NOEXCEPT {
    return inRange_;
  }

private:
  SafeInt<bits_target> safeInt_; ///< The SafeInt after check, it's only hold valid value when inRange==true
  bool inRange_ = false;         ///< If the check result is in range
};

} // namespace vb
#endif
