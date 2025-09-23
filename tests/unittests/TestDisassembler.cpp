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
#include "disassembler/disassembler.hpp"

#include "src/core/compiler/Compiler.hpp"

namespace vb {

static const char *const watStr = R"(
  (module
  (type (;0;) (func))
  (import "env" "import1" (func (;0;) (type 0)))
  (func (;1;) (type 0)
    call 0)
  (func (;2;) (type 0)
    i32.const 0
    call_indirect (type 0))
  (table (;0;) 1 funcref)
  (export "reexport_import1" (func 0))
  (export "call_direct_import1" (func 1))
  (export "call_indirect_import1" (func 2))
  (elem (;0;) (i32.const 0) func 0)
)
)";
// NOLINTNEXTLINE(cert-err58-cpp)
std::vector<uint8_t> const testModule = vb::test::WabtCmd::loadWasmFromWat(watStr);

Span<const uint8_t> const bytecode = Span<const uint8_t>(testModule.data(), testModule.size());

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
TEST(TestDisassembler, canDisassembleWasmModule) {
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc, true);
  ManagedBinary const binaryModule = compiler.compile(bytecode);
  std::string const disassembly = disassembler::disassemble(binaryModule, {});

  EXPECT_NE(disassembly.find("WebAssembly Function Bodies"), std::string::npos);
  EXPECT_NE(disassembly.find("Initial Linear Memory Data"), std::string::npos);
  EXPECT_NE(disassembly.find("Function Names"), std::string::npos);
  EXPECT_NE(disassembly.find("Start Function"), std::string::npos);
  EXPECT_NE(disassembly.find("Mutable Non-Exported Globals"), std::string::npos);
  EXPECT_NE(disassembly.find("Dynamically Imported Functions"), std::string::npos);
  EXPECT_NE(disassembly.find("Linear Memory"), std::string::npos);
  EXPECT_NE(disassembly.find("Exported Globals"), std::string::npos);
  EXPECT_NE(disassembly.find("Exported Functions"), std::string::npos);
  EXPECT_NE(disassembly.find("WebAssembly Link Status of Imported Functions"), std::string::npos);
  EXPECT_NE(disassembly.find("WebAssembly Table"), std::string::npos);
  EXPECT_NE(disassembly.find("More Info"), std::string::npos);
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestDisassembler, canDisassembleDebugMap) {
#if !defined(JIT_TARGET_TRICORE)
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc, true);
  compiler.enableDebugMode(memoryFnc);
  ManagedBinary const binaryModule = compiler.compile(bytecode);
  ManagedBinary const debugMap = compiler.retrieveDebugMap();
  std::string const disassembly = disassembler::disassembleDebugMap(debugMap);

  EXPECT_NE(disassembly.find("Offset of lastFramePtr"), std::string::npos);
  EXPECT_NE(disassembly.find("Offset of actualLinMemSize"), std::string::npos);
  EXPECT_NE(disassembly.find("Offset of linkDataStart "), std::string::npos);
  EXPECT_NE(disassembly.find("Offset of genericTrapHandler"), std::string::npos);
  EXPECT_NE(disassembly.find("Number of non-imported mutable globals"), std::string::npos);
  EXPECT_NE(disassembly.find("Number of non-imported functions"), std::string::npos);
  EXPECT_NE(disassembly.find("Wasm function index"), std::string::npos);
  EXPECT_NE(disassembly.find("Number of locals for this function"), std::string::npos);
  EXPECT_NE(disassembly.find("Number of machine code entries"), std::string::npos);
  EXPECT_NE(disassembly.find("In, out offsets"), std::string::npos);
#endif
}
} // namespace vb
