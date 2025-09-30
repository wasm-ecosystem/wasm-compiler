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

#include "src/config.hpp"

#ifndef JIT_TARGET_TRICORE
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "WabtCmd.hpp"

#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/MemUtils.hpp"
#include "src/utils/STDCompilerLogger.hpp"

static std::filesystem::path const getProjectRoot() {
  std::filesystem::path const currentFilePath(__FILE__);
  std::filesystem::path const projectRoot = currentFilePath.parent_path().parent_path().parent_path();
  return projectRoot;
}

static inline std::filesystem::path getWasmTestCasesDir(std::filesystem::path projectRoot) {
  std::filesystem::path wasmDir = projectRoot / "wasm_examples";
  return wasmDir;
}

static uint8_t *getStackTop() {
  vb::MemUtils::StackInfo const stackInfo{vb::MemUtils::getStackInfo()};
  return vb::pCast<uint8_t *>(stackInfo.stackTop);
}

std::vector<uint8_t> readWasmFile(std::filesystem::path const &filePath) {
  std::ifstream file{filePath, std::ios::binary};
  // coverity[autosar_cpp14_a16_2_3_violation]
  if (file) {
    std::vector<uint8_t> bytecode{};
    // coverity[autosar_cpp14_a16_2_3_violation]
    bytecode.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return bytecode;
  } else {
    throw std::runtime_error("Failed to open WebAssembly file: " + filePath.string());
  }
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestWasmModule, setMaxRam) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  vb::STDCompilerLogger logger{};
  constexpr uint32_t maxRam{512U};
  vb::WasmModule m(maxRam, logger, true, nullptr, 0U);
  m.setMaxRam(maxRam);

  EXPECT_EQ(m.getMaxRam(), maxRam);
  vb::WasmModule::destroyEnvironment();
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestWasmModule, testInitFromBytecode) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  std::filesystem::path const wasmPath = getWasmTestCasesDir(getProjectRoot()) / "addtwo.wasm";

  vb::STDCompilerLogger logger{};

  constexpr uint32_t maxRam{16U * 4096U * 2U};

  vb::WasmModule module(maxRam, logger, true, nullptr, 0U);

  ASSERT_EQ(module.getMaxRam(), maxRam);

  module.setMaxRam(2U * maxRam);

  ASSERT_EQ(module.getMaxRam(), 2U * maxRam);

  std::vector<uint8_t> const bytecode{readWasmFile(wasmPath)};

  module.initFromBytecode(vb::Span<const uint8_t>(bytecode.data(), bytecode.size()), vb::Span<vb::NativeSymbol const>{}, true);

  vb::Span<uint8_t const> const compiledBinary{module.getCompiledBinary()};
  ASSERT_NE(compiledBinary.data(), nullptr);

  vb::Span<uint8_t const> const debugSymbol{module.getRawDebugSymbol()};
  ASSERT_NE(debugSymbol.data(), nullptr);

  uint8_t const *const stackTop{getStackTop()};

  module.start(stackTop);

  std::array<vb::WasmValue, 1U> const res{module.callExportedFunctionWithName<1U, int32_t, int32_t>(stackTop, "addTwo", 1, 2)};
  ASSERT_EQ(res[0].i32, 3);
  vb::WasmModule::destroyEnvironment();
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestWasmModule, testDebugBuild) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  std::filesystem::path const wasmPath = getWasmTestCasesDir(getProjectRoot()) / "addtwo.wasm";

  vb::STDCompilerLogger logger{};

  constexpr uint32_t maxRam{10000U};

  vb::WasmModule module(maxRam, logger, true, nullptr, 0U);

  std::vector<uint8_t> const bytecode{readWasmFile(wasmPath)};

  vb::WasmModule::CompileResult const compileResult{
      module.compile(vb::Span<const uint8_t>(bytecode.data(), bytecode.size()), vb::Span<vb::NativeSymbol const>{})};

  ASSERT_GT(compileResult.getDebugSymbol().size(), 0U);

  vb::WasmModule::destroyEnvironment();
}

