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
#include <unordered_map>

#include "WabtCmd.hpp"
#include "disassembler/disassembler.hpp"
#include "tests/unittests/common.hpp"

#include "src/core/common/function_traits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/compiler/common/ManagedBinary.hpp"
#include "src/core/runtime/Runtime.hpp"
#include "src/utils/ExecutableMemory.hpp"
#include "src/utils/SignalFunctionWrapper.hpp"

#if !defined(JIT_TARGET_TRICORE)

static const char *const watStr = R"(
(module
  (import "env" "from_a" (func $from_a))
  (import "env" "from_b" (func $from_b))
  (import "env" "direct" (func $direct))
  (export "direct" (func $direct))

  (func $a (export "a")
    (local i32 i64 i32)
    i32.const 11
    local.set 0
    i64.const 12
    local.set 1
    i32.const 13
    local.set 2

    i32.const 41
    global.set 0

    i32.const 0
    call_indirect
  )

  (func $a1
    (local i32 i64 i32)
    i32.const 14
    local.set 0
    i64.const 15
    local.set 1
    i32.const 16
    local.set 2
    call $from_a
  )

  (func $b (export "b")
    (local i32 i64)
    i32.const 0x17181920
    local.set 0
    i64.const 0x21222324
    local.set 1

    i32.const 42
    global.set 2

    i32.const 1
    call_indirect
  )

  (table 10 funcref)
  (elem (i32.const 0) $a1 $from_b)

  (global (mut i32) (i32.const 101))
  (global i32 (i32.const 102))
  (global (export "g3") (mut i32) (i32.const 103))
)
)";

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

template <class Dest> static inline Dest read(uint8_t const **const ptr) noexcept {
  Dest const val = vb::readFromPtr<Dest>(*ptr);
  *ptr = vb::pAddI(*ptr, sizeof(Dest));
  return val;
}

struct DeserializedDebugMap final {
  explicit DeserializedDebugMap() {
  }
  explicit DeserializedDebugMap(vb::ManagedBinary const &moduleDebugMap) {
    uint8_t const *ptr = moduleDebugMap.data();
    version_debugMap = read<uint32_t>(&ptr);
    offset_lastFramePtr = read<uint32_t>(&ptr);
    offset_actualLinMemSize = read<uint32_t>(&ptr);
    offset_linkDataStart = read<uint32_t>(&ptr);

    offset_genericTrapHandler = read<uint32_t>(&ptr);

    uint32_t const count_mutableGlobals = read<uint32_t>(&ptr);
    for (uint32_t i = 0; i < count_mutableGlobals; i++) {
      MutableGlobalInfo info{};
      uint32_t const globalIndex = read<uint32_t>(&ptr);
      info.offset_inLinkData = read<uint32_t>(&ptr);
      mutableGlobalInfo[globalIndex] = info;
    }

    uint32_t const count_nonImportedFunctions = read<uint32_t>(&ptr);
    for (uint32_t i = 0; i < count_nonImportedFunctions; i++) {
      NonImportedFunctionInfo info{};
      uint32_t const fncIndex = read<uint32_t>(&ptr);
      info.count_locals = read<uint32_t>(&ptr);
      for (uint32_t j = 0; j < info.count_locals; j++) {
        info.locals_frame_offsets.push_back(read<uint32_t>(&ptr));
      }
      info.count_sourceMap = read<uint32_t>(&ptr);
      ptr = vb::pAddI(ptr, 2 * sizeof(uint32_t) * info.count_sourceMap);
      nonImportedFunctionInfo[fncIndex] = info;
    }
  }

  uint32_t version_debugMap = 0U;
  uint32_t offset_lastFramePtr = 0U;
  uint32_t offset_actualLinMemSize = 0U;
  uint32_t offset_linkDataStart = 0U;

  uint32_t offset_genericTrapHandler = 0U;

  struct MutableGlobalInfo final {
    uint32_t offset_inLinkData;
  };
  std::unordered_map<uint32_t, MutableGlobalInfo> mutableGlobalInfo;

  struct NonImportedFunctionInfo final {
    uint32_t count_locals = 0U;
    std::vector<uint32_t> locals_frame_offsets;
    uint32_t count_sourceMap = 0U;
  };
  std::unordered_map<uint32_t, NonImportedFunctionInfo> nonImportedFunctionInfo;
};
std::unique_ptr<DeserializedDebugMap> desDebugMapPtr;

thread_local vb::Runtime const *runtimePtr;
uint8_t const *getLastFramePtr() {
  uint8_t const *const linearMemoryBase = runtimePtr->unsafe__getLinearMemoryBase();
  return vb::readFromPtr<uint8_t const *>(linearMemoryBase - desDebugMapPtr->offset_lastFramePtr);
}

