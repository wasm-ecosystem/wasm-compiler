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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/BinaryModule.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/STDCompilerLogger.hpp"
#include "src/utils/StackTop.hpp"

#if defined(_MSC_VER)
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif

#ifndef __AFL_FUZZ_TESTCASE_LEN
ssize_t fuzz_len;
#define AFL_ENABLED 0
#define __AFL_FUZZ_TESTCASE_LEN fuzz_len
std::array<uint8_t, 1024000> fuzz_buf;
#define __AFL_FUZZ_TESTCASE_BUF fuzz_buf.data()
#define __AFL_LOOP(x) ((fuzz_len = static_cast<ssize_t>(fread(fuzz_buf.data(), sizeof(fuzz_buf), 1, snapshot))) > 0 ? 1 : 0)
#define __AFL_INIT() sync()
// #define LLVMFuzzerTestOneInput() main()
#else
#define AFL_ENABLED 1
__AFL_FUZZ_INIT();
#endif

namespace FuzzingSupport {
void logI32(uint32_t value, void *const ctx) {
  static_cast<void>(ctx);
  static_cast<void>(value);
}
void logI64(uint64_t value, void *const ctx) {
  static_cast<void>(ctx);
  static_cast<void>(value);
}
void logF32(float value, void *const ctx) {
  static_cast<void>(ctx);
  static_cast<void>(value);
}
void logF64(double value, void *const ctx) {
  static_cast<void>(ctx);
  static_cast<void>(value);
}
} // namespace FuzzingSupport

std::mt19937 mt; // NOLINT(cert-err58-cpp)
void generateFuzzInputArguments(uint8_t *const output, uint32_t length) {
  for (uint32_t i = 0; i < length; i++) {
    *(output + i) = mt() & 0xFFU;
  }
}

void iterateExportedFunctions(
    vb::Span<uint8_t const> const binaryModule,
    std::function<void(char const *exportName, uint32_t nameLength, uint8_t const *signature, uint32_t signatureLength)> lambda) {
  vb::BinaryModule binaryModuleParser{};
  binaryModuleParser.init(binaryModule);

  uint8_t const *stepPtr = binaryModuleParser.getExportedFunctionsEnd();

  uint32_t const numExportedFunctions = vb::readNextValue<uint32_t>(&stepPtr);

  for (uint32_t i = 0; i < numExportedFunctions; i++) {
    uint32_t const fncIndex = vb::readNextValue<uint32_t>(&stepPtr);
    static_cast<void>(fncIndex);

    uint32_t const exportNameLength = vb::readNextValue<uint32_t>(&stepPtr);
    stepPtr -= vb::roundUpToPow2(exportNameLength, 2);
    char const *const exportName = vb::pCast<char const *>(stepPtr);

    uint32_t const signatureLength = vb::readNextValue<uint32_t>(&stepPtr);
    stepPtr -= vb::roundUpToPow2(signatureLength, 2);

    uint8_t const *const signature = stepPtr;

    lambda(exportName, exportNameLength, signature, signatureLength);

    uint32_t const functionCallWrapperSize = vb::readNextValue<uint32_t>(&stepPtr);
    stepPtr -= vb::roundUpToPow2(functionCallWrapperSize, 2);
  }
}

std::atomic<int64_t> wasmFunctionStartTime{INT64_MAX};
std::atomic<bool> finished = false;
std::mutex executionTimeMutex;
std::condition_variable executionTimeCv;

vb::WasmModule *pWasmModule{nullptr};

static int64_t getCurrentTime() {
  int64_t const now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  return now;
}

int main(int argc, const char *argv[]) {
#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_INIT();
#endif
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);

  const auto dynamicLinkedSymbols = vb::make_array(
      DYNAMIC_LINK("fuzzing-support", "log-i32", FuzzingSupport::logI32), DYNAMIC_LINK("fuzzing-support", "log-i64", FuzzingSupport::logI64),
      DYNAMIC_LINK("fuzzing-support", "log-f32", FuzzingSupport::logF32), DYNAMIC_LINK("fuzzing-support", "log-f64", FuzzingSupport::logF64));

  std::thread executionTimeWatchDogThread([] {
    int64_t constexpr maxExecutionTime = 50U;
    while (!finished) {
      std::unique_lock<std::mutex> lock(executionTimeMutex);
      executionTimeCv.wait_for(lock, std::chrono::milliseconds(maxExecutionTime));
      int64_t const currentTime = getCurrentTime();
      if (currentTime - wasmFunctionStartTime > maxExecutionTime) {
        pWasmModule->requestInterruption(vb::TrapCode::RUNTIME_INTERRUPT_REQUESTED);
      }
    }
  });

