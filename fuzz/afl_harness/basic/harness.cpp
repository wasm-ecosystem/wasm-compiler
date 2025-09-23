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

#include <cassert>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <ostream>
#include <random>
#include <vector>

#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/BinaryModule.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/STDCompilerLogger.hpp"
#include "src/utils/StackTop.hpp"

static std::vector<uint8_t> loadWasmFile(char *path) {
  std::cout << "Loading file ...\n" << std::endl;

  FILE *const filePtr = fopen(path, "rb");

  uint64_t length;

  if (filePtr == nullptr) {
    std::cout << "File not found. Aborting." << std::endl;
    exit(0);
  }

  fseek(filePtr, 0, SEEK_END);
  length = static_cast<uint64_t>(ftell(filePtr));
  assert(length < INT32_MAX);
  rewind(filePtr);

  std::vector<uint8_t> buffer(length);
  size_t const readSize = fread(buffer.data(), 1, length, filePtr);

  if (readSize != length) {
    perror("read file error");
    std::exit(0);
  }

  fclose(filePtr);

  return buffer;
}

namespace FuzzingSupport {
void logI32(uint32_t value, void *const ctx) {
  static_cast<void>(ctx);
  std::cout << "logI32 " << value << std::endl;
}
void logI64(uint64_t value, void *const ctx) {
  static_cast<void>(ctx);
  std::cout << "logI64 " << value << std::endl;
}
void logF32(float value, void *const ctx) {
  static_cast<void>(ctx);
  std::cout << "logF32 " << value << std::endl;
}
void logF64(double value, void *const ctx) {
  static_cast<void>(ctx);
  std::cout << "logF64 " << value << std::endl;
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
  uint8_t const *const binaryModuleEnd = binaryModule.data() + binaryModule.size();
  uint8_t const *stepPtr = binaryModuleEnd - 4 * 4; // Skip OPBVMET3, OPBVMET2, OPBVMET1, OPBVMET0

  uint32_t const numTableEntries = vb::readNextValue<uint32_t>(&stepPtr);
  stepPtr -= numTableEntries * (4 + 4); // Skip table entries

  uint32_t const exportedFunctionsSectionSize = vb::readNextValue<uint32_t>(&stepPtr);
  static_cast<void>(exportedFunctionsSectionSize);
  uint32_t const numExportedFunctions = vb::readNextValue<uint32_t>(&stepPtr);

  for (uint32_t i = 0; i < numExportedFunctions; i++) {
    uint32_t const numTableIndices = vb::readNextValue<uint32_t>(&stepPtr);
    stepPtr -= numTableIndices * 4;

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

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "No file specified. Aborting" << std::endl;
    exit(0);
  }
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);
  try {
    std::cout << "Compiling module ..." << std::endl;
    std::vector<uint8_t> const bytecodeVector{loadWasmFile(argv[1])};
    vb::Span<uint8_t const> const bytecode{bytecodeVector.data(), static_cast<uint32_t>(bytecodeVector.size())};

    vb::STDCompilerLogger logger{};

    vb::WasmModule wasmModule{logger};

    auto dynamicLinkedSymbols = vb::make_array(
        DYNAMIC_LINK("fuzzing-support", "log-i32", FuzzingSupport::logI32), DYNAMIC_LINK("fuzzing-support", "log-i64", FuzzingSupport::logI64),
        DYNAMIC_LINK("fuzzing-support", "log-f32", FuzzingSupport::logF32), DYNAMIC_LINK("fuzzing-support", "log-f64", FuzzingSupport::logF64));

    vb::Span<vb::NativeSymbol const> const symbols{dynamicLinkedSymbols.data(), dynamicLinkedSymbols.size()};

    vb::WasmModule::CompileResult const compileResult{wasmModule.compile(bytecode, symbols)};
    wasmModule.initFromCompiledBinary(compileResult.getModule().span(), symbols, compileResult.getDebugSymbol().span());
    uint8_t const *const stackTop{vb::pCast<uint8_t const *>(vb::getStackTop())};

    try {
      wasmModule.start(stackTop);
    } catch (vb::TrapException &e) {
      if (e.getTrapCode() == vb::TrapCode::STACKFENCEBREACHED) {
        std::cout << "Module start function Stackoverflow" << std::endl;
      } else {
        throw;
      }
    }
    {
      using namespace vb;
      mt = std::mt19937(static_cast<std::mt19937::result_type>(42_U64 * compileResult.getModule().size()));
    }

    iterateExportedFunctions(compileResult.getModule().span(), [&wasmModule, stackTop](char const *exportName, uint32_t nameLength,
                                                                                       uint8_t const *signature, uint32_t signatureLength) {
      std::vector<uint8_t> serializationData{};
      std::vector<uint8_t> results{};
      try {
        std::string_view const exportNameView(exportName);
        std::string_view const signatureView(vb::pCast<char const *>(signature));
        std::cout << "Function with name " << exportNameView.substr(0, nameLength) << " and signature" << signatureView.substr(0, signatureLength)
                  << " found. Executing "
                     "with generated input."
                  << std::endl;

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
        wasmModule.callRawExportedFunctionByName(vb::Span<char const>(exportName, nameLength), stackTop, serializationData.data(), results.data());
      } catch (vb::TrapException &e) {
        if (e.getTrapCode() == vb::TrapCode::STACKFENCEBREACHED) {
          std::cout << "Module start function Stackoverflow" << std::endl;
        } else {
          std::cout << e.what() << ": " << static_cast<uint32_t>(e.getTrapCode()) << std::endl;
        }
      }
    });
  } catch (vb::TrapException &e) {
    std::cout << e.what() << ": " << static_cast<uint32_t>(e.getTrapCode()) << std::endl;
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }
  vb::WasmModule::destroyEnvironment();
}