uint8_t const *getLinkDataStart() {
  uint8_t const *const linearMemoryBase = runtimePtr->unsafe__getLinearMemoryBase();
  return linearMemoryBase - desDebugMapPtr->offset_linkDataStart;
}

struct FrameInfo {
  uint8_t const *framePtr;
  uint8_t const *nextFramePtr;
  uint32_t fncIdx;
  uint32_t offsetToLocals;
  uint32_t posCallerInstr;
};

FrameInfo readFrameInfo(uint8_t const *const framePtr) {
  FrameInfo frameInfo{};
  frameInfo.framePtr = framePtr;
  frameInfo.nextFramePtr = vb::readFromPtr<uint8_t const *>(framePtr);
  frameInfo.fncIdx = vb::readFromPtr<uint32_t>(framePtr + 8);
  frameInfo.offsetToLocals = vb::readFromPtr<uint32_t>(framePtr + 12);
  frameInfo.posCallerInstr = vb::readFromPtr<uint32_t>(framePtr + 16);
  return frameInfo;
}

FrameInfo getFrameInfoForLevel(uint32_t const level) {
  uint8_t const *framePtr = getLastFramePtr();
  for (uint32_t i = 0; i < level; i++) {
    framePtr = readFrameInfo(framePtr).nextFramePtr;
  }
  return readFrameInfo(framePtr);
}

template <typename GlobalType> GlobalType getGlobalValue(uint32_t const globalIndex) {
  uint8_t const *const linkDataStart = getLinkDataStart();
  uint8_t const *const globalPtr = linkDataStart + desDebugMapPtr->mutableGlobalInfo[globalIndex].offset_inLinkData;
  GlobalType const globalValue = vb::readFromPtr<GlobalType>(globalPtr);
  return globalValue;
}

static inline void from_a(void *const ctx) {
  static_cast<void>(ctx);
  uint32_t const global0Value = getGlobalValue<uint32_t>(0U);
  ASSERT_EQ(global0Value, 41U);
  uint32_t const global2Value = getGlobalValue<uint32_t>(2U);
  ASSERT_EQ(global2Value, 103U);

  FrameInfo const l0_frameInfo = getFrameInfoForLevel(0U);
  ASSERT_NE(l0_frameInfo.nextFramePtr, nullptr);
  ASSERT_EQ(l0_frameInfo.fncIdx, 0U);
  ASSERT_EQ(l0_frameInfo.posCallerInstr, 0xB1U); // Caller in Wasm

  FrameInfo const l1_frameInfo = getFrameInfoForLevel(1U);
  ASSERT_NE(l1_frameInfo.nextFramePtr, nullptr);
  ASSERT_EQ(l1_frameInfo.fncIdx, 4U);
  ASSERT_EQ(l1_frameInfo.posCallerInstr, 0x99U); // Caller in Wasm

  uint8_t const *const l1_localStart = l1_frameInfo.framePtr - l1_frameInfo.offsetToLocals;
  auto const l1_fncInfo = desDebugMapPtr->nonImportedFunctionInfo[l1_frameInfo.fncIdx];
  ASSERT_EQ(l1_fncInfo.count_locals, 3U);
  uint32_t const l1_local0 = vb::readFromPtr<uint32_t>(l1_localStart - l1_fncInfo.locals_frame_offsets[0]);
  uint64_t const l1_local1 = vb::readFromPtr<uint64_t>(l1_localStart - l1_fncInfo.locals_frame_offsets[1]);
  uint32_t const l1_local2 = vb::readFromPtr<uint32_t>(l1_localStart - l1_fncInfo.locals_frame_offsets[2]);
  ASSERT_EQ(l1_local0, 14U);
  ASSERT_EQ(l1_local1, 15U);
  ASSERT_EQ(l1_local2, 16U);

  FrameInfo const l2_frameInfo = getFrameInfoForLevel(2U);
  ASSERT_EQ(l2_frameInfo.nextFramePtr, nullptr); // First frame in the call sequence
  ASSERT_EQ(l2_frameInfo.fncIdx, 3U);
  ASSERT_EQ(l2_frameInfo.posCallerInstr, 0U); // First frame has no Wasm caller, called from C++

  uint8_t const *const l2_localStart = l2_frameInfo.framePtr - l2_frameInfo.offsetToLocals;
  auto const l2_fncInfo = desDebugMapPtr->nonImportedFunctionInfo[l2_frameInfo.fncIdx];
  ASSERT_EQ(l2_fncInfo.count_locals, 3U);
  uint32_t const l2_local0 = vb::readFromPtr<uint32_t>(l2_localStart - l2_fncInfo.locals_frame_offsets[0]);
  uint64_t const l2_local1 = vb::readFromPtr<uint64_t>(l2_localStart - l2_fncInfo.locals_frame_offsets[1]);
  uint32_t const l2_local2 = vb::readFromPtr<uint32_t>(l2_localStart - l2_fncInfo.locals_frame_offsets[2]);
  ASSERT_EQ(l2_local0, 11U);
  ASSERT_EQ(l2_local1, 12U);
  ASSERT_EQ(l2_local2, 13U);
}

