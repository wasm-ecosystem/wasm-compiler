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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>

#include "dbg_fuzz.hpp"

#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/BinaryModule.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/STDCompilerLogger.hpp"
#include "src/utils/StackTop.hpp"

constexpr uint32_t fuzzHelperBufferSize = 100000U;

GDB_FUZZ_INPUT_BINARY_INIT(fuzzHelperBufferSize) // NOLINT(modernize-avoid-c-arrays)
GDB_FUZZ_OUTPUT(fuzzHelperBufferSize)            // NOLINT(modernize-avoid-c-arrays)
std::array<uint8_t, fuzzHelperBufferSize> safeInput;

std::array<uint64_t, fuzzHelperBufferSize / 8U> largeBuffer;
uint8_t *buffer = vb::pCast<uint8_t *>(largeBuffer.data());

extern "C" {
// Do something to avoid this function is optimized out by compiler
int32_t GDB_FUZZ_UPDATE(int32_t x) noexcept {
  return x + 1;
}
}

void basic_memcpy(uint8_t *dest, const volatile uint8_t *src, uint32_t len) noexcept {
  for (uint32_t i = 0; i < len; i++) {
    dest[i] = src[i];
  }
}

constexpr bool logDetailsAndExitOnFirstError = false;

#ifndef __CPTC__

vb::WasmModule::ReallocFunction reallocFunction{&std::realloc};
vb::WasmModule::MallocFunction mallocFunction{&std::malloc};
vb::WasmModule::FreeFunction freeFunction{&std::free};
alignas(8) std::array<uint8_t, 1024U * 300U> binaryMemoryRegion{};

#elif (CXX_TARGET == ISA_TRICORE)

alignas(8) std::array<uint8_t, 1024U * 290U> jobMemoryRegion{};

alignas(8) std::array<uint8_t, 1024U * 200U> binaryMemoryRegion{};

bool regionInUse1{false};
bool regionInUse2{false};

void *reallocDispatch(void *const ptr, size_t const size) {
  static_cast<void>(size);
  if (ptr == nullptr) {
    if (!regionInUse1) {
      regionInUse1 = true;
      return jobMemoryRegion.data();
    } else if (!regionInUse2) {
      regionInUse2 = true;
      return binaryMemoryRegion.data();
    } else {
      assert(false && "No memory region available for realloc");
    }
  } else if (ptr == static_cast<void *>(jobMemoryRegion.data())) {
    if (size > jobMemoryRegion.size()) {
      assert(false && "Reallocating job memory region to larger size than available");
    }

  } else if (ptr == static_cast<void *>(binaryMemoryRegion.data())) {
    if (size > binaryMemoryRegion.size()) {
      assert(false && "Reallocating binary memory region to larger size than available");
    }
  } else {
    assert(false && "Reallocating memory region that is not job or binary memory region");
  }

  return ptr;
}

void freeDispatch(void *const ptr) {
  if (ptr == static_cast<void *>(jobMemoryRegion.data())) {
    regionInUse1 = false;
  } else if (ptr == static_cast<void *>(binaryMemoryRegion.data())) {
    regionInUse2 = false;
  } else {
    free(ptr);
  }
}

vb::WasmModule::MallocFunction mallocFunction{&std::malloc};
vb::WasmModule::ReallocFunction reallocFunction{&reallocDispatch};
vb::WasmModule::FreeFunction freeFunction{&freeDispatch};
#endif

uint32_t functionsExecuted = 0;

class DeferredLineCounter {
public:
  void push_back() noexcept {
    deferredLinesBack++;
  }
  uint32_t size() noexcept {
    return deferredLinesBack - deferredLinesFront;
  }
  void erase_begin() noexcept {
    deferredLinesFront++;
  }
  void clear() noexcept {
    deferredLinesBack = 0;
    deferredLinesFront = 0;
  }

private:
  uint32_t deferredLinesBack = 0;
  uint32_t deferredLinesFront = 0;
};

DeferredLineCounter deferredLines;

struct ExpectedData {
  std::string type;
  std::string value;
};

bool next = false;

