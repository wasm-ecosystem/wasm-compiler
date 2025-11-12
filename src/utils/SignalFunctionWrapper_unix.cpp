///
/// @file SignalFunctionWrapper_unix.cpp
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

#if (defined VB_POSIX) && (!LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK)
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "RAIISignalHandler.hpp"
#include "SignalFunctionWrapper.hpp"
#include "SignalFunctionWrapper_unix.hpp"

#include "src/core/common/TrapCode.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_encoding.hpp"
#include "src/core/compiler/common/util.hpp"
#include "src/core/runtime/Runtime.hpp"
#include "src/utils/OSAPIChecker.hpp"

#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK

#include <csignal>
#include <unistd.h>

#include "LinearMemoryAllocator.hpp"
#include "MemUtils.hpp"

#include "src/core/common/WasmConstants.hpp"
#include "src/core/common/util.hpp"

#if !(defined __APPLE__)
#include <ucontext.h>
#endif

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

///
/// @brief Set the function parameters after signal handler returns
///
/// @param uc Signal context
/// @param param1 First parameter
/// @param param2 First parameter
static void setParamsForReturn(ucontext_t *const uc, uint64_t const param1, uint64_t const param2) VB_NOEXCEPT {
////////////////////////////////////////////////////////////////////////////////////////////////
#if CXX_TARGET == ISA_X86_64
  static_assert(NBackend::NativeABI::gpParams[0] == NBackend::REG::DI, "First parameter mismatch");
  static_assert(NBackend::NativeABI::gpParams[1] == NBackend::REG::SI, "Second parameter mismatch");
#ifdef __APPLE__
  uc->uc_mcontext->__ss.__di = param1;
  uc->uc_mcontext->__ss.__si = param2;
#elif defined __linux__
  uc->uc_mcontext.gregs[REG_RDI] = bit_cast<int64_t>(param1);
  uc->uc_mcontext.gregs[REG_RSI] = bit_cast<int64_t>(param2);
#elif defined __QNX__
  uc->uc_mcontext.cpu.rdi = param1;
  uc->uc_mcontext.cpu.rsi = param2;
#else
  static_assert(false, "OS not supported");
#endif
////////////////////////////////////////////////////////////////////////////////////////////////
#elif CXX_TARGET == ISA_AARCH64
  static_assert(NBackend::NativeABI::gpParams[0] == NBackend::REG::R0, "First parameter mismatch");
  static_assert(NBackend::NativeABI::gpParams[1] == NBackend::REG::R1, "Second parameter mismatch");
#ifdef __APPLE__
  uc->uc_mcontext->__ss.__x[0] = param1;
  uc->uc_mcontext->__ss.__x[1] = param2;
#elif defined __linux__
  uc->uc_mcontext.regs[0] = param1;
  uc->uc_mcontext.regs[1] = param2;
#elif defined __QNX__
  uc->uc_mcontext.cpu.gpr[0] = param1;
  uc->uc_mcontext.cpu.gpr[1] = param2;
#else
  static_assert(false, "OS not supported");
#endif
////////////////////////////////////////////////////////////////////////////////////////////////
#else
  static_assert(false, "Architecture not supported");
#endif
}

///
/// @brief Set the continue address after signal handler returns
///
/// @param uc Signal context
/// @param pc Target PC
///
static void setReturnFromSignalHandler(ucontext_t *const uc, void const *const pc) VB_NOEXCEPT {
////////////////////////////////////////////////////////////////////////////////////////////////
#if CXX_TARGET == ISA_X86_64
#ifdef __APPLE__
  uc->uc_mcontext->__ss.__rip = static_cast<uint64_t>(pToNum(pc));
#elif defined __linux__
  uc->uc_mcontext.gregs[REG_RIP] = static_cast<int64_t>(pToNum(pc));
#elif defined __QNX__
  uc->uc_mcontext.cpu.rip = static_cast<uint64_t>(pToNum(pc));
#else
  static_assert(false, "OS not supported");
#endif
////////////////////////////////////////////////////////////////////////////////////////////////
#elif CXX_TARGET == ISA_AARCH64
#ifdef __APPLE__
  uc->uc_mcontext->__ss.__pc = static_cast<uint64_t>(pToNum(pc));
#elif defined __linux__
  uc->uc_mcontext.pc = static_cast<uint64_t>(pToNum(pc));
#elif defined __QNX__
  uc->uc_mcontext.cpu.elr = static_cast<uint64_t>(pToNum(pc));
#endif
////////////////////////////////////////////////////////////////////////////////////////////////
#else
  static_assert(false, "Architecture not supported");
#endif
}

#if !LINEAR_MEMORY_BOUNDS_CHECKS

