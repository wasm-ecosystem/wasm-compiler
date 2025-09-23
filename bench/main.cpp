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
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>

#include "src/config.hpp"
//
#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/BinaryModule.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/STDCompilerLogger.hpp"
#include "src/utils/StackTop.hpp"

#if INTERRUPTION_REQUEST
static_assert(false, "Interruption request turned on. This will impact performance. Do not use this setting for performance benchmarks when "
                     "comparing to other runtimes! (They do not have this feature)");
#endif

#if !EAGER_ALLOCATION
static_assert(false, "Eager allocation NOT turned on. This will impact performance. Always use this setting for performance benchmarks when "
                     "comparing to other runtimes! (They all act like that)");
#endif

using Clock = std::chrono::high_resolution_clock;
using namespace vb;

Span<const uint8_t> loadWasmFile(char const *const path) {
  FILE *filePtr;
  uint8_t *buffer;
  uint64_t length;

  filePtr = fopen(path, "rb");
  if (filePtr == nullptr) {
    std::cout << "File not found. Aborting." << std::endl;
    exit(0);
  }

  fseek(filePtr, 0, SEEK_END);
  length = static_cast<uint64_t>(ftell(filePtr));
  assert(length < INT32_MAX);
  rewind(filePtr);
  buffer = vb::pCast<uint8_t *>(malloc(static_cast<size_t>(length) * sizeof(uint8_t)));
  size_t const readSize = fread(buffer, 1, static_cast<size_t>(length), filePtr);

  if (readSize != length) {
    perror("read file error");
    std::exit(0);
  }

  fclose(filePtr);

  return Span<const uint8_t>(buffer, static_cast<uint32_t>(length));
}

int main(int argc, char *argv[]) {
  bool const shouldExecute = argc > 2;
  if (argc < 2) {
    std::cout << "No file specified. Aborting" << std::endl;
    exit(0);
  }

  try {
    Span<const uint8_t> const bytecode = loadWasmFile(argv[1]);

    vb::WasmModule::initEnvironment(&malloc, &realloc, &free);
    vb::STDCompilerLogger stdCompilerLogger{};
    vb::WasmModule module(UINT64_MAX, stdCompilerLogger, false, nullptr, 0U);
    auto const compstart = std::chrono::steady_clock::now();
    WasmModule::CompileResult const compileResult{module.compile(bytecode, vb::Span<vb::NativeSymbol const>())};
    auto const compend = std::chrono::steady_clock::now();
    float const compdur = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(compend - compstart).count()) / 1000.F;

    float execdur = 0;

    if (shouldExecute) {
      char const *const fncName = argv[2];
      uint8_t const *stackTop;
      auto const execstart = std::chrono::steady_clock::now();
#if ACTIVE_STACK_OVERFLOW_CHECK
      stackTop = getStackTop();
#else
      stackTop = nullptr;
#endif
      module.initFromCompiledBinary(compileResult.getModule().span(), vb::Span<vb::NativeSymbol const>(), vb::Span<uint8_t const>());

      module.start(stackTop);
      std::array<vb::WasmValue, 1U> const res{module.callExportedFunctionWithName<1U, int32_t, int32_t>(stackTop, fncName, 0, 0)};

      auto const execend = std::chrono::steady_clock::now();
      std::cout << std::fixed << std::setprecision(2) << "RES " << res[0].f64 << std::endl;

      execdur = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(execend - execstart).count()) / 1000.F;
    }

    //
    //
    //

    std::cout << std::endl;
    float const compPerc = 100.F * compdur / (execdur + compdur);
    float const execPerc = 100.F * execdur / (execdur + compdur);
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Total time (ms): " << (compdur + execdur) << std::endl;
    std::cout << "Compilation time (ms): " << compdur << " (" << compPerc << "%)" << std::endl;
    if (shouldExecute) {
      std::cout << "Execution time (ms): " << execdur << " (" << execPerc << "%)" << std::endl;
    }
    std::cout << std::endl;
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }
  return 0;
}
