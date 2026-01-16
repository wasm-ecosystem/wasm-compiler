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

#include "src/core/common/util.hpp"

namespace vb {

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestCommonUtil, testCLZ) {
  uint64_t constexpr num1 = 0xFFU;
  uint32_t constexpr num2 = 0xFFFU;

  ASSERT_EQ(clzImpl(num1), clzll(num1));
  ASSERT_EQ(clzImpl(num2), clz(num2));
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestCommonUtil, testCTZ) {
  // Test 64-bit variant
  ASSERT_EQ(ctzImpl(uint64_t{0x100U}), 8);               // trailing zeros: 8
  ASSERT_EQ(ctzImpl(uint64_t{0x8000000000000000U}), 63); // trailing zeros: 63
  ASSERT_EQ(ctzImpl(uint64_t{0x1U}), 0);                 // trailing zeros: 0
  ASSERT_EQ(ctzImpl(uint64_t{0xFFFFFFFFFFFFFFFFU}), 0);  // all bits set

  // Test 32-bit variant
  ASSERT_EQ(ctzImpl(uint32_t{0x100U}), 8);       // trailing zeros: 8
  ASSERT_EQ(ctzImpl(uint32_t{0x80000000U}), 31); // trailing zeros: 31
  ASSERT_EQ(ctzImpl(uint32_t{0x1U}), 0);         // trailing zeros: 0
  ASSERT_EQ(ctzImpl(uint32_t{0xFFF0U}), 4);      // trailing zeros: 4
  ASSERT_EQ(ctzImpl(uint32_t{0xFFFFFFFFU}), 0);  // all bits set
}
} // namespace vb
