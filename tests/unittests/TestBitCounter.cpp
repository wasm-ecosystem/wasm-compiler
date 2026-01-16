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

#include <gtest/gtest.h>

#include "src/core/compiler/common/BitCounter.hpp"

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestBitCounter, ContinuousBitSequence) {
  // 0b01110: pos=1, width=3
  {
    auto const result = vb::ContinuousBitSequence::count(0b01110U);
    EXPECT_EQ(result.getPos(), 1U);
    EXPECT_EQ(result.getWidth(), 3U);
  }

  // 0b0101: discontinuous 1s, pos=vb::ContinuousBitSequence::invalidPos
  {
    auto const result = vb::ContinuousBitSequence::count(0b0101U);
    EXPECT_EQ(result.getPos(), vb::ContinuousBitSequence::invalidPos);
    EXPECT_EQ(result.getWidth(), 0U);
  }

  // 0b0111: pos=0, width=3
  {
    auto const result = vb::ContinuousBitSequence::count(0b0111U);
    EXPECT_EQ(result.getPos(), 0U);
    EXPECT_EQ(result.getWidth(), 3U);
  }

  // 0: no 1s, pos=vb::ContinuousBitSequence::invalidPos
  {
    auto const result = vb::ContinuousBitSequence::count(0U);
    EXPECT_EQ(result.getPos(), vb::ContinuousBitSequence::invalidPos);
    EXPECT_EQ(result.getWidth(), 0U);
  }

  // Single bit: 0b1000, pos=3, width=1
  {
    auto const result = vb::ContinuousBitSequence::count(0b1000U);
    EXPECT_EQ(result.getPos(), 3U);
    EXPECT_EQ(result.getWidth(), 1U);
  }

  // All 1s in lower byte: 0xFF, pos=0, width=8
  {
    auto const result = vb::ContinuousBitSequence::count(0xFFU);
    EXPECT_EQ(result.getPos(), 0U);
    EXPECT_EQ(result.getWidth(), 8U);
  }

  // All 32 bits set: 0xFFFFFFFF, pos=0, width=32
  {
    auto const result = vb::ContinuousBitSequence::count(0xFFFFFFFFU);
    EXPECT_EQ(result.getPos(), 0U);
    EXPECT_EQ(result.getWidth(), 32U);
  }

  // High bit only: 0x80000000, pos=31, width=1
  {
    auto const result = vb::ContinuousBitSequence::count(0x80000000U);
    EXPECT_EQ(result.getPos(), 31U);
    EXPECT_EQ(result.getWidth(), 1U);
  }

  // Continuous 1s at high end: 0xFF000000, pos=24, width=8
  {
    auto const result = vb::ContinuousBitSequence::count(0xFF000000U);
    EXPECT_EQ(result.getPos(), 24U);
    EXPECT_EQ(result.getWidth(), 8U);
  }

  // Continuous 1s in middle: 0x00FF00, pos=8, width=8
  {
    auto const result = vb::ContinuousBitSequence::count(0x00FF00U);
    EXPECT_EQ(result.getPos(), 8U);
    EXPECT_EQ(result.getWidth(), 8U);
  }

  // Discontinuous with same popcnt: 0b10000001, pos=invalidPos
  {
    auto const result = vb::ContinuousBitSequence::count(0b10000001U);
    EXPECT_EQ(result.getPos(), vb::ContinuousBitSequence::invalidPos);
    EXPECT_EQ(result.getWidth(), 0U);
  }

  // Alternating pattern: 0x55555555, pos=invalidPos
  {
    auto const result = vb::ContinuousBitSequence::count(0x55555555U);
    EXPECT_EQ(result.getPos(), vb::ContinuousBitSequence::invalidPos);
    EXPECT_EQ(result.getWidth(), 0U);
  }

  // Gap in middle: 0b111011, pos=invalidPos
  {
    auto const result = vb::ContinuousBitSequence::count(0b111011U);
    EXPECT_EQ(result.getPos(), vb::ContinuousBitSequence::invalidPos);
    EXPECT_EQ(result.getWidth(), 0U);
  }

  // 16 continuous 1s: 0xFFFF, pos=0, width=16
  {
    auto const result = vb::ContinuousBitSequence::count(0xFFFFU);
    EXPECT_EQ(result.getPos(), 0U);
    EXPECT_EQ(result.getWidth(), 16U);
  }

  // 16 continuous 1s shifted: 0xFFFF0000, pos=16, width=16
  {
    auto const result = vb::ContinuousBitSequence::count(0xFFFF0000U);
    EXPECT_EQ(result.getPos(), 16U);
    EXPECT_EQ(result.getWidth(), 16U);
  }

  // Two 1s with gap: 0b100001, pos=invalidPos
  {
    auto const result = vb::ContinuousBitSequence::count(0b100001U);
    EXPECT_EQ(result.getPos(), vb::ContinuousBitSequence::invalidPos);
    EXPECT_EQ(result.getWidth(), 0U);
  }
}