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

#include <cstddef>
#include <gtest/gtest.h>

#include "src/config.hpp"

#ifndef JIT_TARGET_TRICORE
#include "src/utils/MemUtils.hpp"
#include "src/utils/VirtualMemoryAllocator.hpp"

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestVirtualMemoryAllocator, testResize) {
  const size_t pageSize = vb::MemUtils::getOSMemoryPageSize();
  const size_t totalSize = 1024 * pageSize;
  vb::VirtualMemoryAllocator allocator(totalSize);

  uint8_t *const data = allocator.data();

  ASSERT_NE(data, nullptr);

  const size_t size_request1 = pageSize * 5U;
  size_t const size_ret1 = allocator.resize(size_request1);

  data[size_request1 - 1U] = 1U;

  ASSERT_GE(size_ret1, size_request1);
  ASSERT_EQ(size_ret1, allocator.getCommitedSize());

  const size_t size_request2 = pageSize * 3U;

  size_t const size_ret2 = allocator.resize(size_request2);

  ASSERT_GE(size_ret2, size_request2);
  ASSERT_LE(size_ret2, size_ret1);

  size_t const size_ret3 = allocator.resize(size_request1 * 2U);
  ASSERT_LE(size_ret1, size_ret3);
  data[size_request1 + 1U] = 1U;

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto)
  EXPECT_THROW({ allocator.resize(totalSize * 2U); }, std::bad_alloc);
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestVirtualMemoryAllocator, testMove) {
  vb::VirtualMemoryAllocator allocator(4096);
  void const *const data = allocator.data();
  vb::VirtualMemoryAllocator const allocator2 = std::move(allocator);

  ASSERT_EQ(allocator2.data(), data);
}

#endif