///
/// @brief Get value of PC register
///
/// @param uc
/// @return void*
///
static void *getContextPC(ucontext_t const *const uc) VB_NOEXCEPT {
#if !(CXX_TARGET == ISA_X86_64 || CXX_TARGET == ISA_AARCH64)
  static_assert(false, "Architecture not supported");
#endif

#ifdef __APPLE__
#if CXX_TARGET == ISA_X86_64
  return numToP<void *>(uc->uc_mcontext->__ss.__rip);
#elif CXX_TARGET == ISA_AARCH64
  return numToP<void *>(uc->uc_mcontext->__ss.__pc);
#endif

#elif defined __linux__
#if CXX_TARGET == ISA_X86_64
  return numToP<void *>(uc->uc_mcontext.gregs[REG_RIP]);
#elif CXX_TARGET == ISA_AARCH64
  return numToP<void *>(uc->uc_mcontext.pc);
#endif

#elif defined __QNX__
#if CXX_TARGET == ISA_X86_64
  return numToP<void *>(uc->uc_mcontext.cpu.rip);
#elif CXX_TARGET == ISA_AARCH64
  return numToP<void *>(uc->uc_mcontext.cpu.elr);
#endif

#else
  static_assert(false, "OS not supported");
#endif
}
#endif

#if !ACTIVE_STACK_OVERFLOW_CHECK
bool SignalFunctionWrapperUnix::isStackoverflow(ucontext_t const *const uc) VB_NOEXCEPT {
#ifdef __APPLE__
  uintptr_t const sp = pToNum(uc->uc_stack.ss_sp);
  uintptr_t const stackAddr = pToNum(secondaryStack_.get());

  // Check whether the stack pointer lies within the secondary stack. If yes, a stack overflow happened
  // TODO(Fabian) Double check for macOS, seems like this is always true in a signal handler or does SIGBUS always
  // enable the secondary stack and SIGSEGV does not?
  bool const isStackOverflow{sp >= stackAddr && sp <= stackAddr + SecondaryStack::getSecondaryStackSize()};
#else
#if CXX_TARGET == ISA_X86_64
  uintptr_t const sp{static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RSP])};
#elif CXX_TARGET == ISA_AARCH64
  uintptr_t const sp = static_cast<uintptr_t>(uc->uc_mcontext.sp);
#else
  static_assert(false, "CPU not supported");
#endif

  // Check whether the stack pointer lies below the stack top (Stack top is lowest allowed address on the stack)
  bool const isStackOverflow{sp <= pToNum(stackTop_)};
#endif
  return isStackOverflow;
}
#endif

thread_local sigset_t SignalFunctionWrapperUnix::sigMask_{};
thread_local bool SignalFunctionWrapperUnix::sigMaskInitialized_{false};
thread_local bool SignalFunctionWrapperUnix::sigMaskIsVolatile_{false};

#if !ACTIVE_STACK_OVERFLOW_CHECK
// coverity[autosar_cpp14_a8_5_2_violation]
thread_local SecondaryStack SignalFunctionWrapperUnix::secondaryStack_{};
thread_local uint32_t SecondaryStack::secondaryStackSize_{0U};

thread_local bool SignalFunctionWrapperUnix::stackTopIsVolatile_{false};
#ifdef __linux__
thread_local void *SignalFunctionWrapperUnix::stackTop_{nullptr};
#endif //__linux__

void SecondaryStack::init() {
  uint32_t const secondaryStackSize{static_cast<uint32_t>(SIGSTKSZ)};
  ptr_ = MemUtils::allocAlignedMemory(static_cast<size_t>(secondaryStackSize), MemUtils::getOSMemoryPageSize());

  stack_t ss;
  ss.ss_size = secondaryStackSize;
  ss.ss_sp = ptr_;
  ss.ss_flags = 0;

  int32_t const error{sigaltstack(&ss, nullptr)};
  checkSysCallReturn("sigaltstack set", error);
  secondaryStackSize_ = secondaryStackSize;
}

SecondaryStack::~SecondaryStack() VB_NOEXCEPT {
  release();
}

void SecondaryStack::release() const VB_NOEXCEPT {
  if (ptr_ != nullptr) {
    MemUtils::freeAlignedMemory(ptr_);
  }

  stack_t ss;
  ss.ss_sp = nullptr;
  ss.ss_size = static_cast<size_t>(MINSIGSTKSZ);
  ss.ss_flags = SS_DISABLE;

  int32_t const error{sigaltstack(&ss, nullptr)};
  static_cast<void>(error);
}
#endif

// Throw an arbitrary trap from the runtime so that the SignalFunctionWrapper call function will catch it and react to the ErrorCode
void SignalFunctionWrapperUnix::handleTrap(ucontext_t *const uc, uint32_t const trapCode) VB_NOEXCEPT {
  setReturnFromSignalHandler(uc, pCast<void *>(SignalFunctionWrapper::getRuntime()->getTrapFnc()));
  void *const linearMemoryBase{SignalFunctionWrapper::getRuntime()->unsafe__getLinearMemoryBase()};
  setParamsForReturn(uc, pToNum(linearMemoryBase), static_cast<uint64_t>(trapCode));
}

