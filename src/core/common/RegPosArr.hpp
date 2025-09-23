///
/// @file RegPosArr.hpp
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
#ifndef REGPOSARR_HPP
#define REGPOSARR_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

#include "src/config.hpp"

namespace vb {

///
/// @brief Helper function to convert a tuple to an std:array
///
/// @tparam Tuple Tuple type
/// @tparam Indices
/// @param tuple Input tuple
/// @return std::array
template <typename Tuple, std::size_t... Indices>
// coverity[autosar_cpp14_a7_1_5_violation]
constexpr decltype(auto) to_array(Tuple &&tuple, std::index_sequence<Indices...> /*unused*/) VB_NOEXCEPT {
  return std::array<std::common_type_t<std::tuple_element_t<Indices, std::remove_reference_t<Tuple>>...>, sizeof...(Indices)>{
      {std::get<Indices>(tuple)...}};
}

///
/// @brief Convert a tuple to an std:array
///
/// @tparam Tuple Tuple type
/// @param tuple Input tuple
/// @return std::array
// coverity[autosar_cpp14_a7_1_5_violation]
// coverity[autosar_cpp14_a13_3_1_violation]
template <typename Tuple> constexpr decltype(auto) to_array(Tuple &&tuple) VB_NOEXCEPT {
  return to_array(std::forward<Tuple>(tuple), std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
}

///
/// @brief Find the position of an element in an array
///
/// @tparam N Array size
/// @tparam RegType Register type
/// @param in Input array
/// @param val Value to look for
/// @param pos Current position to test (for recursion, pass 0 as outside caller)
/// @return Position in the given array
template <size_t N, typename RegType>
// coverity[autosar_cpp14_a7_5_2_violation] it's compiler time checked, no runtime stack overflow risk
constexpr uint8_t findPos(std::array<RegType, N> const &in, RegType const val, uint32_t const pos = 0U) VB_NOEXCEPT {
  static_assert(N < static_cast<size_t>(UINT8_MAX), "Too many registers");
  static_assert(std::is_enum<RegType>::value, "Template parameter T must be an enum class");
  if (pos >= N) {
    return static_cast<uint8_t>(UINT8_MAX);
  } else if (in[pos] == val) {
    return static_cast<uint8_t>(pos);
  } else {
    return findPos<N, RegType>(in, val, pos + 1U);
  }
}

///
/// @brief Helper class to create a position array for an input array of registers
///
/// @tparam TotalRegCount Number of total registers that exist (number of entries in the enum)
/// @tparam InputArrLen Length of the input array
/// @tparam RemainingRegs How many registers are remaining to be processed
/// @tparam RegType Register type
template <uint32_t TotalRegCount, size_t InputArrLen, uint32_t RemainingRegs, typename RegType> class gen_class {
  static_assert(std::is_enum<RegType>::value, "Template parameter T must be an enum class");

public:
  ///
  /// @brief Helper function to create a position array for an input array of registers
  ///
  /// @param in Input array
  /// @return Position array
  static constexpr std::array<uint8_t, RemainingRegs> make(std::array<RegType, InputArrLen> const &in) VB_NOEXCEPT {
    constexpr uint32_t indexInResultArr{TotalRegCount - RemainingRegs};
    constexpr RegType reg{static_cast<RegType>(indexInResultArr)};
    std::array<uint8_t, 1> const singlePosOfRegArr{{findPos<InputArrLen, RegType>(in, reg)}};
    return to_array(std::tuple_cat(singlePosOfRegArr, gen_class<TotalRegCount, InputArrLen, RemainingRegs - 1U, RegType>::make(in)));
  }
};

///
/// @brief Helper class to create a position array for an input array of registers
///
/// @tparam TotalRegCount Number of total registers that exist (number of entries in the enum)
/// @tparam InputArrLen Length of the input array
/// @tparam RegType Register type
template <uint32_t TotalRegCount, size_t InputArrLen, typename RegType> class gen_class<TotalRegCount, InputArrLen, 1, RegType> {
  static_assert(std::is_enum<RegType>::value, "Template parameter T must be an enum class");

public:
  ///
  /// @brief Helper function to create a position array for an input array of registers
  ///
  /// @param in Input array
  /// @return Position array
  static constexpr std::array<uint8_t, 1U> make(std::array<RegType, InputArrLen> const &in) VB_NOEXCEPT {
    constexpr uint32_t indexInResultArr{TotalRegCount - 1U};
    constexpr RegType reg{static_cast<RegType>(indexInResultArr)};

    std::array<uint8_t, 1> const singlePosOfRegArr{{findPos<InputArrLen, RegType>(in, reg)}};
    return singlePosOfRegArr;
  }
};

///
/// @brief Generate a position array for an input array of registers, e.g. first element is the position of
/// static_cast<REG>(0U) in the given array
///
/// @tparam TotalRegCount Number of total registers that exist (number of entries in the enum)
/// @tparam InputArrLen Length of the input array
/// @tparam RegType Register type
/// @param in Input array of registers
/// @return Array with length of number of total register count, defining the index of each register in the array
/// (UINT32_MAX if not found in the given array)
template <uint32_t TotalRegCount, size_t InputArrLen, typename RegType>
constexpr std::array<uint8_t, TotalRegCount> genPosArr(std::array<RegType, InputArrLen> const &in) VB_NOEXCEPT {
  return gen_class<TotalRegCount, InputArrLen, TotalRegCount, RegType>::make(in);
}

} // namespace vb

#endif
