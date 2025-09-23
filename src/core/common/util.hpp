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
#ifndef COREUTIL_HPP
#define COREUTIL_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "src/config.hpp"

#if (defined _MSC_VER) && !(defined __clang__)
#include <intrin.h>
#endif

#if (defined __GNUC__) && !(defined __CPTC__)
#define VB_GCC
#endif

#ifndef UNREACHABLE
#ifdef _MSC_VER
#define UNREACHABLE(STMT, COMMENT) __assume(0);
#elif (defined VB_GCC) || (defined __clang__)
#define UNREACHABLE(STMT, COMMENT) __builtin_unreachable();
#elif (defined __CPTC__)
#define UNREACHABLE(STMT, COMMENT) STMT;
#else
static_assert(false, "C/C++ compiler not supported");
#endif
#endif

namespace vb {
///
/// @brief Count leading zeros implementation
///
/// @param mask Input value
/// @return int32_t Leading zeros of mask
int32_t clz(uint32_t const mask) VB_NOEXCEPT;

///
/// @brief Count leading zeros implementation
///
/// @param mask Input value
/// @return int32_t Leading zeros of mask
int32_t clzll(uint64_t const mask) VB_NOEXCEPT;

///
/// @brief Count trailing zeros implementation
///
/// @param mask Input value
/// @return int32_t Trailing zeros of mask
int32_t ctzll(uint64_t const mask) VB_NOEXCEPT;

///
/// @brief Counts the number of 1 bits (population count) in an unsigned integer
/// @param mask Input value
/// @return population count
int32_t popcnt(uint32_t const mask) VB_NOEXCEPT;

///
/// @brief Counts the number of 1 bits (population count) in an unsigned integer
/// @param mask Input value
/// @return population count
int32_t popcntll(uint64_t const mask) VB_NOEXCEPT;

///
/// @brief Literal suffix for explicit initialization of a uint8_t
///
// NOLINTNEXTLINE(google-runtime-int)
inline constexpr uint8_t operator""_U8(const unsigned long long arg) VB_NOEXCEPT {
  return static_cast<uint8_t>(arg);
}

///
/// @brief Literal suffix for explicit initialization of a uint32_t
///
// NOLINTNEXTLINE(google-runtime-int)
inline constexpr uint32_t operator""_U32(const unsigned long long arg) VB_NOEXCEPT {
  return static_cast<uint32_t>(arg);
}

///
/// @brief Literal suffix for explicit initialization of a uint64_t
///
// NOLINTNEXTLINE(google-runtime-int)
inline constexpr uint64_t operator""_U64(const unsigned long long arg) VB_NOEXCEPT {
  return static_cast<uint64_t>(arg);
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
///
/// @brief Reinterprets a data type so that the underlying bit representation is unchanged. Unconditionally fulfills the
/// strict aliasing rule.
///
/// @tparam Dest Destination type
/// @tparam Source Source type
/// @param source Data to reinterpret
/// @return constexpr Dest Reinterpreted data
// coverity[autosar_cpp14_a8_4_7_violation]
template <class Dest, class Source> constexpr Dest bit_cast(Source const &source) VB_NOEXCEPT {
  static_assert(std::is_trivially_copyable<Source>::value, "bit_cast requires the source type to be copyable");
  static_assert(std::is_trivially_copyable<Dest>::value, "bit_cast requires the destination type to be copyable");
  static_assert(sizeof(Dest) == sizeof(Source), "bit_cast requires source and destination to be the same size");
  Dest dest;
  static_cast<void>(std::memcpy(&dest, &source, static_cast<size_t>(sizeof(dest))));
  return dest;
}

///
/// @brief cast a pointer to uintptr_t
///
/// @tparam T Type of the to be casted pointer
/// @param ptr pointer to cast
/// @return uintptr_t The integer value
///
// coverity[autosar_cpp14_m7_1_2_violation]
template <typename T> inline uintptr_t pToNum(T const ptr) VB_NOEXCEPT {
  static_assert(std::is_pointer<T>::value, "T must be a pointer");
  // coverity[autosar_cpp14_m5_2_9_violation]
  // coverity[autosar_cpp14_a5_2_4_violation]
  return reinterpret_cast<uintptr_t>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

///
/// @brief Special case when check if alignof(T) is invalid
///
/// @tparam T void or function
/// @return always return true
///
template <typename T> inline bool isAligned(uintptr_t addr, std::integral_constant<bool, true> /*unused*/) VB_NOEXCEPT {
  static_cast<void>(addr);
  return true;
}

///
/// @brief Check if an address is aligned with a type
///
/// @tparam T Type to check
/// @return true Aligned
/// @return false Not aligned
///
template <typename T> inline bool isAligned(uintptr_t addr, std::integral_constant<bool, false> /*unused*/) VB_NOEXCEPT {
  return (addr % alignof(T)) == 0;
}

///
/// @brief Wrapped cast between pointer types with alignment assert
///
/// @tparam TargetType Target type
/// @tparam SourceType Source type
/// @param ptr The pointer to cast
/// @return Pointer in target type
///
// coverity[autosar_cpp14_m7_1_2_violation]
// coverity[autosar_cpp14_a5_1_7_violation]
template <typename TargetType, typename SourceType> inline TargetType pCast(SourceType const ptr) VB_NOEXCEPT {
  static_assert(std::is_pointer<TargetType>::value && std::is_pointer<SourceType>::value, "Target and Parameter must be a pointer");
  // coverity[autosar_cpp14_a5_1_7_violation]
  using AlignType = typename std::remove_const<typename std::remove_pointer<TargetType>::type>::type;

  constexpr bool isVoidOrFunction{std::is_same<AlignType, void>::value || std::is_function<AlignType>::value};
  static_cast<void>(isVoidOrFunction);
  assert(isAligned<AlignType>(pToNum(ptr), std::integral_constant<bool, isVoidOrFunction>()) && "Pointer cast with wrong alignment");
#ifdef VB_GCC
// coverity[autosar_cpp14_a16_7_1_violation]
#pragma GCC diagnostic push
// coverity[autosar_cpp14_a16_7_1_violation]
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
  // coverity[autosar_cpp14_m5_2_8_violation]
  // coverity[autosar_cpp14_m5_2_9_violation]
  // coverity[autosar_cpp14_a5_2_4_violation]
  // coverity[autosar_cpp14_a5_1_7_violation]
  return reinterpret_cast<TargetType>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
#ifdef VB_GCC
// coverity[autosar_cpp14_a16_7_1_violation]
#pragma GCC diagnostic pop
#endif
}

///
/// @brief Reads a specific datatype from a given pointer. The pointer does not have to be aligned. Unconditionally
/// fulfills the strict aliasing rule.
///
/// @tparam Dest Destination type; type to read
/// @param ptr Pointer that points to the data that should be read
/// @return Dest Data read from the pointer in the given destination type
template <class Dest> inline Dest readFromPtr(uint8_t const *const ptr) VB_NOEXCEPT {
  static_assert(std::is_trivially_copyable<Dest>::value, "readFromPtr requires the destination type to be copyable");
  Dest dest{};
  // NOLINTBEGIN(bugprone-sizeof-expression,clang-analyzer-unix.cstring.NullArg,clang-analyzer-core.NonNullParamChecker)
  // coverity[autosar_cpp14_a12_0_2_violation]
  static_cast<void>(std::memcpy(&dest, ptr, static_cast<size_t>(sizeof(Dest))));
  // NOLINTEND(bugprone-sizeof-expression,clang-analyzer-unix.cstring.NullArg,clang-analyzer-core.NonNullParamChecker)
  return dest;
}

///
/// @brief Writes data to a given pointer. The pointer must not be aligned. Unconditionally fulfills the strict aliasing
/// rule.
///
/// @tparam Type Type to write; used to make sure the user is fully aware which datatype this function will write
/// @tparam Source Type of the source data, must be equal to Type
/// @param ptr Pointer that points to where the data should be written
/// @param source Data that should be written
// coverity[autosar_cpp14_a8_4_7_violation]
template <class Type, class Source> inline void writeToPtr(uint8_t *const ptr, Source const &source) VB_NOEXCEPT {
  static_assert(std::is_same<typename std::remove_const<typename std::remove_pointer<Type>::type>::type,
                             typename std::remove_const<typename std::remove_pointer<Source>::type>::type>::value ||
                    std::is_same<std::nullptr_t, Source>::value,
                "Tried to write wrong type");
  static_assert(std::is_trivially_copyable<Source>::value, "writeToPtr requires the source type to be copyable");
  static_cast<void>(std::memcpy(ptr, &source,
                                static_cast<size_t>(sizeof(source)))); // NOLINT(bugprone-sizeof-expression)
}

///
/// @brief Creates an std::array from a list of elements as function arguments
///
/// @tparam V The explicit type of the array
/// @tparam T Type of the elements
/// @param t Parameter pack of the elements
/// @return std::array<V, sizeof...(T)> Array of the given type with the elements in place
// coverity[autosar_cpp14_m7_1_2_violation]
// coverity[autosar_cpp14_a8_4_7_violation]
// coverity[autosar_cpp14_a18_1_1_violation]
template <typename... T> constexpr auto make_array(T &&...t) noexcept -> std::array<typename std::common_type<T...>::type const, sizeof...(T)> {
  return {{std::forward<typename std::common_type<T...>::type const>(t)...}};
}

///
/// @brief Software implementation of clz. Shall only be used when the target platform doesn't provide this feature
///
/// @tparam T Number type
/// @param mask Input value
/// @return int32_t Leading zeros of mask
///
template <typename T> int32_t clzImpl(T mask) VB_NOEXCEPT {
  static_assert(std::is_integral<T>::value, "T must be a integral");
  uint32_t where = 0;
  while (mask != 0) {
    mask >>= 1U;
    where++;
  }
  return (static_cast<int32_t>(sizeof(T)) * 8) - static_cast<int32_t>(where);
}

///
/// @brief Software implementation of popcnt. Shall only be used when the target platform doesn't provide this feature
///
/// @tparam T Number type
/// @param mask Input value
/// @return int32_t population count
///
template <typename T> int32_t popcntImpl(T mask) VB_NOEXCEPT {
  static_assert(std::is_integral<T>::value, "T must be a integral");
  uint32_t maskedRegsCount = 0U;
  while (mask != 0) {
    mask &= mask - 1U;
    maskedRegsCount++;
  }
  return static_cast<int32_t>(maskedRegsCount);
}

#if (defined _MSC_VER) && !(defined __clang__)

inline int32_t clzll(uint64_t const mask) VB_NOEXCEPT {
  unsigned long where;

  // BitScanReverse scans from MSB to LSB for first set bit.
  if (_BitScanReverse64(&where, mask)) {
    return static_cast<int32_t>(63 - where);
  }
  return 64; // Undefined Behavior.
}

inline int32_t ctzll(uint64_t const mask) VB_NOEXCEPT {
  unsigned long where;

  // BitScanReverse scans from MSB to LSB for first set bit.
  if (_BitScanForward64(&where, mask)) {
    return static_cast<int32_t>(where);
  }
  return 64; // Undefined Behavior.
}

inline int32_t clz(uint32_t const mask) VB_NOEXCEPT {
  unsigned long where;

  // BitScanReverse scans from MSB to LSB for first set bit.
  if (_BitScanReverse(&where, mask)) {
    return static_cast<int32_t>(31 - where);
  }
  return 32; // Undefined Behavior.
}

#if (defined _M_ARM64) || (defined _M_ARM)

inline int32_t popcnt(uint32_t const mask) VB_NOEXCEPT {
  return _CountOneBits(mask);
}
inline int32_t popcntll(uint64_t const mask) VB_NOEXCEPT {
  return _CountOneBits64(mask);
}

#elif (defined _M_X64)

inline int32_t popcnt(uint32_t const mask) VB_NOEXCEPT {
  return __popcnt(mask);
}
inline int32_t popcntll(uint64_t const mask) VB_NOEXCEPT {
  return static_cast<int32_t>(__popcnt64(mask));
}

#else

inline int32_t popcnt(uint32_t const mask) VB_NOEXCEPT {
  return popcntImpl(mask);
}
inline int32_t popcntll(uint64_t const mask) VB_NOEXCEPT {
  return popcntImpl(mask);
}

#endif // _M_ARM64) || (defined _M_ARM)

#elif (((defined __GNUC__) || (defined __clang__)) && !(defined __CPTC__))

inline int32_t clzll(uint64_t const mask) VB_NOEXCEPT {
  return __builtin_clzll(mask);
}
inline int32_t clz(uint32_t const mask) VB_NOEXCEPT {
  return __builtin_clz(mask);
}
inline int32_t ctzll(uint64_t const mask) VB_NOEXCEPT {
  return __builtin_ctzll(mask);
}
inline int32_t popcnt(uint32_t const mask) VB_NOEXCEPT {
  return __builtin_popcount(mask);
}
inline int32_t popcntll(uint64_t const mask) VB_NOEXCEPT {
  return __builtin_popcountll(mask);
}

#elif defined __CPTC__

inline int32_t clzll(uint64_t const mask) VB_NOEXCEPT {
  return clzImpl(mask);
}
inline int32_t clz(uint32_t const mask) VB_NOEXCEPT {
  return __clz(static_cast<int32_t>(mask));
}
inline int32_t popcnt(uint32_t const mask) VB_NOEXCEPT {
  return __popcntw(mask);
}
inline int32_t popcntll(uint64_t const mask) VB_NOEXCEPT {
  return popcntImpl(mask);
}

#else

inline int32_t clzll(uint64_t const mask) VB_NOEXCEPT {
  return clzImpl(mask);
}
inline int32_t clz(uint32_t const mask) VB_NOEXCEPT {
  return clzImpl(mask);
}
inline int32_t popcnt(uint32_t const mask) VB_NOEXCEPT {
  return popcntImpl(mask);
}
inline int32_t popcntll(uint64_t const mask) VB_NOEXCEPT {
  return popcntImpl(mask);
}

#endif // _MSC_VER) && !(defined __clang__)

///
/// @brief Calculates the log2 of a given number. Only defined for powers of two.
///
/// @param n Input, must be a power of two
/// @return uint32_t log2(n)
inline uint32_t log2I32(uint32_t const n) VB_NOEXCEPT {
  assert(((n & (n - 1)) == 0) && "Number not a power of two");
  assert(n != 0 && "log2 not defined for zero");
  return 32U - static_cast<uint32_t>(clz(n)) - 1U;
}

///
/// @brief Constexpr version of log2I32
///@note This function is only used if result is constexpr because builtin_clz can't be evaluated as constexpr by C++.
/// If n is runtime value, use log2I32 for better performance
/// @param n Input, must be a power of two
/// @return constexpr uint32_t log2(n)
///
inline constexpr uint32_t log2Constexpr(uint32_t n) VB_NOEXCEPT {
  uint32_t result{0U};
  while (n > 1U) {
    n >>= 1U; // Divide n by 2 using bitwise right shift
    ++result;
  }
  return result;
}

///
/// @brief Rounds the input up to the next power of two
///
/// @param value Number to round up
/// @param pow2 Power of two to round up to
/// @return constexpr uint32_t Rounded up number
inline constexpr uint32_t roundUpToPow2(uint32_t const value, uint32_t const pow2) VB_NOEXCEPT {
  uint32_t const mask{(1_U32 << pow2) - 1_U32};
  return ((value & mask) != 0U) ? ((value + (mask + 1_U32)) & ~mask) : value;
}

///
/// @brief Returns the difference to the next power of two
///
/// @param value Base value
/// @param pow2 Power of two for which to calculate the result
/// @return constexpr uint32_t Difference to the next power of two
inline constexpr uint32_t deltaToNextPow2(uint32_t const value, uint32_t const pow2) VB_NOEXCEPT {
  return roundUpToPow2(value, pow2) - value;
}

///
/// @brief Cast number to pointer
///
/// @tparam Target type
/// @tparam TNUM Offset type
/// @param num Number to cast
/// @return T pointer in target type
///
template <typename T, typename TNUM> inline T numToP(TNUM const num) VB_NOEXCEPT {
  static_assert(std::is_pointer<T>::value, "T must be a pointer");
  static_assert(std::is_integral<TNUM>::value, "TNUM must be an integer");
  // coverity[autosar_cpp14_m5_2_8_violation]
  // coverity[autosar_cpp14_a5_2_4_violation]
  return reinterpret_cast<T>(num); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

///
/// @brief add a pointer with offset
///
/// @tparam T Pointer type
/// @tparam TNUM Offset type
/// @param ptr
/// @param offset
/// @return calculate result
///
template <typename T, typename TNUM> inline T pAddI(T const ptr, TNUM const offset) VB_NOEXCEPT {
  static_assert(std::is_pointer<T>::value, "T must be a pointer");
  static_assert(sizeof(typename std::remove_pointer<T>::type) == 1U, "T must point to a byte");
  static_assert(std::is_integral<TNUM>::value, "TNUM must be an integer");
  T const res{ptr + offset};
  return res;
}

///
/// @brief sub a pointer with offset
///
/// @tparam T Pointer type
/// @tparam TNUM Offset type
/// @param ptr pointer to sub
/// @param offset
/// @return calculate result
///
template <typename T, typename TNUM> inline T pSubI(T const ptr, TNUM const offset) VB_NOEXCEPT {
  static_assert(std::is_pointer<T>::value, "T must be a pointer");
  static_assert(sizeof(typename std::remove_pointer<T>::type) == 1U, "T must point to a byte");
  static_assert(std::is_integral<TNUM>::value, "TNUM must be an integer");
  return ptr - offset;
}

///
/// @brief calculate address difference of two pointers
///
/// @tparam T1 Type of the first pointer
/// @tparam T2 Type of the second pointer
/// @param ptr1 The operand 1
/// @param ptr2 The operand 2
/// @return Byte difference
///
template <typename T1, typename T2> inline uintptr_t pSubAddr(T1 const ptr1, T2 const ptr2) VB_NOEXCEPT {
  static_assert(std::is_pointer<T1>::value && std::is_pointer<T2>::value, "Parameter must be a pointer");
  return pToNum(ptr1) - pToNum(ptr2);
}

///
/// @brief get length of a C string
///
/// @param str pointer of the string
/// @param max max size to scan
/// @return size_t
///
inline size_t strlen_s(char const *const str, size_t const max) VB_NOEXCEPT {
#ifdef _MSC_VER
  return strnlen_s(str, max);
#elif (defined _POSIX_C_SOURCE) || (defined __DARWIN_C_ANSI) || (defined _QNX_SOURCE)
  return strnlen(str, max);
#else
  const char *const found = pCast<const char *>(memchr(str, '\0', max));
  return (found != nullptr) ? static_cast<size_t>(pSubAddr(found, str)) : max;
#endif
}

///
/// @brief Wrapper of const_cast, remove the cost of a const pointer type
///
/// @tparam T The pointer type
/// @param ptr Pointer to remove const
/// @return Pointer without const
///
template <typename T> inline typename std::remove_const<typename std::remove_pointer<T>::type>::type *pRemoveConst(T const ptr) VB_NOEXCEPT {
  static_assert(std::is_pointer<T>::value, "T must be a pointer");
  // coverity[autosar_cpp14_a5_2_3_violation]
  return const_cast<typename std::remove_const<typename std::remove_pointer<T>::type>::type *>(ptr); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

///
/// @brief Modifies the referenced pointer by subtracting the size of the datatype to read and reads the type and then
/// returns the data from that memory location
///
/// @tparam Dest Type to read
/// @param ptr Reference to the pointer that will be modified and points to the end of the data to read
/// @return Dest Data that has been read
template <class Dest> inline Dest readNextValue(uint8_t const **const ptr) VB_NOEXCEPT {
  *ptr = pSubI(*ptr, sizeof(Dest));
  return readFromPtr<Dest>(*ptr);
}

} // namespace vb
#endif
