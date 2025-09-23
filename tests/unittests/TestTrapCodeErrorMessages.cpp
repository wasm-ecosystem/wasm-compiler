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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/core/common/TrapCode.hpp"

#if CXX_TARGET == JIT_TARGET

namespace vb {

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestTrapCodeErrorMessages, errorMessagesMatchEnum) {
  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::NONE)], testing::HasSubstr("No trap"));

  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::UNREACHABLE)], testing::HasSubstr("Unreachable instruction executed"));
  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::BUILTIN_TRAP)], testing::HasSubstr("builtin.trap"));

  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::LINMEM_OUTOFBOUNDSACCESS)],
              testing::HasSubstr("Linear memory access out of bounds"));
  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::LINMEM_COULDNOTEXTEND)], testing::HasSubstr("Could not extend linear memory"));

  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::INDIRECTCALL_OUTOFBOUNDS)], testing::HasSubstr("Indirect call out of bounds"));
  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::INDIRECTCALL_WRONGSIG)],
              testing::HasSubstr("Indirect call performed with wrong signature"));

  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::LINKEDMEMORY_NOTLINKED)], testing::HasSubstr("No memory linked"));
  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::LINKEDMEMORY_OUTOFBOUNDS)],
              testing::HasSubstr("Linked memory access out of bounds"));

  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::DIV_ZERO)], testing::HasSubstr("Division by zero"));
  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::DIV_OVERFLOW)], testing::HasSubstr("Integer division overflow"));
  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::TRUNC_OVERFLOW)], testing::HasSubstr("Float to int conversion overflow"));

  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::RUNTIME_INTERRUPT_REQUESTED)],
              testing::HasSubstr("Runtime interrupt externally triggered"));
  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::STACKFENCEBREACHED)], testing::HasSubstr("Stack fence breached"));

  ASSERT_THAT(trapCodeErrorMessages[static_cast<uint32_t>(TrapCode::CALLED_FUNCTION_NOT_LINKED)], testing::HasSubstr("Called function not linked"));
}
} // namespace vb

#endif // CXX_TARGET == JIT_TARGET