#if AFL_ENABLED
  uint8_t *buf = __AFL_FUZZ_TESTCASE_BUF; // must be after __AFL_INIT and before __AFL_LOOP!
  while (__AFL_LOOP(1000)) {
    ssize_t const len = __AFL_FUZZ_TESTCASE_LEN; // don't use the macro directly in a call!
    if (len < 8) {
      continue; // check for a required/useful minimum input length
    }
#else
  FILE *snapshot;
  if (argc > 1) {
    snapshot = fopen(argv[1], "r");
  } else {
    exit(0);
  }
  uint8_t const *const buf = fuzz_buf.data();
  ssize_t const len = static_cast<ssize_t>(fread(fuzz_buf.data(), 1, sizeof(fuzz_buf), snapshot));
#endif

    vb::Span<uint8_t const> const bytecode(buf, static_cast<uint32_t>(len));

    try {
      vb::STDCompilerLogger logger{};
      vb::WasmModule wasmModule{logger};
      pWasmModule = &wasmModule;
      vb::Span<vb::NativeSymbol const> const symbols{dynamicLinkedSymbols.data(), dynamicLinkedSymbols.size()};

      vb::WasmModule::CompileResult const compileResult{wasmModule.compile(bytecode, symbols)};
      wasmModule.initFromCompiledBinary(compileResult.getModule().span(), symbols, compileResult.getDebugSymbol().span());
      uint8_t const *const stackTop{vb::pCast<uint8_t const *>(vb::getStackTop())};

      wasmFunctionStartTime = getCurrentTime();
      try {
        wasmModule.start(stackTop);
      } catch (vb::TrapException &e) {
        if (e.getTrapCode() == vb::TrapCode::STACKFENCEBREACHED) {
          std::cout << "Module start function Stackoverflow" << std::endl;
        } else {
          throw; // Trap
        }
      }

      {
        using namespace vb;
        mt = std::mt19937(static_cast<std::mt19937::result_type>(42_U64 * compileResult.getModule().size()));
      }

      iterateExportedFunctions(compileResult.getModule().span(), [&wasmModule, stackTop](char const *exportName, uint32_t nameLength,
                                                                                         uint8_t const *signature, uint32_t signatureLength) {
        std::vector<uint8_t> serializationData;
        std::vector<uint8_t> results;
        try {
          uint32_t numArgs = 0;
          for (uint32_t i = 0; i < signatureLength; i++) {
            if (*(signature + i) == static_cast<uint8_t>(vb::SignatureType::PARAMEND)) {
              numArgs = i - 1;
              break;
            }
            if (i == signatureLength - 1) {
              exit(1); // Error
            }
          }

          uint32_t const numReturnValues = signatureLength - numArgs - 2;
          if (numReturnValues > 0) {
            results.resize(numReturnValues * 8);
          }
          if (numArgs > 0) {
            serializationData.resize(numArgs * 8);
            generateFuzzInputArguments(serializationData.data(), numArgs * 8);
          }
          wasmFunctionStartTime = getCurrentTime();

          wasmModule.callRawExportedFunctionByName(vb::Span<char const>(exportName, nameLength), stackTop, serializationData.data(), results.data());

        } catch (vb::TrapException &e) {
          if (e.getTrapCode() == vb::TrapCode::STACKFENCEBREACHED) {
            std::cout << "Module start function Stackoverflow" << std::endl;
          } else {
            throw; // Trap
          }
        }
      });
    } catch (const std::exception &e) { // Trap or other exception
      static_cast<void>(e);
    }
    wasmFunctionStartTime = INT64_MAX;
#if AFL_ENABLED
  }
#endif
  finished = true;
  executionTimeCv.notify_one();
  executionTimeWatchDogThread.join();

  vb::WasmModule::destroyEnvironment();
  return 0;
}
