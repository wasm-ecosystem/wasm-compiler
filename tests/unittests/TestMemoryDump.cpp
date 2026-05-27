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

#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

#include "WabtCmd.hpp"

#include "src/WasmModule/WasmModule.hpp"
#include "src/config.hpp"
#include "src/core/common/WasmConstants.hpp"
#include "src/utils/MemUtils.hpp"
#include "src/utils/STDCompilerLogger.hpp"

#if CXX_TARGET == JIT_TARGET
#if ENABLE_EXTENSIONS

static uint8_t *getStackTop() {
  vb::MemUtils::StackInfo const stackInfo{vb::MemUtils::getStackInfo()};
  return vb::pCast<uint8_t *>(stackInfo.stackTop);
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestMemoryDump, DumpMemoryRegionCreatesFile) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  std::filesystem::path const dumpFilePath{"/tmp/wasm_mem_dump_test.bin"};
  std::error_code ec{};
  std::filesystem::remove(dumpFilePath, ec);

  vb::STDCompilerLogger logger{};
  vb::WasmModule wasmModule{logger};

  std::string_view constexpr watStr = R"(
(module
  (import "MemoryDump" "dumpMemoryRegion" (func $dump (param i32 i32)))
  (memory (export "memory") 1)

  (global (export "__data_end") i32 (i32.const 1024))
  (global (export "__heap_base") i32 (i32.const 2048))
  (global (export "__stack_pointer") (mut i32) (i32.const 4096))

  (global i32 (i32.const 1))
  (global (mut i32) (i32.const 2))
  (global (mut f32) (f32.const 3.140000104904175))

  (data (i32.const 0) "/tmp/wasm_mem_dump_test.bin")

  (func (export "_start")
    (call $dump (i32.const 0) (i32.const 27))
  )
)
)";

  std::vector<uint8_t> const bytecode{vb::test::WabtCmd::loadWasmFromWat(watStr)};
  wasmModule.initFromBytecode(vb::Span<const uint8_t>(bytecode.data(), bytecode.size()), vb::Span<vb::NativeSymbol const>{}, true);

  uint8_t const *const stackTop{getStackTop()};
  wasmModule.start(stackTop);
  wasmModule.callExportedFunctionWithName<0>(stackTop, "_start");

  ASSERT_TRUE(std::filesystem::exists(dumpFilePath));

  // Verify dump file format
  std::ifstream inFile(dumpFilePath, std::ios::in | std::ios::binary | std::ios::ate);
  ASSERT_TRUE(inFile.is_open());

  // The WAT module declares (memory 1) = 1 page = 65536 bytes
  // Header is magic(4) + version(4) + dataEnd(4) + heapBase(4) + stackPointer(4)
  // + numMutableI32Globals(4) + mutableI32Globals(2 * 4)
  constexpr uint32_t numMutableI32GlobalsExpected{2U};
  constexpr uint32_t headerSize{24U + (numMutableI32GlobalsExpected * sizeof(uint32_t))};
  constexpr uint32_t expectedLinearMemorySize{1U * vb::WasmConstants::wasmPageSize};
  constexpr uint32_t expectedFileSize{headerSize + expectedLinearMemorySize};

  std::streamsize const fileSize{inFile.tellg()};
  ASSERT_EQ(static_cast<uint32_t>(fileSize), expectedFileSize);

  inFile.seekg(0, std::ios::beg);

  // Read and verify magic number
  std::array<char, 4> magic{};
  inFile.read(magic.data(), magic.size());
  EXPECT_EQ(magic[0], 'A');
  EXPECT_EQ(magic[1], 'S');
  EXPECT_EQ(magic[2], 'H');
  EXPECT_EQ(magic[3], 'D');

  // Read and verify version
  uint32_t version{0U};
  inFile.read(vb::pCast<char *>(&version), sizeof(version));
  EXPECT_EQ(version, 2U);

  // Read and verify dataEnd (WAT: i32.const 1024)
  uint32_t dataEnd{0U};
  inFile.read(vb::pCast<char *>(&dataEnd), sizeof(dataEnd));
  EXPECT_EQ(dataEnd, 1024U);

  // Read and verify heapBase (WAT: i32.const 2048)
  uint32_t heapBase{0U};
  inFile.read(vb::pCast<char *>(&heapBase), sizeof(heapBase));
  EXPECT_EQ(heapBase, 2048U);

  // Read and verify stackPointer (WAT: i32.const 4096)
  uint32_t stackPointer{0U};
  inFile.read(vb::pCast<char *>(&stackPointer), sizeof(stackPointer));
  EXPECT_EQ(stackPointer, 4096U);

  // Read and verify mutableI32Globals metadata
  uint32_t numMutableI32Globals{0U};
  inFile.read(vb::pCast<char *>(&numMutableI32Globals), sizeof(numMutableI32Globals));
  EXPECT_EQ(numMutableI32Globals, numMutableI32GlobalsExpected);

  std::vector<uint32_t> mutableI32Globals(static_cast<size_t>(numMutableI32Globals), 0U);
  inFile.read(vb::pCast<char *>(mutableI32Globals.data()), static_cast<std::streamsize>(mutableI32Globals.size() * sizeof(uint32_t)));
  std::vector<uint32_t> const expectedMutableI32Globals{4096U, 2U};
  EXPECT_EQ(mutableI32Globals, expectedMutableI32Globals);

  // Verify raw linear memory starts with the file path string written by (data (i32.const 0) ...)
  constexpr std::string_view expectedDataStr{"/tmp/wasm_mem_dump_test.bin"};
  constexpr size_t expectedDataLen{expectedDataStr.size()};
  std::array<char, expectedDataLen> memoryHead{};
  inFile.read(memoryHead.data(), static_cast<std::streamsize>(expectedDataLen));
  EXPECT_EQ(std::memcmp(memoryHead.data(), expectedDataStr.data(), expectedDataLen), 0);

  vb::WasmModule::destroyEnvironment();
  std::filesystem::remove(dumpFilePath, ec);
}

#endif // ENABLE_EXTENSIONS
#endif // CXX_TARGET == JIT_TARGET
