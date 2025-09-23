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

#include "WabtCmd.hpp"
#include "tests/unittests/common.hpp"

#include "src/core/common/Span.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/runtime/Runtime.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/ExecutableMemory.hpp"

#if CXX_TARGET == JIT_TARGET

using namespace vb;

static const char *const watStr = R"(
(module
  (type (func))

  (import "env" "import1" (func))

  (export "reexport_import1" (func 0))
  (func (export "call_direct_import1")
        call 0
  )

  (func (export "call_indirect_import1")
    i32.const 0
        call_indirect 0
  )

  (table 1 funcref)
  (elem (i32.const 0) 0)
)
)";
// NOLINTNEXTLINE(cert-err58-cpp)
std::vector<uint8_t> const testModule = vb::test::WabtCmd::loadWasmFromWat(watStr);
vb::Span<const uint8_t> const bytecode = vb::Span<const uint8_t>(testModule.data(), testModule.size());

uint32_t import1CallCount = 0;
void import1(void *const ctx) noexcept {
  static_cast<void>(ctx);
  import1CallCount++;
}

auto const staticallyLinkedSymbols = make_array(STATIC_LINK("env", "import1", import1));

void *allocFnc(uint32_t size, void *ctx) noexcept {
  static_cast<void>(ctx);
  return malloc(static_cast<size_t>(size));
}

void freeFnc(void *ptr, void *ctx) noexcept {
  static_cast<void>(ctx);
  free(ptr);
}

void memoryFnc(ExtendableMemory &currentObject, uint32_t minimumLength, void *const ctx) {
  static_cast<void>(ctx);
  if (minimumLength == 0) {
    free(currentObject.data());
  } else {
    minimumLength = std::max(minimumLength, static_cast<uint32_t>(1000U)) * 2U;
    currentObject.reset(pCast<uint8_t *>(realloc(currentObject.data(), minimumLength)), minimumLength);
  }
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestUnknownImports, compilationSucceedsIfImportsAreProvided) {
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);

  ManagedBinary const binaryModule = compiler.compile(bytecode, staticallyLinkedSymbols);

  ExecutableMemory const executableMemory = ExecutableMemory::make_executable_copy(binaryModule);
  Runtime runtime = test::createRuntime(executableMemory);
  runtime.start();

  import1CallCount = 0U;
  std::string name = "reexport_import1";
  runtime.getRawExportedFunctionByName(vb::Span<char const>(name.c_str(), name.length()))(nullptr, nullptr);
  ASSERT_EQ(import1CallCount, 1U);

  name = "call_direct_import1";
  runtime.getRawExportedFunctionByName(vb::Span<char const>(name.c_str(), name.length()))(nullptr, nullptr);
  ASSERT_EQ(import1CallCount, 2U);

  name = "call_indirect_import1";
  runtime.getRawExportedFunctionByName(vb::Span<char const>(name.c_str(), name.length()))(nullptr, nullptr);
  ASSERT_EQ(import1CallCount, 3U);
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestUnknownImports, compilationFailsIfImportsAreNotProvided) {
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);

  ASSERT_THROW(compiler.compile(bytecode), LinkingException);
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestUnknownImports, compilationSucceedsIfUnknownImportsAreAllowedAndImportsAreNotProvided) {
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc, true);

  ManagedBinary const binaryModule = compiler.compile(bytecode);

  ExecutableMemory const executableMemory = ExecutableMemory::make_executable_copy(binaryModule);
  Runtime runtime = test::createRuntime(executableMemory);
  runtime.start();

  import1CallCount = 0U;
  ASSERT_THROW(
      {
        try {
          std::string const name = "reexport_import1";
          runtime.getRawExportedFunctionByName(vb::Span<char const>(name.c_str(), name.length()))(nullptr, nullptr);
        } catch (TrapException const &e) {
          ASSERT_EQ(e.getTrapCode(), TrapCode::CALLED_FUNCTION_NOT_LINKED);
          throw;
        }
      },
      TrapException);
  ASSERT_EQ(import1CallCount, 0U);
  ASSERT_THROW(
      {
        try {
          std::string const name = "call_direct_import1";
          runtime.getRawExportedFunctionByName(vb::Span<char const>(name.c_str(), name.length()))(nullptr, nullptr);
        } catch (TrapException const &e) {
          ASSERT_EQ(e.getTrapCode(), TrapCode::CALLED_FUNCTION_NOT_LINKED);
          throw;
        }
      },
      TrapException);
  ASSERT_EQ(import1CallCount, 0U);
  ASSERT_THROW(
      {
        try {
          std::string const name = "call_indirect_import1";
          runtime.getRawExportedFunctionByName(vb::Span<char const>(name.c_str(), name.length()))(nullptr, nullptr);
        } catch (TrapException const &e) {
          ASSERT_EQ(e.getTrapCode(), TrapCode::CALLED_FUNCTION_NOT_LINKED);
          throw;
        }
      },
      TrapException);
  ASSERT_EQ(import1CallCount, 0U);
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestUnknownImports, compilationSucceedsIfUnknownImportsAreAllowedAndImportsAreProvided) {
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc, true);

  ManagedBinary const binaryModule = compiler.compile(bytecode, staticallyLinkedSymbols);

  ExecutableMemory const executableMemory = ExecutableMemory::make_executable_copy(binaryModule);
  Runtime runtime = test::createRuntime(executableMemory);
  runtime.start();

  import1CallCount = 0U;
  std::string name = "reexport_import1";
  runtime.getRawExportedFunctionByName(vb::Span<char const>(name.c_str(), name.length()))(nullptr, nullptr);
  ASSERT_EQ(import1CallCount, 1U);

  name = "call_direct_import1";
  runtime.getRawExportedFunctionByName(vb::Span<char const>(name.c_str(), name.length()))(nullptr, nullptr);
  ASSERT_EQ(import1CallCount, 2U);

  name = "call_indirect_import1";
  runtime.getRawExportedFunctionByName(vb::Span<char const>(name.c_str(), name.length()))(nullptr, nullptr);
  ASSERT_EQ(import1CallCount, 3U);
}

#endif
