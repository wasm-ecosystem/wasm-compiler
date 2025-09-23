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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>

#include "WabtCmd.hpp"
#include "tests/unittests/common.hpp"

#include "src/core/common/Span.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/runtime/Runtime.hpp"
#include "src/utils/ExecutableMemory.hpp"
#include "src/utils/LinearMemoryAllocator.hpp"
#include "src/utils/MemUtils.hpp"
#include "src/utils/STDCompilerLogger.hpp"

#if CXX_TARGET == JIT_TARGET

static void *allocFnc(uint32_t size, void *ctx) noexcept {
  static_cast<void>(ctx);
  return malloc(static_cast<size_t>(size));
}

static void freeFnc(void *ptr, void *ctx) noexcept {
  static_cast<void>(ctx);
  free(ptr);
}

static void memoryFnc(vb::ExtendableMemory &currentObject, uint32_t minimumLength, void *const ctx) {
  static_cast<void>(ctx);
  if (minimumLength == 0) {
    free(currentObject.data());
  } else {
    minimumLength = std::max(minimumLength, static_cast<uint32_t>(1000U)) * 2U;
    currentObject.reset(vb::pCast<uint8_t *>(realloc(currentObject.data(), minimumLength)), minimumLength);
  }
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestRuntimeAPI, testGetMemoryUsage) {
  std::vector<uint8_t> const wasmFileContent = vb::test::WabtCmd::loadWasmFromWat("(module)");
  vb::Compiler compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);
  vb::STDCompilerLogger stdCompilerLogger{};
  compiler.setLogger(&stdCompilerLogger);
  vb::ManagedBinary const binaryModule = compiler.compile(vb::Span<uint8_t const>(wasmFileContent.data(), wasmFileContent.size()));
  vb::ExecutableMemory const executableMemory = vb::ExecutableMemory::make_executable_copy(binaryModule);

  // Initialize the module, populate linear memory initial data etc.
#if LINEAR_MEMORY_BOUNDS_CHECKS
  vb::Runtime const runtime(executableMemory, memoryFnc);
  uint64_t const mem = runtime.getMemoryUsage();
  ASSERT_LE(mem, 200U);
#else
  vb::LinearMemoryAllocator linearMemoryAllocator;
  vb::Runtime const runtime(executableMemory, linearMemoryAllocator, nullptr);
  uint64_t const mem = linearMemoryAllocator.getMemoryUsage();
  ASSERT_LE(mem, vb::MemUtils::getOSMemoryPageSize());
#endif
}

TEST(TestRuntimeAPI, CallWithFuncIndex) {
  const char *const watStr = R"(
  (module
  (memory $0 1)
  (func $func)
  (table $0 1 funcref)
  (elem (i32.const 0) $func)
  (export "table" (table $0))
  )
  )";
  std::vector<uint8_t> const wasmFileContent = vb::test::WabtCmd::loadWasmFromWat(watStr);

  vb::Compiler compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);
  vb::STDCompilerLogger stdCompilerLogger{};
  compiler.setLogger(&stdCompilerLogger);
  vb::ManagedBinary const binaryModule = compiler.compile(vb::Span<uint8_t const>(wasmFileContent.data(), wasmFileContent.size()));
  vb::ExecutableMemory const executableMemory = vb::ExecutableMemory::make_executable_copy(binaryModule);

  auto runtime = vb::test::createRuntime(executableMemory);
  runtime.start();

  EXPECT_NO_THROW(runtime.getFunctionByExportedTableIndex<0>(0U));
  EXPECT_THROW(runtime.getFunctionByExportedTableIndex<0>(1U), vb::RuntimeError);
  EXPECT_THROW((runtime.getFunctionByExportedTableIndex<1, int32_t>(0U)), vb::RuntimeError);
  EXPECT_THROW((runtime.getFunctionByExportedTableIndex<2, int32_t, int64_t>(0U)), vb::RuntimeError);
}

#endif // CXX_TARGET == JIT_TARGET
