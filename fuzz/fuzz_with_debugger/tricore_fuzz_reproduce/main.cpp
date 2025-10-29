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

#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/BinaryModule.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/STDCompilerLogger.hpp"
#include "src/utils/StackTop.hpp"

extern uint8_t const *bytecodeStart;
extern size_t bytecodeLength;

alignas(8) std::array<uint8_t, 1024U * 390U> jobMemoryRegion{};

alignas(8) std::array<uint8_t, 1024U * 255U> binaryMemoryRegion{};

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

namespace FuzzingSupport {
void logI32(uint32_t const value, void *const ctx) noexcept {
  static_cast<void>(ctx);
  std::cout << "called host fuzzing-support.log-i32(i32) =>" << value << std::endl;
}
void logI64(uint64_t const value, void *const ctx) noexcept {
  static_cast<void>(ctx);
  std::cout << "called host fuzzing-support.log-i64(i64) =>" << value << std::endl;
}

void logF32(float const value, void *const ctx) noexcept {
  static_cast<void>(ctx);
  std::cout << "called host fuzzing-support.log-f32(f32) =>" << value << std::endl;
}

void logF64(double const value, void *const ctx) noexcept {
  static_cast<void>(ctx);
  std::cout << "called host fuzzing-support.log-f64(f64) =>" << value << std::endl;
}
void callExport(uint32_t param1, uint32_t param2, void *const ctx) {
  static_cast<void>(ctx);
  std::cout << "called host fuzzing-support.call-export(i32:" + std::to_string(param1) + ", i32:" + std::to_string(param2) + ") =>" << std::endl;
}

uint32_t sleep(uint32_t param1, uint32_t param2, void *const ctx) {
  static_cast<void>(ctx);
  std::cout << "called host fuzzing-support.sleep(i32:" + std::to_string(param1) + ", i32:" + std::to_string(param2) + ") => i32:0" << std::endl;
  return 0;
}

uint32_t callExportCatch(uint32_t param1, void *const ctx) {
  static_cast<void>(ctx);
  std::cout << "called host fuzzing-support.call-export-catch(i32:" + std::to_string(param1) + ") => i32:0" << std::endl;
  return 0;
}

} // namespace FuzzingSupport

static void fuzz() noexcept {
  vb::Span<uint8_t const> const bytecode = vb::Span<uint8_t const>(bytecodeStart, bytecodeLength);

  const auto linkedSymbols = vb::make_array(
      DYNAMIC_LINK("fuzzing-support", "log-i32", FuzzingSupport::logI32), DYNAMIC_LINK("fuzzing-support", "log-i64", FuzzingSupport::logI64),
      DYNAMIC_LINK("fuzzing-support", "log-f32", FuzzingSupport::logF32), DYNAMIC_LINK("fuzzing-support", "log-f64", FuzzingSupport::logF64),
      DYNAMIC_LINK("fuzzing-support", "call-export", FuzzingSupport::callExport), DYNAMIC_LINK("fuzzing-support", "sleep", FuzzingSupport::sleep),
      DYNAMIC_LINK("fuzzing-support", "call-export-catch", FuzzingSupport::callExportCatch));

  try {
    vb::Span<vb::NativeSymbol const> const dynamicLinkedSymbols{linkedSymbols.data(), linkedSymbols.size()};
    vb::STDCompilerLogger logger{};
    vb::WasmModule wasmModule{logger};

    vb::WasmModule::CompileResult const compileResult{wasmModule.compile(bytecode, dynamicLinkedSymbols)};
    wasmModule.initFromCompiledBinary(compileResult.getModule().span(), dynamicLinkedSymbols, compileResult.getDebugSymbol().span());
    uint8_t const *const stackTop{vb::pCast<uint8_t const *>(vb::getStackTop())};

    wasmModule.start(stackTop);

    ///-------------------------------------------------------------------------------
    /// Need to adapt this part for different test cases
    std::array<vb::WasmValue, 1U> const res{wasmModule.callExportedFunctionWithName<1>(stackTop, "func")};

    std::cout << "func() => i64:" << res[0].i64 << std::endl;
    ///-------------------------------------------------------------------------------

  } catch (std::exception &e) {
    std::cout << e.what() << std::endl;
  }
}

int main() {
  vb::WasmModule::initEnvironment(mallocFunction, reallocFunction, freeFunction);
  fuzz();
  vb::WasmModule::destroyEnvironment();
  return 0;
}
