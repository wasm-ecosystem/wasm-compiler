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
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <sstream>

#include "WabtCmd.hpp"
#include "extensions/Tracing.hpp"

#include "src/WasmModule/WasmModule.hpp"
#include "src/utils/STDCompilerLogger.hpp"

#if CXX_TARGET == JIT_TARGET
#if ENABLE_EXTENSIONS

static uint8_t *getStackTop() {
  vb::MemUtils::StackInfo const stackInfo{vb::MemUtils::getStackInfo()};
  return vb::pCast<uint8_t *>(stackInfo.stackTop);
}

static const char *const watStr = R"(
(module
  (import "builtin" "tracePoint" (func $tracePoint (param i32)))
  (func (export "_start")
    (call $tracePoint (i32.const 100))
    (call $tracePoint (i32.const 200))
    (call $tracePoint (i32.const 300))
  )
)
)";

// NOLINTNEXTLINE(cert-err58-cpp)
std::vector<uint8_t> const testModule = vb::test::WabtCmd::loadWasmFromWat(watStr);
vb::Span<const uint8_t> const bytecode = vb::Span<const uint8_t>(testModule.data(), testModule.size());

constexpr size_t magicSize = 16U;
constexpr size_t identifier = sizeof(uint64_t);    // 8 bytes for identifier
constexpr size_t timePointSize = sizeof(uint32_t); // 4 bytes for time point
constexpr size_t traceIdSize = sizeof(uint32_t);   // 4 bytes for traceId
struct TraceRecorderTest : public ::testing::Test {
  vb::STDCompilerLogger logger{};

  void TearDown() override {
    vb::extension::traceExtension.~TracingExtension();
    new (&vb::extension::traceExtension) vb::extension::TracingExtension{nullptr, 0};
  }

  static uint32_t getTraceId(std::string const &trace, size_t index) {
    uint32_t id{0U};
    std::memcpy(&id, &trace[magicSize + index * (identifier + timePointSize + traceIdSize) + identifier + timePointSize], traceIdSize);
    return id;
  }

  static size_t getTraceCount(std::string const &trace) {
    return (trace.size() - magicSize) / (identifier + timePointSize + traceIdSize);
  }
};

TEST_F(TraceRecorderTest, Disabled) {
  vb::extension::traceExtension.~TracingExtension();
  new (&vb::extension::traceExtension) vb::extension::TracingExtension{nullptr, 0};
  {
    vb::WasmModule::initEnvironment(&malloc, &realloc, &free);
    vb::WasmModule wasmModule{logger};
    wasmModule.initFromBytecode(bytecode, {}, true);
    wasmModule.start(getStackTop());
    wasmModule.callExportedFunctionWithName<0>(getStackTop(), "_start");
    vb::WasmModule::destroyEnvironment();
  }
}

TEST_F(TraceRecorderTest, Enabled) {
  vb::extension::traceExtension.~TracingExtension();
  auto traceStream{std::make_unique<std::stringstream>()};
  std::stringstream const *const ss = traceStream.get();
  new (&vb::extension::traceExtension) vb::extension::TracingExtension{std::move(traceStream), 100};
  {
    vb::WasmModule::initEnvironment(&malloc, &realloc, &free);
    vb::WasmModule wasmModule{logger};
    wasmModule.initFromBytecode(bytecode, {}, true);
    wasmModule.start(getStackTop());
    wasmModule.callExportedFunctionWithName<0>(getStackTop(), "_start");
    vb::WasmModule::destroyEnvironment();
  }
  std::string const trace = ss->str();

  EXPECT_EQ(getTraceCount(trace), 3U);
  EXPECT_EQ(trace.substr(0, 16), "___WARP_TRACE___");
  EXPECT_EQ(getTraceId(trace, 0U), 100U);
  EXPECT_EQ(getTraceId(trace, 1U), 200U);
  EXPECT_EQ(getTraceId(trace, 2U), 300U);
}

TEST_F(TraceRecorderTest, MultipleRuntime) {
  vb::extension::traceExtension.~TracingExtension();
  auto traceStream{std::make_unique<std::stringstream>()};
  std::stringstream const *const ss = traceStream.get();
  new (&vb::extension::traceExtension) vb::extension::TracingExtension{std::move(traceStream), 100};
  {
    vb::WasmModule::initEnvironment(&malloc, &realloc, &free);
    {
      vb::WasmModule m1{logger};
      m1.initFromBytecode(bytecode, {}, true);
      m1.start(getStackTop());
      m1.callExportedFunctionWithName<0>(getStackTop(), "_start");
    }
    {
      vb::WasmModule m2{logger};
      m2.initFromBytecode(bytecode, {}, true);
      m2.start(getStackTop());
      m2.callExportedFunctionWithName<0>(getStackTop(), "_start");
    }
    vb::WasmModule::destroyEnvironment();
  }
  std::string const trace = ss->str();

  EXPECT_EQ(getTraceCount(trace), 6U);
}

TEST_F(TraceRecorderTest, WithMaxItems) {
  vb::extension::traceExtension.~TracingExtension();
  auto traceStream{std::make_unique<std::stringstream>()};
  std::stringstream const *const ss = traceStream.get();
  new (&vb::extension::traceExtension) vb::extension::TracingExtension{std::move(traceStream), 2};
  {
    vb::WasmModule::initEnvironment(&malloc, &realloc, &free);
    vb::WasmModule wasmModule{logger};
    wasmModule.initFromBytecode(bytecode, {}, true);
    wasmModule.start(getStackTop());
    wasmModule.callExportedFunctionWithName<0>(getStackTop(), "_start");
    vb::WasmModule::destroyEnvironment();
  }
  std::string const trace = ss->str();
  EXPECT_EQ(getTraceCount(trace), 2U);
  EXPECT_EQ(getTraceId(trace, 0U), 100U);
  EXPECT_EQ(getTraceId(trace, 1U), 200U);
}

#endif // ENABLE_EXTENSIONS
#endif // CXX_TARGET == JIT_TARGET
