/*
 * Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VB_BIT_COUNTER_HPP
#define VB_BIT_COUNTER_HPP
#include <cstdint>

#include "src/config.hpp"
#include "src/core/common/util.hpp"

namespace vb {
/// @brief Class to count continuous 1s in a uint32_t value
class ContinuousBitSequence final {
public:
  /// @brief Check if all 1-bits in the value are continuous and return their position and width
  /// @param value The value to check for continuous 1s
  /// @return ContinuousBitSequence object with position and width if all 1s are continuous, otherwise invalidPos
  static inline ContinuousBitSequence count(uint32_t const value) VB_NOEXCEPT {
    ContinuousBitSequence counter{};

    if (value == 0U) {
      return counter;
    }

    // Special case: all bits set
    if (value == 0xFFFFFFFFU) {
      counter.pos_ = 0U;
      counter.width_ = 32U;
      return counter;
    }

    // Count total number of 1s - this is the width candidate
    uint32_t const width{static_cast<uint32_t>(popcnt(value))};

    // Find position of LSB (first 1 bit) using count trailing zeros
    uint32_t const pos{static_cast<uint32_t>(ctz(value))};

    // Check if all 1s are continuous: value >> pos should equal (1 << width) - 1
    if ((value >> pos) == ((static_cast<uint32_t>(1U) << width) - 1U)) {
      counter.pos_ = pos;
      counter.width_ = width;
    } else {
      counter.pos_ = invalidPos;
      counter.width_ = 0U;
    }

    return counter;
  }

  /// @brief Get the position of the least significant bit of continuous 1s
  inline uint32_t getPos() const VB_NOEXCEPT {
    return pos_;
  }
  /// @brief Get the width of continuous 1s
  inline uint32_t getWidth() const VB_NOEXCEPT {
    return width_;
  }

  static constexpr uint32_t invalidPos{UINT32_MAX}; ///< Invalid position constant

private:
  uint32_t pos_{invalidPos}; ///< LSB position of continuous 1
  uint32_t width_{0U};       ///< Width of continuous 1
};
} // namespace vb

#endif // VB_BIT_COUNTER_HPP
