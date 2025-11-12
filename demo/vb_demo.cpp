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

#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <vector>

#include "src/config.hpp"
//
#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/BinaryModule.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/STDCompilerLogger.hpp"
#include "src/utils/StackTop.hpp"
#ifdef VB_WIN32_OR_POSIX
#include <fstream>

#include "src/utils/MemUtils.hpp"
std::vector<uint8_t> bytecode;
static vb::Span<const uint8_t> loadWasmBytecode(std::string const &filePath) {
  std::cout << "Loading file ..." << std::endl;

  std::ifstream file(filePath, std::ios::binary);

  // Check if the file was opened successfully
  if (!file) {
    throw std::runtime_error("Failed to open file: " + filePath);
  }

  // Read the file contents into a vector
  bytecode = std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  return vb::Span<const uint8_t>(bytecode.data(), bytecode.size());
}

static uint8_t const *getStackTop() {
  vb::MemUtils::StackInfo const stackInfo{vb::MemUtils::getStackInfo()};
  uint8_t const *const stackTop{vb::pCast<uint8_t *>(stackInfo.stackTop)};
  return stackTop;
}

#else
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
uint8_t const bcArr[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x0b,
    0x01, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x6c, 0x6f, 0x67, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0x07, 0x07, 0x01,
    0x03, 0x72, 0x75, 0x6e, 0x00, 0x01, 0x0a, 0x08, 0x01, 0x06, 0x00, 0x20, 0x00, 0x10, 0x00, 0x0b,
};

vb::Span<const uint8_t> loadWasmBytecode(std::string const & /*filePath*/) {
  return vb::Span<const uint8_t>(bcArr, sizeof(bcArr));
}

static uint8_t *getStackTop() {
  return nullptr;
}
#endif

int32_t logInt(uint32_t const data, void *const ctx) {
  static_cast<void>(ctx);
  std::cout << "wasm log " << data << std::endl;
  return 0;
}

int main(int argc, char *argv[]) {
#ifdef VB_WIN32_OR_POSIX
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <wasm_file>" << std::endl;
    return EXIT_FAILURE;
  }
#else
  static_cast<void>(argc);
#endif
  try {
    vb::Span<const uint8_t> const bytecodeSpan = loadWasmBytecode(argv[1]);

    vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

    auto staticallyLinkedSymbols = make_array(STATIC_LINK("env", "log", logInt));
    vb::STDCompilerLogger stdCompilerLogger{};
    vb::WasmModule module(1000000U, stdCompilerLogger, false, nullptr, 0U);

    module.initFromBytecode(bytecodeSpan, vb::Span<vb::NativeSymbol const>(staticallyLinkedSymbols.data(), staticallyLinkedSymbols.size()), true);
    std::cout << "Compilation finished." << std::endl;

    module.start(getStackTop());

    std::array<vb::WasmValue, 1U> const res{module.callExportedFunctionWithName<1U, int32_t>(getStackTop(), "run", 42)};
    std::cout << "Result: " << res[0].i32 << std::endl;
  } catch (vb::TrapException &e) {
    std::cout << e.what() << ": " << static_cast<uint32_t>(e.getTrapCode()) << std::endl;
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  vb::WasmModule::destroyEnvironment();
  return 0;
}