static inline void from_b(void *const ctx) {
  static_cast<void>(ctx);
  uint32_t const global0Value = getGlobalValue<uint32_t>(0U);
  ASSERT_EQ(global0Value, 41U);
  uint32_t const global2Value = getGlobalValue<uint32_t>(2U);
  ASSERT_EQ(global2Value, 42U);

  FrameInfo const l0_frameInfo = getFrameInfoForLevel(0U);
  ASSERT_NE(l0_frameInfo.nextFramePtr, nullptr);
  ASSERT_EQ(l0_frameInfo.fncIdx, 1U);
  ASSERT_EQ(l0_frameInfo.posCallerInstr, 0xD0U); // Caller in Wasm

  FrameInfo const l1_frameInfo = getFrameInfoForLevel(1U);
  ASSERT_EQ(l1_frameInfo.nextFramePtr, nullptr); // First frame in the call sequence
  ASSERT_EQ(l1_frameInfo.fncIdx, 5U);
  ASSERT_EQ(l1_frameInfo.posCallerInstr, 0U); //  First frame has no Wasm caller, called from C++

  uint8_t const *const l1_localStart = l1_frameInfo.framePtr - l1_frameInfo.offsetToLocals;
  auto const l1_fncInfo = desDebugMapPtr->nonImportedFunctionInfo[l1_frameInfo.fncIdx];
  ASSERT_EQ(l1_fncInfo.count_locals, 2U);
  uint32_t const l1_local0 = vb::readFromPtr<uint32_t>(l1_localStart - l1_fncInfo.locals_frame_offsets[0]);
  uint64_t const l1_local1 = vb::readFromPtr<uint64_t>(l1_localStart - l1_fncInfo.locals_frame_offsets[1]);
  ASSERT_EQ(l1_local0, 0x17181920U);
  ASSERT_EQ(l1_local1, 0x21222324U);
}

static inline void direct(void *const ctx) {
  static_cast<void>(ctx);
  uint32_t const global0Value = getGlobalValue<uint32_t>(0U);
  ASSERT_EQ(global0Value, 41U);
  uint32_t const global2Value = getGlobalValue<uint32_t>(2U);
  ASSERT_EQ(global2Value, 42U);

  FrameInfo const l0_frameInfo = getFrameInfoForLevel(0U);
  ASSERT_EQ(l0_frameInfo.nextFramePtr, nullptr); // Last frame
  ASSERT_EQ(l0_frameInfo.fncIdx, 2U);
  ASSERT_EQ(l0_frameInfo.posCallerInstr, 0U); // First frame in the call sequence
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestDebugInfo, debugInfoIsCorrect) {
  auto staticallyLinkedSymbols =
      vb::make_array(STATIC_LINK("env", "from_a", from_a), STATIC_LINK("env", "from_b", from_b), STATIC_LINK("env", "direct", direct));

  vb::Compiler compiler = vb::Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc, true);
  compiler.enableDebugMode(memoryFnc);

  std::vector<uint8_t> const testModule = vb::test::WabtCmd::loadWasmFromWat(watStr);

  vb::Span<const uint8_t> const bytecode = vb::Span<const uint8_t>(testModule.data(), testModule.size());
  vb::ManagedBinary const binaryModule = compiler.compile(bytecode, staticallyLinkedSymbols);
  vb::ManagedBinary const tmpDebugMap = compiler.retrieveDebugMap();

  std::string const disassembly = vb::disassembler::disassembleDebugMap(tmpDebugMap);

  desDebugMapPtr = std::make_unique<DeserializedDebugMap>(tmpDebugMap);
  ASSERT_EQ(desDebugMapPtr->version_debugMap, 2U);

  vb::ExecutableMemory const executableMemory = vb::ExecutableMemory::make_executable_copy(binaryModule);
  vb::Runtime runtime = vb::test::createRuntime(executableMemory);
  runtimePtr = &runtime;
  runtime.start();
  runtime.getExportedFunctionByName<0>("a")();
  runtime.getExportedFunctionByName<0>("b")();
  runtime.getExportedFunctionByName<0>("direct")();
}

#endif
