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

static std::vector<uint8_t> readWasmFile(std::filesystem::path const &filePath) {
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

static uint8_t *getStackTop() {
  vb::MemUtils::StackInfo const stackInfo{vb::MemUtils::getStackInfo()};
  return vb::pCast<uint8_t *>(stackInfo.stackTop);
}

static volatile uint32_t *pNum{vb::numToP<uint32_t *, uintptr_t>(0U)};

int32_t logInt(uint32_t const data, void *const ctx) {
  static_cast<void>(ctx);
  *pNum = data;
  return 0;
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestNativeFault, testFaultImportFunction) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  std::filesystem::path const wasmPath = getWasmTestCasesDir(getProjectRoot()) / "log.wasm";

  vb::STDCompilerLogger const logger{};

  std::vector<uint8_t> const bytecode{readWasmFile(wasmPath)};

  auto staticallyLinkedSymbols = make_array(STATIC_LINK("env", "log", logInt));
  vb::STDCompilerLogger stdCompilerLogger{};
  vb::WasmModule module(1000000U, stdCompilerLogger, false, nullptr, 0U);
  vb::Span<const uint8_t> const bytecodeSpan{bytecode.data(), bytecode.size()};
  module.initFromBytecode(bytecodeSpan, vb::Span<vb::NativeSymbol const>(staticallyLinkedSymbols.data(), staticallyLinkedSymbols.size()), true);
  std::cout << "Compilation finished." << std::endl;

  module.start(getStackTop());

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-default"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif
  ASSERT_DEATH((module.callExportedFunctionWithName<1U, int32_t>(getStackTop(), "run", 42)), ".*");
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  vb::WasmModule::destroyEnvironment();
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST(TestNativeFault, testFaultWithoutCurrentRuntime) {
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-default"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif
  ASSERT_DEATH((logInt(1, nullptr)), ".*");
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  vb::WasmModule::destroyEnvironment();
}

#endif