static inline void nop(void *const ctx) {
  static_cast<void>(ctx);
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestWasmModule, testInitFromCompiledBinary) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  std::filesystem::path const wasmPath = getWasmTestCasesDir(getProjectRoot()) / "addtwo.wasm";

  vb::STDCompilerLogger logger{};

  constexpr uint32_t maxRam{16U * 4096U * 2U};

  vb::WasmModule module(maxRam, logger, true, nullptr, 0U);

  std::vector<uint8_t> const bytecode{readWasmFile(wasmPath)};

  vb::WasmModule::CompileResult const compileResult{
      module.compile(vb::Span<const uint8_t>(bytecode.data(), bytecode.size()), vb::Span<vb::NativeSymbol const>{})};

  std::array<vb::NativeSymbol, 1U> const staticSymbol{STATIC_LINK("env", "nop", nop)};

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto)
  ASSERT_THROW(module.initFromCompiledBinary(compileResult.getModule().span(),
                                             vb::Span<vb::NativeSymbol const>{staticSymbol.data(), staticSymbol.size()},
                                             compileResult.getDebugSymbol().span()),
               vb::RuntimeError);

  module.initFromCompiledBinary(compileResult.getModule().span(), vb::Span<vb::NativeSymbol const>{}, compileResult.getDebugSymbol().span());

  ASSERT_NE(module.getRawDebugSymbol().data(), nullptr);
  uint8_t const *const stackTop{getStackTop()};

  module.start(stackTop);

  vb::WasmModule::destroyEnvironment();
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestWasmModule, testRequestInterrupt) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  vb::STDCompilerLogger logger{};

  constexpr uint32_t maxRam{16U * 4096U * 2U};

  vb::WasmModule module(maxRam, logger, false, nullptr, 3U);

  std::string_view constexpr watStr = R"(
  (module
    (func $infinite_loop
      (loop $forever
        br $forever
      )
    )
    (start $infinite_loop)
  )
  )";

  std::vector<uint8_t> const bytecode{vb::test::WabtCmd::loadWasmFromWat(watStr)};

  module.initFromBytecode(vb::Span<const uint8_t>(bytecode.data(), bytecode.size()), vb::Span<vb::NativeSymbol const>{}, true);

  uint8_t const *const stackTop{getStackTop()};

  std::atomic<bool> stopped{false};

  std::thread interruptThread([&module, &stopped]() VB_NOEXCEPT {
    while (!stopped.load()) {
      module.requestInterruption(vb::TrapCode::RUNTIME_INTERRUPT_REQUESTED);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto)
  ASSERT_THROW(module.start(stackTop), vb::TrapException);

  stopped = true;
  interruptThread.join();

  vb::STDCompilerLogger stackLogger{};

  module.printStacktrace(stackLogger);

  uint32_t trapFunctionIndex = UINT32_MAX;
  module.iterateStacktraceRecords(vb::FunctionRef<void(uint32_t)>([&trapFunctionIndex](uint32_t const fncIndex) {
    trapFunctionIndex = fncIndex;
  }));

  ASSERT_EQ(trapFunctionIndex, 0U);

  vb::WasmModule::destroyEnvironment();
}

TEST(TestWasmModule, testShrinkMemory) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  vb::STDCompilerLogger logger{};

  constexpr uint32_t maxRam{16U * 4096U * 2U};

  vb::WasmModule module(maxRam, logger, false, nullptr, 0U);

  std::string_view constexpr watStr = R"(
    (module
      (func $store
        i32.const 0x3000
        i32.const 1
        i32.store
      )
      (start $store)

      (memory 2 100)
    )
  )";

  std::vector<uint8_t> const bytecode{vb::test::WabtCmd::loadWasmFromWat(watStr)};

  module.initFromBytecode(vb::Span<const uint8_t>(bytecode.data(), bytecode.size()), vb::Span<vb::NativeSymbol const>{}, true);

  uint8_t const *const stackTop{getStackTop()};

  module.start(stackTop);

  uint64_t const memoryUseBeforeShrink{module.getRamUsage()};

  module.shrinkMemory(0x1000U);

  uint64_t const memoryUseAfterShrink{module.getRamUsage()};

  vb::WasmModule::destroyEnvironment();

  ASSERT_GT(memoryUseBeforeShrink, memoryUseAfterShrink);
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestWasmModule, testLinkMemory) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  vb::STDCompilerLogger logger{};

  constexpr uint32_t maxRam{16U * 4096U * 2U};

  vb::WasmModule module(maxRam, logger, false, nullptr, 0U);

  std::string_view constexpr watStr = R"(
  (module
    (import "builtin" "getLengthOfLinkedMemory" (func $getLengthOfLinkedMemory (result i32)))
    (func (export "getLengthOfLinkedMemory_wrapper") (result i32) (call $getLengthOfLinkedMemory))
  )
  )";

  std::vector<uint8_t> const bytecode{vb::test::WabtCmd::loadWasmFromWat(watStr)};

  module.initFromBytecode(vb::Span<const uint8_t>(bytecode.data(), bytecode.size()), vb::Span<vb::NativeSymbol const>{}, true);

  uint8_t const *const stackTop{getStackTop()};

  module.start(stackTop);

  std::array<uint8_t, 3U> constexpr data{1U, 2U, 3U};

  module.linkMemory(vb::Span<const uint8_t>(data.data(), data.size()));

  std::array<vb::WasmValue, 1U> const resAfterLink{module.callExportedFunctionWithName<1U>(stackTop, "getLengthOfLinkedMemory_wrapper")};
  ASSERT_EQ(resAfterLink[0].i32, static_cast<int32_t>(data.size()));
  module.unlinkMemory();

  std::array<vb::WasmValue, 1U> const resAfterUnlink{module.callExportedFunctionWithName<1U>(stackTop, "getLengthOfLinkedMemory_wrapper")};
  ASSERT_EQ(resAfterUnlink[0].i32, 0);
  vb::WasmModule::destroyEnvironment();
}

