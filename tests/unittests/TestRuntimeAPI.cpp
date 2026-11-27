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

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/runtime/Runtime.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/ExecutableMemory.hpp"
#include "src/utils/MemUtils.hpp"
#include "src/utils/STDCompilerLogger.hpp"
#include "src/utils/SignalFunctionWrapper.hpp"

#if LINEAR_MEMORY_BOUNDS_CHECKS
#include "src/core/runtime/ActiveMemoryManager.hpp"
#else
#include "src/utils/LinearMemoryAllocator.hpp"
#endif

#if CXX_TARGET == JIT_TARGET

static void *allocFnc(uint32_t size, void *const ctx) noexcept {
  static_cast<void>(ctx);
  return malloc(static_cast<size_t>(size));
}

static void freeFnc(void *const ptr, void *const ctx) noexcept {
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
  vb::ActiveMemoryManager memoryManager{memoryFnc, nullptr};
  vb::Runtime const runtime(executableMemory, memoryManager, nullptr);
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

#if !LINEAR_MEMORY_BOUNDS_CHECKS
TEST(TestRuntimeAPI, landingPadCommitsMemoryAndPreservesFaultingFrame) {
  std::vector<uint8_t> const wasmFileContent{vb::test::WabtCmd::loadWasmFromWat(R"(
    (module
      (memory 1 2)
      (func (export "grow") (result i32)
        i32.const 1
        memory.grow)
      (func (export "store") (param i32)
        local.get 0
        i32.const 42
        i32.store)
      (func (export "load") (param i32) (result i32)
        local.get 0
        i32.load))
  )")};
  vb::Compiler compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);
  compiler.setStacktraceRecordCount(2U);
  vb::ManagedBinary const binaryModule{compiler.compile({wasmFileContent.data(), wasmFileContent.size()})};
  vb::ExecutableMemory const executableMemory{vb::ExecutableMemory::make_executable_copy(binaryModule)};
  auto runtime = vb::test::createRuntime(executableMemory);
#if ACTIVE_STACK_OVERFLOW_CHECK
  runtime.setStackFence(nullptr);
#endif
  vb::SignalFunctionWrapper::start(runtime);
  auto const grow{runtime.getExportedFunctionByName<1>("grow")};
  ASSERT_EQ(vb::SignalFunctionWrapper::call(grow)[0].i32, 1);
  auto const store{runtime.getExportedFunctionByName<0, int32_t>("store")};
  auto const load{runtime.getExportedFunctionByName<1, int32_t>("load")};
  EXPECT_NO_THROW(vb::SignalFunctionWrapper::call(store, 65536));
  EXPECT_EQ(vb::SignalFunctionWrapper::call(load, 65536)[0].i32, 42);
  try {
    vb::SignalFunctionWrapper::call(store, 131072);
    FAIL() << "Access beyond the declared memory must trap";
  } catch (vb::TrapException const &exception) {
    EXPECT_EQ(exception.getTrapCode(), vb::TrapCode::LINMEM_OUTOFBOUNDSACCESS);
  }
  std::vector<uint32_t> stacktrace;
  runtime.iterateStacktraceRecords(vb::FunctionRef<void(uint32_t)>([&stacktrace](uint32_t const fncIndex) {
    stacktrace.push_back(fncIndex);
  }));
  EXPECT_EQ(stacktrace, (std::vector<uint32_t>{1U}));
  EXPECT_EQ(vb::SignalFunctionWrapper::call(load, 65536)[0].i32, 42);
}
#endif

#if ACTIVE_STACK_OVERFLOW_CHECK
TEST(TestRuntimeAPI, stackFenceReservesAlignedHeadroom) {
  std::vector<uint8_t> const wasmFileContent{vb::test::WabtCmd::loadWasmFromWat("(module)")};
  vb::Compiler compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);
  vb::ManagedBinary const binaryModule{compiler.compile({wasmFileContent.data(), wasmFileContent.size()})};
  vb::ExecutableMemory const executableMemory{vb::ExecutableMemory::make_executable_copy(binaryModule)};
  auto runtime = vb::test::createRuntime(executableMemory);

  constexpr uintptr_t stackAlignment{(JIT_TARGET == ISA_TRICORE) ? 8U : 16U};
  constexpr uintptr_t maxFence{UINTPTR_MAX & ~(stackAlignment - 1U)};
  constexpr uintptr_t highestHardLimit{maxFence - static_cast<uintptr_t>(STACK_HEAD_ROOM)};
  for (uintptr_t const hardLimit : {uintptr_t{0U}, uintptr_t{1U}, highestHardLimit}) {
    ASSERT_NO_THROW(runtime.setStackFence(vb::numToP<void *>(hardLimit)));
    uintptr_t const fence{vb::readFromPtr<uintptr_t>(vb::pSubI(runtime.unsafe__getLinearMemoryBase(), Basedata::FromEnd::stackFence))};
    EXPECT_EQ(fence % stackAlignment, 0U);
    EXPECT_GE(fence - hardLimit, static_cast<uintptr_t>(STACK_HEAD_ROOM));
    EXPECT_LT(fence - hardLimit - static_cast<uintptr_t>(STACK_HEAD_ROOM), stackAlignment);
  }
  EXPECT_THROW(runtime.setStackFence(vb::numToP<void *>(highestHardLimit + 1U)), vb::RuntimeError);
  EXPECT_THROW(runtime.setStackFence(vb::numToP<void *>(UINTPTR_MAX)), vb::RuntimeError);
}

TEST(TestRuntimeAPI, stackOverflowTrapPreservesTraceAndRuntime) {
  std::vector<uint8_t> const wasmFileContent{vb::test::WabtCmd::loadWasmFromWat(R"(
    (module
      (func $f (export "f") (param i32)
        local.get 0
        if
          local.get 0
          i32.const 1
          i32.sub
          call $f
        end))
  )")};
  vb::Compiler compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);
  compiler.setStacktraceRecordCount(3U);
  vb::ManagedBinary const binaryModule{compiler.compile({wasmFileContent.data(), wasmFileContent.size()})};
  vb::ExecutableMemory const executableMemory{vb::ExecutableMemory::make_executable_copy(binaryModule)};
  auto runtime = vb::test::createRuntime(executableMemory);
  uint8_t stackMarker{0U};
  uintptr_t const hardLimit{vb::pToNum(&stackMarker) - 16384U - static_cast<uintptr_t>(STACK_HEAD_ROOM)};
  runtime.setStackFence(vb::numToP<void *>(hardLimit));
  runtime.start();
  auto const function{runtime.getExportedFunctionByName<0, int32_t>("f")};

  try {
    function(1024);
    FAIL() << "Recursive Wasm execution must exceed the stack fence";
  } catch (vb::TrapException const &exception) {
    EXPECT_EQ(exception.getTrapCode(), vb::TrapCode::STACKFENCEBREACHED);
  }

  std::vector<uint32_t> stacktrace;
  runtime.iterateStacktraceRecords(vb::FunctionRef<void(uint32_t)>([&stacktrace](uint32_t const fncIndex) {
    stacktrace.push_back(fncIndex);
  }));
  EXPECT_EQ(stacktrace, (std::vector<uint32_t>{0U, 0U, 0U}));
  EXPECT_NO_THROW(function(0));
}
#endif

#endif // CXX_TARGET == JIT_TARGET