// coverity[autosar_cpp14_m7_1_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
void SignalFunctionWrapperUnix::memorySignalHandler(int32_t const signalId, siginfo_t *const si, void *const ptr) VB_NOEXCEPT {
  ucontext_t *const uc{pCast<ucontext_t *>(ptr)};
  uint32_t trapCode{0U};
  void *const pc{getContextPC(uc)};
  if (!SignalFunctionWrapper::pcInWasmCodeRange(pc)) {
    // coverity[autosar_cpp14_a18_1_1_violation] NOLINTNEXTLINE(modernize-avoid-c-arrays)
    constexpr char msg[]{"Current memory fault is not triggered by Wasm code. Rule out any issues in your "
                         "linked host functions and report this issue to the runtime team.\n"};
    ssize_t const sz{write(2, &msg[0], sizeof(msg))};
    static_cast<void>(sz);
    RAIISignalHandler::restoreSignalHandler();
    return;
  }
#if !ACTIVE_STACK_OVERFLOW_CHECK
  if (signalId == SIGSEGV) {
    // Check if this is a stack overflow
    if (isStackoverflow(uc)) {
      trapCode = static_cast<uint32_t>(TrapCode::STACKFENCEBREACHED);
    }
  }
#endif

  if ((signalId == SIGSEGV) || (signalId == SIGBUS)) {
#if !LINEAR_MEMORY_BOUNDS_CHECKS
    // Check if this a linear memory fault
    int64_t const offsetInLinMemAllocation{SignalFunctionWrapper::getOffsetInLinearMemoryAllocation(si->si_addr)};
    if (offsetInLinMemAllocation >= 0) {
      if (static_cast<uint64_t>(offsetInLinMemAllocation) >=
          (static_cast<uint64_t>(WasmConstants::wasmPageSize) * SignalFunctionWrapper::pRuntime_->getLinearMemorySizeInPages())) {
        // Out of bounds of Wasm linear memory
        trapCode = static_cast<uint32_t>(TrapCode::LINMEM_OUTOFBOUNDSACCESS);
      } else {
        // Fault lies in non-commited memory portion, so we try to commit it via a landing pad
        SignalFunctionWrapper::setLandingPad(static_cast<uint32_t>(offsetInLinMemAllocation));
        using LandingPadFnc = void (*)(void);
        LandingPadFnc const landingPad{SignalFunctionWrapper::pRuntime_->prepareLandingPad(&SignalFunctionWrapper::probeLinearMemoryOffset, pc)};
        setReturnFromSignalHandler(uc, pCast<void const *>(landingPad));
        return;
      }
    }
#else
    static_cast<void>(si);
#endif

    if (trapCode != 0U) {
      handleTrap(uc, trapCode);
      return;
    }

    // GCOVR_EXCL_START
    // coverity[autosar_cpp14_a18_1_1_violation] NOLINTNEXTLINE(modernize-avoid-c-arrays)
    constexpr char msg[]{"Unexpected segfault during execution of a Wasm function.\n"};
    ssize_t const sz{write(2, &msg[0], sizeof(msg))};
    static_cast<void>(sz);
    // GCOVR_EXCL_STOP
    RAIISignalHandler::restoreSignalHandler();
  }
}

// coverity[autosar_cpp14_m7_1_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
void SignalFunctionWrapperUnix::divSignalHandler(int32_t const signalId, siginfo_t *const si, void *const ptr) VB_NOEXCEPT {
  assert(signalId == SIGFPE);
  static_cast<void>(signalId);
  ucontext_t *const uc{pCast<ucontext_t *>(ptr)};
  if (!SignalFunctionWrapper::pcInWasmCodeRange(getContextPC(uc))) {
    // coverity[autosar_cpp14_a18_1_1_violation] NOLINTNEXTLINE(modernize-avoid-c-arrays)
    constexpr char msg[]{"Current arithmetic fault is not triggered by Wasm code. Rule out any issues in your "
                         "linked host functions and report this issue to the runtime team.\n"};
    ssize_t const sz{write(2, &msg[0], sizeof(msg))};
    static_cast<void>(sz);
    RAIISignalHandler::restoreSignalHandler();
    return;
  }
  uint32_t trapCode{0U};
  if ((si != nullptr) && (si->si_code == FPE_INTDIV)) {
    trapCode = static_cast<uint32_t>(TrapCode::DIV_ZERO);
  }
  if (trapCode != 0U) {
    handleTrap(uc, trapCode);
    return;
  } else {
    // GCOVR_EXCL_START
    // coverity[autosar_cpp14_a18_1_1_violation] NOLINTNEXTLINE(modernize-avoid-c-arrays)
    constexpr char msg[]{"Arithmetic exception during execution of a Wasm function. Rule out any issues in your "
                         "linked host functions and report this issue to the runtime team.\n"};
    ssize_t const sz{write(2, &msg[0], sizeof(msg))};
    static_cast<void>(sz);
    // GCOVR_EXCL_STOP
    RAIISignalHandler::restoreSignalHandler();
  }
}

} // namespace vb

#endif
#endif
