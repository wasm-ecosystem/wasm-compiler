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

#include <cstdint>
#include <gtest/gtest.h>

#include "src/core/common/function_traits.hpp"

namespace vb {

enum class EM1 : uint32_t { AA };
enum class EM2 : uint64_t { AA };

uint32_t foo(EM1, EM2, void *);

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestFunctionTraits, testEnumType) {
  const char *const typeStr = vb::function_traits<vb::remove_noexcept_t<decltype(foo)>>::getSignature();
  ASSERT_STREQ(typeStr, "(iI)i");
}
} // namespace vb