static std::string_view constexpr watStrStoreAtStart = R"(
    (module
      (func $_start
        i32.const 0
        i32.const 1
        i32.store
      )

      (func $foo (result i32)
        i32.const 0
        i32.load
      )
      
      (export "_start" (func $_start))
      (export "foo" (func $foo))
      
      (memory 1 1)
    )
  )";

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestWasmModule, testCallWasmFunctionByName) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  vb::STDCompilerLogger logger{};

  constexpr uint32_t maxRam{16U * 4096U * 2U};

  vb::WasmModule module(maxRam, logger, false, nullptr, 0U);

  std::vector<uint8_t> const bytecode{vb::test::WabtCmd::loadWasmFromWat(watStrStoreAtStart)};

  module.initFromBytecode(vb::Span<const uint8_t>(bytecode.data(), bytecode.size()), vb::Span<vb::NativeSymbol const>{}, true);

  uint8_t const *const stackTop{getStackTop()};

  module.start(stackTop);
  module.callExportedFunctionWithName<0U>(stackTop, "_start");

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto)
  ASSERT_THROW(module.callExportedFunctionWithName<1U>(stackTop, "f"), vb::RuntimeError);

  std::array<vb::WasmValue, 1U> const res{module.callExportedFunctionWithName<1U>(stackTop, "foo")};
  ASSERT_EQ(res[0].i32, 1);
  vb::WasmModule::destroyEnvironment();
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestWasmModule, testCallWasmFunctionByTableIndex) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  vb::STDCompilerLogger logger{};

  constexpr uint32_t maxRam{16U * 4096U * 2U};

  vb::WasmModule module(maxRam, logger, false, nullptr, 0U);

  std::string_view constexpr watStr = R"(
  (module
    (func $add (param i32 i32) (result i32 i32)
      local.get 0
      local.get 1
      i32.add

      local.get 0
      local.get 1
      i32.sub
    )
    
    (table $my_table 1 1 funcref)
    
    (elem (i32.const 0) $add)
    
    (export "functionTable" (table $my_table))
  )
  )";

  std::vector<uint8_t> const bytecode{vb::test::WabtCmd::loadWasmFromWat(watStr)};

  module.initFromBytecode(vb::Span<const uint8_t>(bytecode.data(), bytecode.size()), vb::Span<vb::NativeSymbol const>{}, true);

  uint8_t const *const stackTop{getStackTop()};
  module.start(stackTop);
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto)
  ASSERT_THROW((module.callWasmFunctionByExportedTableIndex<2U, int32_t, int32_t>(stackTop, 100U, 2, 1)), vb::RuntimeError);

  std::array<vb::WasmValue, 2U> const resAfterLink{module.callWasmFunctionByExportedTableIndex<2U, int32_t, int32_t>(stackTop, 0U, 2, 1)};
  ASSERT_EQ(resAfterLink[0].i32, 3);
  ASSERT_EQ(resAfterLink[1].i32, 1);
  vb::WasmModule::destroyEnvironment();
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestWasmModule, testOutOfMemory) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  vb::STDCompilerLogger logger{};

  constexpr uint32_t maxRam{16U * 4096U * 2U};

  vb::WasmModule module(maxRam, logger, false, nullptr, 0U);

  std::string_view constexpr watStr = R"(
  (module
    (func $_start
      i32.const 0x300000
      i32.const 1
      i32.store
    )
    (export "_start" (func $_start))
    (memory 1 100)
  )
  )";

  std::vector<uint8_t> const bytecode{vb::test::WabtCmd::loadWasmFromWat(watStr)};

  module.initFromBytecode(vb::Span<const uint8_t>(bytecode.data(), bytecode.size()), vb::Span<vb::NativeSymbol const>{}, true);

  uint8_t const *const stackTop{getStackTop()};
  module.start(stackTop);
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto)
  ASSERT_THROW(module.callExportedFunctionWithName<0>(stackTop, "_start"), vb::TrapException);

  vb::WasmModule::destroyEnvironment();
}
#endif
