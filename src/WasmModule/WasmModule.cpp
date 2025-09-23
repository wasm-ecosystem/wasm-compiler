///
/// @file WasmModule.cpp
/// @copyright Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
/// SPDX-License-Identifier: Apache-2.0
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
#include "src/config.hpp"
//
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/core/compiler/Compiler.hpp"
//

#include "WasmModule.hpp"

#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/common/ILogger.hpp"
#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/common/ManagedBinary.hpp"
#include "src/core/runtime/Runtime.hpp"
#include "src/extensions/Extension.hpp"

#ifdef VB_WIN32_OR_POSIX
#include <atomic>

#include "src/utils/ExecutableMemory.hpp"
#include "src/utils/MemUtils.hpp"
#include "src/utils/RAIISignalHandler.hpp"
#include "src/utils/SignalFunctionWrapper.hpp"
#endif

#if LINEAR_MEMORY_BOUNDS_CHECKS && !(defined JIT_TARGET_TRICORE)
#include <mutex>
#endif

#if LINEAR_MEMORY_BOUNDS_CHECKS == 0
#include "src/utils/LinearMemoryAllocator.hpp"
#endif

namespace vb {

WasmModule::MallocFunction WasmModule::mallocFunction_{nullptr};
WasmModule::ReallocFunction WasmModule::reallocFunction_{nullptr};
WasmModule::FreeFunction WasmModule::freeFunction_{nullptr};

// coverity[autosar_cpp14_a15_4_4_violation]
void WasmModule::initEnvironment(MallocFunction const mallocFunction, ReallocFunction const reallocFunction, FreeFunction const freeFunction) {
#ifdef VB_NEED_SIGNAL_HANDLER
  SignalFunctionWrapper::setPersistentHandler();
#endif
  mallocFunction_ = mallocFunction;
  reallocFunction_ = reallocFunction;
  freeFunction_ = freeFunction;
}

// coverity[autosar_cpp14_m0_1_8_violation]
void WasmModule::destroyEnvironment() VB_NOEXCEPT {
#ifdef VB_NEED_SIGNAL_HANDLER
  RAIISignalHandler::restoreSignalHandler();
#endif
#if ENABLE_EXTENSIONS
  extension::stop();
#endif
}

#if ENABLE_EXTENSIONS
WasmModule::~WasmModule() VB_NOEXCEPT {
  extension::unregisterRuntime(runtime_);
}
#else
WasmModule::~WasmModule() VB_NOEXCEPT = default;
#endif

void WasmModule::initFromBytecode(Span<uint8_t const> const &bytecode, Span<NativeSymbol const> const &linkedFunctions,
                                  bool const allowUnknownImports) {
  CompileResult const compileResult{compile(bytecode, linkedFunctions, allowUnknownImports, false)};

  rawDebugSymbol_ = compileResult.getDebugSymbol().span();
  setupRuntime(compileResult.getModule().span(), linkedFunctions, rawDebugSymbol_);
}

WasmModule::CompileResult WasmModule::compile(Span<uint8_t const> const &bytecode, Span<NativeSymbol const> const &linkedFunctions,
                                              bool const allowUnknownImports, bool const highPressureMode) {
  // coverity[autosar_cpp14_a16_2_3_violation] fake positive
  Compiler compiler{&compilerRealloc, &compilerMemoryAllocFnc, &compilerMemoryFreeFnc, this, &jitRealloc, allowUnknownImports};
  compiler.setLogger(&logger_);
  if (stackRecordCount_ != 0U) {
    compiler.setStacktraceRecordCount(stackRecordCount_);
  }

  if (highPressureMode) {
    compiler.forceHighRegisterPressureForTesting();
  }

  if (debugBuild_) {
    compiler.enableDebugMode(&debugLineFnc);
  }

  ManagedBinary module{compiler.compile(bytecode, linkedFunctions)};
  ManagedBinary debugSymbol{compiler.retrieveDebugMap()};

  CompileResult compileResult{std::move(module), std::move(debugSymbol)};
  return compileResult;
}

void WasmModule::initFromCompiledBinary(Span<uint8_t const> const &compiledBinary, Span<NativeSymbol const> const &linkedFunctions,
                                        Span<uint8_t const> const &rawDebugSymbol) {
  for (size_t i{0U}; i < linkedFunctions.size(); ++i) {
    if (linkedFunctions[i].linkage == NativeSymbol::Linkage::STATIC) {
      throw RuntimeError(ErrorCode::Wrong_type);
    }
  }

  rawDebugSymbol_ = rawDebugSymbol;
  setupRuntime(compiledBinary, linkedFunctions, rawDebugSymbol_);
}

// coverity[autosar_cpp14_m7_1_2_violation]
void *WasmModule::compilerMemoryAllocFnc(uint32_t const size, void *const ctx) VB_NOEXCEPT {
  static_cast<void>(ctx);
  // GCOVR_EXCL_START
  assert(mallocFunction_ != nullptr && "Realloc function is not set");
  if (mallocFunction_ == nullptr) {
    return nullptr;
  }
  // GCOVR_EXCL_STOP
  return mallocFunction_(static_cast<size_t>(size));
}

// coverity[autosar_cpp14_m7_1_2_violation]
void WasmModule::compilerMemoryFreeFnc(void *const ptr, void *const ctx) VB_NOEXCEPT {
  static_cast<void>(ctx);

  if (ptr != nullptr) {
    memFree(ptr);
  }
}

// coverity[autosar_cpp14_m7_1_2_violation]
void WasmModule::compilerRealloc(ExtendableMemory &currentObject, uint32_t minimumLength, void *const ctx) VB_NOEXCEPT {
  static_cast<void>(ctx);
  // GCOVR_EXCL_START
  assert(reallocFunction_ != nullptr && "Realloc function is not set");
  if (reallocFunction_ == nullptr) {
    return;
  }
  // GCOVR_EXCL_STOP
  if (minimumLength == 0U) {
    memFree(currentObject.data());
  } else {
    if (minimumLength < 4096U) {
      minimumLength = minimumLength * 2U;
    } else {
      minimumLength = minimumLength + 4096U;
    }
    void *const newPtr{reallocFunction_(currentObject.data(), static_cast<size_t>(minimumLength))};
    currentObject.reset(vb::pCast<uint8_t *>(newPtr), minimumLength);
  }
}

// coverity[autosar_cpp14_m7_1_2_violation]
void WasmModule::jitRealloc(ExtendableMemory &currentObject, uint32_t const minimumLength, void *const ctx) VB_NOEXCEPT {
  static_cast<void>(ctx);
  return compilerRealloc(currentObject, minimumLength, ctx);
}

// coverity[autosar_cpp14_m7_1_2_violation]
void WasmModule::debugLineFnc(vb::ExtendableMemory &currentObject, uint32_t const minimumLength, void *const ctx) VB_NOEXCEPT {
  static_cast<void>(ctx);
  if (minimumLength == 0U) {
    memFree(currentObject.data());
    return;
  }
  void *const newPtr{reallocFunction_(currentObject.data(), static_cast<size_t>(minimumLength))};
  currentObject.reset(vb::pCast<uint8_t *>(newPtr), minimumLength);
}

#if LINEAR_MEMORY_BOUNDS_CHECKS
void WasmModule::runtimeMemoryAllocFncRaw(ExtendableMemory &currentObject, uint32_t const minimumLength, void *const ctx) VB_NOEXCEPT {
  WasmModule *const pWasmModule{pCast<WasmModule *>(ctx)};
  pWasmModule->runtimeMemoryAllocFnc(currentObject, minimumLength);
}

void WasmModule::runtimeMemoryAllocFnc(ExtendableMemory &currentObject, uint32_t const minimumLength) VB_NOEXCEPT {
  // GCOVR_EXCL_START
  assert(reallocFunction_ != nullptr && "Realloc function is not set");
  if (reallocFunction_ == nullptr) {
    return;
  }
  // GCOVR_EXCL_STOP

  if (minimumLength == 0U) {
    memFree(currentObject.data());
    return;
  }
  if (minimumLength > maxRam_) {
    // out of limitation
    maxDesiredRamOnMemoryExtendFailed_ = minimumLength;
    return;
  }
  // Current limitation of the runtime
  uint64_t const effectiveLimit{std::min(maxRam_, static_cast<uint64_t>(UINT32_MAX))};
  uint64_t const proposedLength{std::max(static_cast<uint64_t>(512U), static_cast<uint64_t>(minimumLength)) + 4096U};

  uint32_t const expectedLength{static_cast<uint32_t>(std::min(effectiveLimit, proposedLength))};
#if !(defined JIT_TARGET_TRICORE)
  std::unique_lock<std::mutex> const lock(linearMemoryMutex_);
#endif
  void *newPtr{nullptr};
  newPtr = reallocFunction_(currentObject.data(), static_cast<size_t>(expectedLength));
  if (newPtr != nullptr) {
    currentObject.reset(vb::pCast<uint8_t *const>(newPtr), expectedLength);
    return;
  }
  newPtr = reallocFunction_(currentObject.data(), static_cast<size_t>(minimumLength));
  if (newPtr != nullptr) {
    currentObject.reset(vb::pCast<uint8_t *const>(newPtr), minimumLength);
    return;
  }
  maxDesiredRamOnMemoryExtendFailed_ = minimumLength;
}
#endif

void WasmModule::requestInterruption(vb::TrapCode const trapCode) VB_NOEXCEPT {
  if (runtime_.hasBinaryModule()) {
#if LINEAR_MEMORY_BOUNDS_CHECKS && !(defined JIT_TARGET_TRICORE)
    std::unique_lock<std::mutex> const lock(linearMemoryMutex_);
#endif
#if INTERRUPTION_REQUEST
    runtime_.requestInterruption(trapCode);
#else
    static_cast<void>(trapCode);
    UNREACHABLE(_, "Should not be called if interruption is not requested");
#endif

#ifdef VB_WIN32_OR_POSIX
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
  }
}

void WasmModule::start(uint8_t const *const stackTop) {
  setStackTop(stackTop);
#ifdef VB_NEED_SIGNAL_HANDLER
  SignalFunctionWrapper::start(runtime_);
#else
  runtime_.start();
#endif
}

// coverity[autosar_cpp14_m0_1_8_violation]
// coverity[autosar_cpp14_a7_1_1_violation]
// coverity[autosar_cpp14_a15_4_4_violation]
void WasmModule::setStackTop(uint8_t const *stackTop) const {
  static_cast<void>(stackTop);
  static_cast<void>(this);
#if ACTIVE_STACK_OVERFLOW_CHECK
#ifdef VB_RESERVED_STACK_SIZE
  stackTop = vb::pAddI(stackTop, VB_RESERVED_STACK_SIZE);
#endif
  runtime_.setStackFence(stackTop);
#endif
}

void WasmModule::setupRuntime(Span<uint8_t const> const &compiledBinary, Span<NativeSymbol const> const &linkedFunctions,
                              Span<uint8_t const> const &rawDebugSymbol) {
  Span<uint8_t const> machineCode{};

#ifndef JIT_TARGET_TRICORE
  executableMemory_ = vb::ExecutableMemory::make_executable_copy(compiledBinary);
  machineCode = executableMemory_.span();
#else
  machineCode = compiledBinary;
#endif
#if LINEAR_MEMORY_BOUNDS_CHECKS
  runtime_ = vb::Runtime(machineCode, &runtimeMemoryAllocFncRaw, linkedFunctions, this);
#else
  linearMemoryAllocator_.setMemoryLimit(maxRam_);
  // coverity[autosar_cpp14_a15_0_2_violation]
  runtime_ = Runtime(machineCode, linearMemoryAllocator_, linkedFunctions, this);
#endif

#if ENABLE_EXTENSIONS
  extension::registerRuntime(runtime_);
#endif

  sendDebugSymbolToDebugger(rawDebugSymbol);
}

#if LINEAR_MEMORY_BOUNDS_CHECKS
uint64_t WasmModule::getMaxDesiredRamOnMemoryExtendFailed() const VB_NOEXCEPT {
  return maxDesiredRamOnMemoryExtendFailed_;
}
#else
uint64_t WasmModule::getMaxDesiredRamOnMemoryExtendFailed() const VB_NOEXCEPT {
  return linearMemoryAllocator_.getMaxDesiredRamOnMemoryExtendFailed();
}
#endif // LINEAR_MEMORY_BOUNDS_CHECKS

void WasmModule::sendDebugSymbolToDebugger(Span<uint8_t const> const &debugSymbol) VB_NOEXCEPT {
  // fixme: This function is a placeholder for sending debug symbols to a debugger.
  static_cast<void>(debugSymbol);
}

void WasmModule::shrinkMemory(uint32_t const minimumLength) {
  // GCOVR_EXCL_START
  assert(runtime_.hasBinaryModule());
  // GCOVR_EXCL_STOP

  runtime_.shrinkToSize(minimumLength);
}

Span<uint8_t const> WasmModule::getCompiledBinary() const VB_NOEXCEPT {
#ifndef JIT_TARGET_TRICORE
  return Span<uint8_t const>(executableMemory_.data(), executableMemory_.size());
#else
  return Span<uint8_t const>(nullptr, 0U);
#endif
}

void WasmModule::memFree(void *const ptr) VB_NOEXCEPT {
  // GCOVR_EXCL_START
  assert(freeFunction_ != nullptr && "Realloc function is not set");
  if (freeFunction_ == nullptr) {
    return;
  }
  freeFunction_(ptr);
  // GCOVR_EXCL_STOP
}

#if ENABLE_ADVANCED_APIS
void WasmModule::callRawExportedFunctionByName(Span<char const> const &functionName, uint8_t const *const stackTop,
                                               uint8_t const *const serializedArgs, uint8_t *const results) {
  RawModuleFunction const wasmFunction{runtime_.getRawExportedFunctionByName(functionName, Span<char const>{})};
  setStackTop(stackTop);
#ifdef VB_NEED_SIGNAL_HANDLER
  SignalFunctionWrapper::call(wasmFunction, serializedArgs, results);
#else
  wasmFunction(serializedArgs, results);
#endif
}
#endif

} // namespace vb
