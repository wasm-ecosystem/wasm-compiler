///
/// @file SignalFunctionWrapper_win.cpp
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
// coverity[autosar_cpp14_a16_2_2_violation]
#include "src/config.hpp"
#ifdef VB_WIN32
#include "SignalFunctionWrapper.hpp"
#include "SignalFunctionWrapper_win.hpp"

#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK

#include "io.h"

#include "src/core/common/WasmConstants.hpp"

#if CXX_TARGET == ISA_X86_64
#include "src/core/compiler/backend/x86_64/x86_64_cc.hpp"
namespace NBackend = vb::x86_64;
#elif CXX_TARGET == ISA_AARCH64
#include "src/core/compiler/backend/aarch64/aarch64_cc.hpp"
namespace NBackend = vb::aarch64;
#else
static_assert(false, "OS not supported");
#endif

namespace vb {
#if !LINEAR_MEMORY_BOUNDS_CHECKS

///
/// @brief Set the function parameters after signal handler returns
///
/// @param pExceptionInfo Signal context
/// @param param1 First parameter
/// @param param2 First parameter
static void setParamsForReturn(PEXCEPTION_POINTERS &pExceptionInfo, uint64_t param1, uint64_t param2) VB_NOEXCEPT {
  int64_t const vp1 = bit_cast<int64_t>(param1);
  int64_t const vp2 = bit_cast<int64_t>(param2);

#if CXX_TARGET == ISA_X86_64
  static_assert(NBackend::NativeABI::gpParams[0] == NBackend::REG::C, "First parameter mismatch");
  static_assert(NBackend::NativeABI::gpParams[1] == NBackend::REG::D, "Second parameter mismatch");
  pExceptionInfo->ContextRecord->Rcx = static_cast<DWORD64>(vp1);
  pExceptionInfo->ContextRecord->Rdx = static_cast<DWORD64>(vp2);
#elif CXX_TARGET == ISA_AARCH64
  static_assert(NBackend::NativeABI::gpParams[0] == NBackend::REG::R0, "First parameter mismatch");
  static_assert(NBackend::NativeABI::gpParams[1] == NBackend::REG::R1, "Second parameter mismatch");
  pExceptionInfo->ContextRecord->X0 = vp1;
  pExceptionInfo->ContextRecord->X1 = vp2;
#else
  static_assert(false, "Architecture not supported");
#endif
}

///
/// @brief Set the continue address after signal handler returns
///
/// @param pExceptionInfo Signal context
/// @param pc Target PC
///
static void setReturnFromSignalHandler(PEXCEPTION_POINTERS &pExceptionInfo, void *const pc) VB_NOEXCEPT {
  int64_t const vpc = static_cast<int64_t>(pToNum(pc));
#if CXX_TARGET == ISA_X86_64
  pExceptionInfo->ContextRecord->Rip = static_cast<DWORD64>(vpc);
#elif CXX_TARGET == ISA_AARCH64
  pExceptionInfo->ContextRecord->Pc = vpc;
#else
  static_assert(false, "Architecture not supported");
#endif
}

///
/// @brief Get value of PC register
///
/// @param pExceptionInfo
/// @return void*
///
static void *getContextPC(PEXCEPTION_POINTERS &pExceptionInfo) VB_NOEXCEPT {
#if CXX_TARGET == ISA_X86_64
  return numToP<void *>(pExceptionInfo->ContextRecord->Rip);
#elif CXX_TARGET == ISA_AARCH64
  return numToP<void *>(pExceptionInfo->ContextRecord->Pc);
#else
  static_assert(false, "Architecture not supported");
#endif
}
#endif
thread_local SignalWrapperErrorCode SignalFunctionWrapperWin::error_ = SignalWrapperErrorCode::NONE;

#if !ACTIVE_STACK_OVERFLOW_CHECK
thread_local bool SignalFunctionWrapperWin::stackTopIsVolatile_ = false;
thread_local bool SignalFunctionWrapperWin::stackGuaranteeSet_ = false;
#endif

// Throw an arbitrary trap from the runtime so that the SignalFunctionWrapper call function will catch it and react to the ErrorCode
long SignalFunctionWrapperWin::handleTrap(PEXCEPTION_POINTERS pExceptionInfo, uint32_t const trapCode) VB_NOEXCEPT {
  setReturnFromSignalHandler(pExceptionInfo, pCast<void *>(SignalFunctionWrapper::getRuntime()->getTrapFnc()));
  void *const linearMemoryBase = SignalFunctionWrapper::getRuntime()->unsafe__getLinearMemoryBase();
  setParamsForReturn(pExceptionInfo, pToNum(linearMemoryBase), static_cast<uint64_t>(trapCode));
  return EXCEPTION_CONTINUE_EXECUTION;
}

// NOLINTNEXTLINE(google-runtime-int)
long SignalFunctionWrapperWin::divSignalHandler(PEXCEPTION_POINTERS pExceptionInfo) VB_NOEXCEPT {
  DWORD const exceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
  uint32_t trapCode = 0U;

  if (!SignalFunctionWrapper::pcInWasmCodeRange(getContextPC(pExceptionInfo))) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (exceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO) {
    // Division by zero
    trapCode = static_cast<uint32_t>(TrapCode::DIV_ZERO);
  } else if (exceptionCode == EXCEPTION_INT_OVERFLOW) {
    // Integer overflow (division overflow)
    trapCode = static_cast<uint32_t>(TrapCode::DIV_OVERFLOW);
  } else {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  // Throw the trap from the runtime
  if (trapCode != 0U) {
    return handleTrap(pExceptionInfo, trapCode);
  }

  return EXCEPTION_CONTINUE_SEARCH;
}
// NOLINTNEXTLINE(google-runtime-int)
long SignalFunctionWrapperWin::memorySignalHandler(PEXCEPTION_POINTERS pExceptionInfo) VB_NOEXCEPT {
  DWORD const exceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
  void *const pc{getContextPC(pExceptionInfo)};
  uint32_t trapCode = 0U;
  if (!SignalFunctionWrapper::pcInWasmCodeRange(pc)) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

#if !ACTIVE_STACK_OVERFLOW_CHECK
  // Check if this is a stack overflow
  if (exceptionCode == EXCEPTION_STACK_OVERFLOW) {
    error_ = SignalWrapperErrorCode::STACKOVERFLOW;
    trapCode = static_cast<uint32_t>(TrapCode::STACKFENCEBREACHED);
  }
#endif

#if !LINEAR_MEMORY_BOUNDS_CHECKS
  if (exceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    assert(pExceptionInfo->ExceptionRecord->NumberParameters >= 2 && "Too few arguments in ExceptionInformation, should never happen");
    void *const faultAddr = numToP<void *>(pExceptionInfo->ExceptionRecord->ExceptionInformation[1]);

    // Check if this a linear memory fault
    int64_t const offsetInLinMemAllocation = SignalFunctionWrapper::getOffsetInLinearMemoryAllocation(faultAddr);
    if (offsetInLinMemAllocation >= 0) {
      if (static_cast<uint64_t>(offsetInLinMemAllocation) >=
          (static_cast<uint64_t>(WasmConstants::wasmPageSize) * SignalFunctionWrapper::pRuntime_->getLinearMemorySizeInPages())) {
        // Out of bounds of Wasm linear memory
        trapCode = static_cast<uint32_t>(TrapCode::LINMEM_OUTOFBOUNDSACCESS);
      } else {
        // Fault lies in non-commited memory portion, so we try to commit it via a landing pad
        SignalFunctionWrapper::landingPadData_ = static_cast<uint32_t>(offsetInLinMemAllocation);
        auto landingPad = SignalFunctionWrapper::pRuntime_->prepareLandingPad(SignalFunctionWrapper::probeLinearMemoryOffset, pc);
        setReturnFromSignalHandler(pExceptionInfo, pCast<void *>(landingPad));
        return EXCEPTION_CONTINUE_EXECUTION;
      }
    }
  }
#endif

  if (trapCode != 0U) {
    return handleTrap(pExceptionInfo, trapCode);
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace vb

#endif
#endif