static void executionFailed(std::string const &message) noexcept {
  if (GDB_FUZZ_ITERATION_FAILED) {
    return;
  }

  next = true;
  deferredLines.clear();

  std::cout << "FAILED due to: " << message << std::endl;

  static_cast<void>(memset(VBHELPER_GDB_FUZZ_OUTPUT_MESSAGE, 0, static_cast<size_t>(sizeof(VBHELPER_GDB_FUZZ_OUTPUT_MESSAGE))));

  VBHELPER_GDB_FUZZ_OUTPUT_MESSAGE_SIZE =
      std::min(static_cast<uint32_t>(message.size()), static_cast<uint32_t>(sizeof(VBHELPER_GDB_FUZZ_OUTPUT_MESSAGE)));
  memcpy(VBHELPER_GDB_FUZZ_OUTPUT_MESSAGE, message.data(), static_cast<size_t>(VBHELPER_GDB_FUZZ_OUTPUT_MESSAGE_SIZE));

  GDB_FUZZ_ITERATION_FAILED = true;

  //   if (logDetailsAndExitOnFirstError) { exit(0); }
}

template <typename T> static T normalizeNan(T const value) noexcept {
  return value;
}

template <> float normalizeNan<float>(float const value) noexcept {
  if (std::isnan(value)) {
    uint32_t raw;

    raw = vb::bit_cast<uint32_t>(value) & (~0x8000'0000U);

    return vb::bit_cast<float>(raw);
  }
  return value;
}

template <> double normalizeNan<double>(double const value) noexcept {
  if (std::isnan(value)) {
    uint64_t raw;

    raw = vb::bit_cast<uint64_t>(value) & (~0x8000'0000'0000'0000LLU);

    return vb::bit_cast<double>(raw);
  }
  return value;
}

template <typename Type> void serializeNumToOutput(Type num) noexcept {
  static_assert(std::is_arithmetic<Type>::value, "Argument type can only be arithmetic type");
  num = normalizeNan(num);
  uint32_t const sizeAfterCopy = VBHELPER_GDB_FUZZ_OUTPUT_RESULT_LENGTH + static_cast<uint32_t>(sizeof(Type));

  assert(sizeAfterCopy <= fuzzHelperBufferSize);

  static_cast<void>(
      memcpy(vb::pAddI(&VBHELPER_GDB_FUZZ_OUTPUT_RESULT[0], VBHELPER_GDB_FUZZ_OUTPUT_RESULT_LENGTH), &num, static_cast<size_t>(sizeof(Type))));

  VBHELPER_GDB_FUZZ_OUTPUT_RESULT_LENGTH = sizeAfterCopy;
}

template <typename Type> void logHelper(Type const value, char const *const formatStr) noexcept {
  if (deferredLines.size() == 0) {
    std::stringstream ss;
    ss << "No log expected, log called with format str" << formatStr << "value: " << value << std::endl;

    executionFailed(ss.str());
  } else {
    serializeNumToOutput(value);
    deferredLines.erase_begin();
  }
}

namespace FuzzingSupport {
void logI32(uint32_t const value, void *const ctx) noexcept {
  static_cast<void>(ctx);
  logHelper(value, "called host fuzzing-support.log-i32(i32:%.*s) =>");
}
void logI64(uint64_t const value, void *const ctx) noexcept {
  static_cast<void>(ctx);
  logHelper(value, "called host fuzzing-support.log-i64(i64:%.*s) =>");
}
void logF32(float const value, void *const ctx) noexcept {
  static_cast<void>(ctx);
  logHelper(value, "called host fuzzing-support.log-f32(f32:%.*s) =>");
}
void logF64(double const value, void *const ctx) noexcept {
  static_cast<void>(ctx);
  logHelper(value, "called host fuzzing-support.log-f64(f64:%.*s) =>");
}

void callExport(uint32_t param1, void *const ctx) {
  static_cast<void>(ctx);
  if (deferredLines.size() == 0) {
    executionFailed("No log expected, but called host fuzzing-support.call-export(i32:" + std::to_string(param1) + ") =>");
  } else {
    serializeNumToOutput(param1);
    deferredLines.erase_begin();
  }
}

uint32_t sleep(uint32_t param1, uint32_t param2, void *const ctx) {
  static_cast<void>(ctx);
  if (deferredLines.size() == 0) {
    executionFailed("No log expected, but called host fuzzing-support.sleep(i32:" + std::to_string(param1) + ", i32:" + std::to_string(param2) +
                    ") => i32:0");
  } else {
    serializeNumToOutput(param1);
    serializeNumToOutput(param2);
    serializeNumToOutput(static_cast<uint32_t>(0U));
    deferredLines.erase_begin();
  }
  return 0;
}

uint32_t callExportCatch(uint32_t param1, void *const ctx) {
  static_cast<void>(ctx);
  if (deferredLines.size() == 0) {
    executionFailed("No log expected, but called host fuzzing-support.call-export-catch(i32:" + std::to_string(param1) + ") => i32:0");
  } else {
    serializeNumToOutput(param1);
    serializeNumToOutput(static_cast<uint32_t>(0U));
    deferredLines.erase_begin();
  }
  return 0;
}
} // namespace FuzzingSupport

static void handleLine(char const *const lineStart, uint32_t const lineLength, uint8_t const *const stackTop, vb::WasmModule &wasmModule) {
  if (strncmp(lineStart, "called host", 11U) == 0) {
    deferredLines.push_back();
  } else {
    // hashMemory() => i32:3184230149
    char const *const funcNameEnd = vb::pCast<char const *>(memchr(lineStart, '(', lineLength));
    size_t const funcNameLength = static_cast<size_t>(funcNameEnd - lineStart);
    char const *const arrowPos = vb::pCast<char const *>(memchr(lineStart, '>', lineLength)) - 1;
    vb::Span<char const> const functionName{vb::Span<char const>(lineStart, static_cast<size_t>(funcNameLength))};
    if (arrowPos != nullptr) {
      size_t const arrowOffset = static_cast<size_t>(arrowPos - lineStart);
      if (lineLength >= arrowOffset + 5) {
        // Has return values or error
        char const *const returnTypeStart = arrowPos + 3;
        if (strncmp(returnTypeStart, "err", 3U) == 0) {
          try {
            vb::Span<char const> const functionSignature{wasmModule.getFunctionSignatureByName(functionName)};
            size_t const numReturnValues{functionSignature.size() - 2};
            std::vector<uint8_t> results(numReturnValues * 8);
            wasmModule.callRawExportedFunctionByName(functionName, stackTop, nullptr, results.data());
            std::stringstream ss;
            ss << "Trap expected, but did not occur. Exiting.\n";
            executionFailed(ss.str());
          } catch (vb::TrapException &trapException) { // SUCCESS
            static_cast<void>(trapException);
          }
        } else {
          std::vector<ExpectedData> expectedReturnValues;
          char const *const returnValuesPtr = arrowPos + 2;
          size_t const returnValuesLength = lineLength - arrowOffset - 2;
          std::string const returnValues(returnValuesPtr, returnValuesLength);
          std::istringstream returnValuesStream(returnValues);
          for (std::string token; getline(returnValuesStream, token, ',');) {
            std::string const type = token.substr(1, 3);
            size_t const valueStart = token.find(':') + 1;
            std::string const value = token.substr(valueStart, token.size() - valueStart);
            expectedReturnValues.push_back({type, value});
          }

          std::vector<uint8_t> results(expectedReturnValues.size() * 8);
          wasmModule.callRawExportedFunctionByName(functionName, stackTop, nullptr, results.data());

          uint8_t const *resultPtr = results.data();
          for (ExpectedData const &expected : expectedReturnValues) {
            std::string const expectedType = expected.type;
            std::string const expectedValue = expected.value;
            if (expectedType == "i32") {
              uint32_t actualValue = 0U;
              std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
              serializeNumToOutput(actualValue);
            } else if (expectedType == "i64") {
              uint64_t actualValue = 0U;
              std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
              serializeNumToOutput(actualValue);
            } else if (expectedType == "f32") {
              float actualValue = 0U;
              std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
              serializeNumToOutput(actualValue);
            } else if (expectedType == "f64") {
              double actualValue = 0U;
              std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
              serializeNumToOutput(actualValue);
            } else {
              assert(false && "Unreachable");
            }
            resultPtr += 8;
          }
        }

      } else {
        // No return values expected
        wasmModule.callRawExportedFunctionByName(functionName, stackTop, nullptr, nullptr);
      }
      functionsExecuted++;
    } else {
      if (logDetailsAndExitOnFirstError) {
        std::cout.write(lineStart, static_cast<std::streamsize>(lineLength));
        std::cout << std::endl;
      }
      throw std::runtime_error("Fail");
    }
  }
}

static void resetGlobalFlagsBeforeFuzz() noexcept {
  VBHELPER_GDB_FUZZ_OUTPUT_RESULT_LENGTH = 0;
}

static void fuzz(uint8_t const *const stackTop) noexcept {
  deferredLines.clear();
  int32_t const res = GDB_FUZZ_UPDATE(5);
  resetGlobalFlagsBeforeFuzz();
  static_cast<void>(res);
  basic_memcpy(safeInput.data(), GDB_FUZZ_INPUT_BINARY, GDB_FUZZ_INPUT_BINARY_ACTUAL_LENGTH);

  uint8_t const *const refOutputStart = safeInput.data();
  uint32_t const refOutputLength = GDB_FUZZ_INPUT_REFOUTPUT_LENGTH;
  uint8_t const *const bytecodeStart = safeInput.data() + refOutputLength;
  uint32_t const bytecodeLength = GDB_FUZZ_INPUT_BINARY_ACTUAL_LENGTH - refOutputLength;
  vb::Span<uint8_t const> const bytecode = vb::Span<uint8_t const>(bytecodeStart, bytecodeLength);

  const auto linkedSymbols = vb::make_array(
      DYNAMIC_LINK("fuzzing-support", "log-i32", FuzzingSupport::logI32), DYNAMIC_LINK("fuzzing-support", "log-i64", FuzzingSupport::logI64),
      DYNAMIC_LINK("fuzzing-support", "log-f32", FuzzingSupport::logF32), DYNAMIC_LINK("fuzzing-support", "log-f64", FuzzingSupport::logF64),
      DYNAMIC_LINK("fuzzing-support", "call-export", FuzzingSupport::callExport), DYNAMIC_LINK("fuzzing-support", "sleep", FuzzingSupport::sleep),
      DYNAMIC_LINK("fuzzing-support", "call-export-catch", FuzzingSupport::callExportCatch));

  vb::STDCompilerLogger logger{};
  vb::WasmModule wasmModule{logger};

  try {
    vb::Span<vb::NativeSymbol const> const dynamicLinkedSymbols{linkedSymbols.data(), linkedSymbols.size()};
    if (VBHELPER_INPUT_IS_ALREADY_COMPILED) {
      basic_memcpy(buffer, bytecode.data(), static_cast<uint32_t>(bytecode.size()));
      vb::Span<const uint8_t> const execSpan = vb::Span<const uint8_t>(buffer, bytecode.size());

      wasmModule.initFromCompiledBinary(execSpan, dynamicLinkedSymbols, vb::Span<uint8_t const>());
    } else {
      bool highPressure;
#ifdef VB_FORCE_HIGH_REGISTER_PRESSURE
      highPressure = true;
#else
      highPressure = false;
#endif
      vb::WasmModule::CompileResult const compileResult{wasmModule.compile(bytecode, dynamicLinkedSymbols, highPressure)};
      assert(compileResult.getModule().size() <= binaryMemoryRegion.size() && "Compiled module size exceeds the size of the binary memory region");
      memcpy(binaryMemoryRegion.data(), compileResult.getModule().data(), compileResult.getModule().size());
      wasmModule.initFromCompiledBinary(vb::Span<uint8_t const>(binaryMemoryRegion.data(), compileResult.getModule().size()), dynamicLinkedSymbols,
                                        compileResult.getDebugSymbol().span());
    }

    wasmModule.start(stackTop);
    uint32_t startOffset = 0;

    while (true) {
      uint32_t lineLength = 0;
      while (startOffset + lineLength < refOutputLength) {
        uint8_t const currentChar = *(refOutputStart + startOffset + lineLength);
        if (currentChar == '\n') {
          break;
        } else {
          lineLength++;
        }
      }

      char const *const lineStart = vb::pCast<char const *>(refOutputStart + startOffset);

      if (next) {
        next = false;
        break;
      }

      handleLine(lineStart, lineLength, stackTop, wasmModule);

      startOffset += ++lineLength;
      if (startOffset >= refOutputLength) {
        break;
      }
    }
    size_t const remainingLines = deferredLines.size();
    if (remainingLines != 0) {
      std::stringstream ss;
      ss << "Non-consumed lines in buffer. Exiting. Size " << remainingLines << std::endl;
      executionFailed(ss.str());
    }
  } catch (std::exception &e) {
    std::stringstream ss;
    ss << e.what() << "\n";
    executionFailed(ss.str());
  }
}

int main() {
  vb::WasmModule::initEnvironment(mallocFunction, reallocFunction, freeFunction);
  buffer += 8U;
  uintptr_t rawBuffer = vb::pToNum(buffer);
  rawBuffer &= ~7U;
  buffer = vb::numToP<uint8_t *>(rawBuffer);

  std::cout << "Starting fuzzer  ...\n";
  uint8_t const *const stackTop{vb::pCast<uint8_t const *>(vb::getStackTop())};
  while (true) {
    fuzz(stackTop);
  }

  vb::WasmModule::destroyEnvironment();
}
